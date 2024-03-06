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

class test_ucx_allreduce : public testing::Test {
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
ucg_planc_ucx_config_t test_ucx_allreduce::m_config;
ucg_planc_ucx_group_t test_ucx_allreduce::m_group;
ucg_coll_args_t test_ucx_allreduce::m_args;

TEST_F(test_ucx_allreduce, allrecude_get_rd_args)
{
    ucg_status_t status;
    int32_t count;
    int64_t offset;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.myrank = 0;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.size = 0;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_LAST;
    status = ucg_planc_ucx_allreduce_get_rd_args(&m_group.super.super, &m_args, UCG_TOPO_GROUP_TYPE_SOCKET, &offset, &count);
    EXPECT_EQ(status, UCG_OK);

    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.size = 1;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_LAST;
    status = ucg_planc_ucx_allreduce_get_rd_args(&m_group.super.super, &m_args, UCG_TOPO_GROUP_TYPE_SOCKET, &offset, &count);
    EXPECT_EQ(status, UCG_OK);

    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.size = 7;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_LAST;
    status = ucg_planc_ucx_allreduce_get_rd_args(&m_group.super.super, &m_args, UCG_TOPO_GROUP_TYPE_SOCKET, &offset, &count);
    EXPECT_EQ(status, UCG_OK);

    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.myrank = 1;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].super.size = 7;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_LAST;
    status = ucg_planc_ucx_allreduce_get_rd_args(&m_group.super.super, &m_args, UCG_TOPO_GROUP_TYPE_SOCKET, &offset, &count);
    EXPECT_EQ(status, UCG_OK);

    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_DISABLE;
    status = ucg_planc_ucx_allreduce_get_rd_args(&m_group.super.super, &m_args, UCG_TOPO_GROUP_TYPE_SOCKET, &offset, &count);
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_na_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_na_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_na_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

}

TEST_F(test_ucx_allreduce, allreduce_na_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_na_rabenseifner_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    m_args.allreduce.count = 6;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    m_args.allreduce.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op1);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op3 = NULL;
    m_group.super.super.group->topo->ppn = 1;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op3);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op4 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNBALANCED;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op4);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op5 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNBALANCED;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op5);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op6 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op6);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op7 = NULL;
    m_group.super.super.group->topo->ppn = 3;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op7);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op8 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op8);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_na_rabenseifner_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);

    ucg_plan_op_t *op1 = NULL;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op2);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_bntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_nta_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_nta_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_rabenseifner_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_args.allreduce.count = 6;
    status = ucg_planc_ucx_allreduce_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    m_args.allreduce.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_rabenseifner_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_rd_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_rd_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_ring_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_ring_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_ring_prepare(&m_group.super.super, &m_args, &op1);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_args.allreduce.count = 6;
    status = ucg_planc_ucx_allreduce_ring_prepare(&m_group.super.super, &m_args, &op1);
    m_args.allreduce.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_ring_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->op_mp.super = m_group.super.super.group->context->meta_op_mp.super;
    ucx_group->context->op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_ring_prepare(&m_group.super.super, &m_args, &op1);
    ucx_group->context->op_mp.super.data->quota = 0;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_sa_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op3 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op3);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_sa_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rabenseifner_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    m_args.allreduce.count = 6;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    m_args.allreduce.count = 16;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op1);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op3 = NULL;
    m_group.super.super.group->topo->ppn = 1;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op3);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op4 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNBALANCED;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op4);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op5 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNBALANCED;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op5);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op6 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op6);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op7 = NULL;
    m_group.super.super.group->topo->pps = 1;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op7);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op8 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED)
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op8);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rabenseifner_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->op_mp.super = m_group.super.super.group->context->meta_op_mp.super;
    ucx_group->context->op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_bntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->ppn = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->pps = 2;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op3 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op3);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_bntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_kntree_check_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    ucg_planc_ucx_group_t* ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->context->config.reduce_consistency = 1;
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->context->config.reduce_consistency = 0;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->ppn = UCG_TOPO_PPX_UNKNOWN;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNKNOWN;
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->pps = UCG_TOPO_PPX_UNKNOWN;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);

    ucg_plan_op_t *op3 = NULL;
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED);
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_args.allreduce.op->flags = ucg_op_flag_t(UCG_OP_FLAG_IS_PREDEFINED | UCG_OP_FLAG_IS_COMMUTATIVE);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_UNSUPPORTED);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_kntree_init_error)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 0;
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->context->meta_op_mp.super.data->quota = 2000;
    EXPECT_EQ(status, UCG_ERR_NO_MEMORY);
}

