/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "sct.h"


ucg_status_t sct_ofd_req_init(sct_ofd_req_h req, void *handle)
{
    req->stars.handle            = handle;
    req->stars.trans_task.head.next = NULL;
    req->stars.trans_task.tail   = &req->stars.trans_task.head;
    req->stars.trans_task.count  = 0;
    req->stars.cqe_cnt           = 0;
    req->stars.task_id           = 0;
    req->stars.output.out_num    = 0;
    req->stars.output.out_pos    = 0;
    req->stars.output.cqe_output = NULL;
    req->cb                      = NULL;
    req->context                 = NULL;
    kh_init_inplace(sq_ci, &req->sq_ci_hash);
    return UCG_OK;
}

void sct_ofd_req_clean(sct_ofd_req_h req)
{
    stars_trans_parm_t *next, *cur =
        req->stars.trans_task.head.next;

    if (req->stars.trans_task.count) {
        for (; cur != NULL; cur = next) {
            next = cur->next;
            ucg_mpool_put(cur->trans_parms);
            ucg_mpool_put(cur);
        }

        req->stars.trans_task.head.next = NULL;
        req->stars.trans_task.count = 0;
        req->stars.trans_task.tail  = &req->stars.trans_task.head;
    }

    void *ptr = req->stars.output.cqe_output;
    if (ucg_unlikely(ptr != NULL)) {
        ucg_free(ptr);
        req->stars.output.cqe_output = NULL;
    }

    req->stars.handle               = NULL;
    req->stars.cqe_cnt              = 0;
    req->stars.task_id              = 0;
    req->stars.output.out_num       = 0;
    req->stars.output.out_pos       = 0;
    req->cb                         = NULL;
    req->context                    = NULL;
    kh_destroy_inplace(sq_ci, &req->sq_ci_hash);
}
