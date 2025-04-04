/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */

#include "planc_ucx_p2p.h"

#include "planc_ucx_global.h"
#include "planc_ucx_context.h"
#include "planc_ucx_group.h"

#include "core/ucg_group.h"
#include "core/ucg_rank_map.h"

#include "util/ucg_malloc.h"
#include "util/ucg_log.h"
#include "util/ucg_helper.h"

static ucp_tag_t ucg_planc_ucx_make_tag(int tag, ucg_rank_t rank,
                                        uint32_t group_id)
{
    return ((((uint64_t)(tag)) << UCG_P2P_SEQ_BITS_OFFSET) |
            (((uint64_t)(rank)) << UCG_P2P_RANK_BITS) |
            (((uint64_t)(UCG_P2P_FLAG_ONE)) << UCG_P2P_FLAG_BITS_OFFSET) |
            (((uint64_t)(group_id)) << UCG_P2P_ID_BITS_OFFSET));
}

static void *ucg_planc_ucx_p2p_start_pack(void *context, const void *buffer,
                                          size_t count)
{
    ucg_dt_t *dt = (ucg_dt_t*)context;
    return (void*)ucg_dt_start_pack(buffer, dt, count);
}

static void *ucg_planc_ucx_p2p_start_unpack(void *context, void *buffer,
                                            size_t count)
{
    ucg_dt_t *dt = (ucg_dt_t*)context;
    return (void*)ucg_dt_start_unpack(buffer, dt, count);
}

static size_t ucg_planc_ucx_p2p_packed_size(void *state)
{
    ucg_dt_state_t *dt_state = (ucg_dt_state_t*)state;
    return (size_t)ucg_dt_packed_size(dt_state);
}

static size_t ucg_planc_ucx_p2p_pack(void *state, size_t offset, void *dest,
                                     size_t max_length)
{
    ucg_dt_state_t *dt_state = (ucg_dt_state_t*)state;
    uint64_t len = max_length;
    ucg_status_t status = ucg_dt_pack(dt_state, offset, dest, &len);
    if (status != UCG_OK) {
        return 0;
    }
    return len;
}

static ucs_status_t ucg_planc_ucx_p2p_unpack(void *state, size_t offset,
                                             const void *src, size_t length)
{
    ucg_dt_state_t *dt_state = (ucg_dt_state_t*)state;
    uint64_t len = length;
    ucg_status_t status = ucg_dt_unpack(dt_state, offset, src, &len);
    if (status != UCG_OK) {
        return UCS_ERR_NO_RESOURCE;
    }
    if (len != length) {
        return UCS_ERR_BUFFER_TOO_SMALL;
    }
    return UCS_OK;
}

static void ucg_planc_ucx_p2p_finish(void *state)
{
    ucg_dt_state_t *dt_state = (ucg_dt_state_t*)state;
    ucg_dt_finish(dt_state);
    return;
}

static ucp_generic_dt_ops_t p2p_ucp_dt_ops = {
    .start_pack = ucg_planc_ucx_p2p_start_pack,
    .start_unpack = ucg_planc_ucx_p2p_start_unpack,
    .packed_size = ucg_planc_ucx_p2p_packed_size,
    .pack = ucg_planc_ucx_p2p_pack,
    .unpack = ucg_planc_ucx_p2p_unpack,
    .finish = ucg_planc_ucx_p2p_finish,
};

static ucg_status_t ucg_planc_ucx_p2p_get_ucp_dt(ucg_dt_t *dt,
                                                 ucp_datatype_t *ucp_dt)
{
    UCG_STATIC_ASSERT(sizeof(ucp_datatype_t) == sizeof(uint64_t));
    *ucp_dt = ucg_dt_opaque_obj(dt);
    if (*ucp_dt != 0) {
        return UCG_OK;
    }

     ucg_dt_opaque_t opaque;
    if (ucg_dt_is_contiguous(dt)) {
        *ucp_dt = ucp_dt_make_contig(ucg_dt_size(dt));
        opaque.obj = *ucp_dt;
        opaque.destroy = NULL;
    } else {
        ucs_status_t status = ucp_dt_create_generic(&p2p_ucp_dt_ops, (void*)dt, ucp_dt);
        if (status != UCS_OK) {
            return ucg_status_s2g(status);
        }
        opaque.obj = (uint64_t)*ucp_dt;
        opaque.destroy = ucp_dt_destroy;
    }
    ucg_dt_set_opaque(dt, &opaque);
    return UCG_OK;
}

