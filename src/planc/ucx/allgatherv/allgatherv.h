/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#ifndef UCG_PLANC_UCX_ALLGATHERV_H_
#define UCG_PLANC_UCX_ALLGATHERV_H_

#include "planc_ucx_def.h"
#include "planc_ucx_context.h"
#include "core/ucg_plan.h"
#include "util/algo/ucg_ring.h"

typedef struct ucg_planc_ucx_allgatherv {
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
} ucg_planc_ucx_allgatherv_t;

const ucg_plan_policy_t *ucg_planc_ucx_get_allgatherv_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                                  ucg_planc_ucx_ppn_level_t ppn_level);

ucg_status_t ucg_planc_ucx_allgatherv_neighbor_prepare(ucg_vgroup_t *vgroup,
                                                       const ucg_coll_args_t *args,
                                                       ucg_plan_op_t **op);

ucg_status_t ucg_planc_ucx_allgatherv_ring_prepare(ucg_vgroup_t *vgroup,
                                                   const ucg_coll_args_t *args,
                                                   ucg_plan_op_t **op);

ucg_status_t ucg_planc_ucx_allgatherv_ring_hpl_prepare(ucg_vgroup_t *vgroup,
                                                       const ucg_coll_args_t *args,
                                                       ucg_plan_op_t **op);

#endif //UCG_PLANC_UCX_ALLGATHERV_H_