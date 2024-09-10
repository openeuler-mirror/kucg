/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_IFACE_H_
#define SCT_IFACE_H_

#include "sct_worker.h"

#include "sct_component.h"

/**
 * In release mode - do nothing.
 *
 * In debug mode, if _condition is not true, return an error. This could be less
 * optimal because of additional checks, and that compiler needs to generate code
 * for error flow as well.
 */
#define SCT_CHECK_PARAM(_condition, _err_message, ...) \
    if (ENABLE_PARAMS_CHECK && !(_condition)) { \
        ucg_error(_err_message, ## __VA_ARGS__); \
        return UCS_ERR_INVALID_PARAM; \
    }


/**
 * In release mode - do nothing.
 *
 * In debug mode, if @a _params field mask does not have set
 * @ref UCT_EP_PARAM_FIELD_DEV_ADDR and @ref UCT_EP_PARAM_FIELD_IFACE_ADDR
 * flags, return an error.
 */
#define SCT_EP_PARAMS_CHECK_DEV_IFACE_ADDRS(_params) \
    SCT_CHECK_PARAM(ucs_test_all_flags((_params)->field_mask, \
                                       UCT_EP_PARAM_FIELD_DEV_ADDR | \
                                       UCT_EP_PARAM_FIELD_IFACE_ADDR), \
                    "UCT_EP_PARAM_FIELD_DEV_ADDR and UCT_EP_PARAM_FIELD_IFACE_ADDR are not defined")


#define SCT_EP_PARAMS_GET_PATH_INDEX(_params) \
    (((_params)->field_mask & UCT_EP_PARAM_FIELD_PATH_INDEX) ? \
     (_params)->path_index : 0)


/**
 * Declare classes for structures defined in api/tl.h
 */
UCS_CLASS_DECLARE(sct_iface_h, sct_iface_ops_t, sct_md_h);
UCS_CLASS_DECLARE(sct_ep_t, sct_iface_h);


/**
 * Base structure of all interfaces.
 * Includes the AM table which we don't want to expose.
 */
typedef struct sct_base_iface {
    sct_iface_t             super;
    sct_md_h                md;               /* MD this interface is using */
    sct_priv_worker_t       *worker;          /* Worker this interface is on */

    struct {
        unsigned            num_alloc_methods;
        uct_alloc_method_t  alloc_methods[UCT_ALLOC_METHOD_LAST];
        ucs_log_level_t     failure_level;
        size_t              max_num_eps;
    } config;
} sct_base_iface_t;

UCS_CLASS_DECLARE(sct_base_iface_t, sct_iface_ops_t*,  sct_md_h, sct_worker_h,
                  const sct_iface_params_t*, const sct_iface_config_t*);


/**
 * Base structure of all endpoints.
 */
typedef struct sct_base_ep {
    sct_ep_t          super;
} sct_base_ep_t;
UCS_CLASS_DECLARE(sct_base_ep_t, sct_base_iface_t*);


/**
 * Internal resource descriptor of a transport device
 */
typedef struct uct_tl_device_resource {
    char                     name[UCT_DEVICE_NAME_MAX]; /**< Hardware device name */
    uct_device_type_t        type;       /**< The device represented by this resource
                                              (e.g. UCT_DEVICE_TYPE_NET for a network interface) */
    ucs_sys_device_t         sys_device; /**< The identifier associated with the device
                                              bus_id as captured in ucs_sys_bus_id_t struct */
} sct_tl_device_resource_t;


/**
 * UCT transport definition. This structure should not be used directly; use
 * @ref SCT_TL_DEFINE macro to define a transport.
 */
typedef struct sct_tl {
    char                   name[UCT_TL_NAME_MAX]; /**< Transport name */

    ucs_status_t           (*query_devices)(sct_md_h md,
                                            sct_tl_device_resource_t **tl_devices_p,
                                            unsigned *num_tl_devices_p);

    ucs_status_t           (*iface_open)(sct_md_h md, sct_worker_h worker,
                                         const sct_iface_params_t *params,
                                         const sct_iface_config_t *config,
                                         sct_iface_h *iface_p);

    ucs_config_global_list_entry_t config; /**< Transport configuration entry */
    ucs_list_link_t                list;   /**< Entry in component's transports list */
} sct_tl_t;


/**
 * Define a transport
 *
 * @param _component      Component to add the transport to
 * @param _name           Name of the transport (should be a token, not a string)
 * @param _query_devices  Function to query the list of available devices
 * @param _iface_class    Struct type defining the sct_iface class
 */
#define SCT_TL_DEFINE(_component, _name, _query_devices, _iface_class, \
                      _cfg_prefix, _cfg_table, _cfg_struct) \
    \
    sct_tl_t sct_##_name##_tl = { \
        .name               = #_name, \
        .query_devices      = _query_devices, \
        .iface_open         = UCS_CLASS_NEW_FUNC_NAME(_iface_class), \
        .config = { \
            .name           = #_name" transport", \
            .prefix         = _cfg_prefix, \
            .table          = _cfg_table, \
            .size           = sizeof(_cfg_struct), \
         } \
    }; \
    UCS_CONFIG_REGISTER_TABLE_ENTRY(&(sct_##_name##_tl).config, &ucs_config_global_list); \
    UCS_STATIC_INIT { \
        ucs_list_add_tail(&(_component)->tl_list, &(sct_##_name##_tl).list); \
    }


/**
 * "Base" structure which defines interface configuration options.
 * Specific transport extend this structure.
 */
struct sct_iface_config {
    struct {
        uct_alloc_method_t  *methods;
        unsigned            count;
    } alloc_methods;

    int               failure;   /* Level of failure reports */
    size_t            max_num_eps;
};


/**
 * Memory pool configuration.
 */
typedef struct sct_iface_mpool_config {
    unsigned          max_bufs;  /* Upper limit to number of buffers */
    unsigned          bufs_grow; /* How many buffers (approx.) are allocated every time */
} sct_iface_mpool_config_t;


/**
 * Define configuration fields for memory pool parameters.
 */
#define SCT_IFACE_MPOOL_CONFIG_FIELDS(_prefix, _dfl_max, _dfl_grow, _mp_name, _offset, _desc) \
    {_prefix "MAX_BUFS", UCS_PP_QUOTE(_dfl_max), \
     "Maximal number of " _mp_name " buffers for the interface. -1 is infinite." \
     _desc, \
     (_offset) + ucs_offsetof(sct_iface_mpool_config_t, max_bufs), UCS_CONFIG_TYPE_INT}, \
    \
    {_prefix "BUFS_GROW", UCS_PP_QUOTE(_dfl_grow), \
     "How much buffers are added every time the " _mp_name " memory pool grows.\n" \
     "0 means the value is chosen by the transport.", \
     (_offset) + ucs_offsetof(sct_iface_mpool_config_t, bufs_grow), UCS_CONFIG_TYPE_UINT}


/**
 * TL Memory pool object initialization callback.
 */
typedef void (*sct_iface_mpool_init_obj_cb_t)(sct_iface_h iface, void *obj,
                                              sct_mem_h memh);


extern ucs_config_field_t sct_iface_config_table[];

void sct_base_iface_query(sct_base_iface_t *iface, sct_iface_attr_t *iface_attr);

ucs_status_t sct_single_device_resource(sct_md_h md, const char *dev_name,
                                        uct_device_type_t dev_type,
                                        ucs_sys_device_t sys_device,
                                        sct_tl_device_resource_t **tl_devices_p,
                                        unsigned *num_tl_devices_p);

ucs_status_t sct_iface_free_stars_stream(sct_iface_h tl_iface, void *handle);

#endif
