/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#ifndef SHAREMIND_LIBVM_PDPICACHE_H
#define SHAREMIND_LIBVM_PDPICACHE_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/module-apis/api_0x1.h>
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
    bool const r = (SharemindPdpi_start(ci->pdpi) == SHAREMIND_MODULE_API_OK);
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

inline void SharemindPdpiCacheItem_destroy(SharemindPdpiCacheItem * const ci)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindPdpiCacheItem_destroy(SharemindPdpiCacheItem * const ci) {
    assert(ci);
    assert(ci->pdpi);
    SharemindPdpi_free(ci->pdpi);
}

// static inline
SHAREMIND_VECTOR_DECLARE_BODY(SharemindPdpiCache, SharemindPdpiCacheItem)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindPdpiCache,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindPdpiCache,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindPdpiCache, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindPdpiCache,
                                 inline,
                                 visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindPdpiCache,
                                     inline,,
                                     free,
                                     SharemindPdpiCacheItem_destroy(value);)
SHAREMIND_VECTOR_DECLARE_GET_CONST_POINTER(SharemindPdpiCache,
                                           inline,
                                           visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_CONST_POINTER(SharemindPdpiCache, inline)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindPdpiCache,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindPdpiCache,
                                     inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindPdpiCache,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindPdpiCache, inline)
SHAREMIND_VECTOR_DECLARE_POP(SharemindPdpiCache,
                             inline,
                             visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindPdpiCache, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PDPICACHE_H */
