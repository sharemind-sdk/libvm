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

#ifdef SMVM_DEBUG
#include <stdio.h>
#endif /* SMVM_DEBUG */
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

SM_MAP_DECLARE(SMVM_MemoryMap,uint64_t,struct SMVM_MemorySlot)


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

struct SMVM_Reference {
    struct SMVM_MemorySlot * pMemory;
    uint64_t dataIndexOnPreviousStack;
    size_t offset;
    size_t size;
};


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

SM_VECTOR_DECLARE(SMVM_RegisterVector,union SM_CodeBlock,)

struct SMVM_StackFrame {
    struct SMVM_RegisterVector stack;
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

SM_VECTOR_DECLARE(SMVM_BreakpointVector,struct SMVM_Breakpoint,)
SM_INSTRSET_DECLARE(SMVM_InstrSet)

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

SM_VECTOR_DECLARE(SMVM_CodeSectionsVector,struct SMVM_CodeSection,)
SM_STACK_DECLARE(SMVM_FrameStack,struct SMVM_StackFrame,)

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
