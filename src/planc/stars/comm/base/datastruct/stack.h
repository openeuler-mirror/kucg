/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
*/
#ifndef SCS_STACK_H_
#define SCS_STACK_H_

#include "scs.h"

#define __SCS_STACK_TYPE(name, st_val_t) \
    typedef struct scs_stack_##name##_s { \
        st_val_t *data; \
        int top; \
        int capacity; \
    } scs_stack_##name##_t;

#define __SCS_STACK_IMPL(name, SCOPE, st_val_t) \
    SCOPE scs_stack_##name##_t *scs_stack_init_##name(int cap) { \
        scs_stack_##name##_t *st = (scs_stack_##name##_t*)calloc(1, sizeof(scs_stack_##name##_t)); \
        if (!st) { \
            return NULL; \
        } \
        st->data = (st_val_t*)calloc(cap, sizeof(st_val_t)); \
        if (st->data) { \
            st->capacity = cap; \
            return st; \
        } \
        free(st); \
        return NULL; \
    } \
    SCOPE void scs_stack_destroy_##name(scs_stack_##name##_t *st) { \
        if (st) { \
            free(st->data); \
            free(st); \
        } \
    } \
    SCOPE int scs_stack_push_##name(scs_stack_##name##_t *st, st_val_t *val) { \
        if (st->top == st->capacity) { \
            return 1; \
        } \
        memcpy(&st->data[st->top], (void*)val, sizeof(st_val_t)); \
        st->top++; \
        return 0; \
    } \
    SCOPE int scs_stack_empty_##name(scs_stack_##name##_t *st) { \
        return st->top == 0; \
    } \
    SCOPE int scs_stack_pop_##name(scs_stack_##name##_t *st) { \
        if (scs_stack_empty_##name(st)) { \
            return 1; \
        } \
        st->top--; \
        return 0; \
    } \
    SCOPE int scs_stack_top_##name(scs_stack_##name##_t *st, st_val_t* val) { \
        if (scs_stack_empty_##name(st)) { \
            return 1; \
        } \
        memcpy((void*)val, &st->data[st->top - 1], sizeof(st_val_t)); \
        return 0; \
    }
#define SCS_STACK_INIT(name, st_val_t) \
    __SCS_STACK_TYPE(name, st_val_t) \
    __SCS_STACK_IMPL(name, static inline, st_val_t)

#define scs_stack_t(name) scs_stack_##name##_t

#define scs_stack_h(name) scs_stack_##name##_t*

#define scs_stack_init(name, cap) scs_stack_init_##name(cap)

#define scs_stack_destroy(name, st) scs_stack_destroy_##name(st)

#define scs_stack_push(name, st, val) scs_stack_push_##name(st, val)

#define scs_stack_empty(name, st) scs_stack_empty_##name(st)

#define scs_stack_pop(name, st) scs_stack_pop_##name(st)

#define scs_stack_top(name, st, val) scs_stack_top_##name(st, val)

#endif
