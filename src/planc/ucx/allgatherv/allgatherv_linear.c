/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include "allgatherv.h"
#include "planc_ucx_plan.h"

enum {
    UCG_ALLGATHERV_LINEAR_SEND = UCG_BIT(0),
    UCG_ALLGATHERV_LINEAR_RECV = UCG_BIT(1),
};

#define UCG_ALLGATHERV_LINEAR_FLAGS UCG_ALLGATHERV_LINEAR_SEND | UCG_ALLGATHERV_LINEAR_RECV

static ucg_status_t ucg_planc_ucx_allgatherv_linear_op_progress(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_coll_allgatherv_args_t *args = &op->super.super.args.allgatherv;
    ucg_vgroup_t *vgroup = op->super.vgroup;
    uint32_t myrank = vgroup->myrank;
    uint32_t group_size = vgroup->size;
    int64_t recvtype_extent = ucg_dt_extent(args->recvtype);
    ucg_planc_ucx_p2p_params_t params;
    ucg_planc_ucx_op_set_p2p_params(op, &params);

    if (ucg_test_and_clear_flags(&op->flags, UCG_ALLGATHERV_LINEAR_FLAGS)) {
        for (int i = 1; i < group_size; ++i) {
            ucg_rank_t speer = (myrank + i) % group_size;
            void *sendbuf = args->recvbuf + args->displs[myrank] * recvtype_extent;
            status = ucg_planc_ucx_p2p_isend(sendbuf, args->recvcounts[myrank],
                                             args->recvtype, speer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_ERR_GOTO(status, out);

            ucg_rank_t rpeer = (myrank - i + group_size) % group_size;
            void *recvbuf = args->recvbuf + args->displs[rpeer] * recvtype_extent;
            status = ucg_planc_ucx_p2p_irecv(recvbuf, args->recvcounts[rpeer],
                                             args->recvtype, rpeer, op->tag,
                                             vgroup, &params);
            UCG_CHECK_ERR_GOTO(status, out);
        }
    }
    status = ucg_planc_ucx_p2p_testall(op->ucx_group, params.state);
    UCG_CHECK_GOTO(status, out);

out:
    op->super.super.status = status;
    return status;
}

static ucg_status_t ucg_planc_ucx_allgatherv_linear_op_trigger(ucg_plan_op_t *ucg_op)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *op = ucg_derived_of(ucg_op, ucg_planc_ucx_op_t);
    ucg_planc_ucx_op_reset(op);
    op->flags = UCG_ALLGATHERV_LINEAR_FLAGS;

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

    status = ucg_planc_ucx_allgatherv_linear_op_progress(ucg_op);
out:
    return status == UCG_INPROGRESS ? UCG_OK : status;
}

static inline
ucg_planc_ucx_op_t *ucg_planc_ucx_allgatherv_linear_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                           ucg_vgroup_t *vgroup,
                                                           const ucg_coll_args_t *args)
{
    ucg_status_t status;
    ucg_planc_ucx_op_t *ucx_op = ucg_mpool_get(&ucx_group->context->op_mp);
    if (ucx_op == NULL) {
        goto err;
    }

    status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &ucx_op->super, vgroup,
                                 ucg_planc_ucx_allgatherv_linear_op_trigger,
                                 ucg_planc_ucx_allgatherv_linear_op_progress,
                                 ucg_planc_ucx_op_discard,
                                 args);
    if (status != UCG_OK) {
        ucg_error("Failed to initialize super of ucx op");
        goto err_free_op;
    }
    ucg_planc_ucx_op_init(ucx_op, ucx_group);
    return ucx_op;

err_free_op:
    ucg_mpool_put(ucx_op);
err:
    return NULL;
}

ucg_status_t ucg_planc_ucx_allgatherv_linear_prepare(ucg_vgroup_t *vgroup,
                                                     const ucg_coll_args_t *args,
                                                     ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_op_t *ucx_op = ucg_planc_ucx_allgatherv_linear_op_new(ucx_group, vgroup, args);
    if (ucx_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    *op = &ucx_op->super;

    return UCG_OK;
}