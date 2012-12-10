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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "core.h"
#include "rwdataspecials.h"
#include "program.h"


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SHAREMIND_ENUM_DEFINE_TOSTRING(SharemindVmProcessState, SHAREMIND_VM_PROCESS_STATE_ENUM)
SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmProcessException, SHAREMIND_VM_PROCESS_EXCEPTION_ENUM)


/*******************************************************************************
 *  Forward declarations:
********************************************************************************/

static inline bool SharemindProcess_reinitialize_static_mem_slots(SharemindProcess * p);

static inline uint64_t SharemindProcess_public_alloc_slot(SharemindProcess * p, SharemindMemorySlot ** memorySlot) __attribute__ ((nonnull(1, 2)));

static uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c, uint64_t nBytes) __attribute__ ((nonnull(1)));
static int sharemind_public_free(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));
static size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));
static void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));

static void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));
static int sharemind_private_free(SharemindModuleApi0x1SyscallContext * c, void * ptr) __attribute__ ((nonnull(1)));
static int sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));
static int sharemind_private_release(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));

static const SharemindModuleApi0x1PdpiInfo * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index) __attribute__ ((nonnull(1)));



/*******************************************************************************
 *  SharemindProcess:
********************************************************************************/

static inline void SharemindProcess_destroy(SharemindProcess * p);
static inline bool SharemindProcess_start_pdpis(SharemindProcess * p);
static inline void SharemindProcess_stop_pdpis(SharemindProcess * p);

SharemindProcess * SharemindProcess_new(SharemindProgram * program) {
    assert(program);
    assert(SharemindProgram_is_ready(program));

    SharemindProcess * const p = (SharemindProcess *) malloc(sizeof(SharemindProcess));
    if (!p)
        goto SharemindProcess_new_fail_0;

    if (unlikely(!SharemindProgram_refs_ref(program)))
        goto SharemindProcess_new_fail_1;

    p->program = program;

    /* Copy DATA section */
    SharemindDataSectionsVector_init(&p->dataSections);
    for (size_t i = 0u; i < program->dataSections.size; i++) {
        SharemindDataSection * processSection = SharemindDataSectionsVector_push(&p->dataSections);
        if (!processSection)
            goto SharemindProcess_new_fail_data_sections;
        SharemindDataSection * originalSection = &program->dataSections.data[i];
        if (!SharemindDataSection_init(processSection, originalSection->size, &rwDataSpecials)) {
            SharemindDataSectionsVector_pop(&p->dataSections);
            goto SharemindProcess_new_fail_data_sections;
        }
        assert(processSection->size == originalSection->size);
        memcpy(processSection->pData, originalSection->pData, originalSection->size);
    }
    assert(p->dataSections.size == program->dataSections.size);

    /* Initialize BSS sections */
    SharemindDataSectionsVector_init(&p->bssSections);
    for (size_t i = 0u; i < program->bssSectionSizes.size; i++) {
        SharemindDataSection * bssSection = SharemindDataSectionsVector_push(&p->bssSections);
        if (!bssSection)
            goto SharemindProcess_new_fail_bss_sections;
        SHAREMIND_STATIC_ASSERT(sizeof(program->bssSectionSizes.data[i]) <= sizeof(size_t));
        if (!SharemindDataSection_init(bssSection, program->bssSectionSizes.data[i], &rwDataSpecials)) {
            SharemindDataSectionsVector_pop(&p->bssSections);
            goto SharemindProcess_new_fail_bss_sections;
        }
        assert(bssSection->size == program->bssSectionSizes.data[i]);
        memset(bssSection->pData, 0, program->bssSectionSizes.data[i]);
    }
    assert(p->bssSections.size == program->bssSectionSizes.size);


    /* Initialize PDPI cache: */
    SharemindPdpiCache_init(&p->pdpiCache);
    for (size_t i = 0u; i < program->pdBindings.size; i++) {
        SharemindPdpiCacheItem * ci = SharemindPdpiCache_push(&p->pdpiCache);
        if (!ci)
            goto SharemindProcess_new_fail_pdpiCache;

        if (!SharemindPdpiCacheItem_init(ci, program->pdBindings.data[i])) {
            if (!ci->pdpi)
                SharemindPdpiCache_pop(&p->pdpiCache);
            goto SharemindProcess_new_fail_pdpiCache;
        }
    }
    assert(p->pdpiCache.size == program->pdBindings.size);

    SharemindFrameStack_init(&p->frames);

    SharemindMemoryMap_init(&p->memoryMap);
    p->memorySlotNext = 1u;
    SharemindPrivateMemoryMap_init(&p->privateMemoryMap);

    /* Set currentCodeSectionIndex before SharemindProcess_reinitialize_static_mem_slots: */
    p->currentCodeSectionIndex = program->activeLinkingUnit;
    p->currentIp = 0u;

    p->fpuState = sf_fpu_state_default;

    /* Initialize section pointers: */

    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 0u));
    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 1u));
    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 2u));
    assert(p->memorySlotNext == 1u);
    if (unlikely(!SharemindProcess_reinitialize_static_mem_slots(p)))
        goto SharemindProcess_new_fail_2;

    p->memorySlotNext += 3;

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
    if (unlikely(!p->globalFrame))
        goto SharemindProcess_new_fail_2;

    /* Initialize global frame: */
    SharemindStackFrame_init(p->globalFrame, NULL);
    p->globalFrame->returnAddr = NULL; /* Triggers halt on return. */
    /* p->globalFrame->returnValueAddr = &(p->returnValue); is not needed. */
    p->thisFrame = p->globalFrame;
    p->nextFrame = NULL;

    #ifdef __MACH__
    host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &p->macClock);
    #endif

    return p;