static ucp_ep_h ucg_planc_ucx_p2p_get_ucp_ep(ucg_vgroup_t *vgroup, ucg_rank_t vrank,
                                             ucg_planc_ucx_group_t *ucx_group)
{
    ucg_rank_t group_rank = ucg_rank_map_eval(&vgroup->rank_map, vrank);
    ucg_rank_t ctx_rank = ucg_group_get_ctx_rank(vgroup->group, group_rank);
    ucg_planc_ucx_context_t *ucx_context = ucx_group->context;

    if (ucx_context->eps[ctx_rank] != NULL) {
        return ucx_context->eps[ctx_rank];
    }

    ucp_ep_h ep = NULL;
    if (ucx_context->config.use_oob == UCG_YES) {
        void *group = vgroup->group->oob_group.group;
        ep = ucg_planc_ucx_get_oob_ucp_ep(group, group_rank);
    } else {
        ucg_context_t *context = vgroup->group->context;
        ucg_planc_ucx_t *planc_ucx = ucg_planc_ucx_instance();
        ucg_proc_info_t *proc_info = NULL;
        void *ucp_addr = ucg_context_get_proc_addr(context, ctx_rank, &planc_ucx->super, &proc_info);
        ucp_ep_params_t params = {
            .field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                          UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE,
            .address = (ucp_address_t*)ucp_addr,
        };
        ucs_status_t status = ucp_ep_create(ucx_context->ucp_worker, &params, &ep);
        ucg_free_proc_info(proc_info);
        if (status != UCS_OK) {
            ucg_error("Failed to create ucp ep, %s", ucs_status_string(status));
            return NULL;
        }
    }

    ucx_context->eps[ctx_rank] = ep;
    return ep;
}

static void ucg_planc_ucx_p2p_close_ep(ucp_ep_h ep, ucp_worker_h ucp_worker)
{
    ucs_status_t status;
    // ucs_status_ptr_t close_req = ucp_ep_close_nb(ep, UCP_EP_CLOSE_MODE_FLUSH);
    /**
     * The peer process may have exited when sending the disconnection request.
     * Therefore, the process is hung in @ref ucp_worker_progress.
     * In this example, @ref ucp_ep_close_nb is not performed.
     */
    ucs_status_ptr_t close_req = NULL;
    if (UCS_PTR_IS_PTR(close_req)) {
        do {
            ucp_worker_progress(ucp_worker);
            status = ucp_request_check_status(close_req);
        } while (status == UCS_INPROGRESS);
        ucp_request_free(close_req);
    } else {
        status = UCS_PTR_STATUS(close_req);
    }
    if (status != UCS_OK) {
        ucg_error("Failed to close ucp ep, ep %p, status %s",
                  ep, ucs_status_string(status));
    }
    return;
}

void ucg_planc_ucx_p2p_close_all_ep(ucg_planc_ucx_context_t *context)
{
    int oob_group_size = context->ucg_context->oob_group.size;
    for (int i = 0; i < oob_group_size; ++i) {
        if (context->eps[i] != NULL) {
            ucg_planc_ucx_p2p_close_ep(context->eps[i], context->ucp_worker);
        }
    }
    return;
}

