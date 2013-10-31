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

#include <sharemind/refs.h>


#ifdef __cplusplus
extern "C" {
#endif


struct SharemindVm_ {
    SharemindVirtualMachineContext * context;

    SHAREMIND_REFS_DECLARE_FIELDS
};

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif
SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindVm)
#ifdef __GNUC__
#pragma GCC visibility pop
#endif


static inline const SharemindSyscallBinding * SharemindVm_find_syscall(SharemindVm * vm, const char * signature) __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline const SharemindSyscallBinding * SharemindVm_find_syscall(SharemindVm * vm, const char * signature) {
    assert(vm);
    assert(signature);
    assert(signature[0u]);
    if (!vm->context || !vm->context->find_syscall)
        return NULL;

    return (*(vm->context->find_syscall))(vm->context, signature);
}

static inline SharemindPd * SharemindVm_find_pd(SharemindVm * vm, const char * pdName) __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindPd * SharemindVm_find_pd(SharemindVm * vm, const char * pdName) {
    assert(vm);
    assert(pdName);
    assert(pdName[0u]);
    if (!vm->context || !vm->context->find_pd)
        return NULL;

    return (*(vm->context->find_pd))(vm->context, pdName);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_VM_H */
