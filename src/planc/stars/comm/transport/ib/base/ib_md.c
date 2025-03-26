/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "ib_md.h"


#define SCT_IB_MD_RCACHE_DEFAULT_ALIGN 16

typedef struct sct_ib_md_pci_info {
    double      bw;       /* bandwidth */
    uint16_t    payload;  /* payload used to data transfer */
    uint16_t    overhead; /* PHY + data link layer + header + *CRC* */
    uint16_t    nack;     /* number of TLC before ACK */
    uint16_t    ctrl;     /* length of control TLP */
    uint16_t    encoding; /* number of bits in symbol encoded, 8 - gen 1/2, 128 - gen 3 */
    uint16_t    decoding; /* number of bits in symbol decoded, 10 - gen 1/2, 130 - gen 3 */
    const char *name;     /* name of PCI generation */
} sct_ib_md_pci_info_t;

static UCS_CONFIG_DEFINE_ARRAY(pci_bw,
                               sizeof(ucs_config_bw_spec_t),
                               UCS_CONFIG_TYPE_BW_SPEC);

static ucs_config_field_t sct_ib_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(sct_ib_md_config_t, super), UCS_CONFIG_TYPE_TABLE(sct_md_config_table)},

    {"REG_METHODS", "rcache,odp,direct",
     "List of registration methods in order of preference. Supported methods are:\n"
     "  odp         - implicit on-demand paging\n"
     "  rcache      - userspace registration cache\n"
     "  direct      - direct registration\n",
     ucs_offsetof(sct_ib_md_config_t, reg_methods), UCS_CONFIG_TYPE_STRING_ARRAY},

    {"", "RCACHE_ADDR_ALIGN=" UCS_PP_MAKE_STRING(SCT_IB_MD_RCACHE_DEFAULT_ALIGN), NULL,
     ucs_offsetof(sct_ib_md_config_t, rcache),
     UCS_CONFIG_TYPE_TABLE(sct_md_config_rcache_table)},

    {"MEM_REG_OVERHEAD", "16us", "Memory registration overhead",
     ucs_offsetof(sct_ib_md_config_t, uc_reg_cost.c), UCS_CONFIG_TYPE_TIME},

    {"MEM_REG_GROWTH", "0.06ns", "Memory registration growth rate",
     ucs_offsetof(sct_ib_md_config_t, uc_reg_cost.m), UCS_CONFIG_TYPE_TIME},

    {"FORK_INIT", "try",
     "Initialize a fork-safe IB library with ibv_fork_init().",
     ucs_offsetof(sct_ib_md_config_t, fork_init), UCS_CONFIG_TYPE_TERNARY},

    {"ASYNC_EVENTS", "y",
     "Enable listening for async events on the device",
     ucs_offsetof(sct_ib_md_config_t, async_events), UCS_CONFIG_TYPE_BOOL},

    {"ETH_PAUSE_ON", "y",
     "Whether or not 'Pause Frame' is enabled on an Ethernet network.\n"
     "Pause frame is a mechanism for temporarily stopping the transmission of data to\n"
     "ensure zero loss under congestion on Ethernet family computer networks.\n"
     "This parameter, if set to 'no', will disqualify IB transports that may not perform\n"
     "well on a lossy fabric when working with RoCE.",
     ucs_offsetof(sct_ib_md_config_t, ext.eth_pause), UCS_CONFIG_TYPE_BOOL},

    {"ODP_NUMA_POLICY", "preferred",
     "Override NUMA policy for ODP regions, to avoid extra page migrations.\n"
     " - default: Do no change existing policy.\n"
     " - preferred/bind:\n"
     "     Unless the memory policy of the current thread is MPOL_BIND, set the\n"
     "     policy of ODP regions to MPOL_PREFERRED/MPOL_BIND, respectively.\n"
     "     If the numa node mask of the current thread is not defined, use the numa\n"
     "     nodes which correspond to its cpu affinity mask.",
     ucs_offsetof(sct_ib_md_config_t, ext.odp.numa_policy),
     UCS_CONFIG_TYPE_ENUM(ucs_numa_policy_names)},

    {"ODP_MAX_SIZE", "auto",
     "Maximal memory region size to enable ODP for. 0 - disable.\n",
     ucs_offsetof(sct_ib_md_config_t, ext.odp.max_size), UCS_CONFIG_TYPE_MEMUNITS},

    {"PREFER_NEAREST_DEVICE", "y",
     "Prefer nearest device to cpu when selecting a device from NET_DEVICES list.\n",
     ucs_offsetof(sct_ib_md_config_t, ext.prefer_nearest_device), UCS_CONFIG_TYPE_BOOL},

    {"GID_INDEX", "auto",
     "Port GID index to use.",
     ucs_offsetof(sct_ib_md_config_t, ext.gid_index), UCS_CONFIG_TYPE_ULUNITS},

    {"SUBNET_PREFIX", "",
     "Infiniband subnet prefix to filter ports by, empty means no filter. "
     "Relevant for IB link layer only\n"
     "For example a filter for the default subnet prefix can be specified as: fe80:0:0:0",
     ucs_offsetof(sct_ib_md_config_t, subnet_prefix), UCS_CONFIG_TYPE_STRING},

    {"PCI_BW", "",
     "Maximum effective data transfer rate of PCI bus connected to HCA\n",
     ucs_offsetof(sct_ib_md_config_t, pci_bw), UCS_CONFIG_TYPE_ARRAY(pci_bw)},

    {NULL}
};

