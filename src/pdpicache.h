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


#include <assert.h>
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/api_0x1.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/vector.h>
#include <stddef.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

typedef struct {
    SharemindPdpi * pdpi;
    SharemindModuleApi0x1PdpiInfo info;
} SharemindPdpiCacheItem;

static inline bool SharemindPdpiCacheItem_init(
        SharemindPdpiCacheItem * const ci,
        SharemindPd * const pd)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline bool SharemindPdpiCacheItem_init(
        SharemindPdpiCacheItem * const ci,
        SharemindPd * const pd)
{
    assert(ci);
    assert(pd);

    ci->pdpi = SharemindPd_newPdpi(pd);
    if (!ci->pdpi)
        return false;

    ci->info.pdpiHandle = NULL;
    ci->info.pdHandle = SharemindPd_handle(pd);
    SharemindPdk * pdk = SharemindPd_pdk(pd);
    assert(pdk);
    ci->info.pdkIndex = SharemindPdk_index(pdk);
    SharemindModule * module = SharemindPdk_module(pdk);
    assert(module);
    ci->info.moduleHandle = SharemindModule_handle(module);
    return true;
}

static inline bool SharemindPdpiCacheItem_start(
        SharemindPdpiCacheItem * const ci)
        __attribute__ ((nonnull(1), warn_unused_result));
static inline bool SharemindPdpiCacheItem_start(
        SharemindPdpiCacheItem * const ci)

{
    assert(ci);
    assert(!ci->info.pdpiHandle);
    const bool r = (SharemindPdpi_start(ci->pdpi) == SHAREMIND_MODULE_API_OK);
    if (r)
        ci->info.pdpiHandle = SharemindPdpi_handle(ci->pdpi);
    return r;
}

static inline void SharemindPdpiCacheItem_stop(
        SharemindPdpiCacheItem * const ci) __attribute__ ((nonnull(1)));
static inline void SharemindPdpiCacheItem_stop(
        SharemindPdpiCacheItem * const ci)
{
    assert(ci);
    assert(ci->info.pdpiHandle);
    SharemindPdpi_stop(ci->pdpi);
    ci->info.pdpiHandle = NULL;
}

static inline void SharemindPdpiCacheItem_destroy(
        SharemindPdpiCacheItem * const ci) __attribute__ ((nonnull(1)));
static inline void SharemindPdpiCacheItem_destroy(
        SharemindPdpiCacheItem * const ci)
{
    assert(ci);
    assert(ci->pdpi);
    SharemindPdpi_free(ci->pdpi);
}

SHAREMIND_VECTOR_DECLARE(SharemindPdpiCache,
                         SharemindPdpiCacheItem,,
                         static inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdpiCache,
                        SharemindPdpiCacheItem,
                        malloc,
                        free,
                        realloc,
                        static inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PDPICACHE_H */