TEST_F(test_ucx_allreduce, allreduce_na_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_na_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_na_rabenseifner)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_NODE_LEADER].state = UCG_ALGO_GROUP_STATE_ENABLE;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE].state = UCG_TOPO_GROUP_STATE_ENABLE;
    status = ucg_planc_ucx_allreduce_na_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_NODE_LEADER].state = UCG_ALGO_GROUP_STATE_NOT_INIT;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_NODE].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_meta_op_t *meta_op = (ucg_plan_meta_op_t *)op;
    ucg_plan_op_t *reben_op = meta_op->ops[0];
    raben_op->super.id = 1;
    status = raben_op->trigger(raben_op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *raben_trigger_op = ucg_derived_of(raben_op, ucg_planc_ucx_op_t);
    raben_trigger_op->allreduce.rabenseifner.rank_type = 1;
    status = raben_op->trigger(raben_op);
    EXPECT_EQ(status, UCG_OK);

    raben_trigger_op->allreduce.rabenseifner.rank_type = 2;
    status = raben_op->trigger(raben_op);
    raben_trigger_op->allreduce.rabenseifner.rank_type = 0;
    EXPECT_EQ(status, UCG_OK);

    raben_trigger_op->super.vgroup->size = 4;
    status = raben_op->trigger(raben_op);
    raben_trigger_op->super.vgroup->size = 0;
    EXPECT_EQ(status, UCG_OK);

    status  = op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_na_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_na_rd_and_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_na_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_nta_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op1 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SUBNET].state = UCG_TOPO_GROUP_STATE_ENABLE;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SUBNET].super.size = 1;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op1);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SUBNET].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_op_t *op2 = NULL;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SUBNET_LEADER].state = UCG_TOPO_GROUP_STATE_ENABLE;
    status = ucg_planc_ucx_allreduce_nta_kntree_prepare(&m_group.super.super, &m_args, &op2);
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SUBNET_LEADER].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    op1->super.id = 1;
    status = op1->trigger(op1);
    EXPECT_EQ(status, UCG_OK);

    op2->super.id = 1;
    status = op1->trigger(op2);
    EXPECT_EQ(status, UCG_OK);

    status = op->discard(op);
    EXPECT_EQ(status, UCG_OK);

    status = op1->discard(op1);
    EXPECT_EQ(status, UCG_OK);

    status = op2->discard(op2);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_rabenseifner)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_rd)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_rd_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *rd_trigger_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    rd_trigger_op->allreduce.rd.iter.type = UCG_ALGO_RD_ITER_PROXY;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    rd_trigger_op->allreduce.rd.iter.type = UCG_ALGO_RD_ITER_EXTRA;
    status = op->trigger(op);
    rd_trigger_op->allreduce.rd.iter.type = UCG_ALGO_RD_ITER_BASE;
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_ring)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_ring_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *trigger_op = ucg_derived_of(op, ucg_planc_ucx_op_t);
    trigger_op->super.vgroup->size = 1;
    status = op->trigger(op);
    trigger_op->super.vgroup->size = 16;
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_sa_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_sa_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rabenseifner)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    ucg_planc_ucx_group_t *ucx_group = ucg_derived_of(&m_group.super.super, ucg_planc_ucx_group_t);
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_SOCKET_LEADER].state = UCG_ALGO_GROUP_STATE_ENABLE;
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_NODE_LEADER].state = UCG_ALGO_GROUP_STATE_ENABLE;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_ENABLE;
    status = ucg_planc_ucx_allreduce_sa_rabenseifner_prepare(&m_group.super.super, &m_args, &op);
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_SOCKET_LEADER].state = UCG_ALGO_GROUP_STATE_NOT_INIT;
    ucx_group->groups[UCG_ALGO_GROUP_TYPE_NODE_LEADER].state = UCG_ALGO_GROUP_STATE_NOT_INIT;
    m_group.super.super.group->topo->groups[UCG_TOPO_GROUP_TYPE_SOCKET].state = UCG_TOPO_GROUP_STATE_DISABLE;
    EXPECT_EQ(status, UCG_OK);

    op->super.id = 1;
    status = op->trigger(op);
    EXPECT_EQ(status, UCG_OK);

    ucg_plan_meta_op_t *meta_op = (ucg_plan_meta_op_t *)op;
    ucg_plan_op_t *raben_op = meta_op->ops[3];
    raben_op->super.id = 1;
    status = raben_op->trigger(raben_op);
    EXPECT_EQ(status, UCG_OK);

    ucg_planc_ucx_op_t *raben_trigger_op = ucg_derived_of(raben_op, ucg_planc_ucx_op_t);
    raben_trigger_op->allreduce.rabenseifner.rank_type = 0;
    status = raben_op->trigger(raben_op);
    EXPECT_EQ(status, UCG_OK);

    raben_trigger_op->allreduce.rabenseifner.rank_type = 1;
    status = raben_op->trigger(raben_op);
    raben_trigger_op->allreduce.rabenseifner.rank_type = 2;
    EXPECT_EQ(status, UCG_OK);

    op->discard(op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_bntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_sa_rd_and_bntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

TEST_F(test_ucx_allreduce, allreduce_sa_rd_and_kntree)
{
    ucg_plan_op_t *op = NULL;
    ucg_status_t status;
    status = ucg_planc_ucx_allreduce_sa_rd_and_kntree_prepare(&m_group.super.super, &m_args, &op);
    EXPECT_EQ(status, UCG_OK);
}

