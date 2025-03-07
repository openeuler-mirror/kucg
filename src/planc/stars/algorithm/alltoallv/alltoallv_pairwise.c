/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "alltoallv.h"
#include "barrier/barrier_faninfanout.h"
#include "planc_stars_algo_def.h"

#define PAIRWISE_EID_DIX 0

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, barrier_init)(ucg_planc_stars_op_t *op)
{
    ucg_status_t status;
    ucg_coll_args_t barrier_args;
    barrier_args.type = UCG_COLL_TYPE_IBARRIER;
    status = UCG_STARS_ALGO_PRE_NAME(barrier_faninfanout)(op->super.vgroup, &barrier_args,
                                                          &op->meta_op);
    return status;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, offload_plan)(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t numprocs = vgroup->size;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    comm_dep->get_ranks = NULL;
    comm_dep->get_rank_num = numprocs - 1;
    comm_dep->put_ranks = NULL;
    comm_dep->put_rank_num = numprocs - 1;
    op->plan.eid_shared_flag = 1;
    op->plan.eid_shared_stride = 5;
    ucg_status_t status;
    status = ucg_planc_stars_rank_dep_alloc(comm_dep);
    UCG_ASSERT_CODE_RET(status);

    ucg_rank_t peer_id;
    uint32_t index = 0;
    ucg_rank_t myid = vgroup->myrank;
    for (ucg_rank_t offset = 1; offset < numprocs; ++offset, ++index) {
        peer_id = (myid + offset) % numprocs;
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->put_ranks[index],
                                               peer_id, 1);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);

        peer_id = (myid + numprocs - offset) % numprocs;
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->get_ranks[index],
                                               peer_id, 1);
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

static inline size_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, put_max_size)(ucg_planc_stars_op_t *op,
                                                     ucg_coll_args_t *coll_args)
{
    ucg_coll_alltoallv_args_t *args = &coll_args->alltoallv;
    uint32_t vgroup_size = op->super.vgroup->size;
    int32_t count = 0;
    int32_t max_offset = 0;

    for (uint32_t idx = 0; idx < vgroup_size; idx++) {
        if (max_offset < args->sdispls[idx]) {
            max_offset = args->sdispls[idx];
            count = max_offset + args->sendcounts[idx];
        }
    }
    return count * args->sendtype->size;
}

static size_t ucg_planc_stars_alltoallv_pairwise_caculate_rbuf_size(ucg_coll_alltoallv_args_t *args,
                                                                    ucg_vgroup_t *vgroup)
{
    uint32_t numprocs = vgroup->size;
    int32_t count = 0;
    int32_t max_offset = 0;

    for (uint32_t index = 0; index < numprocs; index++) {
        if (max_offset < args->rdispls[index]) {
            max_offset = args->rdispls[index];
            count = max_offset + args->recvcounts[index];
        }
    }

    return count * args->recvtype->size;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, init_rbuf)(ucg_planc_stars_op_t *op,
                                                  ucg_coll_args_t *coll_args)
{
    ucg_status_t status;
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_alltoallv_args_t *args = &coll_args->alltoallv;
    ucg_vgroup_t *vgroup = op->super.vgroup;

    size_t rbuf_size =
        ucg_planc_stars_alltoallv_pairwise_caculate_rbuf_size(args, vgroup);

    status = ucg_planc_stars_rbuf_init(op, args->recvbuf, rbuf_size);

    UCG_ASSERT_CODE_RET(status);

    for (uint32_t idx = 0; idx < plan->comm_dep.get_rank_num; ++idx) {
        plan->comm_dep.get_ranks[idx].rbuf_desc = plan->lrbuf_desc;
    }

    return status;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, init_sbuf)(ucg_planc_stars_op_t *op,
                                                  ucg_coll_args_t *coll_args)
{
    ucg_status_t status;
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_alltoallv_args_t *args = &coll_args->alltoallv;

    status = ucg_planc_stars_sbuf_init(op, (void *)args->sendbuf, plan->sbuf_size);
    UCG_CHECK_GOTO_ERR(status, out, "Failed to initialize alltoallv pairwise send buffer.");
out:
    return status;
}

