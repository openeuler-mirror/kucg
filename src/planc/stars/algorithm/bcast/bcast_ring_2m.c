/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "bcast.h"
#include "planc_stars_algo.h"

#define UCG_BCAST_RING_2_M_RANK_OFFSET_1 1
#define UCG_BCAST_RING_2_M_RANK_OFFSET_2 2
#define RING_2M_EID_IDX 0

static ucg_status_t ucg_planc_stars_bcast_ring_2m_ofd_put_req(ucg_planc_stars_op_t *op,
                                                              stars_rank_info_h peer_rank)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    stars_buf_desc_h sbuf_desc  = op->plan.lsbuf_desc;
    request->lbuf        = (void *)sbuf_desc->addr;
    request->llen        = sbuf_desc->length;
    request->rbuf        = (void *)peer_rank->rbuf_desc->addr;
    request->lmemh       = op->plan.lsmemh;
    request->type        = OFFLOAD_PUT;

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_put_req_elem(RING_2M_EID_IDX, peer_rank, request);
    UCG_ASSERT_CODE_RET(status);

    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_bcast_ring_2m_ofd_wait_req(ucg_planc_stars_op_t *op, stars_rank_info_h peer_rank)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_wait_req_elem(RING_2M_EID_IDX, peer_rank, request, op->plan.event_elem);
    UCG_ASSERT_CODE_GOTO(status, free);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
free:
    ucg_mpool_put(request);
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, submit_op)(ucg_planc_stars_op_t *op)
{
    stars_comm_plan_t *plan = &op->plan;
    ucg_status_t status;
    stars_rank_info_h peer_rank;

    for (uint32_t idx = 0; idx < plan->comm_dep.get_rank_num; ++idx) {
        peer_rank = &plan->comm_dep.get_ranks[idx];
        status = ucg_planc_stars_bcast_ring_2m_ofd_wait_req(op, peer_rank);
        UCG_CHECK_GOTO_ERR(status, out, "wait notify");
    }

    for (uint32_t idx = 0; idx < plan->comm_dep.put_rank_num; ++idx) {
        peer_rank = &plan->comm_dep.put_ranks[idx];
        status = ucg_planc_stars_bcast_ring_2m_ofd_put_req(op, peer_rank);
        UCG_CHECK_GOTO_ERR(status, out, "construct stars put req");
    }

    return scp_submit_ofd_req(op->stars_group->context->scp_worker,
                              &op->stars_group->stars_stream,
                              &op->ofd_req, op->stats.cur_elem);

out:
	return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, init_rbuf)(ucg_planc_stars_op_t *op,
                                                                 ucg_coll_args_t *coll_args)
{
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_bcast_args_t *args = &coll_args->bcast;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    if (vgroup->myrank == args->root) {
        /* for bcast coll, root rank don't need recv data */
        return UCG_OK;
    }

    ucg_status_t status =
        ucg_planc_stars_rbuf_init(op, args->buffer,
                                  args->count * args->dt->size);
    UCG_ASSERT_CODE_RET(status);

    for (uint32_t idx = 0; idx < plan->comm_dep.get_rank_num; ++idx) {
        plan->comm_dep.get_ranks[idx].rbuf_desc = plan->lrbuf_desc;
    }

    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, init_sbuf)(ucg_planc_stars_op_t *op,
                                                                 ucg_coll_args_t *coll_args)
{
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_bcast_args_t *args = &coll_args->bcast;

    if (ucg_unlikely(op->super.vgroup->myrank != args->root)) {
        /* send buffer and receive buffer are same in bcast ring_m */
        plan->lsbuf_desc    = plan->lrbuf_desc;
        plan->lsmemh        = plan->lrmemh;
        return UCG_OK;
    }

    return ucg_planc_stars_sbuf_init(op, (void *)args->buffer,
                                     plan->sbuf_size);
}

