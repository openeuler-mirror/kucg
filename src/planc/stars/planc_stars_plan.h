/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_PLAN_H_
#define UCG_PLANC_STARS_PLAN_H_

#include "planc_stars_oob.h"

#include "algorithm/planc_stars_algo_def.h"


#ifndef UCG_PLANC_STARS_DEFAULT_SCORE
    #define UCG_PLANC_STARS_DEFAULT_SCORE 20
#endif

typedef enum {
    EVENT_USING,
    EVENT_CLEARED,
} event_clear_status_t;

#define UCG_PLANC_STARS_ALGO_REGISTER(_coll_type, _config, _size) \
    UCG_STATIC_INIT { \
        stars_algo_global_table[_coll_type].config_table = _config; \
        stars_algo_global_table[_coll_type].size = _size; \
    } \
    UCG_STATIC_CLEANUP { \
        stars_algo_global_table[_coll_type].config_table = NULL; \
        stars_algo_global_table[_coll_type].size = 0; \
    }

typedef struct ucg_planc_stars_op {
    ucg_plan_op_t               super;
    ucg_plan_op_t               *meta_op;
    ucg_planc_stars_group_t     *stars_group;
    ucg_planc_stars_p2p_state_t p2p_state;
    ucg_mpool_t                 ofd_req_elem_pool;
    scp_ofd_req_t               ofd_req;

    uint16_t    tag;
    /* Abstracted fields, the concrete op determines how to use these. */
    uint64_t    flags;
    void        *staging_area;
    uint32_t    staging_size;

    union {
        ucg_planc_stars_bcast_t         bcast;
        ucg_planc_stars_allgatherv_t    allgatherv;
        ucg_planc_stars_scatterv_t      scatterv;
        ucg_planc_stars_barrier_t       barrier;
        ucg_planc_stars_alltoallv_t     alltoallv;
    };

    stars_comm_plan_t           plan;
    ofd_algo_stats_t            stats;
    event_clear_status_t        event_clear_flag;
} ucg_planc_stars_op_t;

UCS_F_ALWAYS_INLINE ucg_planc_stars_context_t* ucg_planc_stars_plan_get_context(ucg_planc_stars_op_t *op)
{
    return op->stars_group->context;
}

static UCS_F_ALWAYS_INLINE ucg_status_t
ucg_planc_stars_plan_get_stats_elem(ucg_planc_stars_op_t *op)
{
#ifdef ENABLE_STARS_STATS
    ofd_stats_elem_h elem
        = ucg_mpool_get(&ucg_planc_stars_plan_get_context(op)->stats_pool);
    UCG_ASSERT_RET(elem != NULL, UCG_ERR_NO_MEMORY);

    ucs_queue_push(&op->stats.queue, &elem->queue_elem);
    op->stats.cur_elem = elem;
    elem->index = op->stats.count++;
#endif
    return UCG_OK;
}

static inline void ucg_planc_stars_op_result(ucg_planc_stars_op_t *op,
                                             ucg_status_t status)
{
    op->super.super.status = status;
}

typedef struct ucg_planc_stars_algo_table {
    ucg_config_field_t  *config_table;
    size_t              size;
} ucg_planc_stars_algo_table_t;
UCG_PLAN_ATTR_TABLE_DECLARE(ucg_planc_stars);

static inline ucg_status_t ucg_planc_stars_op_init(ucg_planc_stars_op_t *op,
                                                   ucg_planc_stars_group_t *group)
{
#ifdef ENABLE_STARS_STATS
    memset(&op->stats, 0, sizeof(ofd_algo_stats_t));
#endif
    UCG_STATS_START_TIME(op->stats.start_tick);

    op->stars_group         = group;
    op->tag                 = 0;
    op->flags               = 0;
    op->staging_area        = NULL;
    op->event_clear_flag    = EVENT_USING;
    op->meta_op             = NULL;
    ucs_queue_head_init(&op->stats.queue);
    ucg_planc_stars_p2p_state_reset(&op->p2p_state);
    scp_init_ofd_req(&op->ofd_req);
    return ucg_mpool_init(&op->ofd_req_elem_pool, 0, sizeof(scp_ofd_req_elem_t),
                          0, UCG_CACHE_LINE_SIZE, UCG_ELEMS_PER_CHUNK,
                          UINT_MAX, NULL, "ofd_req_elem_pool");
}

static inline void ucg_planc_stars_op_reset(ucg_planc_stars_op_t *op)
{
    ucg_planc_stars_p2p_state_reset(&op->p2p_state);

    op->super.super.status  = UCG_INPROGRESS;
    op->tag                 = op->super.super.id;
    op->flags               = 0;
}

