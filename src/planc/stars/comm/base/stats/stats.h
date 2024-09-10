/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_STATS_H_
#define UCG_STATS_H_

#include "scs.h"

#include "time/time.h"
#include "datastruct/queue.h"

BEGIN_C_DECLS

#define NS 1000000000


typedef struct ofd_stats_elem {
    struct {
        uint64_t total;
        struct {
            uint64_t total;
            uint64_t get_elem;
            uint64_t push_elem;
            struct {
                uint64_t total;
                uint64_t pre_req;
                uint64_t enque_task;
                uint64_t submit_task;
            } scp_submit;
        } submit;
    } trigger;
    struct {
        uint64_t total;
        uint64_t progress;
    } progress;

    uint64_t start_tick;
    uint64_t end_tick;
    uint64_t cost_time;
    uint32_t index;
    ucs_queue_elem_t queue_elem;
} ofd_stats_elem_t;
typedef struct ofd_stats_elem *ofd_stats_elem_h;

typedef struct ofd_algo_stats {
    uint8_t             nb_type;        /* 0 means nonblock, 1 means block */
    uint32_t            count;          /* run time */
    uint64_t            start_tick;
    uint64_t            end_tick;
    uint64_t            cost_time;
    ucs_queue_head_t    queue;          /* ofd_stats_elem_h queue */
    ofd_stats_elem_h    cur_elem;

    struct {
        uint64_t total;

        struct {
            uint64_t total;
            uint64_t plan;
            struct {
                uint64_t total;
                struct {
                    uint64_t total;
                    uint64_t exch_ep_get;
                    uint64_t exch_ep_put;
                    uint64_t wait_all;
                } exchg_addr;
                uint64_t get_proc;
                uint64_t do_conn;
            } conn;
        } pre_plan;

        struct {
            uint64_t total;
            struct {
                uint64_t total;
                uint64_t lrbuf_desc_alloc;
                uint64_t mem_reg;
                uint64_t mkey_pack;
                uint64_t notify_alloc;
            } init_rbuf;
            uint64_t init_sbuf;
            uint64_t exchg;
            uint64_t unpack_mkey;
        } exch_buf;
    } init;

    struct {
        uint64_t total;
        struct {
            uint64_t total;
            uint64_t event_free;
            uint64_t sbuf_deinit;
            struct {
                uint64_t total;
                uint64_t mem_dereg;
                uint64_t desc_free;
            } rbuf_deinit;
        } buf_cleanup;
        uint64_t op_cleanup;
        uint64_t op_destruct;
        uint64_t mpool_put;
    } discard;
} ofd_algo_stats_t;
typedef struct ofd_algo_stats *ofd_algo_stats_h;

/** Read generic counter frequency */
static inline uint64_t __rte_arm64_cntfrq(void)
{
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

/** Read generic counter */
static inline uint64_t __rte_arm64_cntvct(void)
{
    uint64_t tsc;
    asm volatile("mrs %0, cntvct_el0" : "=r"(tsc));
    return tsc;
}

static inline uint64_t __rte_arm64_cntvct_precise(void)
{
    asm volatile("isb" : : : "memory");
    return __rte_arm64_cntvct();
}

#ifdef ENABLE_STARS_STATS

#define UCG_STATS_SET_TIME(_result, _tick)          \
    _result = (_tick)

#define UCG_STATS_GET_TIME(_tick)                  \
    uint64_t _tick = __rte_arm64_cntvct_precise(); \
    ucs_compiler_fence()

#define UCG_STATS_START_TIME(_start_time)           \
    do {                                            \
        _start_time = __rte_arm64_cntvct_precise(); \
        ucs_compiler_fence();                       \
    } while (0)

#define UCG_STATS_UPDATE_TIME(_cost_time, _start_time)                                                     \
    do {                                                                                                   \
        ucs_compiler_fence();                                                                              \
        _cost_time = (uint64_t)(__rte_arm64_cntvct_precise() - (_start_time)) * NS / __rte_arm64_cntfrq(); \
    } while (0)

#define UCG_STATS_INC_TIME(_cost_time, _start_time)                                                         \
    do {                                                                                                    \
        ucs_compiler_fence();                                                                               \
        _cost_time += (uint64_t)(__rte_arm64_cntvct_precise() - (_start_time)) * NS / __rte_arm64_cntfrq(); \
    } while (0)

#define UCG_STATS_ADD_TIME(_start_time, _end_time, _cost_time)                             \
    do {                                                                                   \
        _cost_time += (uint64_t)((_end_time) - (_start_time)) * NS / __rte_arm64_cntfrq(); \
    } while (0)

#define UCG_STATS_COST_TIME(_start_time, _end_time, _cost_time)                           \
    do {                                                                                  \
        _cost_time = (uint64_t)((_end_time) - (_start_time)) * NS / __rte_arm64_cntfrq(); \
    } while (0)
void ucg_stats_dump(const char *name, ofd_algo_stats_h stats);

#else
#define UCG_STATS_SET_TIME(_result, _tick)
#define UCG_STATS_GET_TIME(_tick)
#define UCG_STATS_GET_TIME(_tick)
#define UCG_STATS_START_TIME(_start_time)
#define UCG_STATS_UPDATE_TIME(_cost_time, _start_time)
#define UCG_STATS_INC_TIME(_cost_time, _start_time)
#define UCG_STATS_ADD_TIME(_start_time, _end_time, _cost_time)
#define UCG_STATS_COST_TIME(_start_time, _end_time, _cost_time)

UCS_F_ALWAYS_INLINE void ucg_stats_dump(const char *name, ofd_algo_stats_h stats){};

#endif

END_C_DECLS

#endif
