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

#ifndef SHAREMIND_LIBVM_MEMORYINFO_H
#define SHAREMIND_LIBVM_MEMORYINFO_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <assert.h>
#include <sharemind/extern_c.h>
#include <stddef.h>


SHAREMIND_EXTERN_C_BEGIN

typedef struct {
    size_t usage;
    size_t max;
    size_t upperLimit;
} SharemindMemoryInfo;

static inline void SharemindMemoryInfo_init(SharemindMemoryInfo * const mi)
        __attribute__ ((nonnull(1)));
static inline void SharemindMemoryInfo_init(SharemindMemoryInfo * const mi) {
    assert(mi);
    mi->usage = 0u;
    mi->max = 0u;
    mi->upperLimit = SIZE_MAX;
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_MEMORYINFO_H */