static const sct_ib_md_pci_info_t sct_ib_md_pci_info[] = {
    { /* GEN 1 */
        .bw       = 2.5 * UCS_GBYTE / 8,
        .payload  = 512,
        .overhead = 28,
        .nack     = 5,
        .ctrl     = 256,
        .encoding = 8,
        .decoding = 10,
        .name     = "gen1"
    },
    { /* GEN 2 */
        .bw       = 5.0 * UCS_GBYTE / 8,
        .payload  = 512,
        .overhead = 28,
        .nack     = 5,
        .ctrl     = 256,
        .encoding = 8,
        .decoding = 10,
        .name     = "gen2"
    },
    { /* GEN 3 */
        .bw       = 8.0 * UCS_GBYTE / 8,
        .payload  = 512,
        .overhead = 30,
        .nack     = 5,
        .ctrl     = 256,
        .encoding = 128,
        .decoding = 130,
        .name     = "gen3"
    },
};

UCS_LIST_HEAD(sct_ib_md_ops_list);

static ucs_status_t sct_ib_md_query(sct_md_h sct_md, uct_md_attr_t *md_attr)
{
    ucg_debug("sct_ib_md_query");
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);

    md_attr->cap.max_alloc = ULONG_MAX;
    md_attr->cap.max_reg   = ULONG_MAX;
    md_attr->cap.flags     = UCT_MD_FLAG_REG       |
                             UCT_MD_FLAG_NEED_MEMH |
                             UCT_MD_FLAG_NEED_RKEY |
                             UCT_MD_FLAG_ADVISE;
    md_attr->cap.reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->cap.alloc_mem_types  = 0;
    md_attr->cap.access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->cap.detect_mem_types = 0;

    md_attr->rkey_packed_size = SCT_IB_MD_PACKED_RKEY_SIZE;
    md_attr->reg_cost         = md->reg_cost;
    ucs_sys_cpuset_copy(&md_attr->local_cpus, &md->dev.local_cpus);

    return UCS_OK;
}

static void sct_ib_md_print_mem_reg_err_msg(void *address, size_t length,
                                            uint64_t access_flags, int err,
                                            int silent)
{
    ucs_log_level_t level = silent ? UCS_LOG_LEVEL_DEBUG : UCS_LOG_LEVEL_ERROR;
    /* 256 is the length of the string buffer */
    UCS_STRING_BUFFER_ONSTACK(msg, 256);
    struct rlimit limit_info;
    size_t page_size;
    size_t unused;

    ucs_string_buffer_appendf(&msg,
                              "%s(address=%p, length=%zu, access=0x%lx) failed: %m",
                              ibv_reg_mr_func_name, address, length, access_flags);
    if (err == ENOMEM) {
        /* Check the value of the max locked memory which is set on the system
        * (ulimit -l) */
        if (!getrlimit(RLIMIT_MEMLOCK, &limit_info) &&
            (limit_info.rlim_cur != RLIM_INFINITY)) {
            ucs_string_buffer_appendf(&msg,
                                      ". Please set max locked memory "
                                      "(ulimit -l) to 'unlimited' "
                                      "(current: %llu kbytes)",
                                      limit_info.rlim_cur / UCS_KBYTE);
        }
    } else if (err == EINVAL) {
        /* Check if huge page is used */
        ucs_get_mem_page_size(address, length, &unused, &page_size);
        if (page_size != ucs_get_page_size()) {
            ucs_string_buffer_appendf(&msg,
                                      ". Application is using HUGE pages. "
                                      "Please set environment variable "
                                      "RDMAV_HUGEPAGES_SAFE=1");
        }
    }

    ucs_log(level, "%s", ucs_string_buffer_cstr(&msg));
}

static ucs_status_t sct_ib_md_reg_mr(sct_ib_md_t *md, void *address,
                                     size_t length, uint64_t access_flags,
                                     int silent, sct_ib_mem_t *memh)
{
    return md->ops->reg_key(md, address, length, access_flags, memh, silent);
}

ucs_status_t sct_ib_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
                           uint64_t access_flags, struct ibv_mr **mr_p,
                           int silent)
{
    struct ibv_mr *mr;
    mr = ibv_reg_mr(pd, addr, length, access_flags);
    if (mr == NULL) {
        sct_ib_md_print_mem_reg_err_msg(addr, length, access_flags,
                                        errno, silent);
        return UCS_ERR_IO_ERROR;
    }

    *mr_p = mr;
    return UCS_OK;
}

ucs_status_t sct_ib_dereg_mr(struct ibv_mr *mr)
{
    int ret;

    if (mr == NULL) {
        return UCS_OK;
    }

    ret = ibv_dereg_mr(mr);
    if (ret != 0) {
        /* bugfix later */
        ucg_debug("ibv_dereg_mr() failed: %m");
        return UCS_ERR_IO_ERROR;
    }

    return UCS_OK;
}

static ucs_status_t sct_ib_memh_dereg_key(sct_ib_md_t *md, sct_ib_mem_t *memh)
{
        return md->ops->dereg_key(md, memh);
}

static ucs_status_t sct_ib_memh_dereg(sct_ib_md_t *md, sct_ib_mem_t *memh)
{
    return sct_ib_memh_dereg_key(md, memh);
}

static inline void sct_ib_memh_free(sct_ib_mem_t *memh)
{
    ucg_free(memh);
}

