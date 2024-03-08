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
#include "planc/ucx/bcast/bcast.h"
#include "ucs/datastruct/mpool.h"
}

using namespace std;

class test_ucx_bcast : public testing::Test {
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
    static void SetUpTestCase()
    {
        uint32_t size = 16;
        ucg_rank_map_t map = {
            .type = UCG_RANK_MAP_TYPE_FULL,
            .size = size,
        };
        static ucg_topo_t topo = {
            .ppn = 2,
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
        static ucs_mpool_data_t mpool_data = {
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
        static ucs_mpool_t mpool = {
            .freelist = NULL,
            .data = &mpool_data,
        };
        static ucg_context_t group_context = {
            .meta_op_mp = {
                .super = mpool,
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
        };
        m_group.super.super.myrank = 0;
        m_group.super.super.size = size;
        m_group.super.super.rank_map = map;
        m_group.super.super.group = &group;
        m_group.context = &context;

        static ucs_mpool_data_t mp_op_mpool_data = {
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
        static ucs_mpool_t mp_op_mpool = {
            .freelist = NULL,
            .data = &mp_op_mpool_data,
        };
        ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
        ucx_group->context->op_mp.super = mp_op_mpool;

        static int buf[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        const int count = 16;
        const int root = 0;
        static ucg_dt_t dt = {
            .type = UCG_DT_TYPE_INT32,
            .flags = UCG_DT_FLAG_IS_PREDEFINED,
        };

        m_args.type = UCG_COLL_TYPE_BCAST;
        m_args.bcast = {
            .buffer = buf,
            .count = count,
            .dt = &dt,
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
ucg_planc_ucx_config_t test_ucx_bcast::m_config;
ucg_planc_ucx_group_t test_ucx_bcast::m_group;
ucg_coll_args_t test_ucx_bcast::m_args;

TEST_F(test_ucx_bcast, bcast_van_de_geijn_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_args.bcast.count = 6;
    status = ucg_planc_ucx_bcast_van_de_geijn_prepare(&m_group.super.super, &m_args, &op);
    m_args.bcast.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

}

TEST_F(test_ucx_bcast, bcast_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_inc_2_ring_m)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_inc_2_ring_m_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_inc_ring_m)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_inc_ring_m_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_ring)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_ring_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_nta_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_nta_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_na_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_bcast_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_bcast, bcast_na_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.myrank = 1;
    m_args.bcast.root = 1;
    status = ucg_planc_ucx_bcast_na_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_args.bcast.root = 0;
    m_group.super.super.myrank = 0;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    op1->super.id = 1;
    status = op1->trigger(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op1->discard(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_na_kntree_and_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_bcast_na_kntree_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_bcast, bcast_na_kntree_and_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_na_kntree_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_na_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_bcast_na_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_bcast, bcast_na_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_na_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *root_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    root_op->bcast._long.kntree_iter.parent = 1;
    status = op->trigger(op);
    root_op->bcast._long.kntree_iter.parent = UCG_INVALID_RANK;
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_long_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_args.bcast.count = 6;
    status = ucg_planc_ucx_bcast_long_prepare(&m_group.super.super, &m_args, &op);
    m_args.bcast.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_bcast, bcast_long)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_long_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *root_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    root_op->bcast._long.kntree_iter.parent = 1;
    status = op->trigger(op);
    root_op->bcast._long.kntree_iter.parent = UCG_INVALID_RANK;
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_long_m_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_args.bcast.count = 6;
    status = ucg_planc_ucx_bcast_long_m_prepare(&m_group.super.super, &m_args, &op);
    m_args.bcast.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_bcast, bcast_long_m)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_long_m_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *root_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    root_op->bcast._long.kntree_iter.parent = 1;
    status = op->trigger(op);
    root_op->bcast._long.kntree_iter.parent = UCG_INVALID_RANK;
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_bcast, bcast_van_de_geijn)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_bcast_van_de_geijn_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *batch_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    batch_op->bcast.van_de_geijn.send_type = 1;
    status = op->trigger(op);
    batch_op->bcast.van_de_geijn.send_type = 0;
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *root_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    root_op->bcast.kntree_iter.parent = 1;
    status = op->trigger(op);
    root_op->bcast.kntree_iter.parent = UCG_INVALID_RANK;
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_bcast_config_t *config = (ucg_planc_ucx_bcast_config_t *)(m_config.config_bundle[UCG_COLL_TYPE_BCAST][UCX_BUILTIN]->data);
    config->min_bsend = UCG_MEMUNITS_AUTO;
    config->max_bsend = UCG_MEMUNITS_AUTO;
    m_group.super.super.group->topo->detail.nnode = 32;
    ucg_plan_op_t *op1 = NULL;
    status = ucg_planc_ucx_bcast_van_de_geijn_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->detail.nnode = 0;
    EXPECT_EQ(status, UCG_OK);

    op1->super.id = 1;
    status = op1->trigger(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op1->discard(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}