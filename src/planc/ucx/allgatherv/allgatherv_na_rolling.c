/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "allgatherv.h"
#include "allgatherv_meta.h"

enum {
    UCG_ALLGATHERV_NA_ROLLING_INTER_SEND = UCG_BIT(0),
    UCG_ALLGATHERV_NA_ROLLING_INTER_RECV = UCG_BIT(1),
    UCG_ALLGATHERV_NA_ROLLING_INTRA_SEND = UCG_BIT(2),
    UCG_ALLGATHERV_NA_ROLLING_INTRA_RECV = UCG_BIT(3),

};


#define UCG_ALLGATHERV_NA_ROLLING_FLAGS (UCG_ALLGATHERV_NA_ROLLING_INTER_SEND | UCG_ALLGATHERV_NA_ROLLING_INTER_RECV | \
                                         UCG_ALLGATHERV_NA_ROLLING_INTRA_SEND | UCG_ALLGATHERV_NA_ROLLING_INTRA_RECV)
#define UCG_ALLGATHERV_NA_ROLLING_INTER_FLAGS (UCG_ALLGATHERV_NA_ROLLING_INTER_SEND | \
                                               UCG_ALLGATHERV_NA_ROLLING_INTER_RECV)
#define UCG_ALLGATHERV_NA_ROLLING_INTRA_FLAGS (UCG_ALLGATHERV_NA_ROLLING_INTRA_SEND | \
                                               UCG_ALLGATHERV_NA_ROLLING_INTRA_RECV)
static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_check(ucg_vgroup_t *vgroup,
                                                              const ucg_coll_args_t *args)
{
    int32_t ppn = vgroup->group->topo->ppn;
    int32_t num_nodes = vgroup->size / ppn;
    if (num_nodes == 1) {
        ucg_info("Allgatherv na_rolling don't support only one node");
        return UCG_ERR_UNSUPPORTED;
    }
    if (ppn == UCG_TOPO_PPX_UNKNOWN) {
        ucg_info("Allgatherv na_rolling don't support unknown ppn");
        return UCG_ERR_UNSUPPORTED;
    }
    if (ppn == 1) {
        ucg_info("Allgatherv na_rolling don't support ppn==1");
        return UCG_ERR_UNSUPPORTED;
    }
    if (ppn == UCG_TOPO_PPX_UNBALANCED) {
        ucg_info("Allgatherv na_rolling don't support unbalanced ppn");
        return UCG_ERR_UNSUPPORTED;
    }

    return UCG_OK;
}

