/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */

#ifndef XUCG_STARS_PROTOCOL_CONTEXT_H
#define XUCG_STARS_PROTOCOL_CONTEXT_H

#include "scp.h"


/**
 * Memory domain.
 */
typedef struct scp_tl_md {
    sct_md_h                      md;         /* Memory domain handle */
    scp_rsc_index_t               cmpt_index; /* Index of owning component */
    uct_md_resource_desc_t        rsc;        /* Memory domain resource */
    uct_md_attr_t                 attr;       /* Memory domain attributes */
} scp_tl_md_t;

typedef struct scp_tl_cmpt {
    sct_component_h               cmpt;      /* UCT component handle */
    uct_component_attr_t          attr;      /* UCT component attributes */
} scp_tl_cmpt_t;

/**
 * UCP communication resource descriptor
 */
typedef struct scp_tl_resource_desc {
    sct_tl_resource_desc_t        tl_rsc;       /* UCT resource descriptor */
    scp_md_index_t                md_index;     /* Memory domain index (within the context) */
    scp_rsc_index_t               dev_index;    /* Arbitrary device index. Resources
                                                   with same index have same device name. */
    uint8_t                       flags;        /* Flags that describe resource specifics */
} scp_tl_resource_desc_t;

typedef struct scp_context_config {
    /** Maximal size of worker name for debugging */
    unsigned                                max_worker_name;
    /** If use mutex for MT support or not */
    int                                     use_mt_mutex;
    uint8_t 							   	multirails;
    size_t 								    multirails_threshold;
    int                                     scp_ep_cache_enabled;
    size_t                                  stream_depth;
} scp_context_config_t;

typedef struct scp_config {
    /** Array of device lists names to use.
     *  This array holds three lists - network devices, shared memory devices
     *  and acceleration devices */
    ucs_config_names_array_t               devices[UCT_DEVICE_TYPE_LAST];
    /** Array of transport names to use */
    ucs_config_names_array_t               tls;
    uint8_t                                is_all_tls;

    /** This config environment prefix */
    char                                   *env_prefix;

    /** Configuration saved directly in the context */
    scp_context_config_t                   ctx;
} scp_config_t;

typedef struct scp_config *scp_config_h;

/**
 * Stars communication protocol context
 */
typedef struct scp_context {
    scp_tl_cmpt_t                           *tl_cmpts;  /* UCT components */
    scp_rsc_index_t                         num_cmpts;  /* Number of UCT components */

    scp_tl_md_t                             *tl_mds;    /* Memory domain resources */
    scp_md_index_t                          num_mds;    /* Number of memory domains */

    scp_tl_resource_desc_t                  *tl_rscs;   /* Array of communication resources */
    scp_rsc_index_t                         num_tls;    /* Number of resources in the array */
    uint64_t                                tl_bitmap;  /* Cached map of tl resources used by workers.
                                                         * Not all resources may be used if unified
                                                         * mode is enabled. */

    uint8_t                                 lane_num;   /* lane number per scp ep */
    scp_config_t                            *scp_config;
    /* All configurations about multithreading support */
    ucs_recursive_spinlock_t                 mt_lock;
} scp_context_t;

typedef struct scp_context *scp_context_h;

ucg_status_t scp_config_read(const char *env_prefix, scp_config_t **config_p);

ucg_status_t scp_context_init(const ucp_context_h ucp_context, scp_context_h *context_p);

void scp_cleanup(scp_context_h context);

static UCS_F_ALWAYS_INLINE void scp_enable_multi_rail(scp_context_h context, size_t send_size)
{
    scp_config_h config = context->scp_config;

    if (send_size < config->ctx.multirails_threshold) {
        context->lane_num = 1;
        return;
    }

    if (config->ctx.multirails == 0) {
        ucg_warn("Invalid MAX_RAILS 0, which should be 1 at least,"
                 " automatically set to 1");
        context->lane_num = 1;
    } else if (config->ctx.multirails > SCP_MAX_LANE) {
        ucg_warn("Invalid MAX_RAILS %d, which should be "
        "less than max lanes %d, automatically set to %d",
        config->ctx.multirails, SCP_MAX_LANE,
        SCP_MAX_LANE);
        context->lane_num = SCP_MAX_LANE;
    }

    context->lane_num = config->ctx.multirails;
}

UCS_F_ALWAYS_INLINE uint8_t
scp_context_is_rsc_match(scp_context_h context, scp_rsc_index_t idx,
                         const char* tl_name)
{
    return !strcmp(context->tl_rscs[idx].tl_rsc.tl_name, tl_name);
}

#endif // XUCG_STARS_PROTOCOL_CONTEXT_H
