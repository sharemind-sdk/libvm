/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_SYSCALL_H
#define SHAREMIND_LIBSMVM_SYSCALL_H

#include "vm.h"


#ifdef __cplusplus
extern "C" {
#endif

struct _SMVM_Module;

typedef struct {
    /** Unique name of the system call. */
    char * name;

    /** Pointer to implementation function if current API, otherwise to a wrapper function. */
    void * impl_or_wrapper;

    /** NULL if current API, otherwise pointer to implementation function. */
    void * null_or_impl;

    /** Pointer to module providing this syscall. */
    struct _SMVM_Module * module;
} SMVM_Syscall;

int SMVM_Syscall_init(SMVM_Syscall * sc, const char * name, void * impl, void * wrapper, struct _SMVM_Module * m);

SMVM_Syscall * SMVM_Syscall_copy(SMVM_Syscall * dest, const SMVM_Syscall * src);

void SMVM_Syscall_destroy(SMVM_Syscall * sc);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_SYSCALL_H */
