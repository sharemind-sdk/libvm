/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H
#define SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <assert.h>
#include <sharemind/fnv.h>
#include <sharemind/map.h>
#include <stddef.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


static inline void SharemindPrivateMemoryMap_destroyer(void * const * key, size_t * value);
static inline void SharemindPrivateMemoryMap_destroyer(void * const * key, size_t * value) {
    (void) value;
    assert(key);
    assert(*key);
    free(*key);
}

SHAREMIND_MAP_DECLARE(SharemindPrivateMemoryMap,void*,void * const,size_t,static inline)
SHAREMIND_MAP_DEFINE(SharemindPrivateMemoryMap,void*,void * const,size_t,fnv_16a_buf(key,sizeof(void *)),SHAREMIND_MAP_KEY_EQUALS_voidptr,SHAREMIND_MAP_KEY_LESS_THAN_voidptr,SHAREMIND_MAP_KEYINIT_REGULAR,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H */
