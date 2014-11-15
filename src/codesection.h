/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
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
        const SharemindCodeBlock * const code,
        const size_t codeSize) __attribute__ ((nonnull(1), warn_unused_result));
static inline bool SharemindCodeSection_init(
        SharemindCodeSection * const s,
        const SharemindCodeBlock * const code,
        const size_t codeSize)
{
    assert(s);
    assert(code || codeSize == 0u);

    /* Add space for an exception pointer: */
    const size_t realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return false;

    const size_t memSize = realCodeSize * sizeof(SharemindCodeBlock);
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
