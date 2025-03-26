/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "ucg_rolling.h"
#include "util/ucg_helper.h"

void ucg_algo_rolling_iter_init(ucg_algo_rolling_iter_t *iter, ucg_rank_t myrank)
{
    ucg_assert(myrank != UCG_INVALID_RANK);
    iter->left = myrank;
    iter->right = myrank;
    iter->left_flag = 0;
    iter->right_flag = 0;
    iter->odd_step = 1;
    return;
}
