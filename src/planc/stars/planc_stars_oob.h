/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_OOB_H_
#define UCG_PLANC_STARS_OOB_H_

#include "planc_stars_group.h"
#include "planc/ucx/planc_ucx_global.h"

#define PLANC_UCX_NAME "ucx"

extern ucg_planc_ucx_t *planc_ucx;

#define UCG_PLANC_STARS_CHECK_GOTO(_cmd, _op, _label) \
    do { \
        ucg_status_t _status = (_cmd); \
        if (_status != UCG_OK) { \
            (_op)->super.super.status = _status; \
            goto _label; \
        } \
    } while (0)

typedef struct ucg_planc_stars_p2p_req {
    uint8_t free_in_cb;
} ucg_planc_stars_p2p_req_t;

typedef struct ucg_planc_stars_p2p_state {
    /** It's only going to be UCG_OK or UCG_ERR_IO_ERROR */
    volatile ucg_status_t status;
    volatile int inflight_send_cnt;  // must be atomic when ucx mt
    volatile int inflight_recv_cnt;  // must be atomic when ucx mt
} ucg_planc_stars_p2p_state_t;

typedef struct ucg_planc_stars_p2p_params {
    /** The real stars group on which the vgroup depends, can not be NULL. */
    ucg_planc_stars_group_t *stars_group;
    /** Recording isend/irecv state, can not be NULL. */
    ucg_planc_stars_p2p_state_t *state;
} ucg_planc_stars_p2p_params_t;

typedef void* (*ucg_planc_stars_get_ucp_ep_cb_t)(void* arg, void *group, int rank);

typedef void* (*ucg_planc_stars_get_ucp_worker_cb_t)(void* arg);

typedef struct ucg_planc_stars_oob_resource {
    void *arg;
    ucg_planc_stars_get_ucp_worker_cb_t get_ucp_worker;
    ucg_planc_stars_get_ucp_ep_cb_t get_ucp_ep;
} ucg_planc_stars_oob_resource_t;

static inline ucg_planc_ucx_t *ucg_planc_stars_get_ucx_instance(void)
{
    return planc_ucx;
}

static inline const ucp_context_h ucg_planc_stars_get_ucx_ucp_context()
{
    ucg_planc_ucx_export_resource_t *resource
        = &ucg_planc_stars_get_ucx_instance()->export_resource;
    return (ucp_context_h)resource->get_ucp_context(resource->get_planc_context());
}

static inline ucp_ep_h ucg_planc_stars_oob_get_ucp_ep(ucg_rank_t vrank, ucg_vgroup_t *vgroup)
{
    ucg_planc_ucx_export_resource_t *resource = &ucg_planc_stars_get_ucx_instance()->export_resource;
    return (ucp_ep_h)resource->get_ucp_ep(resource->get_planc_context(), vgroup, vrank);
}

static inline ucp_worker_h ucg_planc_stars_oob_get_ucp_worker()
{
    ucg_planc_ucx_export_resource_t *resource = &ucg_planc_stars_get_ucx_instance()->export_resource;
    return (ucp_worker_h)resource->get_ucp_worker(resource->get_planc_context());
}

static inline void ucg_planc_stars_p2p_state_reset(ucg_planc_stars_p2p_state_t *state)
{
    state->status               = UCG_OK;
    state->inflight_send_cnt    = 0;
    state->inflight_recv_cnt    = 0;
}

ucg_status_t ucg_planc_stars_oob_irecv(ucg_rank_t vrank, void *buffer, uint32_t size,
                                       uint16_t tag, ucg_vgroup_t *vgroup,
                                       ucg_planc_stars_p2p_params_t *params,
                                       ucp_datatype_t ucp_dt);

ucg_status_t ucg_planc_stars_oob_isend(ucg_rank_t vrank, const void *buffer, uint32_t size,
                                       uint16_t tag, ucg_vgroup_t *vgroup,
                                       ucg_planc_stars_p2p_params_t *params,
                                       ucp_datatype_t ucp_dt);

ucg_status_t ucg_planc_stars_oob_waitall(ucg_planc_stars_p2p_state_t *state);

ucg_status_t ucg_planc_stars_ucx_instance_init(void);

#endif
