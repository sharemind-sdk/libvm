/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_VM_H
#define SHAREMIND_LIBSMVM_VM_H

#include <sharemind/codeblock.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/preprocessor.h>
#include <sharemind/static_assert.h>


#ifdef __cplusplus
extern "C" {
#endif

SHAREMIND_STATIC_ASSERT(sizeof(size_t) >= sizeof(uint16_t));

struct _SMVM_Program;
typedef struct _SMVM_Program SMVM_Program;

typedef struct _SMVM_SyscallBinding {
    SMVM_SyscallWrapper wrapper;
    void * moduleHandle;
} SMVM_SyscallBinding;

struct _SMVM_Context_PDPI;
typedef struct _SMVM_Context_PDPI SMVM_Context_PDPI;
struct _SMVM_Context_PDPI {
    void * pdProcessHandle;
    size_t pdkIndex;
    void * moduleHandle;
};

struct _SMVM_Context;
typedef struct _SMVM_Context SMVM_Context;
struct _SMVM_Context {

    /**
      A destructor (e.g. free) for this SMVM_context.
      \param[in] context a pointer to this struct.
    */
    void (*destructor)(SMVM_Context * context);

    /**
      \param[in] context a pointer to this struct.
      \param[in] signature the system call signature.
      \returns a system call with the given signature.
      \retval NULL if no such system call is provided.
    */
    const SMVM_SyscallBinding * (*find_syscall)(SMVM_Context * context, const char * signature);

    /**
      \param[in] context a pointer to this struct.
      \param[in] pdname the name of the protection domain.
      \param[in] process the process requesting the pd process instance.
      \retval NULL if no such protection domain is found.
    */
    const SMVM_Context_PDPI * (*get_pd_process_instance_handle)(SMVM_Context * context, const char * pdName, SMVM_Program * process);

    /** Pointer to any SMVM_Context data. Not used by libsmvm. */
    void * internal;

};

struct _SMVM;
typedef struct _SMVM SMVM;
SMVM * SMVM_new(SMVM_Context * context);
void SMVM_free(SMVM * smvm);

#define SMVM_ENUM_State \
    (SMVM_INITIALIZED) \
    (SMVM_PREPARING) \
    (SMVM_PREPARED) \
    (SMVM_RUNNING) \
    (SMVM_TRAPPED) \
    (SMVM_TRAPPED_IN_SYSCALL) \
    (SMVM_FINISHED) \
    (SMVM_CRASHED)
SHAREMIND_ENUM_DEFINE(SMVM_State, SMVM_ENUM_State);
SHAREMIND_ENUM_DECLARE_TOSTRING(SMVM_State);


#define SMVM_ENUM_Error \
    ((SMVM_OK, = 0)) \
    ((SMVM_OUT_OF_MEMORY,)) \
    ((SMVM_INVALID_INPUT_STATE,)) \
    ((SMVM_PREPARE_ERROR_INVALID_INPUT_FILE,)) \
    ((SMVM_PREPARE_ERROR_NO_CODE_SECTION,)) \
    ((SMVM_PREPARE_ERROR_INVALID_HEADER,)) \
    ((SMVM_PREPARE_ERROR_INVALID_INSTRUCTION,)) \
    ((SMVM_PREPARE_ERROR_INVALID_ARGUMENTS,)) \
    ((SMVM_PREPARE_UNDEFINED_BIND,)) \
    ((SMVM_PREPARE_UNDEFINED_PDBIND,)) \
    ((SMVM_RUNTIME_EXCEPTION,)) \
    ((SMVM_RUNTIME_TRAP,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SMVM_Error, SMVM_ENUM_Error);
SHAREMIND_ENUM_DECLARE_TOSTRING(SMVM_Error);

#define SMVM_ENUM_Exception \
    ((SMVM_E_NONE, = 0x00)) \
    ((SMVM_E_OUT_OF_MEMORY, = 0x01)) \
    ((SMVM_E_INVALID_ARGUMENT, = 0x02)) \
    ((SMVM_E_INVALID_SYSCALL_INVOCATION, = 0x03)) \
    ((SMVM_E_SYSCALL_FAILURE, = 0x04)) \
    ((SMVM_E_INVALID_INDEX_REGISTER, = 0x100)) \
    ((SMVM_E_INVALID_INDEX_STACK, = 0x101)) \
    ((SMVM_E_INVALID_INDEX_REFERENCE, = 0x102)) \
    ((SMVM_E_INVALID_INDEX_CONST_REFERENCE, = 0x103)) \
    ((SMVM_E_JUMP_TO_INVALID_ADDRESS, = 0x200)) \
    ((SMVM_E_INVALID_INDEX_SYSCALL, = 0x201)) \
    ((SMVM_E_INVALID_REFERENCE, = 0x300)) \
    ((SMVM_E_MEMORY_IN_USE, = 0x301)) \
    ((SMVM_E_OUT_OF_BOUNDS_READ, = 0x302)) \
    ((SMVM_E_OUT_OF_BOUNDS_WRITE, = 0x303)) \
    ((SMVM_E_OUT_OF_BOUNDS_REFERENCE_INDEX, = 0x304)) \
    ((SMVM_E_OUT_OF_BOUNDS_REFERENCE_SIZE, = 0x305)) \
    ((SMVM_E_READ_DENIED, = 0x306)) \
    ((SMVM_E_WRITE_DENIED, = 0x307)) \
    ((SMVM_E_UNKNOWN_FPE, = 0x400)) \
    ((SMVM_E_INTEGER_DIVIDE_BY_ZERO, = 0x401)) \
    ((SMVM_E_INTEGER_OVERFLOW, = 0x402)) \
    ((SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO, = 0x403)) \
    ((SMVM_E_FLOATING_POINT_OVERFLOW, = 0x404)) \
    ((SMVM_E_FLOATING_POINT_UNDERFLOW, = 0x405)) \
    ((SMVM_E_FLOATING_POINT_INEXACT_RESULT, = 0x406)) \
    ((SMVM_E_FLOATING_POINT_INVALID_OPERATION, = 0x407)) \
    ((SMVM_E_COUNT,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SMVM_Exception, SMVM_ENUM_Exception);
SHAREMIND_ENUM_DECLARE_TOSTRING(SMVM_Exception);


/**
 * \brief Allocates and initializes a new SMVM_Program instance.
 * \returns a pointer to the new SMVM_Program instance.
 * \retval NULL if allocation failed.
 */
SMVM_Program * SMVM_Program_new(SMVM * smvm) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Deallocates a SMVM_Program instance.
 * \param[in] program pointer to the SMVM_Program instance to free.
 */
void SMVM_Program_free(SMVM_Program * program) __attribute__ ((nonnull(1)));

/**
 * \brief Loads the program sections from the given data.
 * \param program pointer to the SMVM_Program to initialize.
 * \param[in] data pointer to the sharemind executable file data.
 * \param[in] dataSize size of the data pointed to by the data parameter, in bytes.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_load_from_sme(SMVM_Program * program, const void * data, size_t dataSize) __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Starts execution of the given program in the background.
 * \pre program->state == SMVM_PREPARED
 * \param program The program to run.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_run(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Continues execution of the given program in the background.
 * \pre program->state == SMVM_TRAPPED || program->state == SMVM_TRAPPED_IN_SYSCALL
 * \param program The program to continue.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_continue(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Stops execution of the given program running in the background.
 *
 * Execution is stopped before executing any jump or call instruction, or in the
 * middle of asynchronous system calls.
 *
 * \pre program->state == SMVM_RUNNING
 * \post program->state == SMVM_TRAPPED || program->state == SMVM_TRAPPED_IN_SYSCALL
 * \param program The program to stop.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_stop(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] program pointer to the SMVM_Program instance.
 * \returns the return value of the program.
 */
int64_t SMVM_Program_get_return_value(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] program pointer to the SMVM_Program instance.
 * \returns the exception value of the program.
 */
SMVM_Exception SMVM_Program_get_exception(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] program pointer to the SMVM_Program instance.
 * \returns the current code section of the program.
 */
size_t SMVM_Program_get_current_codesection(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] program pointer to the SMVM_Program instance.
 * \returns the current instruction pointer of the program.
 */
uintptr_t SMVM_Program_get_current_ip(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_VM_H */
