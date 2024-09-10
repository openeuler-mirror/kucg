/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "time.h"


double ucs_get_cpu_clocks_per_sec()
{
    static double clocks_per_sec = 0.0;
    static int initialized = 0;

    if (!initialized) {
        clocks_per_sec = ucs_arch_get_clocks_per_sec();
        ucg_debug("measured arch clock speed: %.2f Hz", clocks_per_sec);
        initialized = 1;
    }
    return clocks_per_sec;
}
