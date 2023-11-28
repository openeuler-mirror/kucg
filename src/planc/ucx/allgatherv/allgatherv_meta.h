/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "planc_ucx_meta.h"

#ifndef UCG_PLANC_UCX_ALLGATHERV_META_H_
#define UCG_PLANC_UCX_ALLGATHERV_META_H_

ucg_status_t ucg_planc_ucx_allgatherv_add_rolling_inter_op(ucg_plan_meta_op_t *meta_op,
                                                           ucg_planc_ucx_group_t *ucx_group,
                                                           ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args,
                                                           int32_t *global_ranks,
                                                           ucg_planc_ucx_algo_group_type_t group_type);

ucg_status_t ucg_planc_ucx_allgatherv_add_rolling_intra_op(ucg_plan_meta_op_t *meta_op,
                                                           ucg_planc_ucx_group_t *ucx_group,
                                                           int32_t *global_ranks,
                                                           ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args,
                                                           ucg_topo_group_type_t group_type);

#endif