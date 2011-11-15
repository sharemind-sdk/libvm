/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMVM_VM_H
#define LIBSMVM_VM_H

#include "../codeblock.h"
#include "../preprocessor.h"
#include "../static_assert.h"
#include "syscallmap.h"

#ifdef __cplusplus
extern "C" {
#endif

SM_STATIC_ASSERT(sizeof(SMVM_CodeBlock) == sizeof(uint64_t));
SM_STATIC_ASSERT(sizeof(size_t) <= sizeof(uint64_t));
SM_STATIC_ASSERT(sizeof(size_t) >= sizeof(uint16_t));
SM_STATIC_ASSERT(sizeof(ptrdiff_t) <= sizeof(uint64_t));
SM_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t));
SM_STATIC_ASSERT(sizeof(char) == sizeof(uint8_t));

#define SMVM_ENUM_State \
    (SMVM_INITIALIZED) \
    (SMVM_PREPARING) \
    (SMVM_PREPARED) \
    (SMVM_RUNNING) \
    (SMVM_TRAPPED) \
    (SMVM_TRAPPED_IN_SYSCALL) \
    (SMVM_FINISHED) \
    (SMVM_CRASHED)
SM_ENUM_DEFINE(SMVM_State, SMVM_ENUM_State);
SM_ENUM_DECLARE_TOSTRING(SMVM_State);


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
    ((SMVM_RUNTIME_EXCEPTION,)) \
    ((SMVM_RUNTIME_TRAP,))
SM_ENUM_CUSTOM_DEFINE(SMVM_Error, SMVM_ENUM_Error);
SM_ENUM_DECLARE_TOSTRING(SMVM_Error);

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
SM_ENUM_CUSTOM_DEFINE(SMVM_Exception, SMVM_ENUM_Exception);
SM_ENUM_DECLARE_TOSTRING(SMVM_Exception);

struct _SMVM_Program;
typedef struct _SMVM_Program SMVM_Program;

/**
 * \brief Allocates and initializes a new SMVM_Program instance.
 * \returns a pointer to the new SMVM_Program instance.
 * \retval NULL if allocation failed.
 */
SMVM_Program * SMVM_Program_new(void) __attribute__ ((warn_unused_result));

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
SMVM_Error SMVM_Program_load_from_sme(SMVM_Program * program, const void * data, size_t dataSize, SMVM_SyscallMap * syscallMap) __attribute__ ((nonnull(1, 2, 4), warn_unused_result));

/**
 * \brief Adds a code section to the program and prepares it for direct execution.
 * \pre codeSize > 0
 * \pre This function has been called less than program->numCodeSections on the program object.
 * \pre program->state == SMVM_INITIALIZED
 * \post program->state == SMVM_INITIALIZED
 * \param program The program to prepare.
 * \param[in] code The program code, of which a copy is made and processed.
 * \param[in] codeSize The length of the code.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_addCodeSection(SMVM_Program * program, const SMVM_CodeBlock * code, const size_t codeSize) __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SMVM_Error.
 */
SMVM_Error SMVM_Program_endPrepare(SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

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

#endif /* LIBSMVM_VM_H */
