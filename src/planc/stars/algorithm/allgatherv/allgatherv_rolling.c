/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "allgatherv.h"

#include "planc_stars_algo.h"

#define ROLLING_EID_IDX  0
#define ROLLING_EID_NUM  1
#define ROLLING_PEER_NUM 2

static size_t UCG_STARS_ALGO_FUN(allgatherv_rolling, rbuf_size)(ucg_coll_allgatherv_args_t *args,
                                                                uint32_t size)
{
    int max_offset = 0, count = 0;
    for (int i = 0; i < size; i++) {
        if (max_offset < args->displs[i]) {
            max_offset = args->displs[i];
            count = max_offset + args->recvcounts[i];
        }
    }
    return count * args->recvtype->size;
}

static ucg_status_t ucg_planc_stars_allgatherv_rolling_ofd_put_req_elem(ucg_planc_stars_op_t *op,
                                                                        stars_rank_info_h target_rank)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    ucg_rank_t myrank = vgroup->myrank;
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_algo_ring_iter_t *iter = &op->allgatherv.ring_iter;
    stars_comm_plan_t *plan = &op->plan;
    int32_t step_idx = ucg_algo_ring_iter_idx(iter);
    ucg_rank_t peer = ((myrank + step_idx) & 1) != 0 ? iter->left : iter->right;
    uint32_t block_idx;
    int64_t send_offset;
    stars_rank_info_h peer_rank;

    if (peer == iter->right) {
        block_idx = (myrank - step_idx / ROLLING_PEER_NUM + group_size) % group_size;
        send_offset = args->displs[block_idx] * recvtype_extent;
        peer_rank = &plan->comm_dep.put_ranks[1];
    } else {
        block_idx = (myrank + step_idx / ROLLING_PEER_NUM + group_size) % group_size;
        send_offset = args->displs[block_idx] * recvtype_extent;
        peer_rank = &plan->comm_dep.put_ranks[0];
    }

    ucg_debug("put step_idx %d my %d peer %d iter {left %d, right %d} block_idx %d",
              step_idx, myrank, peer, iter->left, iter->right, block_idx);
    /* handle empty package */
    if (!args->recvcounts[block_idx]) {
        return UCG_OK;
    }

    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    request->lbuf               = (void *)plan->lrbuf_desc->addr + send_offset;
    request->llen               = args->recvcounts[block_idx] * args->recvtype->size;
    request->rbuf               = (void *)peer_rank->rbuf_desc->addr + send_offset;
    request->lmemh              = plan->lrmemh;
    request->scp_event          = NULL;
    request->type               = OFFLOAD_PUT;

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_put_req_elem(ROLLING_EID_IDX, target_rank, request);
    UCG_ASSERT_CODE_GOTO(status, put_request);

    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return status;

put_request:
    ucg_mpool_put(request);
    return status;
}

static ucg_status_t ucg_planc_stars_allgatherv_rolling_ofd_wait_req_elem(ucg_planc_stars_op_t *op,
                                                                         stars_rank_info_h peer)
{
    ucg_algo_ring_iter_t *iter = &op->allgatherv.ring_iter;
    int32_t step_idx = ucg_algo_ring_iter_idx(iter);
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    ucg_rank_t myrank = vgroup->myrank;

    uint32_t block_idx;
    if (peer->peer_id == iter->right) {
        block_idx = (peer->peer_id + step_idx / ROLLING_PEER_NUM + group_size) % group_size;
    } else {
        block_idx = (peer->peer_id - step_idx / ROLLING_PEER_NUM + group_size) % group_size;
    }

    ucg_debug("wait step_idx %d my %d peer %d iter {left %d, right %d} block_idx %d",
              step_idx, myrank, peer->peer_id, iter->left, iter->right, block_idx);

    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    /* handle empty package */
    if (!args->recvcounts[block_idx]) {
        return UCG_OK;
    }

    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_wait_req_elem(ROLLING_EID_IDX, peer, request, op->plan.event_elem);
    UCG_ASSERT_CODE_GOTO(status, put_elem);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
put_elem:
    ucg_mpool_put(request);
    return status;
}