SharemindProcess_new_fail_pdpiCache:

    SharemindPdpiCache_destroy_with(&p->pdpiCache, &SharemindPdpiCacheItem_destroy);

SharemindProcess_new_fail_bss_sections:

    SharemindDataSectionsVector_destroy_with(&p->bssSections, &SharemindDataSection_destroy);

SharemindProcess_new_fail_data_sections:

    SharemindDataSectionsVector_destroy_with(&p->dataSections, &SharemindDataSection_destroy);
    SharemindProgram_refs_unref(program);
    goto SharemindProcess_new_fail_1;

SharemindProcess_new_fail_2:

    SharemindProcess_destroy(p);

SharemindProcess_new_fail_1:

    free(p);

SharemindProcess_new_fail_0:

    return NULL;
}

static inline void SharemindProcess_destroy(SharemindProcess * p) {
    assert(p);

    #ifdef __MACH__
    mach_port_deallocate(mach_task_self(), p->macClock);
    #endif

    SharemindProgram_refs_unref(p->program);

    SharemindPrivateMemoryMap_destroy_with(&p->privateMemoryMap, &SharemindPrivateMemoryMap_destroyer);
    SharemindMemoryMap_destroy_with(&p->memoryMap, &SharemindMemoryMap_destroyer);

    SharemindFrameStack_destroy_with(&p->frames, &SharemindStackFrame_destroy);

    SharemindPdpiCache_destroy_with(&p->pdpiCache, &SharemindPdpiCacheItem_destroy);

    SharemindDataSectionsVector_destroy_with(&p->bssSections, &SharemindDataSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->dataSections, &SharemindDataSection_destroy);
}

void SharemindProcess_free(SharemindProcess * p) {
    assert(p);
    SharemindProcess_destroy(p);
    free(p);
}

size_t SharemindProcess_get_pdpi_count(const SharemindProcess * process) {
    assert(process);
    return process->pdpiCache.size;
}

SharemindPdpi * SharemindProcess_get_pdpi(const SharemindProcess * process,
                                          size_t pdpiIndex)
{
    assert(process);
    const SharemindPdpiCache * const cache = &process->pdpiCache;
    if (pdpiIndex < cache->size)
        return cache->data[pdpiIndex].pdpi;

    return NULL;
}

bool SharemindProcess_set_pdpi_facility(SharemindProcess * const process,
                                        const char * const name,
                                        void * const facility,
                                        void * const context)
{
    assert(process);
    assert(name);
    const SharemindPdpiCache * const cache = &process->pdpiCache;
    for (size_t i = 0u; i < cache->size; i++)
        if (!SharemindPdpi_set_facility(cache->data[i].pdpi, name, facility, context))
            return false;

    return true;
}

