/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_SCP_MM_H
#define XUCG_SCP_MM_H

#include "scp_context.h"
#include "scp_worker.h"

#define SCT_MM_MAX_PACK_SIZE 8   /* SCT_IB_MD_PACKED_RKEY_SIZE */

/* scp_packed_size + num_mds + max_lane * (sct_packed_len + sct_packed_max_size) */
#define SCP_MM_PACK_SIZE (sizeof(uint32_t) + sizeof(uint8_t) + \
                          SCP_MAX_LANE * (sizeof(uint16_t) + SCT_MM_MAX_PACK_SIZE))

ucg_status_t scp_mem_reg(scp_context_h context, void *address,
                         size_t length, sct_mem_h **memh_p);

ucg_status_t scp_md_mkey_pack(scp_context_h context, sct_mem_h *memhs,
                              void *buffer, size_t *packed_size);

ucg_status_t scp_md_mkey_unpack(scp_context_h context, void *packed,
                                uct_rkey_bundle_t **rkeys, size_t *packed_size);

ucg_status_t scp_mem_dereg(scp_context_h context, uct_mem_h *memh);

#endif // XUCG_SCP_MM_H