static ucg_status_t ucg_planc_ucx_na_rolling_inter_odd_step(ucg_planc_ucx_op_t *op, int left_idx, int right_idx,
                                                            int tag, ucg_planc_ucx_p2p_params_t* params)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_status_t status = UCG_OK;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_rank_t right_peer = (myrank + 1) % group_size;
    ucg_rank_t left_peer = (myrank + group_size - 1) % group_size;
    int32_t lleft = (left_idx + group_size - 1) % group_size;
    int32_t rright = (right_idx + 1) % group_size;
    int32_t flag = myrank % 2;
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    int32_t odd = group_size % 2;
    if (!flag) {
        void *recvbuf = args->recvbuf + args->displs[rright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[rright],
                                         args->recvtype, right_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[left_idx] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[left_idx],
                                         args->recvtype, right_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->right_flag = 1;
    }
    if (flag || (!myrank && odd)) {
        void *recvbuf = args->recvbuf + args->displs[lleft] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[lleft],
                                         args->recvtype, left_peer, tag,
                                         vgroup, params);

        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[right_idx] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[right_idx],
                                         args->recvtype, left_peer, tag,
                                         vgroup, params);

        UCG_CHECK_ERR_GOTO(status, out);
        iter->left_flag = 1;
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_na_rolling_inter_even_step(ucg_planc_ucx_op_t *op, int left_idx, int right_idx,
                                                             int tag, ucg_planc_ucx_p2p_params_t* params)
{
    ucg_status_t status = UCG_OK;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_rank_t right_peer = (myrank + 1) % group_size;
    ucg_rank_t left_peer = (myrank + group_size - 1) % group_size;
    int32_t lleft = (left_idx + group_size - 1) % group_size;
    int32_t rright = (right_idx + 1) % group_size;
    int32_t rleft = (left_idx + 1) % group_size;
    int32_t lright = (right_idx + group_size - 1) % group_size;
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    int32_t flag = myrank % 2;
    int32_t odd = group_size % 2;
    if (myrank == 0 && odd) {
        /* do nothing */
    } else if (!flag) {
        void *recvbuf = args->recvbuf + args->displs[lleft] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[lleft],
                                         args->recvtype, left_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[lright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[lright],
                                         args->recvtype, left_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->left_flag = 1;
    } else {
        void *recvbuf = args->recvbuf + args->displs[rright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[rright],
                                         args->recvtype, right_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[rleft] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[rleft],
                                         args->recvtype, right_peer, tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->right_flag = 1;
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_inter_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    uint32_t group_size = op->super.vgroup->size;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    int32_t odd_step = ucg_algo_rolling_iter_get_odd_step(iter);
    while (!ucg_algo_rolling_iter_full(iter, group_size)) {
        int32_t left_idx = ucg_algo_rolling_iter_left_idx(iter);
        int32_t right_idx = ucg_algo_rolling_iter_right_idx(iter);
        if (ucg_test_and_clear_flags(&op->flags, UCG_ALLGATHERV_NA_ROLLING_INTER_FLAGS)) {
            if (odd_step) {
                status = ucg_planc_ucx_na_rolling_inter_odd_step(op, left_idx, right_idx,
                                                                 op->tag, &params);
                UCG_CHECK_ERR_GOTO(status, out);
            } else {
                status = ucg_planc_ucx_na_rolling_inter_even_step(op, left_idx, right_idx,
                                                                  op->tag, &params);
            }
        }
        status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);
        ucg_algo_rolling_iter_next(iter);
        odd_step = ucg_algo_rolling_iter_get_odd_step(iter);
        if (op->allgatherv.rolling_iter.right_flag) {
            right_idx = ucg_algo_rolling_iter_right_inc(iter, group_size);
            op->allgatherv.rolling_iter.right_flag = 0;
        }
        if (op->allgatherv.rolling_iter.left_flag) {
            left_idx = ucg_algo_rolling_iter_left_inc(iter, group_size);
            op->allgatherv.rolling_iter.left_flag = 0;
        }
        op->flags |= UCG_ALLGATHERV_NA_ROLLING_INTER_FLAGS;
    }

out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_inter_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);
    op->flags = UCG_ALLGATHERV_NA_ROLLING_INTER_FLAGS;
    ucg_algo_rolling_iter_init(&op->allgatherv.rolling_iter, ucg_op->vgroup->myrank);
    ucg_algo_rolling_iter_reset(&op->allgatherv.rolling_iter);

    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    const void *sendbuf = args->sendbuf;
    if (sendbuf != UCG_IN_PLACE) {
        ucg_rank_t myrank = op->super.vgroup->myrank;
        int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
        void *recvbuf = args->recvbuf + args->displs[myrank] * recvtype_extent;
        status = ucg_dt_memcpy(recvbuf, args->recvcounts[myrank], args->recvtype,
                               sendbuf, args->sendcount, args->sendtype);
        UCG_CHECK_GOTO(status, out);
    }
    status = ucg_planc_ucx_allgatherv_na_rolling_inter_op_progress(ucg_op);
out:
    return status == UCG_INPROGRESS ? UCG_OK : status;
}


static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_inter_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_planc_ucx_free_ptr((void **)&args->displs);
    ucg_planc_ucx_free_ptr((void **)&args->recvcounts);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

ucg_planc_ucx_op_t *ucg_planc_ucx_allgatherv_na_rolling_inter_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                                     ucg_vgroup_t *vgroup,
                                                                     const ucg_coll_args_t *args)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (ucx_op == NULL) {
        goto err;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_allgatherv_na_rolling_inter_op_trigger,
                                 ucg_planc_ucx_allgatherv_na_rolling_inter_op_progress,
                                 ucg_planc_ucx_allgatherv_na_rolling_inter_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(ucx_op, ucx_group);
    return ucx_op;

err_free_op:
    ucg_mpool_put(ucx_op);
err:
    return NULL;
}

