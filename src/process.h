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

#include <sharemind/3rdparty/libsoftfloat/softfloat.h>
#include <sharemind/extern_c.h>
#include <sharemind/recursive_locks.h>
#include "datasectionsvector.h"
#include "framestack.h"
#include "lasterror.h"
#include "libvm.h"
#include "memoryinfo.h"
#include "memorymap.h"
#include "pdpicache.h"
#include "privatememorymap.h"
#include "stackframe.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindProcess_ {
    SharemindProgram * program;

    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;

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

    sf_fpu_state fpuState;

    SharemindCodeBlock returnValue;
    int64_t exceptionValue;

    SharemindModuleApi0x1SyscallContext syscallContext;

    SharemindMemoryInfo memPublicHeap;
    SharemindMemoryInfo memPrivate;
    SharemindMemoryInfo memReserved;
    SharemindMemoryInfo memTotal;

    SharemindVmProcessState state;
};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE_DEFINE(
        SharemindProcess,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindProcess)

uint64_t SharemindProcess_public_alloc(SharemindProcess * const p,
                                       const uint64_t nBytes,
                                       SharemindMemorySlot ** const memorySlot)
        __attribute__ ((visibility("internal"),
                        nonnull(1),
                        warn_unused_result));
SharemindVmProcessException SharemindProcess_public_free(
        SharemindProcess * const p,
        const uint64_t ptr)
        __attribute__ ((visibility("internal"),
                        nonnull(1),
                        warn_unused_result));

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PROCESS_H */
