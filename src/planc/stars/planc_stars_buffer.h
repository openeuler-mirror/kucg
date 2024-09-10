/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_PLANC_STARS_BUFFER_H
#define XUCG_PLANC_STARS_BUFFER_H

#include "planc_stars_plan.h"

enum stars_buf_clean_flags {
    STARS_BUFF_NONE = UCS_BIT(0),
    STARS_BUFF_SEND = UCS_BIT(1),
    STARS_BUFF_RECV = UCS_BIT(2),
};

ucg_status_t ucg_planc_stars_sbuf_init(ucg_planc_stars_op_t *op, void *buffer, size_t length);
ucg_status_t ucg_planc_stars_rbuf_init(ucg_planc_stars_op_t *op, void *buffer, size_t length);
void ucg_planc_stars_buf_cleanup(ucg_planc_stars_op_t *op, uint8_t flags);

ucg_status_t ucg_planc_stars_exch_addr_msg(ucg_planc_stars_op_t *op);
ucg_status_t ucg_planc_stars_mkey_unpack(ucg_planc_stars_op_t *op);
#endif // XUCG_PLANC_STARS_BUFFER_H
