/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef XUCG_SCP_ADDRESS_H
#define XUCG_SCP_ADDRESS_H

#include "scp_context.h"
#include "scp_ep.h"

typedef struct scp_address_packed_device {
    size_t                      dev_addr_len;
    uint64_t                    tl_bitmap;
    scp_rsc_index_t             rsc_index;
    scp_rsc_index_t             tl_count;
    unsigned                    num_paths;
    size_t                      tl_addrs_size;
} scp_address_packed_device_t;

typedef struct scp_address_iface_attr {
    uint64_t                    cap_flags;     /* Interface capability flags */
    uint64_t                    event_flags;   /* Interface event capability flags */
    double                      overhead;      /* Interface performance - overhead */
    uct_ppn_bandwidth_t         bandwidth;     /* Interface performance - bandwidth */
    int                         priority;      /* Priority of device */
    double                      lat_ovh;       /* Latency overhead */
    scp_rsc_index_t             dst_rsc_index; /* Destination resource index */
} scp_address_iface_attr_t;
typedef struct scp_address_iface_attr *scp_address_iface_attr_h;

/**
 * Address entry.
 */
typedef struct scp_address_entry {
    const uct_device_addr_t     *dev_addr;      /* Points to device address */
    const uct_iface_addr_t      *iface_addr;    /* Interface address, NULL if not available */
    scp_address_iface_attr_t    *iface_attr;    /* Interface attributes information */
    uint64_t                    md_flags;       /* MD reg/alloc flags */
    unsigned                    dev_num_paths;  /* Number of paths on the device */
    scp_md_index_t              md_index;       /* Memory domain index */
    scp_rsc_index_t             dev_index;      /* Device index */
} scp_address_entry_t;
typedef struct scp_address_entry *scp_address_entry_h;

typedef struct scp_unpacked_address {
    uint8_t                     flag;            /* 0 mean need unpack */
    uint64_t                    uuid;            /* Remote worker UUID */
    char                        name[SCP_WORKER_NAME_MAX]; /* Remote worker name */
    unsigned                    address_count;   /* Length of address list */
    scp_address_entry_t         address_list[0];   /* Pointer to address list */
} scp_unpacked_address_t;
typedef struct scp_unpacked_address *scp_unpacked_address_h;

typedef struct scp_address_entry_ep_addr {
    scp_lane_index_t            lane;         /* Lane index (local or remote) */
    scp_rsc_index_t             rsc_index;
    const uct_ep_addr_t         *addr;        /* Pointer to ep address */
} scp_address_entry_ep_addr_t;
typedef struct scp_address_entry_ep_addr *scp_address_entry_ep_addr_h;

typedef struct scp_ep_unpacked {
    unsigned                    num_ep_addrs;   /* How many endpoint address are in ep_addrs */
    scp_lane_index_t            sdma_ep_idx;
    uint64_t                    guid;
    scp_address_entry_ep_addr_t ep_addrs[0];    /* Endpoint addresses */
} scp_ep_unpacked_t;
typedef struct scp_ep_unpacked *scp_ep_unpacked_h;

/* max sct ep address is sizeof(sct_rc_ofd_ep_address_t) */
#define SCT_EP_ADDR_MAX_SIZE    4
/* 1 means sct ep addr length */
#define SCP_EP_PACK_SIZE        (sizeof(scp_ep_unpacked_t) + \
                                 (sizeof(scp_address_entry_ep_addr_t) + 1 + \
                                  SCT_EP_ADDR_MAX_SIZE) * SCP_MAX_LANE \
                                )

ucg_status_t scp_address_pack(scp_worker_h worker, void **buffer_p, size_t *size_p);

ucg_status_t scp_address_unpack(void *buffer, scp_unpacked_address_h *address);

ucg_status_t scp_ep_address_pack(scp_ep_h ep, scp_address_t *address);

ucg_status_t scp_ep_address_unpack(void *buffer, scp_ep_unpacked_h *unpacked_address);

#endif // XUCG_SCP_ADDRESS_H
