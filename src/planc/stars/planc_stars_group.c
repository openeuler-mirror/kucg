/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_group.h"

#define STREAM_MIN_DEPTH 4096
#define STREAM_MAX_DEPTH 65535

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
    size_t stream_depth = stars_ctx->scp_context->scp_config->ctx.stream_depth;
    if (stream_depth < STREAM_MIN_DEPTH) {
        ucg_warn("Invalid stream_depth %zu, which should be 4096 at least,"
                 " automatically set to 4096", stream_depth);
        stream_depth = STREAM_MIN_DEPTH;
    } else if (stream_depth > STREAM_MAX_DEPTH) {
        ucg_warn("Invalid stream_depth %zu, which should be 65535 at most,"
                 " automatically set to 65535", stream_depth);
        stream_depth = STREAM_MAX_DEPTH;
    }

    status = scp_worker_create_stars_stream(stars_ctx->scp_worker,
                                            &stars_group->stars_stream, stream_depth);
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
