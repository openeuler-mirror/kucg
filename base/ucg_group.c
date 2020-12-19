/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2019-2020.  ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "ucg_group.h"
#include "../builtin/plan/builtin_plan.h"
#include <ucg/api/ucg_mpi.h>

#include <ucp/core/ucp_ep.inl>
#include <ucp/core/ucp_worker.h>
#include <ucs/datastruct/queue.h>
#include <ucs/datastruct/list.h>
#include <ucs/profile/profile.h>
#include <ucs/debug/memtrack.h>
#include <ucp/core/ucp_ep.inl>
#include <ucp/core/ucp_proxy_ep.h> /* for @ref ucp_proxy_ep_test */

#if ENABLE_STATS
/**
 * UCG group statistics counters
 */
enum {
    UCG_GROUP_STAT_PLANS_CREATED,
    UCG_GROUP_STAT_PLANS_USED,

    UCG_GROUP_STAT_OPS_CREATED,
    UCG_GROUP_STAT_OPS_USED,
    UCG_GROUP_STAT_OPS_IMMEDIATE,

    UCG_GROUP_STAT_LAST
};

static ucs_stats_class_t ucg_group_stats_class = {
    .name           = "ucg_group",
    .num_counters   = UCG_GROUP_STAT_LAST,
    .counter_names  = {
        [UCG_GROUP_STAT_PLANS_CREATED] = "plans_created",
        [UCG_GROUP_STAT_PLANS_USED]    = "plans_reused",
        [UCG_GROUP_STAT_OPS_CREATED]   = "ops_created",
        [UCG_GROUP_STAT_OPS_USED]      = "ops_started",
        [UCG_GROUP_STAT_OPS_IMMEDIATE] = "ops_immediate"
    }
};
#endif

#define UCG_GROUP_PROGRESS_ADD(iface, ctx) {         \
    unsigned idx = 0;                                \
    if (ucs_unlikely(idx == UCG_GROUP_MAX_IFACES)) { \
        return UCS_ERR_EXCEEDS_LIMIT;                \
    }                                                \
    while (idx < (ctx)->iface_cnt) {                 \
        if ((ctx)->ifaces[idx] == (iface)) {         \
            break;                                   \
        }                                            \
        idx++;                                       \
    }                                                \
    if (idx == (ctx)->iface_cnt) {                   \
        (ctx)->ifaces[(ctx)->iface_cnt++] = (iface); \
    }                                                \
}

unsigned ucg_worker_progress(ucg_worker_h worker)
{
    unsigned idx;
    unsigned ret = 0;
    ucg_groups_t *gctx = UCG_WORKER_TO_GROUPS_CTX(worker);
    for (idx = 0; idx < gctx->iface_cnt; idx++) {
        ret += uct_iface_progress(gctx->ifaces[idx]);
    }
    return ret;
}

unsigned ucg_group_progress(ucg_group_h group)
{
    unsigned idx;
    unsigned ret = 0;
    ucg_groups_t *gctx = UCG_WORKER_TO_GROUPS_CTX(group->worker);

    for (idx = 0; idx < gctx->num_planners; idx++) {
        ucg_plan_component_t *planc = gctx->planners[idx].plan_component;
        ret += planc->progress(group);
    }

    for (idx = 0; idx < group->iface_cnt; idx++) {
        ret += uct_iface_progress(group->ifaces[idx]);
    }

    return ret;
}

unsigned ucg_base_am_id;
size_t ucg_ctx_worker_offset;

void ucg_init_group_cache(struct ucg_group *new_group)
{
    unsigned idx, rank, algo_idx;
    for (algo_idx = 0; algo_idx <  UCG_GROUP_MSG_SIZE_LEVEL; algo_idx++) {
        for (rank = 0; rank < UCG_GROUP_MAX_ROOT_PARAM; rank++) {
            for (idx = 0; idx < UCG_GROUP_MAX_COLL_TYPE_BUCKETS; idx++) {
                new_group->cache[algo_idx][rank][idx] = NULL;
            }
        }
    }
}

void ucg_init_group_root_used(struct ucg_group *new_group)
{
    unsigned rank;
    /* Initalization of root_used */
    for (rank = 0; rank < UCG_GROUP_MAX_ROOT_PARAM; rank++) {
        new_group->root_used[rank] = (unsigned) -1;
    }
}

