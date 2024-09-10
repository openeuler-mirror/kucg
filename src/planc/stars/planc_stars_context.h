/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_CONTEXT_H_
#define UCG_PLANC_STARS_CONTEXT_H_

#include "scp_context.h"
#include "scp_ep.h"


#define UCG_PLANC_STARS_CONTEXT_BUILTIN_CONFIG_BUNDLE(_context, _coll_name, _coll_type) \
    (ucg_planc_stars_##_coll_name##_config_t *)\
    UCG_PLANC_STARS_CONTEXT_CONFIG_BUNDLE\
    (_context, _coll_name, _coll_type, STARS_BUILTIN)

#define UCG_PLANC_STARS_CONTEXT_CONFIG_BUNDLE(_context, _coll_name, _coll_type, _module_type) \
    _context->config.config_bundle[_coll_type]->data

KHASH_INIT(scp_ep, ucg_rank_t, scp_ep_h, 1,
           kh_int_hash_func, kh_int_hash_equal);

#define EVENTS_POOL_SIZE 5
#define MAX_STARS_DEV_NUM 4

typedef struct ucg_planc_stars_config_bundle {
    ucg_config_field_t                  *table;
    char                                data[];
} ucg_planc_stars_config_bundle_t;

typedef struct ucg_planc_stars_config {
    ucg_planc_stars_config_bundle_t     *config_bundle[UCG_COLL_TYPE_LAST];

    /** Attributes of collective operation plans */
    char                                *plan_attr[UCG_COLL_TYPE_LAST];

    int                                 n_polls;
    int                                 estimated_num_ppn;
} ucg_planc_stars_config_t;

typedef struct ucg_planc_stars_context {
    ucg_context_t                       *super;
    ucg_mpool_t                         op_mp;
    ucg_mpool_t                         msg_mp;

    scp_context_h                       scp_context;
    scp_worker_h                        scp_worker;

    scp_address_t                       *scp_worker_addr;
    size_t                              scp_worker_addr_len;

    /** User-defined plan attribute */
    ucg_plan_policy_t                   *user_policy[UCG_COLL_TYPE_LAST];

    ucg_planc_stars_config_t            config;

    khash_t(scp_ep)                     eps_pool;

    events_pool_h                       events_pool[MAX_STARS_DEV_NUM];
#ifdef ENABLE_STARS_STATS
    ucg_mpool_t                         stats_pool;
#endif
} ucg_planc_stars_context_t;

/** Configuration */
ucg_status_t ucg_planc_stars_config_read(const char *env_prefix,
                                         const char *filename,
                                         ucg_planc_config_h *config);

ucg_status_t ucg_planc_stars_config_modify(ucg_planc_config_h config,
                                           const char *name,
                                           const char *value);

void ucg_planc_stars_config_release(ucg_planc_config_h config);

/** Context */
ucg_status_t ucg_planc_stars_context_init(const ucg_planc_params_t *params,
                                          const ucg_planc_config_h config,
                                          ucg_planc_context_h *context);

ucg_status_t ucg_planc_stars_context_query(ucg_planc_context_h context,
                                           ucg_planc_context_attr_t *attr);

int ucg_planc_stars_context_progress(ucg_planc_context_h context);

void ucg_planc_stars_context_cleanup(ucg_planc_context_h context);

ucg_status_t ucg_planc_stars_mem_query(const void *ptr, ucg_mem_attr_t *attr);

#endif
