/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "allreduce.h"
#include "allreduce_meta.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_meta.h"
#include "core/ucg_topo.h"
#include "core/ucg_group.h"
#include "core/ucg_plan.h"
#include "util/ucg_log.h"

static ucg_status_t ucg_planc_ucx_allreduce_sa_rabenseifner_check(ucg_vgroup_t *vgroup,
                                                                  const ucg_coll_args_t *args)
{
    uint32_t group_size = vgroup->size;
    int32_t count = args->allreduce.count;
    if (count < group_size) {
        ucg_info("Allreduce sa_rabenseifner don't support count < group_size");
        return UCG_ERR_UNSUPPORTED;
    }
    ucg_op_flag_t flags = args->allreduce.op->flags;
    if (!(flags & UCG_OP_FLAG_IS_COMMUTATIVE)) {
        ucg_info("Allreduce sa_rabenseifner don't support non-commutative op");
        return UCG_ERR_UNSUPPORTED;
    }
    if (vgroup->group->topo->ppn == UCG_TOPO_PPX_UNKNOWN) {
        ucg_info("Allreduce sa_rabenseifner don't support unknown ppn");
        return UCG_ERR_UNSUPPORTED;
    }
    if (vgroup->group->topo->pps == UCG_TOPO_PPX_UNKNOWN) {
        ucg_info("Allreduce sa_rabenseifner don't support unknown pps");
        return UCG_ERR_UNSUPPORTED;
    }
    if (vgroup->group->topo->ppn == UCG_TOPO_PPX_UNBALANCED) {
        ucg_info("Allreduce sa_rabenseifner don't support unbalanced ppn");
        return UCG_ERR_UNSUPPORTED;
    }
    if (vgroup->group->topo->pps == UCG_TOPO_PPX_UNBALANCED) {
        ucg_info("Allreduce sa_rabenseifner don't support unbalanced pps");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

ucg_plan_meta_op_t* ucg_planc_ucx_allreduce_sa_rabenseifner_op_new(ucg_planc_ucx_group_t* ucx_group,
                                                                   ucg_vgroup_t* vgroup,
                                                                   const ucg_coll_args_t* args)
{
    UCG_CHECK_NULL(NULL, ucx_group, vgroup, args);

    ucg_plan_meta_op_t* meta_op = ucg_plan_meta_op_new(vgroup->group, vgroup, args);
    if (meta_op == NULL) {
        goto err;
    }

    ucg_status_t status;
    ucg_coll_args_t *meta_args = &meta_op->super.super.args;

    status = ucg_planc_ucx_allreduce_add_reduce_scatter_op(meta_op, ucx_group,
                                                           vgroup, meta_args,
                                                           UCG_TOPO_GROUP_TYPE_SOCKET);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    int32_t offset, count;
    status = ucg_planc_ucx_allreduce_get_rd_args(vgroup, args, UCG_TOPO_GROUP_TYPE_SOCKET,
                                                 &offset, &count);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    ucg_coll_args_t rd_args = *meta_args;
    if (count > 0) { // has added reduce_scatter op
        rd_args.allreduce.sendbuf = args->allreduce.recvbuf + offset;
        rd_args.allreduce.recvbuf = args->allreduce.recvbuf + offset;
        rd_args.allreduce.count = count;
    }

    status = ucg_planc_ucx_create_socket_leader_algo_group(ucx_group, vgroup);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    ucg_planc_ucx_algo_group_type_t group_type = UCG_ALGO_GROUP_TYPE_SOCKET_LEADER;
    status = ucg_planc_ucx_allreduce_add_allreduce_op(meta_op, ucx_group,
                                                      vgroup, &rd_args,
                                                      group_type);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    if (count <= 0 && ucx_group->groups[group_type].state == UCG_ALGO_GROUP_STATE_ENABLE) {
        // has not added reduce_scatter op, but added socket leader allreduce op
        rd_args.allreduce.sendbuf = args->allreduce.recvbuf;
        rd_args.allreduce.recvbuf = args->allreduce.recvbuf;
    }
    status = ucg_planc_ucx_create_node_leader_algo_group(ucx_group, vgroup);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    status = ucg_planc_ucx_allreduce_add_allreduce_op(meta_op, ucx_group,
                                                      vgroup, &rd_args,
                                                      UCG_ALGO_GROUP_TYPE_NODE_LEADER);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    status = ucg_planc_ucx_allreduce_add_allgatherv_op(meta_op, ucx_group,
                                                       vgroup, meta_args,
                                                       UCG_TOPO_GROUP_TYPE_SOCKET);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    return meta_op;

err_free_meta_op:
    meta_op->super.discard(&meta_op->super);
err:
    return NULL;
}

ucg_status_t ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(ucg_vgroup_t *vgroup,
                                                             const ucg_coll_args_t *args,
                                                             ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_plan_meta_op_t* meta_op;
    meta_op = ucg_planc_ucx_allreduce_sa_rabenseifner_op_new(ucx_group, vgroup, args);
    if (meta_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    *op = &meta_op->super;
    return UCG_OK;
}