ucs_status_t ucg_init_group(ucg_worker_h worker,
                            const ucg_group_params_t *params,
                            ucg_groups_t *ctx,
                            size_t distance_size,
                            size_t nodenumber_size,
                            struct ucg_group *new_group)
{
    /* fill in the group fields */
    new_group->is_barrier_outstanding = 0;
    new_group->group_id               = params->cid;
    new_group->worker                 = worker;
    new_group->next_id                = 0;
    new_group->iface_cnt              = 0;

    ucs_queue_head_init(&new_group->pending);
    memcpy((ucg_group_params_t*)&new_group->params, params, sizeof(*params));
    new_group->params.distance = (typeof(params->distance))((char*)(new_group
            + 1) + ctx->total_planner_sizes);
    memcpy(new_group->params.distance, params->distance, distance_size);
    new_group->params.node_index = (typeof(params->node_index))((char*)(new_group
            + 1) + ctx->total_planner_sizes + distance_size);
    memcpy(new_group->params.node_index, params->node_index, nodenumber_size);
    memset(new_group + 1, 0, ctx->total_planner_sizes);

    ucg_init_group_cache(new_group);
    ucg_init_group_root_used(new_group);
    new_group->params.topo_map = NULL;
    if (params->topo_map) {
        new_group->params.topo_map = UCS_ALLOC_CHECK(sizeof(char*) * params->member_count, "topo map");
        unsigned i;
        for (i = 0; i < params->member_count; i++) {
            unsigned topo_size = sizeof(char) * params->member_count;
            new_group->params.topo_map[i] = UCS_ALLOC_CHECK(topo_size, "topo map");
            memcpy(new_group->params.topo_map[i], params->topo_map[i], topo_size);
        }
    }

    return UCS_OK;
}

void ucg_group_clean_planners(ucg_groups_t *ctx,
                              unsigned planner_idx,
                              struct ucg_group *new_group)
{
    while (planner_idx) {
        ucg_plan_component_t *planner = ctx->planners[planner_idx--].plan_component;
        planner->destroy((void*)new_group);
    }
    ucs_free(new_group);
    new_group = NULL;
}

ucs_status_t ucg_group_planner_create(ucg_groups_t *ctx,
                                      ucg_worker_h worker,
                                      struct ucg_group *new_group,
                                      unsigned *idx)
{
    ucs_status_t status = UCS_OK;
    for (*idx = 0; *idx < ctx->num_planners; (*idx)++) {
        /* Create the per-planner per-group context */
        ucg_plan_component_t *planner = ctx->planners[*idx].plan_component;
        status = planner->create(planner, worker, new_group, ucg_base_am_id + *idx,
                                 new_group->group_id, &new_group->worker->am_mp, &new_group->params);
        if (status != UCS_OK) {
            return status;
        }
    }
    return status;
}

ucs_status_t ucg_group_create(ucg_worker_h worker,
                              const ucg_group_params_t *params,
                              ucg_group_h *group_p)
{
    if (worker == NULL || params == NULL || group_p == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }
    ucs_status_t status;
    ucg_groups_t *ctx = UCG_WORKER_TO_GROUPS_CTX(worker);
    UCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(worker);

    /* allocate a new group */
    size_t distance_size              = sizeof(*params->distance) * params->member_count;
    size_t nodenumber_size            = sizeof(*params->node_index) * params->member_count;
    struct ucg_group *new_group       = ucs_malloc(sizeof(struct ucg_group) +
            ctx->total_planner_sizes + distance_size + nodenumber_size, "communicator group");
    if (new_group == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto cleanup_none;
    }

    unsigned idx = 0;
    status = ucg_init_group(worker, params, ctx, distance_size, nodenumber_size, new_group);
    if (status != UCS_OK) {
        goto cleanup_planners;
    }

    status = ucg_group_planner_create(ctx, worker, new_group, &idx);
    if (status != UCS_OK) {
        goto cleanup_planners;
    }

    status = UCS_STATS_NODE_ALLOC(&new_group->stats,
                                  &ucg_group_stats_class, worker->stats, "-%p", new_group);
    if (status != UCS_OK) {
        goto cleanup_planners;
    }
    new_group->params.is_socket_balance = params->is_socket_balance;
    ucs_list_add_head(&ctx->groups_head, &new_group->list);
    UCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(worker);
    *group_p = new_group;
    ucs_info("create ucg group %hu members %lu", new_group->group_id, params->member_count);
    return UCS_OK;

cleanup_planners:
    ucg_group_clean_planners(ctx, idx, new_group);

cleanup_none:
    UCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(worker);
    return status;
}

