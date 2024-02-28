/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 */

#include "bcast.h"
#include "planc_ucx_plan.h"

enum {
    UCG_BCAST_LONG_SCATTERV_PHASE   = UCG_BIT(0), /* long algorithm scatterv phase */
    UCG_BCAST_LONG_ALLGATHERV_PHASE = UCG_BIT(1), /* long algorithm phase */
    UCG_BCAST_LONG_SEND             = UCG_BIT(2), /* long algorithm send operation */
    UCG_BCAST_LONG_RECV             = UCG_BIT(3), /* long algorithm recv operation */
    UCG_BCAST_LONG_SENDRECV         = UCG_BIT(4), /* long algorithm sendrecv operation */
};

#define UCG_BCAST_LONG_FLAGS UCG_BCAST_LONG_SCATTERV_PHASE | \
                             UCG_BCAST_LONG_ALLGATHERV_PHASE | \
                             UCG_BCAST_LONG_SENDRECV | \
                             UCG_BCAST_LONG_SEND | \
                             UCG_BCAST_LONG_RECV

static ucg_status_t ucg_planc_ucx_bcast_long_allgatherv_op_progress(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    void *buffer = args->buffer;
    ucg_dt_t *datatype = args->dt;
    uint32_t extent = ucg_dt_extent(datatype);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    int8_t group_size_is_odd = group_size & 1;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_rank_t my_vrank = op->bcast.kntree_iter.myrank;
    ucg_rank_t root = op->bcast.kntree_iter.root;
    /* the param `peer_zero` only applies when group_size is odd and my_vrank is zero */
    ucg_rank_t peer, peer_zero = UCG_INVALID_RANK;
    uint32_t division = op->bcast._long.division;
    uint32_t quotient = op->bcast._long.quotient;
    uint32_t init_block = my_vrank;
    int32_t send_block, recv_block;
    /* the two params `*_block_zero` only applies when group_size is odd and my_vrank is zero */
    int32_t send_block_zero, recv_block_zero;
    int32_t send_count, recv_count;
    int64_t send_offset, recv_offset;
    while (op->bcast._long.step_idx < group_size) {
        int8_t step_idx_is_odd = op->bcast._long.step_idx & 1;
        if (group_size_is_odd && !step_idx_is_odd && my_vrank == 0) {
            goto skip;
        }

        int32_t block_offset = (op->bcast._long.step_idx + 1) >> 1;
        if ((my_vrank & 1) ^ step_idx_is_odd) {
            recv_block = (init_block + block_offset) % group_size;
            peer = (my_vrank + 1 + root) % group_size;
            send_block = (init_block + 1 - block_offset + group_size) % group_size;
        } else {
            recv_block = (init_block - block_offset + group_size) % group_size;
            peer = (my_vrank - 1 + group_size + root) % group_size;
            send_block = (init_block - 1 + block_offset + group_size) % group_size;
        }

        /* if group_size is odd && my_vrank == 0 && step_idx is odd : 1 <--> 0 <--> (group_size-1) */
        if (group_size_is_odd && my_vrank == 0) {
            recv_block_zero = (group_size - block_offset) % group_size;
            peer_zero = (group_size - 1 + root) % group_size;
            send_block_zero = (group_size - 1 + block_offset) % group_size;
        }

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_SEND)) {
            send_count = (send_block < division) ? quotient + 1 : quotient;
            send_offset = (send_block < division) ?
                          (int64_t)send_block * (quotient + 1) * extent :
                          (int64_t)(division + send_block * quotient) * extent;

            status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                             datatype, peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);

            /* if group_size is odd && my_vrank == 0 && step_idx is odd : 1 <--> 0 <--> (group_size-1) */
            if (peer_zero != UCG_INVALID_RANK) {
                send_count = (send_block_zero < division) ? quotient + 1 : quotient;
                send_offset = (send_block_zero < division) ?
                              (int64_t)send_block_zero * (quotient + 1) * extent :
                              (int64_t)(division + send_block_zero * quotient) * extent;
                status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                                 datatype, peer_zero, op->tag,
                                                 vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }
        }

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_RECV)) {
            recv_count = (recv_block < division) ? quotient + 1 : quotient;
            recv_offset = (recv_block < division) ?
                          (int64_t)recv_block * (quotient + 1) * extent :
                          (int64_t)(division + recv_block * quotient) * extent;
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                             datatype, peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            if (peer_zero != UCG_INVALID_RANK) {
                recv_count = (recv_block_zero < division) ? quotient + 1 : quotient;
                recv_offset = (recv_block_zero < division) ?
                              (int64_t)recv_block_zero * (quotient + 1) * extent :
                              (int64_t)(division + recv_block_zero * quotient) * extent;
                status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                                 datatype, peer_zero,
                                                 op->tag, vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }
        }
        status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);

skip:
        op->flags = op->flags | UCG_BCAST_LONG_RECV | UCG_BCAST_LONG_SEND;
        peer_zero = UCG_INVALID_RANK;
        ++op->bcast._long.step_idx;
    }

out:
    return status;
}


