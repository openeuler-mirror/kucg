/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_plan.h"


UCG_PLAN_ATTR_TABLE_DEFINE(ucg_planc_stars);

static const ucg_plan_policy_t* ucg_planc_stars_get_plan_policy(ucg_coll_type_t coll_type,
                                                                ucg_planc_stars_node_level_t node_level,
                                                                ucg_planc_stars_ppn_level_t ppn_level)
{
    const ucg_plan_policy_t *policy = NULL;
    switch (coll_type) {
        case UCG_COLL_TYPE_BCAST:
        case UCG_COLL_TYPE_IBCAST:
            policy = ucg_planc_stars_get_bcast_plan_policy(node_level, ppn_level);
            break;
        case UCG_COLL_TYPE_SCATTERV:
        case UCG_COLL_TYPE_ISCATTERV:
            policy = ucg_planc_stars_get_scatterv_plan_policy(node_level, ppn_level);
            break;
        case UCG_COLL_TYPE_ALLGATHERV:
        case UCG_COLL_TYPE_IALLGATHERV:
            policy = ucg_planc_stars_get_allgatherv_plan_policy(node_level, ppn_level);
            break;
        case UCG_COLL_TYPE_BARRIER:
        case UCG_COLL_TYPE_IBARRIER:
            policy = ucg_planc_stars_get_barrier_plan_policy(node_level, ppn_level);
            break;
        case UCG_COLL_TYPE_ALLTOALLV:
        case UCG_COLL_TYPE_IALLTOALLV:
            policy = ucg_planc_stars_get_alltoallv_plan_policy(node_level, ppn_level);
            break;
        default:
            break;
    }
    return policy;
}

static ucg_coll_type_t ucg_planc_stars_coll_nonblock_2_block(ucg_coll_type_t coll)
{
    ucg_coll_type_t new_coll = coll;
    switch (coll) {
        case UCG_COLL_TYPE_IBCAST:
            new_coll = UCG_COLL_TYPE_BCAST;
            break;
        case UCG_COLL_TYPE_IALLGATHERV:
            new_coll = UCG_COLL_TYPE_ALLGATHERV;
            break;
        case UCG_COLL_TYPE_ISCATTERV:
            new_coll = UCG_COLL_TYPE_SCATTERV;
            break;
        case UCG_COLL_TYPE_IBARRIER:
            new_coll = UCG_COLL_TYPE_BARRIER;
            break;
        case UCG_COLL_TYPE_IALLTOALLV:
            new_coll = UCG_COLL_TYPE_ALLTOALLV;
            break;
        default:
            break;
    }
    return new_coll;
}

static ucg_status_t ucg_planc_stars_set_plan_attr(ucg_vgroup_t *vgroup,
                                                  ucg_coll_type_t coll_type,
                                                  const ucg_plan_policy_t *policy,
                                                  ucg_plan_attr_t *attr)
{
    attr->id = policy->id;
    attr->range = policy->range;
    attr->score = policy->score;
    attr->vgroup = vgroup;

    /**
     * Non-blocking is treated as blocking because the @ucg_planc_stars_xxx_plan_attr
     * array is registered as blocking mode only.
     */
    coll_type = ucg_planc_stars_coll_nonblock_2_block(coll_type);

    ucg_plan_attr_t *plan_attr = UCG_PLAN_ATTR_ARRAY(ucg_planc_stars, coll_type);
    if (plan_attr == NULL) {
        return UCG_ERR_NOT_FOUND;
    }
    for (; !UCG_PLAN_ATTR_IS_LAST(plan_attr); ++plan_attr) {
        if (plan_attr->id == attr->id) {
            attr->prepare = plan_attr->prepare;
            attr->name = plan_attr->name;
            attr->domain = plan_attr->domain;
            attr->deprecated = plan_attr->deprecated;
            return UCG_OK;
        }
    }
    return UCG_ERR_NOT_FOUND;
}

