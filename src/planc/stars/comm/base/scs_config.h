/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */


#ifndef SCS_CONFIG_H
#define SCS_CONFIG_H


/* Define to 1 if using 'alloca.c'. */
/* #undef C_ALLOCA */

/* Enable assertions */
#define ENABLE_ASSERT 1

/* Enable builtin memcpy */
#define ENABLE_BUILTIN_MEMCPY 1

/* Enable collecting data */
#define ENABLE_DEBUG_DATA 0

/* Enable fault injection code */
/* #undef ENABLE_FAULT_INJECTION */

/* Enable memory tracking */
/* #undef ENABLE_MEMTRACK */

/* Enable thread support in UCP and UCT */
#define ENABLE_MT 0

/* Enable checking user parameters */
#define ENABLE_PARAMS_CHECK 1

/* Enable statistics */
/* #undef ENABLE_STARS_STATS */

/* Enable symbol override */
#define ENABLE_SYMBOL_OVERRIDE 1

/* Enable tuning */
/* #undef ENABLE_TUNING */

/* Enable Groups and collective operations support (UCG) */
/* #undef ENABLE_UCG */

/* bfd_section_size 2-args version */
#define HAVE_1_ARG_BFD_SECTION_SIZE 0

/* Huawei Kunpeng 920 */
#define HAVE_AARCH64_HI1620 1

/* Cavium ThunderX1 */
/* #undef HAVE_AARCH64_THUNDERX1 */

/* Cavium ThunderX2 */
/* #undef HAVE_AARCH64_THUNDERX2 */

/* Define to 1 if you have 'alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if <alloca.h> works. */
#define HAVE_ALLOCA_H 1

/* Check attribute [optimize] */
#define HAVE_ATTRIBUTE_NOOPTIMIZE 1

/* Define to 1 if you have the `clearenv' function. */
#define HAVE_CLEARENV 1

/* Define to 1 if the system has the type `cpu_set_t'. */
#define HAVE_CPU_SET_T 1

/* Enable CUDA support */
/* #undef HAVE_CUDA */

/* Define to 1 if you have the <cuda.h> header file. */
/* #undef HAVE_CUDA_H */

/* Define to 1 if you have the <cuda_runtime.h> header file. */
/* #undef HAVE_CUDA_RUNTIME_H */

/* DC DV support */
#define HAVE_DC_DV 1

/* DC EXP support */
/* #undef HAVE_DC_EXP */

/* Define to 1 if you have the declaration of `asprintf', and to 0 if you
   don't. */
#define HAVE_DECL_ASPRINTF 1

/* Define to 1 if you have the declaration of `basename', and to 0 if you
   don't. */
#define HAVE_DECL_BASENAME 1

/* Define to 1 if you have the declaration of `bfd_get_section_flags', and to
   0 if you don't. */
#define HAVE_DECL_BFD_GET_SECTION_FLAGS 0

/* Define to 1 if you have the declaration of `bfd_get_section_vma', and to 0
   if you don't. */
#define HAVE_DECL_BFD_GET_SECTION_VMA 0

/* Define to 1 if you have the declaration of `bfd_section_flags', and to 0 if
   you don't. */
#define HAVE_DECL_BFD_SECTION_FLAGS 0

/* Define to 1 if you have the declaration of `bfd_section_vma', and to 0 if
   you don't. */
#define HAVE_DECL_BFD_SECTION_VMA 0

/* Define to 1 if you have the declaration of `CPU_ISSET', and to 0 if you
   don't. */
#define HAVE_DECL_CPU_ISSET 1

/* Define to 1 if you have the declaration of `CPU_ZERO', and to 0 if you
   don't. */
#define HAVE_DECL_CPU_ZERO 1

/* Define to 1 if you have the declaration of `ethtool_cmd_speed', and to 0 if
   you don't. */
#define HAVE_DECL_ETHTOOL_CMD_SPEED 1

/* Define to 1 if you have the declaration of `fmemopen', and to 0 if you
   don't. */
#define HAVE_DECL_FMEMOPEN 1

/* Define to 1 if you have the declaration of `F_SETOWN_EX', and to 0 if you
   don't. */
#define HAVE_DECL_F_SETOWN_EX 1

