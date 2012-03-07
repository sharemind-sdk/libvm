/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSHAREMIND_VM_INTERNAL_HELPERS_H
#define SHAREMIND_LIBSHAREMIND_VM_INTERNAL_HELPERS_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include "vm.h"

#ifndef SHAREMIND_SOFT_FLOAT
#include <fenv.h>
#include <setjmp.h>
#endif /* #ifndef SHAREMIND_SOFT_FLOAT */
#ifdef SHAREMIND_DEBUG
#include <stdio.h>
#endif /* SHAREMIND_DEBUG */
#ifdef __USE_POSIX
#include <signal.h>
#endif

#include <sharemind/fnv.h>
#include <sharemind/instrset.h>
#include <sharemind/libmodapi/api_0x1.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/map.h>
#include <sharemind/refs.h>
#include <sharemind/stack.h>
#include <sharemind/vector.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    SharemindCodeBlock * block;
    unsigned type;
} SharemindPreparationBlock;

typedef enum {
    SHAREMIND_I_GET_IMPL_LABEL,
    SHAREMIND_I_RUN,
    SHAREMIND_I_CONTINUE
} SharemindInnerCommand;


/*******************************************************************************
 *  SharemindSyscallBindings
*******************************************************************************/

SHAREMIND_VECTOR_DECLARE(SharemindSyscallBindings,SharemindSyscallBinding,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindSyscallBindings,SharemindSyscallBinding,malloc,free,realloc,inline)


/*******************************************************************************
 *  SharemindPdBindings
*******************************************************************************/

SHAREMIND_VECTOR_DECLARE(SharemindPdBindings,SharemindPd *,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdBindings,SharemindPd *,malloc,free,realloc,inline)

/*******************************************************************************
 *  SharemindPdpiInstance
*******************************************************************************/

typedef struct {
    SharemindPdpi * pdpi;
    void * pdpiHandle;
    size_t pdkIndex;
    void * moduleHandle;
} SharemindPdpiCacheItem;

SHAREMIND_VECTOR_DECLARE(SharemindPdpiCache,SharemindPdpiCacheItem,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdpiCache,SharemindPdpiCacheItem,malloc,free,realloc,inline)


/*******************************************************************************
 *  SharemindMemoryMap
*******************************************************************************/

struct SharemindMemorySlotSpecials_;
typedef struct SharemindMemorySlotSpecials_ SharemindMemorySlotSpecials;

typedef struct {
    void * pData;
    size_t size;
    uint64_t nrefs;
    SharemindMemorySlotSpecials * specials;
} SharemindMemorySlot;

struct SharemindMemorySlotSpecials_ {
    int readable;
    int writeable;
    void (*free)(SharemindMemorySlot *);
};

#define SHAREMIND_MEMORY_SLOT_INIT_DECLARE \
    void SharemindMemorySlot_init(\
        SharemindMemorySlot * m, \
        void * pData, \
        size_t size, \
        SharemindMemorySlotSpecials * specials)

#define SHAREMIND_MEMORY_SLOT_INIT_DEFINE \
    SHAREMIND_MEMORY_SLOT_INIT_DECLARE { \
        m->pData = pData; \
        m->size = size; \
        m->nrefs = 0u; \
        m->specials = specials; \
    }

#define SHAREMIND_MEMORY_SLOT_DESTROY_DECLARE \
    void SharemindMemorySlot_destroy(SharemindMemorySlot * m)

#define SHAREMIND_MEMORY_SLOT_DESTROY_DEFINE \
    SHAREMIND_MEMORY_SLOT_DESTROY_DECLARE { \
        if (m->specials) { \
            if (m->specials->free) \
                m->specials->free(m); \
        } else { \
            free(m->pData); \
        } \
    }

#ifndef SHAREMIND_FAST_BUILD
inline SHAREMIND_MEMORY_SLOT_INIT_DECLARE __attribute__ ((nonnull(1)));
inline SHAREMIND_MEMORY_SLOT_DESTROY_DECLARE __attribute__ ((nonnull(1)));
inline SHAREMIND_MEMORY_SLOT_INIT_DEFINE
inline SHAREMIND_MEMORY_SLOT_DESTROY_DEFINE

SHAREMIND_MAP_DECLARE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,inline)
SHAREMIND_MAP_DEFINE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,(uint16_t)(key),SHAREMIND_MAP_KEY_EQUALS_uint64_t,SHAREMIND_MAP_KEY_LESS_THAN_uint64_t,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,inline)
#else
SHAREMIND_MEMORY_SLOT_INIT_DECLARE __attribute__ ((nonnull(1)));
SHAREMIND_MEMORY_SLOT_DESTROY_DECLARE __attribute__ ((nonnull(1)));
SHAREMIND_MAP_DECLARE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,)
#endif

