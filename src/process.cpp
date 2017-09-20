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

#include "process.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <sharemind/comma.h>
#include "core.h"
#include "program.h"


/*******************************************************************************
 *  Public enum methods
*******************************************************************************/

SHAREMIND_ENUM_DEFINE_TOSTRING(SharemindVmProcessState,
                               SHAREMIND_VM_PROCESS_STATE_ENUM)
SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmProcessException,
                                      SHAREMIND_VM_PROCESS_EXCEPTION_ENUM)


/*******************************************************************************
 *  Forward declarations:
*******************************************************************************/

static uint64_t sharemind_public_alloc(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t nBytes) __attribute__ ((nonnull(1), warn_unused_result));
static bool sharemind_public_free(SharemindModuleApi0x1SyscallContext * c,
                                  uint64_t ptr) __attribute__ ((nonnull(1)));
static size_t sharemind_public_get_size(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t ptr) __attribute__ ((nonnull(1)));
static void * sharemind_public_get_ptr(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t ptr) __attribute__ ((nonnull(1)));

static void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c,
                                      size_t nBytes)
        __attribute__ ((nonnull(1), warn_unused_result));
static void sharemind_private_free(SharemindModuleApi0x1SyscallContext * c,
                                   void * ptr) __attribute__ ((nonnull(1)));
static bool sharemind_private_reserve(
        SharemindModuleApi0x1SyscallContext * c,
        size_t nBytes) __attribute__ ((nonnull(1)));
static bool sharemind_private_release(
        SharemindModuleApi0x1SyscallContext * c,
        size_t nBytes) __attribute__ ((nonnull(1), warn_unused_result));

static SharemindModuleApi0x1PdpiInfo const * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index) __attribute__ ((nonnull(1)));

static void * sharemind_processFacility(
        SharemindModuleApi0x1SyscallContext const * c,
        char const * facilityName) __attribute__ ((nonnull(1, 2)));



/*******************************************************************************
 *  SharemindProcess:
*******************************************************************************/

static inline bool SharemindProcess_start_pdpis(SharemindProcess * p);
static inline void SharemindProcess_stop_pdpis(SharemindProcess * p);

SharemindProcess * SharemindProgram_newProcess(SharemindProgram * program) {
    assert(program);
    assert(SharemindProgram_isReady(program));

    SharemindProcess * p;
    try {
        p = new SharemindProcess();
    } catch (...) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProcess_new_fail_0;
    }

    p->program = program;

    SharemindProgram_lock(program);
    if (!SHAREMIND_RECURSIVE_LOCK_INIT(p)) {
        SharemindProgram_setErrorMie(program);
        goto SharemindProgram_newProcess_fail_mutex;
    }

    SHAREMIND_LIBVM_LASTERROR_INIT(p);
    SHAREMIND_TAG_INIT(p);

    /* Copy DATA section */
    for (std::size_t i = 0u; i < program->dataSections.size(); i++) {
        auto const & originalSection = program->dataSections[i];
        try {
            p->dataSections.emplace_back(
                        std::make_shared<sharemind::RwDataSection>(
                            originalSection->data(),
                            originalSection->size()));
        } catch (...) {
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_data_sections;
        }
    }
    assert(p->dataSections.size() == program->dataSections.size());

    /* Initialize BSS sections */
    for (std::size_t i = 0u; i < program->bssSectionSizes.size(); i++) {
        try {
            static_assert(sizeof(program->bssSectionSizes[i])
                          <= sizeof(std::size_t), "");
            p->bssSections.emplace_back(
                        std::make_shared<sharemind::BssDataSection>(
                            program->bssSectionSizes[i]));
        } catch (...) {
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_bss_sections;
        }
    }
    assert(p->bssSections.size() == program->bssSectionSizes.size());


    /* Initialize PDPI cache: */
    try {
        p->pdpiCache.reinitialize(program->pdBindings);
    } catch (...) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProgram_newProcess_fail_pdpiCache;
    }

    /* Set currentCodeSectionIndex before initializing memory slots: */
    p->currentCodeSectionIndex = program->activeLinkingUnit;
    p->currentIp = 0u;

    p->trapCond = false;

    // This state replicates default AMD64 behaviour.
    // NB! By default, we ignore any fpu exceptions.
    p->fpuState = (sf_fpu_state) (sf_float_tininess_after_rounding |
                                  sf_float_round_nearest_even);

    /* Initialize section pointers: */
    try {
        p->memoryMap.insertDataSection(
                    1u,
                    p->program->rodataSections[p->currentCodeSectionIndex]);
        p->memoryMap.insertDataSection(
                    2u,
                    p->dataSections[p->currentCodeSectionIndex]);
        p->memoryMap.insertDataSection(
                    3u,
                    p->bssSections[p->currentCodeSectionIndex]);
    } catch (...) {
        goto SharemindProgram_newProcess_fail_memslots;
    }

    p->syscallContext.get_pdpi_info = &sharemind_get_pdpi_info;
    p->syscallContext.processFacility = &sharemind_processFacility;
    p->syscallContext.publicAlloc = &sharemind_public_alloc;
    p->syscallContext.publicFree = &sharemind_public_free;
    p->syscallContext.publicMemPtrSize = &sharemind_public_get_size;
    p->syscallContext.publicMemPtrData = &sharemind_public_get_ptr;
    p->syscallContext.allocPrivate = &sharemind_private_alloc;
    p->syscallContext.freePrivate = &sharemind_private_free;
    p->syscallContext.reservePrivate = &sharemind_private_reserve;
    p->syscallContext.releasePrivate = &sharemind_private_release;
    p->syscallContext.vm_internal = p;
    p->syscallContext.process_internal = nullptr;

    try {
        SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(*p, *program);
    } catch (...) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProgram_newProcess_fail_framestack;
    }

    /* Initialize the frame stack */
    try {
        p->frames.emplace_back();
    } catch (...) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProgram_newProcess_fail_framestack;
    }

    /* Initialize global frame: */
    p->globalFrame = &p->frames.back();
    p->globalFrame->returnAddr = nullptr; /* Triggers halt on return. */
    /* p->globalFrame->returnValueAddr = &(p->returnValue); is not needed. */
    p->thisFrame = p->globalFrame;
    p->nextFrame = nullptr;

    p->state = SHAREMIND_VM_PROCESS_INITIALIZED;

    SharemindProgram_unlock(program);

    return p;