/* Define to 1 if you have the declaration of `gdr_copy_to_mapping', and to 0
   if you don't. */
/* #undef HAVE_DECL_GDR_COPY_TO_MAPPING */

/* Define to 1 if you have the declaration of `IBV_ACCESS_ON_DEMAND', and to 0
   if you don't. */
#define HAVE_DECL_IBV_ACCESS_ON_DEMAND 1

/* Define to 1 if you have the declaration of `IBV_ACCESS_RELAXED_ORDERING',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_ACCESS_RELAXED_ORDERING 1

/* Define to 1 if you have the declaration of `ibv_advise_mr', and to 0 if you
   don't. */
#define HAVE_DECL_IBV_ADVISE_MR 1

/* Define to 1 if you have the declaration of `ibv_alloc_dm', and to 0 if you
   don't. */
#define HAVE_DECL_IBV_ALLOC_DM 1

/* Define to 1 if you have the declaration of `ibv_alloc_td', and to 0 if you
   don't. */
#define HAVE_DECL_IBV_ALLOC_TD 1

/* Define to 1 if you have the declaration of `ibv_cmd_modify_qp', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_CMD_MODIFY_QP 1

/* Define to 1 if you have the declaration of
   `IBV_CREATE_CQ_ATTR_IGNORE_OVERRUN', and to 0 if you don't. */
#define HAVE_DECL_IBV_CREATE_CQ_ATTR_IGNORE_OVERRUN 1

/* Define to 1 if you have the declaration of `ibv_create_qp_ex', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_CREATE_QP_EX 1

/* Define to 1 if you have the declaration of `ibv_create_srq', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_CREATE_SRQ 1

/* Define to 1 if you have the declaration of `ibv_create_srq_ex', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_CREATE_SRQ_EX 1

/* Define to 1 if you have the declaration of `IBV_EVENT_GID_CHANGE', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EVENT_GID_CHANGE 1

/* Define to 1 if you have the declaration of `ibv_event_type_str', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EVENT_TYPE_STR 1

/* Define to 1 if you have the declaration of `IBV_EXP_ACCESS_ALLOCATE_MR',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_ACCESS_ALLOCATE_MR 0

/* Define to 1 if you have the declaration of `IBV_EXP_ACCESS_ON_DEMAND', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_ACCESS_ON_DEMAND 0

/* Define to 1 if you have the declaration of `ibv_exp_alloc_dm', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_ALLOC_DM 0

/* Define to 1 if you have the declaration of `IBV_EXP_ATOMIC_HCA_REPLY_BE',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_ATOMIC_HCA_REPLY_BE 0

/* Define to 1 if you have the declaration of `IBV_EXP_CQ_IGNORE_OVERRUN', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_CQ_IGNORE_OVERRUN 0

/* Define to 1 if you have the declaration of `IBV_EXP_CQ_MODERATION', and to
   0 if you don't. */
#define HAVE_DECL_IBV_EXP_CQ_MODERATION 0

/* Define to 1 if you have the declaration of `ibv_exp_create_qp', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_CREATE_QP 0

/* Define to 1 if you have the declaration of `ibv_exp_create_res_domain', and
   to 0 if you don't. */
/* #undef HAVE_DECL_IBV_EXP_CREATE_RES_DOMAIN */

/* Define to 1 if you have the declaration of `ibv_exp_create_srq', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EXP_CREATE_SRQ 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_DCT_OOO_RW_DATA_PLACEMENT', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_DCT_OOO_RW_DATA_PLACEMENT 0

/* Define to 1 if you have the declaration of `ibv_exp_destroy_res_domain',
   and to 0 if you don't. */
/* #undef HAVE_DECL_IBV_EXP_DESTROY_RES_DOMAIN */

