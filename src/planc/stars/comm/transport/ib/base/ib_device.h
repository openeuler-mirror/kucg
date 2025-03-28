/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef SCT_IB_DEVICE_H
#define SCT_IB_DEVICE_H

#include "sct_iface.h"
#include "ib_verbs.h"

#define SCT_IB_QPN_ORDER                  24  /* How many bits can be an IB QP number */
#define SCT_IB_LRH_LEN                    8   /* IB Local routing header */
#define SCT_IB_GRH_LEN                    40  /* IB GLobal routing header */
#define SCT_IB_BTH_LEN                    12  /* IB base transport header */
#define SCT_IB_ROCE_LEN                   14  /* Ethernet header -
                                                 6B for Destination MAC +
                                                 6B for Source MAC + 2B Type (RoCE) */
#define SCT_IB_RETH_LEN                   16  /* IB RDMA header */
#define SCT_IB_ICRC_LEN                   4   /* IB invariant crc footer */
#define SCT_IB_VCRC_LEN                   2   /* IB variant crc footer */
#define SCT_IB_DELIM_LEN                  2   /* IB wire delimiter */
#define SCT_IB_PKEY_PARTITION_MASK        0x7fff /* IB partition number mask */
#define SCT_IB_PKEY_MEMBERSHIP_MASK       0x8000 /* Full/send-only member */
#define SCT_IB_DEV_MAX_PORTS              2
#define SCT_IB_FABRIC_TIME_MAX            32
#define SCT_IB_LINK_LOCAL_PREFIX          be64toh(0xfe80000000000000UL) /* IBTA 4.1.1 12a */
#define SCT_IB_SITE_LOCAL_PREFIX          be64toh(0xfec0000000000000UL) /* IBTA 4.1.1 12b */
#define SCT_IB_SITE_LOCAL_MASK            be64toh(0xffffffffffff0000UL) /* IBTA 4.1.1 12b */
#define SCT_IB_DEFAULT_ROCEV2_DSCP        106  /* Default DSCP for RoCE v2 */
#define SCT_IB_ROCE_UDP_SRC_PORT_BASE     0xC000
#define SCT_IB_DEVICE_SYSFS_PFX           "/sys/class/infiniband/%s"
#define SCT_IB_DEVICE_SYSFS_FMT           SCT_IB_DEVICE_SYSFS_PFX "/device/%s"
#define SCT_IB_DEVICE_SYSFS_GID_ATTR_PFX  SCT_IB_DEVICE_SYSFS_PFX "/ports/%d/gid_attrs"
#define SCT_IB_DEVICE_SYSFS_GID_TYPE_FMT  SCT_IB_DEVICE_SYSFS_GID_ATTR_PFX "/types/%d"
#define SCT_IB_DEVICE_SYSFS_GID_NDEV_FMT  SCT_IB_DEVICE_SYSFS_GID_ATTR_PFX "/ndevs/%d"


enum {
    UCT_IB_DEVICE_STAT_ASYNC_EVENT,
    UCT_IB_DEVICE_STAT_LAST
};


typedef enum sct_ib_roce_version {
    UCT_IB_DEVICE_ROCE_V1,
    UCT_IB_DEVICE_ROCE_V1_5,
    UCT_IB_DEVICE_ROCE_V2,
    UCT_IB_DEVICE_ROCE_ANY
} sct_ib_roce_version_t;


enum {
    UCT_IB_DEVICE_FLAG_LINK_IB      = UCS_BIT(5),   /* Require only IB */
    UCT_IB_DEVICE_FLAG_ODP_IMPLICIT = UCS_BIT(9),
};


/**
 * Flags which specify which address fields are present
 */
enum {
    /* GID index, used for both ETH or IB link layer.  */
    UCT_IB_ADDRESS_FLAG_GID_INDEX      = UCS_BIT(0),
    /* Defines path MTU size, used for both ETH or IB link layer. */
    UCT_IB_ADDRESS_FLAG_PATH_MTU       = UCS_BIT(1),
    /* PKEY value, used for both ETH or IB link layer. */
    UCT_IB_ADDRESS_FLAG_PKEY           = UCS_BIT(2),

    /* If set - ETH link layer, else- IB link layer. */
    UCT_IB_ADDRESS_FLAG_LINK_LAYER_ETH = UCS_BIT(3),

    /* Used for ETH link layer. */
    UCT_IB_ADDRESS_FLAG_ROCE_IPV6      = UCS_BIT(4),
    /* Used for ETH link layer, following bits are used to pack RoCE version. */
    UCT_IB_ADDRESS_FLAG_ETH_LAST       = UCS_BIT(5),

