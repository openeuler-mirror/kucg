/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_IB_MD_H_
#define SCT_IB_MD_H_

#include "sct_md.h"
#include "ib_device.h"


#define SCT_IB_MD_MAX_MR_SIZE                   0x80000000UL
#define SCT_IB_MD_PACKED_RKEY_SIZE              sizeof(uint64_t)

#define SCT_IB_MD_DEFAULT_GID_INDEX             0   /**< The gid index used by default for an IB/RoCE port */

#define SCT_IB_MEM_ACCESS_FLAGS  (IBV_ACCESS_LOCAL_WRITE | \
                                  IBV_ACCESS_REMOTE_WRITE | \
                                  IBV_ACCESS_REMOTE_READ)

#define SCT_IB_MEM_DEREG                        0
#define SCT_IB_CONFIG_PREFIX                    "IB_"

/* offset of some members in struct uct_ib_md
 *   typedef struct uct_ib_md {
 *       uct_md_t                 super;
 *       ucs_rcache_t             *rcache;
 *       struct ibv_pd            *pd;
 *       uct_ib_device_t          dev;
 *       ucs_linear_func_t        reg_cost;
 *       struct uct_ib_md_ops     *ops;
 *       UCS_STATS_NODE_DECLARE(stats)
 *       uct_ib_md_ext_config_t   config;
 *       struct {
 *           uct_ib_device_spec_t *specs;
 *           unsigned             count;
 *       } custom_devices;
 *       int                      ece_enable;
 *       int                      check_subnet_filter;
 *       uint64_t                 subnet_filter;
 *       double                   pci_bw;
 *       int                      relaxed_order;
 *       int                      fork_init;
 *       size_t                   memh_struct_size;
 *       uint64_t                 reg_mem_types;
 *       uint64_t                 reg_nonblock_mem_types;
 *       uint64_t                 cap_flags;
 *       char                     *name;
 *       uint32_t                 flush_rkey;
 *       uct_ib_uint128_t         vhca_id;
 *   } uct_ib_md_t;
 */
#define SCT_UCT_MD_OFFSET_IBV_PD                24
#define SCT_UCT_MD_OFFSET_IBV_DEV               32
#define SCT_UCT_MD_OFFSET_FORK_INIT             944
#define SCT_UCT_IB_DEVICE_OFFSET_IBV_CONTEXT    0


/**
 * IB MD statistics counters
 */
enum {
    UCT_IB_MD_STAT_MEM_ALLOC,
    UCT_IB_MD_STAT_MEM_REG,
    UCT_IB_MD_STAT_LAST
};


enum {
    UCT_IB_MEM_FLAG_ODP              = UCS_BIT(0), /**< The memory region has on
                                                        demand paging enabled */
};

typedef struct sct_ib_md_ext_config {
    int                      eth_pause;    /**< Whether or not Pause Frame is
                                                enabled on the Ethernet network */
    int                      prefer_nearest_device; /**< Give priority for near
                                                         device */

    struct {
        ucs_numa_policy_t    numa_policy;  /**< NUMA policy flags for ODP */
        size_t               max_size;     /**< Maximal memory region size for ODP */
    } odp;

    size_t                   gid_index;    /**< IB GID index to use  */
} sct_ib_md_ext_config_t;


typedef struct sct_ib_mem {
    uint32_t                lkey;
    uint32_t                rkey;
    uint32_t                flags;
} sct_ib_mem_t;


typedef union sct_ib_mr {
    struct ibv_mr           *ib;
} sct_ib_mr_t;


typedef struct sct_ib_verbs_mem {
    sct_ib_mem_t            super;
    sct_ib_mr_t             mrs;
} sct_ib_verbs_mem_t;


/**
 * IB memory domain.
 */
typedef struct sct_ib_md {
    sct_md_t                 super;
    ucs_rcache_t             *rcache;   /**< Registration cache (can be NULL) */
    sct_mem_h                global_odp;/**< Implicit ODP memory handle */
    struct ibv_pd            *pd;       /**< IB memory domain */
    sct_ib_device_t          dev;       /**< IB device */
    ucs_linear_func_t        reg_cost;  /**< Memory registration cost */
    struct sct_ib_md_ops     *ops;
    sct_ib_md_ext_config_t   config;    /* IB external configuration */
    int                      check_subnet_filter;
    uint64_t                 subnet_filter;
    double                   pci_bw;
    int                      fork_init;
    size_t                   memh_struct_size;
    uint8_t                  use_uct_md;
} sct_ib_md_t;


/**
 * IB memory domain configuration.
 */
typedef struct sct_ib_md_config {
    sct_md_config_t          super;

    /** List of registration methods in order of preference */
    UCS_CONFIG_STRING_ARRAY_FIELD(rmtd) reg_methods;

    sct_md_rcache_config_t   rcache;       /**< Registration cache config */
    ucs_linear_func_t        uc_reg_cost;  /**< Memory registration cost estimation
                                                without using the cache */
    unsigned                 fork_init;    /**< Use ibv_fork_init() */
    int                      async_events; /**< Whether async events should be delivered */

    sct_ib_md_ext_config_t   ext;          /**< External configuration */

    char                     *subnet_prefix; /**< Filter of subnet_prefix for IB ports */

    UCS_CONFIG_ARRAY_FIELD(ucs_config_bw_spec_t, device) pci_bw; /**< List of PCI BW for devices */
} sct_ib_md_config_t;