const ucg_group_params_t* ucg_group_get_params(ucg_group_h group)
{
    if (group == NULL) {
        return NULL;
    }
    return &group->params;
}

void ucg_group_planner_destroy(ucg_group_h group)
{
    unsigned idx;
    ucg_groups_t *gctx = UCG_WORKER_TO_GROUPS_CTX(group->worker);
    for (idx = 0; idx < gctx->num_planners; idx++) {
        ucg_plan_component_t *planc = gctx->planners[idx].plan_component;
        planc->destroy(group);
    }
}

void ucg_group_destroy(ucg_group_h group)
{
    if (group == NULL) {
        return;
    }
    ucs_info("destroying ucg group %hu", group->group_id);
    /* First - make sure all the collectives are completed */
    while (!ucs_queue_is_empty(&group->pending)) {
        ucg_group_progress(group);
    }

#if ENABLE_MT
    ucg_worker_h worker = group->worker;
    UCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(worker);
#endif

    ucg_group_planner_destroy(group);
    UCS_STATS_NODE_FREE(group->stats);
    ucs_list_del(&group->list);
    ucs_free(group);
    group = NULL;

#if ENABLE_MT
    UCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(worker);
#endif
}

ucs_status_t ucg_request_check_status(void *request)
{
    ucg_request_t *req = (ucg_request_t*)request - 1;

    if (req->flags & UCG_REQUEST_COMMON_FLAG_COMPLETED) {
        ucs_assert(req->status != UCS_INPROGRESS);
        return req->status;
    }
    return UCS_INPROGRESS;
}

void ucg_request_cancel(ucg_worker_h worker, void *request) { }

void ucg_request_free(void *request) { }

ucs_status_t ucg_plan_select(ucg_group_h group, const char* planner_name,
                             const ucg_collective_params_t *params,
                             ucg_plan_component_t **planc_p)
{
    ucg_groups_t *ctx = UCG_WORKER_TO_GROUPS_CTX(group->worker);
    return ucg_plan_select_component(ctx->planners, ctx->num_planners,
                                     planner_name, &group->params, params, planc_p);
}

static int ucg_chk_noncontig_allreduce_plan(const ucg_collective_params_t *coll_params,
                                            const ucg_group_params_t *group_params,
                                            const ucg_plan_t *plan)
{
    int noncontig_allreduce;

    if (coll_params->type.modifiers != ucg_predefined_modifiers[UCG_PRIMITIVE_ALLREDUCE]) {
        return 0;
    }

    noncontig_allreduce = ucg_is_noncontig_allreduce(group_params, coll_params);
    if (plan->is_noncontig_allreduce) {
        return !noncontig_allreduce;
    } else {
        return noncontig_allreduce;
    }
}

void ucg_get_cache_plan(unsigned int message_size_level, unsigned int coll_root,
                        ucg_group_h group, ucg_collective_params_t *params, ucg_plan_t **cache_plan, unsigned root)
{
    ucg_plan_t *plan = group->cache[message_size_level][coll_root][params->plan_cache_index];
    if (plan == NULL) {
        *cache_plan = NULL;
        return;
    }

    if (params->send.op_ext && !group->params.op_is_commute_f(params->send.op_ext) && !plan->support_non_commutative) {
        *cache_plan = NULL;
        return;
    }

    if (params->send.op_ext && !group->params.op_is_commute_f(params->send.op_ext) && params->send.count > 1) {
        *cache_plan = NULL;
        return;
    }

    ucg_builtin_config_t *config = (ucg_builtin_config_t *)plan->planner->plan_config;
    if (params->send.dt_len > config->large_datatype_threshold && !plan->support_large_datatype) {
        *cache_plan = NULL;
        return;
    }

    if (ucg_chk_noncontig_allreduce_plan(params, &group->params, plan)) {
        *cache_plan = NULL;
        return;
    }

    if (plan->is_ring_plan_topo_type && ucg_is_segmented_allreduce(params)) {
        *cache_plan = NULL;
        return;
    }

    if (plan != NULL && root != plan->type.root) {
        *cache_plan = NULL;
        return;
    }

    ucs_debug("select plan from cache: %p", plan);
    *cache_plan = plan;
}

