/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 */

#include "bcast.h"
#include "planc_ucx_plan.h"

 enum {
    UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE = UCG_BIT(0), // rank 0 send to near phase
    UCG_BCAST_LONG_M_SCATTER_PHASE      = UCG_BIT(1), // scatter phase
    UCG_BCAST_LONG_M_ALLGATHER_PHASE    = UCG_BIT(2), // allgather phase
    UCG_BCAST_LONG_M_SEND               = UCG_BIT(3), // send operation flag
    UCG_BCAST_LONG_M_RECV               = UCG_BIT(4), // recv operation flag
    UCG_BCAST_LONG_M_SENDRECV           = UCG_BIT(5), // sendrecv operation flag
};

#define UCG_BCAST_LONG_M_RANK_OFFSET_1 1

#define UCG_BCAST_LONG_M_FLAGS UCG_BCAST_LONG_M_SCATTER_PHASE | \
                               UCG_BCAST_LONG_M_ALLGATHER_PHASE | \
                               UCG_BCAST_LONG_M_SENDRECV | \
                               UCG_BCAST_LONG_M_SEND | \
                               UCG_BCAST_LONG_M_RECV

#define UCG_BCAST_LONG_M_ROOT_FLAGS UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE | \
                                    UCG_BCAST_LONG_M_SCATTER_PHASE | \
                                    UCG_BCAST_LONG_M_ALLGATHER_PHASE | \
                                    UCG_BCAST_LONG_M_SENDRECV | \
                                    UCG_BCAST_LONG_M_SEND

#define UCG_BCAST_LONG_M_ROOT_SIZE2_FLAGS UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE | \
                                    UCG_BCAST_LONG_M_SENDRECV | \
                                    UCG_BCAST_LONG_M_SEND

#define UCG_BCAST_LONG_M_RANK_NEAR_FLAGS UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE | \
                                         UCG_BCAST_LONG_M_SENDRECV | \
                                         UCG_BCAST_LONG_M_RECV

static ucg_rank_t convert_real_rank_to_kntree_rank(int size, ucg_rank_t root, ucg_rank_t real_rank)
{
    if (real_rank == root) {
        return 0;
    }
    return (real_rank - root + size) % size - 1;
}

