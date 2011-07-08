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

SVM_STATIC_ASSERT(sizeof(union SVM_IBlock) == sizeof(uint64_t));
SVM_STATIC_ASSERT(sizeof(size_t) <= sizeof(uint64_t));
SVM_STATIC_ASSERT(sizeof(size_t) >= sizeof(uint16_t));
SVM_STATIC_ASSERT(sizeof(ptrdiff_t) <= sizeof(uint64_t));
SVM_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t));
SVM_STATIC_ASSERT(sizeof(char) == sizeof(uint8_t));

#define SVM_ENUM_State \
    (SVM_INITIALIZED) \
    (SVM_PREPARING) \
    (SVM_PREPARED) \
    (SVM_RUNNING) \
    (SVM_TRAPPED) \
    (SVM_TRAPPED_IN_SYSCALL) \
    (SVM_FINISHED) \
    (SVM_CRASHED)
SVM_ENUM_DEFINE(SVM_State, SVM_ENUM_State);
SVM_ENUM_DECLARE_TOSTRING(SVM_State);

#define SVM_ENUM_Runtime_State \
    (SVM_RUNNING_NORMAL) \
    (SVM_RUNNING_SYSCALL) \
    (SVM_RUNNING_SYSWAIT)
SVM_ENUM_DEFINE(SVM_Runtime_State, SVM_ENUM_Runtime_State);
SVM_ENUM_DECLARE_TOSTRING(SVM_Runtime_State);


#define SVM_ENUM_Error \
    ((SVM_OK, = 0)) \
    ((SVM_OUT_OF_MEMORY,)) \
    ((SVM_INVALID_INPUT_STATE,)) \
    ((SVM_PREPARE_ERROR_FILE_NOT_RECOGNIZED,)) \
    ((SVM_PREPARE_ERROR_NO_CODE_SECTION,)) \
    ((SVM_PREPARE_ERROR_INVALID_HEADER,)) \
    ((SVM_PREPARE_ERROR_INVALID_INSTRUCTION,)) \
    ((SVM_PREPARE_ERROR_INVALID_ARGUMENTS,)) \
    ((SVM_RUNTIME_EXCEPTION,)) \
    ((SVM_RUNTIME_TRAP,))
SVM_ENUM_CUSTOM_DEFINE(SVM_Error, SVM_ENUM_Error);
SVM_ENUM_DECLARE_TOSTRING(SVM_Error);

#define SVM_ENUM_Exception \
    ((SVM_E_OUT_OF_MEMORY, = 0x00)) \
    ((SVM_E_INVALID_REGISTER_INDEX, = 0x10)) \
    ((SVM_E_INVALID_STACK_INDEX, = 0x20)) \
    ((SVM_E_JUMP_TO_INVALID_ADDRESS, = 0x30))
SVM_ENUM_CUSTOM_DEFINE(SVM_Exception, SVM_ENUM_Exception);
SVM_ENUM_DECLARE_TOSTRING(SVM_Exception);


struct SVM_Program;

/**
 * \brief Allocates and initializes a new SVM_Program instance.
 * \returns a pointer to the new SVM_Program instance.
 * \retval NULL if allocation failed.
 */
struct SVM_Program * SVM_Program_new() __attribute__ ((warn_unused_result));

/**
 * \brief Deallocates a SVM_Program instance.
 * \param[in] p pointer to the SVM_Program instance to free.
 */
void SVM_Program_free(struct SVM_Program *p) __attribute__ ((nonnull(1)));

/**
 * \brief Adds a code section to the program and prepares it for direct execution.
 * \pre codeSize > 0
 * \pre This function has been called less than program->numCodeSections on the program object.
 * \pre program->state == SVM_INITIALIZED
 * \post program->state == SVM_INITIALIZED
 * \param program The program to prepare.
 * \param[in] code The program code, of which a copy is made and processed.
 * \param[in] codeSize The length of the code.
 * \returns an SVM_Error.
 */
int SVM_Program_addCodeSection(struct SVM_Program * program, const union SVM_IBlock * code, const size_t codeSize) __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SVM_Error.
 */
int SVM_Program_endPrepare(struct SVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Starts execution of the given program in the background.
 * \pre program->state == SVM_PREPARED
 * \param program The program to run.
 * \returns an SVM_Error.
 */
int SVM_Program_run(struct SVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Continues execution of the given program in the background.
 * \pre program->state == SVM_TRAPPED || program->state == SVM_TRAPPED_IN_SYSCALL
 * \param program The program to continue.
 * \returns an SVM_Error.
 */
int SVM_Program_continue(struct SVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Stops execution of the given program running in the background.
 *
 * Execution is stopped before executing any jump or call instruction, or in the
 * middle of asynchronous system calls.
 *
 * \pre program->state == SVM_RUNNING
 * \post program->state == SVM_TRAPPED || program->state == SVM_TRAPPED_IN_SYSCALL
 * \param program The program to stop.
 * \returns an SVM_Error.
 */
int SVM_Program_stop(struct SVM_Program * program) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SVM_Program instance.
 * \returns the return value of the program.
 */
int64_t SVM_Program_get_return_value(struct SVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SVM_Program instance.
 * \returns the exception value of the program.
 */
int64_t SVM_Program_get_exception_value(struct SVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SVM_Program instance.
 * \returns the current code section of the program.
 */
size_t SVM_Program_get_current_codesection(struct SVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param[in] p pointer to the SVM_Program instance.
 * \returns the current instruction pointer of the program.
 */
size_t SVM_Program_get_current_ip(struct SVM_Program *p) __attribute__ ((nonnull(1), warn_unused_result));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMVM_VM_H */
