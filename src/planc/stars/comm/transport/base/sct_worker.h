/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_WORKER_H_
#define SCT_WORKER_H_

#include "sct.h"


BEGIN_C_DECLS

typedef struct sct_priv_worker {
    sct_worker_t            super;
    pthread_mutex_t         *async;
    ucs_thread_mode_t       thread_mode;
} sct_priv_worker_t;

END_C_DECLS

#endif
