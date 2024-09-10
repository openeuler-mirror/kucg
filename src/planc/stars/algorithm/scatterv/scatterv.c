/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "scatterv.h"

#include "planc_stars_plan.h"


#define PLAN_DOMAIN "planc stars scatterv"

static ucg_plan_attr_t ucg_planc_stars_scatterv_plan_attr[] = {
    {UCG_STARS_ALGO_PRE_NAME(scatterv_kntree),
     1, "scatterv kntree", PLAN_DOMAIN},

    {NULL},
};
UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_stars, UCG_COLL_TYPE_SCATTERV,
                             ucg_planc_stars_scatterv_plan_attr);

static ucg_config_field_t scatterv_config_table[] = {
    {"SCATTERV_KNTREE_DEGREE", "2",
     "Configure the k value in kntree algo for scatterv",
     ucg_offsetof(ucg_planc_stars_scatterv_config_t, kntree_degree),
     UCG_CONFIG_TYPE_INT},

    {"SCATTERV_RUN_HPL", "n",
     "Configure the scatterv running mode.\n"
     " - y : run in hpl mode\n"
     " - n : run in common mode",
     ucg_offsetof(ucg_planc_stars_scatterv_config_t, run_hpl),
     UCG_CONFIG_TYPE_BOOL},

    {NULL}
};
UCG_PLANC_STARS_ALGO_REGISTER(UCG_COLL_TYPE_SCATTERV, scatterv_config_table,
                                    sizeof(ucg_planc_stars_scatterv_config_t))

static ucg_plan_policy_t scatterv_plan_policy[] = {
    {1,  {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};

const ucg_plan_policy_t *ucg_planc_stars_get_scatterv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                  ucg_planc_stars_ppn_level_t ppn_level)
{
    UCG_UNUSED(node_level, ppn_level);
    ucg_plan_policy_t *policy = scatterv_plan_policy;
    return policy;
}