/* Define to 1 if you have the declaration of
   `IBV_EXP_DEVICE_ATTR_PCI_ATOMIC_CAPS', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_DEVICE_ATTR_PCI_ATOMIC_CAPS 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_DEVICE_ATTR_RESERVED_2', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_DEVICE_ATTR_RESERVED_2 0

/* Define to 1 if you have the declaration of `IBV_EXP_DEVICE_DC_TRANSPORT',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_DEVICE_DC_TRANSPORT 0

/* Define to 1 if you have the declaration of `IBV_EXP_DEVICE_MR_ALLOCATE',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_DEVICE_MR_ALLOCATE 0

/* Define to 1 if you have the declaration of `IBV_EXP_MR_FIXED_BUFFER_SIZE',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_MR_FIXED_BUFFER_SIZE 0

/* Define to 1 if you have the declaration of `IBV_EXP_MR_INDIRECT_KLMS', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_MR_INDIRECT_KLMS 0

/* Define to 1 if you have the declaration of `IBV_EXP_ODP_SUPPORT_IMPLICIT',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_ODP_SUPPORT_IMPLICIT 0

/* Define to 1 if you have the declaration of `ibv_exp_post_send', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_POST_SEND 0

/* Define to 1 if you have the declaration of `ibv_exp_prefetch_mr', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EXP_PREFETCH_MR 0

/* Define to 1 if you have the declaration of `IBV_EXP_PREFETCH_WRITE_ACCESS',
   and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_PREFETCH_WRITE_ACCESS 0

/* Define to 1 if you have the declaration of `IBV_EXP_QPT_DC_INI', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EXP_QPT_DC_INI 0

/* Define to 1 if you have the declaration of `IBV_EXP_QP_CREATE_UMR', and to
   0 if you don't. */
#define HAVE_DECL_IBV_EXP_QP_CREATE_UMR 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_QP_INIT_ATTR_RES_DOMAIN', and to 0 if you don't. */
/* #undef HAVE_DECL_IBV_EXP_QP_INIT_ATTR_RES_DOMAIN */

/* Define to 1 if you have the declaration of
   `IBV_EXP_QP_OOO_RW_DATA_PLACEMENT', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_QP_OOO_RW_DATA_PLACEMENT 0

/* Define to 1 if you have the declaration of `ibv_exp_query_device', and to 0
   if you don't. */
#define HAVE_DECL_IBV_EXP_QUERY_DEVICE 0

/* Define to 1 if you have the declaration of `ibv_exp_query_gid_attr', and to
   0 if you don't. */
#define HAVE_DECL_IBV_EXP_QUERY_GID_ATTR 0

/* Define to 1 if you have the declaration of `ibv_exp_reg_mr', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_REG_MR 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_RES_DOMAIN_THREAD_MODEL', and to 0 if you don't. */
/* #undef HAVE_DECL_IBV_EXP_RES_DOMAIN_THREAD_MODEL */

/* Define to 1 if you have the declaration of
   `IBV_EXP_SEND_EXT_ATOMIC_INLINE', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_SEND_EXT_ATOMIC_INLINE 0

/* Define to 1 if you have the declaration of `ibv_exp_setenv', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_SETENV 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP 0

/* Define to 1 if you have the declaration of
   `IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD', and to 0 if you don't. */
#define HAVE_DECL_IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD 0

/* Define to 1 if you have the declaration of `IBV_EXP_WR_NOP', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_EXP_WR_NOP 0

/* Define to 1 if you have the declaration of `ibv_get_async_event', and to 0
   if you don't. */
#define HAVE_DECL_IBV_GET_ASYNC_EVENT 1

/* Define to 1 if you have the declaration of `ibv_get_device_name', and to 0
   if you don't. */
#define HAVE_DECL_IBV_GET_DEVICE_NAME 1

/* Define to 1 if you have the declaration of `IBV_LINK_LAYER_ETHERNET', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_LINK_LAYER_ETHERNET 1

/* Define to 1 if you have the declaration of `IBV_LINK_LAYER_INFINIBAND', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_LINK_LAYER_INFINIBAND 1

/* Define to 1 if you have the declaration of `ibv_mlx5_exp_get_cq_info', and
   to 0 if you don't. */
/* #undef HAVE_DECL_IBV_MLX5_EXP_GET_CQ_INFO */

/* Define to 1 if you have the declaration of `ibv_mlx5_exp_get_qp_info', and
   to 0 if you don't. */
/* #undef HAVE_DECL_IBV_MLX5_EXP_GET_QP_INFO */

/* Define to 1 if you have the declaration of `ibv_mlx5_exp_get_srq_info', and
   to 0 if you don't. */
/* #undef HAVE_DECL_IBV_MLX5_EXP_GET_SRQ_INFO */

