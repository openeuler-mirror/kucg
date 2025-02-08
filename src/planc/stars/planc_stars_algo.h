/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_PLANC_STARS_ALGO_H_
#define XUCG_PLANC_STARS_ALGO_H_

#include "planc_stars_offload.h"
#include "planc_stars_wireup.h"
#include "planc_stars_buffer.h"

#define UCG_STARS_ALGO_PRE_DEFINE(_algorithm) \
    ucg_status_t \
    UCG_STARS_ALGO_PRE_NAME(_algorithm)(ucg_vgroup_t *_vgroup, \
                                        const ucg_coll_args_t *_args, \
                                        ucg_plan_op_t **_op) \
    { \
        UCG_CHECK_NULL_INVALID(_vgroup, _args, _op); \
      \
        ucg_status_t _status; \
      \
        if (UCG_STARS_ALGO_FUN(_algorithm, check)(_vgroup, _args) != UCG_OK) { \
            return UCG_ERR_UNSUPPORTED; \
        } \
        ucg_planc_stars_group_t *_stars_group \
            = ucg_derived_of(_vgroup, ucg_planc_stars_group_t); \
        ucg_planc_stars_op_t *_stars_op \
            = ucg_mpool_get(&_stars_group->context->op_mp); \
        if (_stars_op == NULL) { \
            return UCG_ERR_NO_MEMORY; \
        } \
        memset(_stars_op, 0, sizeof(ucg_planc_stars_op_t)); \
        _status = UCG_CLASS_CONSTRUCT(ucg_plan_op_t, &_stars_op->super, _vgroup, \
                                      UCG_STARS_ALGO_FUN(_algorithm, trigger),  \
                                      UCG_STARS_ALGO_FUN(_algorithm, progress), \
                                      UCG_STARS_ALGO_FUN(_algorithm, discard),  \
                                      _args); \
        if (_status != UCG_OK) { \
            ucg_error("Failed to construct stars algorithm[%s] operation", #_algorithm); \
            goto _err_free_op; \
        } \
      \
        _status = ucg_planc_stars_op_init(_stars_op, _stars_group); \
        if (_status != UCG_OK) { \
            ucg_error("Failed to init stars algorithm[%s] operation", #_algorithm); \
            goto _err_free_op; \
        } \
      \
        *_op = &_stars_op->super; \
      \
        UCG_STATS_GET_TIME(_init_tick); \
        _status = UCG_STARS_ALGO_FUN(_algorithm, init)(*_op); \
        if (_status != UCG_OK) { \
            ucg_error("Failed to init algorithm[%s]", #_algorithm); \
            goto _err_free_op; \
        } \
        UCG_STATS_UPDATE_TIME(_stars_op->stats.init.total, _init_tick); \
      \
        ucg_debug("successfully prepre planc stars algorithm[%s]", #_algorithm); \
        return UCG_OK; \
      \
    _err_free_op: \
        ucg_mpool_put(_stars_op); \
        return _status; \
    }

typedef ucg_status_t (*ucg_stars_algo_init_buf_fun_t)(ucg_planc_stars_op_t *op,
                                                      ucg_coll_args_t *coll_args);
typedef size_t (*ucg_stars_max_put_size_fun_t)(ucg_planc_stars_op_t *op,
                                               ucg_coll_args_t *coll_args);

typedef ucg_status_t (*ucg_stars_ofd_plan_fun_t)(ucg_planc_stars_op_t *op);

ucg_status_t ucg_planc_stars_algo_prepare_plan(ucg_planc_stars_op_t *op,
                                               ucg_coll_args_t *coll_args,
                                               ucg_stars_max_put_size_fun_t max_put_size,
                                               ucg_stars_ofd_plan_fun_t ofd_plan);

ucg_status_t ucg_planc_stars_algo_exch_buf_addr(ucg_planc_stars_op_t *op,
                                                ucg_coll_args_t *coll_args,
                                                ucg_stars_algo_init_buf_fun_t init_rbuf,
                                                ucg_stars_algo_init_buf_fun_t init_sbuf);

typedef ucg_status_t (*ucg_planc_stars_progress_done_func_t)(ucg_planc_stars_op_t *op);

ucg_status_t ucg_planc_stars_algo_progress(ucg_plan_op_t *ucg_op, ucg_planc_stars_progress_done_func_t cb);

#endif