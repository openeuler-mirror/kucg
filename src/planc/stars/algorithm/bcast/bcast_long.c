/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "bcast.h"
#include "planc_stars_algo.h"

#define BINOMIAL_DEGREE 2
#define ROLLING_PEER_NUM 2
#define SCATTER_EID_IDX 0
#define DEFAULT_GET_PEER_NUM 1


static ucg_status_t ucg_planc_stars_bcast_long_bntree_and_rolling_op_init(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myrank = vgroup->myrank;
    int32_t group_size = vgroup->size;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    ucg_algo_kntree_iter_t *kntree_iter = &op->bcast.longg.kntree_iter;
    ucg_algo_kntree_iter_init(kntree_iter, group_size, BINOMIAL_DEGREE, args->root, vgroup->myrank, 1);
    ucg_algo_ring_iter_t *ring_iter = &op->bcast.longg.ring_iter;
    ucg_algo_ring_iter_init(ring_iter, group_size, myrank);

    op->bcast.longg.lastrank = (args->root - 1 + group_size) % group_size;
    op->bcast.longg.lastcount = args->count % group_size + args->count / group_size;
    op->bcast.longg.recvcount = args->count / group_size;
    op->bcast.longg.division = args->count % group_size;

    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_bcast_long_ofd_wait_req(uint32_t eid_idx,
                                                            ucg_planc_stars_op_t *op,
                                                            stars_rank_info_h peer_rank)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);
    ucg_status_t status =
        ucg_planc_stars_fill_ofd_wait_req_elem(eid_idx, peer_rank,
                                               request, op->plan.event_elem);
    UCG_ASSERT_CODE_GOTO(status, free);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return UCG_OK;
free:
    ucg_mpool_put(request);
    return status;
}