/* Define to 1 if you have the declaration of `ibv_mlx5_exp_update_cq_ci', and
   to 0 if you don't. */
/* #undef HAVE_DECL_IBV_MLX5_EXP_UPDATE_CQ_CI */

/* Define to 1 if you have the declaration of `IBV_ODP_SUPPORT_IMPLICIT', and
   to 0 if you don't. */
#define HAVE_DECL_IBV_ODP_SUPPORT_IMPLICIT 1

/* Define to 1 if you have the declaration of `IBV_QPF_GRH_REQUIRED', and to 0
   if you don't. */
#define HAVE_DECL_IBV_QPF_GRH_REQUIRED 1

/* have upstream ibv_query_device_ex */
#define HAVE_DECL_IBV_QUERY_DEVICE_EX 1

/* Define to 1 if you have the declaration of `ibv_query_gid', and to 0 if you
   don't. */
#define HAVE_DECL_IBV_QUERY_GID 1

/* Define to 1 if you have the declaration of `ibv_wc_status_str', and to 0 if
   you don't. */
#define HAVE_DECL_IBV_WC_STATUS_STR 1

/* Define to 1 if you have the declaration of `IPPROTO_TCP', and to 0 if you
   don't. */
#define HAVE_DECL_IPPROTO_TCP 1

/* Define to 1 if you have the declaration of `MADV_FREE', and to 0 if you
   don't. */
#define HAVE_DECL_MADV_FREE 1

/* Define to 1 if you have the declaration of `MADV_REMOVE', and to 0 if you
   don't. */
#define HAVE_DECL_MADV_REMOVE 1

/* Define to 1 if you have the declaration of
   `MLX5DV_CQ_INIT_ATTR_MASK_CQE_SIZE', and to 0 if you don't. */
#define HAVE_DECL_MLX5DV_CQ_INIT_ATTR_MASK_CQE_SIZE 1

/* Define to 1 if you have the declaration of `mlx5dv_create_qp', and to 0 if
   you don't. */
#define HAVE_DECL_MLX5DV_CREATE_QP 1

/* Define to 1 if you have the declaration of `MLX5DV_DCTYPE_DCT', and to 0 if
   you don't. */
#define HAVE_DECL_MLX5DV_DCTYPE_DCT 1

/* Define to 1 if you have the declaration of
   `mlx5dv_devx_subscribe_devx_event', and to 0 if you don't. */
#define HAVE_DECL_MLX5DV_DEVX_SUBSCRIBE_DEVX_EVENT 1

/* Define to 1 if you have the declaration of `mlx5dv_init_obj', and to 0 if
   you don't. */
#define HAVE_DECL_MLX5DV_INIT_OBJ 1

/* Define to 1 if you have the declaration of `mlx5dv_is_supported', and to 0
   if you don't. */
#define HAVE_DECL_MLX5DV_IS_SUPPORTED 1

/* Define to 1 if you have the declaration of `MLX5DV_OBJ_AH', and to 0 if you
   don't. */
#define HAVE_DECL_MLX5DV_OBJ_AH 1

/* Define to 1 if you have the declaration of
   `MLX5DV_QP_CREATE_ALLOW_SCATTER_TO_CQE', and to 0 if you don't. */
#define HAVE_DECL_MLX5DV_QP_CREATE_ALLOW_SCATTER_TO_CQE 1

/* Define to 1 if you have the declaration of `MLX5DV_UAR_ALLOC_TYPE_BF', and
   to 0 if you don't. */
#define HAVE_DECL_MLX5DV_UAR_ALLOC_TYPE_BF 1

/* Define to 1 if you have the declaration of `MLX5DV_UAR_ALLOC_TYPE_NC', and
   to 0 if you don't. */
#define HAVE_DECL_MLX5DV_UAR_ALLOC_TYPE_NC 1

/* Define to 1 if you have the declaration of `MLX5_WQE_CTRL_SOLICITED', and
   to 0 if you don't. */
/* #undef HAVE_DECL_MLX5_WQE_CTRL_SOLICITED */

/* Define to 1 if you have the declaration of `POSIX_MADV_DONTNEED', and to 0
   if you don't. */
#define HAVE_DECL_POSIX_MADV_DONTNEED 1

/* Define to 1 if you have the declaration of `PR_SET_PTRACER', and to 0 if
   you don't. */
