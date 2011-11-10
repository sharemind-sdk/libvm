/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "syscall.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


int SMVM_Syscall_init(SMVM_Syscall * sc, const char * name, void * impl, void * wrapper, struct _SMVM_Module * m) {
    assert(sc);
    assert(name);
    assert(impl);

    sc->name = strdup(name);
    if (!sc->name)
        return 0;

    if (wrapper) {
        sc->impl_or_wrapper = wrapper;
        sc->null_or_impl = impl;
    } else {
        sc->impl_or_wrapper = impl;
        sc->null_or_impl = NULL;
    }
    sc->module = m;
    return 1;
}

SMVM_Syscall * SMVM_Syscall_copy(SMVM_Syscall * dest, const SMVM_Syscall * src) {
    dest->name = strdup(src->name);
    if (!dest->name)
        return NULL;

    dest->impl_or_wrapper = src->impl_or_wrapper;
    dest->null_or_impl = src->null_or_impl;
    dest->module = src->module;
    return dest;
}

void SMVM_Syscall_destroy(SMVM_Syscall * sc) {
    assert(sc);
    assert(sc->name);
    assert(sc->impl_or_wrapper);

    free(sc->name);
}
