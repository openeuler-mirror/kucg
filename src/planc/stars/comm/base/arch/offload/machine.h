/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_ARCH_MACHINE_H
#define UCG_ARCH_MACHINE_H

#include "scs.h"

#define CPU_DIE_NUM_PER_SOCKET 2
#define IO_DIE_NUM_PER_CPU_DIE 2
#define IO_DIE_NUM_PER_SOCKET (CPU_DIE_NUM_PER_SOCKET * IO_DIE_NUM_PER_CPU_DIE)

typedef struct scs_machine_offload {
    uint8_t     chip_num;
    uint8_t     cpu_die_num;
    uint16_t    core_num;
    uint16_t    core_num_per_skt;
    uint16_t    core_num_per_cpu_die;
    struct {
        uint8_t     inner_die_id; /* die id in the chip (0 or 1) */
        uint8_t     chip_id;
        uint8_t     die_id;       /* die id in the machine */
        uint16_t    core_id;
    } affinity; /* used for process core affinity */
} scs_machine_offload_t;

ucg_status_t scs_machine_offload_init(scs_machine_offload_t *machine);

#endif
