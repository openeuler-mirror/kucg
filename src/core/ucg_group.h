/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef UCG_GROUP_H_
#define UCG_GROUP_H_

#include "ucg_context.h"
#include "ucg_rank_map.h"

/**
 * Like ompi/coll, a negative value is used to avoid tag conflicts with
 * point-to-point communication.
 */
#define UCG_GROUP_BASE_REQ_ID       (0x800000)
#define UCG_GROUP_END_REQ_ID        (0xFFFFFF)

typedef struct ucg_group {
    ucg_context_t *context;
    ucg_plans_t *plans;

    int32_t num_planc_groups;
    ucg_planc_group_h *planc_groups;
    ucg_topo_t *topo;

    /* user parameters */
    uint32_t id; /* group id */
    uint32_t size;
    ucg_rank_t myrank;
    ucg_rank_map_t rank_map; /* convert group rank to context rank */
    ucg_oob_group_t oob_group;

    /* collective operation request id */
    int unique_req_id;
} ucg_group_t;

/**
 * @brief Convert group rank to context rank.
 *
 * @param [in] group    UCG Group
 * @param [in] rank     Group rank
 * @return context rank
 */
static inline ucg_rank_t ucg_group_get_ctx_rank(ucg_group_t *group, ucg_rank_t rank)
{
    return ucg_rank_map_eval(&group->rank_map, rank);
}

/**
 * @brief Get process location by group rank.
 *
 * @param [in] group        UCG Group
 * @param [in] rank         Group rank
 * @param [out] location    Location
 */
static inline ucg_status_t ucg_group_get_location(ucg_group_t *group,
                                                  ucg_rank_t rank,
                                                  ucg_location_t *location)
{
    ucg_rank_t ctx_rank = ucg_rank_map_eval(&group->rank_map, rank);
    ucg_assert(ctx_rank != UCG_INVALID_RANK);
    return ucg_context_get_location(group->context, ctx_rank, location);
}

/* In the same communication group, different members must obtain the same request
   ID when executing this function at the same time.*/
static inline int ucg_group_alloc_req_id(ucg_group_t *ucg_group)
{
    int unique_req_id = ++ucg_group->unique_req_id;
    if (unique_req_id == UCG_GROUP_END_REQ_ID) {
        unique_req_id = UCG_GROUP_BASE_REQ_ID + 1;
        ucg_group->unique_req_id = unique_req_id;
    }
    return unique_req_id;
}

/* Release the request id returned by ucg_group_alloc_req_id() */
static inline void ucg_group_free_req_id(ucg_group_t *ucg_group, uint16_t req_id)
{
    /* Do nothing based on the current implementation. */
    return;
}

#endif