static inline ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, check)(ucg_vgroup_t *vgroup,
                                              const ucg_coll_args_t *coll_args)
{
    // Reserved.
    return UCG_OK;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, init)(ucg_plan_op_t *ucg_op)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_coll_args_t *coll_args = &ucg_op->super.args;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_status_t status;
    status = UCG_STARS_ALGO_FUN(alltoallv_pairwise, barrier_init)(op);
    ucg_algo_ring_iter_init(&op->alltoallv.ring_iter, vgroup->size, vgroup->myrank);
    status = ucg_planc_stars_algo_prepare_plan(op, coll_args,
                                               UCG_STARS_ALGO_FUN(alltoallv_pairwise, put_max_size),
                                               UCG_STARS_ALGO_FUN(alltoallv_pairwise, offload_plan));
    UCG_CHECK_GOTO_ERR(status, out, "prepare alltoallv pairwise plan");

    status = ucg_planc_stars_algo_exch_buf_addr(op, coll_args,
                                                UCG_STARS_ALGO_FUN(alltoallv_pairwise, init_rbuf),
                                                UCG_STARS_ALGO_FUN(alltoallv_pairwise, init_sbuf));
    UCG_CHECK_GOTO_ERR(status, out, "exchange buffer address");

    op->stats.nb_type = (coll_args->type == UCG_COLL_TYPE_IALLTOALLV) ? 0 : 1;
    return UCG_OK;
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_stars_alltoallv_pairwise_put_req(ucg_planc_stars_op_t *op)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    ucg_coll_alltoallv_args_t *args = &op->super.super.args.alltoallv;
    ucg_vgroup_t *vgroup            = op->super.vgroup;
    uint32_t send_type_extent       = ucg_dt_extent(args->sendtype);
    stars_comm_plan_t *plan         = &op->plan;
    ucg_algo_ring_iter_t *iter      = &op->alltoallv.ring_iter;
    int step_idx                    = ucg_algo_ring_iter_idx(iter);
    stars_rank_info_h peer_rank     = &plan->comm_dep.put_ranks[step_idx];
    ucg_rank_t right_peer            = peer_rank->peer_id;

    request->lbuf   = (void *)args->sendbuf + (uint64_t)args->sdispls[right_peer] * send_type_extent;
    request->llen   = args->sendcounts[right_peer] * args->sendtype->size;
    request->rbuf   = (void *)plan->comm_dep.put_ranks[step_idx].rbuf_desc->addr
                      + (uint64_t)args->sdispls[vgroup->myrank] * send_type_extent;
    request->lmemh  = plan->lsmemh;
    request->type   = OFFLOAD_PUT;
    request->flag   = 0;

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_put_req_elem(PAIRWISE_EID_DIX, peer_rank, request);
    UCG_ASSERT_CODE_RET(status);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_alltoallv_pairwise_wait_req(ucg_planc_stars_op_t *op)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    ucg_algo_ring_iter_t *iter  = &op->alltoallv.ring_iter;
    stars_comm_plan_t *plan     = &op->plan;
    int step_idx                = ucg_algo_ring_iter_idx(iter);
    stars_rank_info_h peer_rank = &plan->comm_dep.get_ranks[step_idx];

    ucg_status_t status =
        ucg_planc_stars_fill_ofd_wait_req_elem(PAIRWISE_EID_DIX, peer_rank, request, op->plan.event_elem);
    UCG_ASSERT_CODE_GOTO(status, free);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
free:
    ucg_mpool_put(request);
    return status;
}

