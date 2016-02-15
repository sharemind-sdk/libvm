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

#ifndef SHAREMIND_LIBVM_MEMORYSLOT_H
#define SHAREMIND_LIBVM_MEMORYSLOT_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/extern_c.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

struct SharemindMemorySlotSpecials_;
typedef struct SharemindMemorySlotSpecials_ SharemindMemorySlotSpecials;

typedef struct {
    void * pData;
    size_t size;
    uint64_t nrefs;
    SharemindMemorySlotSpecials * specials;
} SharemindMemorySlot;

struct SharemindMemorySlotSpecials_ {
    void (*free)(SharemindMemorySlot *);
    bool readable;
    bool writeable;
};

inline void SharemindMemorySlot_init(
        SharemindMemorySlot * const m,
        void * const pData,
        size_t const size,
        SharemindMemorySlotSpecials * const specials)
    __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindMemorySlot_init(
        SharemindMemorySlot * const m,
        void * const pData,
        size_t const size,
        SharemindMemorySlotSpecials * const specials)
{
    assert(m);
    m->pData = pData;
    m->size = size;
    m->nrefs = 0u;
    m->specials = specials;
}

inline void SharemindMemorySlot_destroy(SharemindMemorySlot * const m)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindMemorySlot_destroy(SharemindMemorySlot * const m) {
    assert(m);
    if (m->specials) {
        if (m->specials->free)
            m->specials->free(m);
    } else {
        free(m->pData);
    }
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_MEMORYSLOT_H */
