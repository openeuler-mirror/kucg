/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_RC_OFFLOAD_IFACE_H_
#define SCT_RC_OFFLOAD_IFACE_H_

#include "rc_offload_ep.h"

typedef struct sct_rc_ofd_iface_config {
    sct_rc_iface_config_t       super;
    unsigned                    tx_max_wr;
} sct_rc_ofd_iface_config_t;

typedef struct sct_rc_ofd_iface {
    sct_rc_iface_t              super;
    struct {
        size_t                  max_send_sge;
        unsigned                tx_max_wr;
    } config;

    uint64_t                    id;
    ucs_queue_head_t            req_queue;
    ucs_spinlock_t              spinlock;
    int                         wait_flag;
} sct_rc_ofd_iface_t;

ucs_status_t sct_rc_ofd_iface_qp_create(sct_rc_ofd_iface_t *iface, struct ibv_qp **qp_p,
                                        sct_ib_qp_attr_t *attr, unsigned max_send_wr);

#endif