static inline scp_ofd_req_elem_h ucg_planc_stars_op_get_ofd_req_elem(ucg_planc_stars_op_t *op)
{
    UCG_STATS_GET_TIME(start_tick);
    scp_ofd_req_elem_h elem = (scp_ofd_req_elem_h)ucg_mpool_get(&op->ofd_req_elem_pool);
    elem->flag = 0;
    elem->unnotify = 0;
    UCG_STATS_INC_TIME(op->stats.cur_elem->trigger.submit.get_elem, start_tick);
    return elem;
}

static inline void ucg_planc_stars_op_push_ofd_req_elem(ucg_planc_stars_op_t *op,
                                                        scp_ofd_req_elem_h req)
{
    UCG_STATS_GET_TIME(start_tick);
    ucs_queue_push(&op->ofd_req.queue_head, &req->queue_elem);
    op->ofd_req.req_elem_cnt++;
    op->ofd_req.last_req_elem = req;
    UCG_STATS_INC_TIME(op->stats.cur_elem->trigger.submit.push_elem, start_tick);
}

static inline void* ucg_planc_stars_plan_alloc_buf_desc(stars_rank_info_h rank)
{
    uint32_t elem_num = rank->length;
    return ucg_malloc(BUF_DESC_HEAD_SIZE + STARS_EVENT_ELEM_SIZE(elem_num), "alloc_buf_desc");
}

static inline void* ucg_planc_stars_plan_alloc_local_buf_desc(ucg_planc_stars_op_t *op)
{
    return ucg_mpool_get(&op->stars_group->context->msg_mp);
}

static inline void ucg_planc_stars_rkey_bundle_cleanup(ucg_planc_stars_op_t *op)
{
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    for (uint32_t index = 0; index < comm_dep->put_rank_num; ++index) {
        stars_rank_info_h peer_rank = &comm_dep->put_ranks[index];
        if (peer_rank->rkey_bundle != NULL) {
            ucg_free(peer_rank->rkey_bundle);
        }
        peer_rank->rkey_bundle = NULL;
    }
}

static inline void ucg_planc_stars_put_rank_desc_cleanup(ucg_planc_stars_op_t *op)
{
    stars_comm_dep_h comm_dep = &op->plan.comm_dep;
    for (uint32_t index = 0; index < comm_dep->put_rank_num; ++index) {
        stars_rank_info_h peer_rank = &comm_dep->put_ranks[index];
        if (peer_rank->rbuf_desc != NULL) {
            ucg_free(peer_rank->rbuf_desc);
            peer_rank->rbuf_desc = NULL;
        }
    }
}

static inline void ucg_planc_stars_set_p2p_params(ucg_planc_stars_op_t *op,
                                                  ucg_planc_stars_p2p_params_t *params)
{
    params->stars_group     = op->stars_group;
    params->state           = &op->p2p_state;
}

static inline int32_t log2_stars_n(int32_t n, int32_t begin)
{
    int32_t index = 0;
    while (n >= begin) {
        n >>= 1;
        ++index;
    }
    return index;
}

static inline ucg_planc_stars_ppn_level_t ucg_planc_stars_get_ppn_level(int32_t ppn)
{
    const int ppn_lev_small = 4;
    const int ppn_lev_large = 64;
    if (ppn == 1) {
        return PPN_LEVEL_STARS_1;
    } else if (ppn <= ppn_lev_small) {
        return PPN_LEVEL_STARS_4;
    } else if (ppn > ppn_lev_large) {
        return PPN_LEVEL_STARS_LG;
    }
    return (ucg_planc_stars_ppn_level_t)log2_stars_n(ppn, ppn_lev_small);
}

static inline ucg_planc_stars_node_level_t ucg_planc_stars_get_node_level(int32_t node_cnt)
{
    const int node_lev_small = 4;
    const int node_lev_large = 16;
    if (node_cnt <= node_lev_small) {
        return NODE_LEVEL_STARS_4;
    }
    if (node_cnt > node_lev_large) {
        return NODE_LEVEL_STARS_LG;
    }
    return (ucg_planc_stars_node_level_t)log2_stars_n(node_cnt - 1, node_lev_small);
}

ucg_status_t ucg_planc_stars_op_discard(ucg_plan_op_t *ucg_op);

ucg_status_t ucg_planc_stars_get_plans(ucg_planc_group_h planc_group, ucg_plans_t *plans);

#endif