static ucg_status_t ucg_planc_stars_bcast_long_ofd_bntree_put_req(ucg_planc_stars_op_t *op, uint32_t *child_index)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    int64_t recvtype_extent = ucg_dt_extent(args->dt);
    ucg_algo_kntree_iter_t *iter = &op->bcast.longg.kntree_iter;
    uint32_t recvcount = op->bcast.longg.recvcount;
    uint32_t division = op->bcast.longg.division;
    stars_comm_plan_t *plan = &op->plan;
    stars_rank_info_h peer_rank_info = &plan->comm_dep.put_ranks[*child_index];
    ucg_rank_t peer = peer_rank_info->peer_id;
    ucg_rank_t peer_vrank = (peer - args->root + group_size) % group_size;
    uint32_t send_displs = 0;
    uint32_t send_offset = 0;
    uint32_t sendcount = 0;
    int32_t child_size = ucg_algo_kntree_get_subtree_size(iter, peer);

    for (uint32_t block_idx = 0; block_idx < child_size; block_idx++) {
        scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
        UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

        send_displs = peer_vrank + block_idx;
        // If the last rank, add the division count.
        sendcount = send_displs < group_size - 1 ? recvcount : (recvcount + division);
        send_offset = send_displs * recvcount * recvtype_extent;

        request->lbuf        = (void *)args->buffer + send_offset;
        request->llen        = sendcount * recvtype_extent;
        request->rbuf        = (void *)peer_rank_info->rbuf_desc->addr + send_offset;
        request->lmemh       = plan->lsmemh;
        request->type        = OFFLOAD_PUT;
        // Send multiple tasks, the last task with notify.
        request->unnotify    = (block_idx == child_size - 1) ? 0 : 1;

        ucg_status_t status =
            ucg_planc_stars_fill_ofd_put_req_elem(SCATTER_EID_IDX, peer_rank_info, request);
        UCG_ASSERT_CODE_RET(status);
        ucg_planc_stars_op_push_ofd_req_elem(op, request);
    }

    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_bcast_long_ofd_rolling_put_req(ucg_planc_stars_op_t *op,
                                                                   stars_rank_info_h peer_rank_info)
{
    scp_ofd_req_elem_h request = ucg_planc_stars_op_get_ofd_req_elem(op);
    UCG_ASSERT_RET(request != NULL, UCG_ERR_NO_MEMORY);

    ucg_status_t status = UCG_OK;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t group_size = vgroup->size;
    ucg_rank_t myrank = vgroup->myrank;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    ucg_rank_t my_vrank = (myrank + group_size - args->root) % group_size;
    int64_t recvtype_extent = ucg_dt_extent(args->dt);
    ucg_algo_ring_iter_t *ring_iter = &op->bcast.longg.ring_iter;
    stars_comm_plan_t *plan = &op->plan;
    uint32_t step_idx = ucg_algo_ring_iter_idx(ring_iter);
    // If (myrank + step_idx) is odd, peer is my left, else my right.
    ucg_rank_t peer = ((my_vrank + step_idx) & 1) != 0 ? ring_iter->left : ring_iter->right;
    uint32_t block_idx;
    int64_t send_offset;

    if (peer == ring_iter->right) {
        block_idx = (my_vrank - step_idx / ROLLING_PEER_NUM + group_size) % group_size;
    } else {
        block_idx = (my_vrank + step_idx / ROLLING_PEER_NUM + group_size) % group_size;
    }
    send_offset = block_idx * op->bcast.longg.recvcount * recvtype_extent;
    int count = (block_idx == group_size - 1) ?
                op->bcast.longg.lastcount :
                op->bcast.longg.recvcount;
    int length = count * args->dt->size;
    uint32_t peer_eid_num = peer_rank_info->length;

    request->lbuf        = args->buffer + send_offset;
    request->llen        = length;
    request->rbuf        = (void *)peer_rank_info->rbuf_desc->addr + send_offset;
    request->lmemh       = plan->lsmemh;
    request->scp_event   = NULL;
    request->type        = OFFLOAD_PUT;

    status = ucg_planc_stars_fill_ofd_put_req_elem(peer_eid_num - 1, peer_rank_info, request);
    UCG_ASSERT_CODE_RET(status);
    ucg_planc_stars_op_push_ofd_req_elem(op, request);
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, scatter_submit)(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    stars_comm_plan_t *plan = &op->plan;
    ucg_rank_t myid = vgroup->myrank, root = args->root;
    ucg_algo_kntree_iter_t *kntree_iter = &op->bcast.longg.kntree_iter;
    stars_rank_info_h peer_rank;
    uint32_t child_index = 0;
    ucg_status_t status;
    // Scatter
    if (myid != root) {
        ucg_rank_t parent_id = ucg_algo_kntree_iter_parent_value(kntree_iter);
        peer_rank = plan->comm_dep.get_ranks[0].peer_id == parent_id ?
                    &plan->comm_dep.get_ranks[0] :
                    &plan->comm_dep.get_ranks[1];
        status = ucg_planc_stars_bcast_long_ofd_wait_req(0, op, peer_rank);
        UCG_CHECK_GOTO_ERR(status, out, "wait notify");
    }
    while (ucg_algo_kntree_iter_child_value(kntree_iter) != UCG_INVALID_RANK) {
        status = ucg_planc_stars_bcast_long_ofd_bntree_put_req(op, &child_index);
        UCG_CHECK_GOTO(status, out);
        ucg_algo_kntree_iter_child_inc(kntree_iter);
        child_index++;
    }

out:
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, allgather_submit)(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    stars_comm_plan_t *plan = &op->plan;
    uint32_t group_size = vgroup->size;
    ucg_rank_t myid = vgroup->myrank;
    ucg_rank_t root = args->root;
    uint32_t step_idx = 0;
    uint32_t peer_idx = 0;
    ucg_rank_t my_vrank = (myid + group_size - root) % group_size;
    ucg_rank_t peer_id;
    uint32_t get_rank_num = plan->comm_dep.get_rank_num;
    ucg_algo_ring_iter_t *ring_iter = &op->bcast.longg.ring_iter;
    khash_t(rank_idx)* rank_idx_map = &op->bcast.longg.rank_idx_map;
    stars_rank_info_h put_rank;
    stars_rank_info_h get_rank;
    ucg_status_t status = UCG_OK;

    while (!ucg_algo_ring_iter_end(ring_iter)) {
        step_idx = ucg_algo_ring_iter_idx(ring_iter);
        peer_id = (my_vrank + step_idx) & 1 ?
                  (myid + group_size - 1) % group_size :
                  (myid + 1) % group_size;
        rank_idx_map_status_t map_status =
            ucg_planc_stars_algo_get_rank_idx_from_map(peer_id, rank_idx_map, &peer_idx);
        if (map_status != RANK_IDX_MAP_OK) {
            return UCG_ERR_NO_RESOURCE;
        }
        put_rank = &plan->comm_dep.put_ranks[peer_idx];
        get_rank = peer_id == plan->comm_dep.get_ranks[get_rank_num - 1].peer_id ?
                   &plan->comm_dep.get_ranks[get_rank_num - 1] :
                   &plan->comm_dep.get_ranks[get_rank_num - 2];

        ucg_rank_t peer_vrank = (put_rank->peer_id + group_size - root) % group_size;
        uint32_t get_rank_eid_num = get_rank->length;
        if (my_vrank > peer_vrank) {
            status = ucg_planc_stars_bcast_long_ofd_rolling_put_req(op, put_rank);
            UCG_CHECK_GOTO(status, out);
            status = ucg_planc_stars_bcast_long_ofd_wait_req(get_rank_eid_num - 1, op, get_rank);
            UCG_CHECK_GOTO(status, out);
        } else {
            status = ucg_planc_stars_bcast_long_ofd_wait_req(get_rank_eid_num - 1, op, get_rank);
            UCG_CHECK_GOTO(status, out);
            status = ucg_planc_stars_bcast_long_ofd_rolling_put_req(op, put_rank);
            UCG_CHECK_GOTO(status, out);
        }
        ucg_algo_ring_iter_inc(ring_iter);
    }

out:
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, submit_op)(ucg_planc_stars_op_t *op)
{
    ucg_status_t status;

    status = UCG_STARS_ALGO_FUN(bcast_long, scatter_submit)(op);
    UCG_CHECK_GOTO(status, out);
    status = UCG_STARS_ALGO_FUN(bcast_long, allgather_submit)(op);
    UCG_CHECK_GOTO(status, out);

    return scp_submit_ofd_req(op->stars_group->context->scp_worker,
                              &op->stars_group->stars_stream,
                              &op->ofd_req, op->stats.cur_elem);

out:
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, init_rbuf)(ucg_planc_stars_op_t *op,
                                                              ucg_coll_args_t *coll_args)
{
    stars_comm_plan_t *plan = &op->plan;
    ucg_coll_bcast_args_t *args = &coll_args->bcast;

    /* when will eventid resouces be released? */
    ucg_status_t status =
        ucg_planc_stars_rbuf_init(op, args->buffer,
                                  args->count * args->dt->size);
    UCG_ASSERT_CODE_RET(status);

    for (uint32_t idx = 0; idx < plan->comm_dep.get_rank_num; ++idx) {
        plan->comm_dep.get_ranks[idx].rbuf_desc = plan->lrbuf_desc;
    }

    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, init_sbuf)(ucg_planc_stars_op_t *op,
                                                              ucg_coll_args_t *coll_args)
{
    stars_comm_plan_t *plan = &op->plan;

    plan->lsbuf_desc    = plan->lrbuf_desc;
    plan->lsmemh        = plan->lrmemh;
    return UCG_OK;
}