static ucg_status_t ucg_planc_stars_allgatherv_rolling_put_self_data(ucg_planc_stars_op_t *op)
{
    ucg_status_t status = UCG_OK;
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_rank_t myrank = op->super.vgroup->myrank;
    stars_rank_info_h peer_rank;
    if (args->sendbuf == UCG_IN_PLACE || args->recvcounts[myrank] == 0) {
        return UCG_OK;
    }
    // Put req.
    scp_ofd_req_elem_h put_request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(put_request != NULL, UCG_ERR_NO_MEMORY);
    peer_rank = &plan->comm_dep.put_ranks[2];
    int64_t offset = args->displs[myrank] * ucg_dt_extent(args->recvtype);

    put_request->lbuf               = (void*)args->sendbuf;
    put_request->llen               = args->sendcount * ucg_dt_extent(args->sendtype);
    put_request->rbuf               = (void *)peer_rank->rbuf_desc->addr + offset;
    put_request->lmemh              = plan->lsmemh;
    put_request->scp_event          = NULL;
    put_request->type               = OFFLOAD_PUT;
    status = ucg_planc_stars_fill_ofd_put_req_elem(ROLLING_EID_IDX, peer_rank, put_request);
    UCG_ASSERT_CODE_GOTO(status, free_put_request);
    ucg_planc_stars_op_push_ofd_req_elem(op, put_request);
    // Wait req.
    scp_ofd_req_elem_h wait_request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(wait_request != NULL, UCG_ERR_NO_MEMORY);
    peer_rank = &plan->comm_dep.get_ranks[2];
    status = ucg_planc_stars_fill_ofd_wait_req_elem(ROLLING_EID_IDX, peer_rank, wait_request, op->plan.event_elem);
    UCG_ASSERT_CODE_GOTO(status, free_wait_request);
    ucg_planc_stars_op_push_ofd_req_elem(op, wait_request);

    return UCG_OK;
free_wait_request:
    ucg_mpool_put(wait_request);
free_put_request:
    ucg_mpool_put(put_request);
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, submit_op)(ucg_planc_stars_op_t *op)
{
    ucg_status_t status;
    stars_comm_plan_t *plan = &op->plan;
    ucg_rank_t myrank = op->super.vgroup->myrank;
    stars_rank_info_h put_rank, get_rank;
    int step_idx, peer_idx;

    ucg_algo_ring_iter_t *iter = &op->allgatherv.ring_iter;
    // If not in place, put data to self.
    status = ucg_planc_stars_allgatherv_rolling_put_self_data(op);
    UCG_CHECK_GOTO(status, out);

    while (!ucg_algo_ring_iter_end(iter)) {
        step_idx = ucg_algo_ring_iter_idx(iter);
        peer_idx = (myrank + step_idx + 1) & 1;

        put_rank = &plan->comm_dep.put_ranks[peer_idx];
        get_rank = &plan->comm_dep.get_ranks[peer_idx];

        /* two rank can't put first when recv use same eventid, otherwise maybe drop event */
        if (myrank > put_rank->peer_id) {
            status = ucg_planc_stars_allgatherv_rolling_ofd_put_req_elem(op, put_rank);
            UCG_CHECK_GOTO(status, out);
            status = ucg_planc_stars_allgatherv_rolling_ofd_wait_req_elem(op, get_rank);
            UCG_CHECK_GOTO(status, out);
        } else {
            status = ucg_planc_stars_allgatherv_rolling_ofd_wait_req_elem(op, get_rank);
            UCG_CHECK_GOTO(status, out);
            status = ucg_planc_stars_allgatherv_rolling_ofd_put_req_elem(op, put_rank);
            UCG_CHECK_GOTO(status, out);
        }
        ucg_algo_ring_iter_inc(iter);
    }

    return scp_submit_ofd_req(op->stars_group->context->scp_worker,
                              &op->stars_group->stars_stream,
                              &op->ofd_req, op->stats.cur_elem);
out:
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, init_rbuf)(ucg_planc_stars_op_t *op,
                                                                      ucg_coll_args_t *coll_args)
{
    ucg_coll_allgatherv_args_t *args = &coll_args->allgatherv;
    size_t length =
        UCG_STARS_ALGO_FUN(allgatherv_rolling, rbuf_size)(args, op->super.vgroup->size);
    ucg_status_t status = ucg_planc_stars_rbuf_init(op, args->recvbuf, length);
    UCG_ASSERT_CODE_RET(status);

    stars_comm_plan_t *plan = &op->plan;
    for (uint32_t idx = 0; idx < plan->comm_dep.get_rank_num; ++idx) {
        plan->comm_dep.get_ranks[idx].rbuf_desc = plan->lrbuf_desc;
    }

    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, init_sbuf)(ucg_planc_stars_op_t *op,
                                                                      ucg_coll_args_t *coll_args)
{
    // we copy the data of sendbuf to recvbuf and we use recvbuf to communicate, thus we don't need init sendbuf.
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_status_t myrank = op->super.vgroup->myrank;
    if (args->sendbuf == UCG_IN_PLACE || args->recvcounts[myrank] == 0) {
        return UCG_OK;
    }
    return ucg_planc_stars_sbuf_init(op, (void *)args->sendbuf, args->sendcount * ucg_dt_extent(args->sendtype));
}

static inline size_t UCG_STARS_ALGO_FUN(allgatherv_rolling, put_max_size)(ucg_planc_stars_op_t *op,
                                                                          ucg_coll_args_t *coll_args)
{
    return 0;
}