void ucg_update_group_cache(ucg_group_h group,
                            unsigned int message_size_level,
                            unsigned int coll_root,
                            ucg_collective_params_t *params,
                            ucg_plan_t *plan)
{
    if (group->cache[message_size_level][coll_root][params->plan_cache_index] != NULL) {
        ucg_builtin_plan_t *builtin_plan = ucs_derived_of(group->cache[message_size_level][coll_root][params->plan_cache_index], ucg_builtin_plan_t);
        (void)ucg_builtin_destroy_plan(builtin_plan, group);
        group->cache[message_size_level][coll_root][params->plan_cache_index] = NULL;
    }
    group->cache[message_size_level][coll_root][params->plan_cache_index] = plan;
}

void ucg_log_coll_params(ucg_collective_params_t *params)
{
    ucs_debug("ucg_collective_create OP: "
              "params={type=%u, root=%lu, send=[%p,%i,%lu,%p,%p], "
              "recv=[%p,%i,%lu,%p,%p], cb=%p, op=%p}",
              (unsigned)params->type.modifiers, (uint64_t)params->type.root,
              params->send.buf, params->send.count, params->send.dt_len,
              params->send.dt_ext, params->send.displs,
              params->recv.buf, params->recv.count, params->recv.dt_len,
              params->recv.dt_ext, params->recv.displs,
              params->comp_cb, params->recv.op_ext);
}

void ucg_collective_create_choose_algorithm(unsigned msg_size, unsigned *message_size_level)
{
    /* choose algorithm due to message size */
    if (msg_size < UCG_GROUP_MED_MSG_SIZE) {
        *message_size_level = 0;
    } else {
        *message_size_level = 1;
    }
}

UCS_PROFILE_FUNC(ucs_status_t, ucg_collective_create,
        (group, params, coll), ucg_group_h group,
        ucg_collective_params_t *params, ucg_coll_h *coll)
{
    UCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(group->worker);

    /* check the recycling/cache for this collective */
    ucg_op_t *op = NULL;
    ucs_status_t status;
    if (group == NULL || params == NULL || coll == NULL || params->send.count < 0) {
        status = UCS_ERR_INVALID_PARAM;
        goto out;
    }

    /* find the plan of current root whether has been established */
    ucg_group_member_index_t root = UCG_ROOT_RANK(params);
    unsigned msg_size = params->send.count * params->send.dt_len;
    unsigned coll_root;
    unsigned message_size_level;
    unsigned is_coll_root_found = 1;

    if (root >= group->params.member_count) {
        status = UCS_ERR_INVALID_PARAM;
        ucs_error("Invalid root[%ld] for communication group size[%ld]", root, group->params.member_count);
        goto out;
    }

    /* root cache has been not found */
    if (root != group->root_used[root % UCG_GROUP_MAX_ROOT_PARAM]) {
        group->root_used[root % UCG_GROUP_MAX_ROOT_PARAM] = root;
        is_coll_root_found = 0;
    }
    coll_root = root % UCG_GROUP_MAX_ROOT_PARAM;

    ucg_collective_create_choose_algorithm(msg_size, &message_size_level);

    ucg_plan_t *plan = NULL;
    if (is_coll_root_found) {
        ucg_get_cache_plan(message_size_level, coll_root, group, params, &plan, root);
    }

    if (ucs_likely(plan != NULL)) {
        ucs_list_for_each(op, &plan->op_head, list) {
            if (!memcmp(&op->params, params, sizeof(*params)) &&
                !plan->is_noncontig_allreduce) {
                status = UCS_OK;
                goto op_found;
            }
        }

        UCS_STATS_UPDATE_COUNTER(group->stats, UCG_GROUP_STAT_PLANS_USED, 1);
        goto plan_found;
    }

    /* select which plan to use for this collective operation */
    ucg_plan_component_t *planc = NULL;
    status = ucg_plan_select(group, NULL, params, &planc);
    if (status != UCS_OK) {
        goto out;
    }

    /* create the actual plan for the collective operation */
    UCS_PROFILE_CODE("ucg_plan") {
        ucs_trace_req("ucg_collective_create PLAN: planc=%s type=%x root=%lu",
                      &planc->name[0], params->type.modifiers, (uint64_t)params->type.root);
        status = ucg_plan(planc, &params->type, params->send.count * params->send.dt_len, group, params, &plan);
    }
    if (status != UCS_OK) {
        goto out;
    }

    plan->planner           = planc;
    plan->group             = group;
    plan->type              = params->type;
    plan->group_id          = group->group_id;
    plan->am_mp             = &group->worker->am_mp;
    ucg_update_group_cache(group, message_size_level, coll_root, params, plan);
    ucs_list_head_init(&plan->op_head);
    UCS_STATS_UPDATE_COUNTER(group->stats, UCG_GROUP_STAT_PLANS_CREATED, 1);

plan_found:
    UCS_STATS_UPDATE_COUNTER(group->stats, UCG_GROUP_STAT_OPS_CREATED, 1);
    UCS_PROFILE_CODE("ucg_prepare") {
        status = ucg_prepare(plan, params, &op);
    }
    if (status != UCS_OK) {
        goto out;
    }

    ucs_list_add_head(&plan->op_head, &op->list);
    memcpy(&op->params, params, sizeof(*params));
    op->plan = plan;

op_found:
    *coll = op;
    ucg_log_coll_params(params);

out:
    UCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(group->worker);
    return status;
}