static inline size_t UCG_STARS_ALGO_FUN(bcast_long, put_max_size)(ucg_planc_stars_op_t *op,
                                                                  ucg_coll_args_t *coll_args)
{
    ucg_coll_bcast_args_t *args = &coll_args->bcast;
    return args->dt->size * args->count;
}

static inline void ucg_planc_stars_bcast_long_eid_num_buf_init(uint8_t *buf, size_t length, uint8_t val)
{
    for (size_t i = 0; i < length; i++) {
        buf[i] = val;
    }
}

static inline ucg_status_t ucg_planc_stars_bcast_long_add_put_rank(ucg_rank_t peer_id,
                                                                   stars_comm_dep_h comm_dep,
                                                                   khash_t(rank_idx)* rank_idx_map,
                                                                   ucg_rank_t *put_peer)
{
    ucg_status_t status = UCG_OK;
    put_peer[comm_dep->put_rank_num] = peer_id;
    rank_idx_map_status_t map_status =
        ucg_planc_stars_algo_put_rank_idx_to_map(peer_id, comm_dep->put_rank_num, rank_idx_map);
    comm_dep->put_rank_num++;
    if (map_status == RANK_IDX_MAP_OK) {
        return UCG_OK;
    }
    return UCG_ERR_NO_MEMORY;
}

