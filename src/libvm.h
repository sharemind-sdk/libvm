/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSHAREMIND_VM_H
#define SHAREMIND_LIBSHAREMIND_VM_H

#include <sharemind/codeblock.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/preprocessor.h>
#include <sharemind/static_assert.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#define SHAREMIND_LIBVM_TIME_TYPE struct mach_timespec_t
#else
#define SHAREMIND_LIBVM_TIME_TYPE struct timespec
#endif


SHAREMIND_STATIC_ASSERT(sizeof(size_t) >= sizeof(uint16_t));

typedef struct {
    SharemindSyscallWrapper wrapper;
    void * moduleHandle;
} SharemindSyscallBinding;

struct SharemindVirtualMachineContext_;
typedef struct SharemindVirtualMachineContext_ SharemindVirtualMachineContext;
struct SharemindVirtualMachineContext_ {

    /**
      A destructor (e.g. free) for this SharemindVirtualMachineContext.
      \param[in] context a pointer to this struct.
    */
    void (*destructor)(SharemindVirtualMachineContext * context);

    /**
      \param[in] context a pointer to this struct.
      \param[in] signature the system call signature.
      \returns a system call with the given signature.
      \retval NULL if no such system call is provided.
    */
    const SharemindSyscallBinding * (*find_syscall)(
            SharemindVirtualMachineContext * context,
            const char * signature);

    /**
      \param[in] context a pointer to this struct.
      \param[in] pdname the name of the protection domain.
      \returns a running protection domain with the given name.
      \retval NULL if no such protection domain is found.
    */
    SharemindPd * (*find_pd)(SharemindVirtualMachineContext * context,
                             const char * pdName);

    /** Pointer to any SharemindVirtualMachineContext data. Not used by libvm.*/
    void * internal;

};

struct SharemindVm_;
typedef struct SharemindVm_ SharemindVm;
struct SharemindProgram_;
typedef struct SharemindProgram_ SharemindProgram;
struct SharemindProcess_;
typedef struct SharemindProcess_ SharemindProcess;

SharemindVm * SharemindVm_new(SharemindVirtualMachineContext * context);
void SharemindVm_free(SharemindVm * vm);

#define SHAREMIND_VM_PROCESS_STATE_ENUM \
    (SHAREMIND_VM_PROCESS_INITIALIZED) \
    (SHAREMIND_VM_PROCESS_RUNNING) \
    (SHAREMIND_VM_PROCESS_TRAPPED) \
    (SHAREMIND_VM_PROCESS_FINISHED) \
    (SHAREMIND_VM_PROCESS_CRASHED)
SHAREMIND_ENUM_DEFINE(SharemindVmProcessState, SHAREMIND_VM_PROCESS_STATE_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindVmProcessState);