#define HAVE_DECL_PR_SET_PTRACER 1

/* Define to 1 if you have the declaration of `rdma_establish', and to 0 if
   you don't. */
#define HAVE_DECL_RDMA_ESTABLISH 1

/* Define to 1 if you have the declaration of `rdma_init_qp_attr', and to 0 if
   you don't. */
#define HAVE_DECL_RDMA_INIT_QP_ATTR 1

/* Define to 1 if you have the declaration of `SOL_SOCKET', and to 0 if you
   don't. */
#define HAVE_DECL_SOL_SOCKET 1

/* Define to 1 if you have the declaration of `SO_KEEPALIVE', and to 0 if you
   don't. */
#define HAVE_DECL_SO_KEEPALIVE 1

/* Define to 1 if you have the declaration of `SPEED_UNKNOWN', and to 0 if you
   don't. */
#define HAVE_DECL_SPEED_UNKNOWN 1

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of `SYS_brk', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_BRK 1

/* Define to 1 if you have the declaration of `SYS_ipc', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_IPC 0

/* Define to 1 if you have the declaration of `SYS_madvise', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_MADVISE 1

/* Define to 1 if you have the declaration of `SYS_mmap', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_MMAP 1

/* Define to 1 if you have the declaration of `SYS_mremap', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_MREMAP 1

/* Define to 1 if you have the declaration of `SYS_munmap', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_MUNMAP 1

/* Define to 1 if you have the declaration of `SYS_shmat', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_SHMAT 1

/* Define to 1 if you have the declaration of `SYS_shmdt', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_SHMDT 1

/* Define to 1 if you have the declaration of `TCP_KEEPCNT', and to 0 if you
   don't. */
#define HAVE_DECL_TCP_KEEPCNT 1

/* Define to 1 if you have the declaration of `TCP_KEEPIDLE', and to 0 if you
   don't. */
#define HAVE_DECL_TCP_KEEPIDLE 1

/* Define to 1 if you have the declaration of `TCP_KEEPINTVL', and to 0 if you
   don't. */
#define HAVE_DECL_TCP_KEEPINTVL 1

/* Define to 1 if you have the declaration of `__ppc_get_timebase_freq', and
   to 0 if you don't. */
#define HAVE_DECL___PPC_GET_TIMEBASE_FREQ 0

/* Enable detailed backtrace */
/* #undef HAVE_DETAILED_BACKTRACE */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* high-resolution hardware timer enabled */
#define HAVE_HW_TIMER 1

/* IB support */
#define HAVE_IB 1

/* Device Memory support */
#define HAVE_IBV_DM 1

/* Device Memory support (EXP) */
/* #undef HAVE_IBV_EXP_DM */

/* IB QP Create UMR support */
/* #undef HAVE_IBV_EXP_QP_CREATE_UMR */

/* Support UMR max caps v2 */
/* #undef HAVE_IBV_EXP_QP_CREATE_UMR_CAPS */

/* IB resource domain */
/* #undef HAVE_IBV_EXP_RES_DOMAIN */

/* IB extended atomics support */
/* #undef HAVE_IB_EXT_ATOMICS */

/* struct in6_addr has s6_addr32 member */
#define HAVE_IN6_ADDR_S6_ADDR32 1

/* struct in6_addr is BSD-style */
/* #undef HAVE_IN6_ADDR_U6_ADDR32 */

/* Define to 1 if you have the <infiniband/mlx5dv.h> header file. */
#define HAVE_INFINIBAND_MLX5DV_H 1

/* Define to 1 if you have the <infiniband/mlx5_hw.h> header file. */
/* #undef HAVE_INFINIBAND_MLX5_HW_H */

/* Define to 1 if you have the <infiniband/tm_types.h> header file. */
#define HAVE_INFINIBAND_TM_TYPES_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* struct iphdr has daddr member */
/* #undef HAVE_IPHDR_DADDR */

/* struct ip has ip_dst member */
#define HAVE_IP_IP_DST 1

/* Define to 1 if you have the <jni.h> header file. */
/* #undef HAVE_JNI_H */