static UCS_F_ALWAYS_INLINE ucs_status_t ucg_collective_trigger(ucg_group_h group, ucg_op_t *op, ucg_request_t **req)
{
    /* Barrier effect - all new collectives are pending */
    if (ucs_unlikely(op->params.type.modifiers & UCG_GROUP_COLLECTIVE_MODIFIER_BARRIER)) {
        ucs_assert(group->is_barrier_outstanding == 0);
        group->is_barrier_outstanding = 1;
    }

    /* Start the first step of the collective operation */
    ucs_status_t ret;
    UCS_PROFILE_CODE("ucg_trigger") {
        ret = ucg_trigger(op, group->next_id++, req);
    }

    if (ret != UCS_INPROGRESS) {
        UCS_STATS_UPDATE_COUNTER(group->stats, UCG_GROUP_STAT_OPS_IMMEDIATE, 1);
    }

    return ret;
}

ucs_status_t ucg_collective_release_barrier(ucg_group_h group)
{
    if (group->is_barrier_outstanding == 0) {
        // current operation is not barrier.
        return UCS_OK;
    }
    group->is_barrier_outstanding = 0;
    if (ucs_queue_is_empty(&group->pending)) {
        return UCS_OK;
    }

    ucs_status_t ret;
    do {
        /* Move the operation from the pending queue back to the original one */
        ucg_op_t *op = (ucg_op_t*)ucs_queue_pull_non_empty(&group->pending);
        ucg_request_t **req = op->pending_req;
        ucs_list_add_head(&op->plan->op_head, &op->list);

        /* Start this next pending operation */
        ret = ucg_collective_trigger(group, op, req);
    } while ((!ucs_queue_is_empty(&group->pending)) &&
             (!group->is_barrier_outstanding) &&
             (ret == UCS_OK));

    return ret;
}

static UCS_F_ALWAYS_INLINE ucs_status_t ucg_collective_start(ucg_coll_h coll, ucg_request_t **req)
{
    if (coll == NULL || req == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }
    ucs_status_t ret;
    ucg_op_t *op = (ucg_op_t*)coll;
    ucg_group_h group = op->plan->group;

    /* Since group was created - don't need UCP_CONTEXT_CHECK_FEATURE_FLAGS */
    UCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(group->worker);

    ucs_trace_req("ucg_collective_start: op=%p req=%p", coll, *req);

    if (ucs_unlikely(group->is_barrier_outstanding)) {
        ucs_list_del(&op->list);
        ucs_queue_push(&group->pending, &op->queue);
        op->pending_req = req;
        ret = UCS_INPROGRESS;
    } else {
        ret = ucg_collective_trigger(group, op, req);
    }

    UCS_STATS_UPDATE_COUNTER(group->stats, UCG_GROUP_STAT_OPS_USED, 1);
    UCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(group->worker);
    return ret;
}

