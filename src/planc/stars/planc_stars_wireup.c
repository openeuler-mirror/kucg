/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_wireup.h"

#include "scp_address.h"


static ucg_status_t ucg_planc_stars_exch_ep_addr(stars_rank_info_h ranks,
                                                 uint32_t rank_num,
                                                 uint16_t tag,
                                                 ucg_planc_stars_p2p_params_t *params,
                                                 ucg_vgroup_t *vgroup)
{
    ucg_status_t status;
    stars_rank_info_h peer;

    for (int index = 0; index < rank_num; ++index) {
        peer = &ranks[index];
        UCG_ASSERT_GOTO(peer != NULL, out, UCG_ERR_INVALID_ADDR);
        if (scp_ep_is_connected(peer->ep)) {
            continue;
        }

        status = scp_ep_address_pack(peer->ep, peer->ep->self_addr);
        UCG_ASSERT_CODE_GOTO(status, out);
        ucp_datatype_t ucp_dt = ucp_dt_make_contig(1);
        status = ucg_planc_stars_oob_isend(peer->peer_id, peer->ep->self_addr,
                                           SCP_EP_PACK_SIZE, tag, vgroup, params, ucp_dt);
        UCG_ASSERT_CODE_GOTO(status, out);

        status = ucg_planc_stars_oob_irecv(peer->peer_id, peer->ep->peer_addr,
                                           SCP_EP_PACK_SIZE, tag, vgroup, params, ucp_dt);
        UCG_ASSERT_CODE_GOTO(status, out);
    }

    return UCG_OK;

out:
    return status;
}

static ucg_status_t ucg_planc_stars_exch_channel_addr(ucg_planc_stars_op_t *op)
{
    ucg_status_t status;
    ucg_planc_stars_p2p_params_t params;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    ucg_vgroup_t *vgroup = op->super.vgroup;

    ucg_planc_stars_set_p2p_params(op, &params);

    UCG_STATS_GET_TIME(start_tick);
    status = ucg_planc_stars_exch_ep_addr(comm_dep->get_ranks, comm_dep->get_rank_num,
                                          op->tag, &params, vgroup);
    UCG_CHECK_GOTO_ERR(status, out, "exchange ep address for get");

    UCG_STATS_GET_TIME(exch_ep_tick);
    status = ucg_planc_stars_exch_ep_addr(comm_dep->put_ranks, comm_dep->put_rank_num,
                                          op->tag, &params, vgroup);
    UCG_CHECK_GOTO_ERR(status, out, "exchange ep address for put");

    UCG_STATS_GET_TIME(waitall_tick);
    status = ucg_planc_stars_oob_waitall(params.state);
    UCG_CHECK_GOTO_ERR(status, out, "wait ep addr exchagne");

    UCG_STATS_GET_TIME(end_tick);
    UCG_STATS_COST_TIME(start_tick, exch_ep_tick, op->stats.init.pre_plan.conn.exchg_addr.exch_ep_get);
    UCG_STATS_COST_TIME(exch_ep_tick, waitall_tick, op->stats.init.pre_plan.conn.exchg_addr.exch_ep_put);
    UCG_STATS_COST_TIME(waitall_tick, end_tick, op->stats.init.pre_plan.conn.exchg_addr.wait_all);
    return UCG_OK;

out:
    return status;
}

static void ucg_planc_stars_fill_bandwidth(ucg_planc_stars_context_t *context, const scp_ep_h ep)
{
    if (context->config.estimated_num_ppn == 0) {
        ucg_info("Invalid estimated_num_ppn which is 0, automatically set to 1");
        context->config.estimated_num_ppn = 1;
    }

    sct_iface_attr_t *iface_attr;
    uct_ppn_bandwidth_t *iface_bw;
    uint32_t bw;
    for (uint8_t ep_idx = 0; ep_idx < ep->sct_ep_num; ++ep_idx) {
        iface_attr = scp_worker_idx_iface_get_attr(ep->worker, ep_idx);
        iface_bw = &iface_attr->bandwidth;
        bw = iface_bw->dedicated + (iface_bw->shared / context->config.estimated_num_ppn);
        ep->bandwidths[ep_idx] = bw;
    }
}

