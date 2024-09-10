/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "alltoallv.h"
#include "planc_stars_global.h"

#define PLAN_DOMAIN "planc stars alltoallv"

static ucg_plan_attr_t ucg_planc_stars_alltoallv_plan_attr[] = {
    {UCG_STARS_ALGO_PRE_NAME(alltoallv_pairwise),
     1, "alltoallv pairwise", PLAN_DOMAIN},

    {NULL}
};

UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_stars, UCG_COLL_TYPE_ALLTOALLV,
                             ucg_planc_stars_alltoallv_plan_attr);

UCG_PLANC_STARS_ALGO_REGISTER(UCG_COLL_TYPE_ALLTOALLV, NULL, 0);

static ucg_plan_policy_t alltoallv_plan_policy[] = {
    {1,  {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
     UCG_PLAN_LAST_POLICY,
};

const ucg_plan_policy_t *ucg_planc_stars_get_alltoallv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                   ucg_planc_stars_ppn_level_t ppn_level)
{
    UCG_UNUSED(node_level, ppn_level);
    ucg_plan_policy_t *policy = alltoallv_plan_policy;
    return policy;
}