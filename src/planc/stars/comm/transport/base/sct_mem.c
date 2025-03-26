/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "sct_iface.h"
#include "sct_md.h"


typedef struct {
    uct_alloc_method_t method;
    size_t             length;
    sct_mem_h          memh;
} sct_iface_mp_chunk_hdr_t;


typedef struct {
    sct_base_iface_t               *iface;
    sct_iface_mpool_init_obj_cb_t  init_obj_cb;
} sct_iface_mp_priv_t;


const char *sct_alloc_method_names[] = {
    [UCT_ALLOC_METHOD_THP]  = "thp",
    [UCT_ALLOC_METHOD_MD]   = "md",
    [UCT_ALLOC_METHOD_HEAP] = "heap",
    [UCT_ALLOC_METHOD_MMAP] = "mmap",
    [UCT_ALLOC_METHOD_HUGE] = "huge",
    [UCT_ALLOC_METHOD_LAST] = NULL
};


static inline int sct_mem_get_mmap_flags(unsigned uct_mmap_flags)
{
    int mm_flags = 0;

#ifdef MAP_NONBLOCK
    if (uct_mmap_flags & UCT_MD_MEM_FLAG_NONBLOCK) {
        mm_flags |= MAP_NONBLOCK;
    }
#endif

    if (uct_mmap_flags & UCT_MD_MEM_FLAG_FIXED) {
        mm_flags |= MAP_FIXED;
    }

    return mm_flags;
}

static inline void *sct_mem_alloc_params_get_address(const sct_mem_alloc_params_t *params)
{
    return (params->field_mask & UCT_MEM_ALLOC_PARAM_FIELD_ADDRESS) ?
            params->address : NULL;
}

ucs_status_t sct_mem_alloc(size_t length, const uct_alloc_method_t *methods,
                           unsigned num_methods,
                           const sct_mem_alloc_params_t *params,
                           sct_allocated_memory_t *mem)
{
    const char *alloc_name;
    const uct_alloc_method_t *method;
    ucs_memory_type_t mem_type;
    uct_md_attr_t md_attr;
    ucs_status_t status;
    unsigned flags;
    size_t alloc_length;
    unsigned md_index;
    sct_mem_h memh;
    sct_md_h md;
    void *address;
    int ret;

    status = sct_mem_alloc_check_params(length, methods, num_methods, params);
    if (status != UCS_OK) {
        return status;
    }

    /* set defaults in case some param fields are not set */
    address      = sct_mem_alloc_params_get_address(params);
    flags        = (params->field_mask & UCT_MEM_ALLOC_PARAM_FIELD_FLAGS) ?
                   params->flags : (UCT_MD_MEM_ACCESS_LOCAL_READ |
                                    UCT_MD_MEM_ACCESS_LOCAL_WRITE);
    alloc_name   = (params->field_mask & UCT_MEM_ALLOC_PARAM_FIELD_NAME) ?
                   params->name : "anonymous-sct_mem_alloc";
    mem_type     = (params->field_mask & UCT_MEM_ALLOC_PARAM_FIELD_MEM_TYPE) ?
                   params->mem_type : UCS_MEMORY_TYPE_HOST;
    alloc_length = length;

    for (method = methods; method < methods + num_methods; ++method) {
        ucs_trace("trying allocation method %s", sct_alloc_method_names[*method]);

        switch (*method) {
            case UCT_ALLOC_METHOD_MD:
                /* Allocate with one of the specified memory domains */
                for (md_index = 0; md_index < params->mds.count; ++md_index) {
                    alloc_length = length;
                    address      = sct_mem_alloc_params_get_address(params);
                    md           = params->mds.mds[md_index];
                    status = sct_md_query(md, &md_attr);
                    if (status != UCS_OK) {
                        ucg_error("Failed to query MD");
                        return status;
                    }

                    /* Check if MD supports allocation */
                    if (!(md_attr.cap.flags & UCT_MD_FLAG_ALLOC)) {
                        continue;
                    }

                    /* Check if MD supports allocation with fixed address
                    * if it's requested */
                    if ((flags & UCT_MD_MEM_FLAG_FIXED) &&
                        !(md_attr.cap.flags & UCT_MD_FLAG_FIXED)) {
                        continue;
                    }

                    /* Check if MD supports allocation on requested mem_type */
                    if (!(md_attr.cap.alloc_mem_types & UCS_BIT(mem_type))) {
                        continue;
                    }

                    /* Allocate memory using the MD.
                    * If the allocation fails, it's considered an error and we don't
                    * fall-back, because this MD already exposed support for memory
                    * allocation.
                    */
                    status = sct_md_mem_alloc(md, &alloc_length, &address,
                                            mem_type, flags, alloc_name,
                                            &memh);
                    if (status != UCS_OK) {
                        ucg_error("failed to allocate %zu bytes using md %s for %s: %s",
                                  alloc_length, md->component->name,
                                  alloc_name, ucs_status_string(status));
                        return status;
                    }

                    ucg_assert(memh != UCT_MEM_HANDLE_NULL);
                    mem->md       = md;
                    mem->mem_type = mem_type;
                    mem->memh     = memh;
                    goto allocated;
                }

                if (mem_type != UCS_MEMORY_TYPE_HOST) {
                    /* assumes that only MDs are capable of allocating non-host
                    * memory
                    */
                    ucg_error("unable to allocated requested memory type");
                    return UCS_ERR_UNSUPPORTED;
                }

                break;

            case UCT_ALLOC_METHOD_THP:
                if (mem_type != UCS_MEMORY_TYPE_HOST) {
                    break;
                }
                break;

            case UCT_ALLOC_METHOD_HEAP:
                if (mem_type != UCS_MEMORY_TYPE_HOST) {
                    break;
                }

                /* Allocate aligned memory using libc allocator */

                /* Fixed option is not supported for heap allocation */
                if (flags & UCT_MD_MEM_FLAG_FIXED) {
                    break;
                }

                address = sct_mem_alloc_params_get_address(params);
                ret = posix_memalign(&address, ARCH_CACHE_LINE_SIZE, length);
                if (ret == 0) {
                    goto allocated_without_md;
                }

                ucs_trace("failed to allocate %zu bytes from the heap", alloc_length);
                break;

            case UCT_ALLOC_METHOD_MMAP:
                if (mem_type != UCS_MEMORY_TYPE_HOST) {
                    break;
                }

                /* Request memory from operating system using mmap() */

                alloc_length = length;
                address      = sct_mem_alloc_params_get_address(params);
                status = ucs_mmap_alloc(&alloc_length, &address,
                                        sct_mem_get_mmap_flags(flags));
                if (status== UCS_OK) {
                    goto allocated_without_md;
                }

                ucs_trace("failed to mmap %zu bytes: %s", length,
                          ucs_status_string(status));
                break;

            case UCT_ALLOC_METHOD_HUGE:
                if (mem_type != UCS_MEMORY_TYPE_HOST) {
                    break;
                }
                status = UCS_ERR_NO_MEMORY;
                ucs_trace("failed to allocate %zu bytes from hugetlb: %s",
                          length, ucs_status_string(status));
                break;

            default:
                ucg_error("Invalid allocation method %d", *method);
                return UCS_ERR_INVALID_PARAM;
        }
    }

    ucg_debug("Could not allocate memory with any of the provided methods");
    return UCS_ERR_NO_MEMORY;

allocated_without_md:
    mem->md       = NULL;
    mem->mem_type = UCS_MEMORY_TYPE_HOST;
    mem->memh     = UCT_MEM_HANDLE_NULL;
allocated:
    ucs_trace("allocated %zu bytes at %p using %s", alloc_length, address,
              (mem->md == NULL) ? sct_alloc_method_names[*method]
                                : mem->md->component->name);
    mem->address = address;
    mem->length  = alloc_length;
    mem->method  = *method;
    return UCS_OK;
}