static ucg_status_t ucg_planc_ucx_bcast_long_scatterv_op_progress(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    void *buffer = args->buffer;
    ucg_dt_t *datatype = args->dt;
    uint32_t extent = ucg_dt_extent(datatype);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    int32_t division = op->bcast._long.division;
    int32_t quotient = op->bcast._long.quotient;
    ucg_algo_kntree_iter_t *iter = &op->bcast._long.kntree_iter;
    ucg_rank_t my_vrank = iter->myrank;
    ucg_rank_t root = iter->root;
    ucg_rank_t peer = ucg_algo_kntree_iter_parent_value(iter);
    uint32_t group_size = vgroup->size;
    int32_t send_blocks, recv_blocks;
    int32_t send_count, recv_count;
    int64_t send_offset, recv_offset;

    if (peer == UCG_INVALID_RANK) {
        /* myrank is root, just need to send */
        goto send;
    }

    /* receive from parent */
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_RECV)) {
        ucg_rank_t peer_vrank = (peer - root + group_size) % group_size;
        ucg_rank_t right_sibling_rank = peer_vrank + (my_vrank - peer_vrank) * 2;
        if (right_sibling_rank < group_size) {
            int32_t right_sibling_block = right_sibling_rank;
            if (right_sibling_block > group_size) {
                right_sibling_block = group_size;
            }
            recv_blocks = right_sibling_block - my_vrank;
        } else {
            recv_blocks = group_size - my_vrank;
        }

        if (my_vrank >= division) {
            recv_count = recv_blocks * quotient;
            recv_offset = (int64_t)(division + my_vrank * quotient) * extent;
        } else {
            int32_t decrease = (division - my_vrank >= recv_blocks) ?
                               0 :
                               my_vrank + recv_blocks - division;
            recv_count = recv_blocks * (quotient + 1) - decrease;
            recv_offset = (int64_t)my_vrank * (quotient + 1) * extent;
        }

        if (recv_blocks <= 0) {
            op->bcast._long.curr_blocks = 0;
        } else {
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                             datatype, peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            op->bcast._long.curr_blocks = recv_blocks;
        }
    }
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
    UCG_CHECK_GOTO(status, out);

send:
    /* send to my children */
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_SEND)) {
        while ((peer = ucg_algo_kntree_iter_child_value(iter)) != UCG_INVALID_RANK) {
            ucg_rank_t peer_vrank = (peer - root + group_size) % group_size;
            send_blocks = op->bcast._long.curr_blocks - peer_vrank + my_vrank;
            if (send_blocks <= 0) {
                continue;
            }
            if (peer_vrank >= division) {
                send_count = send_blocks * quotient;
                send_offset = (int64_t)(division + peer_vrank * quotient) * extent;
            } else {
                int32_t decrease = (division - peer_vrank >= send_blocks) ?
                                   0 :
                                   peer_vrank + send_blocks - division;
                send_count = send_blocks * (quotient + 1) - decrease;
                send_offset = (int64_t)peer_vrank * (quotient + 1) * extent;
            }
            status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                             args->dt, peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            op->bcast._long.curr_blocks -= send_blocks;
            ucg_algo_kntree_iter_child_inc(iter);
        }
    }
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    if (ucg_test_flags(op->flags, UCG_BCAST_LONG_SCATTERV_PHASE)) {
        status = ucg_planc_ucx_bcast_long_scatterv_op_progress(op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_LONG_SCATTERV_PHASE);
        op->flags = op->flags | UCG_BCAST_LONG_SEND | UCG_BCAST_LONG_RECV;
    }

    if (ucg_test_flags(op->flags, UCG_BCAST_LONG_ALLGATHERV_PHASE)) {
        status = ucg_planc_ucx_bcast_long_allgatherv_op_progress(op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_LONG_ALLGATHERV_PHASE);
    }
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_rank_t myrank = ucg_op->vgroup->myrank;
    ucg_rank_t root = ucg_op->super.args.bcast.root;
    int32_t count = ucg_op->super.args.bcast.count;
    uint32_t group_size = ucg_op->vgroup->size;

    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);
    ucg_algo_kntree_iter_reset(&op->bcast._long.kntree_iter);

    op->bcast._long.step_idx = 1;
    op->bcast._long.quotient = count / group_size;
    op->bcast._long.curr_blocks = (myrank == root) ? group_size : 0;
    op->bcast._long.division = count % group_size; // quotient+1 || quotient
    op->flags = UCG_BCAST_LONG_FLAGS;

    status = ucg_planc_ucx_bcast_long_op_progress(ucg_op);
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

ucg_planc_ucx_op_t *ucg_planc_ucx_bcast_long_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                    ucg_vgroup_t *vgroup,
                                                    const ucg_coll_args_t *args)
{
    UCG_CHECK_NULL(NULL, ucx_group, vgroup, args);
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (op == NULL) {
        ucg_error("Failed to create super of ucx op due to out of memory in memory pool");
        goto err;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &op->super, vgroup,
                                 ucg_planc_ucx_bcast_long_op_trigger,
                                 ucg_planc_ucx_bcast_long_op_progress,
                                 ucg_planc_ucx_bcast_long_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(op, ucx_group);
    ucg_algo_kntree_iter_t *kntree_iter = &op->bcast._long.kntree_iter;
    ucg_algo_kntree_iter_init(kntree_iter, vgroup->size, 2,
                              args->bcast.root, vgroup->myrank, 1);
    return op;
err_free_op:
    ucg_mpool_put(op);
err:
    return NULL;
}

static ucg_status_t ucg_planc_ucx_bcast_long_check(ucg_vgroup_t *vgroup,
                                                   const ucg_coll_args_t *args)
{
    uint32_t group_size = vgroup->size;
    int32_t count = args->bcast.count;
    if (count < group_size) {
        ucg_info("Bcast long don't support count < group_size");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

ucg_status_t ucg_planc_ucx_bcast_long_prepare(ucg_vgroup_t *vgroup,
                                              const ucg_coll_args_t *args,
                                              ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_status_t status;
    status = ucg_planc_ucx_bcast_long_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_planc_ucx_bcast_long_op_new(ucx_group, vgroup, args);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    *op = &ucx_op->super;
    return UCG_OK;
}