#define SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DECLARE \
    uint64_t SharemindMemoryMap_find_unused_ptr(const SharemindMemoryMap * m, uint64_t startFrom)

#define SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DEFINE \
    SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DECLARE { \
        assert(m); \
        assert(startFrom != 0u); \
        assert(m->size < UINT64_MAX); \
        assert(m->size < SIZE_MAX); \
        uint64_t index = startFrom; \
        for (;;) { \
            /* Check if slot is free: */ \
            if (likely(!SharemindMemoryMap_get_const(m, index))) \
                break; \
            /* Increment index, skip "NULL" and static memory pointers: */ \
            if (unlikely(!++index)) \
                index += 4u; \
            assert(index != 0u); \
            /* This shouldn't trigger because (m->size < UINT64_MAX) */ \
            assert(index != startFrom); \
        } \
        assert(index != 0u); \
        return index; \
    }

#ifndef SHAREMIND_FAST_BUILD
inline SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DECLARE __attribute__ ((nonnull(1)));
inline SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DEFINE
#else
SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DECLARE __attribute__ ((nonnull(1)));
#endif

/*******************************************************************************
 *  SharemindReference and SharemindCReference
********************************************************************************/

typedef SharemindModuleApi0x1Reference SharemindReference;
typedef SharemindModuleApi0x1CReference SharemindCReference;

