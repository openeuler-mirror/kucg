/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_global.h"

ucg_planc_stars_algo_table_t stars_algo_global_table[UCG_COLL_TYPE_LAST];

static void ucg_planc_stars_global_fill_oob_resource(const ucg_global_params_t *params,
                                                     ucg_planc_stars_oob_resource_t *stars_oob_resource)
{
    if (ucg_test_flags(params->field_mask, UCG_GLOBAL_PARAMS_FIELD_OOB_RESOURCE)) {
        const ucg_oob_resource_t *ucg_oob_resource = &params->oob_resource;
        stars_oob_resource->get_ucp_ep = ucg_oob_resource->get_ucp_ep;
        stars_oob_resource->get_ucp_worker = ucg_oob_resource->get_ucp_worker;
        stars_oob_resource->arg = ucg_oob_resource->arg;
    } else {
        stars_oob_resource->get_ucp_ep = (ucg_planc_stars_get_ucp_ep_cb_t)ucg_empty_function_return_null;
        stars_oob_resource->get_ucp_worker = (ucg_planc_stars_get_ucp_worker_cb_t)ucg_empty_function_return_null;
        stars_oob_resource->arg = NULL;
    }
}

static ucg_status_t ucg_planc_stars_global_init(const ucg_global_params_t *params)
{
    ucg_planc_stars_t *planc_stars = ucg_planc_stars_instance();
    ucg_planc_stars_global_fill_oob_resource(params, &planc_stars->oob_resource);
    return UCG_OK;
}

static void ucg_planc_stars_global_cleanup(void)
{
    return;
}

ucg_planc_stars_t UCG_PLANC_OBJNAME(stars) = {
    .super.super.name         = "stars",
    .super.mem_query          = ucg_planc_stars_mem_query,

    .super.global_init        = ucg_planc_stars_global_init,
    .super.global_cleanup     = ucg_planc_stars_global_cleanup,

    .super.config_read        = ucg_planc_stars_config_read,
    .super.config_modify      = ucg_planc_stars_config_modify,
    .super.config_release     = ucg_planc_stars_config_release,

    .super.context_init       = ucg_planc_stars_context_init,
    .super.context_cleanup    = ucg_planc_stars_context_cleanup,
    .super.context_query      = ucg_planc_stars_context_query,
    .super.context_progress   = ucg_planc_stars_context_progress,

    .super.group_create       = ucg_planc_stars_group_create,
    .super.group_destroy      = ucg_planc_stars_group_destroy,

    .super.get_plans          = ucg_planc_stars_get_plans,

    .super.priority           = 1,
};

ucg_planc_stars_t *ucg_planc_stars_instance(void)
{
    return &UCG_PLANC_OBJNAME(stars);
}
