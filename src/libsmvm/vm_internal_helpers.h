/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef LIBSMVM_VM_INTERNAL_HELPERS_H
#define LIBSMVM_VM_INTERNAL_HELPERS_H

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
#include "../instrset.h"
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

SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,SMVM_MemorySlot,inline)
SM_MAP_DEFINE(SMVM_MemoryMap,uint64_t,SMVM_MemorySlot,(uint16_t),malloc,free,inline)
#else
SMVM_MemorySlot_init_DECLARE;
SMVM_MemorySlot_destroy_DECLARE;
SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,SMVM_MemorySlot,)
#endif


/*******************************************************************************
 *  SMVM_Reference
********************************************************************************/

typedef struct {
    void * pData;
    size_t size;
    SMVM_MemorySlot * pMemory;
} SMVM_Reference;
typedef struct {
    const void * pData;
    size_t size;
    SMVM_MemorySlot * pMemory;
} SMVM_CReference;

void SMVM_Reference_destroy(SMVM_Reference * r);
void SMVM_CReference_destroy(SMVM_CReference * r);

#ifndef SMVM_FAST_BUILD
SM_VECTOR_DECLARE(SMVM_ReferenceVector,SMVM_Reference,,inline)
SM_VECTOR_DEFINE(SMVM_ReferenceVector,SMVM_Reference,malloc,free,realloc,inline)
SM_VECTOR_DECLARE(SMVM_CReferenceVector,SMVM_CReference,,inline)
SM_VECTOR_DEFINE(SMVM_CReferenceVector,SMVM_CReference,malloc,free,realloc,inline)
#else
SM_VECTOR_DECLARE(SMVM_ReferenceVector,SMVM_Reference,,)
SM_VECTOR_DECLARE(SMVM_CReferenceVector,SMVM_CReference,,)
#endif


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

typedef struct {
    void * data;
    size_t size;
} SMVM_DataSection;

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

struct _SMVM_Program {
    SMVM_State state;
    SMVM_Error error;

    SMVM_CodeSectionsVector codeSections;
    SMVM_DataSectionsVector rodataSections;
    SMVM_DataSectionsVector dataSections;
    SMVM_DataSectionsVector bssSections;

    SMVM_FrameStack frames;
    SMVM_StackFrame * globalFrame;
    SMVM_StackFrame * nextFrame;
    SMVM_StackFrame * thisFrame;


    SMVM_MemoryMap memoryMap;
    uint64_t memorySlotsUsed;
    uint64_t memorySlotNext;

    size_t currentCodeSectionIndex;
    uintptr_t currentIp;

    SMVM_CodeBlock returnValue;
    int64_t exceptionValue;

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

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMVM_VM_INTERNAL_HELPERS_H */