static ucg_rank_t convert_kntree_rank_to_real_rank(int kntree_size, ucg_rank_t root, ucg_rank_t kntree_rank)
{
    if (kntree_rank == 0) {
        return root;
    }
    return (kntree_rank + 1 + root) % (kntree_size + 1);
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_allgather_op_progress(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    void *buffer = args->buffer;
    ucg_dt_t *datatype = args->dt;
    uint32_t extent = ucg_dt_extent(datatype);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    uint32_t kntree_size = group_size - UCG_BCAST_LONG_M_RANK_OFFSET_1;
    int8_t group_size_is_odd = kntree_size & 1;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_rank_t kntree_rank = op->bcast.kntree_iter.myrank;
    ucg_rank_t root = op->bcast.kntree_iter.root;
    // the param `peer_zero` only applies when kntree_size is odd and kntree_rank is zero
    ucg_rank_t peer, peer_zero = UCG_INVALID_RANK;
    uint32_t division = op->bcast._long.division;
    uint32_t quotient = op->bcast._long.quotient;
    uint32_t init_block = kntree_rank;
    int32_t send_block, recv_block;
    // the two params `*_block_zero` only applies when kntree_size is odd and kntree_rank is zero
    int32_t send_block_zero, recv_block_zero;
    int32_t send_count, recv_count;
    int64_t send_offset, recv_offset;

    while (op->bcast._long.step_idx < kntree_size) {
        int8_t step_idx_is_odd = op->bcast._long.step_idx & 1;
        if (group_size_is_odd && !step_idx_is_odd && kntree_rank == 0) {
            goto skip;
        }

        int32_t block_offset = (op->bcast._long.step_idx + 1) >> 1;
        if ((kntree_rank & 1) ^ step_idx_is_odd) {
            recv_block = (init_block + block_offset) % kntree_size;
            peer = (kntree_rank + 1 + root) % kntree_size;
            send_block = (init_block + 1 - block_offset + kntree_size) % kntree_size;
        } else {
            recv_block = (init_block - block_offset + kntree_size) % kntree_size;
            peer = (kntree_rank - 1 + kntree_size + root) % kntree_size;
            send_block = (init_block - 1 + block_offset + kntree_size) % kntree_size;
        }

        // if kntree_size is odd && kntree_rank == 0 && step_idx is odd : 1 <--> 0 <--> (kntree_size-1)
        if (group_size_is_odd && kntree_rank == 0) {
            recv_block_zero = (kntree_size - block_offset) % kntree_size;
            peer_zero = (kntree_size - 1 + root) % kntree_size;
            send_block_zero = (kntree_size - 1 + block_offset) % kntree_size;
        }

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_SEND)) {
            send_count = (send_block < division) ? quotient + 1 : quotient;
            send_offset = (send_block < division) ?
                          (int64_t)send_block * (quotient + 1) * extent :
                          (int64_t)(division + send_block * quotient) * extent;
            ucg_rank_t real_peer = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer);
            status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                             datatype, real_peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);

            // if kntree_size is odd && kntree_rank == 0 && step_idx is odd : 1 <--> 0 <--> (kntree_size-1)
            if (peer_zero != UCG_INVALID_RANK) {
                send_count = (send_block_zero < division) ? quotient + 1 : quotient;
                send_offset = (send_block_zero < division) ?
                              (int64_t)send_block_zero * (quotient + 1) * extent :
                              (int64_t)(division + send_block_zero * quotient) * extent;
                ucg_rank_t real_peer_zero = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer_zero);
                status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                                 datatype, real_peer_zero, op->tag,
                                                 vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }
        }

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_RECV)) {
            recv_count = (recv_block < division) ? quotient + 1 : quotient;
            recv_offset = (recv_block < division) ?
                          (int64_t)recv_block * (quotient + 1) * extent :
                          (int64_t)(division + recv_block * quotient) * extent;
            ucg_rank_t real_peer = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer);
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                             datatype, real_peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            if (peer_zero != UCG_INVALID_RANK) {
                recv_count = (recv_block_zero < division) ? quotient + 1 : quotient;
                recv_offset = (recv_block_zero < division) ?
                              (int64_t)recv_block_zero * (quotient + 1) * extent :
                              (int64_t)(division + recv_block_zero * quotient) * extent;
                ucg_rank_t real_peer_zero = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer_zero);
                status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                                 datatype, real_peer_zero,
                                                 op->tag, vgroup, &params);
                UCG_CHECK_GOTO(status, out);
            }
        }
        status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);

