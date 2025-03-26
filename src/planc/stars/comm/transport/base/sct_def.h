/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef STARS_SCT_DEF_H
#define STARS_SCT_DEF_H

#include "scs.h"

#include <stars_interface.h>


#include "offload/stars.h"
#include "offload/cpu.h"
#include "offload/numa.h"
#include "datastruct/queue.h"
#include "datastruct/stack.h"
#include "memory/numa.h"
#include "stats/stats.h"
#include "sys/sys.h"
#include "time/time.h"

#include <uct/api/uct.h>

#ifndef STARS_DEFAULT_ENV_PREFIX
    #define STARS_DEFAULT_ENV_PREFIX          "UCG_PLANC_STARS_"
#endif

#define SCT_STARS_RC_ACC        "ib_stars"
#define SCT_STARS_SDMA_ACC      "sdma_stars"

#define SET_PTR_BY_OFFSET(_ptr, _type, _base, _offset)        (_ptr) = (_type)((char *)(_base) + (_offset))
#define SET_PTR_BY_ADDRESS(_ptr, _type, _base, _offset)       (_ptr) = *(_type **)((char *)(_base) + (_offset))
#define SET_PTR_BY_VOID_ADDRESS(_ptr, _type, _base, _offset)  (_ptr) = (_type)(*(void **)((char *)(_base) + (_offset)))
#define GET_VAL_BY_ADDRESS(_type, _base, _offset)             *(_type *)((char *)(_base) + (_offset))

/**
 * @addtogroup UCT_RESOURCE
 * @{
 */
typedef struct sct_component       *sct_component_h;
typedef struct sct_iface           *sct_iface_h;
typedef struct sct_iface_config    sct_iface_config_t;
typedef struct sct_md_config       sct_md_config_t;
typedef struct sct_ep              *sct_ep_h;
typedef void                       *sct_mem_h;
typedef uintptr_t                  sct_rkey_t;
typedef struct sct_md              *sct_md_h;          /**< @brief Memory domain handler */
typedef struct sct_md_ops          sct_md_ops_t;
typedef struct sct_iface_attr      sct_iface_attr_t;
typedef struct sct_iface_params    sct_iface_params_t;
typedef struct sct_worker          *sct_worker_h;
typedef struct sct_md              sct_md_t;
typedef struct sct_ep_params       sct_ep_params_t;

/*
 * @note This struct is the same as event_id_type_t type.
 */
typedef struct sct_event_id {
    unsigned    event_id;
    uint64_t    offset;
} sct_event_id_t;
typedef sct_event_id_t *sct_event_id_h;

typedef struct sct_event {
    uint8_t         dev_id;         /* stars dev id */
    sct_event_id_t  event_id;
} sct_event_t;
typedef struct sct_event *sct_event_h;

typedef void (*sct_ofd_cb_req_t)(void *context, ucg_status_t status);

KHASH_INIT(sq_ci, uintptr_t, uint32_t, 1,
           kh_int_hash_func, kh_int_hash_equal);
typedef struct sct_ofd_req {
    struct {
        void                *handle;
        struct {
            stars_trans_parm_t head;
            stars_trans_parm_t *tail;
            uint16_t           count;
        } trans_task;
        uint16_t            cqe_cnt;
        uint32_t            task_id;            /* last task id */
        stars_wait_output_t output;
    } stars;

    khash_t(sq_ci)          sq_ci_hash;
    ucs_queue_elem_t        progress;           /* index for progress queue */
    void                    *context;
    sct_ofd_cb_req_t        cb;
} sct_ofd_req_t;
typedef sct_ofd_req_t *sct_ofd_req_h;

static UCS_F_ALWAYS_INLINE
ucg_status_t sct_inc_ofd_sq_ci(sct_ofd_req_h req, uintptr_t key) {
    int ret = 0;
    khiter_t hash_iter = kh_put(sq_ci, &req->sq_ci_hash, key, &ret);
    if (!ret) {
        kh_value(&req->sq_ci_hash, hash_iter)
            = kh_value(&req->sq_ci_hash, hash_iter) + 1;
        return UCG_OK;
    } else if (ret == 1) {
        kh_value(&req->sq_ci_hash, hash_iter) = 1;
        return UCG_OK;
    }
    return UCG_ERR_IO_ERROR;
}

typedef struct sct_iov {
    void            *buffer;        /**< Data buffer */
    size_t          length;         /**< Length of the payload in bytes */
    uct_mem_h       memh;           /**< Local memory key descriptor for the data */
    uint8_t         unnotify;       /* 1 means only write, 0 means support write with notify */
    uint8_t         flag;
    sct_event_h     sct_event;
    uint64_t        remote_addr;
    uct_rkey_t      rkey;
} sct_iov_t;
typedef sct_iov_t *sct_iov_h;

typedef struct sct_wait_elem {
    uint8_t         flag;
    sct_event_h     sct_event;
} sct_wait_elem_t;
typedef sct_wait_elem_t *sct_wait_elem_h;


/**
 * @ingroup UCT_CLIENT_SERVER
 * @brief Callback to handle the disconnection of the remote peer.
 *
 * This callback routine will be invoked on the client and server sides upon
 * a disconnect of the remote peer. It will disconnect the given endpoint from
 * the remote peer.
 * This callback won't be invoked if the endpoint was not connected to the remote
 * peer yet.
 * This callback has to be thread safe.
 * Other than communication progress routines, it is permissible to call other UCT
 * communication routines from this callback.
 *
 * @param [in]  ep               Transport endpoint to disconnect.
 * @param [in]  arg              User argument for this callback as defined in
 *                               @ref uct_ep_params_t::user_data.
 */
typedef void (*sct_ep_disconnect_cb_t)(sct_ep_h ep, void *arg);
#endif