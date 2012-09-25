/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSHAREMIND_VM_INTERNAL_PROFILE_H
#define SHAREMIND_LIBSHAREMIND_VM_INTERNAL_PROFILE_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <stdint.h>
#include <stdlib.h>
#include "libvm.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct SharemindProcessProfiler_ {
    SHAREMIND_LIBVM_TIME_TYPE profileStartTime;
    SHAREMIND_LIBVM_TIME_TYPE profileEndTime;
    SharemindInstrProfile * currentInstrProfile;
    SharemindCodeSectionProfile * currentCodeSectionProfile;
    SharemindProcessProfile profile;
} SharemindProcessProfiler;

#define LIBVM_TIME_TYPE_INIT_PERIOD(t) \
  do { \
    (t).tv_sec = 0; \
    (t).tv_nsec = 0; \
  } while(0)

#define LIBVM_ADD_TO_TIME(result,startTime,endTime) \
  do { \
    if ((endTime).tv_nsec < (startTime).tv_nsec) { \
        (result).tv_sec += (endTime).tv_sec - (startTime).tv_sec - 1; \
        (result).tv_nsec += (endTime).tv_nsec - (startTime).tv_nsec + 1000000000; \
    } else { \
        (result).tv_sec += (endTime).tv_sec - (startTime).tv_sec; \
        (result).tv_nsec += (endTime).tv_nsec - (startTime).tv_nsec; \
    } \
  } while(0)

#ifdef __MACH__ /* OS X does not have clock_gettime, use clock_get_time */
#define LIBVM_TIME_INSTRUCTION_GETTIME(t) do { clock_get_time(p->macClock, &(t)); } while (0)
#elif defined(CLOCK_MONOTONIC_RAW)
#define LIBVM_TIME_INSTRUCTION_GETTIME(t) do { clock_gettime(CLOCK_MONOTONIC_RAW, &(t)); } while(0)
#else
#define LIBVM_TIME_INSTRUCTION_GETTIME(t) do { clock_gettime(CLOCK_MONOTONIC, &(t)); } while(0)
#endif

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSHAREMIND_VM_INTERNAL_PROFILE_H */