/**
 * @brief Rolling algorithm for allgatherv with O(log(N)) steps.
 *
 * Example on 7 processes:
 *   Initial set up:
 *    #     0      1      2      3      4      5      6
 *         [0]    [ ]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [1]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [2]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [3]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [4]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [ ]    [5]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [ ]    [ ]    [6]
 *   Step 0: send message to (rank + 1 - 2 * rank%2), receive message from (rank + 1 - 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [ ]    [ ]    [ ]    [ ]    [0]
 *         [1]    [1]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [2]    [2]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [3]    [3]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [4]    [4]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [5]    [5]    [ ]
 *         [6]    [ ]    [ ]    [ ]    [ ]    [ ]    [6]
 *   Step 1: send message to (rank - 1 + 2 * rank%2), receive message from (rank - 1 + 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [ ]    [ ]    [ ]    [ ]    [0]
 *         [1]    [1]    [1]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [2]    [2]    [2]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [3]    [3]    [3]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [4]    [4]    [4]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [5]    [5]    [5]
 *         [6]    [ ]    [ ]    [ ]    [ ]    [6]    [6]
 *   Step 2: send message to (rank + 1 - 2 * rank%2), receive message from (rank + 1 - 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [ ]    [ ]    [ ]    [ ]    [0]
 *         [1]    [1]    [1]    [1]    [ ]    [ ]    [1]
 *         [2]    [2]    [2]    [2]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [3]    [3]    [3]    [3]    [ ]
 *         [ ]    [ ]    [4]    [4]    [4]    [4]    [ ]
 *         [5]    [ ]    [ ]    [ ]    [5]    [5]    [5]
 *         [6]    [6]    [ ]    [ ]    [6]    [6]    [6]
 *   Step 3: send message to (rank - 1 + 2 * rank%2), receive message from (rank - 1 + 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [0]    [ ]    [ ]    [0]    [0]
 *         [1]    [1]    [1]    [1]    [ ]    [ ]    [1]
 *         [2]    [2]    [2]    [2]    [2]    [ ]    [ ]
 *         [ ]    [3]    [3]    [3]    [3]    [3]    [ ]
 *         [ ]    [ ]    [4]    [4]    [4]    [4]    [4]
 *         [5]    [ ]    [ ]    [5]    [5]    [5]    [5]
 *         [6]    [6]    [ ]    [ ]    [6]    [6]    [6]
 *   Step 4: send message to (rank + 1 - 2 * rank%2), receive message from (rank + 1 - 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [0]    [0]    [ ]    [0]    [0]
 *         [1]    [1]    [1]    [1]    [1]    [ ]    [1]
 *         [2]    [2]    [2]    [2]    [2]    [2]    [2]
 *         [3]    [3]    [3]    [3]    [3]    [3]    [ ]
 *         [4]    [ ]    [4]    [4]    [4]    [4]    [4]
 *         [5]    [5]    [ ]    [5]    [5]    [5]    [5]
 *         [6]    [6]    [6]    [ ]    [6]    [6]    [6]
 *   Step 5: send message to (rank - 1 + 2 * rank%2), receive message from (rank - 1 + 2 * rank%2)
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [0]    [0]    [0]    [0]    [0]
 *         [1]    [1]    [1]    [1]    [1]    [1]    [1]
 *         [2]    [2]    [2]    [2]    [2]    [2]    [2]
 *         [3]    [3]    [3]    [3]    [3]    [3]    [3]
 *         [4]    [4]    [4]    [4]    [4]    [4]    [4]
 *         [5]    [5]    [5]    [5]    [5]    [5]    [5]
 *         [6]    [6]    [6]    [6]    [6]    [6]    [6]
 */

static ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, offload_plan)(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    ucg_planc_stars_context_t *context = op->stars_group->context;
    ucg_rank_t myid = vgroup->myrank;
    ucg_rank_t numprocs = vgroup->size;
    ucg_status_t status;

    comm_dep->get_ranks = NULL;
    comm_dep->put_ranks = NULL;
    comm_dep->get_rank_num = ROLLING_PEER_NUM + 1;
    comm_dep->put_rank_num = ROLLING_PEER_NUM + 1;

    status = ucg_planc_stars_rank_dep_alloc(comm_dep);
    UCG_ASSERT_CODE_RET(status);

    ucg_rank_t get_peer_id[3], put_peer_id[3];
    put_peer_id[0] = (myid + numprocs - 1) % numprocs;
    put_peer_id[1] = (myid + 1) % numprocs;
    put_peer_id[2] = myid;
    get_peer_id[0] = (myid + numprocs - 1) % numprocs;
    get_peer_id[1] = (myid + 1) % numprocs;
    get_peer_id[2] = myid;

    for (uint32_t cnt = 0; cnt < comm_dep->get_rank_num; ++cnt) {
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->get_ranks[cnt],
                                               get_peer_id[cnt], 1);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }

    for (uint32_t cnt = 0; cnt < comm_dep->put_rank_num; ++cnt) {
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->put_ranks[cnt],
                                               put_peer_id[cnt], 1);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }

    return UCG_OK;

