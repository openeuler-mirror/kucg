/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "allgatherv.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_global.h"

#define PLAN_DOMAIN "planc ucx allgatherv"

static ucg_plan_attr_t ucg_planc_ucx_allgatherv_plan_attr[] = {
    {ucg_planc_ucx_allgatherv_neighbor_prepare,
     1, "Neighbor exchange", PLAN_DOMAIN},

    {ucg_planc_ucx_allgatherv_ring_prepare,
     2, "Ring", PLAN_DOMAIN},

    {ucg_planc_ucx_allgatherv_ring_hpl_prepare,
     3, "Ring-HPL", PLAN_DOMAIN},

     {ucg_planc_ucx_allgatherv_linear_prepare,
     4, "Linear", PLAN_DOMAIN},

     {ucg_planc_ucx_allgatherv_bruck_prepare,
     5, "Bruck", PLAN_DOMAIN},

    {NULL},
};
UCG_PLAN_ATTR_REGISTER_TABLE(ucg_planc_ucx, UCG_COLL_TYPE_ALLGATHERV,
                             ucg_planc_ucx_allgatherv_plan_attr);

UCG_PLANC_UCX_BUILTIN_ALGO_REGISTER(UCG_COLL_TYPE_ALLGATHERV, NULL, 0)

static ucg_plan_policy_t allgatherv_4_1[] = {
    {4, {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_4[] = {
    {1, {0, 8192}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {8192, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 8192}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_8[] = {
    {1, {0, 4096}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {4096, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 4096}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_16[] = {
    {1, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {2048, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_32[] = {
    {1, {0, 16}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {16, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 262144}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {262144, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 16}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, 262144}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_64[] = {
    {2, {0, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 262144}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {262144, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {16384, 262144}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_4_LG[] = {
    {2, {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_1[] = {
    {4, {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_4[] = {
    {5, {0, 512}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {4, {512, 4096}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {4096, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_8[] = {
    {5, {0, 1024}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {1024, 524288}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {524288, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_16[] = {
    {1, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {2048, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_32[] = {
    {2, {0, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {65536, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_64[] = {
    {2, {0, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {65536, 262144}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {262144, 524288}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {524288, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {262144, 524288}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_8_LG[] = {
    {5, {0, 32}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {32, 1024}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {1024, 131072}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {131072, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {1024, 131072}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_1[] = {
    {4, {0, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_4[] = {
    {5, {0, 4096}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {4096, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_8[] = {
    {5, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {2048, 524288}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {524288, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {524288, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_16[] = {
    {1, {0, 4096}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {4096, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 131072}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {131072, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {0, 4096}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {16384, 131072}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_32[] = {
    {2, {0, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 131072}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {131072, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {16384, 131072}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_64[] = {
    {2, {0, 16384}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {65536, 262144}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {262144, 524288}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {524288, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {16384, 65536}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    {3, {262144, 524288}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_16_LG[] = {
    {5, {0, 16}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {16, 1024}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {1024, 32768}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {3, {32768, 131072}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {131072, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {1024, 32768}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_1[] = {
    {5, {0, 256}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {4, {256, 131072}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {131072, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {2, {131072, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_4[] = {
    {5, {0, 2048}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {4, {2048, 4096}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {4096, 262144}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {262144, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {262144, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
    UCG_PLAN_LAST_POLICY,
};
static ucg_plan_policy_t allgatherv_LG_8[] = {
    {5, {0, 1024}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {2, {1024, 1048576}, UCG_PLAN_UCX_PLAN_SCORE_1ST},
    {1, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_1ST},

    {3, {1048576, UCG_PLAN_RANGE_MAX}, UCG_PLAN_UCX_PLAN_SCORE_2ND},
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
    allgatherv_16_16, // allgatherv_LG_16
    allgatherv_16_32, // allgatherv_LG_32
    allgatherv_16_64, // allgatherv_LG_64
    allgatherv_16_LG, // allgatherv_LG_LG
};

const ucg_plan_policy_t *ucg_planc_ucx_get_allgatherv_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                                  ucg_planc_ucx_ppn_level_t ppn_level)
{
    int idx = node_level * PPN_LEVEL_NUMS + ppn_level;
    ucg_assert(idx < NODE_LEVEL_NUMS * PPN_LEVEL_NUMS);
    ucg_plan_policy_t *policy = allgatherv_plan_policy[idx];
    return policy;
}
