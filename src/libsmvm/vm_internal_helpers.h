/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_VM_INTERNAL_HELPERS_H
#define SHAREMIND_LIBSMVM_VM_INTERNAL_HELPERS_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include "vm.h"

#ifndef SMVM_SOFT_FLOAT
#include <fenv.h>
#include <setjmp.h>
#endif /* #ifndef SMVM_SOFT_FLOAT */
#ifdef SMVM_DEBUG
#include <stdio.h>
#endif /* SMVM_DEBUG */
#ifdef __USE_POSIX
#include <signal.h>
#endif

#include <stdlib.h>
#include "../fnv.h"
#include "../instrset.h"
#include "../libsmmod/modapi_0x1.h"
#include "../map.h"
#include "../stack.h"
#include "../vector.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    SMVM_CodeBlock * block;
    unsigned type;
} SMVM_Prepare_IBlock;

typedef enum {
    SMVM_I_GET_IMPL_LABEL,
    SMVM_I_RUN,
    SMVM_I_CONTINUE
} SMVM_InnerCommand;


/*******************************************************************************
 *  SMVM_SyscallBindings
********************************************************************************/

SM_VECTOR_DECLARE(SMVM_SyscallBindings,const SMVM_Context_Syscall *,,inline)
SM_VECTOR_DEFINE(SMVM_SyscallBindings,const SMVM_Context_Syscall *,malloc,free,realloc,inline)


/*******************************************************************************
 *  SMVM_PdBindings
********************************************************************************/

SM_VECTOR_DECLARE(SMVM_PdBindings,const SMVM_Context_PDPI *,,inline)
SM_VECTOR_DEFINE(SMVM_PdBindings,const SMVM_Context_PDPI *,malloc,free,realloc,inline)


/*******************************************************************************
 *  SMVM_MemoryMap
********************************************************************************/

struct _SMVM_MemorySlotSpecials;
typedef struct _SMVM_MemorySlotSpecials SMVM_MemorySlotSpecials;

typedef struct _SMVM_MemorySlot {
    void * pData;
    size_t size;
    uint64_t nrefs;
    SMVM_MemorySlotSpecials * specials;
} SMVM_MemorySlot;

struct _SMVM_MemorySlotSpecials {
    uint64_t ptr;
    int readable;
    int writeable;
    void (*free)(SMVM_MemorySlot *);
};

#define SMVM_MemorySlot_init_DECLARE \
    void SMVM_MemorySlot_init(SMVM_MemorySlot * m, \
                              void * pData, \
                              size_t size, \
                              SMVM_MemorySlotSpecials * specials)

#define SMVM_MemorySlot_init_DEFINE \
    SMVM_MemorySlot_init_DECLARE { \
        m->pData = pData; \
        m->size = size; \
        m->nrefs = 0u; \
        m->specials = specials; \
    }

#define SMVM_MemorySlot_destroy_DECLARE \
    void SMVM_MemorySlot_destroy(SMVM_MemorySlot * m)

#define SMVM_MemorySlot_destroy_DEFINE \
    SMVM_MemorySlot_destroy_DECLARE { \
        if (m->specials) { \
            if (m->specials->free) \
                m->specials->free(m); \
        } else { \
            free(m->pData); \
        } \
    }

#ifndef SMVM_FAST_BUILD
inline SMVM_MemorySlot_init_DEFINE
inline SMVM_MemorySlot_destroy_DEFINE

SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,const uint64_t,SMVM_MemorySlot,inline)
SM_MAP_DEFINE(SMVM_MemoryMap,uint64_t,const uint64_t,SMVM_MemorySlot,(uint16_t),SM_MAP_KEY_EQUALS_uint64_t,SM_MAP_KEY_LESS_THAN_uint64_t,SM_MAP_KEYCOPY_REGULAR,SM_MAP_KEYFREE_REGULAR,malloc,free,inline)
#else
SMVM_MemorySlot_init_DECLARE;
SMVM_MemorySlot_destroy_DECLARE;
SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,const uint64_t,SMVM_MemorySlot,)
#endif

#define SMVM_MemoryMap_find_unused_ptr_DECLARE \
    uint64_t SMVM_MemoryMap_find_unused_ptr(const SMVM_MemoryMap * m, uint64_t startFrom)

