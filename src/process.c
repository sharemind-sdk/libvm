/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__

#include "process.h"

#include <assert.h>
#include <limits.h>
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

static const SharemindModuleApi0x1PdpiInfo * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index) __attribute__ ((nonnull(1)));



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

    SharemindFrameStack_init(&p->frames);

    SharemindMemoryMap_init(&p->memoryMap);
    SharemindPrivateMemoryMap_init(&p->privateMemoryMap);

    /* Set currentCodeSectionIndex before initializing memory slots: */
    p->currentCodeSectionIndex = program->activeLinkingUnit;
    p->currentIp = 0u;

    p->fpuState = sf_fpu_state_default;

    /* Initialize section pointers: */

    assert(!SharemindMemoryMap_get(&p->memoryMap, 0u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 1u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 2u));
    assert(!SharemindMemoryMap_get(&p->memoryMap, 3u));

#define INIT_STATIC_MEMSLOT(index,pSection,errorLabel) \
    if (1) { \
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
    } else (void) 0

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

    SharemindMemoryInfo_init(&p->memPublicHeap);
    SharemindMemoryInfo_init(&p->memPrivate);
    SharemindMemoryInfo_init(&p->memReserved);
    SharemindMemoryInfo_init(&p->memTotal);

    p->state = SHAREMIND_VM_PROCESS_INITIALIZED;
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
    SharemindProgram_unlock(program);
    return p;

SharemindProgram_newProcess_fail_framestack:
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

    SharemindPrivateMemoryMap_destroy(&p->privateMemoryMap);
    SharemindMemoryMap_destroy(&p->memoryMap);
    SharemindFrameStack_destroy(&p->frames);
    SharemindPdpiCache_destroy(&p->pdpiCache);
    SharemindDataSectionsVector_destroy(&p->bssSections);
    SharemindDataSectionsVector_destroy(&p->dataSections);

    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

    SharemindProgram_refs_unref(p->program);
}

void SharemindProcess_free(SharemindProcess * p) {
    SharemindProcess_destroy((assert(p), p));
    free(p);
}

SharemindProgram * SharemindProcess_program(const SharemindProcess * const p) {
    // No locking, program is const after SharemindProcess_new():
    return p->program;
}

SharemindVm * SharemindProcess_vm(const SharemindProcess * const p) {
    // No locking, program is const after SharemindProcess_new():
    return SharemindProgram_vm(p->program);
}

size_t SharemindProcess_pdpiCount(const SharemindProcess * process) {
    assert(process);
    SharemindProcess_lockConst(process);
    const size_t r = process->pdpiCache.size;
    SharemindProcess_unlockConst(process);
    return r;
}

SharemindPdpi * SharemindProcess_pdpi(const SharemindProcess * process,
                                      size_t pdpiIndex)
{
    assert(process);
    SharemindProcess_lockConst(process);
    const SharemindPdpiCache * const cache = &process->pdpiCache;
    SharemindPdpi * const r = (pdpiIndex < cache->size)
                            ? cache->data[pdpiIndex].pdpi
                            : NULL;
    SharemindProcess_unlockConst(process);
    return r;
}

SharemindVmError SharemindProcess_setPdpiFacility(
        SharemindProcess * const process,
        const char * const name,
        void * const facility,
        void * const context)
{
    assert(process);
    assert(name);
    SharemindProcess_lock(process);
    const SharemindPdpiCache * const cache = &process->pdpiCache;
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
    const SharemindPdpiCache * const cache = &p->pdpiCache;
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
    const SharemindPdpiCache * const cache = &p->pdpiCache;
    size_t i = cache->size;
    while (i)
        SharemindPdpiCacheItem_stop(&cache->data[--i]);
}