skip:
        op->flags = op->flags | UCG_BCAST_LONG_M_RECV | UCG_BCAST_LONG_M_SEND;
        peer_zero = UCG_INVALID_RANK;
        ++op->bcast._long.step_idx;
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_scatter_op_recv(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status = UCG_OK;
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
    ucg_rank_t kntree_rank = iter->myrank;
    ucg_rank_t root = iter->root;
    ucg_rank_t peer = ucg_algo_kntree_iter_parent_value(iter);
    uint32_t group_size = vgroup->size;
    uint32_t kntree_size = group_size - 1;
    int32_t recv_blocks;
    int32_t recv_count;
    int64_t recv_offset;

    // receive from parent
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_RECV)) {
        ucg_rank_t peer_vrank = (peer - root + kntree_size) % kntree_size;
        ucg_rank_t right_sibling_rank = peer_vrank + (kntree_rank - peer_vrank) * 2;
        if (right_sibling_rank < kntree_size) {
            int32_t right_sibling_block = right_sibling_rank;
            if (right_sibling_block > kntree_size) {
                right_sibling_block = kntree_size;
            }
            recv_blocks = right_sibling_block - kntree_rank;
        } else {
            recv_blocks = kntree_size - kntree_rank;
        }

        if (kntree_rank >= division) {
            recv_count = recv_blocks * quotient;
            recv_offset = (int64_t)(division + kntree_rank * quotient) * extent;
        } else {
            int32_t decrease = (division - kntree_rank >= recv_blocks) ?
                               0 :
                               kntree_rank + recv_blocks - division;
            recv_count = recv_blocks * (quotient + 1) - decrease;
            recv_offset = (int64_t)kntree_rank * (quotient + 1) * extent;
        }

        if (recv_blocks <= 0) {
            op->bcast._long.curr_blocks = 0;
        } else {
            // Convert the virtual peer rank to the real peer rank.
            ucg_rank_t real_peer = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer);
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count,
                                             datatype, real_peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            op->bcast._long.curr_blocks = recv_blocks;
        }
    }
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_scatter_op_send(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status = UCG_OK;
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
    ucg_rank_t kntree_rank = iter->myrank;
    ucg_rank_t root = iter->root;
    ucg_rank_t peer = ucg_algo_kntree_iter_parent_value(iter);
    uint32_t group_size = vgroup->size;
    uint32_t kntree_size = group_size - 1;
    int32_t send_blocks;
    int32_t send_count;
    int64_t send_offset;

    // send to my children
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_SEND)) {
        while ((peer = ucg_algo_kntree_iter_child_value(iter)) != UCG_INVALID_RANK) {
            ucg_rank_t peer_vrank = (peer - root + kntree_size) % kntree_size;
            send_blocks = op->bcast._long.curr_blocks - peer_vrank + kntree_rank;
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
            // Convert the virtual peer rank to the real peer rank.
            ucg_rank_t real_peer = convert_kntree_rank_to_real_rank(kntree_size, args->root, peer);
            status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count,
                                             args->dt, real_peer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            op->bcast._long.curr_blocks -= send_blocks;
            ucg_algo_kntree_iter_child_inc(iter);
        }
    }
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_scatter_op_progress(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_algo_kntree_iter_t *iter = &op->bcast._long.kntree_iter;
    ucg_rank_t peer = ucg_algo_kntree_iter_parent_value(iter);

    if (peer != UCG_INVALID_RANK) {
        // myrank is root, don't need to recv
        status = ucg_planc_ucx_bcast_long_m_scatter_op_recv(op);
        UCG_CHECK_GOTO(status, out);
        status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);
    }

    status = ucg_planc_ucx_bcast_long_m_scatter_op_send(op);
    UCG_CHECK_GOTO(status, out);
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_root_to_near_op_progress(ucg_planc_ucx_op_t *op)
{
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    uint32_t group_size = vgroup->size;
    ucg_rank_t root = args->root;
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_SEND)) {
        // Root send to the near rank
        ucg_rank_t peer = (root + UCG_BCAST_LONG_M_RANK_OFFSET_1) % group_size;
        status = ucg_planc_ucx_p2p_isend(args->buffer, args->count,
                                         args->dt, peer, op->tag,
                                         vgroup, &params);
        UCG_CHECK_GOTO(status, out);
    }
    if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_LONG_M_RECV)) {
        // Recv from root
        status = ucg_planc_ucx_p2p_irecv(args->buffer, args->count,
                                         args->dt, root, op->tag,
                                         vgroup, &params);
        UCG_CHECK_GOTO(status, out);
    }

    if (vgroup->myrank != root || vgroup->size <= 2) {
        status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
    }
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);

    if (ucg_test_flags(op->flags, UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE)) {
        status = ucg_planc_ucx_bcast_long_m_root_to_near_op_progress(op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_LONG_M_ROOT_TO_NEAR_PHASE);
        op->flags = op->flags | UCG_BCAST_LONG_M_SEND | UCG_BCAST_LONG_M_RECV;
    }

    if (ucg_test_flags(op->flags, UCG_BCAST_LONG_M_SCATTER_PHASE)) {
        status = ucg_planc_ucx_bcast_long_m_scatter_op_progress(op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_LONG_M_SCATTER_PHASE);
        op->flags = op->flags | UCG_BCAST_LONG_M_SEND | UCG_BCAST_LONG_M_RECV;
    }

    if (ucg_test_flags(op->flags, UCG_BCAST_LONG_M_ALLGATHER_PHASE)) {
        status = ucg_planc_ucx_bcast_long_m_allgather_op_progress(op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_LONG_M_ALLGATHER_PHASE);
    }