static void ucg_planc_ucx_p2p_isend_done(void *request, ucs_status_t status,
                                         void *user_data)
{
    ucg_planc_ucx_p2p_state_t *state = (ucg_planc_ucx_p2p_state_t*)user_data;
    if (status != UCS_OK) {
        ucg_error("Failed to send, %s", ucs_status_string(status));
        state->status = UCG_ERR_IO_ERROR;
    }
    --state->inflight_send_cnt;
    ucg_planc_ucx_p2p_req_t *req = (ucg_planc_ucx_p2p_req_t*)request;
    if (req->free_in_cb) {
        ucg_planc_ucx_p2p_req_free(request);
    }
    return;
}

static void ucg_planc_ucx_p2p_irecv_done(void *request, ucs_status_t status,
                                         const ucp_tag_recv_info_t *info,
                                         void *user_data)
{
    ucg_planc_ucx_p2p_state_t *state = (ucg_planc_ucx_p2p_state_t*)user_data;
    if (status != UCS_OK) {
        ucg_error("Failed to receive, %s", ucs_status_string(status));
        state->status = UCG_ERR_IO_ERROR;
    }
    --state->inflight_recv_cnt;
    ucg_planc_ucx_p2p_req_t *req = (ucg_planc_ucx_p2p_req_t*)request;
    if (req->free_in_cb) {
        ucg_planc_ucx_p2p_req_free(request);
    }
    return;
}

ucg_status_t ucg_planc_ucx_p2p_isend(const void *buffer, int32_t count,
                                     ucg_dt_t *dt, ucg_rank_t vrank,
                                     int tag, ucg_vgroup_t *vgroup,
                                     ucg_planc_ucx_p2p_params_t *params)
{
    UCG_CHECK_NULL_INVALID(dt, vgroup, params, params->ucx_group, params->state);
    if (vrank == UCG_INVALID_RANK) {
        return UCG_ERR_INVALID_PARAM;
    }

    ucg_status_t status;
    ucp_datatype_t ucp_dt;
    status = ucg_planc_ucx_p2p_get_ucp_dt(dt, &ucp_dt);
    if (status != UCG_OK) {
        return status;
    }

    ucp_ep_h ep = ucg_planc_ucx_p2p_get_ucp_ep(vgroup, vrank, params->ucx_group);
    if (ep == NULL) {
        return UCG_ERR_NO_RESOURCE;
    }

    ucg_planc_ucx_p2p_state_t *state = params->state;
    ucg_group_t *group = params->ucx_group->super.super.group;
    uint64_t ucp_tag = ucg_planc_ucx_make_tag(tag, group->myrank, group->id);
    ucp_request_param_t req_param = {
        .op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                        UCP_OP_ATTR_FIELD_DATATYPE |
                        UCP_OP_ATTR_FIELD_USER_DATA,
        .datatype = ucp_dt,
        .cb.send = ucg_planc_ucx_p2p_isend_done,
        .user_data = (void*)state,
    };
    ucg_debug("isend: %d to %d, tag 0x%lX, count %d, size %ld, extent %ld",
              group->myrank, ucg_rank_map_eval(&vgroup->rank_map, vrank),
              ucp_tag, count, ucg_dt_size(dt), ucg_dt_extent(dt));
    ucs_status_ptr_t ucp_req = ucp_tag_send_nbx(ep, buffer, count, ucp_tag, &req_param);
    if (ucp_req == NULL || UCS_PTR_IS_ERR(ucp_req)) {
        return ucg_status_s2g(UCS_PTR_STATUS(ucp_req));
    }
    /* If another thread is executing ucp_worker_progress(), the following is
       not thread-safe. */

    /* Send is not finished. */
    ((ucg_planc_ucx_p2p_req_t*)ucp_req)->free_in_cb = 1;
    ++state->inflight_send_cnt;
    /**
     * In some cases, ucp_tag_send_nbx() may return ucp_request pointer
     * instead of UCS_OK when the request is completed.
     * Here the status of ucp_request should be checked again.
     */
    ucs_status_t req_status = ucp_request_check_status(ucp_req);
    if (req_status != UCS_INPROGRESS) {
        ucg_planc_ucx_p2p_req_free(ucp_req);
    }
    if (params->request != NULL) {
        ucg_planc_ucx_p2p_req_t **req = params->request;
        if (req_status == UCS_INPROGRESS)  {
            *req = (ucg_planc_ucx_p2p_req_t*)ucp_req;
            (*req)->free_in_cb = 0;
        } else {
            *req = NULL;
        }
    }
    return UCG_OK;
}

