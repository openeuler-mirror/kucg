/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "bcast.h"
#include "planc_ucx_plan.h"
#include "planc_ucx_p2p.h"
#include "core/ucg_group.h"
#include "core/ucg_topo.h"
#include "util/ucg_log.h"

static ucg_status_t ucg_planc_ucx_bcast_na_kntree_check(ucg_vgroup_t *vgroup,
                                                        const ucg_coll_args_t *args)
{
    if (vgroup->group->topo->ppn == UCG_TOPO_PPX_UNKNOWN) {
        ucg_info("Bcast na_kntree don't support unknown ppn");
        return UCG_ERR_UNSUPPORTED;
    }
    return UCG_OK;
}

static ucg_status_t ucg_planc_ucx_bcast_add_inter_node_op(ucg_plan_meta_op_t *meta_op,
                                                          ucg_planc_ucx_group_t *ucx_group,
                                                          ucg_vgroup_t *vgroup,
                                                          const ucg_coll_args_t *args,
                                                          const ucg_planc_ucx_bcast_config_t *config)
{
    ucg_planc_ucx_bcast_config_t kntree_config;
    kntree_config.root_adjust = config->root_adjust;
    kntree_config.kntree_degree = config->na_kntree_inter_degree;
    return ucg_planc_ucx_bcast_add_topo_group_kntree_op(meta_op, ucx_group, vgroup,
                                                        args, &kntree_config,
                                                        UCG_TOPO_GROUP_TYPE_NODE_LEADER);
}

static ucg_status_t ucg_planc_ucx_bcast_add_intra_node_op(ucg_plan_meta_op_t *meta_op,
                                                          ucg_planc_ucx_group_t *ucx_group,
                                                          ucg_vgroup_t *vgroup,
                                                          const ucg_coll_args_t *args,
                                                          const ucg_planc_ucx_bcast_config_t *config)
{
    ucg_planc_ucx_bcast_config_t kntree_config;
    kntree_config.root_adjust = config->root_adjust;
    kntree_config.kntree_degree = config->na_kntree_intra_degree;
    return ucg_planc_ucx_bcast_add_topo_group_kntree_op(meta_op, ucx_group, vgroup,
                                                        args, &kntree_config,
                                                        UCG_TOPO_GROUP_TYPE_NODE);
}

ucg_plan_meta_op_t* ucg_planc_ucx_bcast_na_kntree_op_new(ucg_planc_ucx_group_t *ucx_group,
                                                         ucg_vgroup_t *vgroup,
                                                         const ucg_coll_args_t *args,
                                                         const ucg_planc_ucx_bcast_config_t *config)
{
    UCG_CHECK_NULL(NULL, ucx_group, vgroup, args, config);

    ucg_status_t status;
    ucg_plan_meta_op_t *meta_op = ucg_plan_meta_op_new(vgroup->group, vgroup, args);
    if (meta_op == NULL) {
        return NULL;
    }

    ucg_planc_ucx_bcast_config_t *adjust_config = (ucg_planc_ucx_bcast_config_t *)config;
    uint8_t old_root_adjust = adjust_config->root_adjust;
    adjust_config->root_adjust = 1;

    /* 1. adjust root if necessary */
    status = ucg_planc_ucx_bcast_add_adjust_root_op(meta_op, ucx_group,
                                                    vgroup, args, adjust_config);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    /* 2. broadcast in node leader group. */
    status = ucg_planc_ucx_bcast_add_inter_node_op(meta_op, ucx_group,
                                                   vgroup, args, adjust_config);
    UCG_CHECK_GOTO(status, err_free_meta_op);
    /* 3. broadcast in node group. */
    status = ucg_planc_ucx_bcast_add_intra_node_op(meta_op, ucx_group,
                                                   vgroup, args, adjust_config);
    UCG_CHECK_GOTO(status, err_free_meta_op);

    adjust_config->root_adjust = old_root_adjust;
    return meta_op;

err_free_meta_op:
    meta_op->super.discard(&meta_op->super);
    return NULL;
}

ucg_status_t ucg_planc_ucx_bcast_na_kntree_prepare(ucg_vgroup_t *vgroup,
                                                   const ucg_coll_args_t *args,
                                                   ucg_plan_op_t **op)
{
    UCG_CHECK_NULL_INVALID(vgroup, args, op);

    ucg_status_t status;
    status = ucg_planc_ucx_bcast_na_kntree_check(vgroup, args);
    if (status != UCG_OK) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(vgroup, ucg_planc_ucx_group_t);
    ucg_planc_ucx_bcast_config_t *config;
    config = UCG_PLANC_UCX_CONTEXT_BUILTIN_CONFIG_BUNDLE(ucx_group->context, bcast,
                                                         UCG_COLL_TYPE_BCAST);
    ucg_plan_meta_op_t *meta_op;
    meta_op = ucg_planc_ucx_bcast_na_kntree_op_new(ucx_group, vgroup, args, config);
    if (meta_op == NULL) {
        return UCG_ERR_NO_MEMORY;
    }
    *op = &meta_op->super;
    return UCG_OK;
}