UCS_PROFILE_FUNC(ucs_status_ptr_t, ucg_collective_start_nb,
                 (coll), ucg_coll_h coll)
{
    ucs_debug("ucg_collective_start_nb %p", coll);
    ucg_request_t *req = NULL;
    ucs_status_ptr_t ret = UCS_STATUS_PTR(ucg_collective_start(coll, &req));
    return UCS_PTR_IS_ERR(ret) ? ret : req;
}

UCS_PROFILE_FUNC(ucs_status_t, ucg_collective_start_nbr,
                 (coll, request), ucg_coll_h coll, void *request)
{
    ucs_debug("ucg_collective_start_nbr %p", coll);
    return ucg_collective_start(coll, (ucg_request_t**)&request);
}

void ucg_collective_destroy(ucg_coll_h coll)
{
    if (coll == NULL) {
        return;
    }
    ucs_info("ucg_collective_destroy %p", coll);
    ucg_discard((ucg_op_t*)coll);
}

ucs_status_t ucg_worker_groups_init(void *groups_ctx)
{

    ucg_groups_t *gctx  = (ucg_groups_t*)groups_ctx;
    ucs_status_t status = ucg_plan_query(&gctx->planners, &gctx->num_planners);
    if (status != UCS_OK) {
        return status;
    }

    unsigned planner_idx;
    size_t group_ctx_offset  = sizeof(struct ucg_group);
    size_t worker_ctx_offset = ucg_ctx_worker_offset + sizeof(ucg_groups_t);
    for (planner_idx = 0; planner_idx < gctx->num_planners; planner_idx++) {
        ucg_plan_desc_t* planner            = &gctx->planners[planner_idx];
        ucg_plan_component_t* planc         = planner->plan_component;
        planc->worker_ctx_offset            = worker_ctx_offset;
        worker_ctx_offset                  += sizeof(void*);
        planc->group_ctx_offset             = group_ctx_offset;
        group_ctx_offset                   += planc->group_context_size;
    }

    gctx->next_id             = 0;
    gctx->iface_cnt           = 0;
    gctx->total_planner_sizes = group_ctx_offset;
    ucs_list_head_init(&gctx->groups_head);
    return UCS_OK;
}

void ucg_worker_groups_cleanup(void *groups_ctx)
{
    ucg_groups_t *gctx = (ucg_groups_t*)groups_ctx;

    ucg_group_h group = NULL;
    ucg_group_h tmp = NULL;
    if (!ucs_list_is_empty(&gctx->groups_head)) {
        ucs_list_for_each_safe(group, tmp, &gctx->groups_head, list) {
            ucg_group_destroy(group);
        }
    }

    ucg_plan_release_list(gctx->planners, gctx->num_planners);
}

ucs_status_t ucg_init_version(unsigned api_major_version,
                              unsigned api_minor_version,
                              const ucp_params_t *params,
                              const ucp_config_t *config,
                              ucp_context_h *context_p)
{
    ucs_status_t status = ucp_init_version(api_major_version, api_minor_version,
                                           params, config, context_p);
    if (status == UCS_OK) {
        size_t ctx_size = sizeof(ucg_groups_t) +
                ucs_list_length(&ucg_plan_components_list) * sizeof(void*);
        status = ucp_extend(*context_p, ctx_size, ucg_worker_groups_init,
                            ucg_worker_groups_cleanup, &ucg_ctx_worker_offset, &ucg_base_am_id);
    }
    ucs_info("ucg_init_version major %u minor %u", api_major_version, api_minor_version);
    return status;
}