#define SHAREMIND_VM_ERROR_ENUM \
    ((SHAREMIND_VM_OK, = 0)) \
    ((SHAREMIND_VM_OUT_OF_MEMORY,)) \
    ((SHAREMIND_VM_IMPLEMENTATION_LIMITS_REACHED,)) \
    ((SHAREMIND_VM_LOCK_FAILURE,)) \
    ((SHAREMIND_VM_INVALID_INPUT_STATE,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_INSTRUCTION,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS,)) \
    ((SHAREMIND_VM_PREPARE_UNDEFINED_BIND,)) \
    ((SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND,)) \
    ((SHAREMIND_VM_PREPARE_DUPLICATE_PDBIND,)) \
    ((SHAREMIND_VM_PDPI_STARTUP_FAILED,)) \
    ((SHAREMIND_VM_RUNTIME_EXCEPTION,)) \
    ((SHAREMIND_VM_RUNTIME_TRAP,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SharemindVmError, SHAREMIND_VM_ERROR_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindVmError);

#define SHAREMIND_VM_PROCESS_EXCEPTION_ENUM \
    ((SHAREMIND_VM_PROCESS_OK, = 0x00)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_MEMORY, = 0x01)) \
    ((SHAREMIND_VM_PROCESS_INVALID_ARGUMENT, = 0x02)) \
    ((SHAREMIND_VM_PROCESS_INVALID_SYSCALL_INVOCATION, = 0x03)) \
    ((SHAREMIND_VM_PROCESS_SHAREMIND_ERROR_IN_SYSCALL, = 0x04)) \
    ((SHAREMIND_VM_PROCESS_MODULE_ERROR_IN_SYSCALL, = 0x05)) \
    ((SHAREMIND_VM_PROCESS_GENERAL_SYSCALL_FAILURE, = 0x06)) \
    ((SHAREMIND_VM_PROCESS_UNKNOWN_SYSCALL_RETURN_CODE, = 0x07)) \
    ((SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER, = 0x100)) \
    ((SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK, = 0x101)) \
    ((SHAREMIND_VM_PROCESS_INVALID_INDEX_REFERENCE, = 0x102)) \
    ((SHAREMIND_VM_PROCESS_INVALID_INDEX_CONST_REFERENCE, = 0x103)) \
    ((SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS, = 0x200)) \
    ((SHAREMIND_VM_PROCESS_INVALID_INDEX_SYSCALL, = 0x201)) \
    ((SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE, = 0x300)) \
    ((SHAREMIND_VM_PROCESS_MEMORY_IN_USE, = 0x301)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_READ, = 0x302)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_WRITE, = 0x303)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_REFERENCE_INDEX, = 0x304)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_REFERENCE_SIZE, = 0x305)) \
    ((SHAREMIND_VM_PROCESS_READ_DENIED, = 0x306)) \
    ((SHAREMIND_VM_PROCESS_WRITE_DENIED, = 0x307)) \
    ((SHAREMIND_VM_PROCESS_UNKNOWN_FPE, = 0x400)) \
    ((SHAREMIND_VM_PROCESS_INTEGER_DIVIDE_BY_ZERO, = 0x401)) \
    ((SHAREMIND_VM_PROCESS_INTEGER_OVERFLOW, = 0x402)) \
    ((SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO, = 0x403)) \
    ((SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW, = 0x404)) \
    ((SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW, = 0x405)) \
    ((SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT, = 0x406)) \
    ((SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION, = 0x407)) \
    ((SHAREMIND_VM_PROCESS_USER_ASSERT, = 0xf00)) \
    ((SHAREMIND_VM_PROCESS_EXCEPTION_COUNT,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SharemindVmProcessException,
                             SHAREMIND_VM_PROCESS_EXCEPTION_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindVmProcessException);

/**
 * \brief Allocates and initializes a new SharemindProgram instance.
 * \param vm The SharemindVm instance under which to run.
 * \param overrides If not NULL, may provide overrides for SharemindVm context.
 * \returns a pointer to the new SharemindProgram instance.
 * \retval NULL if allocation failed.
 */
SharemindProgram * SharemindProgram_new(
        SharemindVm * vm,
        SharemindVirtualMachineContext * overrides)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Deallocates a SharemindProgram instance.
 * \param program pointer to the SharemindProgram instance to free.
 */
void SharemindProgram_free(SharemindProgram * program)
        __attribute__ ((nonnull(1)));

struct SharemindProgramLoadResult_ {

    /** The error code. */
    SharemindVmError error;

    /**
      Position of the error of NULL if the error is not related to an exact
      position of the data passed. In case of SHAREMIND_VM_PREPARE_*BIND values
      this refers to the bind name.
    */
    const void * position;

};
typedef struct SharemindProgramLoadResult_ SharemindProgramLoadResult;

/**
 * \brief Loads the program sections from the given data.
 * \param program pointer to the SharemindProgram to initialize.
 * \param[in] data pointer to the sharemind executable file data.
 * \param[in] dataSize size of the data pointed to by the data parameter, in
 *                     bytes.
 * \returns an SharemindVmError.
 */
SharemindProgramLoadResult SharemindProgram_load_from_sme(
        SharemindProgram * program,
        const void * data,
        size_t dataSize)
        __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \param[in] program pointer to the SharemindProgram instance.
 * \returns whether the SharemindProgram instance is ready to be loaded by
 *          processes.
 */
bool SharemindProgram_is_ready(const SharemindProgram * program)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] program pointer to the SharemindProgram instance.
 * \param[in] codeSection the index of the code section.
 * \param[in] instructionIndex the index of instruction (not block) in the code
 *                             section.
 * \returns a pointer to the description of the instruction in the code at the
 *          given position or NULL if unsuccessful.
 */
