/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "ib_device.h"

#include <libgen.h>
#include "ib_md.h"


/* This table is according to "Encoding for RNR NAK Timer Field"
 * in IBTA specification */
const double sct_ib_qp_rnr_time_ms[] = {
    655.36,  0.01,  0.02,   0.03,   0.04,   0.06,   0.08,   0.12,
    0.16,    0.24,  0.32,   0.48,   0.64,   0.96,   1.28,   1.92,
    2.56,    3.84,  5.12,   7.68,  10.24,  15.36,  20.48,  30.72,
    40.96,  61.44, 81.92, 122.88, 163.84, 245.76, 327.68, 491.52
};


static UCS_F_ALWAYS_INLINE
khint32_t sct_ib_async_event_hash_func(sct_ib_async_event_t event)
{
    return kh_int64_hash_func(((uint64_t)event.event_type << 32) |
                              event.resource_id);
}

static UCS_F_ALWAYS_INLINE int
uct_ib_async_event_hash_equal(sct_ib_async_event_t event1,
                              sct_ib_async_event_t event2)
{
    return (event1.event_type  == event2.event_type) &&
           (event1.resource_id == event2.resource_id);
}

KHASH_IMPL(sct_ib_async_event, sct_ib_async_event_t, sct_ib_async_event_val_t, 1,
           sct_ib_async_event_hash_func, uct_ib_async_event_hash_equal)

static sct_ib_device_spec_t sct_ib_builtin_device_specs[] = {
    {"Generic HCA", {0, 0}, 0, 0},
    {NULL}
};

#define EVENT_INFO_LEN_MAX 200

static void sct_ib_device_get_locality(const char *dev_name,
                                       ucg_sys_cpuset_t *cpu_mask,
                                       int *numa_node)
{
    char *p, buf[ucs_max(CPU_SETSIZE, 10)];
    ucs_status_t status;
    ssize_t nread;
    uint32_t word;
    int base, k;
    long n;

    /* Read list of CPUs close to the device */
    CPU_ZERO(cpu_mask);
    nread = ucs_read_file(buf, sizeof(buf) - 1, 1, SCT_IB_DEVICE_SYSFS_FMT,
                          dev_name, "local_cpus");
    if (nread >= 0) {
        buf[CPU_SETSIZE - 1] = '\0';
        base = 0;
        do {
            p = strrchr(buf, ',');
            if (p == NULL) {
                p = buf;
            } else if (*p == ',') {
                *(p++) = 0;
            }

            word = strtoul(p, 0, 16);
            for (k = 0; word; ++k, word >>= 1) {
                if (word & 1) {
                    CPU_SET(base + k, cpu_mask);
                }
            }
            base += 32;
        } while ((base < CPU_SETSIZE) && (p != buf));
    } else {
        /* If affinity file is not present, treat all CPUs as local */
        for (k = 0; k < CPU_SETSIZE; ++k) {
            CPU_SET(k, cpu_mask);
        }
    }

    /* Read NUMA node number */
    status = ucg_read_file_number(&n, 1,
                                  "/sys/class/infiniband/%s/device/numa_node",
                                  dev_name);
    *numa_node = (status == UCS_OK) ? n : -1;
}

static unsigned sct_ib_device_async_event_proxy(void *arg)
{
    sct_ib_async_event_wait_t *wait_ctx = arg;

    wait_ctx->cb_id = UCS_CALLBACKQ_ID_NULL;
    wait_ctx->cb(wait_ctx);
    return 1;
}

static void sct_ib_device_async_event_dispatch(sct_ib_device_t *dev,
                                               const sct_ib_async_event_t *event)
{
    sct_ib_async_event_val_t *entry;
    khiter_t iter;

    ucs_spin_lock(&dev->async_event_lock);
    iter = kh_get(sct_ib_async_event, &dev->async_events_hash, *event);
    if (iter != kh_end(&dev->async_events_hash)) {
        entry = &kh_value(&dev->async_events_hash, iter);
        entry->flag = 1;
        if (entry->wait_ctx != NULL) {
            /* someone is waiting */
            entry->wait_ctx->cb_id =
                ucs_callbackq_add_safe(entry->wait_ctx->cbq, sct_ib_device_async_event_proxy,
                                       entry->wait_ctx, UCS_CALLBACKQ_FLAG_ONESHOT);
        }
    }
    ucs_spin_unlock(&dev->async_event_lock);
}

