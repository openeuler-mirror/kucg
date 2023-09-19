/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#ifndef UCG_COMPATIBLE_H_
#define UCG_COMPATIBLE_H_

#include <stdlib.h>

#include "ucg/api/ucg.h"

/**
 * @brief change the value of history config filed to current value of config filed
 *
 * @param [in]  old_value       the value of history config filed
 * @return the value of current config filed
 */
typedef char* (*ucg_config_compatible_convert)(const char *old_value);

typedef struct ucg_config_compatible_field {
    const char *old_name;
    const char *new_name;
    ucg_config_compatible_convert convert;
    uint8_t unset;
} ucg_config_compatible_field_t;

/**
 * @brief Change all old config filed to current config filed.
 */
void ucg_config_compatible();

#endif //UCG_COMPATIBLE_H_
