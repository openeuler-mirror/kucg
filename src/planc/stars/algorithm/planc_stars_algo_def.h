/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_PLANC_STARS_ALGO_DEF_H_
#define XUCG_PLANC_STARS_ALGO_DEF_H_

#include "planc_stars_def.h"

KHASH_INIT(rank_idx, ucg_rank_t, uint32_t, 1,
           kh_int_hash_func, kh_int_hash_equal);

typedef enum rank_idx_map_status {
    RANK_IDX_MAP_OK,
    RANK_IDX_MAP_NOT_FOUND,
    RANK_IDX_MAP_ERROR
} rank_idx_map_status_t;

#define UCG_STARS_ALGO_FUN(_algo, _fun) ucg_planc_stars_##_algo##_##_fun
#define UCG_STARS_ALGO_PRE_NAME(_algo) ucg_planc_stars_##_algo##_prepare
#define UCG_STARS_ALGO_PRE_DECLARE(_algo)                                           \
    extern ucg_status_t UCG_STARS_ALGO_PRE_NAME(_algo)(ucg_vgroup_t *vgroup,        \
                                                       const ucg_coll_args_t *args, \
                                                       ucg_plan_op_t **op)

typedef struct ucg_planc_stars_bcast {
    union {
        ucg_algo_kntree_iter_t kntree_iter;
        ucg_algo_ring_iter_t ring_iter;
        struct {
            ucg_algo_kntree_iter_t kntree_iter;
            ucg_algo_ring_iter_t ring_iter;
            uint32_t recvcount;
            uint32_t lastcount;
            /* Extra count in the last block */
            uint32_t division;
            ucg_rank_t lastrank;
            khash_t(rank_idx) rank_idx_map;
        } longg;
    };
} ucg_planc_stars_bcast_t;

typedef struct ucg_planc_stars_allgatherv {
    union {
        struct {
            uint32_t loop_count;
            uint32_t loop_max;
            int32_t send_data_from;
            int32_t recv_data_from[2];
            int32_t neighbor[2];
            int32_t offset_at_step[2];
        } neighbor;
        ucg_algo_ring_iter_t ring_iter;
    };
} ucg_planc_stars_allgatherv_t;

typedef enum {
    HPL_MODE,
    COMMON_MODE
} SCATTERV_RUN_MODE;

typedef struct ucg_planc_stars_scatterv {
    union {
        struct {
            ucg_algo_kntree_iter_t kntree_iter;
            /**
            * For example:
            *      degree=2
            *        0
            *       / \ \
            *      4   2 1
            *      |\  |
            *      6 5 3
            *      |
            *      7
            * the staging_count of rank 4 is 3, means staging area stores the data of
            * rank 5,6,7 (sequential increment).
            */
            uint32_t staging_count;
            /* staging_displs[] indicates the start address of each rank in staging area.
             * staging_displs[] of 0 include rank 1 2 3 4 5 6 7. staging_displs[] of 4 include rank 6 5
             */
            /* sendcounts[] of root rank */
            int32_t *sendcounts;
            int32_t sendtype_size;
            int32_t *staging_displs;
            /* v_group_size indicates the size of kntree_iter after removing procs with recvcount=0 */
            uint32_t v_group_size;
            /* v_myrank indicates my rank in kntree_iter after removing procs with recvcount=0 */
            ucg_rank_t v_myrank;
            /* parent_rank indicates real parent rank in vgroup */
            ucg_rank_t parent_rank;
            /* child_num indicates the num of immediate children after removing the procs with recvcount=0 */
            uint32_t child_num;
            /* child_rank[] indicates the real rank of my child in vgroup after removing the procs with recvcount=0. */
            ucg_rank_t *child_rank;
            /* child_total_size[] indicates total data size that needs to send to each child (just non-root need).
             * the child_total_size of 4: (6+7),(5).
             */
            int32_t *child_total_size;
            /* rank_map[] indicates a mapping between a virtual rank after recvcount=0 procs is excluded and a real rank
             * (just root need).
             */
            int32_t *rank_map;
            int8_t is_empty;
            SCATTERV_RUN_MODE run_mode;
        } kntree;
    };
} ucg_planc_stars_scatterv_t;

typedef struct ucg_planc_stars_barrier {
    union {
        struct {
            ucg_algo_kntree_iter_t fanin_iter;
            ucg_algo_kntree_iter_t fanout_iter;
        } kntree;
    };
} ucg_planc_stars_barrier_t;

typedef struct ucg_planc_stars_alltoallv {
    ucg_algo_ring_iter_t ring_iter;
} ucg_planc_stars_alltoallv_t;

static inline rank_idx_map_status_t ucg_planc_stars_algo_get_rank_idx_from_map(ucg_rank_t rank,
                                                                               khash_t(rank_idx)* rank_idx_map,
                                                                               uint32_t *idx_out)
{
    khiter_t hash_iter;

    hash_iter = kh_get(rank_idx, rank_idx_map, rank);
    if (hash_iter != kh_end(rank_idx_map)) {
        *idx_out = kh_value(rank_idx_map, hash_iter);
        return RANK_IDX_MAP_OK;
    }
    return RANK_IDX_MAP_NOT_FOUND;
}

static inline rank_idx_map_status_t ucg_planc_stars_algo_put_rank_idx_to_map(ucg_rank_t rank,
                                                                             uint32_t put_rank_idx,
                                                                             khash_t(rank_idx)* rank_idx_map)
{
    int ret = 0;
    khiter_t hash_iter = kh_put(rank_idx, rank_idx_map, rank, &ret);
    if (ret == -1) {
        ucg_error("Failed to put rank idx to hash!");
        return RANK_IDX_MAP_ERROR;
    }
    kh_value(rank_idx_map, hash_iter) = put_rank_idx;
    return RANK_IDX_MAP_OK;
}

const ucg_plan_policy_t *ucg_planc_stars_get_allgatherv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                    ucg_planc_stars_ppn_level_t ppn_level);

const ucg_plan_policy_t *ucg_planc_stars_get_bcast_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                               ucg_planc_stars_ppn_level_t ppn_level);

const ucg_plan_policy_t *ucg_planc_stars_get_scatterv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                  ucg_planc_stars_ppn_level_t ppn_level);

const ucg_plan_policy_t *ucg_planc_stars_get_barrier_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                 ucg_planc_stars_ppn_level_t ppn_level);

const ucg_plan_policy_t *ucg_planc_stars_get_alltoallv_plan_policy(ucg_planc_stars_node_level_t node_level,
                                                                   ucg_planc_stars_ppn_level_t ppn_level);

#endif