ucs_status_t sct_ib_device_async_event_register(sct_ib_device_t *dev,
                                                enum ibv_event_type event_type,
                                                uint32_t resource_id)
{
    sct_ib_async_event_val_t *entry;
    sct_ib_async_event_t event;
    ucs_status_t status;
    khiter_t iter;
    int ret;

    event.event_type  = event_type;
    event.resource_id = resource_id;

    ucs_spin_lock(&dev->async_event_lock);
    iter = kh_put(sct_ib_async_event, &dev->async_events_hash, event, &ret);
    if (ret == UCS_KH_PUT_FAILED) {
        status = UCS_ERR_NO_MEMORY;
        goto out;
    }

    ucg_assert(ret != UCS_KH_PUT_KEY_PRESENT);
    entry           = &kh_value(&dev->async_events_hash, iter);
    entry->wait_ctx = NULL;
    entry->flag     = 0;
    status          = UCS_OK;

out:
    ucs_spin_unlock(&dev->async_event_lock);
    return status;
}

ucs_status_t sct_ib_device_async_event_wait(sct_ib_device_t *dev,
                                            enum ibv_event_type event_type,
                                            uint32_t resource_id,
                                            sct_ib_async_event_wait_t *wait_ctx)
{
    sct_ib_async_event_val_t *entry;
    sct_ib_async_event_t event;
    ucs_status_t status;
    khiter_t iter;

    event.event_type  = event_type;
    event.resource_id = resource_id;

    ucs_spin_lock(&dev->async_event_lock);
    iter  = kh_get(sct_ib_async_event, &dev->async_events_hash, event);
    ucg_assert(iter != kh_end(&dev->async_events_hash));
    entry = &kh_value(&dev->async_events_hash, iter);
    if (entry->flag) {
        /* event already arrived */
        status          = UCS_OK;
        entry->wait_ctx = NULL;
    } else if (entry->wait_ctx != NULL) {
        /* someone is already waiting for this event */
        status          = UCS_ERR_BUSY;
    } else {
        /* start waiting for this event */
        wait_ctx->cb_id = UCS_CALLBACKQ_ID_NULL;
        status          = UCS_INPROGRESS;
        entry->wait_ctx = wait_ctx;
    }

    ucs_spin_unlock(&dev->async_event_lock);
    return status;
}

void sct_ib_device_async_event_unregister(sct_ib_device_t *dev,
                                          enum ibv_event_type event_type,
                                          uint32_t resource_id)
{
    sct_ib_async_event_val_t *entry;
    sct_ib_async_event_t event;
    khiter_t iter;

    event.event_type  = event_type;
    event.resource_id = resource_id;

    ucs_spin_lock(&dev->async_event_lock);
    iter = kh_get(sct_ib_async_event, &dev->async_events_hash, event);
    ucg_assert(iter != kh_end(&dev->async_events_hash));
    entry = &kh_value(&dev->async_events_hash, iter);
    if ((entry->wait_ctx != NULL) &&
        (entry->wait_ctx->cb_id != UCS_CALLBACKQ_ID_NULL)) {
        /* cancel scheduled callback */
        ucs_callbackq_remove_safe(entry->wait_ctx->cbq, entry->wait_ctx->cb_id);
    }
    kh_del(sct_ib_async_event, &dev->async_events_hash, iter);
    ucs_spin_unlock(&dev->async_event_lock);
}

static void sct_ib_async_event_handler(int fd, ucs_event_set_types_t events,
                                       void *arg)
{
    sct_ib_device_t *dev = arg;
    struct ibv_async_event ibevent;
    sct_ib_async_event_t event;
    int ret;

    ret = ibv_get_async_event(dev->ibv_context, &ibevent);
    if (ret != 0) {
        if (errno != EAGAIN) {
            ucg_warn("ibv_get_async_event() failed: %m");
        }
        return;
    }

    event.event_type = ibevent.event_type;
    switch (event.event_type) {
        case IBV_EVENT_CQ_ERR:
            event.cookie = ibevent.element.cq;
            break;
        case IBV_EVENT_QP_FATAL:
        case IBV_EVENT_QP_REQ_ERR:
        case IBV_EVENT_QP_ACCESS_ERR:
        case IBV_EVENT_COMM_EST:
        case IBV_EVENT_SQ_DRAINED:
        case IBV_EVENT_PATH_MIG:
        case IBV_EVENT_PATH_MIG_ERR:
        case IBV_EVENT_QP_LAST_WQE_REACHED:
            event.qp_num = ibevent.element.qp->qp_num;
            break;
        case IBV_EVENT_SRQ_ERR:
        case IBV_EVENT_SRQ_LIMIT_REACHED:
            event.cookie = ibevent.element.srq;
            break;
        case IBV_EVENT_DEVICE_FATAL:
        case IBV_EVENT_PORT_ERR:
        case IBV_EVENT_PORT_ACTIVE:
    #if HAVE_DECL_IBV_EVENT_GID_CHANGE
        case IBV_EVENT_GID_CHANGE:
    #endif
        case IBV_EVENT_LID_CHANGE:
        case IBV_EVENT_PKEY_CHANGE:
        case IBV_EVENT_SM_CHANGE:
        case IBV_EVENT_CLIENT_REREGISTER:
            event.port_num = ibevent.element.port_num;
            break;
        default:
            break;
    };

    sct_ib_handle_async_event(dev, &event);
    ibv_ack_async_event(&ibevent);
}