static void ucg_planc_stars_bcast_long_get_peer_init(ucg_planc_stars_op_t *op, ucg_rank_t *get_peer,
                                                     uint8_t *get_event_num)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myid = op->super.vgroup->myrank;
    ucg_rank_t root = op->super.super.args.bcast.root;
    uint32_t numprocs = vgroup->size;
    ucg_algo_kntree_iter_t *iter = &op->bcast.kntree_iter;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    ucg_rank_t left_peer = (myid + numprocs - 1) % numprocs;
    ucg_rank_t right_peer = (myid + 1) % numprocs;

    get_peer[0] = left_peer;
    get_peer[1] = right_peer;
    if (myid != root) {
        ucg_rank_t parent = ucg_algo_kntree_iter_parent_value(iter);
        if (parent == get_peer[0]) {
            get_event_num[0]++;
        } else if (parent == get_peer[1]) {
            get_event_num[1]++;
        } else {
            comm_dep->get_rank_num++;
            get_peer[0] = ucg_algo_kntree_iter_parent_value(iter);
            get_peer[1] = left_peer;
            get_peer[2] = right_peer;
        }
    }
}

static ucg_status_t ucg_planc_stars_bcast_long_rank_dep_init(ucg_planc_stars_op_t *op,
                                                             ucg_rank_t *get_peer, uint8_t *get_event_num,
                                                             ucg_rank_t *put_peer, uint8_t *put_event_num)
{
    ucg_status_t status;
    ucg_rank_t peer_id;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;

    status = ucg_planc_stars_rank_dep_alloc(comm_dep);
    UCG_ASSERT_CODE_RET(status);
    for (uint32_t cnt = 0; cnt < comm_dep->get_rank_num; ++cnt) {
        peer_id = get_peer[cnt];
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->get_ranks[cnt],
                                               peer_id, get_event_num[cnt]);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }

    for (uint32_t cnt = 0; cnt < comm_dep->put_rank_num; ++cnt) {
        peer_id = put_peer[cnt];
        status = ucg_planc_stars_rank_dep_init(op, &comm_dep->put_ranks[cnt],
                                               peer_id, put_event_num[cnt]);
        UCG_ASSERT_CODE_GOTO(status, err_free_memory);
    }
    return UCG_OK;

err_free_memory:
    if (comm_dep->get_ranks) {
        ucg_free(comm_dep->get_ranks);
    }
    if (comm_dep->put_ranks) {
        ucg_free(comm_dep->put_ranks);
    }
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, offload_plan)(ucg_planc_stars_op_t *op)
{
    ucg_vgroup_t *vgroup = op->super.vgroup;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    ucg_planc_stars_context_t *context = op->stars_group->context;
    ucg_coll_bcast_args_t *args = &op->super.super.args.bcast;
    ucg_rank_t root = args->root, myid = vgroup->myrank, peer_id;
    uint32_t numprocs = vgroup->size;
    ucg_algo_kntree_iter_t *iter = &op->bcast.kntree_iter;
    khash_t(rank_idx)* rank_idx_map = &op->bcast.longg.rank_idx_map;
    ucg_rank_t put_peer[numprocs + ROLLING_PEER_NUM]; // 2 for rolling, numprocs for binomial tree;
    uint8_t put_event_num[numprocs + ROLLING_PEER_NUM];
    ucg_rank_t get_peer[DEFAULT_GET_PEER_NUM + ROLLING_PEER_NUM]; // 2 for rolling, 1 for binomial tree;
    uint8_t get_event_num[DEFAULT_GET_PEER_NUM + ROLLING_PEER_NUM];
    ucg_status_t status;

    ucg_rank_t left_peer = (myid + numprocs - 1) % numprocs;
    ucg_rank_t right_peer = (myid + 1) % numprocs;

    ucg_planc_stars_bcast_long_eid_num_buf_init(&put_event_num[0], sizeof(put_event_num), 1);
    ucg_planc_stars_bcast_long_eid_num_buf_init(&get_event_num[0], sizeof(get_event_num), 1);

    comm_dep->get_ranks = NULL;
    comm_dep->get_rank_num = ROLLING_PEER_NUM;
    ucg_planc_stars_bcast_long_get_peer_init(op, &get_peer[0], &get_event_num[0]);

    comm_dep->put_ranks = NULL;
    comm_dep->put_rank_num = 0;

    while ((peer_id = ucg_algo_kntree_iter_child_value(iter)) != UCG_INVALID_RANK) {
        status = ucg_planc_stars_bcast_long_add_put_rank(peer_id, comm_dep,
                                                         rank_idx_map, &put_peer[0]);
        UCG_ASSERT_CODE_RET(status);
        ucg_algo_kntree_iter_child_inc(iter);
    }
    ucg_algo_kntree_iter_reset(iter);

    rank_idx_map_status_t map_ret;
    uint32_t idx_out = 0;
    ucg_rank_t peer[ROLLING_PEER_NUM] = {left_peer, right_peer};
    for (size_t i = 0; i < ROLLING_PEER_NUM; i++) {
        map_ret = ucg_planc_stars_algo_get_rank_idx_from_map(peer[i], rank_idx_map, &idx_out);
        if (map_ret == RANK_IDX_MAP_NOT_FOUND) {
            status = ucg_planc_stars_bcast_long_add_put_rank(peer[i], comm_dep,
                                                             rank_idx_map, &put_peer[0]);
            UCG_ASSERT_CODE_RET(status);
        } else {
            put_event_num[idx_out]++;
        }
    }

    status = ucg_planc_stars_bcast_long_rank_dep_init(op, &get_peer[0], &get_event_num[0],
                                                      &put_peer[0], &put_event_num[0]);

    return status;
}