ucs_status_t ucg_init(const ucp_params_t *params,
                      const ucp_config_t *config,
                      ucp_context_h *context_p)
{
    ucs_status_t status = ucp_init(params, config, context_p);
    if (status == UCS_OK) {
        size_t ctx_size = sizeof(ucg_groups_t) +
                ucs_list_length(&ucg_plan_components_list) * sizeof(void*);
        status = ucp_extend(*context_p, ctx_size, ucg_worker_groups_init,
                            ucg_worker_groups_cleanup, &ucg_ctx_worker_offset, &ucg_base_am_id);
    }
    ucs_info("ucg_init");
    return status;
}

ucs_status_t ucg_plan_connect(ucg_group_h group, ucg_group_member_index_t index,
                              uct_ep_h *ep_p, const uct_iface_attr_t **ep_attr_p, uct_md_h* md_p,
                              const uct_md_attr_t** md_attr_p, ucp_ep_h *ucp_ep_p)
{
    /* fill-in UCP connection parameters */
    size_t remote_addr_len;
    ucp_address_t *remote_addr = NULL;
    ucs_status_t status = group->params.resolve_address_f(group->params.cb_group_obj,
                                                          index, &remote_addr, &remote_addr_len);
    if (status != UCS_OK) {
        ucs_error("failed to obtain a UCP endpoint from the external callback");
        return status;
    }

    /* special case: connecting to a zero-length address means it's "debugging" */
    if (ucs_unlikely(remote_addr_len == 0)) {
        *ep_p = NULL;
        return UCS_OK;
    }

    /* create an endpoint for communication with the remote member */
    ucp_ep_h ucp_ep = NULL;
    ucp_ep_params_t ep_params = {
        .field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS,
        .address = remote_addr
    };

    status = ucp_ep_create(group->worker, &ep_params, &ucp_ep);
    if (status != UCS_OK) {
        goto connect_cleanup;
    }

    status = ucp_wireup_connect_remote(ucp_ep, ucp_ep_get_am_lane(ucp_ep));
    if (status != UCS_OK) {
        goto connect_cleanup;
    }

    while ((ucp_ep->flags & UCP_EP_FLAG_REMOTE_CONNECTED) == 0) {
        ucp_worker_progress(group->worker);
    }

    do {
        *ep_p = ucp_ep_get_am_uct_ep(ucp_ep);
        if (ucp_proxy_ep_test(*ep_p)) {
            ucp_proxy_ep_t *proxy_ep = ucs_derived_of(*ep_p, ucp_proxy_ep_t);
            *ep_p = proxy_ep->uct_ep;
        }

        ucs_assert((*ep_p)->iface != NULL);
        if ((*ep_p)->iface->ops.ep_am_short !=
            (typeof((*ep_p)->iface->ops.ep_am_short))ucs_empty_function_return_no_resource) {
            break;
        }
        ucp_worker_progress(group->worker);
    } while (1);

    /* Register interfaces to be progressed in future calls */
    ucg_groups_t *gctx = UCG_WORKER_TO_GROUPS_CTX(group->worker);
    UCG_GROUP_PROGRESS_ADD((*ep_p)->iface, group);
    UCG_GROUP_PROGRESS_ADD((*ep_p)->iface, gctx);

    *ep_attr_p = ucp_ep_get_am_iface_attr(ucp_ep);
    *md_p      = ucp_ep_get_am_uct_md(ucp_ep);
    *md_attr_p = ucp_ep_get_am_uct_md_attr(ucp_ep);
    *ucp_ep_p = ucp_ep;

connect_cleanup:
    group->params.release_address_f(remote_addr);
    return status;
}


ucs_status_t ucg_worker_create(ucp_context_h context,
                               const ucp_worker_params_t *params,
                               ucp_worker_h *worker_p)
{
    if (params == NULL || worker_p == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }
    ucs_info("ucg_worker_create");
    /* Once  worker is created, the ifaces in the worker may initialize and register handlers
    when it receives wireup messages. Therefore, ucg_builtin_am_handler should be registered before
    the worker is created. */
    ucp_am_handler_t* am_handler  = ucp_am_handlers + ucg_base_am_id;
    am_handler->features          = UCP_FEATURE_GROUPS;
    am_handler->cb                = ucg_builtin_am_handler;
    am_handler->tracer            = ucg_builtin_msg_dump;
    am_handler->flags             = 0;
    return ucp_worker_create(context, params, worker_p);
}