void sct_ib_handle_async_event(sct_ib_device_t *dev, sct_ib_async_event_t *event)
{
    char event_info[EVENT_INFO_LEN_MAX];
    ucs_log_level_t level;

    switch (event->event_type) {
        case IBV_EVENT_CQ_ERR:
            snprintf(event_info, sizeof(event_info), "%s on CQ %p",
                    ibv_event_type_str(event->event_type), event->cookie);
            level = UCS_LOG_LEVEL_ERROR;
            break;
        case IBV_EVENT_COMM_EST:
        case IBV_EVENT_QP_ACCESS_ERR:
            snprintf(event_info, sizeof(event_info), "%s on QPN 0x%x",
                    ibv_event_type_str(event->event_type), event->qp_num);
            level = UCS_LOG_LEVEL_DIAG;
            break;
        case IBV_EVENT_QP_FATAL:
        case IBV_EVENT_QP_REQ_ERR:
        case IBV_EVENT_SQ_DRAINED:
        case IBV_EVENT_PATH_MIG:
        case IBV_EVENT_PATH_MIG_ERR:
            snprintf(event_info, sizeof(event_info), "%s on QPN 0x%x",
                    ibv_event_type_str(event->event_type), event->qp_num);
            level = UCS_LOG_LEVEL_ERROR;
            break;
        case IBV_EVENT_QP_LAST_WQE_REACHED:
            snprintf(event_info, sizeof(event_info), "SRQ-attached QP 0x%x was flushed",
                    event->qp_num);
            sct_ib_device_async_event_dispatch(dev, event);
            level = UCS_LOG_LEVEL_DEBUG;
            break;
        case IBV_EVENT_SRQ_ERR:
            level = UCS_LOG_LEVEL_ERROR;
            snprintf(event_info, sizeof(event_info), "%s on SRQ %p",
                    ibv_event_type_str(event->event_type), event->cookie);
            break;
        case IBV_EVENT_SRQ_LIMIT_REACHED:
            snprintf(event_info, sizeof(event_info), "%s on SRQ %p",
                    ibv_event_type_str(event->event_type), event->cookie);
            level = UCS_LOG_LEVEL_DEBUG;
            break;
        case IBV_EVENT_DEVICE_FATAL:
            snprintf(event_info, sizeof(event_info), "%s on port %d",
                    ibv_event_type_str(event->event_type), event->port_num);
            level = UCS_LOG_LEVEL_ERROR;
            break;
        case IBV_EVENT_PORT_ACTIVE:
        case IBV_EVENT_PORT_ERR:
            snprintf(event_info, sizeof(event_info), "%s on port %d",
                    ibv_event_type_str(event->event_type), event->port_num);
            level = UCS_LOG_LEVEL_DIAG;
            break;
    #if HAVE_DECL_IBV_EVENT_GID_CHANGE
        case IBV_EVENT_GID_CHANGE:
    #endif
        case IBV_EVENT_LID_CHANGE:
        case IBV_EVENT_PKEY_CHANGE:
        case IBV_EVENT_SM_CHANGE:
        case IBV_EVENT_CLIENT_REREGISTER:
            snprintf(event_info, sizeof(event_info), "%s on port %d",
                    ibv_event_type_str(event->event_type), event->port_num);
            level = UCS_LOG_LEVEL_WARN;
            break;
        default:
            snprintf(event_info, sizeof(event_info), "%s (%d)",
                    ibv_event_type_str(event->event_type), event->event_type);
            level = UCS_LOG_LEVEL_INFO;
            break;
    };

    ucs_log(level, "IB Async event on %s: %s", sct_ib_device_name(dev), event_info);
}

static void sct_ib_device_get_ids(sct_ib_device_t *dev)
{
    long vendor_id, device_id;

    if ((ucg_read_file_number(&vendor_id, 1, SCT_IB_DEVICE_SYSFS_FMT,
                              sct_ib_device_name(dev), "vendor") == UCS_OK) &&
        (ucg_read_file_number(&device_id, 1, SCT_IB_DEVICE_SYSFS_FMT,
                              sct_ib_device_name(dev), "device") == UCS_OK)) {
        dev->pci_id.vendor = vendor_id;
        dev->pci_id.device = device_id;
        ucg_debug("%s vendor_id: 0x%x device_id: %d", sct_ib_device_name(dev),
                  dev->pci_id.vendor, dev->pci_id.device);
    } else {
        dev->pci_id.vendor = 0;
        dev->pci_id.device = 0;
        ucg_warn("%s: could not read device/vendor id from sysfs, "
                 "performance may be affected", sct_ib_device_name(dev));
    }
}

ucs_status_t sct_ib_device_query(sct_ib_device_t *dev,
                                 struct ibv_device *ibv_device)
{
    ucs_status_t status;
    uint8_t i;
    int ret;

