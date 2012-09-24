/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__

#include "profile.h"


#define SHAREMIND_VM_TIME_INSTRUCTION_DO(prof,startTime) \
    do { \
        SharemindInstrProfile * const lastInstrProfile = (prof)->currentInstrProfile; \
        if (lastInstrProfile) { \
            /* Add previous instruction time: */ \
            lastInstrProfile->calls++; \
            LIBVM_ADD_TO_TIME(lastInstrProfile->timeTaken, (prof)->profileEndTime, startTime); \
            /* Add previous profiling time: */ \
            LIBVM_ADD_TO_TIME((prof)->profile.timeTaken, (prof)->profileStartTime, (prof)->profileEndTime); \
        } \
    } while (0)

#define SHAREMIND_VM_RUN_PROCESS_NAME sharemind_vm_profile

#define SHAREMIND_VM_TIMING_START \
    { \
        p->profiler->currentCodeSectionProfile = &p->profiler->profile.codeSectionProfiles[p->currentCodeSectionIndex]; \
        p->profiler->currentInstrProfile = NULL; \
    }

#define SHAREMIND_VM_TIMING \
    { \
        SHAREMIND_LIBVM_TIME_TYPE startTime; \
        LIBVM_TIME_INSTRUCTION_GETTIME(startTime); \
        SHAREMIND_VM_TIME_INSTRUCTION_DO(p->profiler,startTime); \
        p->profiler->profileStartTime = startTime; \
        /* assert(((size_t) (ip - codeStart)) < p->profiler->currentCodeSectionProfile->numInstructions); */ \
        p->profiler->currentInstrProfile = &p->profiler->currentCodeSectionProfile->instrProfiles[(size_t) (ip - codeStart)]; \
        LIBVM_TIME_INSTRUCTION_GETTIME(p->profiler->profileEndTime); \
    }

#define SHAREMIND_VM_TIMING_STOP \
    { \
        SHAREMIND_LIBVM_TIME_TYPE startTime; \
        LIBVM_TIME_INSTRUCTION_GETTIME(startTime); \
        SHAREMIND_VM_TIME_INSTRUCTION_DO(p->profiler,startTime); \
        (p->profiler)->currentInstrProfile = NULL; \
    }

#include "core.c"
