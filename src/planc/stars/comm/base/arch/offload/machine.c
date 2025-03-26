/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "machine.h"

#include "sys/sys.h"

#define PATH_SYS_CPU "/sys/devices/system/cpu/cpu%d/topology/core_siblings"

#define PATH_CTX_SIZE 100
#define MAX_CHIP_NUM  8

static ucg_status_t scs_machine_get_core_num_per_socket(scs_machine_offload_t *machine)
{
    machine->core_num = sysconf(_SC_NPROCESSORS_CONF); /* _SC_NPROCESSORS_ONLN */

    char prev_ctx[MAX_CHIP_NUM][PATH_CTX_SIZE] = {0};
    char curr_ctx[PATH_CTX_SIZE] = {0};
    ssize_t size;

    machine->chip_num = 0;
    for (int idx = 0; idx < machine->core_num; idx++) {
        size = ucs_read_file(curr_ctx, sizeof(curr_ctx), 1, PATH_SYS_CPU, idx);
        curr_ctx[size] = '\0';

        /* If the same ctx already exists. */
        int prev_chip_flag = 0;
        for (int prev_idx = 0; prev_idx < machine->chip_num; prev_idx++) {
            if (strcmp(curr_ctx, prev_ctx[prev_idx]) == 0) {
                prev_chip_flag = 1;
                break;
            }
        }
        if (!prev_chip_flag) {
            if (machine->chip_num >= MAX_CHIP_NUM) {
                ucg_fatal("Chips number exceeds the upper limit(%d)!", MAX_CHIP_NUM);
            }
            strcpy(prev_ctx[machine->chip_num++], curr_ctx);
        }
    }

    if (ucg_unlikely(machine->chip_num <= 0)) {
        ucg_error("Invalid socket number %d", machine->chip_num);
        return UCG_ERR_INVALID_PARAM;
    }

    machine->core_num_per_skt = machine->core_num / machine->chip_num;
    return UCG_OK;
}

static void scs_machine_offload_clean(scs_machine_offload_t *machine)
{
    machine->chip_num = 0;
    machine->cpu_die_num = 0;
    machine->core_num = 0;
    machine->core_num_per_skt = 0;
    machine->core_num_per_cpu_die = 0;

    machine->affinity.inner_die_id = UINT8_MAX;
    machine->affinity.chip_id = UINT8_MAX;
    machine->affinity.die_id = UINT8_MAX;
    machine->affinity.core_id = UINT16_MAX;
}

ucg_status_t scs_machine_offload_init(scs_machine_offload_t *machine)
{
    ucg_status_t status;
    scs_machine_offload_clean(machine);

    status = scs_machine_get_core_num_per_socket(machine);
    UCG_CHECK_GOTO(status, out);

    machine->core_num_per_cpu_die = machine->core_num_per_skt / CPU_DIE_NUM_PER_SOCKET;
    machine->cpu_die_num = machine->core_num / machine->core_num_per_cpu_die;

    machine->affinity.core_id = ucs_get_first_cpu();
    machine->affinity.chip_id = machine->affinity.core_id / machine->core_num_per_skt;
    machine->affinity.die_id = machine->affinity.core_id / machine->core_num_per_cpu_die;
    machine->affinity.inner_die_id = machine->affinity.die_id % CPU_DIE_NUM_PER_SOCKET;
    ucg_debug("machine offload information { chip_num %d, cpu_die_num %d, core_num %d, "
              "core_num_per_skt %d, core_num_per_cpu_die %d }, thread location { chip_id %d, "
              "die_id %d die_id_in_chip: %d core_id %d }",
              (int)machine->chip_num, (int)machine->cpu_die_num,
              (int)machine->core_num, (int)machine->core_num_per_skt, (int)machine->core_num_per_cpu_die,
              (int)machine->affinity.chip_id, (int)machine->affinity.die_id,
              (int)machine->affinity.inner_die_id, (int)machine->affinity.core_id);
out:
    return status;
}