SharemindProgram_newProcess_fail_framestack:

    p->frames.clear();

SharemindProgram_newProcess_fail_memslots:
SharemindProgram_newProcess_fail_pdpiCache:

    p->pdpiCache.clear();

SharemindProgram_newProcess_fail_bss_sections:

    p->bssSections.clear();

SharemindProgram_newProcess_fail_data_sections:

    p->dataSections.clear();
    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

SharemindProgram_newProcess_fail_mutex:

    SharemindProgram_unlock(program);
    delete p;

SharemindProcess_new_fail_0:

    return nullptr;
}

void SharemindProcess_free(SharemindProcess * p) {
    assert(p);
    assert(p->state != SHAREMIND_VM_PROCESS_RUNNING);
    if (p->state == SHAREMIND_VM_PROCESS_TRAPPED)
        SharemindProcess_stop_pdpis(p);

    p->frames.clear();
    p->pdpiCache.clear();

    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);
    delete p;
}

SharemindProgram * SharemindProcess_program(SharemindProcess const * const p) {
    // No locking, program is const after SharemindProcess_new():
    return p->program;
}

SharemindVm * SharemindProcess_vm(SharemindProcess const * const p) {
    // No locking, program is const after SharemindProcess_new():
    return SharemindProgram_vm(p->program);
}

size_t SharemindProcess_pdpiCount(SharemindProcess const * process) {
    assert(process);
    // No locking, program is const after SharemindProcess_new():
    return process->program->pdBindings.size();
}

SharemindPdpi * SharemindProcess_pdpi(SharemindProcess const * process,
                                      size_t pdpiIndex)
{
    assert(process);
    SharemindProcess_lockConst(process);
    auto * const r = process->pdpiCache.pdpi(pdpiIndex);
    SharemindProcess_unlockConst(process);
    return r;
}

SharemindVmError SharemindProcess_setPdpiFacility(
        SharemindProcess * const process,
        char const * const name,
        void * const facility,
        void * const context)
{
    assert(process);
    assert(name);
    SharemindProcess_lock(process);
    auto const numPdpis(process->pdpiCache.size());
    auto const & cache = process->pdpiCache;
    for (std::size_t i = 0u; i < numPdpis; i++)
        if (SharemindPdpi_setFacility(cache.pdpi(i),
                                      name,
                                      facility,
                                      context) != SHAREMIND_MODULE_API_OK)
        {
            SharemindProcess_setErrorOom(process);
            SharemindProcess_unlock(process);
            return SHAREMIND_VM_OUT_OF_MEMORY;
        }
    SharemindProcess_unlock(process);
    return SHAREMIND_VM_OK;
}