static ucg_status_t ucg_planc_ucx_na_rolling_intra_odd_step(ucg_planc_ucx_op_t *op, int left_idx, int right_idx,
                                                            ucg_coll_allgatherv_args_t *args,
                                                            ucg_planc_ucx_p2p_params_t* params)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_status_t status = UCG_OK;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_rank_t right_peer = (myrank + 1) % group_size;
    ucg_rank_t left_peer = (myrank + group_size - 1) % group_size;
    int32_t lleft = (left_idx + group_size - 1) % group_size;
    int32_t rright = (right_idx + 1) % group_size;
    int32_t flag = myrank % 2;
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    int32_t odd = group_size % 2;
    if (!flag) {
        void *recvbuf = args->recvbuf + args->displs[rright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[rright],
                                         args->recvtype, right_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[left_idx] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[left_idx],
                                         args->recvtype, right_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->right_flag = 1;
    }
    if (flag || (!myrank && odd)) {
        void *recvbuf = args->recvbuf + args->displs[lleft] * recvtype_extent;

        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[lleft],
                                         args->recvtype, left_peer, op->tag,
                                         vgroup, params);

        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[right_idx] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[right_idx],
                                         args->recvtype, left_peer, op->tag,
                                         vgroup, params);

        UCG_CHECK_ERR_GOTO(status, out);
        iter->left_flag = 1;
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_na_rolling_intra_even_step(ucg_planc_ucx_op_t *op, int left_idx, int right_idx,
                                                             ucg_coll_allgatherv_args_t *args,
                                                             ucg_planc_ucx_p2p_params_t* params)
{
    ucg_status_t status = UCG_OK;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_rank_t right_peer = (myrank + 1) % group_size;
    ucg_rank_t left_peer = (myrank + group_size - 1) % group_size;
    int32_t lleft = (left_idx + group_size - 1) % group_size;
    int32_t rright = (right_idx + 1) % group_size;
    int32_t rleft = (left_idx + 1) % group_size;
    int32_t lright = (right_idx + group_size - 1) % group_size;
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    int32_t flag = myrank % 2;
    int32_t odd = group_size % 2;
    if (myrank == 0 && odd) {
        /* do nothing */
    } else if (!flag) {
        void *recvbuf = args->recvbuf + args->displs[lleft] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[lleft],
                                         args->recvtype, left_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[lright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[lright],
                                         args->recvtype, left_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->left_flag = 1;
    } else {
        void *recvbuf = args->recvbuf + args->displs[rright] * recvtype_extent;
        status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[rright],
                                         args->recvtype, right_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        void *sendbuf = args->recvbuf + args->displs[rleft] * recvtype_extent;
        status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[rleft],
                                         args->recvtype, right_peer, op->tag,
                                         vgroup, params);
        UCG_CHECK_ERR_GOTO(status, out);
        iter->right_flag = 1;
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_intra_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    uint32_t group_size = op->super.vgroup->size;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    int32_t total_size = op->super.vgroup->group->size;
    int32_t num_nodes = total_size / group_size;
    ucg_algo_rolling_iter_t *iter = &op->allgatherv.rolling_iter;
    ucg_coll_allgatherv_args_t *temp_args = (ucg_coll_allgatherv_args_t *)&op->super.super.args.allgatherv;
    int32_t odd_step = ucg_algo_rolling_iter_get_odd_step(iter);

    while (!ucg_algo_rolling_iter_full(iter, group_size)) {
        int32_t left_idx = ucg_algo_rolling_iter_left_idx(iter);
        int32_t right_idx = ucg_algo_rolling_iter_right_idx(iter);
        if (ucg_test_and_clear_flags(&op->flags, UCG_ALLGATHERV_NA_ROLLING_INTRA_FLAGS)) {
            for (int it = 0; it < num_nodes; ++it) {
                temp_args->displs += it * group_size;
                temp_args->recvcounts += it * group_size;
                status = odd_step ?
                ucg_planc_ucx_na_rolling_intra_odd_step(op, left_idx, right_idx, temp_args, &params) :
                ucg_planc_ucx_na_rolling_intra_even_step(op, left_idx, right_idx, temp_args, &params);
                UCG_CHECK_ERR_GOTO(status, out);
                temp_args->displs -= it * group_size;
                temp_args->recvcounts -= it * group_size;
            }
        }
        status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);
        ucg_algo_rolling_iter_next(iter);
        odd_step = ucg_algo_rolling_iter_get_odd_step(iter);
        if (op->allgatherv.rolling_iter.right_flag) {
            right_idx = ucg_algo_rolling_iter_right_inc(iter, group_size);
            op->allgatherv.rolling_iter.right_flag = 0;
        }
        if (op->allgatherv.rolling_iter.left_flag) {
            left_idx = ucg_algo_rolling_iter_left_inc(iter, group_size);
            op->allgatherv.rolling_iter.left_flag = 0;
        }
        op->flags |= UCG_ALLGATHERV_NA_ROLLING_INTRA_FLAGS;
    }

out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_intra_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_algo_rolling_iter_init(&op->allgatherv.rolling_iter, ucg_op->vgroup->myrank);
    ucg_planc_ucx_op_reset(op);
    op->flags = UCG_ALLGATHERV_NA_ROLLING_INTRA_FLAGS;
    ucg_algo_rolling_iter_reset(&op->allgatherv.rolling_iter);
    status = ucg_planc_ucx_allgatherv_na_rolling_intra_op_progress(ucg_op);
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_intra_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_planc_ucx_free_ptr((void **)&args->displs);
    ucg_planc_ucx_free_ptr((void **)&args->recvcounts);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

ucg_planc_ucx_op_t *ucg_planc_ucx_allgatherv_na_rolling_intra_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                                     ucg_vgroup_t *vgroup,
                                                                     const ucg_coll_args_t *args)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (ucx_op == NULL) {
        goto err;
    }
    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_allgatherv_na_rolling_intra_op_trigger,
                                 ucg_planc_ucx_allgatherv_na_rolling_intra_op_progress,
                                 ucg_planc_ucx_allgatherv_na_rolling_intra_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(ucx_op, ucx_group);
    return ucx_op;

