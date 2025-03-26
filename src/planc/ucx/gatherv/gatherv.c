/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "gatherv.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_global.h"

#define PLAN_DOMAIN "planc ucx gatherv"

static ucg_plan_attr_t ucg_planc_ucx_gatherv_plan_attr[] = {
    {ucg_planc_ucx_gatherv_linear_prepare,
     1, "Linear", PLAN_DOMAIN},

    {NULL},
};

UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_ucx, UCG_COLL_TYPE_GATHERV,
                             ucg_planc_ucx_gatherv_plan_attr);

UCG_PLANC_UCX_BUILTIN_ALGO_REGISTER(UCG_COLL_TYPE_GATHERV, NULL, 0)

static ucg_plan_policy_t gatherv_plan_policy[] = {
    {1,  {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};

const ucg_plan_policy_t *ucg_planc_ucx_get_gatherv_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                               ucg_planc_ucx_ppn_level_t ppn_level)
{
    UCG_UNUSED(node_level, ppn_level);
    ucg_plan_policy_t *policy = gatherv_plan_policy;
    return policy;
}