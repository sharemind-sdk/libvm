/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_VM_H
#define SHAREMIND_LIBVM_VM_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include "libvm.h"

#include <assert.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/recursive_locks.h>
#include <sharemind/refs.h>
#include <sharemind/tag.h>
#include "lasterror.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindVm_ {

    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;
    SHAREMIND_REFS_DECLARE_FIELDS
    SHAREMIND_TAG_DECLARE_FIELDS;

    SharemindVirtualMachineContext * context;

};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE_DEFINE(
        SharemindVm,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindVm)

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif
SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindVm)
#ifdef __GNUC__
#pragma GCC visibility pop
#endif


static inline SharemindSyscallWrapper SharemindVm_findSyscall(
        SharemindVm * const vm,
        const char * const signature)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindSyscallWrapper SharemindVm_findSyscall(
        SharemindVm * const vm,
        const char * const signature)
{
    assert(vm);
    assert(signature);
    assert(signature[0u]);
    if (!vm->context || !vm->context->find_syscall) {
        static const SharemindSyscallWrapper nullWrapper = { NULL, NULL };
        return nullWrapper;
    }

    return (*(vm->context->find_syscall))(vm->context, signature);
}

static inline SharemindPd * SharemindVm_findPd(SharemindVm * const vm,
                                               const char * const pdName)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindPd * SharemindVm_findPd(SharemindVm * const vm,
                                               const char * const pdName)
{
    assert(vm);
    assert(pdName);
    assert(pdName[0u]);
    if (!vm->context || !vm->context->find_pd)
        return NULL;

    return (*(vm->context->find_pd))(vm->context, pdName);
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_VM_H */