void *ucg_planc_ucx_get_ucp_ep(void *arg, void *group, int rank)
{
    ucg_planc_ucx_context_t *ucx_context = (ucg_planc_ucx_context_t *)arg;
    ucg_vgroup_t *vgroup = (ucg_vgroup_t *)group;
    ucg_rank_t vrank = (ucg_rank_t)rank;
    ucg_rank_t group_rank = ucg_rank_map_eval(&vgroup->rank_map, vrank);
    ucg_rank_t ctx_rank = ucg_group_get_ctx_rank(vgroup->group, group_rank);
    ucg_context_t *context = vgroup->group->context;
    ucg_planc_ucx_t *planc_ucx = ucg_planc_ucx_instance();
    ucg_proc_info_t *proc_info = NULL;
    ucp_ep_h ep = NULL;

    if (ucx_context->config.use_oob == UCG_YES) {
        void *group = vgroup->group->oob_group.group;
        return ucg_planc_ucx_get_oob_ucp_ep(group, group_rank);
    }

    if (ucx_context->eps[ctx_rank] != NULL) {
        return ucx_context->eps[ctx_rank];
    }

    void *ucp_addr = ucg_context_get_proc_addr(context, ctx_rank, &planc_ucx->super, &proc_info);
    if (!ucp_addr) {
        ucg_error("Failed to get ucp addr for proc %d!", ctx_rank);
        goto free_proc_info;
    }

    ucp_ep_params_t params = {
            .field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS,
            .address = (ucp_address_t*)ucp_addr
    };

    ucs_status_t status = ucp_ep_create(ucx_context->ucp_worker, &params, &ep);
    if (status != UCS_OK) {
        ucg_error("Failed to create ucp ep, %s", ucs_status_string(status));
        goto free_proc_info;
    }

    ucx_context->eps[ctx_rank] = ep;
free_proc_info:
    ucg_free_proc_info(proc_info);
    return ep;
}

