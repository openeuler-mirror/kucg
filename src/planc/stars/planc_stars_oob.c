/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_oob.h"

#include "scp_rma.h"


inline static ucp_tag_t ucg_planc_stars_make_tag(uint16_t tag, ucg_rank_t rank, uint32_t group_id)
{
    return ((((uint64_t)(tag)) << UCG_P2P_SEQ_BITS_OFFSET) |
            (((uint64_t)(rank)) << UCG_P2P_RANK_BITS) |
            (((uint64_t)(UCG_P2P_FLAG_TWO)) << UCG_P2P_FLAG_BITS_OFFSET) |
            (((uint64_t)(group_id)) << UCG_P2P_ID_BITS_OFFSET));
}

static void ucg_planc_stars_oob_isend_done(void *request, ucs_status_t status,
                                           void *user_data)
{
    ucg_planc_stars_p2p_state_t *state = (ucg_planc_stars_p2p_state_t*)user_data;
    if (status != UCS_OK) {
        ucg_error("Failed to send, %s", ucs_status_string(status));
        state->status = UCG_ERR_IO_ERROR;
    }

    --state->inflight_send_cnt;
    ucg_planc_stars_p2p_req_t *req = (ucg_planc_stars_p2p_req_t*)request;
    if (req->free_in_cb) {
        ucp_request_free(request);
    }

    return;
}

static void ucg_planc_stars_oob_irecv_done(void *request, ucs_status_t status,
                                           const ucp_tag_recv_info_t *info,
                                           void *user_data)
{
    ucg_planc_stars_p2p_state_t *state = (ucg_planc_stars_p2p_state_t*)user_data;
    if (status != UCS_OK) {
        ucg_error("Failed to receive, %s", ucs_status_string(status));
        state->status = UCG_ERR_IO_ERROR;
    }

    --state->inflight_recv_cnt;
    ucg_planc_stars_p2p_req_t *req = (ucg_planc_stars_p2p_req_t*)request;
    if (req->free_in_cb) {
        ucp_request_free(request);
    }

    return;
}

ucg_status_t ucg_planc_stars_oob_irecv(ucg_rank_t vrank, void *buffer, uint32_t size,
                                       uint16_t tag, ucg_vgroup_t *vgroup,
                                       ucg_planc_stars_p2p_params_t *params,
                                       ucp_datatype_t ucp_dt)
{
    UCG_ASSERT_RET(vrank != UCG_INVALID_RANK, UCG_ERR_INVALID_PARAM);
    UCG_CHECK_NULL_INVALID(ucp_dt, vgroup, params, params->stars_group, params->state);

    ucg_group_t *group = vgroup->group;
    ucg_rank_t sender_group_rank = ucg_rank_map_eval(&vgroup->rank_map, vrank);
    uint64_t ucp_tag = ucg_planc_stars_make_tag(tag, sender_group_rank, group->id);

    ucg_planc_stars_p2p_state_t *state = params->state;
	ucp_request_param_t req_param = {
        .op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                        UCP_OP_ATTR_FIELD_USER_DATA |
                        UCP_OP_ATTR_FIELD_DATATYPE,
        .cb.recv = ucg_planc_stars_oob_irecv_done,
        .datatype = ucp_dt,
        .user_data = (void*)state
    };

    ucp_worker_h ucp_worker = ucg_planc_stars_oob_get_ucp_worker();
    ucs_status_ptr_t ucp_req = ucp_tag_recv_nbx(ucp_worker, buffer, size, ucp_tag,
                                                UCG_P2P_TAG_MASK, &req_param);
    if (ucp_req == NULL || UCS_PTR_IS_ERR(ucp_req)) {
        return ucg_status_s2g(UCS_PTR_STATUS(ucp_req));
    }

	/* If another thread is executing ucp_worker_progress(), the following is
	   not thread-safe. */

    /* Receive is not finished. */
    ++state->inflight_recv_cnt;

    return UCG_OK;
}

ucg_status_t ucg_planc_stars_oob_isend(ucg_rank_t vrank, const void *buffer, uint32_t size,
                                       uint16_t tag, ucg_vgroup_t *vgroup,
                                       ucg_planc_stars_p2p_params_t *params,
                                       ucp_datatype_t ucp_dt)
{
    UCG_ASSERT_RET(vrank != UCG_INVALID_RANK, UCG_ERR_INVALID_PARAM);
    UCG_CHECK_NULL_INVALID(buffer, vgroup, params, params->stars_group, params->state);

    ucp_ep_h ep = ucg_planc_stars_oob_get_ucp_ep(vrank, vgroup);
    UCG_ASSERT_RET(ep != NULL, UCG_ERR_NO_RESOURCE);

    ucg_planc_stars_p2p_state_t *state = params->state;
    ucg_group_t *group = params->stars_group->super.super.group;
    uint64_t ucp_tag = ucg_planc_stars_make_tag(tag, group->myrank, group->id);
    ucp_request_param_t req_param = {
        .op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                        UCP_OP_ATTR_FIELD_USER_DATA |
                        UCP_OP_ATTR_FIELD_DATATYPE,
        .cb.send = ucg_planc_stars_oob_isend_done,
        .datatype = ucp_dt,
        .user_data = (void*)state
    };

    ucs_status_ptr_t ucp_req = ucp_tag_send_nbx(ep, buffer, size, ucp_tag, &req_param);
    if (ucg_unlikely(ucp_req == NULL || UCS_PTR_IS_ERR(ucp_req))) {
        return ucg_status_s2g(UCS_PTR_STATUS(ucp_req));
    }

	/* If another thread is executing ucp_worker_progress(), the following is
	   not thread-safe. */
    /* Send is not finished. */
    ++state->inflight_send_cnt;
    return UCG_OK;
}

ucg_status_t ucg_planc_stars_oob_waitall(ucg_planc_stars_p2p_state_t *state)
{
    if (state->inflight_send_cnt == 0 &&
        state->inflight_recv_cnt == 0) {
        return state->status;
    }

    ucp_worker_h ucp_worker = ucg_planc_stars_oob_get_ucp_worker();
    UCG_ASSERT_RET(ucp_worker, UCG_ERR_INVALID_ADDR);

    while (1) {
        /* todo timeout to exit */
        ucp_worker_progress(ucp_worker);
        if (state->inflight_send_cnt == 0 &&
            state->inflight_recv_cnt == 0) {
            return state->status;
        }
    }

    return UCG_INPROGRESS;
}

ucg_planc_ucx_t *planc_ucx = NULL;

ucg_status_t ucg_planc_stars_ucx_instance_init(void)
{
    ucg_planc_t *planc = ucg_planc_get_by_name(PLANC_UCX_NAME);
    if (!planc) {
        ucg_error("Can't get planc ucx in planc stars!");
        return UCG_ERR_NO_RESOURCE;
    }
    planc_ucx = ucg_derived_of(planc, ucg_planc_ucx_t);
    return UCG_OK;
}
