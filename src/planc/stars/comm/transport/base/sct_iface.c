/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "sct_iface.h"


ucs_status_t sct_iface_query(sct_iface_h iface, sct_iface_attr_t *iface_attr)
{
    return iface->ops.iface_query(iface, iface_attr);
}

ucs_status_t sct_iface_get_device_address(sct_iface_h iface, uct_device_addr_t *addr)
{
    return iface->ops.iface_get_device_address(iface, addr);
}

ucs_status_t sct_iface_get_address(sct_iface_h iface, uct_iface_addr_t *addr)
{
    return iface->ops.iface_get_address(iface, addr);
}

int sct_iface_is_reachable(const sct_iface_h iface, const uct_device_addr_t *dev_addr,
                           const uct_iface_addr_t *iface_addr)
{
    return iface->ops.iface_is_reachable(iface, dev_addr, iface_addr);
}

void sct_iface_close(sct_iface_h iface)
{
    iface->ops.iface_close(iface);
}

void sct_base_iface_query(sct_base_iface_t *iface, sct_iface_attr_t *iface_attr)
{
    memset(iface_attr, 0, sizeof(*iface_attr));
    iface_attr->max_num_eps   = iface->config.max_num_eps;
    iface_attr->dev_num_paths = 1;
}

ucs_status_t sct_single_device_resource(sct_md_h md, const char *dev_name,
                                        uct_device_type_t dev_type,
                                        ucs_sys_device_t sys_device,
                                        sct_tl_device_resource_t **tl_devices_p,
                                        unsigned *num_tl_devices_p)
{
    sct_tl_device_resource_t *device;

    device = ucg_calloc(1, sizeof(*device), "device resource");
    if (NULL == device) {
        ucg_error("failed to allocate device resource");
        return UCS_ERR_NO_MEMORY;
    }

    ucs_snprintf_zero(device->name, sizeof(device->name), "%s", dev_name);
    device->type       = dev_type;
    device->sys_device = sys_device;

    *num_tl_devices_p = 1;
    *tl_devices_p     = device;
    return UCS_OK;
}

UCS_CLASS_INIT_FUNC(sct_iface_t, sct_iface_ops_t *ops)
{
    self->ops = *ops;
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(sct_iface_t)
{
}

UCS_CLASS_DEFINE(sct_iface_t, void);


UCS_CLASS_INIT_FUNC(sct_base_iface_t, sct_iface_ops_t *ops, sct_md_h md,
                    sct_worker_h worker, const sct_iface_params_t *params,
                    const sct_iface_config_t *config)
{
    uint64_t alloc_methods_bitmap;
    uct_alloc_method_t method;
    unsigned i;

    UCS_CLASS_CALL_SUPER_INIT(sct_iface_t, ops);

    self->md                = md;
    self->worker            = ucs_derived_of(worker, sct_priv_worker_t);

    /* Copy allocation methods configuration. In the process, remove duplicates. */
    UCS_STATIC_ASSERT(sizeof(alloc_methods_bitmap) * 8 >= UCT_ALLOC_METHOD_LAST);
    self->config.num_alloc_methods = 0;
    alloc_methods_bitmap           = 0;
    for (i = 0; i < config->alloc_methods.count; ++i) {
        method = config->alloc_methods.methods[i];
        if (alloc_methods_bitmap & UCS_BIT(method)) {
            continue;
        }

        ucg_assert(self->config.num_alloc_methods < UCT_ALLOC_METHOD_LAST);
        self->config.alloc_methods[self->config.num_alloc_methods++] = method;
        alloc_methods_bitmap |= UCS_BIT(method);
    }

    self->config.failure_level = (ucs_log_level_t)config->failure;
    self->config.max_num_eps   = config->max_num_eps;

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(sct_base_iface_t)
{
}

UCS_CLASS_DEFINE(sct_base_iface_t, sct_iface_t);


ucs_status_t sct_ep_create(const sct_ep_params_t *params, sct_ep_h *ep_p)
{
    if (ucg_unlikely(!(params->field_mask & UCT_EP_PARAM_FIELD_IFACE))) {
        return UCS_ERR_INVALID_PARAM;
    }

    return params->iface->ops.ep_create(params, ep_p);
}

ucs_status_t sct_ep_disconnect(sct_ep_h ep, unsigned flags)
{
    return ep->iface->ops.ep_disconnect(ep, flags);
}

void sct_ep_destroy(sct_ep_h ep)
{
    return ep->iface->ops.ep_destroy(ep);
}

ucs_status_t sct_ep_get_address(sct_ep_h ep, uct_ep_addr_t *addr)
{
    return ep->iface->ops.ep_get_address(ep, addr);
}

ucs_status_t sct_ep_connect_to_ep(sct_ep_h ep, const uct_device_addr_t *dev_addr,
                                  const uct_ep_addr_t *ep_addr)
{
    return ep->iface->ops.ep_connect_to_ep(ep, dev_addr, ep_addr);
}

UCS_CLASS_INIT_FUNC(sct_ep_t, sct_iface_t *iface)
{
    self->iface = iface;
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(sct_ep_t)
{
}

UCS_CLASS_DEFINE(sct_ep_t, void);

UCS_CLASS_INIT_FUNC(sct_base_ep_t, sct_base_iface_t *iface)
{
    UCS_CLASS_CALL_SUPER_INIT(sct_ep_t, &iface->super);
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(sct_base_ep_t)
{
}

UCS_CLASS_DEFINE(sct_base_ep_t, sct_ep_t);


UCS_CONFIG_DEFINE_ARRAY(alloc_methods, sizeof(uct_alloc_method_t),
                        UCS_CONFIG_TYPE_ENUM(sct_alloc_method_names));

ucs_config_field_t sct_iface_config_table[] = {
    {"ALLOC", "huge,thp,md,mmap,heap",
     "Priority of methods to allocate intermediate buffers for communication",
     ucs_offsetof(sct_iface_config_t, alloc_methods), UCS_CONFIG_TYPE_ARRAY(alloc_methods)},

    {"FAILURE", "diag",
     "Level of network failure reporting",
     ucs_offsetof(sct_iface_config_t, failure), UCS_CONFIG_TYPE_ENUM(ucs_log_level_names)},

    {"MAX_NUM_EPS", "inf",
     "Maximum number of endpoints that the transport interface is able to create",
     ucs_offsetof(sct_iface_config_t, max_num_eps), UCS_CONFIG_TYPE_ULUNITS},

    {NULL}
};

ucs_status_t sct_iface_free_stars_stream(sct_iface_h tl_iface, void *handle)
{
    if (handle == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }

    api_stars_release_handle(handle);
    return UCS_OK;
}