/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UCG_SYS_H
#define UCG_SYS_H

#include "scs.h"


#if defined(__linux__) || defined(HAVE_CPU_SET_T)
typedef cpu_set_t ucg_sys_cpuset_t;
#else
#error "Port me"
#endif

BEGIN_C_DECLS

/** @file sys.h */


typedef ino_t ucs_sys_ns_t;


/* namespace type used in @ref ucs_sys_get_ns and @ref ucs_sys_ns_is_default */
typedef enum {
    UCS_SYS_NS_TYPE_IPC,
    UCS_SYS_NS_TYPE_MNT,
    UCS_SYS_NS_TYPE_NET,
    UCS_SYS_NS_TYPE_PID,
    UCS_SYS_NS_TYPE_USER,
    UCS_SYS_NS_TYPE_UTS,
    UCS_SYS_NS_TYPE_LAST
} ucs_sys_namespace_type_t;

/* virtual memory area flags */
typedef enum {
    UCS_SYS_VMA_FLAG_DONTCOPY = UCS_BIT(0),
} ucs_sys_vma_info_flags_t;

/* information about virtual memory area */
typedef struct {
    unsigned long start;
    unsigned long end;
    size_t        page_size;
    uint32_t      flags;
} ucs_sys_vma_info_t;


/**
 * Callback function type used in ucs_sys_iterate_vm.
 */
typedef void (*ucs_sys_vma_cb_t)(ucs_sys_vma_info_t *info, void *ctx);


/**
 * @return Host name.
 */
const char *ucs_get_host_name(void);


/**
 * Get a globally unique identifier of the machine running the current process.
 */
uint64_t ucs_machine_guid();


/**
 * Get the first processor number we are bound to.
 */
int ucs_get_first_cpu(void);


/**
 * Generate a world-wide unique ID
 *
 * @param seed Additional seed to mix in.
 *
 * @note All bits of the returned number have the same randomness.
 */
uint64_t ucs_generate_uuid(uint64_t seed);


/**
 * Read file contents into a string. If the size of the data is smaller than the
 * supplied upper limit (max), a null terminator is appended to the data.
 *
 * @param buffer        Buffer to fill with file contents.
 * @param max           Maximal buffer size.
 * @param filename_fmt  File name printf-like format string.
 *
 * @return Number of bytes read, or -1 in case of error.
 */
ssize_t ucs_read_file(char *buffer, size_t max, int silent,
                      const char *filename_fmt, ...)
    UCS_F_PRINTF(4, 5);


/**
 * Read file contents as a numeric value.
 *
 * @param value         Filled with the number read from the file.
 * @param filename_fmt  File name printf-like format string.
 *
 * @return UCS_OK if successful, or error code otherwise.
 */
ucs_status_t ucg_read_file_number(long *value, int silent,
                                  const char *filename_fmt, ...)
    UCS_F_PRINTF(3, 4);


/**
 * Read file contents into a string closed by null terminator.
 *
 * @param buffer        Buffer to fill with file contents.
 * @param max           Maximal buffer size.
 * @param filename_fmt  File name printf-like format string.
 *
 * @return Number of bytes read, or -1 in case of error.
 */
ssize_t ucs_read_file_str(char *buffer, size_t max, int silent,
                          const char *filename_fmt, ...)
    UCS_F_PRINTF(4, 5);


/**
 * @return Regular page size on the system.
 */
size_t ucs_get_page_size();


/**
 * Get info for memory range.
 *
 * @param address     Memory range start address,
 * @param size        Memory range size.
 * @param cb          Callback function which is called for every vm area.
 * @param ctx         Context argument passed to @a cb call.
 */
void ucs_sys_iterate_vm(void *address, size_t size, ucs_sys_vma_cb_t cb,
                        void *ctx);


