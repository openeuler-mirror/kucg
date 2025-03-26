/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "bcast.h"
#include "planc_stars_plan.h"
#include "planc_stars_global.h"

#define PLAN_DOMAIN "planc stars bcast"

static ucg_plan_attr_t ucg_planc_stars_bcast_plan_attr[] = {
    {UCG_STARS_ALGO_PRE_NAME(bcast_ring_m),
     1, "bcast ring-modified", PLAN_DOMAIN},

    {UCG_STARS_ALGO_PRE_NAME(bcast_ring_2m),
     2, "bcast ring-2-modified", PLAN_DOMAIN},

    {UCG_STARS_ALGO_PRE_NAME(bcast_long),
     3, "bcast long", PLAN_DOMAIN},

    {UCG_STARS_ALGO_PRE_NAME(bcast_long_m),
     4, "bcast long modified", PLAN_DOMAIN},

    {NULL},
};

UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_stars, UCG_COLL_TYPE_BCAST,
                             ucg_planc_stars_bcast_plan_attr);

UCG_PLANC_STARS_ALGO_REGISTER(UCG_COLL_TYPE_BCAST, NULL, 0)

static ucg_plan_policy_t bcast_4_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_4_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_8_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_16_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t bcast_LG_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};

static ucg_plan_policy_t* bcast_plan_policy[] = {
    bcast_4_1,
    bcast_4_4,
    bcast_4_8,
    bcast_4_16,
    bcast_4_32,
    bcast_4_64,
    bcast_4_LG,
    bcast_8_1,
    bcast_8_4,
    bcast_8_8,
    bcast_8_16,
    bcast_8_32,
    bcast_8_64,
    bcast_8_LG,
    bcast_16_1,
    bcast_16_4,
    bcast_16_8,
    bcast_16_16,
    bcast_16_32,
    bcast_16_64,
    bcast_16_LG,
    bcast_LG_1,
    bcast_LG_4,
    bcast_LG_8,
    bcast_LG_16,
    bcast_LG_32,
    bcast_LG_64,
    bcast_LG_LG,
};

const ucg_plan_policy_t *ucg_planc_stars_get_bcast_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                               ucg_planc_stars_ppn_level_t ppn_level)
{
    int idx = node_level * PPN_LEVEL_STARS_NUMS + ppn_level;
    ucg_assert(idx < NODE_LEVEL_STARS_NUMS * PPN_LEVEL_STARS_NUMS);
    ucg_plan_policy_t *policy = bcast_plan_policy[idx];
    return policy;
}