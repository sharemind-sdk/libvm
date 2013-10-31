/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PROCESS_H
#define SHAREMIND_LIBVM_PROCESS_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#ifdef SHAREMIND_SOFT_FLOAT
#include <sharemind/3rdparty/libsoftfloat/softfloat.h>
#endif
#include "datasectionsvector.h"
#include "framestack.h"
#include "libvm.h"
#include "memoryinfo.h"
#include "memorymap.h"
#include "pdpicache.h"
#include "privatememorymap.h"
#include "stackframe.h"


#ifdef __cplusplus
extern "C" {
#endif


struct SharemindProcess_ {
    SharemindProgram * program;

    SharemindDataSectionsVector dataSections;
    SharemindDataSectionsVector bssSections;

    SharemindPdpiCache pdpiCache;

    SharemindFrameStack frames;
    SharemindStackFrame * globalFrame;
    SharemindStackFrame * nextFrame;
    SharemindStackFrame * thisFrame;

    SharemindMemoryMap memoryMap;
    uint64_t memorySlotNext;
    SharemindPrivateMemoryMap privateMemoryMap;

    size_t currentCodeSectionIndex;
    uintptr_t currentIp;

#ifdef SHAREMIND_SOFT_FLOAT
    sf_fpu_state fpuState;
#endif

    SharemindCodeBlock returnValue;
    int64_t exceptionValue;

    SharemindModuleApi0x1SyscallContext syscallContext;

    SharemindMemoryInfo memPublicHeap;
    SharemindMemoryInfo memPrivate;
    SharemindMemoryInfo memReserved;
    SharemindMemoryInfo memTotal;

    SharemindVmProcessState state;
};

uint64_t SharemindProcess_public_alloc(SharemindProcess * p,
                                       uint64_t nBytes,
                                       SharemindMemorySlot ** memorySlot)
        __attribute__ ((visibility("internal")));
SharemindVmProcessException SharemindProcess_public_free(SharemindProcess * p,
                                                         uint64_t ptr)
        __attribute__ ((visibility("internal")));


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_PROCESS_H */