void SharemindProcess_setInternal(SharemindProcess * const process,
                                  void * const value)
{
    assert(process);
    SharemindProcess_lock(process);
    process->syscallContext.process_internal = value;
    SharemindProcess_unlock(process);
}

static inline bool SharemindProcess_start_pdpis(SharemindProcess * p) {
    assert(p);
    try {
        p->pdpiCache.startPdpis();
        return true;
    } catch (...) {
        return false;
    }
}

static inline void SharemindProcess_stop_pdpis(SharemindProcess * p) {
    assert(p);
    p->pdpiCache.stopPdpis();
}

SharemindVmError SharemindProcess_run(SharemindProcess * p) {
    assert(p);
    SharemindProcess_lock(p);
    /** \todo Add support for continue/restart */
    if (unlikely(p->state != SHAREMIND_VM_PROCESS_INITIALIZED)) {
        SharemindProcess_setError(p,
                                  SHAREMIND_VM_INVALID_INPUT_STATE,
                                  "Process already initialized!");
        SharemindProcess_unlock(p);
        return SHAREMIND_VM_INVALID_INPUT_STATE;
    }

    if (unlikely(!SharemindProcess_start_pdpis(p))) {
        SharemindProcess_setError(p,
                                  SHAREMIND_VM_PDPI_STARTUP_FAILED,
                                  "Failed to start PDPIs!");
        SharemindProcess_unlock(p);
        return SHAREMIND_VM_PDPI_STARTUP_FAILED;
    }

    p->state = SHAREMIND_VM_PROCESS_RUNNING;
    SharemindVmError const e = sharemind_vm_run(p, SHAREMIND_I_RUN, nullptr);
    if (e != SHAREMIND_VM_RUNTIME_TRAP)
        SharemindProcess_stop_pdpis(p);
    SharemindProcess_unlock(p);
    return e;
}

SharemindVmError SharemindProcess_continue(SharemindProcess * p) {
    assert(p);
    SharemindProcess_lock(p);
    /** \todo Add support for continue/restart */
    if (unlikely(p->state != SHAREMIND_VM_PROCESS_TRAPPED)) {
        SharemindProcess_setError(p,
                                  SHAREMIND_VM_INVALID_INPUT_STATE,
                                  "Process not in trapped state!");
        SharemindProcess_unlock(p);
        return SHAREMIND_VM_INVALID_INPUT_STATE;
    }

    SharemindVmError const e =
            sharemind_vm_run(p, SHAREMIND_I_CONTINUE, nullptr);
    if (e != SHAREMIND_VM_RUNTIME_TRAP)
        SharemindProcess_stop_pdpis(p);
    SharemindProcess_unlock(p);
    return e;
}

void SharemindProcess_pause(SharemindProcess * p) {
    assert(p);
    __atomic_store_n(&p->trapCond, true, __ATOMIC_RELEASE);
}

int64_t SharemindProcess_returnValue(SharemindProcess const * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    int64_t const r = p->returnValue.int64[0];
    SharemindProcess_unlockConst(p);
    return r;
}

SharemindVmProcessException SharemindProcess_exception(
        SharemindProcess const * p)
{
    assert(p);
    SharemindProcess_lockConst(p);
    assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
    SharemindVmProcessException const r =
            (SharemindVmProcessException) p->exceptionValue;
    SharemindProcess_unlockConst(p);
    assert(SharemindVmProcessException_toString(r));
    return r;
}

void const * SharemindProcess_syscallException(SharemindProcess const * p) {
    assert(p);
    return &p->syscallException;
}

size_t SharemindProcess_currentCodeSection(SharemindProcess const * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    size_t const r = p->currentCodeSectionIndex;
    SharemindProcess_unlockConst(p);
    return r;
}

uintptr_t SharemindProcess_currentIp(SharemindProcess const * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    uintptr_t const r = p->currentIp;
    SharemindProcess_unlockConst(p);
    return r;
}

std::uint64_t SharemindProcess_public_alloc(SharemindProcess * const p,
                                            std::uint64_t const nBytes)
{
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPublicHeap.upperLimit - p->memPublicHeap.usage
                     < nBytes)))
        return 0u;

    try {
        auto const index(p->memoryMap.allocate(nBytes));

        /* Update memory statistics: */
        p->memPublicHeap.usage += nBytes;
        p->memTotal.usage += nBytes;
        if (p->memPublicHeap.usage > p->memPublicHeap.max)
            p->memPublicHeap.max = p->memPublicHeap.usage;
        if (p->memTotal.usage > p->memTotal.max)
            p->memTotal.max = p->memTotal.usage;

        return index;
    } catch (...) {
        return 0u;
    }
}

