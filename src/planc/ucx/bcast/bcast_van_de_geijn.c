/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include "bcast.h"
#include "core/ucg_request.h"
#include "planc_ucx_context.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_p2p.h"
#include "core/ucg_dt.h"
#include "core/ucg_group.h"
#include "ucg/api/ucg.h"
#include "util/algo/ucg_kntree.h"
#include "util/algo/ucg_ring.h"
#include "util/ucg_log.h"
#include "util/ucg_malloc.h"
#include "util/ucg_parser.h"

enum {
    UCG_BCAST_VAN_DE_GEIJN_SCATTERV_PHASE   = UCG_BIT(0), /* van de Geijn algorithm scatterv phase */
    UCG_BCAST_VAN_DE_GEIJN_ALLGATHERV_PHASE = UCG_BIT(1), /* van de Geijn algorithm phase */
    UCG_BCAST_VAN_DE_GEIJN_SEND             = UCG_BIT(2), /* van de Geijn algorithm send operation */
    UCG_BCAST_VAN_DE_GEIJN_RECV             = UCG_BIT(3), /* van de Geijn algorithm recv operation */
    UCG_BCAST_VAN_DE_GEIJN_SENDRECV         = UCG_BIT(4), /* van de Geijn algorithm sendrecv operation */
};

enum {
    UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_ONE_BY_ONE,
    UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_BATCH,
};

#define UCG_BCAST_VAN_DE_GEIJN_FLAGS UCG_BCAST_VAN_DE_GEIJN_SCATTERV_PHASE | \
                                     UCG_BCAST_VAN_DE_GEIJN_ALLGATHERV_PHASE | \
                                     UCG_BCAST_VAN_DE_GEIJN_SENDRECV | \
                                     UCG_BCAST_VAN_DE_GEIJN_SEND | \
                                     UCG_BCAST_VAN_DE_GEIJN_RECV

