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

class test_ucx_scatterv : public testing::Test {
private:
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
        ucg_rank_map_t map = {
            .type = UCG_RANK_MAP_TYPE_FULL,
            .size = size,
        };
        static ucg_topo_detail_t detail = {
            .nrank_continuous = 1,
        };
        static ucg_topo_t topo = {
            .detail = detail,
            .ppn = 2,
            .pps = 2,
        };
        static ucg_mpool_t meta_mpool;
        (void)ucg_mpool_init(&meta_mpool, 0, sizeof(ucg_plan_meta_op_t),
                             0, 64, UCG_ELEMS_PER_CHUNK,
                             UINT_MAX, NULL, "meta op mpool");
        static ucg_context_t group_context = {
            .meta_op_mp = meta_mpool,
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

        static ucg_mpool_t op_mpool;
        (void)ucg_mpool_init(&op_mpool, 0, sizeof(ucg_plan_meta_op_t),
                             0, 64, UCG_ELEMS_PER_CHUNK,
                             UINT_MAX, NULL, "meta op mpool");
        ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
        ucx_group->context->op_mp = op_mpool;

        static int buf[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        static int sendcounts[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        static int displs[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        const int count = 16;
        static ucg_dt_t send_dt = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };
        static ucg_dt_t recv_dt = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };
        m_args.type = UCG_COLL_TYPE_SCATTERV;
        m_args.scatterv = {
            .sendbuf = buf,
            .sendcounts = sendcounts,
            .displs = displs,
            .sendtype = &send_dt,
            .recvbuf = buf,
            .recvcount = count,
            .recvtype = &recv_dt,
            .root = 1,
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
ucg_planc_ucx_config_t test_ucx_scatterv::m_config;
ucg_planc_ucx_group_t test_ucx_scatterv::m_group;
ucg_coll_args_t test_ucx_scatterv::m_args;

TEST_F(test_ucx_scatterv, scatterv_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_scatterv_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);
    ucg_planc_ucx_op_t *trigger_op = (ucg_planc_ucx_op_t *)op;
    trigger_op->scatterv.kntree.kntree_iter.parent = UCG_INVALID_RANK;
    m_group.super.super.myrank = -1;
    trigger_op->super.super.args.scatterv.root = -1;
    trigger_op->scatterv.kntree.kntree_iter.degree = 2;
    trigger_op->scatterv.kntree.kntree_iter.subsize = 4;
    trigger_op->scatterv.kntree.kntree_iter.leftmost = 4;
    trigger_op->scatterv.kntree.kntree_iter.max_subsize = 4;
    trigger_op->scatterv.kntree.kntree_iter.root = -1;
    trigger_op->scatterv.kntree.kntree_iter.myrank = -1;
    op->trigger(op);
    trigger_op->scatterv.kntree.kntree_iter.myrank = 15;
    trigger_op->scatterv.kntree.kntree_iter.root = 1;
    trigger_op->scatterv.kntree.kntree_iter.max_subsize = 1;
    trigger_op->scatterv.kntree.kntree_iter.leftmost = 1;
    trigger_op->scatterv.kntree.kntree_iter.subsize = 1;
    trigger_op->scatterv.kntree.kntree_iter.degree = 16;
    trigger_op->super.super.args.scatterv.root = 1;
    m_group.super.super.myrank = 0;
    trigger_op->scatterv.kntree.kntree_iter.parent = 1;
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_scatterv, scatterv_na_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    // test wrong branch
    ucg_plan_op_t *wrong_op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &wrong_op1);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *wrong_op2 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNBALANCED;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &wrong_op2);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *wrong_op3 = NULL;
    m_group.super.super.group->topo->ppn = 1;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &wrong_op3);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *wrong_op4 = NULL;
    m_group.super.super.group->topo->detail.nrank_continuous = 0;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &wrong_op4);
    m_group.super.super.group->topo->detail.nrank_continuous = 1;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *wrong_op5 = NULL;
    m_group.super.super.group->topo->ppn = m_group.super.super.group->size;
    status = ucg_planc_ucx_scatterv_na_kntree_prepare(&m_group.super.super, &m_args, &wrong_op5);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}