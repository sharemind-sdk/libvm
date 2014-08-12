/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_DATASECTION_H
#define SHAREMIND_LIBVM_DATASECTION_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <stdbool.h>
#include "memoryslot.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef SharemindMemorySlot SharemindDataSection;

static inline bool SharemindDataSection_init(SharemindDataSection * ds,
                                             size_t size,
                                             SharemindMemorySlotSpecials * specials)
        __attribute__ ((nonnull(1), warn_unused_result));
static inline bool SharemindDataSection_init(SharemindDataSection * ds,
                                             size_t size,
                                             SharemindMemorySlotSpecials * specials)
{
    assert(ds);
    assert(specials);

    if (size != 0) {
        ds->pData = malloc(size);
        if (unlikely(!ds->pData))
            return false;
    } else {
        ds->pData = NULL;
    }
    ds->size = size;
    ds->nrefs = 1u;
    ds->specials = specials;
    return true;
}

static inline void SharemindDataSection_destroy(SharemindDataSection * ds) __attribute__ ((nonnull(1)));
static inline void SharemindDataSection_destroy(SharemindDataSection * ds) {
    free(ds->pData);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_DATASECTION_H */

