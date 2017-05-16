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

#include <assert.h>
#include <limits.h>
#include <sharemind/comma.h>
#include <stdio.h>
#include <string.h>
#include "core.h"
#include "rwdataspecials.h"
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

static inline uint64_t SharemindProcess_public_alloc_slot(
        SharemindProcess * const p,
        SharemindMemorySlot ** const memorySlot)
        __attribute__ ((nonnull(1, 2), warn_unused_result));

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

static inline void SharemindProcess_destroy(SharemindProcess * p);
static inline bool SharemindProcess_start_pdpis(SharemindProcess * p);
static inline void SharemindProcess_stop_pdpis(SharemindProcess * p);

SharemindProcess * SharemindProgram_newProcess(SharemindProgram * program) {
    assert(program);
    assert(SharemindProgram_isReady(program));

    SharemindProcess * const p =
            (SharemindProcess *) malloc(sizeof(SharemindProcess));
    if (unlikely(!p)) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProcess_new_fail_0;
    }

    p->program = program;

    SharemindProgram_lock(program);
    if (unlikely(!SharemindProgram_refs_ref(program))) {
        SharemindProgram_setErrorOor(program);
        goto SharemindProgram_newProcess_error_fail_ref;
    }

    if (!SHAREMIND_RECURSIVE_LOCK_INIT(p)) {
        SharemindProgram_setErrorMie(program);
        goto SharemindProgram_newProcess_fail_mutex;
    }

    SHAREMIND_LIBVM_LASTERROR_INIT(p);
    SHAREMIND_TAG_INIT(p);

    /* Copy DATA section */
    SharemindDataSectionsVector_init(&p->dataSections);
    for (size_t i = 0u; i < program->dataSections.size; i++) {
        SharemindDataSection * const processSection =
                SharemindDataSectionsVector_push(&p->dataSections);
        if (!processSection) {
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_data_sections;
        }
        SharemindDataSection * const originalSection =
                &program->dataSections.data[i];
        if (!SharemindDataSection_init(processSection,
                                       originalSection->size,
                                       &rwDataSpecials))
        {
            SharemindDataSectionsVector_pop(&p->dataSections);
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_data_sections;
        }
        assert(processSection->size == originalSection->size);
        memcpy(processSection->pData,
               originalSection->pData,
               originalSection->size);
    }
    assert(p->dataSections.size == program->dataSections.size);

    /* Initialize BSS sections */
    SharemindDataSectionsVector_init(&p->bssSections);
    for (size_t i = 0u; i < program->bssSectionSizes.size; i++) {
        SharemindDataSection * const bssSection =
                SharemindDataSectionsVector_push(&p->bssSections);
        if (!bssSection) {
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_bss_sections;
        }
        SHAREMIND_STATIC_ASSERT(sizeof(program->bssSectionSizes.data[i])
                                <= sizeof(size_t));
        if (!SharemindDataSection_init(bssSection,
                                       program->bssSectionSizes.data[i],
                                       &rwDataSpecials))
        {
            SharemindDataSectionsVector_pop(&p->bssSections);
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_bss_sections;
        }
        assert(bssSection->size == program->bssSectionSizes.data[i]);
        memset(bssSection->pData, 0, program->bssSectionSizes.data[i]);
    }
    assert(p->bssSections.size == program->bssSectionSizes.size);


    /* Initialize PDPI cache: */
    SharemindPdpiCache_init(&p->pdpiCache);
    for (size_t i = 0u; i < program->pdBindings.size; i++) {
        SharemindPdpiCacheItem * const ci =
                SharemindPdpiCache_push(&p->pdpiCache);
        if (!ci) {
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_pdpiCache;
        }

        if (!SharemindPdpiCacheItem_init(ci, program->pdBindings.data[i])) {
            SharemindPdpiCache_pop(&p->pdpiCache);
            SharemindProgram_setErrorOom(program);
            goto SharemindProgram_newProcess_fail_pdpiCache;
        }
    }
    assert(p->pdpiCache.size == program->pdBindings.size);

    /* Set currentCodeSectionIndex before initializing memory slots: */
    p->currentCodeSectionIndex = program->activeLinkingUnit;
    p->currentIp = 0u;

    p->trapCond = false;

    // This state replicates default AMD64 behaviour.
    // NB! By default, we ignore any fpu exceptions.
    p->fpuState = (sf_fpu_state) (sf_float_tininess_after_rounding |
                                  sf_float_round_nearest_even);


    SharemindMemoryMap_init(&p->memoryMap);
    SharemindPrivateMemoryMap_init(&p->privateMemoryMap);

    /* Initialize section pointers: */

    assert(!SharemindMemoryMap_get(&p->memoryMap, 0u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 1u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 2u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 3u));

