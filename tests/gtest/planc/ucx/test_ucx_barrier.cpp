/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#include <gtest/gtest.h>
#include "stub.h"

extern "C" {
#include "planc/ucx/planc_ucx_context.h"
#include "planc/ucx/planc_ucx_group.h"
#include "planc/ucx/planc_ucx_plan.h"
#include "planc/ucg_planc.h"
#include "core/ucg_def.h"
#include "core/ucg_topo.h"
#include "core/ucg_plan.h"
#include "core/ucg_group.h"
#include "util/ucg_malloc.h"
#include "planc/ucx/barrier/barrier.h"
#include "ucs/datastruct/mpool.h"
}

using namespace std;

class test_ucx_barrier : public testing::Test {
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
        static ucg_topo_t topo = {
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
        const int count = 16;
        static ucg_dt_t dt = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };
        static ucg_op_t op = {
            .type = UCG_OP_TYPE_SUM,
            .flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE),
            .func = user_func,
        };
        m_args.type = UCG_COLL_TYPE_ALLREDUCE;
        m_args.allreduce = {
            .sendbuf = buf,
            .recvbuf = buf,
            .count = count,
            .dt = &dt,
            .op = &op,
            .gop = {
                .super = op,
            },
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
ucg_planc_ucx_config_t test_ucx_barrier::m_config;
ucg_planc_ucx_group_t test_ucx_barrier::m_group;
ucg_coll_args_t test_ucx_barrier::m_args;

TEST_F(test_ucx_barrier, barrier_na_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_na_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_bntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_rd_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->op_mp.super = m_group.super.super.group->context->meta_op_mp.super;
    ucx_group->context->op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_rd_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_sa_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->pps = -2;
    status = ucg_planc_ucx_barrier_sa_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_sa_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->pps = -2;
    status = ucg_planc_ucx_barrier_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_bntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = -2;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->pps = -2;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE_LEADER].state = UCG_TOPO_GROUP_STATE_LAST;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE_LEADER].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_barrier, barrier_na_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_sa_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_na_rd_and_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE].state = UCG_TOPO_GROUP_STATE_ENABLE;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE].super.size = 1;
    status = ucg_planc_ucx_barrier_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET_LEADER].state = UCG_TOPO_GROUP_STATE_LAST;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET_LEADER].super.size = 1;
    status = ucg_planc_ucx_barrier_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET_LEADER].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    op1->super.id = 1;
    status = op1->trigger(op1);
    EXPECT_EQ(status, UCG_OK);

    op2->super.id = 1;
    status = op2->trigger(op2);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_meta_op_t *meta_op = (ucg_plan_meta_op_t *)op1;
    ucg_plan_op_t *trigger_op = meta_op->ops[0];
    trigger_op->super.id = 1;
    status = trigger_op->trigger(trigger_op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *send_op = ucg_derived_of(trigger_op, ucg_planc_ucx_op_t);
    send_op->barrier.fanin_iter.parent = 0;
    status = trigger_op->trigger(trigger_op);
    send_op->barrier.fanin_iter.parent = UCG_INVALID_RANK;
    EXPECT_EQ(status, UCG_OK);

    status = trigger_op->discard(trigger_op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);

    status = op1->discard(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op2->discard(op2);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_rd)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_rd_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *rd_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    ucg_algo_rd_iter_t *iter = &rd_op->barrier.rd_iter;
    iter->type = UCG_ALGO_RD_ITER_PROXY;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    iter->type = UCG_ALGO_RD_ITER_EXTRA;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_barrier, barrier_sa_rd_and_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE_LEADER].state = UCG_TOPO_GROUP_STATE_ENABLE;
    status = ucg_planc_ucx_barrier_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE_LEADER].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    op1->super.id = 1;
    status = op1->trigger(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op1);
    EXPECT_EQ(status, UCG_OK);
}