static sct_ib_mem_t *sct_ib_memh_alloc(sct_ib_md_t *md)
{
    return ucg_calloc(1, md->memh_struct_size, "ib_memh");
}

static uint64_t sct_ib_md_access_flags(sct_ib_md_t *md, unsigned flags,
                                       size_t length)
{
    uint64_t access_flags = SCT_IB_MEM_ACCESS_FLAGS;

    if ((flags & UCT_MD_MEM_FLAG_NONBLOCK) && (length > 0) &&
        (length <= md->config.odp.max_size)) {
        access_flags |= IBV_ACCESS_ON_DEMAND;
    }

    return access_flags;
}

#if HAVE_NUMA
static ucs_status_t sct_ib_mem_set_numa_policy(sct_ib_md_t *md, void *address,
                                               size_t length, sct_ib_mem_t *memh)
{
    int ret, old_policy, new_policy;
    struct bitmask *nodemask;
    uintptr_t start, end;
    ucs_status_t status;

    ucg_debug("The numa policy is %d", md->config.odp.numa_policy);
    if (!(memh->flags & UCT_IB_MEM_FLAG_ODP) ||
        (md->config.odp.numa_policy == UCS_NUMA_POLICY_DEFAULT) ||
        (numa_available() < 0)) {
        status = UCS_OK;
        goto out;
    }

    nodemask = numa_allocate_nodemask();
    if (nodemask == NULL) {
        ucg_warn("Failed to allocate numa node mask");
        status = UCS_ERR_NO_MEMORY;
        goto out;
    }

    ret = get_mempolicy(&old_policy, numa_nodemask_p(nodemask),
                        numa_nodemask_size(nodemask), NULL, 0);
    if (ret < 0) {
        ucg_warn("get_mempolicy(maxnode=%zu) failed: %m",
                 numa_nodemask_size(nodemask));
        status = UCS_ERR_INVALID_PARAM;
        goto out_free;
    }

    switch (old_policy) {
        case MPOL_DEFAULT:
            /* if no policy is defined, use the numa node of the current cpu */
            numa_get_thread_node_mask(&nodemask);
            break;
        case MPOL_BIND:
            /* if the current policy is BIND, keep it as-is */
            status = UCS_OK;
            goto out_free;
        default:
            break;
    }

    switch (md->config.odp.numa_policy) {
        case UCS_NUMA_POLICY_BIND:
            new_policy = MPOL_BIND;
            break;
        case UCS_NUMA_POLICY_PREFERRED:
            new_policy = MPOL_PREFERRED;
            break;
        default:
            ucg_error("unexpected numa policy %d", md->config.odp.numa_policy);
            status = UCS_ERR_INVALID_PARAM;
            goto out_free;
    }

    if (new_policy != old_policy) {
        start = ucs_align_down_pow2((uintptr_t)address, ucs_get_page_size());
        end   = ucs_align_up_pow2((uintptr_t)address + length,
                                  ucs_get_page_size());
        ucs_trace("0x%lx..0x%lx: changing numa policy from %d to %d, "
                  "nodemask[0]=0x%lx", start, end, old_policy, new_policy,
                  numa_nodemask_p(nodemask)[0]);

        ret = mbind((void*)start, end - start, new_policy,
                    numa_nodemask_p(nodemask),
                    numa_nodemask_size(nodemask), 0);
        if (ret < 0) {
            ucg_warn("mbind(addr=0x%lx length=%ld policy=%d) failed: %m",
                     start, end - start, new_policy);
            status = UCS_ERR_IO_ERROR;
            goto out_free;
        }
    }

    status = UCS_OK;

out_free:
    numa_free_nodemask(nodemask);
out:
    return status;
}
#else
static ucs_status_t sct_ib_mem_set_numa_policy(sct_ib_md_t *md, void *address,
                                               size_t length, sct_ib_mem_t *memh)
{
    return UCS_OK;
}
#endif /* UCT_MD_DISABLE_NUMA */

static void sct_ib_mem_init(sct_ib_mem_t *memh, unsigned sct_flags,
                            uint64_t access_flags)
{
    memh->flags = 0;

    if (access_flags & IBV_ACCESS_ON_DEMAND) {
        memh->flags |= UCT_IB_MEM_FLAG_ODP;
    }
}

static ucs_status_t sct_ib_mem_reg_internal(sct_md_h sct_md, void *address,
                                            size_t length, unsigned flags,
                                            int silent, sct_ib_mem_t *memh)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    ucs_status_t status;
    uint64_t access_flags;

    access_flags = sct_ib_md_access_flags(md, flags, length);
    sct_ib_mem_init(memh, flags, access_flags);
    status = sct_ib_md_reg_mr(md, address, length, access_flags, silent, memh);
    if (status != UCS_OK) {
        return status;
    }

    ucg_debug("registered memory %p..%p on %s lkey 0x%x rkey 0x%x "
              "access 0x%lx flags 0x%x", address,
              UCS_PTR_BYTE_OFFSET(address, length),
              sct_ib_device_name(&md->dev), memh->lkey, memh->rkey,
              access_flags, flags);

    sct_ib_mem_set_numa_policy(md, address, length, memh);

    return UCS_OK;
}