#define INIT_STATIC_MEMSLOT(index,pSection,errorLabel) \
    do { \
        assert((pSection)->size > p->currentCodeSectionIndex); \
        SharemindDataSection * const restrict staticSlot = \
                &(pSection)->data[p->currentCodeSectionIndex]; \
        SharemindMemoryMap_value * const restrict v = \
                SharemindMemoryMap_insertNew(&p->memoryMap, (index)); \
        if (unlikely(!v)) { \
            SharemindProgram_setErrorOom(program); \
            goto errorLabel; \
        } \
        v->value = *staticSlot; \
    } while ((0))

    INIT_STATIC_MEMSLOT(1u,
                        &p->program->rodataSections,
                        SharemindProgram_newProcess_fail_firstmemslot);
    INIT_STATIC_MEMSLOT(2u,
                        &p->dataSections,
                        SharemindProgram_newProcess_fail_memslots);
    INIT_STATIC_MEMSLOT(3u,
                        &p->bssSections,
                        SharemindProgram_newProcess_fail_memslots);

    p->memorySlotNext = 4u;

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
    p->syscallContext.process_internal = NULL;

    SharemindProcessFacilityMap_init(&p->facilityMap,
                                     &program->processFacilityMap);

    SharemindMemoryInfo_init(&p->memPublicHeap);
    SharemindMemoryInfo_init(&p->memPrivate);
    SharemindMemoryInfo_init(&p->memReserved);
    SharemindMemoryInfo_init(&p->memTotal);

    /* Initialize the frame stack */
    SharemindFrameStack_init(&p->frames);

    p->globalFrame = SharemindFrameStack_push(&p->frames);
    if (unlikely(!p->globalFrame)) {
        SharemindProgram_setErrorOom(program);
        goto SharemindProgram_newProcess_fail_framestack;
    }


    /* Initialize global frame: */
    SharemindStackFrame_init(p->globalFrame, NULL);
    p->globalFrame->returnAddr = NULL; /* Triggers halt on return. */
    /* p->globalFrame->returnValueAddr = &(p->returnValue); is not needed. */
    p->thisFrame = p->globalFrame;
    p->nextFrame = NULL;

    p->state = SHAREMIND_VM_PROCESS_INITIALIZED;

    SharemindProgram_unlock(program);

    return p;

SharemindProgram_newProcess_fail_framestack:
    /* There is nothing to destroy in SharemindFrameStack, because push of the
     * globalFrame failed */

    SharemindProcessFacilityMap_destroy(&p->facilityMap);

SharemindProgram_newProcess_fail_memslots:

    SharemindMemoryMap_destroy(&p->memoryMap);

SharemindProgram_newProcess_fail_firstmemslot:
SharemindProgram_newProcess_fail_pdpiCache:

    SharemindPdpiCache_destroy(&p->pdpiCache);

SharemindProgram_newProcess_fail_bss_sections:

    SharemindDataSectionsVector_destroy(&p->bssSections);

SharemindProgram_newProcess_fail_data_sections:

    SharemindDataSectionsVector_destroy(&p->dataSections);
    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

SharemindProgram_newProcess_fail_mutex:

    SharemindProgram_refs_unref(program);

SharemindProgram_newProcess_error_fail_ref:

    SharemindProgram_unlock(program);
    free(p);

SharemindProcess_new_fail_0:

    return NULL;
}

static inline void SharemindProcess_destroy(SharemindProcess * p) {
    assert(p);
    assert(p->state != SHAREMIND_VM_PROCESS_RUNNING);
    if (p->state == SHAREMIND_VM_PROCESS_TRAPPED)
        SharemindProcess_stop_pdpis(p);

    SharemindFrameStack_destroy_with(&p->frames, &SharemindStackFrame_destroy);
    SharemindProcessFacilityMap_destroy(&p->facilityMap);
    SharemindPrivateMemoryMap_destroy(&p->privateMemoryMap);
    SharemindMemoryMap_destroy(&p->memoryMap);
    SharemindPdpiCache_destroy(&p->pdpiCache);
    SharemindDataSectionsVector_destroy(&p->bssSections);
    SharemindDataSectionsVector_destroy(&p->dataSections);

    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

    SharemindProgram_refs_unref(p->program);
}