/*
 * van de Geijn algorithm for broadcast:
 *     scatter(v) + allgather(v)
 *
 *   Data layout (e.g num_procs=4, count=6, root=3)
 *  Initial phase:
 *    #     0      1      2      3
 *         [ ]    [ ]    [ ]    [0]
 *         [ ]    [ ]    [ ]    [1]
 *         [ ]    [ ]    [ ]    [2]
 *         [ ]    [ ]    [ ]    [3]
 *         [ ]    [ ]    [ ]    [4]
 *         [ ]    [ ]    [ ]    [5]
 *  After scatter(v) phase:
 *    #     0      1      2      3
 *         [0]    [ ]    [ ]    [0]
 *         [1]    [ ]    [ ]    [1]
 *         [ ]    [2]    [ ]    [2]
 *         [ ]    [ ]    [3]    [3]
 *         [ ]    [ ]    [ ]    [4]
 *         [ ]    [ ]    [ ]    [5]
 *  After allgather(v) phase:
 *    #     0      1      2      3
 *         [0]    [0]    [0]    [0]
 *         [1]    [1]    [1]    [1]
 *         [2]    [2]    [2]    [2]
 *         [3]    [3]    [3]    [3]
 *         [4]    [4]    [4]    [4]
 *         [5]    [5]    [5]    [5]
 */

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_scatterv_send_one_by_one(ucg_planc_ucx_op_t *op) {
    ucg_status_t status = UCG_OK;
    ucg_algo_kntree_iter_t *iter = &op->bcast.kntree_iter;
    ucg_rank_t peer;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    int32_t curr_count, send_count;
    int32_t quotient = op->bcast.van_de_geijn.quotient;
    ucg_rank_t my_vrank = iter->myrank;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    void *buffer = args->buffer;
    ucg_dt_t *datatype = args->dt;
    int64_t extent = ucg_dt_extent(datatype);
    uint64_t send_offset;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);

    /* send to children one by one */
    if (ucg_test_flags(op->flags, UCG_BCAST_VAN_DE_GEIJN_SEND)) {
        curr_count = op->bcast.van_de_geijn.curr_count;
        while ((peer = ucg_algo_kntree_iter_child_value(iter)) != UCG_INVALID_RANK) {
            ucg_rank_t peer_vrank = (peer - iter->root + group_size) % group_size;
            send_count = curr_count - quotient * (peer_vrank - my_vrank);
            if (send_count > 0) {
                send_offset = peer_vrank * quotient * extent;
                status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count, args->dt,
                                                 peer, op->tag, vgroup, &params);
                UCG_CHECK_GOTO(status, out);
                curr_count -= send_count;
                op->bcast.van_de_geijn.curr_count = curr_count;
            }
            status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
            UCG_CHECK_GOTO(status, out);
            ucg_algo_kntree_iter_child_inc(iter);
        }
        ucg_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_SEND);
    }

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_scatterv_send_batch(ucg_planc_ucx_op_t *op) {
    ucg_status_t status = UCG_OK;
    ucg_algo_kntree_iter_t *iter = &op->bcast.kntree_iter;
    ucg_rank_t peer;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    int32_t curr_count, send_count;
    int32_t quotient = op->bcast.van_de_geijn.quotient;
    ucg_rank_t my_vrank = iter->myrank;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    void *buffer = args->buffer;
    ucg_dt_t *datatype = args->dt;
    int64_t extent = ucg_dt_extent(datatype);
    uint64_t send_offset;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);

    /* send to children batch */
    if (ucg_test_flags(op->flags, UCG_BCAST_VAN_DE_GEIJN_SEND)) {
        curr_count = op->bcast.van_de_geijn.curr_count;
        while ((peer = ucg_algo_kntree_iter_child_value(iter)) != UCG_INVALID_RANK) {
            ucg_rank_t peer_vrank = (peer - iter->root + group_size) % group_size;
            send_count = curr_count - quotient * (peer_vrank - my_vrank);
            if (send_count > 0) {
                send_offset = peer_vrank * quotient * extent;
                status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count, args->dt,
                                                 peer, op->tag, vgroup, &params);
                UCG_CHECK_GOTO(status, out);
                curr_count -= send_count;
                op->bcast.van_de_geijn.curr_count = curr_count;
            }
            ucg_algo_kntree_iter_child_inc(iter);
        }
        ucg_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_SEND);
    }
    status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_scatterv_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &ucg_op->super.args.bcast;
    void *buffer = args->buffer;
    int32_t count = args->count;
    ucg_dt_t *datatype = args->dt;
    int64_t extent = ucg_dt_extent(datatype);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_rank_t peer;
    uint32_t group_size = vgroup->size;
    int32_t recv_count;
    ucg_algo_kntree_iter_t *iter = &op->bcast.kntree_iter;
    ucg_rank_t my_vrank = iter->myrank;
    uint64_t recv_offset;
    int32_t quotient = op->bcast.van_de_geijn.quotient;

    /* receive from parent */
    peer = ucg_algo_kntree_iter_parent_value(iter);
    if (peer != UCG_INVALID_RANK && ucg_test_and_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_RECV)) {
        ucg_rank_t peer_vrank = (peer - iter->root + group_size) % group_size;
        ucg_rank_t right_sibling_rank = peer_vrank + (my_vrank - peer_vrank) * 2;
        if (right_sibling_rank < group_size) {
            int32_t right_sibling_count = right_sibling_rank * quotient;
            if (right_sibling_count > count) {
                right_sibling_count = count;
            }
            recv_count = right_sibling_count - my_vrank * quotient;
        } else {
            recv_count = count - my_vrank * quotient;
        }
        recv_offset = my_vrank * quotient * extent;
        if (recv_count <= 0) {
            op->bcast.van_de_geijn.curr_count = 0;
        } else {
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count, args->dt,
                                             peer, op->tag, vgroup, &params);
            UCG_CHECK_GOTO(status, out);
            op->bcast.van_de_geijn.curr_count = recv_count;
        }
    }
    status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
    UCG_CHECK_GOTO(status, out);

    /* send to my children */
    status = (op->bcast.van_de_geijn.send_type == UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_ONE_BY_ONE) ?
             ucg_planc_ucx_bcast_van_de_geijn_scatterv_send_one_by_one(op) :
             ucg_planc_ucx_bcast_van_de_geijn_scatterv_send_batch(op);