SharemindVmError SharemindProcess_run(SharemindProcess * const p) {
    assert(p);
    SharemindProcess_lock(p);
    /** \todo Add support for continue/restart */
    if (unlikely(!p->state == SHAREMIND_VM_PROCESS_INITIALIZED)) {
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

    const SharemindVmError e = sharemind_vm_run(p, SHAREMIND_I_RUN, NULL);
    if (e != SHAREMIND_VM_RUNTIME_TRAP)
        SharemindProcess_stop_pdpis(p);
    SharemindProcess_unlock(p);
    return e;
}

int64_t SharemindProcess_returnValue(const SharemindProcess * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    const int64_t r = p->returnValue.int64[0];
    SharemindProcess_unlockConst(p);
    return r;
}

SharemindVmProcessException SharemindProcess_exception(
        const SharemindProcess * p)
{
    assert(p);
    SharemindProcess_lockConst(p);
    assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
    const SharemindVmProcessException r =
            (SharemindVmProcessException) p->exceptionValue;
    SharemindProcess_unlockConst(p);
    assert(SharemindVmProcessException_toString(r) != 0);
    return r;
}

size_t SharemindProcess_currentCodeSection(const SharemindProcess * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    const size_t r = p->currentCodeSectionIndex;
    SharemindProcess_unlockConst(p);
    return r;
}

uintptr_t SharemindProcess_currentIp(const SharemindProcess * p) {
    assert(p);
    SharemindProcess_lockConst(p);
    const uintptr_t r = p->currentIp;
    SharemindProcess_unlockConst(p);
    return r;
}

static inline uint64_t SharemindProcess_public_alloc_slot(
        SharemindProcess * const p,
        SharemindMemorySlot ** const memorySlot)
{
    assert(p);
    assert(memorySlot);
    assert(p->memoryMap.size < (UINT64_MAX < SIZE_MAX ? UINT64_MAX : SIZE_MAX));

    /* Find a free memory slot: */
    const uint64_t index =
            SharemindMemoryMap_findUnusedPtr(&p->memoryMap, p->memorySlotNext);
    assert(index != 0u);

    /* Fill the slot: */
#ifndef NDEBUG
    const size_t oldSize = p->memoryMap.size;
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
                                       const uint64_t nBytes,
                                       SharemindMemorySlot ** const memorySlot)
{
    assert(p);
    assert(p->memorySlotNext >= 4u);

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
    const uint64_t index = SharemindProcess_public_alloc_slot(p, &slot);
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

    if (p->memPublicHeap.usage > p->memPublicHeap.stats.max)
        p->memPublicHeap.stats.max = p->memPublicHeap.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    if (memorySlot)
        (*memorySlot) = slot;

    return index;
}

SharemindVmProcessException SharemindProcess_public_free(
        SharemindProcess * const p,
        const uint64_t ptr)
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

    const SharemindVmProcessException r = SharemindProcess_public_free(p, ptr);
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

    const SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindMemoryMap_value * const v =
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

    const SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindMemoryMap_value * const v =
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
    const size_t oldSize = p->privateMemoryMap.size;
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

    if (p->memPrivate.usage > p->memPrivate.stats.max)
        p->memPrivate.stats.max = p->memPrivate.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

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
    const size_t nBytes = v->value;
    assert(nBytes > 0u);

    /* Free the memory: */
    free(ptr);

    /* Remove pointer from valid list: */
    #ifndef NDEBUG
    const int r =
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

    if (p->memReserved.usage > p->memReserved.stats.max)
        p->memReserved.stats.max = p->memReserved.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

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

static const SharemindModuleApi0x1PdpiInfo * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index)
{
    assert(c);
    assert(c->vm_internal);

    if (pd_index > SIZE_MAX)
        return NULL;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindPdpiCacheItem * const pdpiCacheItem =
            SharemindPdpiCache_get_const_pointer(&p->pdpiCache, pd_index);
    if (pdpiCacheItem) {
        assert(pdpiCacheItem->info.pdpiHandle);
        return &pdpiCacheItem->info;
    }

    return NULL;
}

SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindProcess)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindProcess,)
