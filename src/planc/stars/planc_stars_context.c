/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "planc_stars_context.h"
#include "planc_stars_global.h"


#define PLANC_STARS_CONFIG_PREFIX "PLANC_STARS_"

static ucg_config_field_t ucg_planc_stars_config_table[] = {
    {"BCAST_ATTR", "",
     "Configure the bcast algorithm for different scenario",
     ucg_offsetof(ucg_planc_stars_config_t,  plan_attr[UCG_COLL_TYPE_BCAST]),
     UCG_CONFIG_TYPE_STRING},

    {"SCATTERV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_SCATTERV]),
     UCG_CONFIG_TYPE_STRING},

    {"ALLGATHERV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_ALLGATHERV]),
     UCG_CONFIG_TYPE_STRING},

    {"BARRIER_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_BARRIER]),
     UCG_CONFIG_TYPE_STRING},

    {"ALLTOALLV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_ALLTOALLV]),
     UCG_CONFIG_TYPE_STRING},

    {"IBCAST_ATTR", "",
     "Configure the ibcast algorithm for different scenario",
     ucg_offsetof(ucg_planc_stars_config_t,  plan_attr[UCG_COLL_TYPE_IBCAST]),
     UCG_CONFIG_TYPE_STRING},

    {"ISCATTERV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_ISCATTERV]),
     UCG_CONFIG_TYPE_STRING},

    {"IALLGATHERV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_IALLGATHERV]),
     UCG_CONFIG_TYPE_STRING},

    {"IBARRIER_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_IBARRIER]),
     UCG_CONFIG_TYPE_STRING},

    {"IALLTOALLV_ATTR", "", UCG_PLAN_ATTR_DESC,
     ucg_offsetof(ucg_planc_stars_config_t, plan_attr[UCG_COLL_TYPE_IALLTOALLV]),
     UCG_CONFIG_TYPE_STRING},

    {"NPOLLS", "10",
     "Number of ucp progress polling cycles for p2p requests testing",
     ucg_offsetof(ucg_planc_stars_config_t, n_polls),
     UCG_CONFIG_TYPE_UINT},

    {"ESTIMATED_NUM_PPN", "0",
     "An optimization hint of how many endpoints created on this context reside on the same node",
     ucg_offsetof(ucg_planc_stars_config_t, estimated_num_ppn),
     UCG_CONFIG_TYPE_UINT},

    {NULL}
};
UCG_CONFIG_REGISTER_TABLE(ucg_planc_stars_config_table, "UCG PlanC STARS", PLANC_STARS_CONFIG_PREFIX,
                          ucg_planc_stars_config_t, &ucg_config_global_list)

