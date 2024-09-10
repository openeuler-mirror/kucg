/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "rc_ep.h"
#include "rc_iface.h"


void sct_rc_ep_cleanup_qp(sct_rc_iface_t *iface, sct_rc_ep_t *ep,
                          sct_rc_ep_cleanup_ctx_t *cleanup_ctx, uint32_t qp_num)
{
    sct_rc_iface_ops_t *ops = ucs_derived_of(iface->super.ops, sct_rc_iface_ops_t);
    sct_ib_md_t *md         = sct_ib_iface_md(&iface->super);
    ucs_status_t status;

    cleanup_ctx->iface     = iface;
    // cleanup_ctx->super.cbq = &iface->super.super.worker->super.progress_q;
    cleanup_ctx->super.cb  = ops->cleanup_qp;

    ucs_list_del(&ep->list);
    ucs_list_add_tail(&iface->ep_gc_list, &cleanup_ctx->list);

    sct_rc_iface_remove_qp(iface, qp_num);

    status = sct_ib_device_async_event_wait(&md->dev,
                                            IBV_EVENT_QP_LAST_WQE_REACHED,
                                            qp_num, &cleanup_ctx->super);
    if (status == UCS_OK) {
        /* event already arrived, finish cleaning up */
        ops->cleanup_qp(&cleanup_ctx->super);
    } else {
        /* deferred cleanup callback was scheduled */
        ucg_assert(status == UCS_INPROGRESS);
    }
}

void sct_rc_ep_cleanup_qp_done(sct_rc_ep_cleanup_ctx_t *cleanup_ctx,
                               uint32_t qp_num)
{
    sct_ib_md_t *md = ucs_derived_of(cleanup_ctx->iface->super.super.md,
                                     sct_ib_md_t);

    sct_ib_device_async_event_unregister(&md->dev,
                                         IBV_EVENT_QP_LAST_WQE_REACHED,
                                         qp_num);
    ucs_list_del(&cleanup_ctx->list);
    ucg_free(cleanup_ctx);
}

UCS_CLASS_INIT_FUNC(sct_rc_ep_t, sct_rc_iface_t *iface, uint32_t qp_num,
                    const sct_ep_params_t *params)
{
    UCS_CLASS_CALL_SUPER_INIT(sct_base_ep_t, &iface->super.super);

    self->txqp.available  = 0;
    self->path_index = SCT_EP_PARAMS_GET_PATH_INDEX(params);
    self->flags      = 0;

    ucs_spin_lock(&iface->eps_lock);
    ucs_list_add_head(&iface->ep_list, &self->list);
    ucs_spin_unlock(&iface->eps_lock);

    ucg_debug("created rc ep %p", self);
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(sct_rc_ep_t)
{
}

UCS_CLASS_DEFINE(sct_rc_ep_t, sct_base_ep_t)

ucs_status_t sct_ep_free_event(sct_event_h event, uint8_t flag)
{
    return scs_stars_free_events(event->dev_id, 1, &event->event_id, flag);
}