SharemindVmProcessException SharemindProcess_public_free(
        SharemindProcess * const p,
        uint64_t const ptr)
{
    assert(p);
    auto const r(p->memoryMap.free(ptr));
    if (r.first == SHAREMIND_VM_PROCESS_OK) {
        /* Update memory statistics: */
        assert(p->memPublicHeap.usage >= r.second);
        assert(p->memTotal.usage >= r.second);
        p->memPublicHeap.usage -= r.second;
        p->memTotal.usage -= r.second;
    }
    return r.first;
}

/*******************************************************************************
 *  Other procedures:
*******************************************************************************/

static uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);
    return SharemindProcess_public_alloc(p, nBytes);
}

static bool sharemind_public_free(SharemindModuleApi0x1SyscallContext * c,
                                  uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    SharemindVmProcessException const r = SharemindProcess_public_free(p, ptr);
    assert(r == SHAREMIND_VM_PROCESS_OK
           || r == SHAREMIND_VM_PROCESS_MEMORY_IN_USE
           || r == SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE);
    return r != SHAREMIND_VM_PROCESS_MEMORY_IN_USE;
}

static size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c,
                                        uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(ptr == 0u))
        return 0u;

    SharemindProcess const * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    auto const & v = p->memoryMap.get(ptr);
    return v ? v->size() : 0u;
}

static void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(ptr == 0u))
        return nullptr;

    SharemindProcess const * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    auto const & v = p->memoryMap.get(ptr);
    return v ? v->data() : nullptr;
}

static void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c,
                                      size_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(nBytes == 0))
        return nullptr;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPrivate.upperLimit - p->memPrivate.usage < nBytes)))
        return nullptr;

    /** \todo Check any other memory limits? */

    /* Allocate the memory: */
    try {
        auto const ptr = p->privateMemoryMap.allocate(nBytes);

        /* Update memory statistics: */
        p->memPrivate.usage += nBytes;
        p->memTotal.usage += nBytes;
        if (p->memPrivate.usage > p->memPrivate.max)
            p->memPrivate.max = p->memPrivate.usage;
        if (p->memTotal.usage > p->memTotal.max)
            p->memTotal.max = p->memTotal.usage;

        return ptr;
    } catch (...) {
        return nullptr;
    }
}

static void sharemind_private_free(SharemindModuleApi0x1SyscallContext * c,
                                   void * ptr)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(!ptr))
        return;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    if (auto const bytesDeleted = p->privateMemoryMap.free(ptr)) {
        /* Update memory statistics: */
        assert(p->memPrivate.usage >= bytesDeleted);
        assert(p->memTotal.usage >= bytesDeleted);
        p->memPrivate.usage -= bytesDeleted;
        p->memTotal.usage -= bytesDeleted;
    }
}

static bool sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c,
                                      size_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(nBytes == 0u))
        return true;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memReserved.upperLimit - p->memReserved.usage
                     < nBytes)))
        return false;

    /* Update memory statistics */
    p->memReserved.usage += nBytes;
    p->memTotal.usage += nBytes;
    if (p->memReserved.usage > p->memReserved.max)
        p->memReserved.max = p->memReserved.usage;
    if (p->memTotal.usage > p->memTotal.max)
        p->memTotal.max = p->memTotal.usage;
    return true;
}

static bool sharemind_private_release(SharemindModuleApi0x1SyscallContext * c,
                                      size_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(nBytes == 0u))
        return true;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    /* Check for underflow: */
    if (p->memReserved.usage < nBytes)
        return false;

    assert(p->memTotal.usage >= nBytes);
    p->memReserved.usage -= nBytes;
    p->memTotal.usage -= nBytes;
    return true;
}

static SharemindModuleApi0x1PdpiInfo const * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index)
{
    assert(c);
    assert(c->vm_internal);

    if (pd_index > SIZE_MAX)
        return nullptr;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);
    return p->pdpiCache.info(pd_index);
}

static void * sharemind_processFacility(
        SharemindModuleApi0x1SyscallContext const * c,
        char const * facilityName)
{
    assert(c);
    assert(c->vm_internal);
    assert(facilityName);

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);
    return SharemindProcess_facility(p, facilityName);
}

SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindProcess)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindProcess,)
SHAREMIND_DEFINE_PROCESSFACILITYMAP_SELF_ACCESSORS(SharemindProcess)
