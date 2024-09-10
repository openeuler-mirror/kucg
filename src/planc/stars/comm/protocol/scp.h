/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_SCP_H
#define XUCG_SCP_H

#include "scp_def.h"


ucg_status_t scp_worker_create_stars_stream(scp_worker_h worker, scp_ofd_stars_stream_h stream);

ucg_status_t scp_worker_delete_stars_stream(scp_worker_h worker, scp_ofd_stars_stream_h stream);

void scp_init_ofd_req(scp_ofd_req_h req);
void scp_release_ofd_req(scp_ofd_req_h req);

ucg_status_t scp_submit_ofd_req(scp_worker_h worker, scp_ofd_stars_stream_h stream,
                                scp_ofd_req_h request, ofd_stats_elem_h stats);

#endif // XUCG_SCP_H
