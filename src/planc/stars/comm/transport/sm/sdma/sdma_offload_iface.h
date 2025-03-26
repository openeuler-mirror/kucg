/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: sdma_offload_iface.h
 * Author:
 * Create: 2021
 * Notes:
 */
#ifndef SCT_SDMA_OFFLOAD_IFACE_H
#define SCT_SDMA_OFFLOAD_IFACE_H

#include "sct_iface.h"

#include "sdma_offload_md.h"

#define SDMA_OFD_TASK_COUNT             1216

typedef struct sct_sdma_ofd_iface_config {
    sct_iface_config_t    super;
    double                bw;        /* BW for SDMA */
} sct_sdma_ofd_iface_config_t;

typedef struct sct_sdma_ofd_iface_addr {
    unsigned pasid;
    int      dev_id;
} sct_sdma_ofd_iface_addr_t;

typedef struct sct_sdma_ofd_iface {
    sct_base_iface_t      super;
    sct_sdma_ofd_md_t     *sdma_md;
    unsigned              sdma_pasid;
    ucs_spinlock_t        lock;
    int                   wait_flag;
    ucs_queue_head_t      req_queue;
    struct {
        double            bw;        /* BW for SDMA */
    } config;
} sct_sdma_ofd_iface_t;

#endif
