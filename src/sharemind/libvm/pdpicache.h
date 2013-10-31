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

#include <sharemind/libmodapi/api_0x1.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/vector.h>
#include <stddef.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    SharemindPdpi * pdpi;
    SharemindModuleApi0x1PdpiInfo info;
} SharemindPdpiCacheItem;

static inline bool SharemindPdpiCacheItem_init(SharemindPdpiCacheItem * ci,
                                               SharemindPd * pd)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline bool SharemindPdpiCacheItem_init(SharemindPdpiCacheItem * ci,
                                               SharemindPd * pd)
{
    assert(ci);
    assert(pd);

    ci->pdpi = SharemindPdpi_new(pd);
    if (!ci->pdpi)
        return false;

    ci->info.pdpiHandle = NULL;
    ci->info.pdHandle = SharemindPd_get_handle(pd);
    SharemindPdk * pdk = SharemindPd_get_pdk(pd);
    assert(pdk);
    ci->info.pdkIndex = SharemindPdk_get_index(pdk);
    SharemindModule * module = SharemindPdk_get_module(pdk);
    assert(module);
    ci->info.moduleHandle = SharemindModule_get_handle(module);
    return true;
}

static inline bool SharemindPdpiCacheItem_start(SharemindPdpiCacheItem * ci)
        __attribute__ ((nonnull(1), warn_unused_result));
static inline bool SharemindPdpiCacheItem_start(SharemindPdpiCacheItem * ci) {
    assert(!ci->info.pdpiHandle);
    bool r = SharemindPdpi_start(ci->pdpi);
    if (r)
        ci->info.pdpiHandle = SharemindPdpi_get_handle(ci->pdpi);
    return r;
}

static inline void SharemindPdpiCacheItem_stop(SharemindPdpiCacheItem * ci)
        __attribute__ ((nonnull(1)));
static inline void SharemindPdpiCacheItem_stop(SharemindPdpiCacheItem * ci) {
    assert(ci->info.pdpiHandle);
    SharemindPdpi_stop(ci->pdpi);
    ci->info.pdpiHandle = NULL;
}

static inline void SharemindPdpiCacheItem_destroy(SharemindPdpiCacheItem * ci) __attribute__ ((nonnull(1)));
static inline void SharemindPdpiCacheItem_destroy(SharemindPdpiCacheItem * ci) {
    assert(ci);
    assert(ci->pdpi);
    SharemindPdpi_free(ci->pdpi);
}

SHAREMIND_VECTOR_DECLARE(SharemindPdpiCache,SharemindPdpiCacheItem,,static inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdpiCache,SharemindPdpiCacheItem,malloc,free,realloc,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_PDPICACHE_H */
