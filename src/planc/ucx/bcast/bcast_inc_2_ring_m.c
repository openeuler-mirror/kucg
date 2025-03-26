/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 */

#include "bcast.h"
#include "planc_ucx_plan.h"

#define UCG_BCAST_RING_M_RANK_OFFSET2 2
#define UCG_BCAST_RING_M_RING1_SIZE3 3
#define UCG_BCAST_RING_M_RING2_SIZE6 6
enum {
    UCG_BCAST_INC_2_RING_M_SEND = UCG_BIT(0), /* send to my right */
    UCG_BCAST_INC_2_RING_M_RECV = UCG_BIT(1), /* receive from my left */
};

static ucg_status_t ucg_planc_ucx_bcast_inc_2_ring_m_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_coll_bcast_args_t *args = &ucg_op->super.args.bcast;
    ucg_algo_ring_iter_t *iter = &op->bcast.ring_iter;
    ucg_rank_t myrank = op->super.vgroup->myrank;
    ucg_rank_t root = args->root;
    uint32_t group_size = vgroup->size;

    ucg_rank_t recv_from_root_rank1 = (root + UCG_BCAST_RING_M_RANK_OFFSET2) % group_size;
    /* ring 1: if group size is 2, root proc doesn't need to send to rank 2*/
    if (group_size < UCG_BCAST_RING_M_RING1_SIZE3) {
        recv_from_root_rank1 = UCG_INVALID_RANK;
    }
    /* ring 2: if group size < 6(mid_rank <= 2), root proc doesn't need to send to mid_rank*/
    ucg_rank_t recv_from_root_rank2 = ((group_size >> 1) + root) % group_size;
    if (group_size < UCG_BCAST_RING_M_RING2_SIZE6) {
        recv_from_root_rank2 = UCG_INVALID_RANK;
    }

    ucg_rank_t peer;
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_INC_2_RING_M_RECV)) {
        peer = ucg_algo_ring_iter_left_value(iter);
        if (myrank == recv_from_root_rank1 || myrank == recv_from_root_rank2) {
            peer = root;
        }
        status = ucg_planc_ucx_p2p_irecv(args->buffer, args->count,
                                         args->dt, peer, op->tag,
                                         vgroup, &params);
        UCG_CHECK_GOTO(status, out);
    }
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
    UCG_CHECK_GOTO(status, out);

    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_INC_2_RING_M_SEND)) {
        peer = ucg_algo_ring_iter_right_value(iter);
        status = ucg_planc_ucx_p2p_isend(args->buffer, args->count,
                                         args->dt, peer, op->tag,
                                         vgroup, &params);
        UCG_CHECK_GOTO(status, out);

        /* root proc still need to send to rank 2(ring 1) and rank mid_proc(ring 2) */
        if (myrank == root) {
            if ((peer = recv_from_root_rank1) != UCG_INVALID_RANK) {
                status = ucg_planc_ucx_p2p_isend(args->buffer, args->count,
                                             args->dt, peer, op->tag,
                                             vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }

            if ((peer = recv_from_root_rank2) != UCG_INVALID_RANK) {
                status = ucg_planc_ucx_p2p_isend(args->buffer, args->count,
                                             args->dt, peer, op->tag,
                                             vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }
        }
    }
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_inc_2_ring_m_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);
    ucg_algo_ring_iter_t *iter = &op->bcast.ring_iter;
    ucg_algo_ring_iter_reset(iter);

    ucg_coll_bcast_args_t *args = &ucg_op->super.args.bcast;
    ucg_rank_t right_peer = ucg_algo_ring_iter_right_value(iter);
    ucg_rank_t left_peer = ucg_algo_ring_iter_left_value(iter);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    ucg_rank_t root = args->root;

    ucg_rank_t mid_rank = ((group_size >> 1) + root) % group_size;
    if (myrank == args->root) {
        op->flags = UCG_BCAST_INC_2_RING_M_SEND;
    } else if (right_peer == root || right_peer == mid_rank || left_peer == root){
        op->flags = UCG_BCAST_INC_2_RING_M_RECV;
    } else {
        op->flags = UCG_BCAST_INC_2_RING_M_RECV | UCG_BCAST_INC_2_RING_M_SEND;
    }
    status = ucg_planc_ucx_bcast_inc_2_ring_m_op_progress(ucg_op);
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_bcast_inc_2_ring_m_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

ucg_status_t ucg_planc_ucx_bcast_inc_2_ring_m_prepare(ucg_vgroup_t *vgroup,
                                                      const ucg_coll_args_t *args,
                                                      ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);
    ucg_status_t status;

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_bcast_inc_2_ring_m_op_trigger,
                                 ucg_planc_ucx_bcast_inc_2_ring_m_op_progress,
                                 ucg_planc_ucx_bcast_inc_2_ring_m_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(ucx_op, ucx_group);
    ucg_algo_ring_iter_init(&ucx_op->bcast.ring_iter,
                            vgroup->size,
                            vgroup->myrank);
    *op = &ucx_op->super;
    return UCG_OK;

err_free_op:
    ucg_mpool_put(ucx_op);
    return status;
}