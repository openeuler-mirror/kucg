/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "reduce.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_global.h"

#define PLAN_DOMAIN "planc ucx reduce"

static ucg_plan_attr_t ucg_planc_ucx_reduce_plan_attr[] = {
    {ucg_planc_ucx_reduce_kntree_prepare,
     1, "K-nomial tree", PLAN_DOMAIN},

    {NULL},
};
UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_ucx, UCG_COLL_TYPE_REDUCE,
                             ucg_planc_ucx_reduce_plan_attr);

static ucg_config_field_t reduce_config_table[] = {
    {"REDUCE_KNTREE_DEGREE", "2",
     "Configure the k value in kntree algo for reduce",
     ucg_offsetof(ucg_planc_ucx_reduce_config_t, kntree_degree),
     UCG_CONFIG_TYPE_INT},

    {NULL}
};
UCG_PLANC_UCX_BUILTIN_ALGO_REGISTER(UCG_COLL_TYPE_REDUCE, reduce_config_table,
                                    sizeof(ucg_planc_ucx_reduce_config_t))

static ucg_plan_policy_t reduce_plan_policy[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};

const ucg_plan_policy_t *ucg_planc_ucx_get_reduce_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                              ucg_planc_ucx_ppn_level_t ppn_level)
{
    UCG_UNUSED(node_level, ppn_level);
    ucg_plan_policy_t *policy = reduce_plan_policy;
    return policy;
}