/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "planc_ucx_meta.h"

ucg_status_t ucg_planc_ucx_allgatherv_add_rolling_inter_op(ucg_plan_meta_op_t *meta_op,
                                                           ucg_planc_ucx_group_t *ucx_group,
                                                           ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args,
                                                           int32_t *global_ranks,
                                                           ucg_planc_ucx_algo_group_type_t group_type)
{
    ucg_planc_ucx_algo_group_t *algo_group = &ucx_group->groups[group_type];
    ucg_status_t status = UCG_OK;
    if (algo_group == NULL) {
        return UCG_ERR_UNSUPPORTED;
    }

    if (algo_group->state == UCG_ALGO_GROUP_STATE_DISABLE) {
        /* I'm not in the algo group. */
        return ucg_planc_ucx_add_empty_op(meta_op, ucx_group, vgroup);
    }

    if (algo_group->state != UCG_ALGO_GROUP_STATE_ENABLE) {
        /* The group state is incorrect. */
        return UCG_ERR_NO_RESOURCE;
    }

    int32_t num_nodes = algo_group->super.size;
    int32_t ppn = vgroup->size/num_nodes;
    int32_t *temp_displs = (int32_t *)ucg_malloc(num_nodes*sizeof(int32_t),
                                                 "leader temp displs");
    if (temp_displs == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err;
    }

    int32_t *temp_recvcounts = (int32_t *)ucg_malloc(num_nodes*sizeof(int32_t),
                                                     "leader temp recvcounts");
    if (temp_recvcounts == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err_free_displs;
    }

    if (!vgroup->group->topo->detail.nrank_continuous) {
        int my_index = -1;
        for (int i = 0; i < vgroup->size; ++i) {
            if (global_ranks[i] == vgroup->myrank) {
                my_index = i;
            }
        }
        for (int i = 0; i < num_nodes; ++i) {
            temp_displs[i] = args->allgatherv.displs[i * ppn + my_index % ppn];
            temp_recvcounts[i] = args->allgatherv.recvcounts[i * ppn + my_index % ppn];
        }
    } else {
        for (int i = 0; i < num_nodes; ++i) {
            temp_displs[i] = args->allgatherv.displs[i * ppn + vgroup->myrank % ppn];
            temp_recvcounts[i] = args->allgatherv.recvcounts[i * ppn + vgroup->myrank % ppn];
        }
    }

    const int32_t *store_displs = args->allgatherv.displs;
    const int32_t *store_recvcounts = args->allgatherv.recvcounts;
    ucg_coll_args_t *temp_args = (ucg_coll_args_t *)args;
    temp_args->allgatherv.displs = temp_displs;
    temp_args->allgatherv.recvcounts = temp_recvcounts;
    ucg_planc_ucx_op_t *ucx_op;
    ucx_op = ucg_planc_ucx_allgatherv_na_rolling_inter_op_new(ucx_group, &algo_group->super, temp_args);
    if (ucx_op == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err_free_recvcounts;
    }
    temp_args->allgatherv.displs = store_displs;
    temp_args->allgatherv.recvcounts = store_recvcounts;
    return ucg_plan_meta_op_add(meta_op, &ucx_op->super);

err_free_recvcounts:
    ucg_planc_ucx_free_ptr(&temp_recvcounts);
err_free_displs:
    ucg_planc_ucx_free_ptr(&temp_displs);
err:
    return status;
}

ucg_status_t ucg_planc_ucx_allgatherv_add_rolling_intra_op(ucg_plan_meta_op_t *meta_op,
                                                           ucg_planc_ucx_group_t *ucx_group,
                                                           int32_t *global_ranks,
                                                           ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args,
                                                           ucg_topo_group_type_t group_type)
{
    ucg_topo_group_t *topo_group;
    ucg_status_t status;

    topo_group = ucg_topo_get_group(vgroup->group->topo, group_type);
    if (topo_group == NULL) {
        return UCG_ERR_UNSUPPORTED;
    }

    if (topo_group->state == UCG_TOPO_GROUP_STATE_DISABLE) {
        /* I'm not in the topo group. */
        return ucg_planc_ucx_add_empty_op(meta_op, ucx_group, vgroup);
    }

    if (topo_group->state != UCG_TOPO_GROUP_STATE_ENABLE) {
        /* The group state is incorrect. */
        return UCG_ERR_NO_RESOURCE;
    }

    ucg_coll_args_t *temp_args = (ucg_coll_args_t *)args;
    ucg_planc_ucx_op_t *ucx_op;

    ucx_op = ucg_planc_ucx_allgatherv_na_rolling_intra_op_new(ucx_group, &topo_group->super, temp_args);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    status = ucg_plan_meta_op_add(meta_op, &ucx_op->super);
    return status;
}