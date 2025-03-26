/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_MD_H_
#define SCT_MD_H_

#include "sct_component.h"


extern ucs_config_field_t sct_md_config_table[];

#define sct_md_log_mem_reg_error(_flags, _fmt, ...) \
    ucs_log(sct_md_reg_log_lvl(_flags), _fmt, ## __VA_ARGS__)


typedef struct sct_md_rcache_config {
    size_t               alignment;    /**< Force address alignment */
    unsigned             event_prio;   /**< Memory events priority */
    double               overhead;     /**< Lookup overhead estimation */
} sct_md_rcache_config_t;


extern ucs_config_field_t sct_md_config_rcache_table[];

/**
 * "Base" structure which defines MD configuration options.
 * Specific MDs extend this structure.
 */
struct sct_md_config {
    /* C standard prohibits empty structures */
    char                   __dummy;
};


typedef void (*sct_md_close_func_t)(sct_md_h md);

typedef ucs_status_t (*sct_md_query_func_t)(sct_md_h md,
                                            uct_md_attr_t *md_attr);

typedef ucs_status_t (*sct_md_mem_alloc_func_t)(sct_md_h md,
                                                size_t *length_p,
                                                void **address_p,
                                                ucs_memory_type_t mem_type,
                                                unsigned flags,
                                                const char *alloc_name,
                                                sct_mem_h *memh_p);

typedef ucs_status_t (*sct_md_mem_free_func_t)(sct_md_h md, sct_mem_h memh);

typedef ucs_status_t (*sct_md_mem_reg_func_t)(sct_md_h md, void *address,
                                              size_t length,
                                              unsigned flags,
                                              sct_mem_h *memh_p);

typedef ucs_status_t (*sct_md_mem_dereg_func_t)(sct_md_h md, sct_mem_h memh);

typedef ucs_status_t (*sct_md_mem_query_func_t)(sct_md_h md,
                                                const void *addr,
                                                const size_t length,
                                                uct_md_mem_attr_t *mem_attr_p);

typedef ucs_status_t (*sct_md_mkey_pack_func_t)(sct_md_h md, sct_mem_h memh,
                                                void *rkey_buffer);

/**
 * Memory domain operations
 */
struct sct_md_ops {
    sct_md_close_func_t                  close;
    sct_md_query_func_t                  query;
    sct_md_mem_alloc_func_t              mem_alloc;
    sct_md_mem_free_func_t               mem_free;
    sct_md_mem_reg_func_t                mem_reg;
    sct_md_mem_dereg_func_t              mem_dereg;
    sct_md_mem_query_func_t              mem_query;
    sct_md_mkey_pack_func_t              mkey_pack;
};


/**
 * Memory domain
 */
struct sct_md {
    uint8_t                stars_dev_id;
    uint64_t               subnet_id;
    sct_md_ops_t           *ops;
    sct_component_t        *component;
};

#define SCT_MD_DEFAULT_CONFIG_INITIALIZER \
    { \
        .name        = "Default memory domain", \
        .prefix      =  "", \
        .table       = sct_md_config_table, \
        .size        = sizeof(sct_md_config_t), \
    }

static UCS_F_ALWAYS_INLINE void*
sct_md_fill_md_name(sct_md_h md, void *buffer)
{
    return buffer;
}

/*
 * Base implementation of query_md_resources(), which returns a single md
 * resource whose name is identical to component name.
 */
ucs_status_t sct_md_query_single_md_resource(sct_component_t *component,
                                             uct_md_resource_desc_t **resources_p,
                                             unsigned *num_resources_p);

ucs_status_t sct_md_query_empty_md_resource(uct_md_resource_desc_t **resources_p,
                                            unsigned *num_resources_p);


/**
 * @ingroup UCT_MD
 * @brief Allocate memory for zero-copy sends and remote access.
 *
 * Allocate memory on the memory domain. In order to use this function, MD
 * must support @ref UCT_MD_FLAG_ALLOC flag.
 *
 * @param [in]     md          Memory domain to allocate memory on.
 * @param [in,out] length_p    Points to the size of memory to allocate. Upon successful
 *                             return, filled with the actual size that was allocated,
 *                             which may be larger than the one requested. Must be >0.
 * @param [in,out] address_p   The address
 * @param [in]     mem_type    Memory type of the allocation
 * @param [in]     flags       Memory allocation flags, see @ref uct_md_mem_flags.
 * @param [in]     name        Name of the allocated region, used to track memory
 *                             usage for debugging and profiling.
 * @param [out]    memh_p      Filled with handle for allocated region.
 */
ucs_status_t sct_md_mem_alloc(sct_md_h md, size_t *length_p, void **address_p,
                              ucs_memory_type_t mem_type, unsigned flags,
                              const char *alloc_name, sct_mem_h *memh_p);

/**
 * @ingroup UCT_MD
 * @brief Release memory allocated by @ref sct_md_mem_alloc.
 *
 * @param [in]     md          Memory domain memory was allocated on.
 * @param [in]     memh        Memory handle, as returned from @ref sct_md_mem_alloc.
 */
ucs_status_t sct_md_mem_free(sct_md_h md, sct_mem_h memh);


/**
 * @brief Dummy function
 * Dummy function to emulate unpacking a remote key buffer to handle.
 *
 */
ucs_status_t sct_md_stub_rkey_unpack(sct_component_t *component,
                                     const void *rkey_buffer, uct_rkey_t *rkey_p,
                                     void **handle_p);

/**
 * Check allocation parameters and return an appropriate error if parameters
 * cannot be used for an allocation
 */
ucs_status_t sct_mem_alloc_check_params(size_t length,
                                        const uct_alloc_method_t *methods,
                                        unsigned num_methods,
                                        const sct_mem_alloc_params_t *params);

static inline ucs_log_level_t sct_md_reg_log_lvl(unsigned flags)
{
    return (flags & UCT_MD_MEM_FLAG_HIDE_ERRORS) ? UCS_LOG_LEVEL_DIAG :
            UCS_LOG_LEVEL_ERROR;
}

#endif
