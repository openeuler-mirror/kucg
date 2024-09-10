/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "allgatherv.h"
#include "planc_stars_plan.h"

#define PLAN_DOMAIN "planc stars allgatherv"

static ucg_plan_attr_t ucg_planc_stars_allgatherv_plan_attr[] = {
    {UCG_STARS_ALGO_PRE_NAME(allgatherv_rolling),
     1, "allgatherv rolling", PLAN_DOMAIN},

    {NULL},
};

UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_stars, UCG_COLL_TYPE_ALLGATHERV,
                             ucg_planc_stars_allgatherv_plan_attr);

UCG_PLANC_STARS_ALGO_REGISTER(UCG_COLL_TYPE_ALLGATHERV, NULL, 0)

static ucg_plan_policy_t allgatherv_4_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};

static ucg_plan_policy_t* allgatherv_plan_policy[] = {
    allgatherv_4_1,
    allgatherv_4_4,
    allgatherv_4_8,
    allgatherv_4_16,
    allgatherv_4_32,
    allgatherv_4_64,
    allgatherv_4_LG,
    allgatherv_8_1,
    allgatherv_8_4,
    allgatherv_8_8,
    allgatherv_8_16,
    allgatherv_8_32,
    allgatherv_8_64,
    allgatherv_8_LG,
    allgatherv_16_1,
    allgatherv_16_4,
    allgatherv_16_8,
    allgatherv_16_16,
    allgatherv_16_32,
    allgatherv_16_64,
    allgatherv_16_LG,
    allgatherv_LG_1,
    allgatherv_LG_4,
    allgatherv_LG_8,
    allgatherv_LG_16,
    allgatherv_LG_32,
    allgatherv_LG_64,
    allgatherv_LG_LG,
};

const ucg_plan_policy_t *ucg_planc_stars_get_allgatherv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                    ucg_planc_stars_ppn_level_t ppn_level)
{
    int idx = node_level * PPN_LEVEL_STARS_NUMS + ppn_level;
    ucg_assert(idx < NODE_LEVEL_STARS_NUMS * PPN_LEVEL_STARS_NUMS);
    ucg_plan_policy_t *policy = allgatherv_plan_policy[idx];
    return policy;
}