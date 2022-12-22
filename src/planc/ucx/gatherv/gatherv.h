/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#ifndef UCG_PLANC_UCX_GATHERV_H_
#define UCG_PLANC_UCX_GATHERV_H_

#include "planc_ucx_def.h"
#include "core/ucg_plan.h"

const ucg_plan_policy_t *ucg_planc_ucx_get_gatherv_plan_policy(ucg_planc_ucx_node_level_t node_level,
                                                               ucg_planc_ucx_ppn_level_t ppn_level);

ucg_status_t ucg_planc_ucx_gatherv_linear_op_progress(ucg_plan_op_t *ucg_op);

ucg_planc_ucx_op_t *ucg_planc_ucx_gatherv_linear_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                        ucg_vgroup_t *vgroup,
                                                        const ucg_coll_args_t *args);

ucg_status_t ucg_planc_ucx_gatherv_linear_prepare(ucg_vgroup_t *vgroup,
                                                  const ucg_coll_args_t *args,
                                                  ucg_plan_op_t **op);

#endif // UCG_PLANC_UCX_GATHERV_H_