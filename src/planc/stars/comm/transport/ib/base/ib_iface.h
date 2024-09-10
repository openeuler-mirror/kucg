/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2014.  ALL RIGHTS RESERVED.
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
*
* See file LICENSE for terms.
*/

#ifndef SCT_IB_IFACE_H
#define SCT_IB_IFACE_H

#include "sct_iface.h"
#include "ib_md.h"


#define SCT_IB_MAX_IOV                     8UL
#define SCT_IB_ADDRESS_INVALID_GID_INDEX   UINT8_MAX
#define SCT_IB_ADDRESS_INVALID_PATH_MTU    ((enum ibv_mtu)0)
#define SCT_IB_ADDRESS_INVALID_PKEY        0
#define SCT_IB_ADDRESS_DEFAULT_PKEY        0xffff
#define SCT_IB_SL_NUM                      16

/* Forward declarations */
typedef struct sct_ib_iface_config   sct_ib_iface_config_t;
typedef struct sct_ib_iface_ops      sct_ib_iface_ops_t;
typedef struct sct_ib_iface          sct_ib_iface_t;


/**
 * IB port/path MTU.
 */
typedef enum sct_ib_mtu {
    UCT_IB_MTU_DEFAULT = 0,
    UCT_IB_MTU_512     = 1,
    UCT_IB_MTU_1024    = 2,
    UCT_IB_MTU_2048    = 3,
    UCT_IB_MTU_4096    = 4,
    UCT_IB_MTU_LAST
} sct_ib_mtu_t;


/**
 * Traffic direction.
 */
typedef enum {
    UCT_IB_DIR_RX,
    UCT_IB_DIR_TX,
    UCT_IB_DIR_NUM
} sct_ib_dir_t;

enum {
    UCT_IB_QPT_UNKNOWN,
    UCT_IB_QPT_DCI = IBV_QPT_DRIVER,
};


/**
 * IB address packing flags
 */
enum {
    UCT_IB_ADDRESS_PACK_FLAG_ETH           = UCS_BIT(0),
    UCT_IB_ADDRESS_PACK_FLAG_INTERFACE_ID  = UCS_BIT(1),
    UCT_IB_ADDRESS_PACK_FLAG_SUBNET_PREFIX = UCS_BIT(2),
    UCT_IB_ADDRESS_PACK_FLAG_PATH_MTU      = UCS_BIT(3),
    UCT_IB_ADDRESS_PACK_FLAG_GID_INDEX     = UCS_BIT(4),
    UCT_IB_ADDRESS_PACK_FLAG_PKEY          = UCS_BIT(5)
};


typedef struct sct_ib_address_pack_params {
    /* Packing flags, UCT_IB_ADDRESS_PACK_FLAG_xx. */
    uint64_t                          flags;
    /* GID address to pack/unpack. */
    union ibv_gid                     gid;
    /* LID address to pack/unpack. */
    uint16_t                          lid;
    /* RoCE version to pack/unpack in case of an Ethernet link layer,
       must be valid if @ref UCT_IB_ADDRESS_PACK_FLAG_ETH is set. */
    sct_ib_roce_version_info_t        roce_info;
    /* path MTU size as defined in enum ibv_mtu,
       must be valid if @ref UCT_IB_ADDRESS_PACK_FLAG_PATH_MTU is set. */
    enum ibv_mtu                      path_mtu;
    /* GID index,
       must be valid if @ref UCT_IB_ADDRESS_PACK_FLAG_GID_INDEX is set. */
    uint8_t                           gid_index;
    /* PKEY value,
       must be valid if @ref UCT_IB_ADDRESS_PACK_FLAG_PKEY is set. */
    uint16_t                          pkey;
} sct_ib_address_pack_params_t;


struct sct_ib_iface_config {
    sct_iface_config_t      super;

    size_t                  seg_size;      /* Maximal size of copy-out sends */

    struct {
        unsigned            queue_len;       /* Queue length */
        unsigned            max_batch;       /* How many fragments can be batched to one post send */
        unsigned            max_poll;        /* How many wcs can be picked when polling tx cq */
        size_t              min_inline;      /* Inline space to reserve for sends */
        sct_iface_mpool_config_t mp;
    } tx;

