/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include "allgatherv.h"
#include "planc_ucx_plan.h"

enum {
    UCG_ALLGATHERV_BRUCK_SEND = UCG_BIT(0),
    UCG_ALLGATHERV_BRUCK_RECV = UCG_BIT(1),
};

#define UCG_ALLGATHERV_BRUCK_FLAGS UCG_ALLGATHERV_BRUCK_SEND | UCG_ALLGATHERV_BRUCK_RECV

/**
 * @brief Bruck algorithm for allgatherv with O(log(N)) steps.
 *
 * Example on 7 processes:
 *   Initial set up:
 *    #     0      1      2      3      4      5      6
 *         [0]    [ ]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [1]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [2]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [3]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [4]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [ ]    [5]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [ ]    [ ]    [6]
 *   Step 0: send message to (rank - 2^0), receive message from (rank + 2^0)
 *    #     0      1      2      3      4      5      6
 *         [0]    [ ]    [ ]    [ ]    [ ]    [ ]    [0]
 *         [1]    [1]    [ ]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [2]    [2]    [ ]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [3]    [3]    [ ]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [4]    [4]    [ ]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [5]    [5]    [ ]
 *         [ ]    [ ]    [ ]    [ ]    [ ]    [6]    [6]
 *   Step 1: send message to (rank - 2^1), receive message from (rank + 2^1).
 *           message contains all blocks from (rank) .. (rank + 2^2) with
 *           wrap around.
 *    #     0      1      2      3      4      5      6
 *         [0]    [ ]    [ ]    [ ]    [0]    [0]    [0]
 *         [1]    [1]    [ ]    [ ]    [ ]    [1]    [1]
 *         [2]    [2]    [2]    [ ]    [ ]    [ ]    [2]
 *         [3]    [3]    [3]    [3]    [ ]    [ ]    [ ]
 *         [ ]    [4]    [4]    [4]    [4]    [ ]    [ ]
 *         [ ]    [ ]    [5]    [5]    [5]    [5]    [ ]
 *         [ ]    [ ]    [ ]    [6]    [6]    [6]    [6]
 *   Step 2: send message to (rank - 2^2), receive message from (rank + 2^2).
 *           message size is "all remaining blocks"
 *    #     0      1      2      3      4      5      6
 *         [0]    [0]    [0]    [0]    [0]    [0]    [0]
 *         [1]    [1]    [1]    [1]    [1]    [1]    [1]
 *         [2]    [2]    [2]    [2]    [2]    [2]    [2]
 *         [3]    [3]    [3]    [3]    [3]    [3]    [3]
 *         [4]    [4]    [4]    [4]    [4]    [4]    [4]
 *         [5]    [5]    [5]    [5]    [5]    [5]    [5]
 *         [6]    [6]    [6]    [6]    [6]    [6]    [6]
 */
static ucg_status_t ucg_planc_ucx_allgatherv_bruck_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status = UCG_OK;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    ucg_rank_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);
    int32_t *new_rcounts = op->allgatherv.bruck.new_cnt_displs;
    int32_t *new_rdispls = new_rcounts + group_size;
    int32_t *new_scounts = new_rdispls + group_size;
    int32_t *new_sdispls = new_scounts + group_size;
    int32_t *merged_rcounts = op->allgatherv.bruck.merged_cnt_displs;
    int32_t *merged_rdispls = merged_rcounts + group_size;
    int32_t *merged_scounts = merged_rdispls + group_size;
    int32_t *merged_sdispls = merged_scounts + group_size;

    int *distance = &op->allgatherv.bruck.distance;
    while (*distance < group_size) {
        if (ucg_test_and_clear_flags(&op->flags, UCG_ALLGATHERV_BRUCK_FLAGS))  {
            int blockcount = *distance <= (group_size >> 1) ?
                             *distance : group_size - *distance;
            ucg_rank_t recvfrom = (myrank + *distance) % group_size;
            ucg_rank_t sendto = (myrank - *distance + group_size) % group_size;
            for (int i = 0; i < blockcount; ++i) {
                int tmp_srank = (myrank + i) % group_size;
                new_scounts[i] = args->recvcounts[tmp_srank];
                new_sdispls[i] = args->displs[tmp_srank];
                int tmp_rrank = (recvfrom + i) % group_size;
                new_rcounts[i] = args->recvcounts[tmp_rrank];
                new_rdispls[i] = args->displs[tmp_rrank];
            }

            /* optimization that decreasing isend/irecv count */
            int send_cnt = 0, recv_cnt = 0;
            merged_sdispls[send_cnt] = new_sdispls[0];
            merged_scounts[send_cnt] = new_scounts[0];
            merged_rdispls[recv_cnt] = new_rdispls[0];
            merged_rcounts[recv_cnt] = new_rcounts[0];
            for (int i = 1; i < blockcount; ++i) {
                if (new_sdispls[i] == new_sdispls[i - 1] + new_scounts[i - 1]) {
                    merged_scounts[send_cnt] += new_scounts[i];
                } else {
                    ++send_cnt;
                    merged_sdispls[send_cnt] = new_sdispls[i];
                    merged_scounts[send_cnt] = new_scounts[i];
                }
                if (new_rdispls[i] == new_rdispls[i - 1] + new_rcounts[i - 1]) {
                    merged_rcounts[recv_cnt] += new_rcounts[i];
                } else {
                    ++recv_cnt;
                    merged_rdispls[recv_cnt] = new_rdispls[i];
                    merged_rcounts[recv_cnt] = new_rcounts[i];
                }
            }

            for (int i = 0; i <= send_cnt; ++i) {
                void *sendbuf = args->recvbuf + merged_sdispls[i] * ucg_dt_extent(args->sendtype);
                status = ucg_planc_ucx_p2p_isend(sendbuf, merged_scounts[i],
                                                 args->sendtype, sendto, op->tag,
                                                 vgroup, &params);
                UCG_CHECK_ERR_GOTO(status, out);
            }
            for (int i = 0; i <= recv_cnt; ++i) {
                void *recvbuf = args->recvbuf + merged_rdispls[i] * ucg_dt_extent(args->recvtype);
                status = ucg_planc_ucx_p2p_irecv(recvbuf, merged_rcounts[i],
                                                 args->recvtype, recvfrom, op->tag,
                                                 vgroup, &params);
                UCG_CHECK_ERR_GOTO(status, out);
            }
        }

        status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
        UCG_CHECK_GOTO(status, out);
        op->flags |= UCG_ALLGATHERV_BRUCK_FLAGS;
        op->allgatherv.bruck.distance <<= 1;
    }