inline void SharemindReference_destroy(SharemindReference * r) {
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

inline void SharemindCReference_destroy(SharemindCReference * r) {
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

SHAREMIND_VECTOR_DECLARE(SharemindReferenceVector,SharemindReference,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindReferenceVector,SharemindReference,malloc,free,realloc,inline)
SHAREMIND_VECTOR_DECLARE(SharemindCReferenceVector,SharemindCReference,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindCReferenceVector,SharemindCReference,malloc,free,realloc,inline)


/*******************************************************************************
 *  SharemindStackFrame
********************************************************************************/

#ifndef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DECLARE(SharemindRegisterVector,SharemindCodeBlock,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindRegisterVector,SharemindCodeBlock,malloc,free,realloc,inline)
#else
SHAREMIND_VECTOR_DECLARE(SharemindRegisterVector,SharemindCodeBlock,,)
#endif

struct SharemindStackFrame_ {
    SharemindRegisterVector stack;
    SharemindReferenceVector refstack;
    SharemindCReferenceVector crefstack;
    struct SharemindStackFrame_ * prev;

    const SharemindCodeBlock * returnAddr;
    SharemindCodeBlock * returnValueAddr;

#ifdef SHAREMIND_DEBUG
    size_t lastCallIp;
    size_t lastCallSection;
#endif
};
typedef struct SharemindStackFrame_ SharemindStackFrame;

void SharemindStackFrame_init(SharemindStackFrame * f, SharemindStackFrame * prev) __attribute__ ((nonnull(1)));
void SharemindStackFrame_destroy(SharemindStackFrame * f) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SharemindCodeSection
********************************************************************************/

typedef struct {
    size_t offset;
    SharemindCodeBlock originalInstruction;
} SharemindBreakpoint;

#ifndef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DECLARE(SharemindBreakpointVector,SharemindBreakpoint,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindBreakpointVector,SharemindBreakpoint,malloc,free,realloc,inline)
SHAREMIND_INSTRSET_DECLARE(SharemindInstrSet,inline)
SHAREMIND_INSTRSET_DEFINE(SharemindInstrSet,malloc,free,inline)
#else
SHAREMIND_VECTOR_DECLARE(SharemindBreakpointVector,SharemindBreakpoint,,)
SHAREMIND_INSTRSET_DECLARE(SharemindInstrSet,)
#endif

typedef struct {
    SharemindCodeBlock * data;
    size_t size;
    SharemindBreakpointVector breakPoints;
    SharemindInstrSet instrset;
} SharemindCodeSection;

int SharemindCodeSection_init(SharemindCodeSection * s,
                               const SharemindCodeBlock * const code,
                               const size_t codeSize)
    __attribute__ ((nonnull(1)));

void SharemindCodeSection_destroy(SharemindCodeSection * const s) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SharemindDataSection
********************************************************************************/

typedef SharemindMemorySlot SharemindDataSection;

int SharemindDataSection_init(SharemindDataSection * ds, size_t size, SharemindMemorySlotSpecials * specials);
void SharemindDataSection_destroy(SharemindDataSection * ds);


/*******************************************************************************
 *  SharemindProgram
********************************************************************************/

#ifndef SHAREMIND_SOFT_FLOAT
typedef enum {
    SHAREMIND_HET_FPE_UNKNOWN = 0x00, /* Unknown arithmetic or floating-point error */
    SHAREMIND_HET_FPE_INTDIV  = 0x01, /* integer divide by zero */
    SHAREMIND_HET_FPE_INTOVF  = 0x02, /* integer overflow */
    SHAREMIND_HET_FPE_FLTDIV  = 0x03, /* floating-point divide by zero */
    SHAREMIND_HET_FPE_FLTOVF  = 0x04, /* floating-point overflow */
    SHAREMIND_HET_FPE_FLTUND  = 0x05, /* floating-point underflow */
    SHAREMIND_HET_FPE_FLTRES  = 0x06, /* floating-point inexact result */
    SHAREMIND_HET_FPE_FLTINV  = 0x07, /* floating-point invalid operation */
    SHAREMIND_HET_COUNT
} SharemindHardwareExceptionType;
#endif

#ifndef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DECLARE(SharemindCodeSectionsVector,SharemindCodeSection,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindCodeSectionsVector,SharemindCodeSection,malloc,free,realloc,inline)
SHAREMIND_VECTOR_DECLARE(SharemindDataSectionsVector,SharemindDataSection,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindDataSectionsVector,SharemindDataSection,malloc,free,realloc,inline)
SHAREMIND_VECTOR_DECLARE(SharemindDataSectionSizesVector,uint32_t,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindDataSectionSizesVector,uint32_t,malloc,free,realloc,inline)
SHAREMIND_STACK_DECLARE(SharemindFrameStack,SharemindStackFrame,,inline)
SHAREMIND_STACK_DEFINE(SharemindFrameStack,SharemindStackFrame,malloc,free,inline)
#else
SHAREMIND_VECTOR_DECLARE(SharemindCodeSectionsVector,SharemindCodeSection,,)
SHAREMIND_VECTOR_DECLARE(SharemindDataSectionsVector,SharemindDataSection,,)
SHAREMIND_VECTOR_DECLARE(SharemindDataSectionSizesVector,uint32_t,,)
SHAREMIND_STACK_DECLARE(SharemindFrameStack,SharemindStackFrame,,)
#endif

#ifndef SHAREMIND_FAST_BUILD
SHAREMIND_MAP_DECLARE(SharemindPrivateMemoryMap,void*,void * const,size_t,inline)
SHAREMIND_MAP_DEFINE(SharemindPrivateMemoryMap,void*,void * const,size_t,fnv_16a_buf(key,sizeof(void *)),SHAREMIND_MAP_KEY_EQUALS_voidptr,SHAREMIND_MAP_KEY_LESS_THAN_voidptr,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,inline)
#else
SHAREMIND_MAP_DECLARE(SharemindPrivateMemoryMap,void*,void * const,size_t,)
#endif

typedef struct {
    size_t max;
} SharemindMemoryInfoStatistics;

typedef struct {
    size_t usage;
    size_t upperLimit;
    SharemindMemoryInfoStatistics stats;
} SharemindMemoryInfo;

struct SharemindProgram_ {
    bool ready;

    SharemindVmError error;

    SharemindCodeSectionsVector codeSections;
    SharemindDataSectionsVector rodataSections;
    SharemindDataSectionsVector dataSections;
    SharemindDataSectionSizesVector bssSectionSizes;

    SharemindSyscallBindings bindings;
    SharemindPdBindings pdBindings;

    size_t activeLinkingUnit;

    size_t prepareCodeSectionIndex;
    uintptr_t prepareIp;

    SharemindVm * vm;

    SHAREMIND_REFS_DECLARE_FIELDS

};

SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindProgram)


/*******************************************************************************
 *  SharemindProcess
********************************************************************************/

struct SharemindProcess_ {
    SharemindProgram * program;

    SharemindVmProcessState state;

    SharemindDataSectionsVector dataSections;
    SharemindDataSectionsVector bssSections;

    SharemindPdpiCache pdpiCache;

    SharemindFrameStack frames;
    SharemindStackFrame * globalFrame;
    SharemindStackFrame * nextFrame;
    SharemindStackFrame * thisFrame;

    SharemindMemoryMap memoryMap;
    uint64_t memorySlotNext;
    SharemindPrivateMemoryMap privateMemoryMap;

    size_t currentCodeSectionIndex;
    uintptr_t currentIp;

    SharemindCodeBlock returnValue;
    int64_t exceptionValue;

    SharemindModuleApi0x1SyscallContext syscallContext;

    SharemindMemoryInfo memPublicHeap;
    SharemindMemoryInfo memPrivate;
    SharemindMemoryInfo memReserved;
    SharemindMemoryInfo memTotal;

};

uint64_t SharemindProcess_public_alloc(SharemindProcess * p, uint64_t nBytes, SharemindMemorySlot ** memorySlot);
SharemindVmProcessException SharemindProcess_public_free(SharemindProcess * p, uint64_t ptr);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSHAREMIND_VM_INTERNAL_HELPERS_H */