    /* Inline space to reserve in CQ */
    size_t                  inl[UCT_IB_DIR_NUM];

    /* Change the address type */
    int                     addr_type;

    /* Force global routing */
    int                     is_global;

    /* IB SL to use (default: AUTO) */
    unsigned long           sl;

    /* IB Traffic Class to use */
    unsigned long           traffic_class;

    /* IB hop limit / TTL */
    unsigned                hop_limit;

    /* Number of paths to expose for the interface  */
    unsigned long           num_paths;

    /* Whether to check RoCEv2 reachability by IP address and local subnet */
    int                     rocev2_local_subnet;

    /* Length of subnet prefix for reachability check */
    unsigned long           rocev2_subnet_pfx_len;

    /* Multiplier for RoCE LAG UDP source port calculation */
    unsigned                roce_path_factor;

    /* Ranges of path bits */
    UCS_CONFIG_ARRAY_FIELD(ucs_range_spec_t, ranges) lid_path_bits;

    /* IB PKEY to use */
    unsigned                pkey;

    /* Path MTU size */
    sct_ib_mtu_t            path_mtu;
};


enum {
    UCT_IB_CQ_IGNORE_OVERRUN         = UCS_BIT(0),
    UCT_IB_TM_SUPPORTED              = UCS_BIT(1)
};


typedef struct sct_ib_iface_init_attr {
    unsigned    cq_len[UCT_IB_DIR_NUM]; /* CQ length */
    size_t      seg_size;               /* Transport segment size */
    int         flags;                  /* Various flags (see enum) */
} sct_ib_iface_init_attr_t;


typedef struct sct_ib_qp_attr {
    struct ibv_qp_cap           cap;
    int                         port;
    uint32_t                    srq_num;
    unsigned                    sq_sig_all;
    unsigned                    max_inl_cqe[UCT_IB_DIR_NUM];
    struct ibv_qp_init_attr_ex  ibv;
} sct_ib_qp_attr_t;


typedef ucs_status_t (*sct_ib_iface_create_cq_func_t)(sct_ib_iface_t *iface,
                                                      sct_ib_dir_t dir,
                                                      const sct_ib_iface_init_attr_t *init_attr,
                                                      int preferred_cpu,
                                                      size_t inl);


struct sct_ib_iface_ops {
    sct_iface_ops_t                    super;
    sct_ib_iface_create_cq_func_t      create_cq;
};


struct sct_ib_iface {
    sct_base_iface_t          super;

    struct ibv_cq             *cq[UCT_IB_DIR_NUM];
    struct ibv_comp_channel   *comp_channel;

    uint8_t                   *path_bits;
    unsigned                  path_bits_count;
    unsigned                  num_paths;
    uint16_t                  pkey_index;
    uint16_t                  pkey;
    uint8_t                   addr_size;
    uint8_t                   addr_prefix_bits;
    sct_ib_device_gid_info_t  gid_info;

    struct {
        unsigned              rx_payload_offset;   /* offset from desc to payload */
        unsigned              rx_hdr_offset;       /* offset from desc to network header */
        unsigned              rx_headroom_offset;  /* offset from desc to user headroom */
        unsigned              rx_max_batch;
        unsigned              rx_max_poll;
        unsigned              tx_max_poll;
        unsigned              seg_size;
        unsigned              roce_path_factor;
        uint8_t               max_inl_cqe[UCT_IB_DIR_NUM];
        uint8_t               port_num;
        uint8_t               sl;
        uint8_t               traffic_class;
        uint8_t               hop_limit;
        uint8_t               force_global_addr;
        enum ibv_mtu          path_mtu;
    } config;

    sct_ib_iface_ops_t        *ops;
};

UCS_CLASS_DECLARE(sct_ib_iface_t, sct_ib_iface_ops_t*, sct_md_h, sct_worker_h,
                  const sct_iface_params_t*, const sct_ib_iface_config_t*,
                  const sct_ib_iface_init_attr_t*);


extern ucs_config_field_t sct_ib_iface_config_table[];
extern const char *sct_ib_mtu_values[];


/**
 * @return Whether the port used by this interface is RoCE
 */
int sct_ib_iface_is_roce(sct_ib_iface_t *iface);


