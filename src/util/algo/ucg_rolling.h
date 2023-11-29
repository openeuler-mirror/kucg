/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef UCG_ALGO_ROLLING_H_
#define UCG_ALGO_ROLLING_H_

#include "ucg/api/ucg.h"


/**
 * @brief Rolling algorithm iterator
 */
typedef struct ucg_algo_rolling_iter {
    ucg_rank_t left;
    ucg_rank_t right;
    ucg_rank_t left_flag;
    ucg_rank_t right_flag;
    int odd_step;
} ucg_algo_rolling_iter_t;

/**
 * @brief Initialize iterator of rolling algorithm
 *
 * At every step i (0 <= i < size), my left and right are not changed.
 */
void ucg_algo_rolling_iter_init(ucg_algo_rolling_iter_t *iter, ucg_rank_t myrank);

/**
 * @brief Reset the iterator to the beginning.
 */
static inline void ucg_algo_rolling_iter_reset(ucg_algo_rolling_iter_t *iter)
{
    iter->odd_step = 1;
    return;
}

static inline int ucg_algo_rolling_iter_get_odd_step(ucg_algo_rolling_iter_t *iter)
{
    return iter->odd_step;
}

/**
 * @brief move to the next iteration.
 */
static inline void ucg_algo_rolling_iter_next(ucg_algo_rolling_iter_t *iter)
{
    iter->odd_step = !iter->odd_step;
    return;
}

/**
 * @brief Get the current step left index.
 */
static inline int ucg_algo_rolling_iter_left_idx(ucg_algo_rolling_iter_t *iter)
{
    return iter->left;
}

/**
 * @brief Get the current step right index.
 */
static inline int ucg_algo_rolling_iter_right_idx(ucg_algo_rolling_iter_t *iter)
{
    return iter->right;
}

static inline int ucg_algo_rolling_iter_full(ucg_algo_rolling_iter_t *iter, int size)
{
    if (iter->left == (iter->right + 1) % size) {
        return 1;
    }
    return 0;
}

/**
 * @brief Upgrade my left index.
 */
static inline int ucg_algo_rolling_iter_left_inc(ucg_algo_rolling_iter_t *iter, uint32_t size)
{
    iter->left = (iter->left + size - 1) % size;
    return iter->left;
}

/**
 * @brief Upgrade my right index.
 */
static inline int ucg_algo_rolling_iter_right_inc(ucg_algo_rolling_iter_t *iter, uint32_t size)
{
    iter->right = (iter->right + 1) % size;
    return iter->right;
}

#endif