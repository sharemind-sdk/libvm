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
#include <sharemind/extern_c.h>
#include <sharemind/map.h>
#include "memoryslot.h"


SHAREMIND_MAP_DECLARE_BODY(SharemindMemoryMap, uint64_t, SharemindMemorySlot)
SHAREMIND_MAP_DECLARE_init(SharemindMemoryMap, inline, visibility("internal"))
SHAREMIND_MAP_DEFINE_init(SharemindMemoryMap, inline)
SHAREMIND_MAP_DECLARE_destroy(SharemindMemoryMap,
                              inline,,
                              visibility("internal"))
SHAREMIND_MAP_DEFINE_destroy(SharemindMemoryMap,
                             inline,,
                             free,
                             SharemindMemorySlot_destroy(&v->value);)
SHAREMIND_MAP_DECLARE_get(SharemindMemoryMap,
                          inline,
                          const uint64_t,
                          visibility("internal"))
SHAREMIND_MAP_DEFINE_get(SharemindMemoryMap,
                         inline,
                         const uint64_t,
                         ((uint16_t) (key)),
                         SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                         SHAREMIND_MAP_KEY_LESS_uint64_t)
SHAREMIND_MAP_DECLARE_insertHint(SharemindMemoryMap,
                                 inline,
                                 const uint64_t,
                                 visibility("internal"))
SHAREMIND_MAP_DEFINE_insertHint(SharemindMemoryMap,
                                inline,
                                const uint64_t,
                                ((uint16_t) (key)),
                                SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                                SHAREMIND_MAP_KEY_LESS_uint64_t)
SHAREMIND_MAP_DECLARE_emplaceAtHint(SharemindMemoryMap,
                                    inline,
                                    visibility("internal"))
SHAREMIND_MAP_DEFINE_emplaceAtHint(SharemindMemoryMap, inline)
SHAREMIND_MAP_DECLARE_insertAtHint(SharemindMemoryMap,
                                   inline,
                                   const uint64_t,
                                   visibility("internal"))
SHAREMIND_MAP_DEFINE_insertAtHint(SharemindMemoryMap,
                                  inline,
                                  const uint64_t,
                                  SHAREMIND_MAP_KEY_COPY_uint64_t,
                                  malloc,
                                  free)
SHAREMIND_MAP_DECLARE_insertNew(SharemindMemoryMap,
                                inline,
                                const uint64_t,
                                visibility("internal"))
SHAREMIND_MAP_DEFINE_insertNew(SharemindMemoryMap,
                               inline,
                               const uint64_t)
SHAREMIND_MAP_DECLARE_remove(SharemindMemoryMap,
                             inline,
                             const uint64_t,
                             visibility("internal"))
SHAREMIND_MAP_DEFINE_remove(SharemindMemoryMap,
                            inline,
                            const uint64_t,
                            ((uint16_t) (key)),
                            SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                            SHAREMIND_MAP_KEY_LESS_uint64_t,
                            free,
                            SharemindMemorySlot_destroy(&v->value);)

SHAREMIND_EXTERN_C_BEGIN

inline uint64_t SharemindMemoryMap_findUnusedPtr(
            const SharemindMemoryMap * const m,
            const uint64_t startFrom)
        __attribute__ ((nonnull(1),
                        warn_unused_result,
                        visibility("internal")));
inline uint64_t SharemindMemoryMap_findUnusedPtr(
            const SharemindMemoryMap * const m,
            const uint64_t startFrom)
{
    assert(m);
    assert(startFrom >= 4u);
    assert(m->size < UINT64_MAX);
    assert(m->size < SIZE_MAX);
    uint64_t index = startFrom;
    for (;;) {
        /* Check if slot is free: */
        if (likely(!SharemindMemoryMap_get(m, index)))
            break;
        /* Increment index, skip "NULL" and static memory pointers: */
        if (unlikely(!++index))
            index = 4u;
        /* This shouldn't trigger because (m->size < UINT64_MAX) */
        assert(index != startFrom);
    }
    assert(index != 0u);
    return index;
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_MEMORYMAP_H */