static inline ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, check)(ucg_vgroup_t *vgroup,
                                                                 const ucg_coll_args_t *coll_args) {
    int32_t numprocs = vgroup->size;
    int32_t count = coll_args->bcast.count;
    if (numprocs > count) {
        ucg_warn("bcast long don't support the number of progresses larger than count");
        return UCG_ERR_UNSUPPORTED;
    }
    if (numprocs == 2) {
        ucg_warn("bcast long don't support 2 progresses");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, init)(ucg_plan_op_t *ucg_op)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_coll_args_t *coll_args = &ucg_op->super.args;
    ucg_status_t status;
    status = ucg_planc_stars_bcast_long_bntree_and_rolling_op_init(op);
    kh_init_inplace(rank_idx, &op->bcast.longg.rank_idx_map);

    status = ucg_planc_stars_algo_prepare_plan(op, coll_args,
                                               UCG_STARS_ALGO_FUN(bcast_long, put_max_size),
                                               UCG_STARS_ALGO_FUN(bcast_long, offload_plan));
    UCG_CHECK_GOTO_ERR(status, out, "prepare bcast long plan");

    status = ucg_planc_stars_algo_exch_buf_addr(op, coll_args,
                                                UCG_STARS_ALGO_FUN(bcast_long, init_rbuf),
                                                UCG_STARS_ALGO_FUN(bcast_long, init_sbuf));
    UCG_CHECK_GOTO_ERR(status, out, "exchange buffer address");

    op->stats.nb_type = (coll_args->type == UCG_COLL_TYPE_IBCAST) ? 0 : 1;
    return UCG_OK;
out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, trigger)(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op  = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    ucg_planc_stars_op_reset(op);

    op->super.super.status = UCG_ERR_IO_ERROR;

    status = ucg_planc_stars_plan_get_stats_elem(op);
    UCG_ASSERT_CODE_RET(status);

    ucg_algo_kntree_iter_reset(&op->bcast.longg.kntree_iter);
    ucg_algo_ring_iter_reset(&op->bcast.longg.ring_iter);
    UCG_STATS_GET_TIME(submit_tick);
    status = UCG_STARS_ALGO_FUN(bcast_long, submit_op)(op);
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

inline static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, progress)(ucg_plan_op_t *ucg_op)
{
    return ucg_planc_stars_algo_progress(ucg_op, NULL);
}

static ucg_status_t UCG_STARS_ALGO_FUN(bcast_long, discard)(ucg_plan_op_t *ucg_op)
{
    UCG_STATS_GET_TIME(start_tick);
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    UCG_STATS_GET_TIME(buf_cleanup);
    ucg_planc_stars_buf_cleanup(op);
    UCG_STATS_GET_TIME(op_cleanup);
    kh_init_inplace(rank_idx, &op->bcast.longg.rank_idx_map);
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

    ucg_stats_dump("bcast_long", &op->stats);
    return UCG_OK;
}

UCG_STARS_ALGO_PRE_DEFINE(bcast_long)