const SharemindVmInstruction * SharemindProgram_get_instruction(
        const SharemindProgram * program,
        size_t codeSection,
        size_t instructionIndex) __attribute__ ((nonnull(1)));

/**
 * \brief Allocates and initializes a new SharemindProcess instance.
 * \pre The SharemindProgram instance must be ready.
 * \param program pointer to the SharemindProgram instance to create a process
 *        of.
 * \returns a pointer to the new SharemindProcess instance.
 * \retval NULL if allocation failed.
 */
SharemindProcess * SharemindProcess_new(SharemindProgram * program)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Deallocates a SharemindProcess instance.
 * \param process pointer to the SharemindProcess instance to free.
 */
void SharemindProcess_free(SharemindProcess * process)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] process pointer to the SharemindProcess instance.
 * \returns the number of protection domain process instances for this process.
 */
size_t SharemindProcess_get_pdpi_count(const SharemindProcess * process)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] process pointer to the SharemindProcess instance.
 * \param[in] pdpiIndex The index of the PDPI.
 * \returns the a protection domain process instance for the given index.
 * \retval NULL if no such PDPI exists.
 */
SharemindPdpi * SharemindProcess_get_pdpi(
        const SharemindProcess * process,
        size_t pdpiIndex) __attribute__ ((nonnull(1)));

/**
 * \brief Sets a PDPI facility for all the PDPI's of the given process.
 * \param[in] process pointer to the SharemindProcess instance.
 * \param[in] name The name of the facility to set.
 * \param[in] facility The facility to set.
 * \param[in] context The facility context to set.
 * \warning This destructively overwrites any previously set PDPI facility in
            the respective SharemindPdpi instance.
 * \warning When an out of memory error is encountered this procedure provides
            no rollback. This means that some associated PDPI's might have the
            facility set and some have not.
 * \returns whether setting the facility succeeded.
 * \retval false Out of memory.
 */
bool SharemindProcess_set_pdpi_facility(
        SharemindProcess * process,
        const char * name,
        void * facility,
        void * context) __attribute__ ((nonnull(1, 2)));

/**
 * \brief Sets the process_internal field for all SharemindSyscallContext
 *        structs.
 */
void SharemindProcess_set_process_internal(
        SharemindProcess * process,
        void * value) __attribute__ ((nonnull(1)));

/**
 * \brief Starts execution of the given program in the background.
 * \pre program->state == SHAREMIND_PREPARED
 * \param process The process to run.
 * \returns an SharemindVmError.
 */
SharemindVmError SharemindProcess_run(SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Continues execution of the given program in the background.
 * \pre process->state == SHAREMIND_TRAPPED
 * \param process The process to continue.
 * \returns an SharemindVmError.
 */
SharemindVmError SharemindProcess_continue(SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Pauses the execution of the given program running in the background.
 *
 * Execution is stopped before executing any jump or call instruction.
 *
 * \pre process->state == SHAREMIND_RUNNING
 * \post process->state == SHAREMIND_TRAPPED
 *       || process->state == SHAREMIND_VM_PROCESS_FINISHED
 *       || process->state == SHAREMIND_VM_PROCESS_CRASHED
 * \param process The process to pause.
 * \returns an SharemindVmError.
 */
SharemindVmError SharemindProcess_pause(SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the return value of the program.
 */
int64_t SharemindProcess_get_return_value(const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the exception value of the program.
 */
SharemindVmProcessException SharemindProcess_get_exception(
        const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the current code section of the program.
 */
size_t SharemindProcess_get_current_code_section(
        const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the current instruction pointer of the program.
 */
uintptr_t SharemindProcess_get_current_ip(const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBSHAREMIND_VM_H */