    /* Used for IB link layer. */
    UCT_IB_ADDRESS_FLAG_SUBNET16       = UCS_BIT(4),
    /* Used for IB link layer. */
    UCT_IB_ADDRESS_FLAG_SUBNET64       = UCS_BIT(5),
    /* Used for IB link layer. */
    UCT_IB_ADDRESS_FLAG_IF_ID          = UCS_BIT(6)
};


/**
 * IB network address
 */
typedef struct sct_ib_address {
    /* Using flags from UCT_IB_ADDRESS_FLAG_xx
     * For ETH link layer, the 4 msb's are used to indicate the RoCE version -
     * (by shifting the UCT_IB_DEVICE_ROCE_xx values when packing and unpacking
     * the ib address) */
    uint8_t            flags;
    /* Following fields appear in this order (if specified by flags).
     * The full gid always appears last:
     * - uint16_t lid
     * - uint64_t if_id
     * - uint16_t subnet16
     * - uint64_t subnet64
     * For RoCE:
     * - uint8_t gid[16]
     */
} UCS_S_PACKED sct_ib_address_t;


/**
 * PCI identifier of a device
 */
typedef struct {
    uint16_t                    vendor;
    uint16_t                    device;
} sct_ib_pci_id_t;


/**
 * IB device specification.
 */
typedef struct sct_ib_device_spec {
    const char                  *name;
    sct_ib_pci_id_t             pci_id;
    unsigned                    flags;
    uint8_t                     priority;
} sct_ib_device_spec_t;


/**
 * IB async event descriptor.
 */
typedef struct sct_ib_async_event {
    enum ibv_event_type event_type;             /* Event type */
    union {
        uint8_t         port_num;               /* Port number */
        uint32_t        qp_num;                 /* QP number */
        uint32_t        dct_num;                /* DCT number */
        void            *cookie;                /* Pointer to resource */
        uint32_t        resource_id;            /* Opaque resource ID */
    };
} sct_ib_async_event_t;


/**
 * IB async event waiting context.
 */
typedef struct sct_ib_async_event_wait {
    void                (*cb)(struct sct_ib_async_event_wait*); /* Callback */
    ucs_callbackq_t     *cbq;                   /* Async queue for callback */
    int                 cb_id;                  /* Scheduled callback ID */
} sct_ib_async_event_wait_t;


/**
 * IB async event state.
 */
typedef struct {
    unsigned                  flag;             /* Event happened */
    sct_ib_async_event_wait_t *wait_ctx;        /* Waiting context */
} sct_ib_async_event_val_t;


KHASH_TYPE(sct_ib_async_event, sct_ib_async_event_t, sct_ib_async_event_val_t);


/**
 * IB device (corresponds to HCA)
 */
typedef struct sct_ib_device {
    struct ibv_context          *ibv_context;    /* Verbs context */
    sct_ib_device_attr          dev_attr;        /* Cached device attributes */
    uint8_t                     first_port;      /* Number of first port (usually 1) */
    uint8_t                     num_ports;       /* Amount of physical ports */
    ucg_sys_cpuset_t            local_cpus;      /* CPUs local to device */
    int                         numa_node;       /* NUMA node of the device */
    int                         async_events;    /* Whether async events are handled */
    struct ibv_port_attr        port_attr[SCT_IB_DEV_MAX_PORTS]; /* Cached port attributes */
    sct_ib_pci_id_t             pci_id;
    unsigned                    flags;

    /* Async event subscribers */
    ucs_spinlock_t              async_event_lock;
    khash_t(sct_ib_async_event) async_events_hash;
} sct_ib_device_t;


/**
 * RoCE version
 */
typedef struct sct_ib_roce_version_info {
    /** RoCE version described by the UCT_IB_DEVICE_ROCE_xx values */
    sct_ib_roce_version_t ver;
    /** Address family of the port */
    sa_family_t           addr_family;
} sct_ib_roce_version_info_t;


typedef struct {
    union ibv_gid              gid;
    uint8_t                    gid_index;    /* IB/RoCE GID index to use */
    sct_ib_roce_version_info_t roce_info;    /* For a RoCE port */
} sct_ib_device_gid_info_t;


extern const double sct_ib_qp_rnr_time_ms[];


/**
 * Check if a port on a device is active and supports the given flags.
 */
ucs_status_t sct_ib_device_port_check(sct_ib_device_t *dev, uint8_t port_num,
                                      unsigned flags);


/*
 * Helper function to list IB transport resources.
 *
 * @param dev              IB device.
 * @param flags            Transport requirements from IB device (see UCT_IB_RESOURCE_FLAG_xx)
 * @param devices_p        Filled with a pointer to an array of devices.
 * @param num_devices_p    Filled with the number of devices.
 */