static ucg_status_t
ucg_planc_stars_config_bundle_read(ucg_planc_stars_config_bundle_t **bundle,
                                   ucg_config_field_t *config_table,
                                   size_t config_size, const char *env_prefix,
                                   const char *cfg_prefix)
{
    ucg_planc_stars_config_bundle_t *config_bundle;
    config_bundle = ucg_calloc(1, sizeof(*config_bundle) + config_size,
                               "ucg planc stars coll config");
    if (config_bundle == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    config_bundle->table = config_table;
    ucg_config_global_list_entry_t entry = {
        .table  = config_table,
        .prefix = cfg_prefix,
        .flags  = 0
    };
    ucg_status_t status = ucg_config_parser_fill_opts(config_bundle->data,
                                                      &entry,
                                                      env_prefix, 0);
    if (status != UCG_OK) {
        ucg_error("Failed to parse config opts");
        goto err_free_bundle;
    }

    *bundle = config_bundle;
    return UCG_OK;

err_free_bundle:
    ucg_free(config_bundle);
    return status;
}

static ucg_status_t ucg_planc_stars_fill_coll_config(ucg_planc_stars_config_t *cfg,
                                                     const char *full_env_prefix)
{
    ucg_status_t status = UCG_OK;

    for (ucg_coll_type_t coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        if (stars_algo_global_table[coll_type].config_table == NULL) {
            cfg->config_bundle[coll_type] = NULL;
            continue;
        }

        status = ucg_planc_stars_config_bundle_read(&cfg->config_bundle[coll_type],
                                                    stars_algo_global_table[coll_type].config_table,
                                                    stars_algo_global_table[coll_type].size,
                                                    full_env_prefix, PLANC_STARS_CONFIG_PREFIX);
        if (status != UCG_OK) {
            ucg_error("Failed to bundle read stars config for coll type %d", coll_type);
            break;
        }
    }

    return status;
}

ucg_status_t ucg_planc_stars_config_read(const char *env_prefix, const char *filename,
                                         ucg_planc_config_h *config)
{
    UCG_CHECK_NULL_INVALID(config);

    if (filename != NULL) {
        return UCG_ERR_UNSUPPORTED;
    }

    ucg_planc_stars_config_t *cfg = ucg_calloc(1, sizeof(ucg_planc_stars_config_t),
                                               "ucg planc stars config");
    if (cfg == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    ucg_status_t status;
    char *full_env_prefix;
    if (env_prefix == NULL) {
        full_env_prefix = ucg_strdup(UCG_DEFAULT_ENV_PREFIX, "default prefix");
        if (full_env_prefix == NULL) {
            status = UCG_ERR_NO_MEMORY;
            goto err_free_cfg;
        }
    } else {
        int full_env_prefix_len = strlen(env_prefix)
                                  + 1 /* '_' */
                                  + sizeof(UCG_DEFAULT_ENV_PREFIX);
        full_env_prefix = ucg_malloc(full_env_prefix_len, "ucg planc stars env prefix");
        if (full_env_prefix == NULL) {
            status = UCG_ERR_NO_MEMORY;
            goto err_free_cfg;
        }
        snprintf(full_env_prefix, full_env_prefix_len, "%s_%s", env_prefix, UCG_DEFAULT_ENV_PREFIX);
    }

    status = ucg_config_parser_fill_opts(cfg, UCG_CONFIG_GET_TABLE(ucg_planc_stars_config_table),
                                         full_env_prefix, 0);
    if (status != UCG_OK) {
        ucg_error("Failed to read PlanC STARS configuration");
        goto err_free_prefix;
    }

    status = ucg_planc_stars_fill_coll_config(cfg, full_env_prefix);
    ucg_free(full_env_prefix);
    if (status != UCG_OK) {
        ucg_error("Failed to read PlanC STARS bundle configuration");
        goto err_release_cfg;
    }

    *config = (ucg_planc_config_h)cfg;
    return UCG_OK;

err_free_prefix:
    ucg_free(full_env_prefix);
err_release_cfg:
    ucg_config_parser_release_opts(cfg, ucg_planc_stars_config_table);
err_free_cfg:
    ucg_free(cfg);
    return status;
}

ucg_status_t ucg_planc_stars_config_modify(ucg_planc_config_h config,
                                           const char *name, const char *value)
{
    UCG_CHECK_NULL_INVALID(config, name, value);

    ucg_planc_stars_config_t *cfg = (ucg_planc_stars_config_t*)config;
    ucg_status_t status = ucg_config_parser_set_value(cfg, ucg_planc_stars_config_table,
                                                      name, value);
    if (status == UCG_OK) {
        return UCG_OK;
    }

    ucg_coll_type_t coll_type;
    for (coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        if (cfg->config_bundle[coll_type] != NULL) {
            status = ucg_config_parser_set_value(cfg->config_bundle[coll_type]->data,
                                                 stars_algo_global_table[coll_type].config_table,
                                                 name, value);
            if (status == UCG_OK) {
                break;
            }
        }
    }

    if (coll_type == UCG_COLL_TYPE_LAST) {
        ucg_error("Failed to modify PlanC STARS configuration");
    }

    return status;
}

static void ucg_planc_stars_config_bundle_release(ucg_planc_stars_config_t *cfg)
{
    for (ucg_coll_type_t coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        if (cfg->config_bundle[coll_type] == NULL) {
            continue;
        }

        if (cfg->config_bundle[coll_type]->table != NULL) {
            ucg_config_parser_release_opts(cfg->config_bundle[coll_type]->data,
                                           cfg->config_bundle[coll_type]->table);
        }

        ucg_free(cfg->config_bundle[coll_type]);
    }
}

void ucg_planc_stars_config_release(ucg_planc_config_h config)
{
    UCG_CHECK_NULL_VOID(config);

    ucg_planc_stars_config_t *cfg = (ucg_planc_stars_config_t*)config;
    ucg_planc_stars_config_bundle_release(cfg);
    ucg_config_parser_release_opts(cfg, ucg_planc_stars_config_table);
    ucg_free(cfg);
}

static void ucg_planc_stars_context_free_config(ucg_planc_stars_context_t *ctx)
{
    ucg_planc_stars_config_t *cfg = &ctx->config;
    ucg_planc_stars_config_bundle_release(cfg);
    ucg_config_parser_release_opts(cfg, ucg_planc_stars_config_table);
}

static ucg_status_t ucg_planc_stars_context_fill_config(ucg_planc_stars_context_t *ctx,
                                                        ucg_planc_stars_config_t *cfg)
{
    ucg_status_t status;
    status = ucg_config_parser_clone_opts(cfg, &ctx->config, ucg_planc_stars_config_table);
    if (status != UCG_OK) {
        return status;
    }

    for (ucg_coll_type_t coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        if (cfg->config_bundle[coll_type] == NULL) {
            continue;
        }
        ctx->config.config_bundle[coll_type] = ucg_malloc(
            sizeof(ucg_planc_stars_config_bundle_t) + stars_algo_global_table[coll_type].size,
            "ucg planc stars config bundle");
        if (ctx->config.config_bundle[coll_type] == NULL) {
            status = UCG_ERR_NO_MEMORY;
            goto err_free_cfg;
        }
        status = ucg_config_parser_clone_opts(cfg->config_bundle[coll_type]->data,
                                              ctx->config.config_bundle[coll_type]->data,
                                              cfg->config_bundle[coll_type]->table);
        if (status != UCG_OK) {
            goto err_free_cfg;
        }
        ctx->config.config_bundle[coll_type]->table = cfg->config_bundle[coll_type]->table;
    }

    return UCG_OK;

err_free_cfg:
    ucg_planc_stars_context_free_config(ctx);
    return status;
}

static void ucg_planc_stars_context_free_policy(ucg_planc_stars_context_t *ctx)
{
    for (ucg_coll_type_t coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        if (ctx->user_policy[coll_type] != NULL) {
            ucg_plan_policy_destroy(&ctx->user_policy[coll_type]);
        }
    }
    return;
}

static ucg_status_t ucg_planc_stars_context_fill_policy(ucg_planc_stars_context_t *ctx,
                                                        ucg_planc_stars_config_t *cfg)
{
    ucg_status_t status;
    for (ucg_coll_type_t coll_type = 0; coll_type < UCG_COLL_TYPE_LAST; ++coll_type) {
        status = ucg_plan_policy_create(&ctx->user_policy[coll_type], cfg->plan_attr[coll_type]);
        if (status != UCG_OK) {
            return status;
        }
    }
    return UCG_OK;
}

ucg_status_t ucg_planc_stars_eps_pool_destroy(ucg_planc_stars_context_t *context)
{
    scp_ep_h ep;
    kh_foreach_value(&context->eps_pool, ep, {
        scp_ep_destroy(ep);
    })

    kh_destroy_inplace(scp_ep, &context->eps_pool);
    return UCG_OK;
}

ucg_status_t ucg_planc_stars_events_pool_init(ucg_planc_stars_context_t *context)
{
    events_pool_h event_pool;
    for (uint8_t i = 0; i < MAX_STARS_DEV_NUM; i++) {
        context->events_pool[i] = NULL;
    }

    for (uint8_t i = 0; i < MAX_STARS_DEV_NUM; i++) {
        event_pool = scs_stack_init(sct_event, EVENTS_POOL_SIZE);
        if (!event_pool) {
            return UCG_ERR_NO_MEMORY;
        }
        context->events_pool[i] = event_pool;
    }
    return UCG_OK;
}

ucg_status_t ucg_planc_stars_events_pool_destroy(ucg_planc_stars_context_t *context)
{
    sct_event_t event;
    events_pool_h event_pool;

    for (uint8_t i = 0; i < MAX_STARS_DEV_NUM; i++) {
        event_pool = context->events_pool[i];
        if (!event_pool) {
            continue;
        }
        while (!scs_stack_empty(sct_event, event_pool)) {
            scs_stack_top(sct_event, event_pool, &event);
            scs_stack_pop(sct_event, event_pool);
            sct_ep_free_event(&event, 0);
        }
        scs_stack_destroy(sct_event, event_pool);
    }

    return UCG_OK;
}

ucg_status_t ucg_planc_stars_context_init(const ucg_planc_params_t *params,
                                          const ucg_planc_config_h config,
                                          ucg_planc_context_h *context)
{
    UCG_CHECK_NULL_INVALID(params, config, context);
    ucg_status_t status;

    ucg_planc_stars_context_t *ctx =
        ucg_malloc(sizeof(ucg_planc_stars_context_t), "planc stars context");
    UCG_ASSERT_RET(ctx != NULL, UCG_ERR_NO_MEMORY);

    ctx->super                  = params->context;
    ctx->scp_worker_addr        = NULL;
    ctx->scp_worker_addr_len    = 0;

    ucg_planc_stars_config_t *cfg = (ucg_planc_stars_config_t *) config;
    status = ucg_planc_stars_context_fill_config(ctx, cfg);
    UCG_ASSERT_CODE_GOTO(status, err_free_ctx);

    status = ucg_planc_stars_context_fill_policy(ctx, cfg);
    UCG_ASSERT_CODE_GOTO(status, err_free_config);

    status = ucg_planc_stars_ucx_instance_init();
    UCG_ASSERT_CODE_GOTO(status, err_free_config);

    ucp_context_h ucp_context = ucg_planc_stars_get_ucx_ucp_context();
    UCG_ASSERT_GOTO(ucp_context != NULL, err_free_policy,
                    UCG_ERR_UNSUPPORTED);

    status = scp_context_init(ucp_context, &ctx->scp_context);
    UCG_ASSERT_CODE_GOTO(status, err_free_policy);

    status = scp_create_worker(ctx->scp_context, &ctx->scp_worker);
    UCG_ASSERT_CODE_GOTO(status, err_cleanup_context);

    status = ucg_mpool_init(&ctx->op_mp, 0, sizeof(ucg_planc_stars_op_t),
                            0, UCG_CACHE_LINE_SIZE, UCG_ELEMS_PER_CHUNK,
                            UINT_MAX, NULL, "planc stars op mpool");
    UCG_ASSERT_CODE_GOTO(status, err_destroy_worker);

    status = ucg_mpool_init(&ctx->msg_mp, 0, BUF_DESC_HEAD_SIZE,
                            0, UCG_CACHE_LINE_SIZE, UCG_ELEMS_PER_CHUNK,
                            UINT_MAX, NULL, "planc stars buf desc mpool");
    UCG_ASSERT_CODE_GOTO(status, err_destory_op_pool);

#ifdef ENABLE_STARS_STATS
    status = ucg_mpool_init(&ctx->stats_pool, 0, sizeof(ofd_stats_elem_t),
                            0, UCG_CACHE_LINE_SIZE, UCG_ELEMS_PER_CHUNK,
                            UINT_MAX, NULL, "planc stars stats mpool");
    UCG_ASSERT_CODE_GOTO(status, err_destory_msg_pool);
#endif

    kh_init_inplace(scp_ep, &ctx->eps_pool);
    ucg_planc_stars_events_pool_init(ctx);
    UCG_ASSERT_CODE_GOTO(status, err_destory_events_pool);
    *context = (ucg_planc_context_h)ctx;
    return UCG_OK;

err_destory_events_pool:
    ucg_planc_stars_events_pool_destroy(ctx);
#ifdef ENABLE_STARS_STATS
err_destory_msg_pool:
    ucg_mpool_cleanup(&ctx->msg_mp, 1);
#endif
err_destory_op_pool:
    ucg_mpool_cleanup(&ctx->op_mp, 1);
err_destroy_worker:
    scp_worker_destroy(ctx->scp_worker);
err_cleanup_context:
    scp_cleanup(ctx->scp_context);
err_free_policy:
    ucg_planc_stars_context_free_policy(ctx);
err_free_config:
    ucg_planc_stars_context_free_config(ctx);
err_free_ctx:
    ucg_free(ctx);
    return status;
}

ucg_status_t ucg_planc_stars_mem_query(const void *ptr, ucg_mem_attr_t *attr)
{
    UCG_CHECK_NULL_INVALID(ptr, attr);
    ucg_debug("Planc stars doesn't support mem type detection");
    return UCG_ERR_UNSUPPORTED;
}

void ucg_planc_stars_context_cleanup(ucg_planc_context_h context)
{
    UCG_CHECK_NULL_VOID(context);

    ucg_planc_stars_context_t *ctx = (ucg_planc_stars_context_t *)context;

    if (ctx->scp_worker_addr != NULL) {
        ucg_free(ctx->scp_worker_addr);
    }

#ifdef ENABLE_STARS_STATS
    ucg_mpool_cleanup(&ctx->stats_pool, 1);
#endif
    ucg_mpool_cleanup(&ctx->op_mp, 1);
    ucg_mpool_cleanup(&ctx->msg_mp, 1);
    ucg_planc_stars_context_free_policy(ctx);
    ucg_planc_stars_eps_pool_destroy(ctx);
    ucg_planc_stars_events_pool_destroy(ctx);
    scp_worker_destroy(ctx->scp_worker);
    scp_cleanup(ctx->scp_context);
    ucg_free(ctx);
}

ucg_status_t ucg_planc_stars_context_query(ucg_planc_context_h context,
                                           ucg_planc_context_attr_t *attr)
{
    UCG_CHECK_NULL_INVALID(context, attr);

    ucg_status_t status = UCG_OK;
    ucg_planc_stars_context_t *ctx = (ucg_planc_stars_context_t *)context;

    if ((attr->field_mask & (UCG_PLANC_CONTEXT_ATTR_FIELD_ADDR
        | UCG_PLANC_CONTEXT_ATTR_FIELD_ADDR_LEN))
        && ctx->scp_worker_addr == NULL) {
        status = scp_worker_get_address(ctx->scp_worker,
                                        &ctx->scp_worker_addr,
                                        &ctx->scp_worker_addr_len);
        if (status != UCG_OK) {
            ucg_error("Failed to get scp worker address, %s", ucg_status_string(status));
            goto out;
        }
    }

    if (attr->field_mask & UCG_PLANC_CONTEXT_ATTR_FIELD_ADDR_LEN) {
        attr->addr_len = ctx->scp_worker_addr_len;
    }

    if (attr->field_mask & UCG_PLANC_CONTEXT_ATTR_FIELD_ADDR) {
        attr->addr = ctx->scp_worker_addr;
    }

out:
    return status;
}

int ucg_planc_stars_context_progress(ucg_planc_context_h context)
{
    // must execute scp_worker_progress later when it support batch mode
    return 0;
}