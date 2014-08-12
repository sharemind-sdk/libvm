/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PROGRAM_H
#define SHAREMIND_LIBVM_PROGRAM_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/mutex.h>
#include <sharemind/refs.h>
#include "codesectionsvector.h"
#include "datasectionsvector.h"
#include "datasectionsizesvector.h"
#include "pdbindingsvector.h"
#include "syscallbindingsvector.h"
#include "vm.h"


#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 *  SharemindProgram
*******************************************************************************/

#ifndef SHAREMIND_SOFT_FLOAT
typedef enum {
    SHAREMIND_HET_FPE_UNKNOWN = 0x00, /* Unknown arithmetic or FP error */
    SHAREMIND_HET_FPE_INTDIV  = 0x01, /* integer divide by zero */
    SHAREMIND_HET_FPE_INTOVF  = 0x02, /* integer overflow */
    SHAREMIND_HET_FPE_FLTDIV  = 0x03, /* floating-point divide by zero */
    SHAREMIND_HET_FPE_FLTOVF  = 0x04, /* floating-point overflow */
    SHAREMIND_HET_FPE_FLTUND  = 0x05, /* floating-point underflow */
    SHAREMIND_HET_FPE_FLTRES  = 0x06, /* floating-point inexact result */
    SHAREMIND_HET_FPE_FLTINV  = 0x07, /* floating-point invalid operation */
    SHAREMIND_HET_COUNT
} SharemindHardwareExceptionType;
#endif


struct SharemindProgram_ {
    SharemindCodeSectionsVector codeSections;
    SharemindDataSectionsVector rodataSections;
    SharemindDataSectionsVector dataSections;
    SharemindDataSectionSizesVector bssSectionSizes;

    SharemindSyscallBindingsVector bindings;
    SharemindPdBindings pdBindings;

    SharemindMutex mutex;
    SHAREMIND_REFS_DECLARE_FIELDS

    size_t activeLinkingUnit;

    size_t prepareCodeSectionIndex;
    uintptr_t prepareIp;

    SharemindVm * vm;
    SharemindVirtualMachineContext * overrides;

    SharemindVmError error;
    bool ready;

};

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif
SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindProgram)
#ifdef __GNUC__
#pragma GCC visibility pop
#endif


static inline const SharemindSyscallBinding * SharemindProgram_find_syscall(
        SharemindProgram * const p,
        const char * const signature)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline const SharemindSyscallBinding * SharemindProgram_find_syscall(
        SharemindProgram * const p,
        const char * const signature)
{
    assert(p);
    assert(signature);
    assert(signature[0u]);
    if (p->overrides && p->overrides->find_syscall)
        return (*(p->overrides->find_syscall))(p->overrides, signature);

    return SharemindVm_find_syscall(p->vm, signature);
}

static inline SharemindPd * SharemindProgram_find_pd(
        SharemindProgram * p,
        const char * pdName) __attribute__ ((nonnull(1, 2),
                                             warn_unused_result));
static inline SharemindPd * SharemindProgram_find_pd(SharemindProgram * const p,
                                                     const char * const pdName)
{
    assert(p);
    assert(pdName);
    assert(pdName[0u]);
    if (p->overrides && p->overrides->find_pd)
        return (*(p->overrides->find_pd))(p->overrides, pdName);

    return SharemindVm_find_pd(p->vm, pdName);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