#define SMVM_MemoryMap_find_unused_ptr_DEFINE \
    SMVM_MemoryMap_find_unused_ptr_DECLARE { \
        assert(m); \
        assert(startFrom != 0u); \
        assert(m->size < UINT64_MAX); \
        assert(m->size < SIZE_MAX); \
        uint64_t index = startFrom; \
        for (;;) { \
            /* Check if slot is free: */ \
            if (likely(!SMVM_MemoryMap_get_const(m, index))) \
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

#ifndef SMVM_FAST_BUILD
inline SMVM_MemoryMap_find_unused_ptr_DECLARE;
inline SMVM_MemoryMap_find_unused_ptr_DEFINE
#else
SMVM_MemoryMap_find_unused_ptr_DECLARE;
#endif

/*******************************************************************************
 *  SMVM_Reference and SMVM_CReference
********************************************************************************/

typedef SMVM_MODAPI_0x1_Reference SMVM_Reference;
typedef SMVM_MODAPI_0x1_CReference SMVM_CReference;

inline void SMVM_Reference_destroy(SMVM_Reference * r) {
    if (r->internal)
        ((SMVM_MemorySlot *) r->internal)->nrefs--;
}

inline void SMVM_CReference_destroy(SMVM_CReference * r) {
    if (r->internal)
        ((SMVM_MemorySlot *) r->internal)->nrefs--;
}

SM_VECTOR_DECLARE(SMVM_ReferenceVector,SMVM_Reference,,inline)
SM_VECTOR_DEFINE(SMVM_ReferenceVector,SMVM_Reference,malloc,free,realloc,inline)
SM_VECTOR_DECLARE(SMVM_CReferenceVector,SMVM_CReference,,inline)
SM_VECTOR_DEFINE(SMVM_CReferenceVector,SMVM_CReference,malloc,free,realloc,inline)


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

#ifndef SMVM_FAST_BUILD
SM_VECTOR_DECLARE(SMVM_RegisterVector,SMVM_CodeBlock,,inline)
SM_VECTOR_DEFINE(SMVM_RegisterVector,SMVM_CodeBlock,malloc,free,realloc,inline)
#else
SM_VECTOR_DECLARE(SMVM_RegisterVector,SMVM_CodeBlock,,)
#endif

struct _SMVM_StackFrame {
    SMVM_RegisterVector stack;
    SMVM_ReferenceVector refstack;
    SMVM_CReferenceVector crefstack;
    struct _SMVM_StackFrame * prev;

    const SMVM_CodeBlock * returnAddr;
    SMVM_CodeBlock * returnValueAddr;

#ifdef SMVM_DEBUG
    size_t lastCallIp;
    size_t lastCallSection;
#endif
};
typedef struct _SMVM_StackFrame SMVM_StackFrame;

void SMVM_StackFrame_init(SMVM_StackFrame * f, SMVM_StackFrame * prev) __attribute__ ((nonnull(1)));
void SMVM_StackFrame_destroy(SMVM_StackFrame * f) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SMVM_CodeSection
********************************************************************************/

typedef struct {
    size_t offset;
    SMVM_CodeBlock originalInstruction;
} SMVM_Breakpoint;

#ifndef SMVM_FAST_BUILD
SM_VECTOR_DECLARE(SMVM_BreakpointVector,SMVM_Breakpoint,,inline)
SM_VECTOR_DEFINE(SMVM_BreakpointVector,SMVM_Breakpoint,malloc,free,realloc,inline)
SM_INSTRSET_DECLARE(SMVM_InstrSet,inline)
SM_INSTRSET_DEFINE(SMVM_InstrSet,malloc,free,inline)
#else
SM_VECTOR_DECLARE(SMVM_BreakpointVector,SMVM_Breakpoint,,)
SM_INSTRSET_DECLARE(SMVM_InstrSet,)
#endif

typedef struct {
    SMVM_CodeBlock * data;
    size_t size;
    SMVM_BreakpointVector breakPoints;
    SMVM_InstrSet instrset;
} SMVM_CodeSection;

int SMVM_CodeSection_init(SMVM_CodeSection * s,
                          const SMVM_CodeBlock * const code,
                          const size_t codeSize) __attribute__ ((nonnull(1)));

void SMVM_CodeSection_destroy(SMVM_CodeSection * const s) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SMVM_DataSection
********************************************************************************/

typedef SMVM_MemorySlot SMVM_DataSection;

int SMVM_DataSection_init(SMVM_DataSection * ds, size_t size, SMVM_MemorySlotSpecials * specials);
void SMVM_DataSection_destroy(SMVM_DataSection * ds);


/*******************************************************************************
 *  SMVM_Program
********************************************************************************/

#ifndef SMVM_SOFT_FLOAT
typedef enum {
    SMVM_HET_FPE_UNKNOWN = 0x00, /* Unknown arithmetic or floating-point error */
    SMVM_HET_FPE_INTDIV  = 0x01, /* integer divide by zero */
    SMVM_HET_FPE_INTOVF  = 0x02, /* integer overflow */
    SMVM_HET_FPE_FLTDIV  = 0x03, /* floating-point divide by zero */
    SMVM_HET_FPE_FLTOVF  = 0x04, /* floating-point overflow */
    SMVM_HET_FPE_FLTUND  = 0x05, /* floating-point underflow */
    SMVM_HET_FPE_FLTRES  = 0x06, /* floating-point inexact result */
    SMVM_HET_FPE_FLTINV  = 0x07, /* floating-point invalid operation */
    _SMVM_HET_COUNT
} SMVM_HardwareExceptionType;
#endif

#ifndef SMVM_FAST_BUILD
SM_VECTOR_DECLARE(SMVM_CodeSectionsVector,SMVM_CodeSection,,inline)
SM_VECTOR_DEFINE(SMVM_CodeSectionsVector,SMVM_CodeSection,malloc,free,realloc,inline)
SM_VECTOR_DECLARE(SMVM_DataSectionsVector,SMVM_DataSection,,inline)
SM_VECTOR_DEFINE(SMVM_DataSectionsVector,SMVM_DataSection,malloc,free,realloc,inline)
SM_STACK_DECLARE(SMVM_FrameStack,SMVM_StackFrame,,inline)
SM_STACK_DEFINE(SMVM_FrameStack,SMVM_StackFrame,malloc,free,inline)
#else
SM_VECTOR_DECLARE(SMVM_CodeSectionsVector,SMVM_CodeSection,,)
SM_VECTOR_DECLARE(SMVM_DataSectionsVector,SMVM_DataSection,,)
SM_STACK_DECLARE(SMVM_FrameStack,SMVM_StackFrame,,)
#endif

typedef struct {
    const SMVM_Context_Syscall * syscall;
    SMVM_Program * program;
} SMVM_SyscallContextInternal;


#ifndef SMVM_FAST_BUILD
SM_MAP_DECLARE(SMVM_PrivateMemoryMap,void*,void * const,size_t,inline)
SM_MAP_DEFINE(SMVM_PrivateMemoryMap,void*,void * const,size_t,fnv_16a_buf(key,sizeof(void *)),SM_MAP_KEY_EQUALS_voidptr,SM_MAP_KEY_LESS_THAN_voidptr,SM_MAP_KEYCOPY_REGULAR,SM_MAP_KEYFREE_REGULAR,malloc,free,inline)
#else
SM_MAP_DECLARE(SMVM_PrivateMemoryMap,void*,void * const,size_t,)
#endif

struct _SMVM_Program {
    SMVM_State state;
    SMVM_Error error;

    SMVM_CodeSectionsVector codeSections;
    SMVM_DataSectionsVector rodataSections;
    SMVM_DataSectionsVector dataSections;
    SMVM_DataSectionsVector bssSections;

    SMVM_SyscallBindings bindings;
    SMVM_PdBindings pdBindings;

    SMVM_FrameStack frames;
    SMVM_StackFrame * globalFrame;
    SMVM_StackFrame * nextFrame;
    SMVM_StackFrame * thisFrame;

    SMVM_MemoryMap memoryMap;
    uint64_t memorySlotNext;
    SMVM_PrivateMemoryMap privateMemoryMap;

    size_t currentCodeSectionIndex;
    uintptr_t currentIp;

    SMVM_CodeBlock returnValue;
    int64_t exceptionValue;

    SMVM * smvm;
    SMVM_MODAPI_0x1_Syscall_Context syscallContext;
    SMVM_SyscallContextInternal syscallContextInternal;

    size_t memPrivate;
    size_t memReserved;

#ifndef SMVM_SOFT_FLOAT
    int hasSavedFpeEnv;
    fenv_t savedFpeEnv;
#ifdef __USE_POSIX
    sigjmp_buf safeJmpBuf[_SMVM_HET_COUNT];
    struct sigaction oldFpeAction;
#else
    jmp_buf safeJmpBuf[_SMVM_HET_COUNT];
    void * oldFpeAction;
#endif
#endif

#ifdef SMVM_DEBUG
    FILE * debugFileHandle;
#endif
};

#ifdef SMVM_DEBUG
void SMVM_RegisterVector_printStateBencoded(SMVM_RegisterVector * const v, FILE * const f) __attribute__ ((nonnull(1)));
void SMVM_StackFrame_printStateBencoded(SMVM_StackFrame * const s, FILE * const f) __attribute__ ((nonnull(1)));
void SMVM_Program_printStateBencoded(SMVM_Program * const p, FILE * const f) __attribute__ ((nonnull(1)));
#endif /* SMVM_DEBUG */

uint64_t SMVM_Program_public_alloc(SMVM_Program * p, uint64_t nBytes, SMVM_MemorySlot ** memorySlot) __attribute__ ((nonnull(1)));
SMVM_Exception SMVM_Program_public_free(SMVM_Program * p, uint64_t ptr) __attribute__ ((nonnull(1)));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_VM_INTERNAL_HELPERS_H */
