/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "scp_mm.h"

#include "scp_ep.h"


ucg_status_t scp_mem_dereg(scp_context_h context, uct_mem_h *memh)
{
    scp_tl_md_t *tl_mds = context->tl_mds;
    scp_md_index_t tl_md_num = context->num_mds;
    ucg_status_t status;
    ucs_status_t ucs_status;
    for (scp_md_index_t index = 0; index < tl_md_num; ++index) {
        ucs_status = sct_md_mem_dereg(tl_mds[index].md, memh[index]);
        if (ucs_status != UCS_OK) {
            ucg_error("Failed to deregister memory: %s", ucs_status_string(ucs_status));
            status = ucg_status_s2g(ucs_status);
            return status;
        }
    }

    return UCG_OK;
}

ucg_status_t scp_mem_reg(scp_context_h context, void *address, size_t length, sct_mem_h **memh_p)
{
    scp_tl_md_t *tl_mds = context->tl_mds;
    scp_md_index_t tl_md_num = context->num_mds;
    ucg_status_t status;
    ucs_status_t ucs_status;
    sct_mem_h *memh;

    memh = ucg_malloc(tl_md_num * sizeof(sct_mem_h), "sct_mem_h array");
    if (memh == NULL) {
        ucg_error("Failed to alloc memory for mem register handler.");
        return UCG_ERR_NO_MEMORY;
    }

    for (scp_md_index_t index = 0; index < tl_md_num; ++index) {
        ucs_status = sct_md_mem_reg(tl_mds[index].md, address, length,
                                    UCT_MD_MEM_ACCESS_RMA, &memh[index]);
        if (ucs_status != UCS_OK) {
            ucg_error("Failed to register memory: %s", ucs_status_string(ucs_status));
            status = ucg_status_s2g(ucs_status);
            goto free_memh;
        }
    }

    *memh_p = memh;
    return UCG_OK;

free_memh:
    ucg_free(memh);
    return status;
}

ucg_status_t scp_md_mkey_pack(scp_context_h context, sct_mem_h *memhs, void *buffer, size_t *packed_size)
{
    scp_tl_md_t *tl_mds = context->tl_mds;
    ucs_status_t ucs_status;
    size_t key_packed_size = 0;
    void *ptr = buffer + sizeof(uint32_t);

    *(uint8_t*)ptr = context->num_mds;
    ptr = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);

    for (uint8_t md_idx = 0; md_idx < context->num_mds; ++md_idx) {
        key_packed_size = (tl_mds[md_idx].attr.cap.flags & (UCT_MD_FLAG_ALLOC | UCT_MD_FLAG_REG)) ?
                            tl_mds[md_idx].attr.rkey_packed_size : 0;
        *(uint16_t*)ptr = key_packed_size;
        ptr = UCS_PTR_TYPE_OFFSET(ptr, uint16_t);

        if (key_packed_size == 0) {
            continue;
        }

        ucs_status = sct_md_mkey_pack(tl_mds[md_idx].md, memhs[md_idx], ptr);
        if (ucs_status != UCS_OK) {
            ucg_error("Failed to pack uct memory domain key: %s", ucs_status_string(ucs_status));
            return ucg_status_s2g(ucs_status);
        }

        ptr = UCS_PTR_BYTE_OFFSET(ptr, key_packed_size);
    }

    *packed_size = ptr - buffer;
    *(uint32_t *)buffer = *packed_size;

    return UCG_OK;
}

ucg_status_t scp_md_mkey_unpack(scp_context_h context, void *packed,
                                uct_rkey_bundle_t **rkeys, size_t *packed_size)
{
    void *ptr = packed + sizeof(uint32_t);
    uint8_t num_mds = *(uint8_t*)ptr;

    if (num_mds == 0) {
        ucg_error("Invalid mkey buffer.");
        return UCG_ERR_INVALID_ADDR;
    }
    ptr = UCS_PTR_TYPE_OFFSET(ptr, uint8_t);

    uct_rkey_bundle_t *rkey_bundle;
    rkey_bundle = ucg_calloc(num_mds, sizeof(uct_rkey_bundle_t), "rkey_bundle");
    if (rkey_bundle == NULL) {
        return UCG_ERR_NO_MEMORY;
    }

    ucg_status_t status;
    sct_component_h cmpt;
    ucs_status_t ucs_status;
    scp_rsc_index_t cmpt_idx;
    uint16_t key_packed_size;

    for (uint8_t md_idx = 0; md_idx < num_mds; ++md_idx) {
        key_packed_size = *(uint16_t*)ptr;
        ptr = UCS_PTR_TYPE_OFFSET(ptr, uint16_t);

        if (key_packed_size == 0) {
            continue;
        }

        cmpt_idx = context->tl_mds[md_idx].cmpt_index;
        cmpt = context->tl_cmpts[cmpt_idx].cmpt;
        ucs_status = sct_rkey_unpack(cmpt, ptr, &rkey_bundle[md_idx]);
        if (ucs_status != UCS_OK) {
            ucg_error("Failed to unpack uct remote key: %s", ucs_status_string(ucs_status));
            status = ucg_status_s2g(ucs_status);
            goto free_rkey_bundle;
        }
        ptr = UCS_PTR_BYTE_OFFSET(ptr, key_packed_size);
    }

    *rkeys = rkey_bundle;
    *packed_size = (ptr - packed);

    return UCG_OK;

free_rkey_bundle:
    ucg_free(rkey_bundle);
    return status;
}

