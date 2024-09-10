/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "barrier.h"
#include "planc_stars_plan.h"
#include "planc_stars_global.h"

#define PLAN_DOMAIN "planc stars barrier"

static ucg_plan_attr_t ucg_planc_stars_barrier_plan_attr[] = {
    {UCG_STARS_ALGO_PRE_NAME(barrier_faninfanout),
     1, "barrier faninfanout", PLAN_DOMAIN},

    {NULL},
};

UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_stars, UCG_COLL_TYPE_BARRIER,
                             ucg_planc_stars_barrier_plan_attr);

static ucg_config_field_t barrier_config_table[] = {
    {"BARRIER_FANIN_DEGREE", "4",
     "Configure the k value in fanin kntree algo for barrier",
     ucg_offsetof(ucg_planc_stars_barrier_config_t, fanin_degree),
     UCG_CONFIG_TYPE_INT},

    {"BARRIER_FANOUT_DEGREE", "2",
     "Configure the k value in fanout kntree algo for barrier",
     ucg_offsetof(ucg_planc_stars_barrier_config_t, fanout_degree),
     UCG_CONFIG_TYPE_INT},

    {NULL}
};

UCG_PLANC_STARS_ALGO_REGISTER(UCG_COLL_TYPE_BARRIER, barrier_config_table,
                              sizeof(ucg_planc_stars_barrier_config_t))

static ucg_plan_policy_t barrier_4_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_4_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_8_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_16_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_1[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_4[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_8[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_16[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_32[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_64[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t barrier_LG_LG[] = {
    {1, {0, UCG_PLAN_RANGE_MAX}, UCG_PLANC_STARS_DEFAULT_SCORE},
    UCG_PLAN_LAST_POLICY,
};

static ucg_plan_policy_t* barrier_plan_policy[] = {
    barrier_4_1,
    barrier_4_4,
    barrier_4_8,
    barrier_4_16,
    barrier_4_32,
    barrier_4_64,
    barrier_4_LG,
    barrier_8_1,
    barrier_8_4,
    barrier_8_8,
    barrier_8_16,
    barrier_8_32,
    barrier_8_64,
    barrier_8_LG,
    barrier_16_1,
    barrier_16_4,
    barrier_16_8,
    barrier_16_16,
    barrier_16_32,
    barrier_16_64,
    barrier_16_LG,
    barrier_LG_1,
    barrier_LG_4,
    barrier_LG_8,
    barrier_LG_16,
    barrier_LG_32,
    barrier_LG_64,
    barrier_LG_LG,
};

const ucg_plan_policy_t *ucg_planc_stars_get_barrier_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                 ucg_planc_stars_ppn_level_t ppn_level)
{
    int idx = node_level * PPN_LEVEL_STARS_NUMS + ppn_level;
    ucg_assert(idx < NODE_LEVEL_STARS_NUMS * PPN_LEVEL_STARS_NUMS);
    ucg_plan_policy_t *policy = barrier_plan_policy[idx];
    return policy;
}
