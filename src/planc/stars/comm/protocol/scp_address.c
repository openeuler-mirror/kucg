/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "scp_address.h"

#define SCP_ADDRESS_FLAG_LAST         0x80u  /* Last address in the list */
#define SCP_ADDRESS_FLAG_HAVE_PATHS   0x40u  /* For device address:
                                                Indicates that number of paths on the
                                                device is packed right after device
                                                address, otherwise number of paths
                                                defaults to 1. */
#define SCP_ADDRESS_FLAG_LEN_MASK     (UCS_MASK(8) ^ \
                                        (SCP_ADDRESS_FLAG_HAVE_PATHS  | \
                                         SCP_ADDRESS_FLAG_LAST))

#define SCP_ADDRESS_FLAG_MD_EMPTY_DEV 0x80u  /* Device without TL addresses */
#define SCP_ADDRESS_FLAG_MD_ALLOC     0x40u  /* MD can register  */
#define SCP_ADDRESS_FLAG_MD_REG       0x20u  /* MD can allocate */
#define SCP_ADDRESS_FLAG_MD_MASK      (UCS_MASK(8) ^ \
                                        (SCP_ADDRESS_FLAG_MD_EMPTY_DEV | \
                                         SCP_ADDRESS_FLAG_MD_ALLOC | \
                                         SCP_ADDRESS_FLAG_MD_REG))

static UCS_F_ALWAYS_INLINE
void print_scp_unpacked_address(scp_unpacked_address_h address)
{
    ucg_debug("address_count %d", address->address_count);

    scp_address_entry_h entry;

    for (uint8_t index = 0; index < address->address_count; ++index) {
        entry = &address->address_list[index];
        ucg_debug("dev_addr %p iface_addr %p iface_attr %p md_flags %lu dev_num_paths %u, "
                  "md_index %d dev_index %d",
                  entry->dev_addr, entry->iface_addr, entry->iface_attr, entry->md_flags,
                  entry->dev_num_paths, entry->md_index, entry->dev_index);
    }
    return;
}

static UCS_F_ALWAYS_INLINE
uint64_t scp_worker_iface_can_connect(sct_iface_attr_t *attrs)
{
    return attrs->cap.flags &
           (UCT_IFACE_FLAG_CONNECT_TO_IFACE | UCT_IFACE_FLAG_CONNECT_TO_EP);
}

static UCS_F_ALWAYS_INLINE void*
scp_address_pack_length(scp_worker_h worker, void *ptr, size_t addr_length)
{
    ucg_assert(addr_length <= SCP_ADDRESS_FLAG_LEN_MASK);
    *(uint8_t*)ptr = addr_length;
    ucg_debug("pack addr_length %d", *(uint8_t*)ptr);

    return UCS_PTR_TYPE_OFFSET(ptr, uint8_t);
}

static int scp_address_pack_iface_attr(const sct_iface_attr_t *iface_attr, scp_rsc_index_t rsc_index, void *ptr)
{
    scp_address_iface_attr_t  *attr;

    /* check if at least one of bandwidth values is 0 */
    if ((iface_attr->bandwidth.dedicated * iface_attr->bandwidth.shared) != 0) {
        ucg_error("Incorrect bandwidth value: one of bandwidth dedicated/shared must be zero");
        return -1;
    }

    attr                        = ptr;
    attr->cap_flags             = iface_attr->cap.flags;
    attr->event_flags           = iface_attr->cap.event_flags;
    attr->overhead              = iface_attr->overhead;
    attr->bandwidth.dedicated   = iface_attr->bandwidth.dedicated;
    attr->bandwidth.shared      = iface_attr->bandwidth.shared;
    attr->priority              = iface_attr->priority;
    attr->lat_ovh               = iface_attr->latency.c;
    attr->dst_rsc_index         = rsc_index;

    return sizeof(scp_address_iface_attr_t);
}

