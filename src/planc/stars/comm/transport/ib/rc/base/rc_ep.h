/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_RC_EP_H
#define SCT_RC_EP_H

#include "rc_iface.h"


#define RC_UNSIGNALED_INF UINT16_MAX

/*
 * Auxillary AM ID bits used by FC protocol.
 */
enum {
    /* Keepalive Request scheduled: indicates that keepalive request
     * is scheduled in pending queue and no more keepalive actions
     * are needed */
    UCT_RC_EP_FLAG_KEEPALIVE_PENDING = UCS_BIT(0),

    /* EP is connected to peer */
    UCT_RC_EP_FLAG_CONNECTED         = UCS_BIT(1),

    /* Flush cancel was executed on EP */
    UCT_RC_EP_FLAG_FLUSH_CANCEL      = UCS_BIT(2),

    /* Error handler already called or flush(CANCEL) disabled it */
    UCT_RC_EP_FLAG_NO_ERR_HANDLER    = UCS_BIT(3)
};

/*
 * Check for send resources
 */
#define SCT_RC_CHECK_CQE_RET(_iface, _ep, _ret) \
    /* tx_moderation == 0 for TLs which don't support it */ \
    /* Will check cqe available when available cqe number less than tx moderation number. */ \
    if (ucs_unlikely((_iface)->tx.cq_available <= \
        (signed)(_iface)->config.tx_moderation)) { \
        if (!sct_rc_iface_have_tx_cqe_avail(_iface)) { \
            return _ret; \
        } \
    }

#define SCT_RC_CHECK_TXQP_RET(_iface, _ep, _ret) \
    if (sct_rc_txqp_available(&(_ep)->txqp) <= 0) { \
        return _ret; \
    }

#define SCT_RC_CHECK_TX_CQ_RES(_iface, _ep) \
    SCT_RC_CHECK_CQE_RET(_iface, _ep, UCS_ERR_NO_RESOURCE) \
    SCT_RC_CHECK_TXQP_RET(_iface, _ep, UCS_ERR_NO_RESOURCE)

/**
 * All operations are not allowed if no RDMA_READ credits. Otherwise operations
 * ordering can be broken. If some AM sends added to the pending queue after
 * RDMA_READ operation, it may be stuck there until RDMA_READ credits arrive,
 * therefore need to block even AM sends, until all resources are available.
 */
#define SCT_RC_CHECK_RES(_iface, _ep) \
    do { \
    ucg_assert((_ep)->flags & UCT_RC_EP_FLAG_CONNECTED); \
    SCT_RC_CHECK_TX_CQ_RES(_iface, _ep) \
    } while (0)

/* this is a common type for all rc and dc transports */
struct sct_rc_txqp {
    int16_t             available;
};

struct sct_rc_ep {
    sct_base_ep_t       super;
    sct_rc_txqp_t       txqp;
    ucs_list_link_t     list;
    uint8_t             path_index;
    uint8_t             flags;
};


/* EP QP TX cleanup context */
typedef struct {
    sct_ib_async_event_wait_t super;      /* LAST_WQE event callback */
    ucs_list_link_t           list;       /* entry in interface ep_gc_list */
    sct_rc_iface_t            *iface;     /* interface */
} sct_rc_ep_cleanup_ctx_t;


UCS_CLASS_DECLARE(sct_rc_ep_t, sct_rc_iface_t*, uint32_t, const sct_ep_params_t*);

void sct_rc_ep_cleanup_qp(sct_rc_iface_t *iface, sct_rc_ep_t *ep,
                          sct_rc_ep_cleanup_ctx_t *cleanup_ctx, uint32_t qp_num);

void sct_rc_ep_cleanup_qp_done(sct_rc_ep_cleanup_ctx_t *cleanup_ctx,
                               uint32_t qp_num);

static inline int16_t sct_rc_txqp_available(sct_rc_txqp_t *txqp)
{
    return txqp->available;
}

static inline void sct_rc_txqp_available_add(sct_rc_txqp_t *txqp, int16_t val)
{
    txqp->available += val;
}

static inline void sct_rc_txqp_available_set(sct_rc_txqp_t *txqp, int16_t val)
{
    txqp->available = val;
}

static UCS_F_ALWAYS_INLINE int
sct_rc_ep_has_tx_resources(sct_rc_ep_t *ep)
{
    return (sct_rc_txqp_available(&ep->txqp) > 0);
}

#endif