    status = sct_ib_query_device(dev->ibv_context, &dev->dev_attr);
    if (status != UCS_OK) {
        return status;
    }

    /* Check device type */
    switch (ibv_device->node_type) {
        case IBV_NODE_SWITCH:
            dev->first_port = 0;
            dev->num_ports  = 1;
            break;
        case IBV_NODE_CA:
        default:
            dev->first_port = 1;
            dev->num_ports  = IBV_DEV_ATTR(dev, phys_port_cnt);
            break;
    }

    if (dev->num_ports > SCT_IB_DEV_MAX_PORTS) {
        ucg_debug("%s has %d ports, but only up to %d are supported",
                  ibv_get_device_name(ibv_device), dev->num_ports,
                  SCT_IB_DEV_MAX_PORTS);
        dev->num_ports = SCT_IB_DEV_MAX_PORTS;
    }

    /* Query all ports */
    for (i = 0; i < dev->num_ports; ++i) {
        ret = ibv_query_port(dev->ibv_context, i + dev->first_port,
                             &dev->port_attr[i]);
        if (ret != 0) {
            ucg_error("ibv_query_port() returned %d: %m", ret);
            return UCS_ERR_IO_ERROR;
        }
    }

    sct_ib_device_get_ids(dev);

    return UCS_OK;
}

ucs_status_t sct_ib_device_init(sct_ib_device_t *dev, uint8_t use_uct_md,
                                struct ibv_device *ibv_device, int async_events)
{
    ucs_status_t status;

    dev->async_events = async_events;

    sct_ib_device_get_locality(ibv_get_device_name(ibv_device), &dev->local_cpus,
                               &dev->numa_node);

    if (!use_uct_md) {
        status = ucs_sys_fcntl_modfl(dev->ibv_context->async_fd, O_NONBLOCK, 0);
        if (status != UCS_OK) {
            goto err;
        }

        /* Register to IB async events */
        if (dev->async_events) {
            status = ucs_async_set_event_handler(UCS_ASYNC_MODE_THREAD_MUTEX,
                                                 dev->ibv_context->async_fd,
                                                 UCS_EVENT_SET_EVREAD,
                                                 sct_ib_async_event_handler, dev,
                                                 NULL);
            if (status != UCS_OK) {
                goto err;
            }
        }
    }

    kh_init_inplace(sct_ib_async_event, &dev->async_events_hash);
    ucs_spinlock_init(&dev->async_event_lock, 0);

    ucg_debug("initialized device '%s' (%s) with %d ports", sct_ib_device_name(dev),
              ibv_node_type_str(ibv_device->node_type),
              dev->num_ports);
    return UCS_OK;

err:
    return status;
}

void sct_ib_device_cleanup(sct_ib_device_t *dev)
{
    ucg_debug("destroying ib device %s", sct_ib_device_name(dev));

    if (kh_size(&dev->async_events_hash) != 0) {
        ucg_warn("async_events_hash not empty");
    }

    kh_destroy_inplace(sct_ib_async_event, &dev->async_events_hash);
    ucs_spinlock_destroy(&dev->async_event_lock);

    if (dev->async_events) {
        ucs_async_remove_handler(dev->ibv_context->async_fd, 1);
    }
}

static inline int sct_ib_device_spec_match(sct_ib_device_t *dev,
                                           const sct_ib_device_spec_t *spec)
{
    return (spec->pci_id.vendor == dev->pci_id.vendor) &&
           (spec->pci_id.device == dev->pci_id.device);
}

const sct_ib_device_spec_t* sct_ib_device_spec(sct_ib_device_t *dev)
{
    sct_ib_device_spec_t *spec;

    /* search through built-in list of device specifications */
    spec = sct_ib_builtin_device_specs;
    while ((spec->name != NULL) && !sct_ib_device_spec_match(dev, spec)) {
        ++spec;
    }
    return spec; /* if no match is found, return the last entry, which contains
                    default settings for unknown devices */
}

static size_t sct_ib_device_get_ib_gid_index(sct_ib_md_t *md)
{
    if (md->config.gid_index == UCS_ULUNITS_AUTO) {
        return SCT_IB_MD_DEFAULT_GID_INDEX;
    } else {
        return md->config.gid_index;
    }
}

static int sct_ib_device_is_iwarp(sct_ib_device_t *dev)
{
    return dev->ibv_context->device->transport_type == IBV_TRANSPORT_IWARP;
}