void SharemindProcess_set_process_internal(SharemindProcess * const process,
                                           void * const value)
{
    assert(process);
    process->syscallContext.process_internal = value;
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

static inline bool SharemindProcess_reinitialize_static_mem_slots(SharemindProcess * p) {
    assert(p);

#define INIT_STATIC_MEMSLOT(index,pSection) \
    if (1) { \
        assert((pSection)->size > p->currentCodeSectionIndex); \
        SharemindDataSection * const restrict staticSlot = SharemindDataSectionsVector_get_pointer((pSection), p->currentCodeSectionIndex); \
        assert(staticSlot); \
        SharemindMemorySlot * const restrict slot = SharemindMemoryMap_get_or_insert(&p->memoryMap, (index)); \
        if (unlikely(!slot)) \
            return false; \
        (*slot) = *staticSlot; \
    } else (void) 0

    INIT_STATIC_MEMSLOT(1u,&p->program->rodataSections);
    INIT_STATIC_MEMSLOT(2u,&p->dataSections);
    INIT_STATIC_MEMSLOT(3u,&p->bssSections);

    return true;
}

SharemindVmError SharemindProcess_run(SharemindProcess * const p) {
    assert(p);

    /** \todo Add support for continue/restart */
    if (unlikely(!p->state == SHAREMIND_VM_PROCESS_INITIALIZED))
        return SHAREMIND_VM_INVALID_INPUT_STATE;

    if (!SharemindProcess_start_pdpis(p))
        return SHAREMIND_VM_PDPI_STARTUP_FAILED;

    const SharemindVmError e = sharemind_vm_run(p, SHAREMIND_I_RUN, NULL);
    if (e != SHAREMIND_VM_RUNTIME_TRAP)
        SharemindProcess_stop_pdpis(p);

    return e;
}

int64_t SharemindProcess_get_return_value(SharemindProcess * p) {
    assert(p);
    return p->returnValue.int64[0];
}

SharemindVmProcessException SharemindProcess_get_exception(SharemindProcess * p) {
    assert(p);
    assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
    assert(SharemindVmProcessException_toString((SharemindVmProcessException) p->exceptionValue) != 0);
    return (SharemindVmProcessException) p->exceptionValue;
}

size_t SharemindProcess_get_current_code_section(SharemindProcess * p) {
    assert(p);
    return p->currentCodeSectionIndex;
}

uintptr_t SharemindProcess_get_current_ip(SharemindProcess * p) {
    assert(p);
    return p->currentIp;
}

static inline uint64_t SharemindProcess_public_alloc_slot(SharemindProcess * p, SharemindMemorySlot ** memorySlot) {
    assert(p);
    assert(memorySlot);
    assert(p->memoryMap.size < (UINT64_MAX < SIZE_MAX ? UINT64_MAX : SIZE_MAX));

    /* Find a free memory slot: */
    const uint64_t index = SharemindMemoryMap_find_unused_ptr(&p->memoryMap, p->memorySlotNext);
    assert(index != 0u);

    /* Fill the slot: */
#ifndef NDEBUG
    size_t oldSize = p->memoryMap.size;
#endif
    SharemindMemorySlot * const slot = SharemindMemoryMap_get_or_insert(&p->memoryMap, index);
    if (unlikely(!slot))
        return 0u;
    assert(oldSize < p->memoryMap.size);

    /* Optimize next alloc: */
    p->memorySlotNext = index + 1u;
    /* skip "NULL" and static memory pointers: */
    if (unlikely(!p->memorySlotNext))
        p->memorySlotNext += 4u;

    (*memorySlot) = slot;

    return index;
}

uint64_t SharemindProcess_public_alloc(SharemindProcess * p,
                                       uint64_t nBytes,
                                       SharemindMemorySlot ** memorySlot)
{
    assert(p);
    assert(p->memorySlotNext != 0u);

    /* Fail if allocating too little: */
    if (unlikely(nBytes <= 0u))
        return 0u;

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPublicHeap.upperLimit - p->memPublicHeap.usage < nBytes)))
        return 0u;

    /** \todo Check any other memory limits? */

    /* Fail if all memory slots are used. */
    SHAREMIND_STATIC_ASSERT(sizeof(uint64_t) >= sizeof(size_t));
    if (unlikely(p->memoryMap.size >= SIZE_MAX))
        return 0u;

    /* Allocate the memory: */
    void * const pData = malloc((size_t) nBytes);
    if (unlikely(!pData))
        return 0u;

    SharemindMemorySlot * slot;
    const uint64_t index = SharemindProcess_public_alloc_slot(p, &slot);
    if (unlikely(index == 0u)) {
        free(pData);
        return index;
    }

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

