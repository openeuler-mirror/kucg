/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_SCATTER_H_
#define UCG_PLANC_STARS_SCATTER_H_

#include "planc_stars_global.h"

typedef struct ucg_planc_stars_scatterv_config {
    /* configuration of kntree scatterv */
    int kntree_degree;
    int8_t run_hpl;
} ucg_planc_stars_scatterv_config_t;

UCG_STARS_ALGO_PRE_DECLARE(scatterv_kntree);

#endif // UCG_PLANC_STARS_SCATTER_H_
