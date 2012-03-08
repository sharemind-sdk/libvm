/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PDPICACHE_H
#define SHAREMIND_LIBVM_PDPICACHE_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/vector.h>
#include <stddef.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    SharemindPdpi * pdpi;
    void * pdpiHandle;
    size_t pdkIndex;
    void * moduleHandle;
} SharemindPdpiCacheItem;

SHAREMIND_VECTOR_DECLARE(SharemindPdpiCache,SharemindPdpiCacheItem,,static inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdpiCache,SharemindPdpiCacheItem,malloc,free,realloc,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_PDPICACHE_H */