out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_allgatherv_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_group_t *ucx_group = op->ucx_group;
    ucg_coll_bcast_args_t *args = &ucg_op->super.args.bcast;
    void *buffer = args->buffer;
    int32_t count = args->count;
    ucg_dt_t *datatype = args->dt;
    int64_t extent = ucg_dt_extent(datatype);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    ucg_algo_ring_iter_t *iter = &op->bcast.van_de_geijn.ring_iter;
    ucg_rank_t left_peer = ucg_algo_ring_iter_left_value(iter);
    ucg_rank_t right_peer = ucg_algo_ring_iter_right_value(iter);
    ucg_rank_t my_vrank = op->bcast.kntree_iter.myrank;
    uint32_t group_size = vgroup->size;
    int32_t send_count, recv_count;
    int64_t send_offset, recv_offset;
    int32_t quotient = op->bcast.van_de_geijn.quotient;

    /* sendrecv : only post the request of operations */
    while (!ucg_algo_ring_iter_end(iter)) {
        int step_idx = ucg_algo_ring_iter_idx(iter);

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_SEND)) {
            int send_block = (my_vrank - step_idx + group_size) % group_size;
            send_count = (quotient < count - send_block * quotient) ?
                          quotient : count - send_block * quotient;
            if (send_count < 0)
                send_count = 0;
            send_offset = send_block * quotient * extent;
            status = ucg_planc_ucx_p2p_isend(buffer + send_offset, send_count, datatype, right_peer, op->tag, vgroup, &params);
            UCG_CHECK_GOTO(status, out);
        }

        if (ucg_test_and_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_RECV)) {
            int recv_block = (my_vrank - step_idx - 1 + group_size) % group_size;
            recv_count = (quotient < count - recv_block * quotient) ?
                          quotient : count - recv_block * quotient;
            if (recv_count < 0)
                recv_count = 0;
            recv_offset = recv_block * quotient * extent;
            status = ucg_planc_ucx_p2p_irecv(buffer + recv_offset, recv_count, datatype, left_peer, op->tag, vgroup, &params);
            UCG_CHECK_GOTO(status, out);
        }

        status = ucg_planc_ucx_p2p_testall(ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);

        op->flags = op->flags | UCG_BCAST_VAN_DE_GEIJN_RECV | UCG_BCAST_VAN_DE_GEIJN_SEND;
        ucg_algo_ring_iter_inc(iter);
    }
out:
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    if (ucg_test_flags(op->flags, UCG_BCAST_VAN_DE_GEIJN_SCATTERV_PHASE)) {
        //TODO: support more algorithms for scatterv phase
        status = ucg_planc_ucx_bcast_van_de_geijn_scatterv_op_progress(ucg_op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_SCATTERV_PHASE);
        op->flags = op->flags | UCG_BCAST_VAN_DE_GEIJN_SEND | UCG_BCAST_VAN_DE_GEIJN_RECV;
    }

    if (ucg_test_flags(op->flags, UCG_BCAST_VAN_DE_GEIJN_ALLGATHERV_PHASE)) {
        //TODO: support more algorithms for allgatherv phase
        status = ucg_planc_ucx_bcast_van_de_geijn_allgatherv_op_progress(ucg_op);
        UCG_CHECK_GOTO(status, out);
        ucg_clear_flags(&op->flags, UCG_BCAST_VAN_DE_GEIJN_ALLGATHERV_PHASE);
    }
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_rank_t my_rank = ucg_op->vgroup->myrank;
    ucg_rank_t root = ucg_op->super.args.bcast.root;
    uint32_t count = ucg_op->super.args.bcast.count;
    uint32_t group_size = ucg_op->vgroup->size;

    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);
    ucg_algo_kntree_iter_reset(&op->bcast.van_de_geijn.kntree_iter);
    ucg_algo_ring_iter_reset(&op->bcast.van_de_geijn.ring_iter);
    op->bcast.van_de_geijn.quotient = (count + group_size - 1) / group_size;
    op->bcast.van_de_geijn.curr_count = (my_rank == root) ? count : 0;
    op->flags = UCG_BCAST_VAN_DE_GEIJN_FLAGS;

    status = ucg_planc_ucx_bcast_van_de_geijn_op_progress(ucg_op);
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_op_discard(ucg_plan_op_t *ucg_op)
{
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_mpool_put(ucg_op);
    return UCG_OK;
}

static ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_check(ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args)
{
    uint32_t group_size = vgroup->size;
    int32_t count = args->bcast.count;
    if (count < group_size) {
        ucg_info("Bcast van_de_geijn don't support count < group_size");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

static void ucg_planc_ucx_bcast_van_de_geijn_bsend_threshold(const ucg_planc_ucx_bcast_config_t *config,
                                                             ucg_vgroup_t *vgroup,
                                                             size_t *min, size_t *max)
{
    int32_t ppn = vgroup->group->topo->ppn;
    int32_t nnode = vgroup->group->topo->detail.nnode;

    *min = config->min_bsend;
    *max = config->min_bsend;

    if (config->min_bsend == UCG_MEMUNITS_INF) {
        *max = UCG_MEMUNITS_INF;
    }

    if (config->min_bsend == UCG_MEMUNITS_AUTO) {
        if (nnode == 1) {
            *min = 128;
        } else if (ppn == 1 && nnode <= 2) {
            *min = 256;
        } else if (ppn == 1 && nnode <= 4) {
            *min = 0;
        } else if (ppn == 1) {
            *min = 128;
        } else if (ppn > 1) {
            if (nnode <= 4 && ppn <= 16) {
                *min = 128;
            } else if (nnode <= 4) {
                *min = 64;
            } else if (nnode <= 8 && ppn <= 16) {
                *min = 256;
            } else if (nnode <= 8 && ppn <= 32) {
                *min = 1024;
            } else if (nnode <= 8) {
                *min = 64;
            } else if (nnode <= 16 && ppn <= 8) {
                *min = 131072;
            } else if (nnode <= 16) {
                *min = 0;
            } else {
                *min = 64;
            }
        } else {
            *min = UCG_MEMUNITS_INF;
        }
    }

    if (config->max_bsend == UCG_MEMUNITS_AUTO) {
        if (nnode == 1) {
            *max = SIZE_MAX;
        } else if (ppn == 1) {
            *max = 131072;
        } else if (ppn > 1) {
            if (nnode <= 4 && ppn <= 8) {
                *max = 2048;
            } else if (nnode <=4) {
                *max = 32768;
            } else if (nnode <= 8 && ppn <= 4) {
                *max = 32768;
            } else if (nnode <= 8 && ppn <= 8) {
                *max = 131072;
            } else if (nnode <= 8 && ppn <= 32) {
                *max = 16384;
            } else if (nnode <= 8) {
                *max = 65536;
            } else if (nnode <= 16 && ppn <= 2) {
                *max = 1048576;
            } else if (nnode <= 16 && ppn <= 8) {
                *max = 0;
            } else if (nnode <= 16 && ppn <= 32) {
                *max = 32768;
            } else {
                *max = 524288;
            }
        } else {
            *max = 0;
        }
    }
}

static void ucg_planc_ucx_bcast_van_de_geijn_send_type_init (ucg_planc_ucx_op_t *op,
                                                             ucg_vgroup_t *vgroup,
                                                             const ucg_planc_ucx_bcast_config_t *config)
{
    size_t min_bsend;
    size_t max_bsend;
    uint8_t send_type;
    ucg_planc_ucx_bcast_van_de_geijn_bsend_threshold(config, vgroup,
                                                     &min_bsend, &max_bsend);
    uint32_t count = op->super.super.args.bcast.count;
    if (count < min_bsend || count > max_bsend) {
        send_type = UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_ONE_BY_ONE;
    } else {
        send_type = UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_BATCH;
    }
    op->bcast.van_de_geijn.send_type = send_type;
    ucg_info("bcast van de geijn scatterv send type: %s",
             send_type == UCG_BCAST_VAN_DE_GEIJN_SCATTERV_SEND_TYPE_ONE_BY_ONE ?
             "one by one" : "batch");
}


ucg_status_t ucg_planc_ucx_bcast_van_de_geijn_prepare(ucg_vgroup_t *vgroup,
                                                      const ucg_coll_args_t *args,
                                                      ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_status_t status;
    status = ucg_planc_ucx_bcast_van_de_geijn_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    ucg_planc_ucx_bcast_config_t *config;
    config = UCG_PLANC_UCX_CONTEXT_BUILTIN_CONFIG_BUNDLE(ucx_group->context, bcast,
                                                         UCG_COLL_TYPE_BCAST);

    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_bcast_van_de_geijn_op_trigger,
                                 ucg_planc_ucx_bcast_van_de_geijn_op_progress,
                                 ucg_planc_ucx_bcast_van_de_geijn_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_algo_kntree_iter_t *kntree_iter = &ucx_op->bcast.van_de_geijn.kntree_iter;
    ucg_algo_kntree_iter_init(kntree_iter, vgroup->size, 2,
                              args->bcast.root, vgroup->myrank, 1);
    ucg_algo_ring_iter_t *ring_iter = &ucx_op->bcast.van_de_geijn.ring_iter;
    ucg_algo_ring_iter_init(ring_iter, vgroup->size, vgroup->myrank);
    ucg_planc_ucx_op_init(ucx_op, ucx_group);
    ucg_planc_ucx_bcast_van_de_geijn_send_type_init(ucx_op, vgroup, config);
    *op = &ucx_op->super;
    return UCG_OK;

err_free_op:
    ucg_mpool_put(ucx_op);
    return status;
}