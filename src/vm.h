/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#ifndef SHAREMIND_LIBVM_VM_H
#define SHAREMIND_LIBVM_VM_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include "libvm.h"

#include <assert.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/recursive_locks.h>
#include <sharemind/tag.h>
#include "lasterror.h"
#include "processfacilitymap.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindVm_ {

    SharemindProcessFacilityMap processFacilityMap;
    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;
    SHAREMIND_TAG_DECLARE_FIELDS;

    SharemindVirtualMachineContext * context;

};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE_DEFINE(
        SharemindVm,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindVm)

static inline SharemindSyscallWrapper SharemindVm_findSyscall(
        SharemindVm * const vm,
        char const * const signature)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindSyscallWrapper SharemindVm_findSyscall(
        SharemindVm * const vm,
        char const * const signature)
{
    assert(vm);
    assert(signature);
    assert(signature[0u]);
    if (!vm->context || !vm->context->find_syscall) {
        static SharemindSyscallWrapper const nullWrapper = { nullptr, nullptr };
        return nullWrapper;
    }

    return (*(vm->context->find_syscall))(vm->context, signature);
}

static inline SharemindPd * SharemindVm_findPd(SharemindVm * const vm,
                                               char const * const pdName)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindPd * SharemindVm_findPd(SharemindVm * const vm,
                                               char const * const pdName)
{
    assert(vm);
    assert(pdName);
    assert(pdName[0u]);
    if (!vm->context || !vm->context->find_pd)
        return nullptr;

    return (*(vm->context->find_pd))(vm->context, pdName);
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_VM_H */
