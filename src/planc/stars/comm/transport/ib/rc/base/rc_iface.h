/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_RC_IFACE_H
#define SCT_RC_IFACE_H

#include "rc_def.h"

#include "sct_iface.h"
#include "ib_iface.h"


#define SCT_RC_QP_TABLE_ORDER       12
#define SCT_RC_QP_TABLE_SIZE        UCS_BIT(SCT_RC_QP_TABLE_ORDER)
#define SCT_RC_QP_TABLE_MEMB_ORDER  (SCT_IB_QPN_ORDER - SCT_RC_QP_TABLE_ORDER)
#define SCT_RC_QP_MAX_RETRY_COUNT   7

enum {
    UCT_RC_IFACE_STAT_RX_COMPLETION,
    UCT_RC_IFACE_STAT_TX_COMPLETION,
    UCT_RC_IFACE_STAT_NO_CQE,
    UCT_RC_IFACE_STAT_NO_READS,
    UCT_RC_IFACE_STAT_LAST
};

/* Common configuration used for rc verbs, rcx and dc transports */
typedef struct sct_rc_iface_common_config {
    sct_ib_iface_config_t    super;

    struct {
        double               timeout;
        unsigned             retry_count;
        double               rnr_timeout;
        unsigned             rnr_retry_count;
        size_t               max_get_bytes;
    } tx;
} sct_rc_iface_common_config_t;


/* RC specific configuration used for rc verbs and rcx transports only */
struct sct_rc_iface_config {
    sct_rc_iface_common_config_t   super;
    unsigned                       tx_cq_moderation; /* How many TX messages are
                                                        batched to one CQE */
    unsigned                       tx_cq_len;
};


typedef struct sct_rc_iface_ops {
    sct_ib_iface_ops_t   super;
    void (*cleanup_qp)(sct_ib_async_event_wait_t *cleanup_ctx);
} sct_rc_iface_ops_t;


struct sct_rc_iface {
    sct_ib_iface_t              super;

    struct {
        /* Credits for completions.
         * May be negative in case mlx5 because we take "num_bb" credits per
         * post to be able to calculate credits of outstanding ops on failure.
         * In case of verbs TL we use QWE number, so 1 post always takes 1
         * credit */
        signed                  cq_available;
    } tx;

    struct {
        unsigned             tx_qp_len;
        unsigned             tx_min_inline;
        unsigned             tx_ops_count;
        uint16_t             tx_moderation;

        uint8_t              min_rnr_timer;
        uint8_t              timeout;
        uint8_t              rnr_retry;
        uint8_t              retry_cnt;
    } config;

    ucs_spinlock_t           eps_lock; /* common lock for eps and ep_list */
    sct_rc_ep_t              **eps[SCT_RC_QP_TABLE_SIZE];
    ucs_list_link_t          ep_list;
    ucs_list_link_t          ep_gc_list;
};
UCS_CLASS_DECLARE(sct_rc_iface_t, sct_rc_iface_ops_t*, sct_md_h, sct_worker_h,
                  const sct_iface_params_t*, const sct_rc_iface_common_config_t*,
                  sct_ib_iface_init_attr_t*);


extern ucs_config_field_t sct_rc_iface_config_table[];
extern ucs_config_field_t sct_rc_iface_common_config_table[];

ucs_status_t sct_rc_iface_query(sct_rc_iface_t *iface, sct_iface_attr_t *iface_attr);

void sct_rc_iface_add_qp(sct_rc_iface_t *iface, sct_rc_ep_t *ep,
                         unsigned qp_num);

void sct_rc_iface_remove_qp(sct_rc_iface_t *iface, unsigned qp_num);

void sct_rc_iface_cleanup_eps(sct_rc_iface_t *iface);

ucs_status_t sct_rc_iface_qp_init(sct_rc_iface_t *iface, struct ibv_qp *qp);

ucs_status_t sct_rc_iface_qp_connect(sct_rc_iface_t *iface, struct ibv_qp *qp,
                                     const uint32_t qp_num,
                                     struct ibv_ah_attr *ah_attr,
                                     enum ibv_mtu path_mtu);

static UCS_F_ALWAYS_INLINE int
sct_rc_iface_have_tx_cqe_avail(sct_rc_iface_t* iface)
{
    return iface->tx.cq_available > 0;
}

#endif
