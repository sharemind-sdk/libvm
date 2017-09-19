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

#ifndef SHAREMIND_LIBVM_MEMORYMAP_H
#define SHAREMIND_LIBVM_MEMORYMAP_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <cassert>
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
                          uint64_t const ,
                          visibility("internal"))
SHAREMIND_MAP_DEFINE_get(SharemindMemoryMap,
                         inline,
                         uint64_t const,
                         ((uint16_t) (key)),
                         SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                         SHAREMIND_MAP_KEY_LESS_uint64_t)
SHAREMIND_MAP_DECLARE_insertHint(SharemindMemoryMap,
                                 inline,
                                 uint64_t const,
                                 visibility("internal"))
SHAREMIND_MAP_DEFINE_insertHint(SharemindMemoryMap,
                                inline,
                                uint64_t const,
                                ((uint16_t) (key)),
                                SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                                SHAREMIND_MAP_KEY_LESS_uint64_t)
SHAREMIND_MAP_DECLARE_emplaceAtHint(SharemindMemoryMap,
                                    inline,
                                    visibility("internal"))
SHAREMIND_MAP_DEFINE_emplaceAtHint(SharemindMemoryMap, inline)
SHAREMIND_MAP_DECLARE_insertAtHint(SharemindMemoryMap,
                                   inline,
                                   uint64_t const,
                                   visibility("internal"))
SHAREMIND_MAP_DEFINE_insertAtHint(SharemindMemoryMap,
                                  inline,
                                  uint64_t const,
                                  SHAREMIND_MAP_KEY_COPY_uint64_t,
                                  malloc,
                                  free)
SHAREMIND_MAP_DECLARE_insertNew(SharemindMemoryMap,
                                inline,
                                uint64_t const,
                                visibility("internal"))
SHAREMIND_MAP_DEFINE_insertNew(SharemindMemoryMap,
                               inline,
                               uint64_t const)
SHAREMIND_MAP_DECLARE_take(SharemindMemoryMap,
                           inline,
                           uint64_t const,
                           visibility("internal"))
SHAREMIND_MAP_DEFINE_take(SharemindMemoryMap,
                          inline,
                          uint64_t const,
                          ((uint16_t) (key)),
                          SHAREMIND_MAP_KEY_EQUALS_uint64_t,
                          SHAREMIND_MAP_KEY_LESS_uint64_t)
SHAREMIND_MAP_DECLARE_remove(SharemindMemoryMap,
                             inline,
                             uint64_t const,
                             visibility("internal"))
SHAREMIND_MAP_DEFINE_remove(SharemindMemoryMap,
                            inline,
                            uint64_t const,
                            free,
                            SharemindMemorySlot_destroy(&v->value);)

SHAREMIND_EXTERN_C_BEGIN

inline uint64_t SharemindMemoryMap_findUnusedPtr(
            SharemindMemoryMap const * const m,
            uint64_t const startFrom)
        __attribute__ ((nonnull(1),
                        warn_unused_result,
                        visibility("internal")));
inline uint64_t SharemindMemoryMap_findUnusedPtr(
            SharemindMemoryMap const * const m,
            uint64_t const startFrom)
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