static ucg_status_t scp_address_pack_iface(scp_worker_h worker, scp_rsc_index_t rsc_index,
                                           void **buffer, void **last)
{
    scp_worker_iface_t *wiface = scp_worker_iface(worker, rsc_index);
    ucg_assert(wiface != NULL);

    sct_iface_attr_t *iface_attr = &wiface->attr;
    if (!scp_worker_iface_can_connect(iface_attr)) {
        return UCG_ERR_INVALID_ADDR;
    }

    void *ptr = *buffer;
    /* Transport information */
    int attr_len = scp_address_pack_iface_attr(iface_attr, rsc_index, ptr);
    UCG_ASSERT_RET(attr_len >= 0, UCG_ERR_INVALID_ADDR);

    ptr = UCS_PTR_BYTE_OFFSET(ptr, attr_len);

    /* Pack iface address */
    size_t iface_addr_len = iface_attr->iface_addr_len;
    *last = ptr;
    ptr = scp_address_pack_length(worker, ptr, iface_addr_len);
    if (iface_addr_len > 0) {
        ucs_status_t ucs_status =
            sct_iface_get_address(wiface->iface, (uct_iface_addr_t*)ptr);
        UCG_ASSERT_RET(ucs_status == UCS_OK, ucg_status_s2g(ucs_status));

        ptr = UCS_PTR_BYTE_OFFSET(ptr, iface_addr_len);
    }

    *buffer = ptr;
    return UCG_OK;
}

static ucg_status_t scp_address_pack_device(scp_worker_h worker,
                                            const scp_address_packed_device_t *dev,
                                            const scp_address_packed_device_t *last_dev,
                                            void **buffer)
{
    ucg_status_t status;
    scp_context_h context = worker->context;
    scp_md_index_t md_idx = context->tl_rscs[dev->rsc_index].md_index;
    ucg_assert(md_idx <= SCP_ADDRESS_FLAG_MD_MASK);

    uint64_t md_flags = context->tl_mds[md_idx].attr.cap.flags &
                        (UCT_MD_FLAG_REG | UCT_MD_FLAG_ALLOC);
    uint64_t dev_tl_bitmap = context->tl_bitmap & dev->tl_bitmap;

    void *ptr = *buffer;
    *(uint8_t*)ptr = md_idx |
                     ((dev_tl_bitmap == 0)           ? SCP_ADDRESS_FLAG_MD_EMPTY_DEV : 0) |
                     ((md_flags & UCT_MD_FLAG_ALLOC) ? SCP_ADDRESS_FLAG_MD_ALLOC     : 0) |
                     ((md_flags & UCT_MD_FLAG_REG)   ? SCP_ADDRESS_FLAG_MD_REG       : 0);
    ptr = UCS_PTR_TYPE_OFFSET(ptr, md_idx);

    /* Device address length */
    *(uint8_t*)ptr = (dev == last_dev) ? SCP_ADDRESS_FLAG_LAST : 0;
    ucg_assert(dev->dev_addr_len <= SCP_ADDRESS_FLAG_LEN_MASK);
    *(uint8_t*)ptr |= dev->dev_addr_len;

    /* Device number of paths flag and value */
    ucg_assert(dev->num_paths >= 1 && dev->num_paths <= UINT8_MAX);

    if (dev->num_paths > 1) {
        *(uint8_t*)ptr |= SCP_ADDRESS_FLAG_HAVE_PATHS;
        ptr = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);
        *(uint8_t*)ptr = dev->num_paths;
    }
    ptr = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);

    /* Device address */
    scp_worker_iface_t *wiface = scp_worker_iface(worker, dev->rsc_index);
    ucs_status_t ucs_status =
        sct_iface_get_device_address(wiface->iface, (uct_device_addr_t*)ptr);
    UCG_ASSERT_RET(ucs_status == UCS_OK, ucg_status_s2g(ucs_status));

    ptr = UCS_PTR_BYTE_OFFSET(ptr, dev->dev_addr_len);

    void *last_tl_ptr = NULL;
    scp_rsc_index_t rsc_idx;
    ucs_for_each_bit(rsc_idx, dev_tl_bitmap) {
        status = scp_address_pack_iface(worker, rsc_idx, &ptr, &last_tl_ptr);
        UCG_ASSERT_CODE_RET(status);
    }

    UCG_ASSERT_RET(last_tl_ptr != NULL, UCG_ERR_INVALID_ADDR);

    *(uint8_t*)last_tl_ptr |= SCP_ADDRESS_FLAG_LAST;
    *buffer = ptr;
    return UCG_OK;
}

