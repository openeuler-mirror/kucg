/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "sct_component.h"


UCS_LIST_HEAD(sct_components_list);

ucs_status_t sct_query_components(sct_component_h **components_p,
                                  unsigned *num_components_p)
{
    sct_component_h *components;
    sct_component_t *component;
    size_t num_components;

    num_components = ucs_list_length(&sct_components_list);
    components = ucg_malloc(num_components * sizeof(*components),
                            "uct_components");
    if (components == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    ucg_assert(num_components < UINT_MAX);
    *num_components_p = num_components;
    *components_p     = components;

    ucs_list_for_each(component, &sct_components_list, list) {
        *(components++) = component;
    }

    return UCS_OK;
}

void sct_release_component_list(sct_component_h *components)
{
    ucg_free(components);
}

ucs_status_t sct_component_query(sct_component_h component,
                                 uct_component_attr_t *component_attr)
{
    uct_md_resource_desc_t *resources = NULL;
    unsigned num_resources            = 0;
    ucs_status_t status;

    if (component_attr->field_mask & (UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT|
                                      UCT_COMPONENT_ATTR_FIELD_MD_RESOURCES)) {
        status = component->query_md_resources(component, &resources,
                                               &num_resources);
        if (status != UCS_OK) {
            return status;
        }
    }

    if (component_attr->field_mask & UCT_COMPONENT_ATTR_FIELD_NAME) {
        ucs_snprintf_zero(component_attr->name, sizeof(component_attr->name),
                          "%s", component->name);
    }

    if (component_attr->field_mask & UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT) {
        component_attr->md_resource_count = num_resources;
    }

    if ((resources != NULL) &&
        (component_attr->field_mask & UCT_COMPONENT_ATTR_FIELD_MD_RESOURCES)) {
        memcpy(component_attr->md_resources, resources,
               sizeof(uct_md_resource_desc_t) * num_resources);
    }

    if (component_attr->field_mask & UCT_COMPONENT_ATTR_FIELD_FLAGS) {
        component_attr->flags = component->flags;
    }

    ucg_free(resources);
    return UCS_OK;
}

ucs_status_t sct_config_read(sct_config_bundle_t **bundle,
                             ucs_config_field_t *config_table,
                             size_t config_size, const char *env_prefix,
                             const char *cfg_prefix)
{
    char full_prefix[128] = STARS_DEFAULT_ENV_PREFIX;
    sct_config_bundle_t *config_bundle;
    ucs_status_t status;

    config_bundle = ucg_calloc(1, sizeof(*config_bundle) + config_size, "uct_config");
    if (config_bundle == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto err;
    }

    if ((env_prefix != NULL) && (strlen(env_prefix) > 0)) {
        ucs_snprintf_zero(full_prefix, sizeof(full_prefix), "%s_%s",
                          env_prefix, STARS_DEFAULT_ENV_PREFIX);
    }

    ucg_config_global_list_entry_t entry = {
        .table  = config_table,
        .prefix = cfg_prefix,
        .flags  = 0
    };

    status = ucg_config_parser_fill_opts(config_bundle->data, &entry,
                                         full_prefix, 0);
    if (status != UCS_OK) {
        goto err_free_bundle;
    }

    config_bundle->table = config_table;
    config_bundle->table_prefix = ucg_strdup(cfg_prefix, "uct_config");
    if (config_bundle->table_prefix == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto err_free_bundle;
    }

    *bundle = config_bundle;
    return UCS_OK;

err_free_bundle:
    ucg_free(config_bundle);
err:
    return status;
}
