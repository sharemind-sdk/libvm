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

#ifdef __cplusplus
extern "C" {
#endif

SM_STATIC_ASSERT(sizeof(union SM_CodeBlock) == sizeof(uint64_t));
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
    ((SMVM_RUNTIME_EXCEPTION,)) \
    ((SMVM_RUNTIME_TRAP,))
SM_ENUM_CUSTOM_DEFINE(SMVM_Error, SMVM_ENUM_Error);
SM_ENUM_DECLARE_TOSTRING(SMVM_Error);

#define SMVM_ENUM_Exception \
    ((SMVM_E_OUT_OF_MEMORY, = 0x00)) \
    ((SMVM_E_INVALID_REGISTER_INDEX, = 0x10)) \
    ((SMVM_E_INVALID_STACK_INDEX, = 0x20)) \
    ((SMVM_E_INVALID_MEMORY_POINTER, = 0x30)) \
    ((SMVM_E_JUMP_TO_INVALID_ADDRESS, = 0x40))
SM_ENUM_CUSTOM_DEFINE(SMVM_Exception, SMVM_ENUM_Exception);
SM_ENUM_DECLARE_TOSTRING(SMVM_Exception);


struct SMVM_Program;

/**
 * \brief Allocates and initializes a new SMVM_Program instance.
 * \returns a pointer to the new SMVM_Program instance.
 * \retval NULL if allocation failed.
 */
struct SMVM_Program * SMVM_Program_new() __attribute__ ((warn_unused_result));

/**
 * \brief Deallocates a SMVM_Program instance.
 * \param[in] p pointer to the SMVM_Program instance to free.
 */
void SMVM_Program_free(struct SMVM_Program *p) __attribute__ ((nonnull(1)));

/**
 * \brief Loads the program sections from the given data.
 * \param p pointer to the SMVM_Program to initialize.
 * \param[in] data pointer to the sharemind executable file data.
 * \param[in] dataSize size of the data pointed to by the data parameter, in bytes.
 * \returns an SMVM_Error.
 */
int SMVM_Program_load_from_sme(struct SMVM_Program *p, const void * data, size_t dataSize) __attribute__ ((nonnull(1, 2), warn_unused_result));

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
int SMVM_Program_addCodeSection(struct SMVM_Program * program, const union SM_CodeBlock * code, const size_t codeSize) __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SMVM_Error.
 */
int SMVM_Program_endPrepare(struct SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Starts execution of the given program in the background.
 * \pre program->state == SMVM_PREPARED
 * \param program The program to run.
 * \returns an SMVM_Error.
 */
int SMVM_Program_run(struct SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Continues execution of the given program in the background.
 * \pre program->state == SMVM_TRAPPED || program->state == SMVM_TRAPPED_IN_SYSCALL
 * \param program The program to continue.
 * \returns an SMVM_Error.
 */
int SMVM_Program_continue(struct SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

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
int SMVM_Program_stop(struct SMVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SMVM_Program instance.
 * \returns the return value of the program.
 */
int64_t SMVM_Program_get_return_value(struct SMVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SMVM_Program instance.
 * \returns the exception value of the program.
 */
int64_t SMVM_Program_get_exception_value(struct SMVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SMVM_Program instance.
 * \returns the current code section of the program.
 */
size_t SMVM_Program_get_current_codesection(struct SMVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SMVM_Program instance.
 * \returns the current instruction pointer of the program.
 */
size_t SMVM_Program_get_current_ip(struct SMVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMVM_VM_H */