static scp_address_packed_device_t*
scp_address_get_device(scp_context_h context, scp_rsc_index_t rsc_index,
                       scp_address_packed_device_t *devices,
                       scp_rsc_index_t *num_devices_p)
{
    const scp_tl_resource_desc_t *tl_rsc = context->tl_rscs;
    scp_address_packed_device_t *dev;

    for (dev = devices; dev < devices + *num_devices_p; ++dev) {
        if ((tl_rsc[rsc_index].md_index == tl_rsc[dev->rsc_index].md_index) &&
            !strcmp(tl_rsc[rsc_index].tl_rsc.dev_name,
                    tl_rsc[dev->rsc_index].tl_rsc.dev_name)) {
            ucg_debug("already exist device name %s", tl_rsc[rsc_index].tl_rsc.dev_name);
            goto out;
        }
    }

    dev = &devices[(*num_devices_p)++];
    memset(dev, 0, sizeof(*dev));
out:
    return dev;
}

static ucg_status_t scp_address_gather_devices(scp_worker_h worker,
                                               scp_address_packed_device_t **devices_p,
                                               scp_rsc_index_t *num_devices_p)
{
    scp_context_h context = worker->context;
    scp_address_packed_device_t *devices =
        ucg_calloc(context->num_tls, sizeof(*devices), "packed_devices");
    UCG_ASSERT_RET(devices != NULL, UCG_ERR_NO_MEMORY);

    ucg_debug("statistic malloc devices %lu , num_tls %d",
              context->num_tls * sizeof(*devices), context->num_tls);

    scp_address_packed_device_t *dev;
    sct_iface_attr_t *iface_attr;
    scp_rsc_index_t rsc_idx;

    ucs_for_each_bit(rsc_idx, context->tl_bitmap) {
        iface_attr = scp_worker_iface_get_attr(worker, rsc_idx);
        if (!scp_worker_iface_can_connect(iface_attr)) {
            continue;
        }

        dev = scp_address_get_device(context, rsc_idx, devices, num_devices_p);

        /* iface address (its length will be packed in non-unified mode only) */
        dev->tl_addrs_size += 1;
        dev->tl_addrs_size += iface_attr->iface_addr_len;
        dev->tl_addrs_size += sizeof(scp_address_iface_attr_t);
        dev->dev_addr_len = iface_attr->device_addr_len;

        if (iface_attr->dev_num_paths > UINT8_MAX) {
            ucg_error("only up to %d paths are supported by address pack (got: %u)",
                      UINT8_MAX, iface_attr->dev_num_paths);
            ucg_free(devices);
            return UCG_ERR_UNSUPPORTED;
        }

        dev->rsc_index  = rsc_idx;
        dev->tl_bitmap |= UCS_BIT(rsc_idx);
        dev->num_paths  = iface_attr->dev_num_paths;
    }

    *devices_p = devices;
    return UCG_OK;
}

static size_t scp_address_packed_size(scp_worker_h worker,
                                      const scp_address_packed_device_t *devices,
                                      uint8_t num_devices,
                                      size_t *packed_offset)
{
    *packed_offset = sizeof(scp_unpacked_address_t)
        + num_devices * sizeof(scp_address_entry_t);
    size_t size = *packed_offset;
    const scp_address_packed_device_t *dev;

    if (num_devices == 0) {
        return size;             /* Only scp_unpacked_address_t */
    }

    for (dev = devices; dev < (devices + num_devices); ++dev) {
        size += 1;                  /* device md_index */
        size += 1;                  /* device address length */
        size += dev->dev_addr_len;  /* device address */

        if (dev->num_paths > 1) {
            size += 1;              /* number of paths */
        }
        size += dev->tl_addrs_size; /* transport addresses */
    }

    return size;
}

static ucg_status_t scp_address_do_pack(scp_worker_h worker,
                                        void *buffer, size_t size,
                                        const scp_address_packed_device_t *devices,
                                        uint8_t num_devices)
{
    if (num_devices == 0) {
        goto out;
    }

    ucg_status_t status;
    void *ptr = buffer;
    const scp_address_packed_device_t *last_dev = devices + num_devices - 1;
    for (const scp_address_packed_device_t *dev = devices;
         dev < (devices + num_devices); ++dev) {
        status = scp_address_pack_device(worker, dev, last_dev, &ptr);
        UCG_ASSERT_CODE_RET(status);
    }

out:
    ucg_assert(UCS_PTR_BYTE_OFFSET(buffer, size) == ptr);
    return UCG_OK;
}

