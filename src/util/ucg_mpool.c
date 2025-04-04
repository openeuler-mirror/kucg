/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */
#include "ucg_mpool.h"
#include "ucg_helper.h"
#include "ucg_malloc.h"

static ucg_mpool_ops_t ucg_default_mpool_ops = {
    .chunk_alloc = ucg_mpool_hugetlb_malloc,
    .chunk_release = ucg_mpool_hugetlb_free,
    .obj_init = NULL,
    .obj_cleanup = NULL
};

/**
 * @brief The wrapper functions registered to UCS_MPOOL, they will call ucg's
 *        mpool functions.
 */
static ucs_status_t ucg_mpool_chunk_alloc_wrapper(ucs_mpool_t *ucs_mp,
                                                  size_t *psize, void **pchunk)
{
    ucg_mpool_t *ucg_mp = ucg_derived_of(ucs_mp, ucg_mpool_t);
    ucg_status_t status = ucg_mp->ops->chunk_alloc(ucg_mp, psize, pchunk);

    return ucg_status_g2s(status);
}

static void ucg_mpool_chunk_release_wrapper(ucs_mpool_t *ucs_mp, void *chunk)
{
    ucg_mpool_t *ucg_mp = ucg_derived_of(ucs_mp, ucg_mpool_t);
    ucg_mp->ops->chunk_release(ucg_mp, chunk);
}

static void ucg_mpool_obj_init_wrapper(ucs_mpool_t *ucs_mp, void *obj, void *chunk)
{
    ucg_mpool_t *ucg_mp = ucg_derived_of(ucs_mp, ucg_mpool_t);
    ucg_mp->ops->obj_init(ucg_mp, obj, chunk);
}

static void ucg_mpool_obj_cleanup_wrapper(ucs_mpool_t *ucs_mp, void *obj)
{
    ucg_mpool_t *ucg_mp = ucg_derived_of(ucs_mp, ucg_mpool_t);
    ucg_mp->ops->obj_cleanup(ucg_mp, obj);
}

static ucs_mpool_ops_t ucs_ops = {
    .chunk_alloc        = ucg_mpool_chunk_alloc_wrapper,
    .chunk_release      = ucg_mpool_chunk_release_wrapper,
    .obj_init           = ucg_mpool_obj_init_wrapper,
    .obj_cleanup        = ucg_mpool_obj_cleanup_wrapper,
    .obj_str            = NULL
};

ucg_status_t ucg_mpool_init(ucg_mpool_t *mp, size_t priv_size,
                            size_t elem_size, size_t align_offset, size_t alignment,
                            unsigned elems_per_chunk, unsigned max_elems,
                            ucg_mpool_ops_t *ops, const char *name)
{
    ucg_status_t status;
    ucs_mpool_params_t mp_params;

    if (mp == NULL || name == NULL) {
        return UCG_ERR_INVALID_PARAM;
    }

    mp->ops = (ops == NULL) ? &ucg_default_mpool_ops : ops;
    if (mp->ops->obj_init == NULL) {
        ucs_ops.obj_init = NULL;
    }
    if (mp->ops->obj_cleanup == NULL) {
        ucs_ops.obj_cleanup = NULL;
    }

    ucs_mpool_params_reset(&mp_params);
    mp_params.priv_size         = priv_size;
    mp_params.elem_size         = elem_size;
    mp_params.align_offset      = align_offset;
    mp_params.alignment         = alignment;
    mp_params.elems_per_chunk   = elems_per_chunk;
    mp_params.max_elems         = max_elems;
    mp_params.ops               = &ucs_ops;
    mp_params.name              = name;

    status = ucg_status_s2g(ucs_mpool_init(&mp_params, &mp->super));
    if (status != UCG_OK) {
        return status;
    }
    status = ucg_lock_init(&mp->lock, UCG_LOCK_TYPE_NONE);
    return status;
}

ucg_status_t ucg_mpool_init_mt(ucg_mpool_t *mp, size_t priv_size,
                               size_t elem_size, size_t align_offset, size_t alignment,
                               unsigned elems_per_chunk, unsigned max_elems,
                               ucg_mpool_ops_t *ops, const char *name)
{
    ucg_status_t status;
    status = ucg_mpool_init(mp, priv_size, elem_size, align_offset, alignment,
                            elems_per_chunk, max_elems, ops, name);
    if (status != UCG_OK) {
        return status;
    }
    ucg_lock_destroy(&mp->lock);
    status = ucg_lock_init(&mp->lock, UCG_LOCK_TYPE_SPINLOCK);
    return status;
}

void ucg_mpool_cleanup(ucg_mpool_t *mp, int check_leak)
{
    if (mp == NULL) {
        return;
    }

    ucs_mpool_cleanup(&mp->super, check_leak);
    ucg_lock_destroy(&mp->lock);
    return;
}

void *ucg_mpool_get(ucg_mpool_t *mp)
{
    if (mp == NULL) {
        return NULL;
    }
    void *obj = NULL;
    ucg_lock_enter(&mp->lock);
    obj = ucs_mpool_get(&mp->super);
    ucg_lock_leave(&mp->lock);
    return obj;
}

void ucg_mpool_put(void *obj)
{
    if (obj == NULL) {
        return;
    }
    /* depends on the implementation of ucs mpool. */
    ucs_mpool_elem_t *elem = (ucs_mpool_elem_t*)obj - 1;
    ucg_mpool_t *mp = ucg_derived_of(elem->mpool, ucg_mpool_t);
    ucg_lock_enter(&mp->lock);
    ucs_mpool_put(obj);
    ucg_lock_leave(&mp->lock);
    return;
}

ucg_status_t ucg_mpool_hugetlb_malloc(ucg_mpool_t *mp, size_t *psize, void **pchunk)
{
    ucs_status_t ucs_status = ucs_mpool_hugetlb_malloc(&mp->super, psize, pchunk);
    return ucg_status_s2g(ucs_status);
}

void ucg_mpool_hugetlb_free(ucg_mpool_t *mp, void *chunk)
{
    ucs_mpool_hugetlb_free(&mp->super, chunk);
}