ucs_status_t sct_ib_device_port_check(sct_ib_device_t *dev, uint8_t port_num,
                                      unsigned flags)
{
    sct_ib_md_t *md = ucs_container_of(dev, sct_ib_md_t, dev);
    ucs_status_t status;
    union ibv_gid gid;

    if (port_num < dev->first_port || port_num >= dev->first_port + dev->num_ports) {
        return UCS_ERR_NO_DEVICE;
    }

    if (sct_ib_device_port_attr(dev, port_num)->gid_tbl_len == 0) {
        ucg_debug("%s:%d has no gid", sct_ib_device_name(dev),
                  port_num);
        return UCS_ERR_UNSUPPORTED;
    }

    if (sct_ib_device_port_attr(dev, port_num)->state != IBV_PORT_ACTIVE) {
        ucs_trace("%s:%d is not active (state: %d)", sct_ib_device_name(dev),
                  port_num, sct_ib_device_port_attr(dev, port_num)->state);
        return UCS_ERR_UNREACHABLE;
    }

    if (sct_ib_device_is_iwarp(dev)) {
        ucg_debug("iWarp device %s is not supported", sct_ib_device_name(dev));
        return UCS_ERR_UNSUPPORTED;
    }

    if (!sct_ib_device_is_port_ib(dev, port_num) && (flags & UCT_IB_DEVICE_FLAG_LINK_IB)) {
        ucg_debug("%s:%d is not IB link layer", sct_ib_device_name(dev),
                  port_num);
        return UCS_ERR_UNSUPPORTED;
    }

    if (md->check_subnet_filter && sct_ib_device_is_port_ib(dev, port_num)) {
        status = sct_ib_device_query_gid(dev, port_num,
                                         sct_ib_device_get_ib_gid_index(md), &gid);
        if (status != UCS_OK) {
            return status;
        }

        if (md->subnet_filter != gid.global.subnet_prefix) {
            ucs_trace("%s:%d subnet_prefix does not match",
                      sct_ib_device_name(dev), port_num);
            return UCS_ERR_UNSUPPORTED;
        }
    }

    return UCS_OK;
}

const char *sct_ib_roce_version_str(sct_ib_roce_version_t roce_ver)
{
    switch (roce_ver) {
        case UCT_IB_DEVICE_ROCE_V1:
            return "RoCE v1";
        case UCT_IB_DEVICE_ROCE_V1_5:
            return "RoCE v1.5";
        case UCT_IB_DEVICE_ROCE_V2:
            return "RoCE v2";
        default:
            return "<unknown RoCE version>";
    }
}

const char *sct_ib_gid_str(const union ibv_gid *gid, char *str, size_t max_size)
{
    inet_ntop(AF_INET6, gid, str, max_size);
    return str;
}

static int sct_ib_device_is_addr_ipv4_mcast(const struct in6_addr *raw,
                                            const uint32_t addr_last_bits)
{
    /* IPv4 encoded multicast addresses */
    return (raw->s6_addr32[0] == htonl(0xff0e0000)) &&
           !(raw->s6_addr32[1] | addr_last_bits);
}

static sa_family_t sct_ib_device_get_addr_family(union ibv_gid *gid, int gid_index)
{
    const struct in6_addr *raw    = (struct in6_addr *)gid->raw;
    const uint32_t addr_last_bits = raw->s6_addr32[2] ^ htonl(0x0000ffff);
    char p[128];

    ucs_trace_func("testing addr_family on gid index %d: %s",
                   gid_index, sct_ib_gid_str(gid, p, sizeof(p)));

    if (!((raw->s6_addr32[0] | raw->s6_addr32[1]) | addr_last_bits) ||
        sct_ib_device_is_addr_ipv4_mcast(raw, addr_last_bits)) {
        return AF_INET;
    } else {
        return AF_INET6;
    }
}

ucs_status_t sct_ib_device_query_gid_info(struct ibv_context *ctx, const char *dev_name,
                                          uint8_t port_num, unsigned gid_index,
                                          sct_ib_device_gid_info_t *info)
{
    int ret;
    char buf[16];
    static char RoCE_v1[] = "IB/RoCE v1";
    static char RoCE_v2[] = "RoCE v2";

    ret = ibv_query_gid(ctx, port_num, gid_index, &info->gid);
    if (ret == 0) {
        ret = ucs_read_file(buf, sizeof(buf) - 1, 1,
                            SCT_IB_DEVICE_SYSFS_GID_TYPE_FMT,
                            dev_name, port_num, gid_index);
        if (ret > 0) {
            if (!strncmp(buf, RoCE_v1, strlen(RoCE_v1))) {
                info->roce_info.ver = UCT_IB_DEVICE_ROCE_V1;
            } else if (!strncmp(buf, RoCE_v2, strlen(RoCE_v2))) {
                info->roce_info.ver = UCT_IB_DEVICE_ROCE_V2;
            } else {
                ucg_error("failed to parse gid type '%s' (dev=%s port=%d index=%d)",
                          buf, dev_name, port_num, gid_index);
                return UCS_ERR_INVALID_PARAM;
            }
        } else {
            info->roce_info.ver = UCT_IB_DEVICE_ROCE_V1;
        }

        info->roce_info.addr_family =
                        sct_ib_device_get_addr_family(&info->gid, gid_index);
        info->gid_index            = gid_index;
        return UCS_OK;
    }

    ucg_error("ibv_query_gid(dev=%s port=%d index=%d) failed: %m",
              dev_name, port_num, gid_index);
    return UCS_ERR_INVALID_PARAM;
}