/* Define to 1 if you have the <jni_md.h> header file. */
/* #undef HAVE_JNI_MD_H */

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define to 1 if you have the `rt' library (-lrt). */
#define HAVE_LIBRT 1

/* Define to 1 if you have the <linux/futex.h> header file. */
#define HAVE_LINUX_FUTEX_H 1

/* Define to 1 if you have the <linux/ip.h> header file. */
#define HAVE_LINUX_IP_H 1

/* Define to 1 if you have the <linux/mman.h> header file. */
#define HAVE_LINUX_MMAN_H 1

/* Define to 1 if you have the `malloc_get_state' function. */
/* #undef HAVE_MALLOC_GET_STATE */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* malloc hooks support */
/* #undef HAVE_MALLOC_HOOK */

/* Define to 1 if you have the <malloc_np.h> header file. */
/* #undef HAVE_MALLOC_NP_H */

/* Define to 1 if you have the `malloc_set_state' function. */
/* #undef HAVE_MALLOC_SET_STATE */

/* Define to 1 if you have the `malloc_trim' function. */
#define HAVE_MALLOC_TRIM 1

/* have masked atomic endianness */
/* #undef HAVE_MASKED_ATOMICS_ENDIANNESS */

/* Define to 1 if you have the `memalign' function. */
#define HAVE_MEMALIGN 1

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* mlx5 bare-metal support */
#define HAVE_MLX5_HW 1

/* mlx5 UD bare-metal support */
#define HAVE_MLX5_HW_UD 1

/* MPI support */
/* #undef HAVE_MPI */

/* Define to 1 if you have the `mremap' function. */
#define HAVE_MREMAP 1

/* Define to 1 if you have the <netinet/ip.h> header file. */
#define HAVE_NETINET_IP_H 1

/* Define to 1 if you have the <net/ethernet.h> header file. */
#define HAVE_NET_ETHERNET_H 1

/* Define to 1 to enable NUMA support */
#define HAVE_NUMA 1

/* Define to 1 if you have the <numaif.h> header file. */
#define HAVE_NUMAIF_H 1

/* Define to 1 if you have the <numa.h> header file. */
#define HAVE_NUMA_H 1

/* ODP support */
#define HAVE_ODP 1

/* Implicit ODP support */
#define HAVE_ODP_IMPLICIT 1

/* Define to 1 if you have the `posix_memalign' function. */
#define HAVE_POSIX_MEMALIGN 1

/* Prefetch support */
#define HAVE_PREFETCH 1

/* Enable profiling */
/* #undef HAVE_PROFILING */

/* Define to 1 if you have the <pthread_np.h> header file. */
/* #undef HAVE_PTHREAD_NP_H */

/* RDMACM QP less support */
#define HAVE_RDMACM_QP_LESS 1

/* Enable ROCM support */
/* #undef HAVE_ROCM */

/* RTE support */
/* #undef HAVE_RTE */

/* Define to 1 if you have the `sched_getaffinity' function. */
#define HAVE_SCHED_GETAFFINITY 1

/* Define to 1 if you have the `sched_setaffinity' function. */
#define HAVE_SCHED_SETAFFINITY 1

/* struct sigaction has sa_restorer member */
#define HAVE_SIGACTION_SA_RESTORER 1

/* struct sigevent has sigev_notify_thread_id */
/* #undef HAVE_SIGEVENT_SIGEV_NOTIFY_THREAD_ID */

/* struct sigevent has _sigev_un._tid */
#define HAVE_SIGEVENT_SIGEV_UN_TID 1

/* Define to 1 if the system has the type `sighandler_t'. */
#define HAVE_SIGHANDLER_T 1

/* Define to 1 if you have the <stars_interface.h> header file. */
#define HAVE_STARS_INTERFACE_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define if you have `strerror_r'. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if the system has the type `struct bitmask'. */
#define HAVE_STRUCT_BITMASK 1

/* Define to 1 if the system has the type `struct dl_phdr_info'. */
#define HAVE_STRUCT_DL_PHDR_INFO 1

/* Hi1636 hns don't exist element.dct in ibv_async_event */
/* Define to 1 if `element.dct' is a member of `struct ibv_async_event'. */
/* #undef HAVE_STRUCT_IBV_ASYNC_EVENT_ELEMENT_DCT */

/* Define to 1 if `pci_atomic_caps' is a member of `struct
   ibv_device_attr_ex'. */
