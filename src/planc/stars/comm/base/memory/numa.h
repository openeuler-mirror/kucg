/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef _UCG_NUMA_H_
#define _UCG_NUMA_H_

#include "scs.h"

#if HAVE_NUMA
#include <numaif.h>
#include <numa.h>

#if HAVE_STRUCT_BITMASK
#  define numa_nodemask_p(_nm)            ((_nm)->maskp)
#  define numa_nodemask_size(_nm)         ((_nm)->size)
#  define numa_get_thread_node_mask(_nmp) \
        do { \
            numa_free_nodemask(*(_nmp)); \
            *(_nmp) = numa_get_run_node_mask(); \
        } while (0)
#else
#error "Port me"
#endif /* HAVE_STRUCT_BITMASK */
#endif /* HAVE_NUMA */

#define UCS_NUMA_MIN_DISTANCE    10

typedef enum {
    UCS_NUMA_POLICY_DEFAULT,
    UCS_NUMA_POLICY_BIND,
    UCS_NUMA_POLICY_PREFERRED,
    UCS_NUMA_POLICY_LAST
} ucs_numa_policy_t;

extern const char *ucs_numa_policy_names[];

int ucs_numa_node_of_cpu(int cpu);

#endif