/**
 * Get the expected size of IB packed address.
 *
 * @param [in]  params   Address parameters as defined in
 *                       @ref sct_ib_address_pack_params_t.
 *
 * @return IB address size of the given link scope.
 */
size_t sct_ib_address_size(const sct_ib_address_pack_params_t *params);


/**
 * @return IB address packing flags of the given iface.
 */
unsigned sct_ib_iface_address_pack_flags(sct_ib_iface_t *iface);


/**
 * @return IB address size of the given iface.
 */
size_t sct_ib_iface_address_size(sct_ib_iface_t *iface);


/**
 * Pack IB address.
 *
 * @param [in]     params   Address parameters as defined in
 *                          @ref sct_ib_address_pack_params_t.
 * @param [in/out] ib_addr  Filled with packed ib address. Size of the structure
 *                          must be at least what @ref sct_ib_address_size()
 *                          returns for the given scope.
 */
void sct_ib_address_pack(const sct_ib_address_pack_params_t *params,
                         sct_ib_address_t *ib_addr);


/**
 * Pack the IB address of the given iface.
 *
 * @param [in]  iface      Iface whose IB address to pack.
 * @param [in/out] ib_addr Filled with packed ib address. Size of the structure
 *                         must be at least what @ref sct_ib_address_size()
 *                         returns for the given scope.
 */
void sct_ib_iface_address_pack(sct_ib_iface_t *iface, sct_ib_address_t *ib_addr);


/**
 * Unpack IB address.
 *
 * @param [in]  ib_addr    IB address to unpack.
 * @param [out] params_p   Filled with address attributes as in
 *                         @ref sct_ib_address_pack_params_t.
 */
void sct_ib_address_unpack(const sct_ib_address_t *ib_addr,
                           sct_ib_address_pack_params_t *params_p);


ucs_status_t sct_ib_iface_get_device_address(sct_iface_h tl_iface,
                                             uct_device_addr_t *dev_addr);

int sct_ib_iface_is_reachable(const sct_iface_h tl_iface, const uct_device_addr_t *dev_addr,
                              const uct_iface_addr_t *iface_addr);

/*
 * @param xport_hdr_len       How many bytes this transport adds on top of IB header (LRH+BTH+iCRC+vCRC)
 */
ucs_status_t sct_ib_iface_query(sct_ib_iface_t *iface, size_t xport_hdr_len,
                                sct_iface_attr_t *iface_attr);


int sct_ib_iface_is_roce_v2(sct_ib_iface_t *iface, sct_ib_device_t *dev);


/**
 * Select the IB gid index and RoCE version to use for a RoCE port.
 *
 * @param iface                 IB interface
 * @param md_config_index       Gid index from the md configuration.
 */
ucs_status_t sct_ib_iface_init_roce_gid_info(sct_ib_iface_t *iface,
                                             size_t md_config_index);


static inline sct_ib_md_t* sct_ib_iface_md(sct_ib_iface_t *iface)
{
    return ucs_derived_of(iface->super.md, sct_ib_md_t);
}

static inline sct_ib_device_t* sct_ib_iface_device(sct_ib_iface_t *iface)
{
    return &sct_ib_iface_md(iface)->dev;
}

static inline struct ibv_port_attr* sct_ib_iface_port_attr(sct_ib_iface_t *iface)
{
    return sct_ib_device_port_attr(sct_ib_iface_device(iface), iface->config.port_num);
}


void sct_ib_iface_fill_ah_attr_from_gid_lid(sct_ib_iface_t *iface, uint16_t lid,
                                            const union ibv_gid *gid,
                                            uint8_t gid_index,
                                            unsigned path_index,
                                            struct ibv_ah_attr *ah_attr);

void sct_ib_iface_fill_ah_attr_from_addr(sct_ib_iface_t *iface,
                                         const sct_ib_address_t *ib_addr,
                                         unsigned path_index,
                                         struct ibv_ah_attr *ah_attr,
                                         enum ibv_mtu *path_mtu);

#define SCT_IB_IFACE_FMT "%s:%d"
#define SCT_IB_IFACE_ARG(_iface) \
    sct_ib_device_name(sct_ib_iface_device(_iface)), (_iface)->config.port_num

#endif