err_free_memory:
    if (comm_dep->get_ranks) {
        ucg_free(comm_dep->get_ranks);
        comm_dep->get_ranks = NULL;
    }
    if (comm_dep->put_ranks) {
        ucg_free(comm_dep->put_ranks);
        comm_dep->put_ranks = NULL;
    }
    return status;
}

static inline ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, check)(ucg_vgroup_t *vgroup,
                                                                         const ucg_coll_args_t *coll_args) {
    return UCG_OK;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(allgatherv_rolling, init)(ucg_plan_op_t *ucg_op)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_coll_args_t *coll_args = &ucg_op->super.args;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_status_t status;

    ucg_algo_ring_iter_init(&op->allgatherv.ring_iter,
                            vgroup->size, vgroup->myrank);

    status = ucg_planc_stars_algo_prepare_plan(op, coll_args,
                                               UCG_STARS_ALGO_FUN(allgatherv_rolling, put_max_size),
                                               UCG_STARS_ALGO_FUN(allgatherv_rolling, offload_plan));
    UCG_CHECK_GOTO_ERR(status, out, "prepare allgatherv rolling plan");

    status = ucg_planc_stars_algo_exch_buf_addr(op, coll_args,
                                                UCG_STARS_ALGO_FUN(allgatherv_rolling, init_rbuf),
                                                UCG_STARS_ALGO_FUN(allgatherv_rolling, init_sbuf));
    UCG_CHECK_GOTO_ERR(status, out, "exchange buffer address");

    op->stats.nb_type = (coll_args->type == UCG_COLL_TYPE_IALLGATHERV) ? 0 : 1;
    return UCG_OK;
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(allgatherv_rolling, trigger)(ucg_plan_op_t *ucg_op)
{
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_planc_stars_op_reset(op);
    ucg_algo_ring_iter_reset(&op->allgatherv.ring_iter);

    ucg_status_t status;
    op->super.super.status = UCG_ERR_IO_ERROR;
    status = ucg_planc_stars_plan_get_stats_elem(op);
    UCG_ASSERT_CODE_RET(status);

    UCG_STATS_GET_TIME(submit_tick);
    status = UCG_STARS_ALGO_FUN(allgatherv_rolling, submit_op)(op);
    UCG_ASSERT_CODE_RET(status);
    op->super.super.status = UCG_INPROGRESS;

    UCG_STATS_GET_TIME(end_tick);
    UCG_STATS_SET_TIME(op->stats.cur_elem->start_tick, start_tick);
    UCG_STATS_COST_TIME(submit_tick, end_tick,
                        op->stats.cur_elem->trigger.submit.total);
    UCG_STATS_COST_TIME(start_tick, end_tick,
                        op->stats.cur_elem->trigger.total);
    return status;
}

inline static ucg_status_t UCG_STARS_ALGO_FUN(allgatherv_rolling, progress)(ucg_plan_op_t *ucg_op)
{
    return ucg_planc_stars_algo_progress(ucg_op, NULL);
}

static ucg_status_t
UCG_STARS_ALGO_FUN(allgatherv_rolling, discard)(ucg_plan_op_t *ucg_op)
{
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    UCG_STATS_GET_TIME(buf_cleanup);
    ucg_planc_stars_buf_cleanup(op, STARS_BUFF_RECV);
    UCG_STATS_GET_TIME(op_cleanup);
    ucg_planc_stars_op_discard(ucg_op);
    UCG_STATS_GET_TIME(op_destruct);
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    UCG_STATS_GET_TIME(mpool_put);
    UCG_STATS_GET_TIME(end_tick);

    UCG_STATS_COST_TIME(buf_cleanup, op_cleanup,
                        op->stats.discard.buf_cleanup.total);
    UCG_STATS_COST_TIME(op_cleanup, op_destruct,
                        op->stats.discard.op_cleanup);
    UCG_STATS_COST_TIME(op_destruct, mpool_put,
                        op->stats.discard.op_destruct);
    UCG_STATS_COST_TIME(mpool_put, end_tick,
                        op->stats.discard.mpool_put);
    UCG_STATS_COST_TIME(start_tick, end_tick, op->stats.discard.total);
    UCG_STATS_SET_TIME(op->stats.end_tick, end_tick);
    UCG_STATS_COST_TIME(op->stats.start_tick, end_tick, op->stats.cost_time);

    ucg_stats_dump("allgatherv_rolling", &op->stats);
    return UCG_OK;
}
UCG_STARS_ALGO_PRE_DEFINE(allgatherv_rolling)