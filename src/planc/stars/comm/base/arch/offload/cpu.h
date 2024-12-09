/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_ARCH_AARCH64_CPU_H_
#define UCG_ARCH_AARCH64_CPU_H_

#include "scs.h"

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif

#define ARCH_CACHE_LINE_SIZE 64

BEGIN_C_DECLS

/** @file cpu.h */

/**
 * Assume the worst - weak memory ordering.
 */

#define ucs_aarch64_dmb(_op)          asm volatile ("dmb " #_op ::: "memory")
#define ucs_aarch64_isb(_op)          asm volatile ("isb " #_op ::: "memory")
#define ucs_aarch64_dsb(_op)          asm volatile ("dsb " #_op ::: "memory")

/* The macro is used to serialize stores across Normal NC (or Device) and WB
 * memory, (see Arm Spec, B2.7.2).  Based on recent changes in Linux kernel:
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds
 * /linux.git/commit/?id=22ec71615d824f4f11d38d0e55a88d8956b7e45f
 *
 * The underlying barrier code was changed to use lighter weight DMB instead
 * of DSB. The barrier used for synchronization of access between write back
 * and device mapped memory (PCIe BAR).
 */
#define ucs_memory_bus_fence()        ucs_aarch64_dmb(oshsy)
#define ucs_memory_bus_store_fence()  ucs_aarch64_dmb(oshst)
#define ucs_memory_bus_load_fence()   ucs_aarch64_dmb(oshld)

/* The macro is used to flush all pending stores from write combining buffer.
 * Some uarch "auto" flush the stores once cache line is full (no need for additional barrier).
 */
#if defined(HAVE_AARCH64_THUNDERX2)
#define ucs_memory_bus_cacheline_wc_flush()
#else
/* The macro is used to flush stores to Normal NC or Device memory */
#define ucs_memory_bus_cacheline_wc_flush()     ucs_aarch64_dmb(oshst)
#endif

#define ucs_memory_cpu_fence()        ucs_aarch64_dmb(ish)
#define ucs_memory_cpu_store_fence()  ucs_aarch64_dmb(ishst)
#define ucs_memory_cpu_load_fence()   ucs_aarch64_dmb(ishld)

/* The macro is used to serialize stores to Normal NC or Device memory
 * (see Arm Spec, B2.7.2)
 */
#define ucs_memory_cpu_wc_fence()     ucs_aarch64_dmb(oshst)


#if HAVE_HW_TIMER
static inline uint64_t ucs_arch_read_hres_clock(void)
{
    uint64_t ticks;
    asm volatile("isb" : : : "memory");
    asm volatile("mrs %0, cntvct_el0" : "=r" (ticks));
    return ticks;
}

static inline double ucs_arch_get_clocks_per_sec()
{
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return (double) freq;
}

#else
static inline uint64_t ucs_arch_generic_read_hres_clock(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return ((((uint64_t)(tv.tv_sec)) * 1000000ULL) + ((uint64_t)(tv.tv_usec)));
}

static inline double ucs_arch_generic_get_clocks_per_sec()
{
    return 1.0E6;
}

static inline void ucs_arch_generic_wait_mem(void *address)
{
    /* NOP */
}

#define ucs_arch_read_hres_clock ucs_arch_generic_read_hres_clock
#define ucs_arch_get_clocks_per_sec ucs_arch_generic_get_clocks_per_sec

#endif

END_C_DECLS

#endif
