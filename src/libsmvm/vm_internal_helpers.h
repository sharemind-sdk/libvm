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

#include <setjmp.h>
#ifdef SMVM_DEBUG
#include <stdio.h>
#endif /* SMVM_DEBUG */
#include <stdlib.h>
#include "../instrset.h"
#include "../map.h"
#include "../stack.h"
#include "../vector.h"


#ifdef __cplusplus
extern "C" {
#endif

struct SMVM_Prepare_IBlock {
    union SM_CodeBlock * block;
    unsigned type;
};

enum SMVM_InnerCommand {
    SMVM_I_GET_IMPL_LABEL,
    SMVM_I_RUN,
    SMVM_I_CONTINUE
};


/*******************************************************************************
 *  SMVM_MemoryMap
********************************************************************************/

struct SMVM_MemorySlot {
    uint64_t nrefs;
    size_t size;
    void * pData;
};

#ifdef SMVM_RELEASE
SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,struct SMVM_MemorySlot,inline)
SM_MAP_DEFINE(SMVM_MemoryMap,uint64_t,struct SMVM_MemorySlot,(uint16_t),malloc,free,inline)
#else
SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,struct SMVM_MemorySlot,)
#endif


/*******************************************************************************
 *  SMVM_Reference
********************************************************************************/

struct SMVM_Reference {
    struct SMVM_MemorySlot * pMemory;
    union SM_CodeBlock * pBlock;
    size_t offset;
    size_t size;
};

int SMVM_Reference_deallocator(struct SMVM_Reference * r);
void SMVM_Reference_destroy(struct SMVM_Reference * r);

#ifdef SMVM_RELEASE
SM_VECTOR_DECLARE(SMVM_ReferenceVector,struct SMVM_Reference,,inline)
SM_VECTOR_DEFINE(SMVM_ReferenceVector,struct SMVM_Reference,malloc,free,realloc,inline)
#else
SM_VECTOR_DECLARE(SMVM_ReferenceVector,struct SMVM_Reference,,)
#endif


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

#ifdef SMVM_RELEASE
SM_VECTOR_DECLARE(SMVM_RegisterVector,union SM_CodeBlock,,inline)
SM_VECTOR_DEFINE(SMVM_RegisterVector,union SM_CodeBlock,malloc,free,realloc,inline)
#else
SM_VECTOR_DECLARE(SMVM_RegisterVector,union SM_CodeBlock,,)
#endif

struct SMVM_StackFrame {
    struct SMVM_RegisterVector stack;
    struct SMVM_ReferenceVector refstack;
    struct SMVM_ReferenceVector crefstack;
    struct SMVM_StackFrame * prev;

    union SM_CodeBlock * returnAddr;
    union SM_CodeBlock * returnValueAddr;

#ifdef SMVM_DEBUG
    size_t lastCallIp;
    size_t lastCallSection;
#endif
};

void SMVM_StackFrame_init(struct SMVM_StackFrame * f, struct SMVM_StackFrame * prev) __attribute__ ((nonnull(1)));
void SMVM_StackFrame_destroy(struct SMVM_StackFrame * f) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SMVM_CodeSection
********************************************************************************/

struct SMVM_Breakpoint {
    size_t offset;
    union SM_CodeBlock originalInstruction;
};

#ifdef SMVM_RELEASE
SM_VECTOR_DECLARE(SMVM_BreakpointVector,struct SMVM_Breakpoint,,inline)
SM_VECTOR_DEFINE(SMVM_BreakpointVector,struct SMVM_Breakpoint,malloc,free,realloc,inline)
SM_INSTRSET_DECLARE(SMVM_InstrSet,inline)
SM_INSTRSET_DEFINE(SMVM_InstrSet,malloc,free,inline)
#else
SM_VECTOR_DECLARE(SMVM_BreakpointVector,struct SMVM_Breakpoint,,)
SM_INSTRSET_DECLARE(SMVM_InstrSet,)
#endif

struct SMVM_CodeSection {
    union SM_CodeBlock * data;
    size_t size;
    struct SMVM_BreakpointVector breakPoints;
    struct SMVM_InstrSet instrset;
};

int SMVM_CodeSection_init(struct SMVM_CodeSection * s,
                          const union SM_CodeBlock * const code,
                          const size_t codeSize) __attribute__ ((nonnull(1)));

void SMVM_CodeSection_destroy(struct SMVM_CodeSection * const s) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SMVM_Program
********************************************************************************/

#ifdef SMVM_RELEASE
SM_VECTOR_DECLARE(SMVM_CodeSectionsVector,struct SMVM_CodeSection,,inline)
SM_VECTOR_DEFINE(SMVM_CodeSectionsVector,struct SMVM_CodeSection,malloc,free,realloc,inline)
SM_STACK_DECLARE(SMVM_FrameStack,struct SMVM_StackFrame,,inline)
SM_STACK_DEFINE(SMVM_FrameStack,struct SMVM_StackFrame,malloc,free,inline)
#else
SM_VECTOR_DECLARE(SMVM_CodeSectionsVector,struct SMVM_CodeSection,,)
SM_STACK_DECLARE(SMVM_FrameStack,struct SMVM_StackFrame,,)
#endif

struct SMVM_Program {
    enum SMVM_State state;
    enum SMVM_Error error;

    struct SMVM_CodeSectionsVector codeSections;

    struct SMVM_FrameStack frames;
    struct SMVM_StackFrame * globalFrame;
    struct SMVM_StackFrame * nextFrame;
    struct SMVM_StackFrame * thisFrame;


    struct SMVM_MemoryMap memoryMap;
    uint64_t memorySlotsUsed;
    uint64_t memorySlotNext;

    size_t currentCodeSectionIndex;
    size_t currentIp;

    union SM_CodeBlock returnValue;
    int64_t exceptionValue;

    sigjmp_buf safeJmpBuf[_SMVM_HET_COUNT];

#ifdef SMVM_DEBUG
    FILE * debugFileHandle;
#endif
};

#ifdef SMVM_DEBUG
void SMVM_RegisterVector_printStateBencoded(struct SMVM_RegisterVector * const v, FILE * const f) __attribute__ ((nonnull(1)));
void SMVM_StackFrame_printStateBencoded(struct SMVM_StackFrame * const s, FILE * const f) __attribute__ ((nonnull(1)));
void SMVM_Program_printStateBencoded(struct SMVM_Program * const p, FILE * const f) __attribute__ ((nonnull(1)));
#endif /* SMVM_DEBUG */

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMVM_VM_INTERNAL_HELPERS_H */
