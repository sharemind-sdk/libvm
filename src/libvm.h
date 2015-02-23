/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#ifndef SHAREMIND_LIBVM_H
#define SHAREMIND_LIBVM_H

#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/preprocessor.h>
#include <sharemind/static_assert.h>
#include <sharemind/tag.h>
#include <stdbool.h>
#include <stdio.h>


SHAREMIND_EXTERN_C_BEGIN


/*******************************************************************************
  Helper macros
*******************************************************************************/

#define SHAREMIND_LIBVM_DECLARE_ERROR_FUNCTIONS(ClassName) \
    SHAREMIND_LASTERROR_PUBLIC_FUNCTIONS_DECLARE(ClassName,, SharemindVmError,)

/*******************************************************************************
  Forward declarations
*******************************************************************************/

struct SharemindVirtualMachineContext_;
typedef struct SharemindVirtualMachineContext_ SharemindVirtualMachineContext;
struct SharemindVm_;
typedef struct SharemindVm_ SharemindVm;
struct SharemindProgram_;
typedef struct SharemindProgram_ SharemindProgram;
struct SharemindProcess_;
typedef struct SharemindProcess_ SharemindProcess;


/*******************************************************************************
  Types
*******************************************************************************/

SHAREMIND_STATIC_ASSERT(sizeof(size_t) >= sizeof(uint16_t));

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
    ((SHAREMIND_VM_MUTEX_ERROR,)) \
    ((SHAREMIND_VM_INVALID_INPUT_STATE,)) \
    ((SHAREMIND_VM_IO_ERROR,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,= 100)) \
    ((SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_INSTRUCTION,)) \
    ((SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS,)) \
    ((SHAREMIND_VM_PREPARE_UNDEFINED_BIND,)) \
    ((SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND,)) \
    ((SHAREMIND_VM_PREPARE_DUPLICATE_PDBIND,)) \
    ((SHAREMIND_VM_PDPI_STARTUP_FAILED,= 200)) \
    ((SHAREMIND_VM_RUNTIME_EXCEPTION,)) \
    ((SHAREMIND_VM_RUNTIME_TRAP,))
SHAREMIND_ENUM_CUSTOM_DEFINE(SharemindVmError, SHAREMIND_VM_ERROR_ENUM);
SHAREMIND_ENUM_DECLARE_TOSTRING(SharemindVmError);

#define SHAREMIND_VM_PROCESS_EXCEPTION_ENUM \
    ((SHAREMIND_VM_PROCESS_OK, = 0x00)) \
    ((SHAREMIND_VM_PROCESS_OUT_OF_MEMORY, = 0x01)) \
    ((SHAREMIND_VM_PROCESS_INVALID_ARGUMENT, = 0x02)) \
    ((SHAREMIND_VM_PROCESS_SYSCALL_ERROR, = 0x10)) \
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


/*******************************************************************************
  SharemindVirtualMachineContext
*******************************************************************************/

struct SharemindVirtualMachineContext_ {

    /** Pointer to any SharemindVirtualMachineContext data. Not used by libvm.*/
    void * internal;

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
    SharemindSyscallWrapper (*find_syscall)(
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

};


/*******************************************************************************
  SharemindVm
*******************************************************************************/

SharemindVm * SharemindVm_new(SharemindVirtualMachineContext * context,
                              SharemindVmError * error,
                              const char ** errorStr);
void SharemindVm_free(SharemindVm * vm);

SHAREMIND_LIBVM_DECLARE_ERROR_FUNCTIONS(SharemindVm)
SHAREMIND_TAG_FUNCTIONS_DECLARE(SharemindVm,,)

/**
 * \brief Allocates and initializes a new SharemindProgram instance.
 * \param vm The SharemindVm instance under which to run.
 * \param overrides If not NULL, may provide overrides for SharemindVm context.
 * \returns a pointer to the new SharemindProgram instance.
 * \retval NULL if allocation failed.
 */
SharemindProgram * SharemindVm_newProgram(
        SharemindVm * vm,
        SharemindVirtualMachineContext * overrides)
        __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
  SharemindProgram
*******************************************************************************/

/**
 * \brief Deallocates a SharemindProgram instance.
 * \param program pointer to the SharemindProgram instance to free.
 */
void SharemindProgram_free(SharemindProgram * program)
        __attribute__ ((nonnull(1)));

SHAREMIND_LIBVM_DECLARE_ERROR_FUNCTIONS(SharemindProgram)
SHAREMIND_TAG_FUNCTIONS_DECLARE(SharemindProgram,,)

SharemindVm * SharemindProgram_vm(const SharemindProgram * program)
        __attribute__ ((nonnull(1)));

/**
 * \brief Loads the program sections from the given file.
 * \param program pointer to the SharemindProgram to initialize.
 * \param[in] filename the name of the file to load the program from.
 * \returns a SharemindVmError.
 */
SharemindVmError SharemindProgram_loadFromFile(SharemindProgram * program,
                                               const char * filename)
        __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Loads the program sections from the given C FILE pointer.
 * \param program pointer to the SharemindProgram to initialize.
 * \param[in] file The C FILE pointer to read the file from (not rewinded).
 * \returns a SharemindVmError.
 */
SharemindVmError SharemindProgram_loadFromCFile(SharemindProgram * program,
                                                FILE * file)
        __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Loads the program sections from the given file descriptor.
 * \param program pointer to the SharemindProgram to initialize.
 * \param[in] fd The file descriptor to read the file from (not rewinded).
 * \returns a SharemindVmError.
 */
SharemindVmError SharemindProgram_loadFromFileDescriptor(
        SharemindProgram * program,
        int fd)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \brief Loads the program sections from the given data.
 * \param program pointer to the SharemindProgram to initialize.
 * \param[in] data pointer to the sharemind executable file data.
 * \param[in] dataSize size of the data pointed to by the data parameter, in
 *                     bytes.
 * \returns a SharemindVmError.
 */
SharemindVmError SharemindProgram_loadFromMemory(SharemindProgram * program,
                                                 const void * data,
                                                 size_t dataSize)
        __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \param[in] program pointer to the SharemindProgram instance.
 * \returns a pointer to the position where parsing last failed or NULL if not
 *          applicable.
 */
const void * SharemindProgram_lastParsePosition(
        const SharemindProgram * program) __attribute__ ((nonnull(1)));

/**
 * \param[in] program pointer to the SharemindProgram instance.
 * \returns whether the SharemindProgram instance is ready to be loaded by
 *          processes.
 */
bool SharemindProgram_isReady(const SharemindProgram * program)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] program pointer to the SharemindProgram instance.
 * \param[in] codeSection the index of the code section.
 * \param[in] instructionIndex the index of instruction (not block) in the code
 *                             section.
 * \returns a pointer to the description of the instruction in the code at the
 *          given position or NULL if unsuccessful.
 */