static inline size_t UCG_STARS_ALGO_FUN(bcast_ring_2m, put_max_size)(ucg_planc_stars_op_t *op,
                                                                     ucg_coll_args_t *coll_args)
{
    ucg_coll_bcast_args_t *args = &coll_args->bcast;
    return args->dt->size * args->count;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, offload_plan)(ucg_planc_stars_op_t *op)
{
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t numprocs = vgroup->size;
    ucg_rank_t myid = vgroup->myrank;
    ucg_rank_t root = args->root;
    ucg_rank_t rank_1 = (root + UCG_BCAST_RING_2_M_RANK_OFFSET_2) % numprocs;
    ucg_rank_t rank_2 = (root + (numprocs >> 1)) % numprocs;

    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    comm_dep->get_ranks = NULL;
    comm_dep->get_rank_num = (myid == root) ? 0 : 1;
    comm_dep->put_ranks = NULL;

    if ((myid == (root - 1 + numprocs) % numprocs) ||
        (myid == (root + 1) % numprocs) ||
        (myid == (rank_2 - 1 + numprocs) % numprocs)) {
        comm_dep->put_rank_num = 0;
    } else if (myid == root) {
        if (rank_1 != rank_2) {
            comm_dep->put_rank_num = 3;
        } else {
            comm_dep->put_rank_num = 2;
        }
    } else {
        comm_dep->put_rank_num = 1;
    }

    ucg_status_t status;
    status = ucg_planc_stars_rank_dep_alloc(comm_dep);
    UCG_ASSERT_CODE_RET(status);

    ucg_rank_t peer_id;
    ucg_planc_stars_context_t *context = op->stars_group->context;
    for (uint32_t cnt = 0; cnt < comm_dep->get_rank_num; ++cnt) {
        peer_id = (myid == rank_1) || (myid == rank_2) ? root : (myid + numprocs - 1) % numprocs;
        status =
            ucg_planc_stars_rank_dep_init(op, &comm_dep->get_ranks[cnt],
                                          peer_id, 1);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }

    for (uint32_t cnt = 0; cnt < comm_dep->put_rank_num; ++cnt) {
        if (myid == root) {
            peer_id = cnt < 1 ? rank_2 : (myid + cnt) % numprocs;
        } else {
            peer_id = (myid + 1) % numprocs;
        }
        status =
            ucg_planc_stars_rank_dep_init(op, &comm_dep->put_ranks[cnt],
                                          peer_id, 1);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }

    return UCG_OK;

err_free_memory:
    return status;
}

static inline ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, check)(ucg_vgroup_t *vgroup,
                                                                    const ucg_coll_args_t *coll_args)
{
    if (ucg_unlikely(vgroup->size <= 3)) {
        ucg_info("ring-2-Modified don't support the number of progresses less than 4");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}


static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, init)(ucg_plan_op_t *ucg_op)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_coll_args_t *coll_args = &ucg_op->super.args;
    ucg_status_t status;

    status = ucg_planc_stars_algo_prepare_plan(op, coll_args,
                                               UCG_STARS_ALGO_FUN(bcast_ring_2m, put_max_size),
                                               UCG_STARS_ALGO_FUN(bcast_ring_2m, offload_plan));
    UCG_CHECK_GOTO_ERR(status, out, "prepare bcast ring 2m plan");

    status = ucg_planc_stars_algo_exch_buf_addr(op, coll_args,
                                                UCG_STARS_ALGO_FUN(bcast_ring_2m, init_rbuf),
                                                UCG_STARS_ALGO_FUN(bcast_ring_2m, init_sbuf));
    UCG_CHECK_GOTO_ERR(status, out, "exchange buffer address");

    op->stats.nb_type = (coll_args->type == UCG_COLL_TYPE_IBCAST) ? 0 : 1;
    return UCG_OK;
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, trigger)(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_planc_stars_op_reset(op);

    op->super.super.status = UCG_ERR_IO_ERROR;
    status = ucg_planc_stars_plan_get_stats_elem(op);
    UCG_ASSERT_CODE_RET(status);
    UCG_STATS_GET_TIME(submit_tick);
    status = UCG_STARS_ALGO_FUN(bcast_ring_2m, submit_op)(op);
    UCG_ASSERT_CODE_RET(status);

    op->super.super.status = UCG_INPROGRESS;

    UCG_STATS_GET_TIME(end_tick);
    UCG_STATS_SET_TIME(op->stats.cur_elem->start_tick, start_tick);
    UCG_STATS_COST_TIME(submit_tick, end_tick,
                        op->stats.cur_elem->trigger.submit.total);
    UCG_STATS_COST_TIME(start_tick, end_tick,
                        op->stats.cur_elem->trigger.total);
    return UCG_OK;
}

inline static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, progress)(ucg_plan_op_t *ucg_op)
{
    return ucg_planc_stars_algo_progress(ucg_op, NULL);
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_ring_2m, discard)(ucg_plan_op_t *ucg_op)
{
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    UCG_STATS_GET_TIME(buf_cleanup);
    if (op->super.vgroup->myrank == op->super.super.args.bcast.root) {
        ucg_planc_stars_buf_cleanup(op, STARS_BUFF_SEND);
    } else {
        ucg_planc_stars_buf_cleanup(op, STARS_BUFF_RECV);
    }
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

    ucg_stats_dump("bcast_ring_2m", &op->stats);
    return UCG_OK;
}

UCG_STARS_ALGO_PRE_DEFINE(bcast_ring_2m)