ucs_status_t sct_mem_free(const sct_allocated_memory_t *mem)
{
    switch (mem->method) {
        case UCT_ALLOC_METHOD_MD:
            return sct_md_mem_free(mem->md, mem->memh);

        case UCT_ALLOC_METHOD_THP:
        case UCT_ALLOC_METHOD_HEAP:
            ucg_free(mem->address);
            return UCS_OK;

        case UCT_ALLOC_METHOD_MMAP:
            return ucs_mmap_free(mem->address, mem->length);

        case UCT_ALLOC_METHOD_HUGE:
            return ucs_sysv_free(mem->address);

        default:
            ucg_warn("Invalid memory allocation method: %d", mem->method);
            return UCS_ERR_INVALID_PARAM;
    }
}

ucs_status_t sct_iface_mem_alloc(sct_iface_h tl_iface, size_t length, unsigned flags,
                                 const char *name, sct_allocated_memory_t *mem)
{
    sct_base_iface_t *iface = ucs_derived_of(tl_iface, sct_base_iface_t);
    void *address           = NULL;
    uct_md_attr_t md_attr;
    ucs_status_t status;
    sct_mem_alloc_params_t params;

    params.field_mask      = UCT_MEM_ALLOC_PARAM_FIELD_FLAGS    |
                             UCT_MEM_ALLOC_PARAM_FIELD_ADDRESS  |
                             UCT_MEM_ALLOC_PARAM_FIELD_MEM_TYPE |
                             UCT_MEM_ALLOC_PARAM_FIELD_MDS      |
                             UCT_MEM_ALLOC_PARAM_FIELD_NAME;
    params.flags           = UCT_MD_MEM_ACCESS_ALL;
    params.name            = name;
    params.mem_type        = UCS_MEMORY_TYPE_HOST;
    params.address         = address;
    params.mds.mds         = &iface->md;
    params.mds.count       = 1;

    status = sct_mem_alloc(length, iface->config.alloc_methods,
                           iface->config.num_alloc_methods, &params, mem);
    if (status != UCS_OK) {
        goto err;
    }

    /* If the memory was not allocated using MD, register it */
    if (mem->method != UCT_ALLOC_METHOD_MD) {
        status = sct_md_query(iface->md, &md_attr);
        if (status != UCS_OK) {
            goto err_free;
        }

        /* If MD does not support registration, allow only the MD method */
        if ((md_attr.cap.flags & UCT_MD_FLAG_REG) &&
            (md_attr.cap.reg_mem_types & UCS_BIT(mem->mem_type))) {
            status = sct_md_mem_reg(iface->md, mem->address, mem->length, flags,
                                    &mem->memh);
            if (status != UCS_OK) {
                goto err_free;
            }

            ucg_assert(mem->memh != UCT_MEM_HANDLE_NULL);
        } else {
            mem->memh = UCT_MEM_HANDLE_NULL;
        }

        mem->md = iface->md;
    }

    return UCS_OK;

err_free:
    sct_mem_free(mem);
err:
    return status;
}

void sct_iface_mem_free(const sct_allocated_memory_t *mem)
{
    if ((mem->method != UCT_ALLOC_METHOD_MD) &&
        (mem->memh != UCT_MEM_HANDLE_NULL)) {
        (void)sct_md_mem_dereg(mem->md, mem->memh);
    }
    sct_mem_free(mem);
}