const SharemindVmInstruction * SharemindProgram_instruction(
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
SharemindProcess * SharemindProgram_newProcess(SharemindProgram * program)
        __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
  SharemindProcess
*******************************************************************************/

/**
 * \brief Deallocates a SharemindProcess instance.
 * \param process pointer to the SharemindProcess instance to free.
 */
void SharemindProcess_free(SharemindProcess * process)
        __attribute__ ((nonnull(1)));

SHAREMIND_LIBVM_DECLARE_ERROR_FUNCTIONS(SharemindProcess)
SHAREMIND_TAG_FUNCTIONS_DECLARE(SharemindProcess,,)

SharemindProgram * SharemindProcess_program(const SharemindProcess * process)
        __attribute__ ((nonnull(1)));

SharemindVm * SharemindProcess_vm(const SharemindProcess * process)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] process pointer to the SharemindProcess instance.
 * \returns the number of protection domain process instances for this process.
 */
size_t SharemindProcess_pdpiCount(const SharemindProcess * process)
        __attribute__ ((nonnull(1)));

/**
 * \param[in] process pointer to the SharemindProcess instance.
 * \param[in] pdpiIndex The index of the PDPI.
 * \returns the a protection domain process instance for the given index.
 * \retval NULL if no such PDPI exists.
 */
SharemindPdpi * SharemindProcess_pdpi(
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
 * \returns an error, if any.
 */
SharemindVmError SharemindProcess_setPdpiFacility(
        SharemindProcess * process,
        const char * name,
        void * facility,
        void * context) __attribute__ ((nonnull(1, 2)));

/**
 * \brief Sets the process_internal field for all SharemindSyscallContext
 *        structs.
 */
void SharemindProcess_setInternal(SharemindProcess * process,
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
 * Execution is stopped immediately after executing any jump, call, return or
 * system call instruction, or before running any instructions.
 *
 * \pre process->state == SHAREMIND_RUNNING
 * \post process->state == SHAREMIND_TRAPPED
 *       || process->state == SHAREMIND_VM_PROCESS_FINISHED
 *       || process->state == SHAREMIND_VM_PROCESS_CRASHED
 * \param process The process to pause.
 */
void SharemindProcess_pause(SharemindProcess * process)
        __attribute__ ((nonnull(1)));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the return value of the program.
 */
int64_t SharemindProcess_returnValue(const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the exception value of the process.
 */
SharemindVmProcessException SharemindProcess_exception(
        const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns a pointer to the exception value of the last failed syscall.
 * \warning the pointed memory might be undefined if no syscall has failed.
 */
void const * SharemindProcess_syscallException(
        const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the current code section of the program.
 */
size_t SharemindProcess_currentCodeSection(
        const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));

/**
 * \param process pointer to the SharemindProcess instance.
 * \returns the current instruction pointer of the program.
 */
uintptr_t SharemindProcess_currentIp(const SharemindProcess * process)
        __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
  Clean up helper macros
*******************************************************************************/

#undef SHAREMIND_LIBVM_DECLARE_ERROR_FUNCTIONS

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_H */
