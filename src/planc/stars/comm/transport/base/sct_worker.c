/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "sct_worker.h"


static UCS_CLASS_INIT_FUNC(sct_worker_t)
{
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(sct_worker_t)
{
}

UCS_CLASS_DEFINE(sct_worker_t, void);

static UCS_CLASS_INIT_FUNC(sct_priv_worker_t, pthread_mutex_t *async,
                           ucs_thread_mode_t thread_mode)
{
    UCS_CLASS_CALL_SUPER_INIT(sct_worker_t);

    if (async == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }

    self->async       = async;
    self->thread_mode = thread_mode;
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(sct_priv_worker_t)
{
}

UCS_CLASS_DEFINE(sct_priv_worker_t, sct_worker_t);

UCS_CLASS_DEFINE_NAMED_NEW_FUNC(sct_worker_create, sct_priv_worker_t, sct_worker_t,
                                pthread_mutex_t*, ucs_thread_mode_t)
UCS_CLASS_DEFINE_NAMED_DELETE_FUNC(sct_worker_destroy, sct_priv_worker_t, sct_worker_t)
