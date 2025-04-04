/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */
#ifndef TEST_STUB_H_
#define TEST_STUB_H_

#include <ucg/api/ucg.h>

#include <map>
#include <string>
#include <vector>

extern "C" {
#include "planc/ucg_planc.h"
#include "planc/ucg_planc.h"
#include "planc/ucx/planc_ucx_p2p.h"
#include "planc/ucx/planc_ucx_group.h"
#include "core/ucg_dt.h"
#include "ucs/type/status.h"
#include "ucs/datastruct/mpool.h"
}

namespace test {

class stub {
public:
    enum routine_type_t {
        // memory, only availabe when macro UCG_ENABLE_DEBUG is on.
        MALLOC,
        CALLOC,
        REALLOC,
        POSIX_MEMALIGN,
        STRDUP,
        // planc api
        PLANC_MEM_QUERY,
        PLANC_CONFIG_READ,
        PLANC_CONFIG_MODIFY,
        PLANC_CONTEXT_INIT,
        PLANC_CONTEXT_QUERY,
        PLANC_GROUP_CREATE,
        PLANC_GET_PLANS,
        // plan op
        PLAN_PREPARE,
        PLAN_OP_TRIGGER,
        PLAN_OP_PROGRESS,
        PLAN_OP_DISCARD,
        // User Callback
        GET_LOCATION_CB,
        GET_PROC_INFO_CB,
        ALLGATHER_CB,
        ROUTINE_TYPE_LAST,
    };

    enum routine_result_t {
        SUCCESS,
        FAILURE,
    };

    struct routine_info_t {
        int count;
        std::vector<routine_result_t> results;
    };
public:
    static void init(bool load_planc = false);
    static void cleanup();

    /** Mock results.size() times, i-th routine call return results[i]. */
    static void mock(routine_type_t type, std::vector<routine_result_t> results,
                     const char *match = "*");
    /** Determines the return value of the stub function. */
    static bool call(routine_type_t type, const char *name = "*");

private:
    static bool m_load_planc;
    static std::map<std::string, routine_info_t> routines[ROUTINE_TYPE_LAST];
};

} // namespace test

extern "C" {
/*******************************************************************************
 *                          Parameters to create stub context and group
 ******************************************************************************/
extern ucg_params_t test_stub_context_params;
extern ucg_group_params_t test_stub_group_params;

extern void *test_stub_acl_buffer;

ucg_status_t test_stub_allgather(const void *sendbuf, void *recvbuf, int32_t count, void *group);
ucs_status_t fake_ucs_mpool_chunk_malloc(ucs_mpool_t *mp, size_t *size_p, void **chunk_p);
void fake_ucs_mpool_chunk_free(ucs_mpool_t *mp, void *chunk);

void ucg_mpool_put(void *obj);

ucg_status_t ucg_planc_ucx_p2p_isend(const void *buffer, int32_t count,
                                     ucg_dt_t *dt, ucg_rank_t vrank,
                                     int tag, ucg_vgroup_t *vgroup,
                                     ucg_planc_ucx_p2p_params_t *params);

ucg_status_t ucg_planc_ucx_p2p_irecv(void *buffer, int32_t count,
                                     ucg_dt_t *dt, ucg_rank_t vrank,
                                     int tag, ucg_vgroup_t *vgroup,
                                     ucg_planc_ucx_p2p_params_t *params);

ucg_status_t ucg_planc_ucx_p2p_testall(ucg_planc_ucx_group_t *ucx_group,
                                       ucg_planc_ucx_p2p_state_t *state);
}
#endif