err_free_op:
    ucg_mpool_put(ucx_op);
err:
    return NULL;
}

static ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_get_global_ranks(ucg_vgroup_t *vgroup, int32_t *global_ranks)
{
    uint32_t size = vgroup->size;
    ucg_topo_group_t *node_group = ucg_topo_get_group(vgroup->group->topo,
                                                      UCG_TOPO_GROUP_TYPE_NODE);
    uint32_t ppn = node_group->super.size;
    uint32_t num_nodes = size / ppn;
    int32_t *temp_nodes = ucg_calloc(num_nodes, sizeof(ucg_rank_t), "na rolling");
    if (temp_nodes == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    for (uint32_t i = 0; i < size; ++i) {
        int32_t node_id = ucg_topo_get_location_id(vgroup->group->topo, i,
                                                   UCG_TOPO_LOC_NODE_ID);
        if (node_id < 0) {
            return UCG_ERR_UNSUPPORTED;
        }
        global_ranks[node_id * ppn + temp_nodes[node_id]] = i;
        ++temp_nodes[node_id];
    }
    ucg_planc_ucx_free_ptr(&temp_nodes);
    return UCG_OK;
}

ucg_plan_meta_op_t* ucg_planc_ucx_allgatherv_na_rolling_op_new(ucg_planc_ucx_group_t* ucx_group,
                                                               ucg_vgroup_t* vgroup,
                                                               const ucg_coll_args_t* args)
{
    UCG_CHECK_NULL(NULL, ucx_group, vgroup, args);

    ucg_plan_meta_op_t* meta_op = ucg_plan_meta_op_new(vgroup->group, vgroup, args);

    if (meta_op == NULL) {
        goto err;
    }

    ucg_status_t status;
    ucg_coll_args_t *meta_args = &meta_op->super.super.args;

    int32_t *global_ranks = ucg_malloc(sizeof(int32_t) * vgroup->size, "Global_ranks");
    if (global_ranks == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err;
    }

    int32_t *temp_displs = ucg_malloc(sizeof(int32_t) * vgroup->size, "Temporary displs");
    if (temp_displs == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err_free_global_ranks;
    }

    int32_t *temp_recvcounts = ucg_malloc(sizeof(int32_t) * vgroup->size, "Temporary recvcounts");
    if (temp_recvcounts == NULL) {
        status = UCG_ERR_NO_MEMORY;
        goto err_free_displs;
    }
    const int32_t *store_displs = meta_args->allgatherv.displs;
    const int32_t *store_recvcounts = meta_args->allgatherv.recvcounts;
    if (!vgroup->group->topo->detail.nrank_continuous) {
        status = ucg_planc_ucx_allgatherv_na_rolling_get_global_ranks(vgroup, global_ranks);
        for (uint32_t i = 0; i < vgroup->size; ++i) {
            temp_displs[i] = meta_args->allgatherv.displs[global_ranks[i]];
            temp_recvcounts[i] = meta_args->allgatherv.recvcounts[global_ranks[i]];
        }
    } else {
        for (uint32_t i = 0; i < vgroup->size; ++i) {
            temp_displs[i] = meta_args->allgatherv.displs[i];
            temp_recvcounts[i] = meta_args->allgatherv.recvcounts[i];
        }
    }

    meta_args->allgatherv.displs = temp_displs;
    meta_args->allgatherv.recvcounts = temp_recvcounts;
    status = ucg_planc_ucx_create_only_node_leader_algo_group(ucx_group, vgroup);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    status = ucg_planc_ucx_allgatherv_add_rolling_inter_op(meta_op, ucx_group, vgroup, meta_args, global_ranks,
                                                           UCG_ALGO_GROUP_TYPE_NODE_LEADER);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    status = ucg_planc_ucx_allgatherv_add_rolling_intra_op(meta_op, ucx_group, global_ranks,
                                                           vgroup, meta_args,
                                                           UCG_TOPO_GROUP_TYPE_NODE);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    meta_args->allgatherv.displs = store_displs;
    meta_args->allgatherv.recvcounts = store_recvcounts;
    ucg_planc_ucx_free_ptr(&global_ranks);

    return meta_op;

err_free_meta_op:
    meta_op->super.discard(&meta_op->super);
    ucg_planc_ucx_free_ptr(&temp_recvcounts);
err_free_displs:
    ucg_planc_ucx_free_ptr(&temp_displs);
err_free_global_ranks:
    ucg_planc_ucx_free_ptr(&global_ranks);
err:
    return NULL;
}

ucg_status_t ucg_planc_ucx_allgatherv_na_rolling_prepare(ucg_vgroup_t *vgroup,
                                                         const ucg_coll_args_t *args,
                                                         ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);
    ucg_status_t status;
    status = ucg_planc_ucx_allgatherv_na_rolling_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_plan_meta_op_t* meta_op;
    meta_op = ucg_planc_ucx_allgatherv_na_rolling_op_new(ucx_group, vgroup, args);
    if (meta_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    *op = &meta_op->super;
    return UCG_OK;
}