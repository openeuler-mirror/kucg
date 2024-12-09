/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_ARCH_NUMA_H
#define UCG_ARCH_NUMA_H

#include "scs.h"


typedef struct scs_numa_cpu_list {
    uint16_t first;
    uint16_t last;
} scs_numa_cpu_list_t;

typedef struct scs_numa_info {
    uint8_t numa_num;
    uint8_t *distance;              /* the distance from current
                                       numa node to other node */
    uint8_t node_id;
    scs_numa_cpu_list_t *cpus_list; /* the cpu list of each numa node */
} scs_numa_info_t;

ucg_status_t scs_numa_get_information(uint16_t core_id, scs_numa_info_t *numa_info);
ucg_status_t scs_numa_get_distance_by_node_id(scs_numa_info_t *numa_info,
                                              uint8_t node_id, uint8_t *distance);
uint8_t scs_numa_get_node_id_by_dev(char *dev_name);


#endif