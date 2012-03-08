/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_MEMORYSLOT_H
#define SHAREMIND_LIBVM_MEMORYSLOT_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


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

static inline void SharemindMemorySlot_init(SharemindMemorySlot * m,
                                            void * pData,
                                            size_t size,
                                            SharemindMemorySlotSpecials * specials)
    __attribute__ ((nonnull(1)));
static inline void SharemindMemorySlot_init(SharemindMemorySlot * m,
                                            void * pData,
                                            size_t size,
                                            SharemindMemorySlotSpecials * specials)
{
    m->pData = pData;
    m->size = size;
    m->nrefs = 0u;
    m->specials = specials;
}

static inline void SharemindMemorySlot_destroy(SharemindMemorySlot * m) __attribute__ ((nonnull(1)));
static inline void SharemindMemorySlot_destroy(SharemindMemorySlot * m) {
    if (m->specials) {
        if (m->specials->free)
            m->specials->free(m);
    } else {
        free(m->pData);
    }
}

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_MEMORYSLOT_H */
