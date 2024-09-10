/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "stats.h"

ucs_stats_node_t *ucs_stats_get_root()
{
    return NULL;
}

#ifdef ENABLE_STARS_STATS
void ucg_stats_dump(const char *name, ofd_algo_stats_h stats)
{
    ucg_info(
        "[%s: %d once init] {"
          "\"cost_time\": {"
            "\"sum\": %lu,"
            "\"start_tick\": %lu,"
            "\"end_tick\": %lu,"
            "\"cnt\": %d,"
            "\"init\": {"
              "\"sum\": %lu,"
              "\"pre_plan\": {"
                "\"sum\": %lu,"
                "\"plan\": %lu,"
                "\"conn\": {"
                  "\"sum\": %lu,"
                  "\"exchg_addr\": {"
                    "\"sum\": %lu,"
                    "\"exch_ep_get\": %lu,"
                    "\"exch_ep_put\": %lu,"
                    "\"wait_all\": %lu"
                  "},"
                  "\"get_proc\": %lu,"
                  "\"do_conn\": %lu"
                "}"
              "},"
              "\"exch_buf\": {"
                "\"sum\": %lu,"
                "\"init_rbuf\": {"
                  "\"sum\": %lu,"
                  "\"lrbuf_desc_alloc\": %lu,"
                  "\"mem_reg\": %lu,"
                  "\"mkey_pack\": %lu"
                "},"
                "\"init_sbuf\": %lu,"
                "\"exchg\": %lu,"
                "\"unpack_mkey\": %lu"
              "}"
            "},"
            "\"discard\": {"
              "\"sum\": %lu,"
              "\"buf_cleanup\": {"
                "\"sum\": %lu,"
                "\"event_free\": %lu,"
                "\"sbuf_deinit\": %lu,"
                "\"rbuf_deinit\": {"
                  "\"sum\": %lu,"
                  "\"mem_dereg\": %lu,"
                  "\"desc_free\": %lu"
                "}"
              "},"
              "\"op_cleanup\": %lu,"
              "\"op_destruct\": %lu,"
              "\"mpool_put\": %lu"
            "}"
          "}"
        "}",
        /* total cost */
        name, (int)stats->nb_type,
        stats->cost_time,
        stats->start_tick, stats->end_tick, stats->count,
        /* init phase */
        stats->init.total,
        /* [init] prepare offload plan */
        stats->init.pre_plan.total, stats->init.pre_plan.plan,
        stats->init.pre_plan.conn.total,
        stats->init.pre_plan.conn.exchg_addr.total,
        stats->init.pre_plan.conn.exchg_addr.exch_ep_get,
        stats->init.pre_plan.conn.exchg_addr.exch_ep_put,
        stats->init.pre_plan.conn.exchg_addr.wait_all,
        stats->init.pre_plan.conn.get_proc,
        stats->init.pre_plan.conn.do_conn,
        /* [init] exchange buffer and event id */
        stats->init.exch_buf.total,
        stats->init.exch_buf.init_rbuf.total,
        stats->init.exch_buf.init_rbuf.lrbuf_desc_alloc,
        stats->init.exch_buf.init_rbuf.mem_reg,
        stats->init.exch_buf.init_rbuf.mkey_pack,
        stats->init.exch_buf.init_sbuf, stats->init.exch_buf.exchg,
        stats->init.exch_buf.unpack_mkey,
        /* discard phase */
        stats->discard.total,
        /* [discard] buf_cleanup */
        stats->discard.buf_cleanup.total,
        /* [discard] [buf_cleanup] */
        stats->discard.buf_cleanup.event_free,
        stats->discard.buf_cleanup.sbuf_deinit,
        stats->discard.buf_cleanup.rbuf_deinit.total,
        stats->discard.buf_cleanup.rbuf_deinit.mem_dereg,
        stats->discard.buf_cleanup.rbuf_deinit.desc_free,
        stats->discard.op_cleanup,
        stats->discard.op_destruct,
        stats->discard.mpool_put);

    ucs_queue_iter_t iter;
    ofd_stats_elem_h elem;
    ucs_queue_for_each_safe(elem, iter, &stats->queue, queue_elem) {
        ucg_info(
            "[%s once trigger] {"
              "\"idx\": %d,"
              "\"start_tick\": %lu,"
              "\"end_tick\": %lu,"
              "\"cost\": {"
                "\"sum\": %lu,"
                "\"trigger\": {"
                  "\"sum\": %lu,"
                  "\"submit\": {"
                    "\"sum\": %lu,"
                    "\"get_elem\": %lu,"
                    "\"push_elem\": %lu,"
                    "\"scp_submit\": {"
                      "\"sum\": %lu,"
                      "\"pre_req\": %lu,"
                      "\"enque_task\": %lu,"
                      "\"submit_task\": %lu"
                    "}"
                  "}"
                "},"
                "\"progress\": {"
                  "\"sum\": %lu,"
                  "\"progress\": %lu"
                "}"
              "}"
            "}",
            name, elem->index,
            elem->start_tick,
            elem->end_tick,
            elem->cost_time,
            /* trigger phase */
            elem->trigger.total,
            /* [trigger] submit */
            elem->trigger.submit.total,
            /* [trigger] [submit] */
            elem->trigger.submit.get_elem, elem->trigger.submit.push_elem,
            elem->trigger.submit.scp_submit.total,
            elem->trigger.submit.scp_submit.pre_req,
            elem->trigger.submit.scp_submit.enque_task,
            elem->trigger.submit.scp_submit.submit_task,
            /* progress phase */
            elem->progress.total, elem->progress.progress);
        ucs_queue_del_iter(&stats->queue, iter);
        ucg_mpool_put(elem);
    }
}
#endif