int sct_ib_device_test_roce_gid_index(sct_ib_device_t *dev, uint8_t port_num,
                                      const union ibv_gid *gid,
                                      uint8_t gid_index)
{
    struct ibv_ah_attr ah_attr;
    struct ibv_ah *ah;

    ucg_assert(sct_ib_device_is_port_roce(dev, port_num));

    memset(&ah_attr, 0, sizeof(ah_attr));
    ah_attr.port_num       = port_num;
    ah_attr.is_global      = 1;
    ah_attr.grh.dgid       = *gid;
    ah_attr.grh.sgid_index = gid_index;
    ah_attr.grh.hop_limit  = 255;   /* set max limit of hop times = 255 */
    ah_attr.grh.flow_label = 1;
    ah_attr.dlid           = SCT_IB_ROCE_UDP_SRC_PORT_BASE;

    ah = ibv_create_ah(ucs_container_of(dev, sct_ib_md_t, dev)->pd, &ah_attr);
    if (ah == NULL) {
        return 0; /* gid entry is not operational */
    }

    ibv_destroy_ah(ah);
    return 1;
}

ucs_status_t sct_ib_device_select_gid(sct_ib_device_t *dev, uint8_t port_num,
                                      sct_ib_device_gid_info_t *gid_info)
{
    static const sct_ib_roce_version_info_t roce_prio[] = {
        {UCT_IB_DEVICE_ROCE_V2, AF_INET},
        {UCT_IB_DEVICE_ROCE_V2, AF_INET6},
        {UCT_IB_DEVICE_ROCE_V1, AF_INET},
        {UCT_IB_DEVICE_ROCE_V1, AF_INET6}
    };
    int gid_tbl_len         = sct_ib_device_port_attr(dev, port_num)->gid_tbl_len;
    ucs_status_t status     = UCS_OK;
    int priorities_arr_len  = ucs_static_array_size(roce_prio);
    sct_ib_device_gid_info_t gid_info_tmp;
    int i, prio_idx;

    ucg_assert(sct_ib_device_is_port_roce(dev, port_num));

    /* search for matching GID table entries, according to the order defined
     * in priorities array
     */
    for (prio_idx = 0; prio_idx < priorities_arr_len; prio_idx++) {
        for (i = 0; i < gid_tbl_len; i++) {
            status = sct_ib_device_query_gid_info(dev->ibv_context,
                                                  sct_ib_device_name(dev),
                                                  port_num, i, &gid_info_tmp);
            if (status != UCS_OK) {
                goto out;
            }

            if ((roce_prio[prio_idx].ver         == gid_info_tmp.roce_info.ver) &&
                (roce_prio[prio_idx].addr_family == gid_info_tmp.roce_info.addr_family) &&
                sct_ib_device_test_roce_gid_index(dev, port_num, &gid_info_tmp.gid, i)) {
                gid_info->gid_index = i;
                gid_info->roce_info = gid_info_tmp.roce_info;
                goto out_print;
            }
        }
    }

    gid_info->gid_index             = SCT_IB_MD_DEFAULT_GID_INDEX;
    gid_info->roce_info.ver         = UCT_IB_DEVICE_ROCE_V1;
    gid_info->roce_info.addr_family = AF_INET;

out_print:
    ucg_debug("%s:%d using gid_index %d", sct_ib_device_name(dev), port_num,
              gid_info->gid_index);
out:
    return status;
}

int sct_ib_device_is_port_ib(sct_ib_device_t *dev, uint8_t port_num)
{
    return sct_ib_device_port_attr(dev, port_num)->link_layer == IBV_LINK_LAYER_INFINIBAND;
}

int sct_ib_device_is_port_roce(sct_ib_device_t *dev, uint8_t port_num)
{
    return IBV_PORT_IS_LINK_LAYER_ETHERNET(sct_ib_device_port_attr(dev, port_num));
}

const char *sct_ib_device_name(sct_ib_device_t *dev)
{
    return ibv_get_device_name(dev->ibv_context->device);
}

size_t sct_ib_mtu_value(enum ibv_mtu mtu)
{
    switch (mtu) {
        case IBV_MTU_256:
            return 256;
        case IBV_MTU_512:
            return 512;
        case IBV_MTU_1024:
            return 1024;
        case IBV_MTU_2048:
            return 2048;
        case IBV_MTU_4096:
            return 4096;
    }
    ucg_fatal("Invalid MTU value (%d)", mtu);
    return -1;
}

