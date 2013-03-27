/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_MEMORYMAP_H
#define SHAREMIND_LIBVM_MEMORYMAP_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/map.h>
#include "memoryslot.h"


#ifdef __cplusplus
extern "C" {
#endif


SHAREMIND_MAP_DECLARE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,static inline)
SHAREMIND_MAP_DEFINE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,(uint16_t)(key),SHAREMIND_MAP_KEY_EQUALS_uint64_t,SHAREMIND_MAP_KEY_LESS_THAN_uint64_t,SHAREMIND_MAP_KEYINIT_REGULAR,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,static inline)

static inline uint64_t SharemindMemoryMap_find_unused_ptr(const SharemindMemoryMap * m, uint64_t startFrom);
static inline uint64_t SharemindMemoryMap_find_unused_ptr(const SharemindMemoryMap * m, uint64_t startFrom) {
    assert(m);
    assert(startFrom != 0u);
    assert(m->size < UINT64_MAX);
    assert(m->size < SIZE_MAX);
    uint64_t index = startFrom;
    for (;;) {
        /* Check if slot is free: */
        if (likely(!SharemindMemoryMap_get_const(m, index)))
            break;
        /* Increment index, skip "NULL" and static memory pointers: */
        if (unlikely(!++index))
            index += 4u;
        assert(index != 0u);
        /* This shouldn't trigger because (m->size < UINT64_MAX) */
        assert(index != startFrom);
    }
    assert(index != 0u);
    return index;
}

static inline void SharemindMemoryMap_destroyer(const uint64_t * key,
                                                SharemindMemorySlot * value)
{
    (void) key;
    SharemindMemorySlot_destroy(value);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_MEMORYMAP_H */
