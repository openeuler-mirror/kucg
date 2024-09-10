/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_BARRIER_H_
#define UCG_PLANC_STARS_BARRIER_H_

#include "planc_stars_global.h"

typedef struct ucg_planc_stars_barrier_config {
    int fanin_degree;
    int fanout_degree;
} ucg_planc_stars_barrier_config_t;

UCG_STARS_ALGO_PRE_DECLARE(barrier_faninfanout);

#endif // UCG_PLANC_STARS_BARRIER_H_