ucg_status_t scp_address_pack(scp_worker_h worker, void **buffer_p, size_t *size_p)
{
    scp_address_packed_device_t *devices;
    uint8_t num_devices = 0;

    /* Collect all devices we want to pack */
    ucg_status_t status = scp_address_gather_devices(worker, &devices, &num_devices);
    UCG_ASSERT_CODE_RET(status);

    /* Calculate packed size */
    size_t packed_offset;
    size_t size = scp_address_packed_size(worker, devices, num_devices, &packed_offset);

    /* Allocate address */
    void *buffer = ucg_malloc(size, "scp_address");
    UCG_ASSERT_GOTO(buffer != NULL, free_devices, UCG_ERR_NO_MEMORY);

    /* Pack the address */
    status = scp_address_do_pack(worker, buffer + packed_offset,
                                 size - packed_offset, devices, num_devices);
    UCG_ASSERT_CODE_GOTO(status, free_buf);

    scp_unpacked_address_h unpacked = (scp_unpacked_address_h)buffer;
    unpacked->flag          = 0;
    unpacked->address_count = num_devices;
    unpacked->uuid          = worker->uuid;

    *size_p   = size;
    *buffer_p = buffer;

    ucg_free(devices);
    return UCG_OK;

free_buf:
    ucg_free(buffer);
free_devices:
    ucg_free(devices);
    return status;
}

static void* scp_address_unpack_length(const void *ptr, size_t *addr_length, int *is_last)
{
    if (is_last != NULL) {
        *is_last = *(uint8_t*)ptr & SCP_ADDRESS_FLAG_LAST;
    }

    *addr_length = *(uint8_t*)ptr & SCP_ADDRESS_FLAG_LEN_MASK;
    return UCS_PTR_TYPE_OFFSET(ptr, uint8_t);
}

static ucg_status_t scp_address_unpack_device(scp_address_entry_h *address_entry,
                                              scp_rsc_index_t *dev_index,
                                              int *last_dev,
                                              void **buffer)
{
    scp_address_entry_h address = *address_entry;
    const uct_device_addr_t *dev_addr;
    scp_md_index_t md_index;
    unsigned dev_num_paths;
    size_t iface_addr_len;
    size_t dev_addr_len;
    void *ptr = *buffer;
    uint64_t md_flags;
    uint8_t md_byte;
    int empty_dev;
    int last_tl;

    /* md_index */
    md_byte      = (*(uint8_t*)ptr);
    md_index     = md_byte & SCP_ADDRESS_FLAG_MD_MASK; // low 5bit md_index
    md_flags     = (md_byte & SCP_ADDRESS_FLAG_MD_ALLOC) ? UCT_MD_FLAG_ALLOC : 0;
    md_flags    |= (md_byte & SCP_ADDRESS_FLAG_MD_REG)   ? UCT_MD_FLAG_REG   : 0;
    empty_dev    = md_byte & SCP_ADDRESS_FLAG_MD_EMPTY_DEV;
    ptr          = UCS_PTR_TYPE_OFFSET(ptr, md_byte);

    /* device address length */
    dev_addr_len = (*(uint8_t*)ptr) & SCP_ADDRESS_FLAG_LEN_MASK;
    *last_dev     = (*(uint8_t*)ptr) & SCP_ADDRESS_FLAG_LAST;
    if ((*(uint8_t*)ptr) & SCP_ADDRESS_FLAG_HAVE_PATHS) {
        ptr           = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);
        dev_num_paths = *(uint8_t*)ptr;
    } else {
        dev_num_paths = 1;
    }

    ptr      = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);
    dev_addr = ptr;
    ptr      = UCS_PTR_BYTE_OFFSET(ptr, dev_addr_len);

    last_tl = empty_dev;
    while (!last_tl) {
        address->dev_addr      = (dev_addr_len > 0) ? dev_addr : NULL;
        address->md_index      = md_index;
        address->dev_index     = *dev_index;
        address->md_flags      = md_flags;
        address->dev_num_paths = dev_num_paths;
        address->iface_attr    = (scp_address_iface_attr_h)ptr;
        ptr                    = UCS_PTR_BYTE_OFFSET(ptr, sizeof(scp_address_iface_attr_t));

        ptr                    = scp_address_unpack_length(ptr, &iface_addr_len, &last_tl);
        address->iface_addr    = (iface_addr_len > 0) ? ptr : NULL;
        ptr                    = UCS_PTR_BYTE_OFFSET(ptr, iface_addr_len);

        ++address;
    }

    ++(*dev_index);
    *buffer = ptr;
    *address_entry = address;
    return UCG_OK;
}

