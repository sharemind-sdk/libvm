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

#ifndef SHAREMIND_LIBVM_CODESECTION_H
#define SHAREMIND_LIBVM_CODESECTION_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "instrmap.h"


SHAREMIND_EXTERN_C_BEGIN

typedef struct {
    SharemindCodeBlock * data;
    size_t size;
    SharemindInstrMap instrmap;
    SharemindInstrMapP blockmap;
} SharemindCodeSection;


static inline bool SharemindCodeSection_init(
        SharemindCodeSection * const s,
        SharemindCodeBlock const * const code,
        size_t const codeSize) __attribute__ ((nonnull(1), warn_unused_result));
static inline bool SharemindCodeSection_init(
        SharemindCodeSection * const s,
        SharemindCodeBlock const * const code,
        size_t const codeSize)
{
    assert(s);
    assert(code || codeSize == 0u);

    /* Add space for an exception pointer: */
    size_t const realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return false;

    size_t const memSize = realCodeSize * sizeof(SharemindCodeBlock);
    if (unlikely(memSize / sizeof(SharemindCodeBlock) != realCodeSize))
        return false;


    s->data = (SharemindCodeBlock *) malloc(memSize);
    if (unlikely(!s->data))
        return false;

    s->size = codeSize;
    memcpy(s->data, code, memSize);

    SharemindInstrMap_init(&s->instrmap);
    SharemindInstrMapP_init(&s->blockmap);

    return true;
}


inline void SharemindCodeSection_destroy(SharemindCodeSection * const s)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindCodeSection_destroy(SharemindCodeSection * const s)
{
    assert(s);
    free(s->data);
    SharemindInstrMapP_destroy(&s->blockmap);
    SharemindInstrMap_destroy(&s->instrmap);
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_CODESECTION_H */