ucs_status_t sct_ib_device_query_ports(sct_ib_device_t *dev, unsigned flags,
                                       sct_tl_device_resource_t **devices_p,
                                       unsigned *num_devices_p);

ucs_status_t sct_ib_device_query(sct_ib_device_t *dev,
                                 struct ibv_device *ibv_device);

ucs_status_t sct_ib_device_init(sct_ib_device_t *dev, uint8_t use_uct_md,
                                struct ibv_device *ibv_device, int async_events);

void sct_ib_device_cleanup(sct_ib_device_t *dev);


/**
 * @return device specification.
 */
const sct_ib_device_spec_t* sct_ib_device_spec(sct_ib_device_t *dev);


/**
 * Select the best gid to use and set its information on the RoCE port -
 * gid index, RoCE version and address family.
 *
 * @param [in]  dev             IB device.
 * @param [in]  port_num        Port number.
 * @param [out] gid_info        Filled with the selected gid index and the
 *                              port's RoCE version and address family.
 */
ucs_status_t sct_ib_device_select_gid(sct_ib_device_t *dev,
                                      uint8_t port_num,
                                      sct_ib_device_gid_info_t *gid_info);


/**
 * @return device name.
 */
const char *sct_ib_device_name(sct_ib_device_t *dev);


/**
 * @return whether the port is InfiniBand
 */
int sct_ib_device_is_port_ib(sct_ib_device_t *dev, uint8_t port_num);


/**
 * @return whether the port is RoCE
 */
int sct_ib_device_is_port_roce(sct_ib_device_t *dev, uint8_t port_num);


/**
 * @return 1 if the gid_raw is 0, 0 otherwise.
 */
int sct_ib_device_is_gid_raw_empty(uint8_t *gid_raw);


/**
 * Convert time-in-seconds to IB fabric QP time value
 */
uint8_t sct_ib_to_qp_fabric_time(double time);


/**
 * Convert time-in-seconds to IB fabric RNR time value
 */
uint8_t sct_ib_to_rnr_fabric_time(double time);


/**
 * @return MTU in bytes.
 */
size_t sct_ib_mtu_value(enum ibv_mtu mtu);


/**
 * Modify QP to a given state and check for error
 */
ucs_status_t sct_ib_modify_qp(struct ibv_qp *qp, enum ibv_qp_state state);

ucs_status_t sct_ib_device_find_port(sct_ib_device_t *dev,
                                     const char *resource_dev_name,
                                     uint8_t *p_port_num);

unsigned sct_ib_device_get_roce_lag_level(sct_ib_device_t *dev,
                                          uint8_t port_num,
                                          uint8_t gid_index);


static inline struct ibv_port_attr* sct_ib_device_port_attr(sct_ib_device_t *dev,
                                                            uint8_t port_num)
{
    return &dev->port_attr[port_num - dev->first_port];
}

const char *sct_ib_roce_version_str(sct_ib_roce_version_t roce_ver);

const char *sct_ib_gid_str(const union ibv_gid *gid, char *str, size_t max_size);

ucs_status_t sct_ib_device_query_gid(sct_ib_device_t *dev, uint8_t port_num,
                                     unsigned gid_index, union ibv_gid *gid);

ucs_status_t sct_ib_device_query_gid_info(struct ibv_context *ctx, const char *dev_name,
                                          uint8_t port_num, unsigned gid_index,
                                          sct_ib_device_gid_info_t *info);

int sct_ib_device_test_roce_gid_index(sct_ib_device_t *dev, uint8_t port_num,
                                      const union ibv_gid *gid,
                                      uint8_t gid_index);

ucs_status_t sct_ib_device_async_event_register(sct_ib_device_t *dev,
                                                enum ibv_event_type event_type,
                                                uint32_t resource_id);

ucs_status_t sct_ib_device_async_event_wait(sct_ib_device_t *dev,
                                            enum ibv_event_type event_type,
                                            uint32_t resource_id,
                                            sct_ib_async_event_wait_t *wait_ctx);

void sct_ib_device_async_event_unregister(sct_ib_device_t *dev,
                                          enum ibv_event_type event_type,
                                          uint32_t resource_id);

const char* sct_ib_ah_attr_str(char *buf, size_t max,
                               const struct ibv_ah_attr *ah_attr);

void sct_ib_handle_async_event(sct_ib_device_t *dev, sct_ib_async_event_t *event);

ucs_status_t sct_ib_device_get_roce_ndev_name(sct_ib_device_t *dev, uint8_t port_num,
                                              uint8_t gid_index, char *ndev_name, size_t max);

#endif