static ucs_status_t sct_ib_mem_reg(sct_md_h sct_md, void *address, size_t length,
                                   unsigned flags, sct_mem_h *memh_p)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    ucs_status_t status;
    sct_ib_mem_t *memh;

    memh = sct_ib_memh_alloc(md);
    if (memh == NULL) {
        sct_md_log_mem_reg_error(flags,
                                 "md %p: failed to allocate memory handle", md);
        return UCS_ERR_NO_MEMORY;
    }

    status = sct_ib_mem_reg_internal(sct_md, address, length, flags, 0, memh);
    if (status != UCS_OK) {
        sct_ib_memh_free(memh);
        return status;
    }
    *memh_p = memh;

    return UCS_OK;
}

static ucs_status_t sct_ib_mem_dereg(sct_md_h sct_md, sct_mem_h memh)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    sct_ib_mem_t *ib_memh = memh;
    ucs_status_t status;

    status = sct_ib_memh_dereg(md, ib_memh);
    sct_ib_memh_free(ib_memh);
    return status;
}

ucs_status_t sct_ib_verbs_reg_key(sct_ib_md_t *md, void *address,
                                  size_t length, uint64_t access_flags,
                                  sct_ib_mem_t *ib_memh, int silent)
{
    sct_ib_verbs_mem_t *memh = ucs_derived_of(ib_memh, sct_ib_verbs_mem_t);

    return sct_ib_reg_key_impl(md, address, length, access_flags,
                               ib_memh, &memh->mrs, silent);
}

ucs_status_t sct_ib_reg_key_impl(sct_ib_md_t *md, void *address,
                                 size_t length, uint64_t access_flags,
                                 sct_ib_mem_t *memh, sct_ib_mr_t *mr,
                                 int silent)
{
    ucs_status_t status;

    status = sct_ib_reg_mr(md->pd, address, length, access_flags, &mr->ib,
                           silent);
    if (status != UCS_OK) {
        return status;
    }

    sct_ib_memh_init_keys(memh, mr->ib->lkey, mr->ib->rkey);

    return UCS_OK;
}

ucs_status_t sct_ib_verbs_dereg_key(sct_ib_md_t *md,
                                    sct_ib_mem_t *ib_memh)
{
    sct_ib_verbs_mem_t *memh = ucs_derived_of(ib_memh, sct_ib_verbs_mem_t);

    return sct_ib_dereg_mr(memh->mrs.ib);
}

static ucs_status_t sct_ib_mkey_pack(sct_md_h sct_md, sct_mem_h uct_memh,
                                     void *rkey_buffer)
{
    sct_ib_mem_t *memh      = uct_memh;
    sct_ib_md_pack_rkey(memh->rkey, rkey_buffer);
    return UCS_OK;
}

static ucs_status_t sct_ib_rkey_unpack(sct_component_t *component,
                                       const void *rkey_buffer, uct_rkey_t *rkey_p,
                                       void **handle_p)
{
    uint64_t packed_rkey = *(const uint64_t*)rkey_buffer;

    *rkey_p   = packed_rkey;
    *handle_p = NULL;
    ucs_trace("unpacked rkey 0x%llx: direct 0x%x",
              (unsigned long long)packed_rkey,
              sct_ib_md_direct_rkey(*rkey_p));
    return UCS_OK;
}

static sct_md_ops_t sct_ib_md_ops = {
    .close              = sct_ib_md_close,
    .query              = sct_ib_md_query,
    .mem_reg            = sct_ib_mem_reg,
    .mem_dereg          = sct_ib_mem_dereg,
    .mkey_pack          = sct_ib_mkey_pack,
};

static inline sct_ib_rcache_region_t* sct_ib_rcache_region_from_memh(sct_mem_h memh)
{
    return ucs_container_of(memh, sct_ib_rcache_region_t, memh);
}

static ucs_status_t sct_ib_mem_rcache_reg(sct_md_h sct_md, void *address,
                                          size_t length, unsigned flags,
                                          sct_mem_h *memh_p)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    ucs_rcache_region_t *rregion;
    ucs_status_t status;
    sct_ib_mem_t *memh;

    status = ucs_rcache_get(md->rcache, address, length, PROT_READ|PROT_WRITE,
                            &flags, &rregion);
    if (status != UCS_OK) {
        return status;
    }

    ucg_assert(rregion->refcount > 0);
    memh = &ucs_derived_of(rregion, sct_ib_rcache_region_t)->memh;

    *memh_p = memh;
    return UCS_OK;
}

static ucs_status_t sct_ib_mem_rcache_dereg(sct_md_h sct_md, sct_mem_h memh)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    sct_ib_rcache_region_t *region = sct_ib_rcache_region_from_memh(memh);

    ucs_rcache_region_put(md->rcache, &region->super);
    return UCS_OK;
}

static sct_md_ops_t sct_ib_md_rcache_ops = {
    .close              = sct_ib_md_close,
    .query              = sct_ib_md_query,
    .mem_reg            = sct_ib_mem_rcache_reg,
    .mem_dereg          = sct_ib_mem_rcache_dereg,
    .mkey_pack          = sct_ib_mkey_pack,
};

static ucs_status_t sct_ib_rcache_mem_reg_cb(void *context, ucs_rcache_t *rcache,
                                             void *arg, ucs_rcache_region_t *rregion,
                                             uint16_t rcache_mem_reg_flags)
{
    sct_ib_rcache_region_t *region = ucs_derived_of(rregion, sct_ib_rcache_region_t);
    sct_ib_md_t *md = context;
    int *flags      = arg;
    int silent      = (rcache_mem_reg_flags & UCS_RCACHE_MEM_REG_HIDE_ERRORS) ||
                      (*flags & UCT_MD_MEM_FLAG_HIDE_ERRORS);
    ucs_status_t status;

    status = sct_ib_mem_reg_internal(&md->super, (void*)region->super.super.start,
                                     region->super.super.end - region->super.super.start,
                                     *flags, silent, &region->memh);
    if (status != UCS_OK) {
        return status;
    }

    return UCS_OK;
}