void SharemindProcess_free(SharemindProcess * p) {
    assert(p);
    SharemindProcess_destroy(p);
    free(p);
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
    SharemindProcess_lockConst(process);
    size_t const r = process->pdpiCache.size;
    SharemindProcess_unlockConst(process);
    return r;
}

SharemindPdpi * SharemindProcess_pdpi(SharemindProcess const * process,
                                      size_t pdpiIndex)
{
    assert(process);
    SharemindProcess_lockConst(process);
    SharemindPdpiCache const * const cache = &process->pdpiCache;
    SharemindPdpi * const r = (pdpiIndex < cache->size)
                            ? cache->data[pdpiIndex].pdpi
                            : NULL;
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
    SharemindPdpiCache const * const cache = &process->pdpiCache;
    for (size_t i = 0u; i < cache->size; i++)
        if (SharemindPdpi_setFacility(cache->data[i].pdpi,
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
    SharemindPdpiCache const * const cache = &p->pdpiCache;
    size_t i;
    for (i = 0u; i < cache->size; i++) {
        if (!SharemindPdpiCacheItem_start(&cache->data[i])) {
            while (i)
                SharemindPdpiCacheItem_stop(&cache->data[--i]);
            return false;
        }
    }
    return true;
}

static inline void SharemindProcess_stop_pdpis(SharemindProcess * p) {
    assert(p);
    SharemindPdpiCache const * const cache = &p->pdpiCache;
    size_t i = cache->size;
    while (i)
        SharemindPdpiCacheItem_stop(&cache->data[--i]);
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
    SharemindVmError const e = sharemind_vm_run(p, SHAREMIND_I_RUN, NULL);
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

    SharemindVmError const e = sharemind_vm_run(p, SHAREMIND_I_CONTINUE, NULL);
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
    assert(SharemindVmProcessException_toString(r) != 0);
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

static inline uint64_t SharemindProcess_public_alloc_slot(
        SharemindProcess * const p,
        SharemindMemorySlot ** const memorySlot)
{
    assert(p);
    assert(memorySlot);
    assert(p->memorySlotNext >= 4u);
    assert(p->memoryMap.size < (UINT64_MAX < SIZE_MAX ? UINT64_MAX : SIZE_MAX));

    /* Find a free memory slot: */
    uint64_t const index =
            SharemindMemoryMap_findUnusedPtr(&p->memoryMap, p->memorySlotNext);
    assert(index != 0u);

    /* Fill the slot: */
#ifndef NDEBUG
    size_t const oldSize = p->memoryMap.size;
#endif
    SharemindMemoryMap_value * const v =
            SharemindMemoryMap_insertNew(&p->memoryMap, index);
    if (unlikely(!v))
        return 0u;
    assert(oldSize < p->memoryMap.size);

    /* Optimize next alloc: */
    p->memorySlotNext = index + 1u;
    /* skip "NULL" and static memory pointers: */
    if (unlikely(!p->memorySlotNext))
        p->memorySlotNext = 4u;

    (*memorySlot) = &v->value;

    return index;
}

uint64_t SharemindProcess_public_alloc(SharemindProcess * const p,
                                       uint64_t const nBytes,
                                       SharemindMemorySlot ** const memorySlot)
{
    assert(p);

    /* Fail if allocating too much: */
    if (unlikely(nBytes > SIZE_MAX))
        return 0u;

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPublicHeap.upperLimit - p->memPublicHeap.usage
                     < nBytes)))
        return 0u;

    /** \todo Check any other memory limits? */

    /* Fail if all memory slots are used. */
    SHAREMIND_STATIC_ASSERT(sizeof(uint64_t) >= sizeof(size_t));
    if (unlikely(p->memoryMap.size >= SIZE_MAX))
        return 0u;

    /* Allocate the memory: */
    void * const pData = malloc((size_t) nBytes);
    if (unlikely(!pData && (nBytes != 0u)))
        return 0u;

    SharemindMemorySlot * slot;
    uint64_t const index = SharemindProcess_public_alloc_slot(p, &slot);
    if (unlikely(index == 0u)) {
        free(pData);
        return 0u;
    }
    assert(index >= 4u);

    /* Initialize the slot: */
    SharemindMemorySlot_init(slot, pData, (size_t) nBytes, NULL);

    /* Update memory statistics: */
    p->memPublicHeap.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memPublicHeap.usage > p->memPublicHeap.max)
        p->memPublicHeap.max = p->memPublicHeap.usage;

    if (p->memTotal.usage > p->memTotal.max)
        p->memTotal.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    if (memorySlot)
        (*memorySlot) = slot;

    return index;
}

