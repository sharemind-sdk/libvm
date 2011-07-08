/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "vm.h"
#include "vm_internal_helpers.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../likely.h"
#include "vm_internal_core.h"


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SVM_ENUM_DEFINE_TOSTRING(SVM_State, SVM_ENUM_State);
SVM_ENUM_DEFINE_TOSTRING(SVM_Runtime_State, SVM_ENUM_Runtime_State);
SVM_ENUM_CUSTOM_DEFINE_TOSTRING(SVM_Error, SVM_ENUM_Error);
SVM_ENUM_CUSTOM_DEFINE_TOSTRING(SVM_Exception, SVM_ENUM_Exception);

/*******************************************************************************
 *  SVM_StackFrame
********************************************************************************/

SVM_VECTOR_DEFINE(SVM_RegisterVector,union SVM_IBlock,malloc,free,realloc)

void SVM_StackFrame_init(struct SVM_StackFrame * f, struct SVM_StackFrame * prev) {
    assert(f);
    SVM_RegisterVector_init(&f->stack);
    f->prev = prev;

#ifdef SVM_DEBUG
    f->lastCallIp = 0u;
    f->lastCallSection = 0u;
#endif
}

void SVM_StackFrame_destroy(struct SVM_StackFrame * f) {
    assert(f);
    SVM_RegisterVector_destroy(&f->stack);
}

/*******************************************************************************
 *  SVM_CodeSection
********************************************************************************/

SVM_VECTOR_DEFINE(SVM_BreakpointVector,struct SVM_Breakpoint,malloc,free,realloc)

SVM_INSTRSET_DEFINE(SVM_InstrSet,malloc,free)

int SVM_CodeSection_init(struct SVM_CodeSection * s,
                                const union SVM_IBlock * const code,
                                const size_t codeSize)
{
    assert(s);

    /* Add space for an exception pointer: */
    size_t realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return 0;

    size_t memSize = realCodeSize * sizeof(union SVM_IBlock);
    if (unlikely(memSize / sizeof(union SVM_IBlock) != realCodeSize))
        return 0;


    s->data = malloc(memSize);
    if (unlikely(!s->data))
        return 0;

    s->size = codeSize;
    memcpy(s->data, code, memSize);

    SVM_InstrSet_init(&s->instrset);

    return 1;
}

void SVM_CodeSection_destroy(struct SVM_CodeSection * const s) {
    assert(s);
    free(s->data);
    SVM_InstrSet_destroy(&s->instrset);
}


/*******************************************************************************
 *  SVM_Program
********************************************************************************/

SVM_VECTOR_DEFINE(SVM_CodeSectionsVector,struct SVM_CodeSection,malloc,free,realloc)
SVM_STACK_DEFINE(SVM_FrameStack,struct SVM_StackFrame,malloc,free)

struct SVM_Program * SVM_Program_new() {
    struct SVM_Program * const p = malloc(sizeof(struct SVM_Program));
    if (likely(p)) {
        p->state = SVM_INITIALIZED;
        p->error = SVM_OK;
        SVM_CodeSectionsVector_init(&p->codeSections);
        SVM_FrameStack_init(&p->frames);
        p->currentCodeSectionIndex = 0u;
        p->currentIp = 0u;
#ifdef SVM_DEBUG
        p->debugFileHandle = stderr;
#endif
    }
    return p;
}

void SVM_Program_free(struct SVM_Program * const p) {
    assert(p);
    SVM_CodeSectionsVector_destroy_with(&p->codeSections, &SVM_CodeSection_destroy);
    SVM_FrameStack_destroy_with(&p->frames, &SVM_StackFrame_destroy);
    free(p);
}

int SVM_Program_addCodeSection(struct SVM_Program * const p,
                               const union SVM_IBlock * const code,
                               const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);

    if (unlikely(p->state != SVM_INITIALIZED))
        return SVM_INVALID_INPUT_STATE;

    struct SVM_CodeSection * s = SVM_CodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SVM_OUT_OF_MEMORY;

    if (unlikely(!SVM_CodeSection_init(s, code, codeSize)))
        return SVM_OUT_OF_MEMORY;

    return SVM_OK;
}

#define SVM_PREPARE_NOP (void)0
#define SVM_PREPARE_INVALID_INSTRUCTION SVM_PREPARE_ERROR(SVM_PREPARE_ERROR_INVALID_INSTRUCTION)
#define SVM_PREPARE_ERROR(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = i; \
        goto svm_prepare_codesection_return_error; \
    } else (void) 0
#define SVM_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SVM_PREPARE_ERROR(e); \
    } else (void) 0
#define SVM_PREPARE_END_AS(impl_type,index,numargs) \
    if (1) { \
        struct SVM_Prepare_IBlock pb = { .block = (union SVM_IBlock *) &c[i], .type = impl_type }; \
        pb.block->uint64[0] = index; \
        int64_t r = _SVM(p, SVM_I_GET_IMPL_LABEL, (void*) &pb); \
        assert(r == SVM_OK); \
        i += (numargs); \
        break; \
    } else (void) 0