SharemindVmProcessException SharemindProcess_public_free(SharemindProcess * p,
                                                         uint64_t ptr)
{
    assert(p);
    assert(ptr != 0u);
    SharemindMemorySlot * slot = SharemindMemoryMap_get(&p->memoryMap, ptr);
    if (unlikely(!slot))
        return SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE;

    if (unlikely(slot->nrefs != 0u))
        return SHAREMIND_VM_PROCESS_MEMORY_IN_USE;


    /* Update memory statistics: */
    assert(p->memPublicHeap.usage >= slot->size);
    assert(p->memTotal.usage >= slot->size);
    p->memPublicHeap.usage -= slot->size;
    p->memTotal.usage -= slot->size;

    /** \todo Update any other memory statistics? */

    /* Deallocate the memory and release the slot: */
    SharemindMemorySlot_destroy(slot);
    SharemindMemoryMap_remove(&p->memoryMap, ptr);

    return SHAREMIND_VM_PROCESS_OK;
}

/*******************************************************************************
 *  Other procedures:
********************************************************************************/

static uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(nBytes > UINT64_MAX))
        return 0u;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    return SharemindProcess_public_alloc(p, nBytes, NULL);
}

static int sharemind_public_free(SharemindModuleApi0x1SyscallContext * c,
                                 uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    return SharemindProcess_public_free(p, ptr) == SHAREMIND_VM_PROCESS_OK;
}

static size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c,
                                        uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    const SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindMemorySlot * slot = SharemindMemoryMap_get_const(&p->memoryMap, ptr);
    if (!slot)
        return 0u;

    return slot->size;
}

static void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t ptr)
{
    assert(c);
    assert(c->vm_internal);

    const SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindMemorySlot * slot = SharemindMemoryMap_get_const(&p->memoryMap, ptr);
    if (!slot)
        return NULL;

    return slot->pData;
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
    size_t oldSize = p->privateMemoryMap.size;
#endif
    size_t * s = SharemindPrivateMemoryMap_get_or_insert(&p->privateMemoryMap, ptr);
    if (unlikely(!s)) {
        free(ptr);
        return NULL;
    }
    assert(oldSize < p->privateMemoryMap.size);
    (*s) = nBytes;

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

static int sharemind_private_free(SharemindModuleApi0x1SyscallContext * c,
                                  void * ptr)
{
    assert(c);
    assert(c->vm_internal);

    if (unlikely(!ptr))
        return false;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    /* Check if pointer is in private memory map: */
    const size_t * s = SharemindPrivateMemoryMap_get_const(&p->privateMemoryMap, ptr);
    if (unlikely(!s))
        return false;

    /* Get allocated size: */
    const size_t nBytes = (*s);

    /* Free the memory: */
    free(ptr);

    /* Remove pointer from valid list: */
    #ifndef NDEBUG
    int r =
    #endif
        SharemindPrivateMemoryMap_remove(&p->privateMemoryMap, ptr);
    assert(r);

    /* Update memory statistics: */
    assert(p->memPrivate.usage >= nBytes);
    assert(p->memTotal.usage >= nBytes);
    p->memPrivate.usage -= nBytes;
    p->memTotal.usage -= nBytes;

    /** \todo Update any other memory statistics? */

    return true;
}

static int sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c,
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
                 || (p->memReserved.upperLimit - p->memReserved.usage < nBytes)))
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

static int sharemind_private_release(SharemindModuleApi0x1SyscallContext * c,
                                     size_t nBytes)
{
    assert(c);
    assert(c->vm_internal);

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

const SharemindModuleApi0x1PdpiInfo * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index)
{
    assert(c);
    assert(c->vm_internal);

    if (pd_index > SIZE_MAX)
        return NULL;

    SharemindProcess * const p = (SharemindProcess *) c->vm_internal;
    assert(p);

    const SharemindPdpiCacheItem * const pdpiCacheItem = SharemindPdpiCache_get_const_pointer(&p->pdpiCache, pd_index);
    if (pdpiCacheItem) {
        assert(pdpiCacheItem->info.pdpiHandle);
        return &pdpiCacheItem->info;
    }

    return NULL;
}