uint8_t sct_ib_to_qp_fabric_time(double t)
{
    double to;

    to = log(t / 4.096e-6) / log(2.0);
    if (to < 1) {
        return 1; /* Very small timeout */
    } else if ((long)(to + 0.5) >= SCT_IB_FABRIC_TIME_MAX) {
        return 0; /* No timeout */
    } else {
        return (long)(to + 0.5);
    }
}

uint8_t sct_ib_to_rnr_fabric_time(double t)
{
    double time_ms = t * UCS_MSEC_PER_SEC;
    uint8_t idx, next_index;
    double avg_ms;

    for (idx = 1; idx < SCT_IB_FABRIC_TIME_MAX; idx++) {
        next_index = (idx + 1) % SCT_IB_FABRIC_TIME_MAX;

        if (time_ms <= sct_ib_qp_rnr_time_ms[next_index]) {
            avg_ms = (sct_ib_qp_rnr_time_ms[idx] +
                      sct_ib_qp_rnr_time_ms[next_index]) * 0.5;

            if (time_ms < avg_ms) {
                /* return previous index */
                return idx;
            } else {
                /* return current index */
                return next_index;
            }
        }
    }

    return 0; /* this is a special value that means the maximum value */
}

ucs_status_t sct_ib_modify_qp(struct ibv_qp *qp, enum ibv_qp_state state)
{
    struct ibv_qp_attr qp_attr;

    ucg_debug("modify QP 0x%x to state %d", qp->qp_num, state);
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = state;
    if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE)) {
        ucg_warn("modify qp 0x%x to state %d failed: %m", qp->qp_num, state);
        return UCS_ERR_IO_ERROR;
    }

    return UCS_OK;
}

static ucs_sys_device_t sct_ib_device_get_sys_dev(sct_ib_device_t *dev)
{
    char path_buffer[PATH_MAX], *resolved_path;
    ucs_sys_device_t sys_dev;
    ucs_sys_bus_id_t bus_id;
    ucs_status_t status;
    char *pcie_bus;
    int num_fields;

    /* realpath name is of form /sys/devices/.../0000:05:00.0/infiniband/mlx5_0
     * and bus_id is constructed from 0000:05:00.0 */

    resolved_path = realpath(dev->ibv_context->device->ibdev_path, path_buffer);
    if (resolved_path == NULL) {
        return UCS_SYS_DEVICE_ID_UNKNOWN;
    }

    /* Make sure there is "/infiniband/" substring in path_buffer */
    if (strstr(path_buffer, "/infiniband/") == NULL) {
        return UCS_SYS_DEVICE_ID_UNKNOWN;
    }

    pcie_bus   = basename(dirname(dirname(path_buffer)));
    num_fields = sscanf(pcie_bus, "%hx:%hhx:%hhx.%hhx", &bus_id.domain,
                        &bus_id.bus, &bus_id.slot, &bus_id.function);
    if (num_fields != 4) {
        return UCS_SYS_DEVICE_ID_UNKNOWN;
    }

    status = ucs_topo_find_device_by_bus_id(&bus_id, &sys_dev);
    if (status != UCS_OK) {
        return UCS_SYS_DEVICE_ID_UNKNOWN;
    }

    ucg_debug("%s bus id %hu:%hhu:%hhu.%hhu sys_dev %d",
              sct_ib_device_name(dev), bus_id.domain, bus_id.bus, bus_id.slot,
              bus_id.function, sys_dev);
    return sys_dev;
}

ucs_status_t sct_ib_device_query_ports(sct_ib_device_t *dev, unsigned flags,
                                       sct_tl_device_resource_t **tl_devices_p,
                                       unsigned *num_tl_devices_p)
{
    sct_tl_device_resource_t *tl_devices;
    unsigned num_tl_devices;
    ucs_status_t status;
    uint8_t port_num;

    /* Allocate resources array
     * We may allocate more memory than really required, but it's not so bad. */
    tl_devices = ucg_calloc(dev->num_ports, sizeof(*tl_devices), "ib device resource");
    if (tl_devices == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto err;
    }

    /* Second pass: fill port information */
    num_tl_devices = 0;
    for (port_num = dev->first_port; port_num < dev->first_port + dev->num_ports;
         ++port_num) {
        /* Check port capabilities */
        status = sct_ib_device_port_check(dev, port_num, flags);
        if (status != UCS_OK) {
           ucs_trace("%s:%d does not support flags 0x%x: %s",
                     sct_ib_device_name(dev), port_num, flags,
                     ucs_status_string(status));
           continue;
        }

        /* Save device information */
        ucs_snprintf_zero(tl_devices[num_tl_devices].name,
                          sizeof(tl_devices[num_tl_devices].name),
                          "%s:%d", sct_ib_device_name(dev), port_num);
        tl_devices[num_tl_devices].type       = UCT_DEVICE_TYPE_NET;
        tl_devices[num_tl_devices].sys_device = sct_ib_device_get_sys_dev(dev);
        ++num_tl_devices;
    }

    if (num_tl_devices == 0) {
        ucg_debug("no compatible IB ports found for flags 0x%x", flags);
        status = UCS_ERR_NO_DEVICE;
        goto err_free;
    }

    *num_tl_devices_p = num_tl_devices;
    *tl_devices_p     = tl_devices;
    return UCS_OK;

err_free:
    ucg_free(tl_devices);
err:
    return status;
}

