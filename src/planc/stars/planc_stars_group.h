/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_PLANC_STARS_GROUP_H_
#define UCG_PLANC_STARS_GROUP_H_

#include "planc_stars_context.h"
#include "planc/ucg_planc.h"

typedef struct ucg_planc_stars_group {
    ucg_planc_group_t           super;
    ucg_planc_stars_context_t   *context;
    scp_ofd_stars_stream_t      stars_stream;
} ucg_planc_stars_group_t;

ucg_status_t ucg_planc_stars_group_create(ucg_planc_context_h context,
                                          const ucg_planc_group_params_t *params,
                                          ucg_planc_group_h *planc_group);
void ucg_planc_stars_group_destroy(ucg_planc_group_h planc_group);

#endif