ucg_status_t ucg_planc_stars_channel_connect(ucg_planc_stars_op_t *op)
{
    ucg_planc_stars_t *planc_stars = ucg_planc_stars_instance();
    ucg_planc_stars_context_t *context = op->stars_group->context;
    ucg_group_t *group = op->stars_group->super.super.group;
    stars_rank_info_h peer_info;
    void *peer_proc;

    UCG_STATS_GET_TIME(start_tick);
    op->plan.max_lanes_num_in_ep = 0;
    ucg_status_t status = ucg_planc_stars_exch_channel_addr(op);
    UCG_ASSERT_CODE_RET(status);

    UCG_STATS_GET_TIME(finish_exchg_tick);
    op->plan.max_lanes_num_in_ep = 0;
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    for (uint32_t index = 0; index < comm_dep->get_rank_num; ++index) {
        peer_info = &comm_dep->get_ranks[index];
        if (peer_info->ep->conn_num > 0) {
            /* Avoid repeated line establishment */
            goto count_lanes_get;
        }
        UCG_STATS_GET_TIME(get_proc_tick);
        ucg_planc_stars_fill_bandwidth(context, peer_info->ep);

        ucg_proc_info_t *proc_info = NULL;
        ucg_rank_t ctx_rank = ucg_rank_map_eval(&group->rank_map, peer_info->peer_id);
        peer_proc = ucg_context_get_proc_addr(context->super, ctx_rank,
                                              &planc_stars->super, &proc_info);
        ucg_free_proc_info(proc_info);

        UCG_STATS_GET_TIME(get_connect_tick);
        status = scp_connect_ep((scp_address_t *)peer_proc, &peer_info->ep);
        UCG_ASSERT_CODE_GOTO(status, error_handle);
        UCG_STATS_GET_TIME(conn_ep_tick);
        UCG_STATS_ADD_TIME(get_proc_tick, get_connect_tick,
                           op->stats.init.pre_plan.conn.get_proc);
        UCG_STATS_ADD_TIME(get_connect_tick, conn_ep_tick,
                           op->stats.init.pre_plan.conn.do_conn);

    count_lanes_get:
        /* Record the possible maximum number of lines to calculate the
           event_id number. */
        if (peer_info->ep->conn_num > op->plan.max_lanes_num_in_ep) {
            op->plan.max_lanes_num_in_ep = peer_info->ep->conn_num;
        }
    }

    for (uint32_t index = 0; index < comm_dep->put_rank_num; ++index) {
        peer_info = &comm_dep->put_ranks[index];
        if (peer_info->ep->conn_num > 0) {
            /* Avoid repeated line establishment */
            goto count_lanes_put;
        }
        UCG_STATS_GET_TIME(get_proc_tick);
        ucg_planc_stars_fill_bandwidth(context, peer_info->ep);

        ucg_proc_info_t *proc_info = NULL;
        ucg_rank_t ctx_rank = ucg_rank_map_eval(&group->rank_map, peer_info->peer_id);
        peer_proc = ucg_context_get_proc_addr(context->super, ctx_rank,
                                              &planc_stars->super, &proc_info);
        ucg_free_proc_info(proc_info);

        UCG_STATS_GET_TIME(get_connect_tick);
        status = scp_connect_ep((scp_address_t *)peer_proc, &peer_info->ep);
        UCG_ASSERT_CODE_GOTO(status, error_handle);
        UCG_STATS_GET_TIME(conn_ep_tick);
        UCG_STATS_ADD_TIME(get_proc_tick, get_connect_tick,
                           op->stats.init.pre_plan.conn.get_proc);
        UCG_STATS_ADD_TIME(get_connect_tick, conn_ep_tick,
                           op->stats.init.pre_plan.conn.do_conn);
    count_lanes_put:
        /* Record the possible maximum number of lines to calculate the
           event_id number. */
        if (peer_info->ep->conn_num > op->plan.max_lanes_num_in_ep) {
            op->plan.max_lanes_num_in_ep = peer_info->ep->conn_num;
        }
    }
    UCG_STATS_GET_TIME(end_tick);

    UCG_STATS_COST_TIME(start_tick, finish_exchg_tick,
                        op->stats.init.pre_plan.conn.exchg_addr.total);
    UCG_STATS_COST_TIME(start_tick, end_tick, op->stats.init.pre_plan.conn.total);

    return UCG_OK;

error_handle:
    return status;
}