SharemindVmProcessException SharemindProcess_public_free(
        SharemindProcess * const p,
        uint64_t const ptr)
{
    assert(p);

    /* If ptr is a special value: */
    if (ptr <= 3u)
        return (ptr == 0u)
               ? SHAREMIND_VM_PROCESS_OK
               : SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE;

    SharemindMemoryMap_value * const v =
            SharemindMemoryMap_get(&p->memoryMap, ptr);
    if (unlikely(!v))
        return SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE;

    SharemindMemorySlot * const slot = &v->value;
    if (unlikely(slot->nrefs != 0u))
        return SHAREMIND_VM_PROCESS_MEMORY_IN_USE;


    /* Update memory statistics: */
    assert(p->memPublicHeap.usage >= slot->size);
    assert(p->memTotal.usage >= slot->size);
    p->memPublicHeap.usage -= slot->size;
    p->memTotal.usage -= slot->size;

    /** \todo Update any other memory statistics? */

    /* Deallocate the memory and release the slot: */
    SharemindMemoryMap_remove(&p->memoryMap, ptr);

    return SHAREMIND_VM_PROCESS_OK;
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
    return SharemindProcess_public_alloc(p, nBytes, NULL);
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

    SharemindMemoryMap_value const * const v =
            SharemindMemoryMap_get(&p->memoryMap, ptr);
    if (!v)
        return 0u;

    return v->value.size;
}

static void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(ptr == 0u))
        return NULL;

    SharemindProcess const * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    SharemindMemoryMap_value const * const v =
            SharemindMemoryMap_get(&p->memoryMap, ptr);
    if (!v)
        return NULL;

    return v->value.pData;
}

static void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c,
                                      size_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(nBytes == 0))
        return NULL;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPrivate.upperLimit - p->memPrivate.usage < nBytes)))
        return NULL;

    /** \todo Check any other memory limits? */

    /* Allocate the memory: */
    void * ptr = malloc(nBytes);
    if (unlikely(!ptr))
        return NULL;

    /* Add pointer to private memory map: */
#ifndef NDEBUG
    size_t const oldSize = p->privateMemoryMap.size;
#endif
    SharemindPrivateMemoryMap_value * const v =
            SharemindPrivateMemoryMap_insertNew(&p->privateMemoryMap, ptr);
    if (unlikely(!v)) {
        free(ptr);
        return NULL;
    }
    assert(oldSize < p->privateMemoryMap.size);
    v->value = nBytes;

    /* Update memory statistics: */
    p->memPrivate.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memPrivate.usage > p->memPrivate.max)
        p->memPrivate.max = p->memPrivate.usage;

    if (p->memTotal.usage > p->memTotal.max)
        p->memTotal.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    return ptr;
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

    /* Check if pointer is in private memory map: */
    SharemindPrivateMemoryMap_value * const v =
            SharemindPrivateMemoryMap_get(&p->privateMemoryMap, ptr);
    if (unlikely(!v))
        return;

    /* Get allocated size: */
    size_t const nBytes = v->value;
    assert(nBytes > 0u);

    /* Free the memory: */
    free(ptr);

    /* Remove pointer from valid list: */
    #ifndef NDEBUG
    bool const r =
    #endif
        SharemindPrivateMemoryMap_remove(&p->privateMemoryMap, ptr);
    assert(r);

    /* Update memory statistics: */
    assert(p->memPrivate.usage >= nBytes);
    assert(p->memTotal.usage >= nBytes);
    p->memPrivate.usage -= nBytes;
    p->memTotal.usage -= nBytes;

    /** \todo Update any other memory statistics? */
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

    /** \todo Check any other memory limits? */

    /* Update memory statistics */
    p->memReserved.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memReserved.usage > p->memReserved.max)
        p->memReserved.max = p->memReserved.usage;

    if (p->memTotal.usage > p->memTotal.max)
        p->memTotal.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

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
        return NULL;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    SharemindPdpiCacheItem const * const pdpiCacheItem =
            SharemindPdpiCache_get_const_pointer(&p->pdpiCache, pd_index);
    if (pdpiCacheItem) {
        assert(pdpiCacheItem->info.pdpiHandle);
        return &pdpiCacheItem->info;
    }

    return NULL;
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
    SharemindProcessFacility * const f =
            SharemindProcessFacilityMap_get(&p->facilityMap, facilityName);
    return f ? *f : NULL;
}

SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindProcess)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindProcess,)

SHAREMIND_DEFINE_SELF_FACILITYMAP_ACCESSORS(SharemindProcess)