ucs_status_t sct_ib_device_find_port(sct_ib_device_t *dev,
                                     const char *resource_dev_name,
                                     uint8_t *p_port_num)
{
    const char *ibdev_name;
    unsigned port_num;
    size_t devname_len;
    char *p;

    p = strrchr(resource_dev_name, ':');
    if (p == NULL) {
        goto err; /* Wrong device name format */
    }
    devname_len = p - resource_dev_name;

    ibdev_name = sct_ib_device_name(dev);
    if ((strlen(ibdev_name) != devname_len) ||
        strncmp(ibdev_name, resource_dev_name, devname_len)) {
        goto err; /* Device name is wrong */
    }

    port_num = strtod(p + 1, &p);
    if (*p != '\0') {
        goto err; /* Failed to parse port number */
    }
    if ((port_num < dev->first_port) || (port_num >= dev->first_port + dev->num_ports)) {
        goto err; /* Port number out of range */
    }

    *p_port_num = port_num;
    return UCS_OK;

err:
    ucg_error("%s: failed to find port", resource_dev_name);
    return UCS_ERR_NO_DEVICE;
}

int sct_ib_device_is_gid_raw_empty(uint8_t *gid_raw)
{
    return (*(uint64_t *)gid_raw == 0) && (*(uint64_t *)(gid_raw + 8) == 0);
}

ucs_status_t sct_ib_device_query_gid(sct_ib_device_t *dev, uint8_t port_num,
                                     unsigned gid_index, union ibv_gid *gid)
{
    sct_ib_device_gid_info_t gid_info;
    ucs_status_t status;

    status = sct_ib_device_query_gid_info(dev->ibv_context, sct_ib_device_name(dev),
                                          port_num, gid_index, &gid_info);
    if (status != UCS_OK) {
        return status;
    }

    if (sct_ib_device_is_gid_raw_empty(gid_info.gid.raw)) {
        ucg_error("Invalid gid[%d] on %s:%d", gid_index,
                  sct_ib_device_name(dev), port_num);
        return UCS_ERR_INVALID_ADDR;
    }

    *gid = gid_info.gid;
    return UCS_OK;
}

ucs_status_t sct_ib_device_get_roce_ndev_name(sct_ib_device_t *dev, uint8_t port_num,
                                              uint8_t gid_index, char *ndev_name, size_t max)
{
    ssize_t nread;

    /* get the network device name which corresponds to a RoCE port */
    nread = ucs_read_file_str(ndev_name, max, 1,
                              SCT_IB_DEVICE_SYSFS_GID_NDEV_FMT,
                              sct_ib_device_name(dev), port_num, gid_index);
    if (nread < 0) {
        ucs_diag("failed to read " SCT_IB_DEVICE_SYSFS_GID_NDEV_FMT": %m",
                 sct_ib_device_name(dev), port_num, 0);
        return UCS_ERR_NO_DEVICE;
    }

    ucs_strtrim(ndev_name);
    return UCS_OK;
}

unsigned sct_ib_device_get_roce_lag_level(sct_ib_device_t *dev, uint8_t port_num,
                                          uint8_t gid_index)
{
    char ndev_name[IFNAMSIZ];
    unsigned roce_lag_level;
    ucs_status_t status;

    status = sct_ib_device_get_roce_ndev_name(dev, port_num, gid_index,
                                              ndev_name, sizeof(ndev_name));
    if (status != UCS_OK) {
        return 1;
    }

    roce_lag_level = ucs_netif_bond_ad_num_ports(ndev_name);
    ucg_debug("RoCE LAG level on %s:%d (%s) is %u", sct_ib_device_name(dev),
              port_num, ndev_name, roce_lag_level);
    return roce_lag_level;
}

const char* sct_ib_ah_attr_str(char *buf, size_t max,
                               const struct ibv_ah_attr *ah_attr)
{
    char *p    = buf;
    char *endp = buf + max;

    snprintf(p, endp - p, "dlid=%d sl=%d port=%d src_path_bits=%d",
             ah_attr->dlid, ah_attr->sl,
             ah_attr->port_num, ah_attr->src_path_bits);
    p += strlen(p);

    if (ah_attr->is_global) {
        snprintf(p, endp - p, " dgid=");
        p += strlen(p);
        sct_ib_gid_str(&ah_attr->grh.dgid, p, endp - p);
        p += strlen(p);
        snprintf(p, endp - p, " sgid_index=%d traffic_class=%d",
                 ah_attr->grh.sgid_index, ah_attr->grh.traffic_class);
    }

    return buf;
}