ucg_status_t scp_address_unpack(void *buffer, scp_unpacked_address_h *address)
{
    scp_unpacked_address_h unpacked_address;
    scp_address_entry_h address_entry;
    scp_rsc_index_t dev_index;
    ucg_status_t status;
    int last_dev;
    void *ptr;

    unpacked_address = (scp_unpacked_address_h)buffer;
    if (unpacked_address->flag) {
        ucg_debug("Already unpack for address.");
    }

    ptr = buffer + sizeof(scp_unpacked_address_t)
        + sizeof(scp_address_entry_t) * unpacked_address->address_count;

    /* Empty address list */
    if (*(uint8_t*)ptr == SCP_NULL_RESOURCE) {
        ucg_assert(unpacked_address->address_count == 0);
        return UCG_OK;
    }

    address_entry = &unpacked_address->address_list[0];

    /* Unpack addresses */
    dev_index = 0;
    do {
        status = scp_address_unpack_device(&address_entry, &dev_index,
                                           &last_dev, &ptr);
        if (status != UCG_OK) {
            ucg_error("Failed to unpack device for index %d", dev_index);
        }
    } while (!last_dev);

    unpacked_address->flag = 1;
    *address = unpacked_address;

    return UCG_OK;
}

ucg_status_t scp_ep_address_pack(scp_ep_h ep, scp_address_t *address)
{
    ucg_assert(ep != NULL);
    scp_worker_h worker = ep->worker;
    scp_worker_iface_t *wiface;
    scp_rsc_index_t rsc_idx;
    size_t ep_addr_len;
    ucs_status_t ucs_status;

    scp_ep_unpacked_h unpacked = (scp_ep_unpacked_h)address;
    void *ptr = (void *)address + sizeof(scp_ep_unpacked_t) +
        sizeof(scp_address_entry_ep_addr_t) * ep->sct_ep_num;

    unpacked->num_ep_addrs = ep->sct_ep_num;
    unpacked->sdma_ep_idx  = ep->sdma_ep_idx;
    unpacked->guid         = ucs_machine_guid();

    for (uint8_t ep_idx = 0; ep_idx < ep->sct_ep_num; ++ep_idx) {
        rsc_idx = ep->sct_eps_rsc_idx[ep_idx];
        wiface = worker->ifaces[rsc_idx];
        unpacked->ep_addrs[ep_idx].rsc_index = rsc_idx;

        if (!(wiface->attr.cap.flags & UCT_IFACE_FLAG_CONNECT_TO_EP)) {
            /* connect to iface, don't need exchange ep address */
            continue;
        }

        ep_addr_len = wiface->attr.ep_addr_len;

        /* pack ep address length and save pointer to flags */
        ptr = scp_address_pack_length(worker, ptr, ep_addr_len);

        /* pack ep address */
        ucs_status = sct_ep_get_address(ep->sct_eps[ep_idx], ptr);
        if (ucs_status != UCS_OK) {
            return ucg_status_s2g(ucs_status);
        }
        ptr = UCS_PTR_BYTE_OFFSET(ptr, ep_addr_len);
    }

    return UCG_OK;
}

ucg_status_t scp_ep_address_unpack(void *buffer, scp_ep_unpacked_h *unpacked_address)
{
    scp_ep_unpacked_h address = (scp_ep_unpacked_h)buffer;
    if (address->num_ep_addrs == 0 ||
        address->num_ep_addrs > SCP_MAX_LANE) {
        ucg_error("failed to parse address: number of ep addresses"
                  " exceeds %d", SCP_MAX_LANE);
        return UCG_ERR_INVALID_ADDR;
    }

    void *ptr = (void *)buffer + sizeof(scp_ep_unpacked_t)
        + address->num_ep_addrs * sizeof(scp_address_entry_ep_addr_t);

    size_t ep_addr_len;
    scp_address_entry_ep_addr_h ep_addr;
    for (uint8_t ep_idx = 0; ep_idx < address->num_ep_addrs; ++ep_idx) {
        ep_addr       = &address->ep_addrs[ep_idx];
        /* sdma means connect to iface, don't need exchange ep address */
        if (ep_idx == address->sdma_ep_idx) {
            ep_addr->addr = NULL;
            continue;
        }
        ptr           = scp_address_unpack_length(ptr, &ep_addr_len, NULL);
        ep_addr->addr = ptr;
        ptr           = UCS_PTR_BYTE_OFFSET(ptr, ep_addr_len);
    }

    *unpacked_address = address;

    return UCG_OK;
}