#define HAVE_STRUCT_IBV_DEVICE_ATTR_EX_PCI_ATOMIC_CAPS 1

/* Define to 1 if `flags' is a member of `struct ibv_tm_caps'. */
#define HAVE_STRUCT_IBV_TM_CAPS_FLAGS 1

/* Define to 1 if `cq_uar' is a member of `struct mlx5dv_cq'. */
#define HAVE_STRUCT_MLX5DV_CQ_CQ_UAR 1

/* Define to 1 if you have the <sys/epoll.h> header file. */
#define HAVE_SYS_EPOLL_H 1

/* Define to 1 if you have the <sys/eventfd.h> header file. */
#define HAVE_SYS_EVENTFD_H 1

/* Define to 1 if you have the <sys/event.h> header file. */
/* #undef HAVE_SYS_EVENT_H */

/* Define to 1 if you have the <sys/platform/ppc.h> header file. */
/* #undef HAVE_SYS_PLATFORM_PPC_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/thr.h> header file. */
/* #undef HAVE_SYS_THR_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* DC transport support */
#define HAVE_TL_DC 1

/* RC transport support */
#define HAVE_TL_RC 1

/* UD transport support */
#define HAVE_TL_UD 1

/* Defined if UGNI transport exists */
/* #undef HAVE_TL_UGNI */

/* UB component register */
#define HAVE_UB_REG 1

/* Use ptmalloc-2.8.6 version */
#define HAVE_UCM_PTMALLOC286 1

/* Define to 1 if you have the <umdk/urma_ex_api.h> header file. */
/* #undef HAVE_UMDK_URMA_EX_API_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* URMA SUPPORT IB */
/* #undef HAVE_URMA_IB */

/* IB experimental verbs */
/* #undef HAVE_VERBS_EXP_H */

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `__aarch64_sync_cache_range' function. */
#define HAVE___AARCH64_SYNC_CACHE_RANGE 1

/* Define to 1 if you have the `__clear_cache' function. */
#define HAVE___CLEAR_CACHE 1

/* Define to 1 if you have the `__curbrk' function. */
#define HAVE___CURBRK 1

/* Define to 1 if the system has the type `__sighandler_t'. */
#define HAVE___SIGHANDLER_T 1

/* IB Tag Matching support */
#define IBV_HW_TM 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to 1 to disable Valgrind annotations. */
#define NVALGRIND 1

/* Name of package */
#define PACKAGE "ucx"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "ucx"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "ucx 1.10"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "ucx"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.10"

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

/* Enable BISTRO hooks */
#define UCM_BISTRO_HOOKS 1

/* Enable TCP keepalive configuration */
#define SCT_TCP_EP_KEEPALIVE 1

/* Enable packet header inspection/rewriting in UCT/UD */
#define SCT_UD_EP_DEBUG_HOOKS 0

/* Set aligment assumption for compiler */
/* #undef UCX_ALLOC_ALIGN */

/* UCX module sub-directory */
#define UCX_MODULE_SUBDIR "ucx"

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif


/* Version number of package */
#define VERSION "1.10"

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported only directly.  */
#define restrict __restrict__
/* Work around a bug in older versions of Sun C++, which did not
   #define __restrict__ or support _Restrict or __restrict__
   even though the corresponding Sun C compiler ended up with
   "#define restrict _Restrict" or "#define restrict __restrict__"
   in the previous line.  This workaround can be removed once
   we assume Oracle Developer Studio 12.5 (2016) or later.  */
#if defined __SUNPRO_CC && !defined __RESTRICT && !defined __restrict__
# define _Restrict
# define __restrict__
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Test loadable modules */
#define test_MODULES ":module"

/* UCM loadable modules */
#define ucm_MODULES ""

/* UCT loadable modules */
#define uct_MODULES ":ib:rdmacm:cma:sdma:sdma_offload"

/* CUDA loadable modules */
#define uct_cuda_MODULES ""

/* IB loadable modules */
#define uct_ib_MODULES ""

/* ROCM loadable modules */
#define uct_rocm_MODULES ""

/* UB loadable modules */
#define uct_ub_MODULES ""

/* Perftest loadable modules */
#define ucx_perftest_MODULES ""


#endif /* UCX_CONFIG_H */
