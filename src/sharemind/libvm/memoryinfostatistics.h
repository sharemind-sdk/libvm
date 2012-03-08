/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_MEMORYINFOSTATISTICS_H
#define SHAREMIND_LIBVM_MEMORYINFOSTATISTICS_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    size_t max;
} SharemindMemoryInfoStatistics;

static inline void SharemindMemoryInfoStatistics_init(SharemindMemoryInfoStatistics * mis) __attribute__ ((nonnull(1)));
static inline void SharemindMemoryInfoStatistics_init(SharemindMemoryInfoStatistics * mis) {
    assert(mis);
    mis->max = 0u;
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_MEMORYINFOSTATISTICS_H */