static void sct_ib_rcache_mem_dereg_cb(void *context, ucs_rcache_t *rcache,
                                       ucs_rcache_region_t *rregion)
{
    sct_ib_rcache_region_t *region = ucs_derived_of(rregion, sct_ib_rcache_region_t);
    sct_ib_md_t *md = (sct_ib_md_t *)context;

    (void)sct_ib_memh_dereg(md, &region->memh);
}

static void sct_ib_rcache_dump_region_cb(void *context, ucs_rcache_t *rcache,
                                         ucs_rcache_region_t *rregion, char *buf,
                                         size_t max)
{
    sct_ib_rcache_region_t *region = ucs_derived_of(rregion, sct_ib_rcache_region_t);
    sct_ib_mem_t *memh = &region->memh;

    snprintf(buf, max, "lkey 0x%x rkey 0x%x", memh->lkey, memh->rkey);
}

static ucs_rcache_ops_t sct_ib_rcache_ops = {
    .mem_reg     = sct_ib_rcache_mem_reg_cb,
    .mem_dereg   = sct_ib_rcache_mem_dereg_cb,
    .dump_region = sct_ib_rcache_dump_region_cb
};

static ucs_status_t sct_ib_md_odp_query(sct_md_h sct_md, uct_md_attr_t *md_attr)
{
    ucs_status_t status;

    ucg_debug("sct_ib_md_odp_query");

    status = sct_ib_md_query(sct_md, md_attr);
    if (status != UCS_OK) {
        return status;
    }

    /* ODP supports only host memory */
    md_attr->cap.reg_mem_types &= UCS_BIT(UCS_MEMORY_TYPE_HOST);
    return UCS_OK;
}

static ucs_status_t sct_ib_mem_global_odp_reg(sct_md_h sct_md, void *address,
                                              size_t length, unsigned flags,
                                              sct_mem_h *memh_p)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);
    ucg_assert(md->global_odp != NULL);

    if (flags & UCT_MD_MEM_FLAG_LOCK) {
        return sct_ib_mem_reg(sct_md, address, length, flags, memh_p);
    }

    /* cppcheck-suppress autoVariables */
    *memh_p = md->global_odp;
    return UCS_OK;
}

static ucs_status_t sct_ib_mem_global_odp_dereg(sct_md_h sct_md, sct_mem_h memh)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);

    if (memh == md->global_odp) {
        return UCS_OK;
    }

    return sct_ib_mem_dereg(sct_md, memh);
}

static sct_md_ops_t UCS_V_UNUSED sct_ib_md_global_odp_ops = {
    .close              = sct_ib_md_close,
    .query              = sct_ib_md_odp_query,
    .mem_reg            = sct_ib_mem_global_odp_reg,
    .mem_dereg          = sct_ib_mem_global_odp_dereg,
    .mkey_pack          = sct_ib_mkey_pack,
};

static ucs_status_t sct_ib_query_md_resources(sct_component_t *component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources;
    struct ibv_device **device_list;
    ucs_status_t status;
    int i, num_devices;

    /* Get device list from driver */
    device_list = ibv_get_device_list(&num_devices);
    if (device_list == NULL) {
        ucg_debug("Failed to get IB device list, assuming no devices are present");
        *resources_p     = NULL;
        *num_resources_p = 0;
        return UCS_OK;
    }

    resources = ucg_calloc(num_devices, sizeof(*resources), "ib resources");
    if (resources == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto out_free_device_list;
    }

    for (i = 0; i < num_devices; ++i) {
        ucs_snprintf_zero(resources[i].md_name, sizeof(resources[i].md_name),
                          "%s", ibv_get_device_name(device_list[i]));
    }

    *resources_p     = resources;
    *num_resources_p = num_devices;
    status = UCS_OK;

out_free_device_list:
    ibv_free_device_list(device_list);
    return status;
}

static void sct_ib_fork_warn(void)
{
    ucg_warn("IB: ibv_fork_init() was disabled or failed, yet a fork() has been issued.");
    ucg_warn("IB: data corruption might occur when using registered memory.");
}

static void sct_ib_fork_warn_enable(void)
{
    static volatile uint32_t enabled = 0;
    int ret;

    if (ucs_atomic_cswap32(&enabled, 0, 1) != 0) {
        return;
    }

    ret = pthread_atfork(sct_ib_fork_warn, NULL, NULL);
    if (ret) {
        ucg_warn("registering fork() warning failed: %m");
    }
}