out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_rank_t myrank = ucg_op->vgroup->myrank;
    ucg_rank_t root = ucg_op->super.args.bcast.root;
    int32_t count = ucg_op->super.args.bcast.count;
    uint32_t group_size = ucg_op->vgroup->size;
    uint32_t kntree_size = group_size - 1;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);

    op->bcast._long.step_idx = 1;
    op->bcast._long.quotient = count / kntree_size;
    op->bcast._long.curr_blocks = (myrank == root) ? kntree_size : 0;
    op->bcast._long.division = count % kntree_size; // quotient+1 || quotient
    if (myrank != root && myrank != (root + UCG_BCAST_LONG_M_RANK_OFFSET_1) % group_size) {
        // Other ranks need to go scatter phase and allgather phase
        op->flags = UCG_BCAST_LONG_M_FLAGS;
        ucg_algo_kntree_iter_reset(&op->bcast.kntree_iter);
    } else if (myrank == root) {
        // Root needs to send data to the next rank first.
        op->flags = group_size > 2 ?
                    UCG_BCAST_LONG_M_ROOT_FLAGS :
                    UCG_BCAST_LONG_M_ROOT_SIZE2_FLAGS;
        if (group_size > 2) {
            ucg_algo_kntree_iter_reset(&op->bcast.kntree_iter);
        }
    } else {
        // Rank next to root only do step1(recv all the data)
        op->flags = UCG_BCAST_LONG_M_RANK_NEAR_FLAGS;
    }

    status = ucg_planc_ucx_bcast_long_m_op_progress(ucg_op);
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

static ucg_planc_ucx_op_t *ucg_planc_ucx_bcast_long_m_op_new(ucg_planc_ucx_group_t *ucx_group,
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
                                 ucg_planc_ucx_bcast_long_m_op_trigger,
                                 ucg_planc_ucx_bcast_long_m_op_progress,
                                 ucg_planc_ucx_bcast_long_m_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }

    ucg_planc_ucx_op_init(op, ucx_group);
    // Rank1 doesn't need to join kntree
    if ((vgroup->myrank == (args->bcast.root + UCG_BCAST_LONG_M_RANK_OFFSET_1) % vgroup->size)
        || vgroup->size <= 2) {
        goto out;
    }
    /*
     * Create a virtual group to build a kntree, in which root is the rank 0, and the rank
     * next to the root is skipped.
     * For example, a 4-ranks group: 0, 1, 2, 3, rank 2 is the root, so rank 3 will be skipped,
     * then create a virtual group whose size is 3, in the virtual group, rank 2 -> rank 0,
     * rank 3 skipped, rank 0 -> rank 1, rank 1 -> rank 2.
     */
    ucg_algo_kntree_iter_t *kntree_iter = &op->bcast._long.kntree_iter;
    ucg_rank_t kntree_rank = convert_real_rank_to_kntree_rank(vgroup->size,
                                                              args->bcast.root,
                                                              vgroup->myrank);
    int kntree_size = vgroup->size - 1;
    ucg_algo_kntree_iter_init(kntree_iter, kntree_size, 2,
                              0, kntree_rank, 1);
out:
    return op;
err_free_op:
    ucg_mpool_put(op);
err:
    return NULL;
}

static ucg_status_t ucg_planc_ucx_bcast_long_m_check(ucg_vgroup_t *vgroup,
                                                     const ucg_coll_args_t *args)
{
    uint32_t group_size = vgroup->size;
    int32_t count = args->bcast.count;
    if (count < group_size) {
        ucg_info("Bcast long modified don't support count < group_size");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

ucg_status_t ucg_planc_ucx_bcast_long_m_prepare(ucg_vgroup_t *vgroup,
                                                const ucg_coll_args_t *args,
                                                ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_status_t status;
    status = ucg_planc_ucx_bcast_long_m_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_planc_ucx_bcast_long_m_op_new(ucx_group, vgroup, args);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    *op = &ucx_op->super;
    return UCG_OK;

}