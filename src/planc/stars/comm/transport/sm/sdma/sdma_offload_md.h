/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: sdma_md.h
 * Author:
 * Create: 2021
 * Notes:
 */

#ifndef SCT_SDMA_OFFLOAD_MD_H_
#define SCT_SDMA_OFFLOAD_MD_H_

#include "sct_md.h"


extern sct_component_t sct_sdma_ofd_cmpt;

typedef int (*sdma_pin_mem_func)(int fd, void *vma, uint32_t size, uint64_t *cookie);
typedef int (*sdma_unpin_mem_func)(int fd, uint64_t cookie);

typedef struct sct_sdma_ofd_md {
    struct sct_md       super;                  /**< Domain info */
    int                 num_devices;            /**< Number of devices to create */
    sdma_pin_mem_func   pin_umem_cb;            /**< pin mem handler */
    sdma_unpin_mem_func unpin_umem_cb;          /**< unpin mem handler */
    ucg_mpool_t         sdma_params_pool;
    ucg_mpool_t         event_params_pool;
    ucg_mpool_t         notify_params_pool;
} sct_sdma_ofd_md_t;

typedef struct sct_sdma_key {
    void        *address; /**< base addr for the registration */
    uint64_t    cookie;
} sct_sdma_key_t;

UCS_F_ALWAYS_INLINE sdma_trans_parm_t *sct_sdma_ofd_md_get_sdma_elem(sct_md_h sct_md)
{
    sct_sdma_ofd_md_t *md = ucs_derived_of(sct_md, sct_sdma_ofd_md_t);
    return ucg_mpool_get(&md->sdma_params_pool);
}

UCS_F_ALWAYS_INLINE event_trans_parm_t *sct_sdma_ofd_md_get_event_param(sct_md_h sct_md)
{
    sct_sdma_ofd_md_t *md = ucs_derived_of(sct_md, sct_sdma_ofd_md_t);
    return ucg_mpool_get(&md->event_params_pool);
}

UCS_F_ALWAYS_INLINE write_notify_trans_param_t *sct_sdma_ofd_md_get_notify_param(sct_md_h sct_md)
{
    sct_sdma_ofd_md_t *md = ucs_derived_of(sct_md, sct_sdma_ofd_md_t);
    return ucg_mpool_get(&md->event_params_pool);
}

UCS_F_ALWAYS_INLINE void sct_sdma_ofd_md_put_param(void *parm)
{
    ucg_mpool_put(parm);
}

#endif
