/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_group.h"


ucg_status_t ucg_planc_stars_group_create(ucg_planc_context_h context,
                                          const ucg_planc_group_params_t *params,
                                          ucg_planc_group_h *planc_group)
{
    UCG_CHECK_NULL_INVALID(context, params, planc_group);

    ucg_status_t status;
    ucg_planc_stars_group_t *stars_group;
    stars_group = ucg_malloc(sizeof(ucg_planc_stars_group_t), "ucg planc stars group");
    UCG_ASSERT_RET(stars_group != NULL, UCG_ERR_NO_MEMORY);

    stars_group->context = context;
    status = UCG_CLASS_CONSTRUCT(ucg_planc_group_t, &stars_group->super, params->group);
    UCG_ASSERT_CODE_GOTO(status, err_free_group);

    ucg_planc_stars_context_t *stars_ctx = (ucg_planc_stars_context_t *)context;
    status = scp_worker_create_stars_stream(stars_ctx->scp_worker,
                                            &stars_group->stars_stream);
    UCG_ASSERT_CODE_GOTO(status, err_free_group);

    *planc_group = (ucg_planc_group_h)stars_group;
    return UCG_OK;

err_free_group:
    ucg_free(stars_group);
    return status;
}

void ucg_planc_stars_group_destroy(ucg_planc_group_h planc_group)
{
    ucg_planc_stars_group_t *stars_group = ucg_derived_of(planc_group, ucg_planc_stars_group_t);
    ucg_planc_stars_context_t *stars_ctx = (ucg_planc_stars_context_t *)stars_group->context;

    scp_worker_delete_stars_stream(stars_ctx->scp_worker, &stars_group->stars_stream);
    UCG_CLASS_DESTRUCT(ucg_planc_group_t, &stars_group->super);
    ucg_free(stars_group);
}
