/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#include <gtest/gtest.h>
#include "stub.h"

extern "C" {
#include "planc/ucx/planc_ucx_context.h"
#include "planc/ucx/planc_ucx_group.h"
#include "planc/ucx/planc_ucx_plan.h"
#include "core/ucg_def.h"
#include "core/ucg_plan.h"
#include "core/ucg_group.h"
#include "util/ucg_malloc.h"
#include "planc/ucx/allreduce/allreduce.h"
#include "planc/ucx/allreduce/allreduce_meta.h"
#include "ucs/datastruct/mpool.h"
}

using namespace std;

class test_ucx_gatherv : public testing::Test {
private :
    static void fill_config()
    {
        static ucg_planc_ucx_config_bundle_t config_bundle[UCG_COLL_TYPE_LAST][UCX_MODULE_LAST];
        for (int i = 0; i < UCG_COLL_TYPE_LAST; ++i) {
            for (int j = 0; j < UCX_MODULE_LAST; ++j) {
                config_bundle[i][j].data[0] = '1';
                m_config.config_bundle[i][j] = &config_bundle[i][j];
            }
        }
        return;
    }
public:
    static ucg_status_t user_func(void *op, const void *source, void *target,
                                  int32_t count, void *dt)
    {
        return UCG_OK;
    }

    static void SetUpTestCase()
    {
        uint32_t size = 16;
        ucg_tank_map_t  map = {
            .type = UCG_RANK_MAP_TYPE_FULL,
            .size = size,
        };
        static ucg_topo_t topo = {
            .ppn = 2,
            .pps = 2,
        };
        static ucs_mpool_ops_t ops = {
            fake_ucs_mpool_chunk_malloc,
            fake_ucs_mpool_chunk_free,
            NULL,
            NULL,
        };
        static const size_t header_size = 30;
        static const size_t data_size = 152;
        static const size_t align = 128;
        static ucs_mpool_data_t meta_mpool_data = {
            .elem_size = sizeof(ucs_mpool_elem_t) + (header_size + data_size),
            .alignment = align,
            .align_offset = sizeof(ucs_mpool_elem_t) + header_size,
            .elems_per_chunk = (unsigned)1,
            .quota = (unsigned)2000,
            .tail = NULL,
            .chunks = NULL,
            .ops = &ops,
            .name = strdup("test"),
        };
        static ucs_mpool_t meta_mpool = {
            .freelist = NULL,
            .deta = &meta_mpool_data,
        };
        static ucg_context_t group_context = {
            .meta_op_mp = {
                .super = meta_mpool,
                .lock = {
                    .type = UCG_LOCK_TYPE_NONE,
                },
            },
        };
        static ucg_group_t group = {
            .context = &group_context,
            .topo = &topo,
            .size = size,
        };
        fill_config();
        static ucg_planc_ucx_context_t context = {
            .config = m_config,
            .eps = NULL,
        };

        m_group.super.super.myrank = 0;
        m_group.super.super.size = size;
        m_group.super.super.rank_map = map;
        m_group.super.super.group = &group;
        m_group.context = &context;

        static ucs_mpool_data_t op_mpool_data = {
            .elem_size = sizeof(ucs_mpool_elem_t) + (header_size + data_size),
            .alignment = align,
            .align_offset = sizeof(ucs_mpool_elem_t) + header_size,
            .elems_per_chunk = (unsigned)1,
            .quota = (unsigned)2000,
            .tail = NULL,
            .chunks = NULL,
            .ops = &ops,
            .name = strdup("test"),
        };
        static ucs_mpool_t op_mpool = {
            .freelist = NULL,
            .deta = &op_mpool_data,
        };
        ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
        ucx_group->context->op_mp.super = op_mpool;

        static int buf[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        static int recvcounts[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        static int displs[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        const int count = 16;
        static ucg_dt_t sendtype = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };
        static ucg_dt_t recvtype = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };
        m_args.type = UCG_COLL_TYPE_GATHERV;
        m_args.gatherv = {
            .sendbuf = buf,
            .sendcount = count,
            .sendtype = &sendtype,
            .recvbuf = buf,
            .recvcounts = recvcounts,
            .displs = displs,
            .recvtype = &recvtype,
            .root = root,
        };
        return;
    }

    static void TearDownTestCase()
    {
        return;
    }
    static ucg_planc_ucx_config_t m_config;
    static ucg_planc_ucx_group_t m_group;
    static ucg_coll_args_t m_args;
};
ucg_planc_ucx_config_t test_ucx_gatherv::m_config;
ucg_planc_ucx_group_t test_ucx_gatherv::m_group;
ucg_coll_args_t test_ucx_gatherv::m_args;

TEST_F(test_ucx_gatherv, gatherv_linear)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_gatherv_linear_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_ucx_op_t *trigger_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    trigger_op->super.vgroup->myrank = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}