/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_IB_VERBS_H
#define SCT_IB_VERBS_H

#include <infiniband/verbs.h>

#include "scs.h"

/* Read device properties */
#define IBV_DEV_ATTR(_dev, _attr)        ((_dev)->dev_attr.orig_attr._attr)

typedef struct ibv_device_attr_ex sct_ib_device_attr;

static inline ucs_status_t sct_ib_query_device(struct ibv_context *ctx,
                                               sct_ib_device_attr *attr)
{
    attr->comp_mask = 0;
    int ret = ibv_query_device_ex(ctx, NULL, attr);
    if (ret != 0) {
        ucg_error("ibv_query_device_ex(%s) returned %d: %m",
                  ibv_get_device_name(ctx->device), ret);
        return UCS_ERR_IO_ERROR;
    }

    return UCS_OK;
}

/*
 * On-demand paging support
 */
#define ibv_reg_mr_func_name          "ibv_reg_mr"

#if !HAVE_ODP
#  define IBV_ACCESS_ON_DEMAND          0
#endif

#define UCT_IB_HAVE_ODP_IMPLICIT(_attr)         ((_attr)->odp_caps.general_caps & IBV_ODP_SUPPORT_IMPLICIT)


/* Ethernet link layer */
#define IBV_PORT_IS_LINK_LAYER_ETHERNET(_attr)    ((_attr)->link_layer == IBV_LINK_LAYER_ETHERNET)

#define uct_ib_grh_required(_attr)                ((_attr)->flags & IBV_QPF_GRH_REQUIRED)

typedef uint8_t sct_ib_uint24_t[3];

static inline void sct_ib_pack_uint24(sct_ib_uint24_t buf, uint32_t val)
{
    buf[0] = (val >> 0)  & 0xFF;
    buf[1] = (val >> 8)  & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
}

static inline uint32_t sct_ib_unpack_uint24(const sct_ib_uint24_t buf)
{
    return buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
}

static inline void sct_ib_destroy_qp(struct ibv_qp *qp)
{
    int ret = ibv_destroy_qp(qp);
    if (ret) {
        ucg_warn("ibv_destroy_qp() failed: %m");
    }
}

#endif /* UCT_IB_VERBS_H */
