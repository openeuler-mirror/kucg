/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_SCP_WORKER_H
#define XUCG_SCP_WORKER_H

#include "scp_context.h"

#if ENABLE_MT

#define SCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(_worker)                 \
    do {                                                                \
        pthread_mutex_lock(&(_worker)->async);                         \
    } while (0)


#define SCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(_worker)                  \
    do {                                                                \
        pthread_mutex_unlock(&(_worker)->async);                       \
    } while (0)

#else
#define SCP_WORKER_THREAD_CS_ENTER_CONDITIONAL(_worker)
#define SCP_WORKER_THREAD_CS_EXIT_CONDITIONAL(_worker)
#endif

/**
 * Stars communication protocol worker iface, which encapsulates SCT iface, its attributes and
 * some auxiliary info needed for tag matching offloads.
 */
typedef struct scp_worker_iface {
    sct_iface_h                     iface;         /* UCT interface */
    sct_iface_attr_t                attr;          /* UCT interface attributes */
    scp_worker_h                    worker;        /* The parent worker */
    scp_rsc_index_t                 rsc_index;     /* Resource index */

    uint8_t                         flags;         /* Interface flags */
} scp_worker_iface_t;
typedef struct scp_worker_iface *scp_worker_iface_h;

/**
 * Stars communication protocol worker (thread context).
 */
typedef struct scp_worker {
    scp_context_h                    context;
    pthread_mutex_t                  async;
    sct_worker_h                     uct;               /* UCT worker handle */

    pthread_mutex_t                  submit_mutex;

    ucg_mpool_t                      ep_mp;             /* used for create scp ep instance */
    ucg_mpool_t                      ep_addr_mp;        /* used for alloc scp ep address */

    scp_worker_iface_h               *ifaces;           /* Array of pointers to interfaces,
                                                       one for each resource */
    uint8_t                          num_ifaces;        /* Number of elements in ifaces array */

    scp_rsc_index_t                  sdma_iface_idx;    /* ifaces don't exist sdma, sdma_iface_idx is UINT8_MAX */
    scp_rsc_index_t                  iface2idx[SCP_MAX_LANE];

    scp_address_t                    *address;
    size_t                           address_len;

    uint64_t                         uuid;
} scp_worker_t;

ucg_status_t scp_create_worker(scp_context_h context, scp_worker_h *worker_p);

ucg_status_t scp_worker_get_address(scp_worker_h worker, scp_address_t **address_p,
                                    size_t *address_length_p);

ucg_status_t scp_worker_progress(scp_worker_h worker);

void scp_worker_destroy(scp_worker_h worker);

/**
 * @return worker-iface struct by resource index
 */
static UCS_F_ALWAYS_INLINE scp_worker_iface_t*
scp_worker_iface(scp_worker_h worker, scp_rsc_index_t rsc_index)
{
    if (ucg_unlikely(rsc_index == SCP_NULL_RESOURCE)) {
        ucg_error("Invalid resource index.");
        return NULL;
    }

    scp_context_h context = worker->context;
    uint64_t tl_bitmap = worker->context->tl_bitmap;
    ucg_assert(UCS_BIT(rsc_index) & tl_bitmap);
    ucg_debug("rsc_index %d tl_bitmap %lu ucs_bitmap2idx %d tl_name %s dev_name %s ",
             rsc_index, tl_bitmap, ucs_bitmap2idx(tl_bitmap, rsc_index),
             context->tl_rscs[rsc_index].tl_rsc.tl_name,
             context->tl_rscs[rsc_index].tl_rsc.dev_name);
    return worker->ifaces[ucs_bitmap2idx(tl_bitmap, rsc_index)];
}

static UCS_F_ALWAYS_INLINE scp_worker_iface_t*
scp_worker_idx_iface(scp_worker_h worker, scp_rsc_index_t rsc_index)
{
    if (ucg_unlikely(rsc_index == SCP_NULL_RESOURCE)) {
        ucg_error("Invalid resource index.");
        return NULL;
    }
    return worker->ifaces[worker->iface2idx[rsc_index]];
}

/**
 * @return worker's iface attributes by resource index
 */
static UCS_F_ALWAYS_INLINE sct_iface_attr_t*
scp_worker_iface_get_attr(scp_worker_h worker, scp_rsc_index_t rsc_idx)
{
    return &scp_worker_iface(worker, rsc_idx)->attr;
}

static UCS_F_ALWAYS_INLINE sct_iface_attr_t*
scp_worker_idx_iface_get_attr(scp_worker_h worker, scp_rsc_index_t rsc_idx)
{
    return &scp_worker_idx_iface(worker, rsc_idx)->attr;
}

static inline void *scp_worker_alloc_ep_addr(scp_worker_h worker)
{
    return ucg_mpool_get(&worker->ep_addr_mp);
}

static inline scp_ep_h scp_worker_alloc_ep(scp_worker_h worker)
{
    return (scp_ep_h)ucg_mpool_get(&worker->ep_mp);
}


#endif // XUCG_SCP_WORKER_H
