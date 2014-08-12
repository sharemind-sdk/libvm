/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_MEMORYINFO_H
#define SHAREMIND_LIBVM_MEMORYINFO_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <stddef.h>
#include "memoryinfostatistics.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    size_t usage;
    size_t upperLimit;
    SharemindMemoryInfoStatistics stats;
} SharemindMemoryInfo;

static inline void SharemindMemoryInfo_init(SharemindMemoryInfo * mi) __attribute__ ((nonnull(1)));
static inline void SharemindMemoryInfo_init(SharemindMemoryInfo * mi) {
    assert(mi);
    mi->usage = 0u;
    mi->upperLimit = SIZE_MAX;
    SharemindMemoryInfoStatistics_init(&mi->stats);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_MEMORYINFO_H */
