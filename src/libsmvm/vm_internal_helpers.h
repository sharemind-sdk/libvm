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

#include "../instrset.h"
#include "../stack.h"
#include "../vector.h"


#ifdef __cplusplus
extern "C" {
#endif

struct SVM_Prepare_IBlock {
    union SVM_IBlock * block;
    unsigned type;
};

enum SVM_InnerCommand {
    SVM_I_GET_IMPL_LABEL,
    SVM_I_RUN,
    SVM_I_CONTINUE
};


/*******************************************************************************
 *  SVM_StackFrame
********************************************************************************/

SVM_VECTOR_DECLARE(SVM_RegisterVector,union SVM_IBlock,)

struct SVM_StackFrame {
    struct SVM_RegisterVector stack;
    struct SVM_StackFrame * prev;

    union SVM_IBlock * returnAddr;
    int64_t * returnValueAddr;

#ifdef SVM_DEBUG
    size_t lastCallIp;
    size_t lastCallSection;
#endif
};

void SVM_StackFrame_init(struct SVM_StackFrame * f, struct SVM_StackFrame * prev) __attribute__ ((nonnull(1)));
void SVM_StackFrame_destroy(struct SVM_StackFrame * f) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SVM_CodeSection
********************************************************************************/

struct SVM_Breakpoint {
    size_t offset;
    union SVM_IBlock originalInstruction;
};

SVM_VECTOR_DECLARE(SVM_BreakpointVector,struct SVM_Breakpoint,)
SVM_INSTRSET_DECLARE(SVM_InstrSet)

struct SVM_CodeSection {
    union SVM_IBlock * data;
    size_t size;
    struct SVM_BreakpointVector breakPoints;
    struct SVM_InstrSet instrset;
};

int SVM_CodeSection_init(struct SVM_CodeSection * s,
                        const union SVM_IBlock * const code,
                        const size_t codeSize) __attribute__ ((nonnull(1)));

void SVM_CodeSection_destroy(struct SVM_CodeSection * const s) __attribute__ ((nonnull(1)));


/*******************************************************************************
 *  SVM_Program
********************************************************************************/

SVM_VECTOR_DECLARE(SVM_CodeSectionsVector,struct SVM_CodeSection,)
SVM_STACK_DECLARE(SVM_FrameStack,struct SVM_StackFrame,)

struct SVM_Program {
    enum SVM_State state;
    enum SVM_Error error;
    enum SVM_Runtime_State runtimeState;

    struct SVM_CodeSectionsVector codeSections;
    struct SVM_FrameStack frames;
    struct SVM_StackFrame * globalFrame;
    struct SVM_StackFrame * nextFrame;
    struct SVM_StackFrame * thisFrame;

    size_t currentCodeSectionIndex;
    size_t currentIp;

    int64_t returnValue;
    int64_t exceptionValue;

#ifdef SVM_DEBUG
    FILE * debugFileHandle;
#endif
};

#ifdef SVM_DEBUG
void SVM_RegisterVector_printStateBencoded(struct SVM_RegisterVector * const v, FILE * const f) __attribute__ ((nonnull(1)));
void SVM_StackFrame_printStateBencoded(struct SVM_StackFrame * const s, FILE * const f) __attribute__ ((nonnull(1)));
void SVM_Program_printStateBencoded(struct SVM_Program * const p, FILE * const f) __attribute__ ((nonnull(1)));
#endif /* SVM_DEBUG */

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* LIBSMVM_VM_INTERNAL_HELPERS_H */