out:
    op->super.super.status = status;
    return status;
}

static inline void ucg_planc_ucx_allgatherv_bruck_op_reset(ucg_planc_ucx_op_t *op)
{
    ucg_planc_ucx_op_reset(op);
    op->flags = UCG_ALLGATHERV_BRUCK_FLAGS;
    op->allgatherv.bruck.distance = 1;
    return;
}

static ucg_status_t ucg_planc_ucx_allgatherv_bruck_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_allgatherv_bruck_op_reset(op);

    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    const void *sendbuf = args->sendbuf;
    if (sendbuf != UCG_IN_PLACE) {
        ucg_rank_t myrank = op->super.vgroup->myrank;
        int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
        void *recvbuf = args->recvbuf + args->displs[myrank] * recvtype_extent;
        status = ucg_dt_memcpy(recvbuf, args->recvcounts[myrank], args->recvtype,
                               sendbuf, args->sendcount, args->sendtype);
        UCG_CHECK_GOTO(status, out);
    }

    status = ucg_planc_ucx_allgatherv_bruck_op_progress(ucg_op);
out:
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_bruck_op_init(ucg_planc_ucx_op_t *ucg_op)
{
    ucg_vgroup_t *vgroup = ucg_op->super.vgroup;
    uint32_t group_size = vgroup->size;

    ucg_op->allgatherv.bruck.new_cnt_displs = ucg_calloc(4 * group_size, sizeof(int32_t),
                                                         "alloc new count displs");
    if (ucg_op->allgatherv.bruck.new_cnt_displs == NULL) {
        goto err;
    }
    ucg_op->allgatherv.bruck.merged_cnt_displs = ucg_calloc(4 * group_size, sizeof(int32_t),
                                                            "alloc merged count displs");
    if (ucg_op->allgatherv.bruck.merged_cnt_displs == NULL) {
        goto err_free_new;
    }
    return UCG_OK;

err_free_new:
    ucg_free(ucg_op->allgatherv.bruck.new_cnt_displs);
err:
    return UCG_ERR_NO_MEMORY;
}

static ucg_status_t ucg_planc_ucx_allgatherv_bruck_op_discard(ucg_plan_op_t *ucg_op)
{
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    if (op->allgatherv.bruck.new_cnt_displs != NULL) {
        ucg_free(op->allgatherv.bruck.new_cnt_displs);
    }
    if (op->allgatherv.bruck.merged_cnt_displs != NULL) {
        ucg_free(op->allgatherv.bruck.merged_cnt_displs);
    }
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, &op->super);
    ucg_mpool_put(op);
    return UCG_OK;
}

static inline
ucg_planc_ucx_op_t *ucg_planc_ucx_allgatherv_bruck_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                          ucg_vgroup_t *vgroup,
                                                          const ucg_coll_args_t *args)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (ucx_op == NULL) {
        goto err;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_allgatherv_bruck_op_trigger,
                                 ucg_planc_ucx_allgatherv_bruck_op_progress,
                                 ucg_planc_ucx_allgatherv_bruck_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(ucx_op, ucx_group);

    status = ucg_planc_ucx_allgatherv_bruck_op_init(ucx_op);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize allgatherv bruck op");
        goto err_destruct;
    }
    return ucx_op;

err_destruct:
    UCG_CLASS_DESTRUCT(ucg_plan_op_t, &ucx_op->super);
err_free_op:
    ucg_mpool_put(ucx_op);
err:
    return NULL;
}

ucg_status_t ucg_planc_ucx_allgatherv_bruck_prepare(ucg_vgroup_t *vgroup,
                                                    const ucg_coll_args_t *args,
                                                    ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_planc_ucx_allgatherv_bruck_op_new(ucx_group, vgroup, args);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    *op = &ucx_op->super;

    return UCG_OK;
}