ucg_status_t ucg_planc_stars_op_discard(ucg_plan_op_t *ucg_op)
{
    ucg_planc_stars_op_t *op = ucg_derived_of(ucg_op, ucg_planc_stars_op_t);
    scp_release_ofd_req(&op->ofd_req);
    ucg_planc_stars_rkey_bundle_cleanup(op);
    ucg_planc_stars_put_rank_desc_cleanup(op);
    if (op->plan.comm_dep.get_ranks) {
        ucg_free(op->plan.comm_dep.get_ranks);
        op->plan.comm_dep.get_ranks = NULL;
    }

    if (op->plan.comm_dep.put_ranks) {
        ucg_free(op->plan.comm_dep.put_ranks);
        op->plan.comm_dep.put_ranks = NULL;
    }

    ucg_mpool_cleanup(&op->ofd_req_elem_pool, 1);
    ucg_mpool_put(op);
    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_add_default_plans(ucg_planc_stars_group_t *stars_group,
                                                      ucg_plans_t *plans)
{
    ucg_vgroup_t *vgroup = &stars_group->super.super;
    ucg_group_t *group = vgroup->group;

    /* calc node_level and ppn_level */
    int32_t ppn = group->topo->ppn;
    int32_t node_cnt = group->size / ppn;
    ucg_planc_stars_node_level_t node_level = ucg_planc_stars_get_node_level(node_cnt);
    ucg_planc_stars_ppn_level_t ppn_level = ucg_planc_stars_get_ppn_level(ppn);

    ucg_plan_params_t params;
    params.mem_type = UCG_MEM_TYPE_HOST;

    const ucg_plan_policy_t *default_policy = NULL;
    ucg_coll_type_t coll_type = UCG_COLL_TYPE_BCAST;
    for (; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        /* get internal policy */
        default_policy = ucg_planc_stars_get_plan_policy(coll_type, node_level, ppn_level);
        if (default_policy == NULL) {
            continue;
        }

        params.coll_type = coll_type;
        for (; !UCG_PLAN_POLICY_IS_LAST(default_policy); ++default_policy) {
            ucg_status_t status = ucg_planc_stars_set_plan_attr(vgroup, coll_type,
                                                                default_policy, &params.attr);
            if (status != UCG_OK) {
                continue;
            }
            status = ucg_plans_add(plans, &params);
            if (status != UCG_OK) {
                ucg_error("Failed to add default plan, coll type %d", coll_type);
                return status;
            }
        }
    }
    return UCG_OK;
}

static ucg_status_t ucg_planc_stars_add_user_plans(ucg_planc_stars_group_t *stars_group,
                                                   ucg_plans_t *plans)
{
    ucg_vgroup_t *vgroup = &stars_group->super.super;
    ucg_planc_stars_context_t *context = stars_group->context;

    ucg_plan_params_t params;
    params.mem_type = UCG_MEM_TYPE_HOST;

    ucg_coll_type_t coll_type = UCG_COLL_TYPE_BCAST;
    for (; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        params.coll_type = coll_type;
        ucg_plan_policy_t *user_policy_ptr = context->user_policy[coll_type];
        for (; !UCG_PLAN_POLICY_IS_LAST(user_policy_ptr); ++user_policy_ptr) {
            ucg_status_t status = ucg_planc_stars_set_plan_attr(vgroup, coll_type,
                                                                user_policy_ptr, &params.attr);
            if (status != UCG_OK) {
                continue;
            }
            status = ucg_plans_add(plans, &params);
            if (status != UCG_OK) {
                ucg_error("Failed to add user plan, coll type %d", coll_type);
                return status;
            }
        }
    }
    return UCG_OK;
}

ucg_status_t ucg_planc_stars_get_plans(ucg_planc_group_h planc_group, ucg_plans_t *plans)
{
    UCG_CHECK_NULL_INVALID(planc_group, plans);
    ucg_status_t status = UCG_OK;
    ucg_planc_stars_group_t *stars_group = ucg_derived_of(planc_group, ucg_planc_stars_group_t);

    status = ucg_planc_stars_add_default_plans(stars_group, plans);
    if (status != UCG_OK) {
        return status;
    }
    status = ucg_planc_stars_add_user_plans(stars_group, plans);
    if (status != UCG_OK) {
        return status;
    }

    return status;
}
