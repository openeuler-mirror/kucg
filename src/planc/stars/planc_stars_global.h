/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_GLOBAL_H_
#define UCG_PLANC_STARS_GLOBAL_H_

#include "planc_stars_plan.h"


typedef struct ucg_planc_stars {
    ucg_planc_t super;
    ucg_planc_stars_oob_resource_t oob_resource;
} ucg_planc_stars_t;

extern ucg_planc_stars_algo_table_t stars_algo_global_table[UCG_COLL_TYPE_LAST];

ucg_planc_stars_t *ucg_planc_stars_instance(void);

#endif