ucg_status_t ucg_planc_ucx_p2p_irecv(void *buffer, int32_t count,
                                     ucg_dt_t *dt, ucg_rank_t vrank,
                                     int tag, ucg_vgroup_t *vgroup,
                                     ucg_planc_ucx_p2p_params_t *params)
{
    UCG_CHECK_NULL_INVALID(dt, vgroup, params, params->ucx_group, params->state);
    if (vrank == UCG_INVALID_RANK) {
        return UCG_ERR_INVALID_PARAM;
    }

    ucg_status_t status;
    ucp_datatype_t ucp_dt;
    status = ucg_planc_ucx_p2p_get_ucp_dt(dt, &ucp_dt);
    if (status != UCG_OK) {
        return status;
    }

    ucg_planc_ucx_p2p_state_t *state = params->state;
    ucg_rank_t sender_group_rank = ucg_rank_map_eval(&vgroup->rank_map, vrank);
    ucg_group_t *group = vgroup->group;
    uint64_t ucp_tag = ucg_planc_ucx_make_tag(tag, sender_group_rank, group->id);
    ucp_request_param_t req_param = {
        .op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                        UCP_OP_ATTR_FIELD_DATATYPE |
                        UCP_OP_ATTR_FIELD_USER_DATA,
        .datatype = ucp_dt,
        .cb.recv = ucg_planc_ucx_p2p_irecv_done,
        .user_data = (void*)state,
    };
    ucg_debug("irecv: %d to %d, tag 0x%lX, count %d, size %ld, extent %ld",
              sender_group_rank, group->myrank, ucp_tag, count, ucg_dt_size(dt),
              ucg_dt_extent(dt));
    ucg_planc_ucx_context_t *context = params->ucx_group->context;
    ucp_worker_h ucp_worker = ucg_planc_ucx_context_get_worker(context);
    if (ucg_unlikely(ucp_worker == NULL)) {
        return UCG_ERR_INVALID_PARAM;
    }
    ucs_status_ptr_t ucp_req = ucp_tag_recv_nbx(ucp_worker, buffer, count, ucp_tag,
                                                UCG_P2P_TAG_MASK, &req_param);
    if (ucp_req == NULL || UCS_PTR_IS_ERR(ucp_req)) {
        return ucg_status_s2g(UCS_PTR_STATUS(ucp_req));
    }
    /* If another thread is executing ucp_worker_progress(), the following is
       not thread-safe. */

    /* Receive is not finished. */
    ++state->inflight_recv_cnt;
    ((ucg_planc_ucx_p2p_req_t*)ucp_req)->free_in_cb = 1;
    /**
     * In some cases, ucp_tag_recv_nbx() may return ucp_request pointer
     * instead of UCS_OK when the request is completed.
     * Here the status of ucp_request should be checked again.
     */
    ucs_status_t req_status = ucp_request_check_status(ucp_req);
    if (req_status != UCS_INPROGRESS) {
        ucg_planc_ucx_p2p_req_free(ucp_req);
    }
    if (params->request != NULL) {
        ucg_planc_ucx_p2p_req_t **req = params->request;
        if (req_status == UCS_INPROGRESS)  {
            *req = (ucg_planc_ucx_p2p_req_t*)ucp_req;
            (*req)->free_in_cb = 0;
        } else {
            *req = NULL;
        }
    }

    return UCG_OK;
}

ucg_status_t ucg_planc_ucx_p2p_test(ucg_planc_ucx_group_t *ucx_group,
                                    ucg_planc_ucx_p2p_req_t **req)
{
    if (*req == NULL) {
        return UCG_OK;
    }

    ucg_planc_ucx_context_t *context = ucx_group->context;
    ucp_worker_h ucp_worker = ucg_planc_ucx_context_get_worker(context);
    if (ucg_unlikely(ucp_worker == NULL)) {
        return UCG_ERR_INVALID_PARAM;
    }
    int polls = 0;
    int n_polls = context->config.n_polls;
    while (polls++ < n_polls) {
        ucs_status_t status = ucp_request_check_status(*req);
        if (status != UCS_INPROGRESS) {
            ucg_planc_ucx_p2p_req_free(*req);
            *req = NULL;
            return ucg_status_s2g(status);
        }
        ucp_worker_progress(ucp_worker);
    }
    return UCG_INPROGRESS;
}

ucg_status_t ucg_planc_ucx_p2p_testall(ucg_planc_ucx_group_t *ucx_group,
                                       ucg_planc_ucx_p2p_state_t *state)
{
    if (state->inflight_send_cnt == 0 && state->inflight_recv_cnt == 0) {
        return state->status;
    }

    ucg_planc_ucx_context_t *context = ucx_group->context;
    ucp_worker_h ucp_worker = ucg_planc_ucx_context_get_worker(context);
    if (ucg_unlikely(ucp_worker == NULL)) {
        return UCG_ERR_INVALID_PARAM;
    }
    int polls = 0;
    int n_polls = context->config.n_polls;
    while (polls++ < n_polls) {
        ucp_worker_progress(ucp_worker);
        if (state->inflight_send_cnt == 0 && state->inflight_recv_cnt == 0) {
            return state->status;
        }
    }
    return UCG_INPROGRESS;
}

void ucg_planc_ucx_p2p_req_init(void *request)
{
    ucg_planc_ucx_p2p_req_t *req = (ucg_planc_ucx_p2p_req_t*)request;
    req->free_in_cb = 0;
    return;
}