static ucg_status_t ucg_planc_stars_alltoallv_pairwise_handle_not_inplace(ucg_coll_alltoallv_args_t *args,
                                                                          ucg_rank_t myid)
{
    void *recvbuf = args->recvbuf;
    const void *sendbuf = args->sendbuf;

    ucg_dt_t *recvtype = args->recvtype;
    ucg_dt_t *sendtype = args->sendtype;
    uint32_t recvtype_extent = ucg_dt_extent(recvtype);
    uint32_t sendtype_extent = ucg_dt_extent(sendtype);

    return ucg_dt_memcpy((char*)recvbuf + args->rdispls[myid] * recvtype_extent,
                         args->recvcounts[myid], recvtype,
                         (char*)sendbuf + args->sdispls[myid] * sendtype_extent,
                         args->sendcounts[myid], sendtype);
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, submit_op)(ucg_planc_stars_op_t *op)
{
    ucg_status_t status;
    ucg_algo_ring_iter_t *iter  = &op->alltoallv.ring_iter;

    while (!ucg_algo_ring_iter_end(iter)) {
        status = ucg_planc_stars_alltoallv_pairwise_put_req(op);
        UCG_CHECK_GOTO_ERR(status, out, "stars alltoallv pairwise wait req");
        status = ucg_planc_stars_alltoallv_pairwise_wait_req(op);
        UCG_CHECK_GOTO_ERR(status, out, "stars alltoallv pairwise put req");
        ucg_algo_ring_iter_inc(iter);
        if (iter->idx % op->plan.eid_shared_stride == 0) {
            status = UCG_STARS_ALGO_FUN(barrier_faninfanout, meta_trigger)(&op->super, op->meta_op);
        }
        UCG_CHECK_GOTO_ERR(status, out, "stars alltoallv pairwise barrier");
    }

    return scp_submit_ofd_req(op->stars_group->context->scp_worker,
                              &op->stars_group->stars_stream,
                              &op->ofd_req, op->stats.cur_elem);

out:
	return status;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, trigger)(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_planc_stars_op_reset(op);
    ucg_algo_ring_iter_reset(&op->alltoallv.ring_iter);
    ucg_coll_alltoallv_args_t *args = &op->super.super.args.alltoallv;
    op->super.super.status = UCG_ERR_IO_ERROR;

    status = ucg_planc_stars_plan_get_stats_elem(op);
    UCG_ASSERT_CODE_RET(status);
    status = UCG_STARS_ALGO_FUN(alltoallv_pairwise, submit_op)(op);
    UCG_ASSERT_CODE_RET(status);
    op->super.super.status = UCG_INPROGRESS;

    return UCG_OK;
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, progress_done)(ucg_planc_stars_op_t *op)
{
    ucg_status_t status = UCG_OK;
    ucg_coll_alltoallv_args_t *args = &op->super.super.args.alltoallv;
    ucg_rank_t myid = op->super.vgroup->myrank;

    if (args->sendbuf != UCG_IN_PLACE) {
        status = ucg_planc_stars_alltoallv_pairwise_handle_not_inplace(args, myid);
        UCG_ASSERT_CODE_RET(status);
    }

    return status;
}

inline static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, progress)(ucg_plan_op_t *ucg_op)
{
    return ucg_planc_stars_algo_progress(ucg_op,
                                         &UCG_STARS_ALGO_FUN(alltoallv_pairwise, progress_done));
}

static ucg_status_t
UCG_STARS_ALGO_FUN(alltoallv_pairwise, discard)(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    status = op->meta_op->discard(op->meta_op);
    op->meta_op = NULL;
    UCG_ASSERT_CODE_RET(status);
    ucg_planc_stars_buf_cleanup(op);
    status = ucg_planc_stars_op_discard(ucg_op);
    UCG_ASSERT_CODE_RET(status);
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, ucg_op);
    ucg_stats_dump("bcast_alltoallv_pairwise", &op->stats);
    return UCG_OK;
}

UCG_STARS_ALGO_PRE_DEFINE(alltoallv_pairwise)