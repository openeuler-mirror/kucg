/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_COMPONENT_H_
#define SCT_COMPONENT_H_

#include "sct.h"

/* Forward declaration */
typedef struct sct_component sct_component_t;


/**
 * Keeps information about allocated configuration structure, to be used when
 * releasing the options.
 */
typedef struct sct_config_bundle {
    ucs_config_field_t *table;
    const char         *table_prefix;
    char               data[];
} sct_config_bundle_t;


/**
 * Component method to query component memory domain resources.
 *
 * @param [in]  component               Query memory domain resources for this
 *                                      component.
 * @param [out] resources_p             Filled with a pointer to an array of
 *                                      memory domain resources, which should be
 *                                      released with ucg_free().
 * @param [out] num_resources_p         Filled with the number of memory domain
 *                                      resource entries in the array.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_component_query_md_resources_func_t)(
    sct_component_t *component, uct_md_resource_desc_t **resources_p,
    unsigned *num_resources_p);


/**
 * Component method to open a memory domain.
 *
 * @param [in]  component               Open memory domain resources on this
 *                                      component.
 * @param [in]  md_name                 Name of the memory domain to open, as
 *                                      returned by
 *                                      @ref uct_component_query_resources_func_t
 * @param [in]  config                  Memory domain configuration.
 * @param [out] md_p                    Handle to the opened memory domain.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_component_md_open_func_t)(
    sct_component_t *component, const char *md_name,
    const sct_md_config_t *config, uct_md_h context, sct_md_h *md_p);


/**
 * Component method to unpack a remote key buffer into a remote key object.
 *
 * @param [in]  component               Unpack the remote key buffer on this
 *                                      component.
 * @param [in]  rkey_buffer             Remote key buffer to unpack.
 * @param [in]  config                  Memory domain configuration.
 * @param [out] rkey_p                  Filled with a pointer to the unpacked
 *                                      remote key.
 * @param [out] handle_p                Filled with an additional handle which
 *                                      is used to release the remote key, but
 *                                      is not required for remote memory
 *                                      access operations.
 *
 * @return UCS_OK on success or error code in case of failure.
 */
typedef ucs_status_t (*sct_component_rkey_unpack_func_t)(
    sct_component_t *component, const void *rkey_buffer,
    uct_rkey_t *rkey_p, void **handle_p);


/**
 * Defines a UCT component
 */
struct sct_component {
    const char                              name[UCT_COMPONENT_NAME_MAX]; /**< Component name */
    sct_component_query_md_resources_func_t query_md_resources; /**< Query memory domain resources method */
    sct_component_md_open_func_t            md_open;            /**< Memory domain open method */
    sct_component_rkey_unpack_func_t        rkey_unpack;        /**< Remote key unpack method */
    ucs_config_global_list_entry_t          md_config;          /**< MD configuration entry */
    ucs_list_link_t                         tl_list;            /**< List of transports */
    ucs_list_link_t                         list;               /**< Entry in global list of components */
    uint64_t                                flags;              /**< Flags as defined by
                                                                     UCT_COMPONENT_FLAG_xx */
};


/**
 * Register a component for usage, so it will be returned from
 * @ref sct_query_components.
 *
 * @param [in] _component  Pointer to a global component structure to register.
 */
#define SCT_COMPONENT_REGISTER(_component) \
    extern ucs_list_link_t sct_components_list; \
    UCS_STATIC_INIT { \
        ucs_list_add_tail(&sct_components_list, &(_component)->list); \
    } \
    UCS_CONFIG_REGISTER_TABLE_ENTRY(&(_component)->md_config, &ucs_config_global_list) \


/**
 * Helper macro to initialize component's transport list head.
 */
#define SCT_COMPONENT_TL_LIST_INITIALIZER(_component) \
    UCS_LIST_INITIALIZER(&(_component)->tl_list, &(_component)->tl_list)


ucs_status_t sct_config_read(sct_config_bundle_t **bundle,
                             ucs_config_field_t *config_table,
                             size_t config_size, const char *env_prefix,
                             const char *cfg_prefix);

#endif
