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

#include <stdlib.h>


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmError, SHAREMIND_VM_ERROR_ENUM)


/*******************************************************************************
 *  SharemindVm
********************************************************************************/

SharemindVm * SharemindVm_new(SharemindVirtualMachineContext * context) {
    assert(context);
    SharemindVm * vm = (SharemindVm *) malloc(sizeof(SharemindVm));
    if (!vm)
        return NULL;
    vm->context = context;
    SHAREMIND_REFS_INIT(vm);
    return vm;
}

void SharemindVm_free(SharemindVm * vm) {
    assert(vm);
    SHAREMIND_REFS_ASSERT_IF_REFERENCED(vm);
    if (vm->context && vm->context->destructor)
        (*(vm->context->destructor))(vm->context);
    free(vm);
}

SHAREMIND_REFS_DEFINE_FUNCTIONS(SharemindVm)
