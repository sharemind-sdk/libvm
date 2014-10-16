/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__

#include "vm.h"

#include <sharemind/likely.h>
#include <stdlib.h>


/*******************************************************************************
 *  Public enum methods
*******************************************************************************/

SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmError, SHAREMIND_VM_ERROR_ENUM)


/*******************************************************************************
 *  SharemindVm
*******************************************************************************/

SharemindVm * SharemindVm_new(SharemindVirtualMachineContext * context,
                              SharemindVmError * error,
                              const char ** errorStr)
{
    assert(context);

    SharemindVm * const vm = (SharemindVm *) malloc(sizeof(SharemindVm));
    if (unlikely(!vm)) {
        if (error)
            (*error) = SHAREMIND_VM_OUT_OF_MEMORY;
        if (errorStr)
            (*errorStr) = "Out of memory!";
        goto SharemindVm_new_error_0;
    }

    if (!SHAREMIND_RECURSIVE_LOCK_INIT(vm)) {
        if (error)
            (*error) = SHAREMIND_VM_MUTEX_ERROR;
        if (errorStr)
            (*errorStr) = "Mutex initialization error!";
        goto SharemindVm_new_error_1;
    }

    SHAREMIND_LIBVM_LASTERROR_INIT(vm);
    SHAREMIND_REFS_INIT(vm);
    SHAREMIND_TAG_INIT(vm);
    vm->context = context;
    return vm;

SharemindVm_new_error_1:

    free(vm);

SharemindVm_new_error_0:

    return NULL;
}

void SharemindVm_free(SharemindVm * vm) {
    assert(vm);
    SHAREMIND_REFS_ASSERT_IF_REFERENCED(vm);
    if (vm->context && vm->context->destructor)
        (*(vm->context->destructor))(vm->context);
    SHAREMIND_TAG_DESTROY(vm);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(vm);
    free(vm);
}

SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindVm)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindVm,)
SHAREMIND_REFS_DEFINE_FUNCTIONS_WITH_RECURSIVE_MUTEX(SharemindVm)
