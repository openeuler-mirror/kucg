/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include "ucg_compatible.h"

#include "util/ucg_helper.h"
#include "util/ucg_log.h"
#include "util/ucg_malloc.h"

#define HOLD_AFTER_CONVERT 0
#define UNSET_AFTER_CONVERT 1

static char* ucg_config_compatible_convert_empty(const char *old_value)
{
    return ucg_strdup(old_value, "replace value");
}

static char* ucg_convert_algo_id(const char *old_value, int max)
{
    int algo_id = 0;
    sscanf(old_value, "%i", &algo_id);
    if (algo_id < 1 || algo_id > max) {
        algo_id = 1;
    }

    char attr_key[8] = {0};
    snprintf(attr_key, 8, "I:%d", algo_id);

    return ucg_strdup(attr_key, "replace algo id.");
}

static char* ucg_config_compatible_convert_bcast_algo(const char *old_value)
{
    return ucg_convert_algo_id(old_value, 5);
}

static char* ucg_config_compatible_convert_allreduce_algo(const char *old_value)
{
    return ucg_convert_algo_id(old_value, 14);
}

static char* ucg_config_compatible_convert_barrier_algo(const char *old_value)
{
    return ucg_convert_algo_id(old_value, 10);
}

static ucg_config_compatible_field_t ucg_config_compatible_table[] = {
    { "UCX_BUILTIN_BCAST_ALGORITHM", "UCG_PLANC_UCX_BCAST_ATTR",
      ucg_config_compatible_convert_bcast_algo, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_ALLREDUCE_ALGORITHM", "UCG_PLANC_UCX_ALLREDUCE_ATTR",
      ucg_config_compatible_convert_allreduce_algo, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_BARRIER_ALGORITHM", "UCG_PLANC_UCX_BARRIER_ATTR",
      ucg_config_compatible_convert_barrier_algo, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_INTER_DEGREE_FANOUT", "UCG_PLANC_UCX_ALLREDUCE_FANOUT_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTER_DEGREE_FANOUT", "UCG_PLANC_UCX_BARRIER_FANOUT_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTER_DEGREE_FANOUT", "UCG_PLANC_UCX_BCAST_NA_KNTREE_INTER_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTER_FANOUT", "UCG_PLANC_UCX_ALLREDUCE_FANOUT_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTER_FANOUT", "UCG_PLANC_UCX_BARRIER_FANOUT_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTER_FANOUT", "UCG_PLANC_UCX_BCAST_NA_KNTREE_INTER_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_INTER_DEGREE_FANIN", "UCG_PLANC_UCX_ALLREDUCE_FANIN_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTER_DEGREE_FANIN", "UCG_PLANC_UCX_BARRIER_FANIN_INTER_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTER_FANIN", "UCG_PLANC_UCX_ALLREDUCE_FANIN_INTER_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTER_FANIN", "UCG_PLANC_UCX_BARRIER_FANIN_INTER_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_INTRA_DEGREE_FANOUT", "UCG_PLANC_UCX_ALLREDUCE_FANOUT_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTRA_DEGREE_FANOUT", "UCG_PLANC_UCX_BARRIER_FANOUT_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTRA_DEGREE_FANOUT", "UCG_PLANC_UCX_BCAST_NA_KNTREE_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTRA_FANOUT", "UCG_PLANC_UCX_ALLREDUCE_FANOUT_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTRA_FANOUT", "UCG_PLANC_UCX_BARRIER_FANOUT_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTRA_FANOUT", "UCG_PLANC_UCX_BCAST_NA_KNTREE_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_INTRA_DEGREE_FANIN", "UCG_PLANC_UCX_ALLREDUCE_FANIN_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_INTRA_DEGREE_FANIN", "UCG_PLANC_UCX_BARRIER_FANIN_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTRA_FANIN", "UCG_PLANC_UCX_ALLREDUCE_FANIN_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, HOLD_AFTER_CONVERT },

    { "UCX_BUILTIN_DEGREE_INTRA_FANIN", "UCG_PLANC_UCX_BARRIER_FANIN_INTRA_DEGREE",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_MAX_MSG_LIST_SIZE", "UCG_PLANC_UCX_NPOLLS",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_BCOPY_TO_ZCOPY_OPT", NULL,
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_BCOPY_MAX_TX_SIZE", NULL,
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_LARGE_DATATYPE_THRESHOLD", NULL,
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT },

    { "UCX_BUILTIN_REDUCE_CONSISTENCY", "UCG_PLANC_UCX_REDUCE_CONSISTENCY",
        ucg_config_compatible_convert_empty, UNSET_AFTER_CONVERT }
};

static void ucg_config_compatible_convert_fun(ucg_config_compatible_field_t *compatible_config)
{
    if (ucg_is_null(compatible_config) || ucg_is_null(compatible_config->old_name)
    || ucg_is_null(compatible_config->convert)) {
        ucg_warn("Invalid params as null.");
        return;
    }

    // exit as config that belong to old version don't set
    char *old_value = getenv(compatible_config->old_name);
    if (ucg_is_null(old_value)) {
        return;
    } else if (strlen(old_value) <= 0) {
        ucg_warn("Abandoned config<%s> as unset value.", compatible_config->old_name);
        goto unset_env;
    }

    if (ucg_is_null(compatible_config->new_name)) {
        ucg_info("Can't compatible old config<%s>.", compatible_config->old_name);
        goto unset_env;
    }

    char *new_value = getenv(compatible_config->new_name);
    if (!ucg_is_null(new_value) && strlen(new_value) > 0) {
        ucg_info("Abandoned config<%s> as already exist config<%s=%s>.",
                 compatible_config->old_name, compatible_config->new_name, new_value);
        goto unset_env;
    }

    char *replace_value = compatible_config->convert(old_value);
    if (ucg_is_null(replace_value)) {
        ucg_warn("Abandoned config<%s> as failure convert to <%s>.",
                 compatible_config->old_name, compatible_config->new_name);
        return;
    }

    setenv(compatible_config->new_name, replace_value, 1);
    ucg_info("Replace abandoned config<%s=%s> with config<%s=%s>.", compatible_config->old_name, old_value,
             compatible_config->new_name, replace_value);

    ucg_free(replace_value);

unset_env:
    if (compatible_config->unset == UNSET_AFTER_CONVERT &&
    !ucg_is_null(compatible_config->old_name)) {
        unsetenv(compatible_config->old_name);
    }

    return;
}

void ucg_config_compatible()
{
    uint8_t field_num = sizeof(ucg_config_compatible_table) / sizeof(ucg_config_compatible_table[0]);

    for (uint8_t index = 0; index < field_num; ++index) {
        ucg_config_compatible_convert_fun(&ucg_config_compatible_table[index]);
    }

    return;
}
