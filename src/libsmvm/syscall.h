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

#include "references.h"
#include "vm.h"


#ifdef __cplusplus
extern "C" {
#endif

/** Additional context provided for system calls: */
struct _SMVM_syscall_context;
typedef struct _SMVM_syscall_context SMVM_syscall_context;
struct _SMVM_syscall_context {
    /**
      A handle to the private data of the module instance. This is the same
      handle as provided to SMVM_module_context on module initialization.
    */
    void * const moduleHandle;

    /** Access to public dynamic memory inside the VM process: */
    uint64_t (* const publicAlloc)(size_t nBytes, SMVM_syscall_context * c);
    int (* const publicFree)(uint64_t ptr, SMVM_syscall_context * c);
    size_t (* const publicMemPtrSize)(uint64_t ptr, SMVM_syscall_context * c);
    void * (* const publicMemPtrData)(uint64_t ptr, SMVM_syscall_context * c);

    /** Access to dynamic memory not exposed to VM instructions: */
    void * (* const allocPrivate)(size_t nBytes, SMVM_syscall_context * c);
    int (* const freePrivate)(void * ptr, SMVM_syscall_context * c);

    /**
      Used to get access to internal data of protection domain per-process data
      (see below for pdProcessHandle):
    */
    void * (* const get_pd_process_handle)(uint64_t pd_index,
                                           SMVM_syscall_context * p);

    /* OTHER STUFF */
};

typedef SMVM_Exception (* SMVM_syscall_f)(
    /** Arguments passed to syscall: */
    SMVM_CodeBlock * args,
    size_t num_args,

    /** Mutable references passed to syscall: */
    const SMVM_Reference * refs,
    size_t num_refs,

    /** Immutable references passed to syscall: */
    const SMVM_CReference * crefs,
    size_t num_crefs,

    /**
      The pointer to where the return value of the syscall resides, or NULL if
      no return value is expected:
    */
    SMVM_CodeBlock * returnValue,

    SMVM_syscall_context * c
);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_SYSCALL_H */