/**
 * Memory domain constructor.
 *
 * @param [in]  ibv_device    IB device.
 *
 * @param [in]  md_config     Memory domain configuration parameters.
 *
 * @param [out] md_p          Handle to memory domain.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_ib_md_open_func_t)(uct_md_h uct_md,
                                              struct ibv_device *ibv_device,
                                              const sct_ib_md_config_t *md_config,
                                              struct sct_ib_md **md_p);

/**
 * Memory domain destructor.
 *
 * @param [in]  md      Memory domain.
 */
typedef void (*sct_ib_md_cleanup_func_t)(struct sct_ib_md *);

/**
 * Memory domain method to register memory area.
 *
 * @param [in]  md      Memory domain.
 *
 * @param [in]  address Memory area start address.
 *
 * @param [in]  length  Memory area length.
 *
 * @param [in]  access  IB verbs registration access flags
 *
 * @param [in]  memh    Memory region handle.
 *                      Method should initialize lkey & rkey.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_ib_md_reg_key_func_t)(struct sct_ib_md *md,
                                                 void *address, size_t length,
                                                 uint64_t access,
                                                 sct_ib_mem_t *memh,
                                                 int silent);

/**
 * Memory domain method to deregister memory area.
 *
 * @param [in]  md      Memory domain.
 *
 * @param [in]  memh    Memory region handle registered with
 *                      sct_ib_md_reg_key_func_t.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_ib_md_dereg_key_func_t)(struct sct_ib_md *md,
                                                   sct_ib_mem_t *memh);


typedef struct sct_ib_md_ops {
    sct_ib_md_open_func_t                open;
    sct_ib_md_cleanup_func_t             cleanup;
    sct_ib_md_reg_key_func_t             reg_key;
    sct_ib_md_dereg_key_func_t           dereg_key;
} sct_ib_md_ops_t;


/**
 * IB memory region in the registration cache.
 */
typedef struct sct_ib_rcache_region {
    ucs_rcache_region_t  super;
    sct_ib_mem_t         memh;      /**<  mr exposed to the user as the memh */
} sct_ib_rcache_region_t;


/**
 * IB memory domain constructor. Should have following logic:
 * - probe provided IB device, may return UCS_ERR_UNSUPPORTED
 * - allocate MD and IB context
 * - setup atomic MR ops
 * - determine device attributes and flags
 */
typedef struct sct_ib_md_ops_entry {
    ucs_list_link_t             list;
    const char                  *name;
    sct_ib_md_ops_t             *ops;
    int                         priority;
} sct_ib_md_ops_entry_t;

#define SCT_IB_MD_OPS(_md_ops, _priority) \
    extern ucs_list_link_t sct_ib_md_ops_list; \
    UCS_STATIC_INIT { \
        static sct_ib_md_ops_entry_t *p, entry = { \
            .name     = UCS_PP_MAKE_STRING(_md_ops), \
            .ops      = &_md_ops, \
            .priority = _priority, \
        }; \
        ucs_list_for_each(p, &sct_ib_md_ops_list, list) { \
            if (p->priority < _priority) { \
                ucs_list_insert_before(&p->list, &entry.list); \
                return; \
            } \
        } \
        ucs_list_add_tail(&sct_ib_md_ops_list, &entry.list); \
    }

extern sct_component_t sct_ib_component;

static inline uint32_t sct_ib_md_direct_rkey(uct_rkey_t uct_rkey)
{
    return (uint32_t)uct_rkey;
}

static UCS_F_ALWAYS_INLINE void
sct_ib_md_pack_rkey(uint32_t rkey, void *rkey_buffer)
{
    uint64_t *rkey_p = (uint64_t*)rkey_buffer;
    *rkey_p = rkey;
     ucs_trace("packed rkey: direct 0x%x", rkey);
}

static inline void sct_ib_memh_init_keys(sct_ib_mem_t *memh, uint32_t lkey, uint32_t rkey)
{
    memh->lkey = lkey;
    memh->rkey = rkey;
}

static UCS_F_ALWAYS_INLINE uint32_t sct_ib_memh_get_lkey(sct_mem_h memh)
{
    ucg_assert(memh != UCT_MEM_HANDLE_NULL);
    return ((sct_ib_mem_t*)memh)->lkey;
}

ucs_status_t sct_ib_md_open_common(sct_ib_md_t *md,
                                   struct ibv_device *ib_device,
                                   const sct_ib_md_config_t *md_config);

void sct_ib_md_close(sct_md_h uct_md);

ucs_status_t sct_ib_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
                           uint64_t access, struct ibv_mr **mr_p, int silent);
ucs_status_t sct_ib_dereg_mr(struct ibv_mr *mr);

ucs_status_t sct_ib_reg_key_impl(sct_ib_md_t *md, void *address,
                                 size_t length, uint64_t access_flags,
                                 sct_ib_mem_t *memh, sct_ib_mr_t *mrs,
                                 int silent);

ucs_status_t sct_ib_verbs_reg_key(sct_ib_md_t *md, void *address,
                                  size_t length, uint64_t access_flags,
                                  sct_ib_mem_t *ib_memh,
                                  int silent);

ucs_status_t sct_ib_verbs_dereg_key(sct_ib_md_t *md, sct_ib_mem_t *ib_memh);

#endif