static ucs_status_t UCS_V_UNUSED
sct_ib_md_global_odp_init(sct_ib_md_t *md, sct_mem_h *memh_p)
{
    sct_ib_verbs_mem_t *global_odp;
    sct_ib_mr_t *mr;
    ucs_status_t status;

    global_odp = (sct_ib_verbs_mem_t *)sct_ib_memh_alloc(md);
    if (global_odp == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    mr = &global_odp->mrs;
    status = sct_ib_reg_mr(md->pd, 0, UINT64_MAX,
                           SCT_IB_MEM_ACCESS_FLAGS | IBV_ACCESS_ON_DEMAND,
                           &mr->ib, 1);
    if (status != UCS_OK) {
        ucg_debug("%s: failed to register global mr: %m",
                  sct_ib_device_name(&md->dev));
        goto err;
    }

    global_odp->super.flags = UCT_IB_MEM_FLAG_ODP;
    sct_ib_memh_init_keys(&global_odp->super, mr->ib->lkey, mr->ib->rkey);
    *memh_p = global_odp;
    return UCS_OK;

err:
    sct_ib_memh_free(&global_odp->super);
    return status;
}

static ucs_status_t sct_ib_md_parse_reg_methods(sct_ib_md_t *md, uct_md_attr_t *md_attr,
                                                const sct_ib_md_config_t *md_config)
{
    ucs_rcache_params_t rcache_params;
    ucs_status_t status;
    int i;

    for (i = 0; i < md_config->reg_methods.count; ++i) {
        if (!strcasecmp(md_config->reg_methods.rmtd[i], "rcache")) {
            ucg_debug("Using rcache registration method");
            rcache_params.region_struct_size = sizeof(ucs_rcache_region_t) +
                                               md->memh_struct_size;
            rcache_params.alignment          = md_config->rcache.alignment;
            rcache_params.max_alignment      = ucs_get_page_size();
            rcache_params.ucm_events         = UCM_EVENT_VM_UNMAPPED;
            if (md_attr->cap.reg_mem_types & ~UCS_BIT(UCS_MEMORY_TYPE_HOST)) {
                rcache_params.ucm_events     |= UCM_EVENT_MEM_TYPE_FREE;
            }
            rcache_params.ucm_event_priority = md_config->rcache.event_prio;
            rcache_params.context            = md;
            rcache_params.ops                = &sct_ib_rcache_ops;
            rcache_params.flags              = UCS_RCACHE_FLAG_PURGE_ON_FORK;

            status = ucs_rcache_create(&rcache_params, "hns_0_acc",
                                       NULL, &md->rcache);
            if (status != UCS_OK) {
                ucg_debug("%s: failed to create registration cache: %s",
                          "hns_0_acc",
                          ucs_status_string(status));
                continue;
            }

            md->super.ops = &sct_ib_md_rcache_ops;
            md->reg_cost  = ucs_linear_func_make(md_config->rcache.overhead, 0);
            ucg_debug("%s: using registration cache, event_prio is %u, overhead is %f ns, alignment %zu",
                      "hns_0_acc", md_config->rcache.event_prio,
                      md_config->rcache.overhead * 1e9, md_config->rcache.alignment);

            return UCS_OK;
        } else if (!strcasecmp(md_config->reg_methods.rmtd[i], "odp")) {
            ucg_debug("Using on-demand-paging registration method");
            if (!(md->dev.flags & UCT_IB_DEVICE_FLAG_ODP_IMPLICIT)) {
                ucg_debug("%s: on-demand-paging with global memory region is "
                          "not supported", sct_ib_device_name(&md->dev));
                continue;
            }

            status = sct_ib_md_global_odp_init(md, &md->global_odp);
            if (status != UCS_OK) {
                continue;
            }

            md->super.ops = &sct_ib_md_global_odp_ops;
            md->reg_cost  = ucs_linear_func_make(10e-9, 0);
            ucg_debug("%s: using odp global key", sct_ib_device_name(&md->dev));
            return UCS_OK;
        } else if (!strcmp(md_config->reg_methods.rmtd[i], "direct")) {
            ucg_debug("Using direct registration method");
            md->super.ops = &sct_ib_md_ops;
            md->reg_cost  = md_config->uc_reg_cost;
            ucg_debug("%s: using direct registration, overhead is %f us, growth is %f ns",
                      sct_ib_device_name(&md->dev), md_config->uc_reg_cost.c * 1e6, md_config->uc_reg_cost.m * 1e9);
            return UCS_OK;
        }
    }

    return UCS_ERR_INVALID_PARAM;
}

static void sct_ib_md_release_reg_method(sct_ib_md_t *md)
{
    if (md->rcache != NULL) {
        ucs_rcache_destroy(md->rcache);
    }
    if (md->global_odp != NULL) {
        sct_ib_mem_dereg(&md->super, md->global_odp);
    }
}

static ucs_status_t sct_ib_md_parse_subnet_prefix(const char *subnet_prefix_str,
                                                  uint64_t *subnet_prefix)
{
    uint16_t pfx[4] = {0};
    uint64_t pfx64 = 0;
    int res, i;

    res = sscanf(subnet_prefix_str, "%hx:%hx:%hx:%hx",
                 &pfx[0], &pfx[1], &pfx[2], &pfx[3]);
    if (res != 4) {
        ucg_error("subnet filter '%s' is invalid", subnet_prefix_str);
        return UCS_ERR_INVALID_PARAM;
    }

    for (i = 0; i < 4; i++) {
        pfx64 = pfx[i] + (pfx64 << 16);
    }

    *subnet_prefix = htobe64(pfx64);
    return UCS_OK;
}

static double sct_ib_md_read_pci_bw(struct ibv_device *ib_device)
{
    const char *pci_width_file_name = "current_link_width";
    const char *pci_speed_file_name = "current_link_speed";
    char pci_width_str[16];
    char pci_speed_str[16];
    char gts[16];
    const sct_ib_md_pci_info_t *p;
    double bw, effective_bw;
    unsigned width;
    ssize_t len;
    size_t i;

    len = ucs_read_file(pci_width_str, sizeof(pci_width_str) - 1, 1,
                        SCT_IB_DEVICE_SYSFS_FMT, ib_device->name,
                        pci_width_file_name);
    if (len < 1) {
        ucg_debug("failed to read file: " SCT_IB_DEVICE_SYSFS_FMT,
                  ib_device->name, pci_width_file_name);
        return DBL_MAX; /* failed to read file */
    }
    pci_width_str[len] = '\0';

    len = ucs_read_file(pci_speed_str, sizeof(pci_speed_str) - 1, 1,
                        SCT_IB_DEVICE_SYSFS_FMT, ib_device->name,
                        pci_speed_file_name);
    if (len < 1) {
        ucg_debug("failed to read file: " SCT_IB_DEVICE_SYSFS_FMT,
                  ib_device->name, pci_speed_file_name);
        return DBL_MAX; /* failed to read file */
    }
    pci_speed_str[len] = '\0';

    if (sscanf(pci_width_str, "%u", &width) < 1) {
        ucg_debug("incorrect format of %s file: expected: <unsigned integer>, actual: %s\n",
                  pci_width_file_name, pci_width_str);
        return DBL_MAX;
    }

    if ((sscanf(pci_speed_str, "%lf%s", &bw, gts) < 2) ||
        strcasecmp("GT/s", ucs_strtrim(gts))) {
        ucg_debug("incorrect format of %s file: expected: <double> GT/s, actual: %s\n",
                  pci_speed_file_name, pci_speed_str);
        return DBL_MAX;
    }

    bw *= UCS_GBYTE / 8; /* gigabit -> gigabyte */

    for (i = 0; i < ucs_static_array_size(sct_ib_md_pci_info); i++) {
        if (bw < (sct_ib_md_pci_info[i].bw * 1.2)) { /* use 1.2 multiplex to avoid round issues */
            p = &sct_ib_md_pci_info[i]; /* use pointer to make equation shorter */
            effective_bw = bw * width *
                           (p->payload * p->nack) /
                           (((p->payload + p->overhead) * p->nack) + p->ctrl) *
                           p->encoding / p->decoding;
            ucs_trace("%s: pcie %ux %s, effective throughput %.3lfMB/s (%.3lfGb/s)",
                      ib_device->name, width, p->name,
                      (effective_bw / UCS_MBYTE), (effective_bw * 8 / UCS_GBYTE));
            return effective_bw;
        }
    }

    return DBL_MAX;
}

static double sct_ib_md_pci_bw(const sct_ib_md_config_t *md_config,
                               struct ibv_device *ib_device)
{
    unsigned i;

    for (i = 0; i < md_config->pci_bw.count; i++) {
        if (!strcmp(ib_device->name, md_config->pci_bw.device[i].name)) {
            if (UCS_CONFIG_DBL_IS_AUTO(md_config->pci_bw.device[i].bw)) {
                break; /* read data from system */
            }
            return md_config->pci_bw.device[i].bw;
        }
    }

    return sct_ib_md_read_pci_bw(ib_device);
}

/*
 * Must to check whether the fork init done by ucx ib
 */
static int sct_ib_check_is_uct_fork_init(uct_md_h uct_md)
{
    int fork_int = 0;

    if (uct_md != NULL) {
        fork_int = GET_VAL_BY_ADDRESS(int, uct_md, SCT_UCT_MD_OFFSET_FORK_INIT);
        ucg_assert(fork_int == 1);
    }
    return fork_int;
}

static struct ibv_device **
sct_ib_md_get_match_device(struct ibv_device **match_device, const char *md_name)
{
    int num_devices;
    struct ibv_device **ib_device_list;
    struct ibv_device *ib_device;

    /* Get device list from driver */
    ib_device_list = ibv_get_device_list(&num_devices);
    if (ib_device_list == NULL) {
        ucg_debug("Failed to get IB device list, assuming no devices are present");
        return NULL;
    }

    ib_device = NULL;
    for (int i = 0; i < num_devices; ++i) {
        if (!strcmp(ibv_get_device_name(ib_device_list[i]), md_name)) {
            ib_device = ib_device_list[i];
            break;
        }
    }

    if (ib_device == NULL) {
        ucg_debug("IB device %s not found", md_name);
        goto out_free_dev_list;
    }

    *match_device = ib_device;
    return ib_device_list;

out_free_dev_list:
    ibv_free_device_list(ib_device_list);
    return NULL;
}

static ucs_status_t sct_ib_md_fork_init(const sct_ib_md_config_t *md_config,
                                        uct_md_h uct_md, int *fork_init)
{
    int ret;

    *fork_init = sct_ib_check_is_uct_fork_init(uct_md);
    if (*fork_init) {
        if (md_config->fork_init == UCS_NO) {
            ucg_error("Invalid fork_init set no as UCX IB already fork init");
            return UCS_ERR_INVALID_PARAM;
        }
        return UCS_OK;
    }

    if (md_config->fork_init == UCS_NO) {
        sct_ib_fork_warn_enable();
        return UCS_OK;
    }

    ret = ibv_fork_init();
    if (!ret) {
        *fork_init = 1;
        return UCS_OK;
    }

    if (md_config->fork_init == UCS_YES) {
        ucg_error("ibv_fork_init() failed: %m");
        return UCS_ERR_IO_ERROR;
    }

    ucg_debug("ibv_fork_init() failed: %m, continuing, but fork may be unsafe.");
    sct_ib_fork_warn_enable();

    return UCS_OK;
}

static ucs_status_t sct_ib_md_open(sct_component_t *component,
                                   const char *md_name,
                                   const sct_md_config_t *sct_md_config,
                                   uct_md_h uct_md, sct_md_h *md_p)
{
    const sct_ib_md_config_t *md_config = ucs_derived_of(sct_md_config, sct_ib_md_config_t);
    ucs_status_t status = UCS_ERR_UNSUPPORTED;
    sct_ib_md_t *md = NULL;
    struct ibv_device **ib_device_list, *ib_device;
    sct_ib_md_ops_entry_t *md_ops_entry;
    int fork_init = 0;

    ucs_trace("opening SCT IB device %s", md_name);

    ib_device_list = sct_ib_md_get_match_device(&ib_device, md_name);
    if (ib_device_list == NULL) {
        status = UCS_ERR_NO_DEVICE;
        goto out;
    }

    status = sct_ib_md_fork_init(md_config, uct_md, &fork_init);
    if (status != UCS_OK) {
        goto out_free_dev_list;
    }

    ucs_list_for_each(md_ops_entry, &sct_ib_md_ops_list, list) {
        status = md_ops_entry->ops->open(uct_md, ib_device, md_config, &md);
        if (status == UCS_OK) {
            ucg_debug("%s: md open by '%s' is successful", md_name,
                      md_ops_entry->name);
            md->ops = md_ops_entry->ops;
            break;
        } else if (status != UCS_ERR_UNSUPPORTED) {
            goto out_free_dev_list;
        }
        ucg_debug("%s: md open by '%s' failed, trying next", md_name,
                  md_ops_entry->name);
    }

    if (status != UCS_OK) {
        ucg_assert(status == UCS_ERR_UNSUPPORTED);
        ucg_debug("Unsupported IB device %s", md_name);
        goto out_free_dev_list;
    }

    /* cppcheck-suppress autoVariables */
    *md_p         = &md->super;
    md->fork_init = fork_init;
    status        = UCS_OK;

out_free_dev_list:
    ibv_free_device_list(ib_device_list);
out:
    return status;
}

ucs_status_t sct_ib_md_open_common(sct_ib_md_t *md,
                                   struct ibv_device *ib_device,
                                   const sct_ib_md_config_t *md_config)
{
    uct_md_attr_t md_attr;
    ucs_status_t status;

    md->super.ops       = &sct_ib_md_ops;
    md->super.component = &sct_ib_component;
    ucg_debug("ODP max size is %zu", md->config.odp.max_size);
    if (md->config.odp.max_size == UCS_MEMUNITS_AUTO) {
        md->config.odp.max_size = 0;
    }

    status = sct_ib_device_init(&md->dev, md->use_uct_md, ib_device, md_config->async_events);
    if (status != UCS_OK) {
        goto err;
    }

    if (strlen(md_config->subnet_prefix) > 0) {
        status = sct_ib_md_parse_subnet_prefix(md_config->subnet_prefix,
                                               &md->subnet_filter);
        if (status != UCS_OK) {
            goto err_cleanup_device;
        }

        md->check_subnet_filter = 1;
    }

    if (!md->use_uct_md) {
        /* Allocate memory domain */
        md->pd = ibv_alloc_pd(md->dev.ibv_context);
        if (md->pd == NULL) {
            ucg_error("ibv_alloc_pd() failed: %m");
            status = UCS_ERR_NO_MEMORY;
            goto err_cleanup_device;
        }
    }

    status = sct_md_query(&md->super, &md_attr);
    if (status != UCS_OK) {
        goto err_dealloc_pd;
    }

    status = sct_ib_md_parse_reg_methods(md, &md_attr, md_config);
    if (status != UCS_OK) {
        goto err_dealloc_pd;
    }

    // value 232474549.174560
    md->pci_bw = sct_ib_md_pci_bw(md_config, ib_device);
    return UCS_OK;

err_dealloc_pd:
    if (!md->use_uct_md) {
        ibv_dealloc_pd(md->pd);
    }
err_cleanup_device:
    sct_ib_device_cleanup(&md->dev);
err:
    return status;
}

void sct_ib_md_close(sct_md_h sct_md)
{
    sct_ib_md_t *md = ucs_derived_of(sct_md, sct_ib_md_t);

    md->ops->cleanup(md);
    sct_ib_md_release_reg_method(md);
    if (!md->use_uct_md) {
        ibv_dealloc_pd(md->pd);
    }
    sct_ib_device_cleanup(&md->dev);
    if (!md->use_uct_md) {
        ibv_close_device(md->dev.ibv_context);
    }

    ucg_free(md);
}

sct_component_t sct_ib_component = {
    .query_md_resources = sct_ib_query_md_resources,
    .md_open            = sct_ib_md_open,
    .rkey_unpack        = sct_ib_rkey_unpack,
    .name               = SCT_STARS_RC_ACC,
    .md_config          = {
        .name           = "IB memory domain",
        .prefix         = SCT_IB_CONFIG_PREFIX,
        .table          = sct_ib_md_config_table,
        .size           = sizeof(sct_ib_md_config_t),
    },
    .tl_list            = SCT_COMPONENT_TL_LIST_INITIALIZER(&sct_ib_component),
    .flags              = 0
};
SCT_COMPONENT_REGISTER(&sct_ib_component);