/**
 * Get page size of a memory range.
 *
 * @param [in]  address          Memory range start address,
 * @param [in]  size             Memory range size.
 * @param [out] min_page_size_p  Set to the minimal page size in the memory range.
 * @param [out] max_page_size_p  Set to the maximal page size in the memory range.
 */
void ucs_get_mem_page_size(void *address, size_t size, size_t *min_page_size_p,
                           size_t *max_page_size_p);


/**
 * Allocate shared memory using SystemV API.
 *
 * @param size       Pointer to memory size to allocate, updated with actual size
 *                   (rounded up to huge page size or to regular page size).
 * @param max_size   maximal size to allocate. If need to allocate more than this,
 *                   the function fails and returns UCS_ERR_EXCEEDS_LIMIT.
 * @param address_p  Filled with allocated memory address.
 * @param flags      Flags to indicate the permissions for the allocate memory.
 *                   (also, whether or not to allocate memory with huge pages).
 * @param alloc_name Name of memory allocation, for debug/error reporting purposes.
 * @param shmid      Filled with the shmid from the shmget call in the function.
 */
ucs_status_t ucs_sysv_alloc(size_t *size, size_t max_size, void **address_p,
                            int flags, const char *alloc_name, int *shimd);


/**
 * Release memory allocated via SystemV API.
 *
 * @param address   Memory to release (returned from @ref ucs_sysv_alloc).
 */
ucs_status_t ucs_sysv_free(void *address);


/**
 * Allocate private memory using mmap API.
 *
 * @param size      Pointer to memory size to allocate, updated with actual size
 *                  (rounded up to huge page size or to regular page size).
 * @param address_p Filled with allocated memory address.
 * @param flags     Flags to pass to the mmap() system call
 */
ucs_status_t ucs_mmap_alloc(size_t *size, void **address_p,
                            int flags);

/**
 * Release memory allocated via mmap API.
 *
 * @param address   Address of memory to release as returned from @ref ucs_mmap_alloc.
 * @param length    Length of memory to release passed to @ref ucs_mmap_alloc.
 */
ucs_status_t ucs_mmap_free(void *address, size_t length);


/**
 * Modify file descriptor flags via fcntl().
 *
 * @param fd     File descriptor to modify.
 * @param add    Flags to add.
 * @param remove Flags to remove.
 *
 * Note: if a flags is specified in both add and remove, it will be removed.
 */
ucs_status_t ucs_sys_fcntl_modfl(int fd, int add, int remove);


/**
 * Get current thread (LWP) id.
 */
pid_t ucs_get_tid(void);


/**
 * Queries affinity for the current process.
 *
 * @param [out] cpuset      Pointer to the cpuset to return result
 *
 * @return -1 on error with errno set, 0 on success
 */
int ucs_sys_getaffinity(ucg_sys_cpuset_t *cpuset);

/**
 * Copies ucg_sys_cpuset_t to ucs_cpu_set_t.
 *
 * @param [in]  src         Source
 * @param [out] dst         Destination
 */
void ucs_sys_cpuset_copy(ucs_cpu_set_t *dst, const ucg_sys_cpuset_t *src);

/**
 * Get namespace id for resource.
 *
 * @param [in]  name        Namespace to get value
 *
 * @return namespace value or 0 if namespaces are not supported
 */
ucs_sys_ns_t ucs_sys_get_ns(ucs_sys_namespace_type_t name);


/**
 * Check if namespace is namespace of host system.
 *
 * @param [in]  name        Namespace to evaluate
 *
 * @return 1 in case if namespace is root, 0 - in other cases
 */
int ucs_sys_ns_is_default(ucs_sys_namespace_type_t ns);


/**
 * Get 128-bit boot ID value.
 *
 * @param [out]  high       Pointer to high 64 bit of 128 boot ID
 * @param [out]  low        Pointer to low 64 bit of 128 boot ID
 *
 * @return UCS_OK or error in case of failure.
 */
ucs_status_t ucs_sys_get_boot_id(uint64_t *high, uint64_t *low);


END_C_DECLS

#endif
