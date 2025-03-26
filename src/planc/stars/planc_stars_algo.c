/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_algo.h"


static ucg_status_t ucg_planc_stars_create_rbuf_desc(ucg_planc_stars_op_t *op)
{
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    stars_rank_info_h put_rank;

    for (uint32_t idx = 0; idx < comm_dep->put_rank_num; ++idx) {
        put_rank = &comm_dep->put_ranks[idx];
        UCG_ASSERT_RET(put_rank != NULL, UCG_ERR_INVALID_ADDR);

        put_rank->rbuf_desc = ucg_planc_stars_plan_alloc_buf_desc(put_rank);
        UCG_ASSERT_RET(put_rank->rbuf_desc != NULL, UCG_ERR_NO_MEMORY);
    }

    return UCG_OK;
}

ucg_status_t ucg_planc_stars_algo_prepare_plan(ucg_planc_stars_op_t *op,
                                               ucg_coll_args_t *coll_args,
                                               ucg_stars_max_put_size_fun_t max_put_size,
                                               ucg_stars_ofd_plan_fun_t ofd_plan)
{
    ucg_status_t status;

    UCG_STATS_GET_TIME(start_tick);
    stars_comm_plan_t *plan = &op->plan;
    plan->sbuf_size = max_put_size(op, coll_args);
    scp_enable_multi_rail(op->stars_group->context->scp_context,
                          plan->sbuf_size);

    UCG_STATS_GET_TIME(plan_tick_start);
    status = ofd_plan(op);
    UCG_CHECK_GOTO_ERR(status, out, "generate offload plan");

    UCG_STATS_GET_TIME(plan_tick_end);
    status = ucg_planc_stars_create_rbuf_desc(op);
    UCG_CHECK_GOTO_ERR(status, out, "alloc remote desc");

    status = ucg_planc_stars_channel_connect(op);
    UCG_CHECK_GOTO_ERR(status, out, "communication connect");
    UCG_STATS_GET_TIME(end_tick);

    UCG_STATS_COST_TIME(plan_tick_start, plan_tick_end, op->stats.init.pre_plan.plan);
    UCG_STATS_COST_TIME(start_tick, end_tick, op->stats.init.pre_plan.total);

    return UCG_OK;

out:
    return status;
}

ucg_status_t ucg_planc_stars_algo_exch_buf_addr(ucg_planc_stars_op_t *op,
                                                ucg_coll_args_t *coll_args,
                                                ucg_stars_algo_init_buf_fun_t init_rbuf,
                                                ucg_stars_algo_init_buf_fun_t init_sbuf)
{
    UCG_STATS_GET_TIME(start_tick);
    ucg_status_t status = init_rbuf(op, coll_args);
    UCG_CHECK_GOTO_ERR(status, out, "init local receive buffer");

    UCG_STATS_GET_TIME(sbuf_tick);
    status = init_sbuf(op, coll_args);
    UCG_CHECK_GOTO_ERR(status, out, "init local send buffer");

    UCG_STATS_GET_TIME(exch_tick);
    status = ucg_planc_stars_exch_addr_msg(op);
    UCG_CHECK_GOTO_ERR(status, out, "exchange buffer msg");

    UCG_STATS_GET_TIME(unpack_tick);
    status = ucg_planc_stars_mkey_unpack(op);
    UCG_CHECK_GOTO_ERR(status, out, "unpack mkey");
    UCG_STATS_GET_TIME(end_tick);

    UCG_STATS_COST_TIME(start_tick, sbuf_tick, op->stats.init.exch_buf.init_rbuf.total);
    UCG_STATS_COST_TIME(sbuf_tick, exch_tick, op->stats.init.exch_buf.init_sbuf);
    UCG_STATS_COST_TIME(exch_tick, unpack_tick, op->stats.init.exch_buf.exchg);
    UCG_STATS_COST_TIME(start_tick, unpack_tick, op->stats.init.exch_buf.total);
    UCG_STATS_COST_TIME(unpack_tick, end_tick, op->stats.init.exch_buf.unpack_mkey);

    return UCG_OK;

out:
    return status;
}

ucg_status_t ucg_planc_stars_algo_progress(ucg_plan_op_t *ucg_op, ucg_planc_stars_progress_done_func_t cb)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    scp_worker_h worker = op->stars_group->context->scp_worker;

    UCG_STATS_GET_TIME(start_tick);
    ucg_status_t status = scp_worker_progress(worker);
    UCG_STATS_GET_TIME(finish_progress);
    UCG_ASSERT_CODE_GOTO(status, out);

    status = op->ofd_req.result;
    if (status == UCG_OK) {
        if (cb) {
            status = cb(op);
        }
        scp_release_ofd_req(&op->ofd_req);
        UCG_STATS_GET_TIME(end_tick);
        UCG_STATS_SET_TIME(op->stats.cur_elem->end_tick, end_tick);
        UCG_STATS_ADD_TIME(start_tick, finish_progress,
                           op->stats.cur_elem->progress.progress);
        UCG_STATS_ADD_TIME(start_tick, end_tick,
                           op->stats.cur_elem->progress.total);
        UCG_STATS_COST_TIME(op->stats.cur_elem->start_tick,
                            op->stats.cur_elem->end_tick,
                            op->stats.cur_elem->cost_time);
    }

out:
    op->super.super.status = status;
    return status;
}