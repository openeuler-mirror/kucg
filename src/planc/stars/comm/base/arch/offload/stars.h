/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_ARCH_STARS_H
#define UCG_ARCH_STARS_H

#include <dlfcn.h>

#include <stars_interface.h>
#include "scs.h"
#include "offload/machine.h"
#include "sct.h"


#define STARS_DRIVER_SO "libruntime.so"

typedef struct scs_stars_info {
    stars_info_t driver;
} scs_stars_info_t;

typedef RTS_API int (*STARS_DEV_INIT)(int dev_id);
typedef RTS_API int (*STARS_DEV_DEINIT)(int dev_id);
typedef RTS_API int (*STARS_GET_INFO)(stars_info_t *info);
typedef RTS_API void* (*STARS_GET_HANDLE)(int dev_id, unsigned int pool_id);
typedef RTS_API void* (*STARS_GET_HANDLE_EX)(int dev_id, unsigned int pool_id, stars_handle_attrs_t *config);
typedef RTS_API int (*STARS_RELEASE_HANDLE)(void *phandle);
typedef RTS_API int (*STARS_SEND_TASK)(void *phandle, stars_trans_parm_t *trans_parm, unsigned int task_count);
typedef RTS_API int (*STARS_WAIT_CQE)(void *phandle, unsigned int task_count,
                                      unsigned int *task_cmpl_count, void *output);
typedef RTS_API int (*STARS_GET_EVENT_ID)(int dev_id, unsigned int count,
                                          event_id_type_t *event_id, unsigned int pool_id);
typedef RTS_API int (*STARS_RELEASE_EVENT_ID)(int dev_id, unsigned int count, event_id_type_t *event_id);
typedef RTS_API int (*STARS_GET_PASID)(int dev_id, unsigned int *pasid);
typedef RTS_API int (*STARS_PIN_UMEM)(int dev_id, void *vma, unsigned int size, uint64_t *cookie);
typedef RTS_API int (*STARS_UNPIN_UMEM)(int dev_id, uint64_t cookie);
typedef RTS_API int (*STARS_SDMA_AUTHORIZE)(int dev_id, int *pid_list, unsigned int pid_num);
typedef RTS_API int (*STARS_SEND_TASK_WITH_ID)(void *phandle, stars_trans_parm_t *trans_parm,
                                               unsigned int task_count, unsigned int *task_id);
typedef RTS_API int (*STARS_WAIT_CQE_WITH_ID)(void *phandle, unsigned int task_id, unsigned int task_count,
                                              unsigned int *task_cmpl_count, void *output, int timeout);

extern STARS_DEV_INIT           api_stars_dev_init;
extern STARS_DEV_DEINIT         api_stars_dev_deinit;
extern STARS_GET_INFO           api_stars_get_info;
extern STARS_GET_HANDLE         api_stars_get_handle;
extern STARS_GET_HANDLE_EX      api_stars_get_handle_ex;
extern STARS_RELEASE_HANDLE     api_stars_release_handle;
extern STARS_SEND_TASK          api_stars_send_task;
extern STARS_WAIT_CQE           api_stars_wait_cqe;
extern STARS_GET_EVENT_ID       api_stars_get_event_id;
extern STARS_RELEASE_EVENT_ID   api_stars_release_event_id;
extern STARS_GET_PASID          api_stars_get_pasid;
extern STARS_PIN_UMEM           api_stars_pin_umem;
extern STARS_UNPIN_UMEM         api_stars_unpin_umem;
extern STARS_SDMA_AUTHORIZE     api_stars_sdma_authorize;
extern STARS_SEND_TASK_WITH_ID  api_stars_send_task_with_id;
extern STARS_WAIT_CQE_WITH_ID   api_stars_wait_cqe_with_id;

ucg_status_t sct_stars_load();
void sct_stars_unload(void);

const scs_stars_info_t *sct_stars_get_info(void);

#ifdef UCG_STARS_DEBUG
void sct_stars_print_trans_parm_info(stars_trans_parm_t *stars_tasks, uint32_t task_count);
void sct_stars_print_wait_output(stars_wait_output_t *output);
#else
static inline void sct_stars_print_trans_parm_info(stars_trans_parm_t *stars_tasks, uint32_t task_count)
{
}

static inline void sct_stars_print_wait_output(stars_wait_output_t *output)
{
}
#endif

const scs_machine_offload_t *scs_stars_get_machine_info();
stars_trans_parm_t* scs_stars_get_trans_parm();

ucs_status_t scs_stars_alloc_events(uint8_t dev_id, uint16_t count, void *event, uint8_t flag);
ucs_status_t scs_stars_free_events(uint8_t dev_id, uint16_t count, void *event, uint8_t flag);

UCS_F_ALWAYS_INLINE void sct_stars_print_info(stars_info_t *stars)
{
    ucg_debug("stars_info_t { dev_num %d } ", stars->dev_num);
    struct device_info *dev_info = NULL;
    for (uint32_t idx = 0; idx < stars->dev_num; ++idx) {
        dev_info = &stars->dev_info[idx];
        ucg_debug("device_info { name %s, chip_idx %d, die_idx %d, io_mode %d }",
                  dev_info->name, dev_info->chip_index, dev_info->die_index, dev_info->io_mode);
    }
}

#endif