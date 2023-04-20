/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#ifndef UCG_PLANC_UCX_SCATTERV_H_
#define UCG_PLANC_UCX_SCATTERV_H_

#include "planc_ucx_def.h"
#include "planc_ucx_context.h"
#include "planc_ucx_group.h"
#include "core/ucg_plan.h"
#include "util/algo/ucg_kntree.h"

typedef struct ucg_planc_ucx_scatterv {
    union {
        struct {
            int32_t idx;
            uint8_t send_type;
        } linear;
        struct {
            ucg_algo_kntree_iter_t kntree_iter;
            int32_t first_trigger;
            /**
             * staging_count indicates the number of rank data in staging area.
             * For example:
             *      degree = 2
             *          0
             *      / /  \ \
             *    8  4    2  1
             *    |  | \  |
             *    9  6  5 3
             *       |
             *       7
             * The staging_count of rank 4 is 3, means staging area stores the data of
             * rank 5,6,7 (sequential increment).
             */
            uint32_t staging_count;
            /* staging_displs[] indicates the start address of each rank in staging area. */
            int32_t *staging_displs;
            /* sendcounts[] of root rank */
            int32_t *sendcounts;
            /* sendtype true length of root rank */
            int32_t sdtype_size;
        } kntree;
    };
} ucg_planc_ucx_scatterv_t;

typedef struct ucg_planc_ucx_scatterv_config {
    size_t min_bsend;    /* for linear, closed boundary */
    size_t max_bsend;    /* for linear, closed boundary */
    int kntree_degree;   /* for kntree */
} ucg_planc_ucx_scatterv_config_t;

const ucg_plan_policy_t *ucg_planc_ucx_get_scatterv_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                                ucg_planc_ucx_ppn_level_t ppn_level);

ucg_planc_ucx_op_t *ucg_planc_ucx_scatterv_kntree_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                         ucg_vgroup_t *vgroup,
                                                         const ucg_coll_args_t *args,
                                                         const ucg_planc_ucx_scatterv_config_t *config);

ucg_status_t ucg_planc_ucx_scatterv_kntree_prepare(ucg_vgroup_t *vgroup,
                                                   const ucg_coll_args_t *args,
                                                   ucg_plan_op_t **op);

ucg_status_t ucg_planc_ucx_scatterv_linear_op_progress(ucg_plan_op_t *ucg_op);

ucg_planc_ucx_op_t *ucg_planc_ucx_scatterv_linear_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                         ucg_vgroup_t *vgroup,
                                                         const ucg_coll_args_t *args,
                                                         const ucg_planc_ucx_scatterv_config_t *config);

ucg_status_t ucg_planc_ucx_scatterv_linear_prepare(ucg_vgroup_t *vgroup,
                                                   const ucg_coll_args_t *args,
                                                   ucg_plan_op_t **op);

#endif