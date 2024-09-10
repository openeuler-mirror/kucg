/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCS_DEF_H
#define SCS_DEF_H

/* compile define header copy from ucx compile directory */
#include "scs_config.h"

/* system header */
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <inttypes.h>
#include <math.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netdb.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/ip.h>
#define ucs_assert(...)

/* ucm header */
#include <ucm/api/ucm.h>

/* ucs header */

#include <ucs/algorithm/crc.h>
#include <ucs/arch/atomic.h>
#include <ucs/arch/bitops.h>
#include <ucs/async/async_fwd.h>
#include <ucs/config/global_opts.h>
#include <ucs/config/parser.h>
#include <ucs/config/types.h>
#include <ucs/datastruct/callbackq.h>
#include <ucs/datastruct/hlist.h>
#include <ucs/datastruct/linear_func.h>
#include <ucs/datastruct/list.h>
#include <ucs/datastruct/khash.h>
#include <ucs/datastruct/mpool.h>
#include <ucs/datastruct/string_buffer.h>
#include <ucs/memory/memory_type.h>
#include <ucs/memory/rcache.h>
#include <ucs/stats/stats_fwd.h>
#include <ucs/sys/event_set.h>
#include <ucs/sys/math.h>
#include <ucs/sys/preprocessor.h>
#include <ucs/sys/sock.h>
#include <ucs/sys/string.h>
#include <ucs/sys/stubs.h>
#include <ucs/time/time_def.h>
#include <ucs/type/class.h>
#include <ucs/type/cpu_set.h>
#include <ucs/type/init_once.h>
#include <ucs/type/spinlock.h>
#include <ucs/type/status.h>
#include <ucs/type/thread_mode.h>

/* ucg header */
#include <core/ucg_dt.h>
#include <core/ucg_global.h>
#include <core/ucg_group.h>
#include <core/ucg_plan.h>
#include <core/ucg_rank_map.h>
#include <core/ucg_request.h>
#include <core/ucg_vgroup.h>

#include <ucg/api/ucg.h>
#include <util/algo/ucg_kntree.h>
#include <util/algo/ucg_ring.h>
#include <util/ucg_cpu.h>
#include <util/ucg_helper.h>
#include <util/ucg_log.h>
#include <util/ucg_malloc.h>
#include <util/ucg_math.h>
#include <util/ucg_mpool.h>


#define UCG_CHECK_GOTO_ERR(_stmt, _label, _msg) \
    do {                                        \
        if (_stmt != UCG_OK) {                  \
            ucg_error("%s", _msg);              \
            goto _label;                        \
        }                                       \
    } while (0)

#define CHKERR_ACTION(_cond, _msg, _action) \
    do {                                    \
        if (_cond) {                        \
            ucg_error("%s", _msg);          \
            _action;                        \
        }                                   \
    } while (0)

#define CHKERR_JUMP(_cond, _msg, _label) CHKERR_ACTION(_cond, _msg, goto _label)

#define UCG_ASSERT_CODE_GOTO(_code, _label)            \
    do {                                               \
        if (_code != UCG_OK) {                         \
            ucg_error("%s", ucg_status_string(_code)); \
            goto _label;                               \
        }                                              \
    } while (0)

#define UCG_ASSERT_CODE_RET(_code)                     \
    do {                                               \
        if (_code != UCG_OK) {                         \
            ucg_error("%s", ucg_status_string(_code)); \
            return _code;                              \
        }                                              \
    } while (0)

#define UCG_ASSERT_GOTO(_stmt, _label, _code)           \
    do {                                                \
        if (ucg_unlikely(!(_stmt))) {                   \
            status = _code;                             \
            ucg_error("%s", ucg_status_string(status)); \
            goto _label;                                \
        }                                               \
    } while (0)

#define UCG_ASSERT_RET(_stmt, _code)                   \
    do {                                               \
        if (ucg_unlikely(!(_stmt))) {                  \
            ucg_error("%s", ucg_status_string(_code)); \
            return _code;                              \
        }                                              \
    } while (0)

#define UCG_MATCH_GOTO(_stmt, _label) \
    do {                              \
        if (_stmt) {                  \
            goto _label;              \
        }                             \
    } while (0)

#define UCG_MATCH_RET(_stmt, _code) \
    do {                            \
        if (_stmt) {                \
            return _code;           \
        }                           \
    } while (0)

/**
 * alloca which makes sure the size is small enough.
 */
#define ucg_alloca(_size)                           \
    ({                                              \
        ucg_assert((_size) <= UCS_ALLOCA_MAX_SIZE); \
        alloca(_size);                              \
    })

#endif