#define SVM_FOREACH_PREPARE_PASS1_CASE(bytecode, args) \
    case bytecode: \
        SVM_PREPARE_CHECK_OR_ERROR(i + (args) < s->size, \
                                   SVM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        SVM_PREPARE_CHECK_OR_ERROR(SVM_InstrSet_insert(&s->instrset, SVM_PREPARE_CURRENT_I), \
                                   SVM_OUT_OF_MEMORY); \
        i += (args); \
        break;
#define SVM_PREPARE_ARG_AS(v,t) (c[i+(v)].t[0])
#define SVM_PREPARE_CURRENT_I i
#define SVM_PREPARE_CODESIZE(s) (s)->size
#define SVM_PREPARE_CURRENT_CODE_SECTION s

#define SVM_PREPARE_IS_INSTR(addr) SVM_InstrSet_contains(&s->instrset, (addr))

int SVM_Program_endPrepare(struct SVM_Program * const p) {
    assert(p);

    if (unlikely(p->state != SVM_INITIALIZED))
        return SVM_INVALID_INPUT_STATE;

    if (unlikely(!p->codeSections.size))
        return SVM_PREPARE_ERROR_NO_CODE_SECTION;

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        int returnCode = SVM_OK;
        struct SVM_CodeSection * s = &p->codeSections.data[j];

        union SVM_IBlock * c = s->data;

        /* Initialize instructions hashmap: */
        for (size_t i = 0u; i < s->size; i++) {
            switch (c[i].uint64[0]) {
#include "../m4/preprocess_pass1_cases.h"
                default:
                    SVM_PREPARE_INVALID_INSTRUCTION;
            }
        }

        for (size_t i = 0u; i < s->size; i++) {
            switch (c[i].uint64[0]) {
#include "../m4/preprocess_pass2_cases.h"
                default:
                    SVM_PREPARE_INVALID_INSTRUCTION;
            }
        }

        /* Initialize exception pointer: */
        c[s->size].uint64[0] = 0u;
        struct SVM_Prepare_IBlock pb = { .block = &c[s->size], .type = 2 };
        int r = _SVM(p, SVM_I_GET_IMPL_LABEL, &pb);
        assert(r == SVM_OK);

        continue;

    svm_prepare_codesection_return_error:
        p->currentCodeSectionIndex = j;
        return returnCode;
    }

    p->globalFrame = SVM_FrameStack_push(&p->frames);
    if (unlikely(!p->globalFrame))
        return SVM_OUT_OF_MEMORY;

    /* Initialize root stack frame: */
    SVM_StackFrame_init(p->globalFrame, NULL);
    p->globalFrame->returnValueAddr = &(p->returnValue);

    p->thisFrame = p->globalFrame;
    p->nextFrame = NULL;

    p->state = SVM_PREPARED;

    return SVM_OK;
}

int SVM_Program_run(struct SVM_Program * const program) {
    assert(program);

    if (unlikely(program->state != SVM_PREPARED))
        return SVM_INVALID_INPUT_STATE;

    program->currentCodeSectionIndex = 0u;
    program->currentIp = 0u;
    return _SVM(program, SVM_I_RUN, NULL);
}

int64_t SVM_Program_get_return_value(struct SVM_Program *p) {
    return p->returnValue;
}

int64_t SVM_Program_get_exception_value(struct SVM_Program *p) {
    return p->exceptionValue;
}

size_t SVM_Program_get_current_codesection(struct SVM_Program *p) {
    return p->currentCodeSectionIndex;
}

size_t SVM_Program_get_current_ip(struct SVM_Program *p) {
    return p->currentIp;
}

#ifdef SVM_DEBUG

void SVM_RegisterVector_printStateBencoded(struct SVM_RegisterVector * const v, FILE * const f) {
    fprintf(f, "l");
    for (size_t i = 0u; i < v->size; i++)
        fprintf(f, "i%" PRIu64 "e", v->data[i].uint64[0]);
    fprintf(f, "e");
}

void SVM_StackFrame_printStateBencoded(struct SVM_StackFrame * const s, FILE * const f) {
    assert(s);
    fprintf(f, "d");
    fprintf(f, "5:Stack");
    SVM_RegisterVector_printStateBencoded(&s->stack, f);
    fprintf(f, "10:LastCallIpi%zue", s->lastCallIp);
    fprintf(f, "15:LastCallSectioni%ze", s->lastCallSection);
    fprintf(f, "e");
}

void SVM_Program_printStateBencoded(struct SVM_Program * const p, FILE * const f) {
    assert(p);
    fprintf(f, "d");
        fprintf(f, "5:Statei%de", p->state);
        fprintf(f, "5:Errori%de", p->error);
        fprintf(f, "12:RuntimeStatei%de", p->runtimeState);
        fprintf(f, "18:CurrentCodeSectioni%zue", p->currentCodeSectionIndex);
        fprintf(f, "9:CurrentIPi%zue", p->currentIp);
        fprintf(f, "12:CurrentStackl");
        for (struct SVM_FrameStack_item * s = p->frames.d; s != NULL; s = s->prev)
            SVM_StackFrame_printStateBencoded(&s->value, f);
        fprintf(f, "e");
        fprintf(f, "9:NextStackl");
        if (p->nextFrame)
            SVM_StackFrame_printStateBencoded(p->nextFrame, f);
        fprintf(f, "e");
    fprintf(f, "e");
}

#endif /* SVM_DEBUG */
