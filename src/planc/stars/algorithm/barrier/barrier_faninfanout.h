#ifndef BARRIER_FANINFANOUT_H_
#define BARRIER_FANINFANOUT_H_

#include "barrier.h"
#include "planc_stars_algo.h"

#define SEND_RECV_BYTE 8
#define KNTREE_ITER_ROOT 0
#define EID_IDX 0

ucg_status_t UCG_STARS_ALGO_FUN(barrier_faninfanout, meta_trigger)(ucg_plan_op_t *meta_op, ucg_plan_op_t *ucg_op);

#endif