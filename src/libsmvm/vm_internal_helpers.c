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
#include <string.h>
#include "../libsme/libsme.h"
#include "../libsme/libsme_0x0.h"
#include "../likely.h"
#include "vm_internal_core.h"

#ifdef SMVM_DEBUG
#include <inttypes.h>
#include "../libsmvmi/instr.h"
#endif /* SMVM_DEBUG */


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SM_ENUM_DEFINE_TOSTRING(SMVM_State, SMVM_ENUM_State);
SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMVM_Error, SMVM_ENUM_Error);
SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMVM_Exception, SMVM_ENUM_Exception);


/*******************************************************************************
 *  SMVM_MemoryMap
********************************************************************************/

#ifndef SMVM_RELEASE
SM_MAP_DEFINE(SMVM_MemoryMap,uint64_t,struct SMVM_MemorySlot,(uint16_t),malloc,free,)
#endif

int SMVM_MemoryMap_deallocator(struct SMVM_MemorySlot * s) {
    free(s->pData);
    return 1;
}


/*******************************************************************************
 *  SMVM_Reference
********************************************************************************/

#ifndef SMVM_RELEASE
SM_VECTOR_DEFINE(SMVM_ReferenceVector,struct SMVM_Reference,malloc,free,realloc,)
#endif

int SMVM_Reference_deallocator(struct SMVM_Reference * r) {
    if (!r->pBlock) {
        assert(r->pMemory);
        r->pMemory->nrefs--;
    } else {
        assert(!r->pMemory);
    }
    return 1;
}

void SMVM_Reference_destroy(struct SMVM_Reference * r) {
    SMVM_Reference_deallocator(r);
}


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

#ifndef SMVM_RELEASE
SM_VECTOR_DEFINE(SMVM_RegisterVector,union SM_CodeBlock,malloc,free,realloc,)
#endif

void SMVM_StackFrame_init(struct SMVM_StackFrame * f, struct SMVM_StackFrame * prev) {
    assert(f);
    SMVM_RegisterVector_init(&f->stack);
    SMVM_ReferenceVector_init(&f->refstack);
    SMVM_ReferenceVector_init(&f->crefstack);
    f->prev = prev;

#ifdef SMVM_DEBUG
    f->lastCallIp = 0u;
    f->lastCallSection = 0u;
#endif
}

void SMVM_StackFrame_destroy(struct SMVM_StackFrame * f) {
    assert(f);
    SMVM_RegisterVector_destroy(&f->stack);
    SMVM_ReferenceVector_destroy_with(&f->refstack, &SMVM_Reference_destroy);
    SMVM_ReferenceVector_destroy_with(&f->crefstack, &SMVM_Reference_destroy);
}

/*******************************************************************************
 *  SMVM_CodeSection
********************************************************************************/

#ifndef SMVM_RELEASE
SM_VECTOR_DEFINE(SMVM_BreakpointVector,struct SMVM_Breakpoint,malloc,free,realloc,)
SM_INSTRSET_DEFINE(SMVM_InstrSet,malloc,free,)
#endif

int SMVM_CodeSection_init(struct SMVM_CodeSection * s,
                          const union SM_CodeBlock * const code,
                          const size_t codeSize)
{
    assert(s);

    /* Add space for an exception pointer: */
    size_t realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return 0;

    size_t memSize = realCodeSize * sizeof(union SM_CodeBlock);
    if (unlikely(memSize / sizeof(union SM_CodeBlock) != realCodeSize))
        return 0;


    s->data = malloc(memSize);
    if (unlikely(!s->data))
        return 0;

    s->size = codeSize;
    memcpy(s->data, code, memSize);

    SMVM_InstrSet_init(&s->instrset);

    return 1;
}

void SMVM_CodeSection_destroy(struct SMVM_CodeSection * const s) {
    assert(s);
    free(s->data);
    SMVM_InstrSet_destroy(&s->instrset);
}


/*******************************************************************************
 *  SMVM_Program
********************************************************************************/

#ifndef SMVM_RELEASE
SM_VECTOR_DEFINE(SMVM_CodeSectionsVector,struct SMVM_CodeSection,malloc,free,realloc,)
SM_STACK_DEFINE(SMVM_FrameStack,struct SMVM_StackFrame,malloc,free,)
#endif

struct SMVM_Program * SMVM_Program_new() {
    struct SMVM_Program * const p = malloc(sizeof(struct SMVM_Program));
    if (likely(p)) {
        p->state = SMVM_INITIALIZED;
        p->error = SMVM_OK;
        SMVM_CodeSectionsVector_init(&p->codeSections);
        SMVM_FrameStack_init(&p->frames);
        SMVM_MemoryMap_init(&p->memoryMap);
        p->memorySlotsUsed = 0u;
        p->memorySlotNext = 1u;
        p->currentCodeSectionIndex = 0u;
        p->currentIp = 0u;
#ifdef SMVM_DEBUG
        p->debugFileHandle = stderr;
#endif
    }
    return p;
}

void SMVM_Program_free(struct SMVM_Program * const p) {
    assert(p);
    SMVM_CodeSectionsVector_destroy_with(&p->codeSections, &SMVM_CodeSection_destroy);
    SMVM_FrameStack_destroy_with(&p->frames, &SMVM_StackFrame_destroy);

    SMVM_MemoryMap_foreach(&p->memoryMap, &SMVM_MemoryMap_deallocator);
    SMVM_MemoryMap_destroy(&p->memoryMap);
    free(p);
}

int SMVM_Program_load_from_sme(struct SMVM_Program *p, const void * data, size_t dataSize) {
    assert(p);
    assert(data);

    if (dataSize < sizeof(struct SME_Common_Header))
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

    const struct SME_Common_Header * ch;
    if (SME_Common_Header_read(data, &ch) != SME_READ_OK)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

    if (ch->file_format_version > 0u)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE; /** \todo new error code? */


    const void * pos = data + sizeof(struct SME_Common_Header);

    const struct SME_Header_0x0 * h;
    if (SME_Header_0x0_read(pos, &h) != SME_READ_OK)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;
    pos += sizeof(struct SME_Header_0x0);

    for (unsigned ui = 0; ui <= h->number_of_units_minus_one; ui++) {
        const struct SME_Unit_Header_0x0 * uh;
        if (SME_Unit_Header_0x0_read(pos, &uh) != SME_READ_OK)
            return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

        pos += sizeof(struct SME_Unit_Header_0x0);
        for (unsigned si = 0; si <= uh->sections_minus_one; si++) {
            const struct SME_Section_Header_0x0 * sh;
            if (SME_Section_Header_0x0_read(pos, &sh) != SME_READ_OK)
                return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

            pos += sizeof(struct SME_Section_Header_0x0);

            enum SME_Section_Type type = SME_Section_Header_0x0_type(sh);
            assert(type != (enum SME_Section_Type) -1);
            if (type == SME_SECTION_TYPE_TEXT) {
                union {
                    const void * v;
                    const union SM_CodeBlock * cb;
                } c = { .v = pos };

                int r = SMVM_Program_addCodeSection(p, c.cb, sh->length);
                if (r != SMVM_OK)
                    return r;

                pos += sh->length * sizeof(union SM_CodeBlock);
            } else {
                /** \todo also add other sections. */
                abort();
                /*
                int r = SMVM_Program_addSection(p, pos, sh->length);
                if (r != SMVM_OK)
                    return r;

                pos += sh->length;
                */
            }
        }
    }
    p->currentCodeSectionIndex = h->active_linking_unit;

    return SMVM_Program_endPrepare(p);
}

#ifdef SMVM_DEBUG
static void printCodeSection(FILE * stream, const union SM_CodeBlock * code, size_t size, const char * linePrefix);
#endif /* SMVM_DEBUG */

int SMVM_Program_addCodeSection(struct SMVM_Program * const p,
                                const union SM_CodeBlock * const code,
                                const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);

    if (unlikely(p->state != SMVM_INITIALIZED))
        return SMVM_INVALID_INPUT_STATE;

    struct SMVM_CodeSection * s = SMVM_CodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SMVM_OUT_OF_MEMORY;

    if (unlikely(!SMVM_CodeSection_init(s, code, codeSize)))
        return SMVM_OUT_OF_MEMORY;

    #ifdef SMVM_DEBUG
    fprintf(stderr, "Added code section %zu:\n", p->codeSections.size - 1);
    printCodeSection(stderr, code, codeSize, "    ");
    #endif /* SMVM_DEBUG */

    return SMVM_OK;
}

#define SMVM_PREPARE_NOP (void)0
#define SMVM_PREPARE_INVALID_INSTRUCTION SMVM_PREPARE_ERROR(SMVM_PREPARE_ERROR_INVALID_INSTRUCTION)
#define SMVM_PREPARE_ERROR(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = i; \
        goto smvm_prepare_codesection_return_error; \
    } else (void) 0
#define SMVM_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SMVM_PREPARE_ERROR(e); \
    } else (void) 0
#define SMVM_PREPARE_END_AS(index,numargs) \
    if (1) { \
        c[i].uint64[0] = (index); \
        struct SMVM_Prepare_IBlock pb = { .block = &c[i], .type = 0 }; \
        if (_SMVM(p, SMVM_I_GET_IMPL_LABEL, (void *) &pb) != SMVM_OK) \
            abort(); \
        i += (numargs); \
        break; \
    } else (void) 0
#define SMVM_FOREACH_PREPARE_PASS1_CASE(bytecode, args) \
    case bytecode: \
        SMVM_PREPARE_CHECK_OR_ERROR(i + (args) < s->size, \
                                    SMVM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        SMVM_PREPARE_CHECK_OR_ERROR(SMVM_InstrSet_insert(&s->instrset, SMVM_PREPARE_CURRENT_I), \
                                    SMVM_OUT_OF_MEMORY); \
        i += (args); \
        break;
#define SMVM_PREPARE_ARG_AS(v,t) (c[i+(v)].t[0])
#define SMVM_PREPARE_CURRENT_I i
#define SMVM_PREPARE_CODESIZE(s) (s)->size
#define SMVM_PREPARE_CURRENT_CODE_SECTION s

#define SMVM_PREPARE_IS_INSTR(addr) SMVM_InstrSet_contains(&s->instrset, (addr))
#define SMVM_PREPARE_IS_EXCEPTIONCODE(c) (SMVM_Exception_toString((c)) != NULL)

int SMVM_Program_endPrepare(struct SMVM_Program * const p) {
    assert(p);

    if (unlikely(p->state != SMVM_INITIALIZED))
        return SMVM_INVALID_INPUT_STATE;

    if (unlikely(!p->codeSections.size))
        return SMVM_PREPARE_ERROR_NO_CODE_SECTION;

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        int returnCode = SMVM_OK;
        struct SMVM_CodeSection * s = &p->codeSections.data[j];

        union SM_CodeBlock * c = s->data;

        /* Initialize instructions hashmap: */
        for (size_t i = 0u; i < s->size; i++) {
            switch (c[i].uint64[0]) {
#include "../m4/preprocess_pass1_cases.h"
                default:
                    SMVM_PREPARE_INVALID_INSTRUCTION;
            }
        }

        for (size_t i = 0u; i < s->size; i++) {
            switch (c[i].uint64[0]) {
#include "../m4/preprocess_pass2_cases.h"
                default:
                    SMVM_PREPARE_INVALID_INSTRUCTION;
            }
        }

        /* Initialize exception pointer: */
        c[s->size].uint64[0] = 0u; /* && eof */
        struct SMVM_Prepare_IBlock pb = { .block = &c[s->size], .type = 2 };
        if (_SMVM(p, SMVM_I_GET_IMPL_LABEL, &pb) != SMVM_OK)
            abort();

        continue;

    smvm_prepare_codesection_return_error:
        p->currentCodeSectionIndex = j;
        return returnCode;
    }

    p->globalFrame = SMVM_FrameStack_push(&p->frames);
    if (unlikely(!p->globalFrame))
        return SMVM_OUT_OF_MEMORY;

    /* Initialize global frame: */
    SMVM_StackFrame_init(p->globalFrame, NULL);
    p->globalFrame->returnAddr = NULL; /* Triggers halt on return. */
    /* p->globalFrame->returnValueAddr = &(p->returnValue); is not needed. */

    p->thisFrame = p->globalFrame;
    p->nextFrame = NULL;

    p->state = SMVM_PREPARED;

    return SMVM_OK;
}

int SMVM_Program_run(struct SMVM_Program * const program) {
    assert(program);

    if (unlikely(program->state != SMVM_PREPARED))
        return SMVM_INVALID_INPUT_STATE;

    return _SMVM(program, SMVM_I_RUN, NULL);
}

int64_t SMVM_Program_get_return_value(struct SMVM_Program *p) {
    return p->returnValue.int64[0];
}

int64_t SMVM_Program_get_exception_value(struct SMVM_Program *p) {
    return p->exceptionValue;
}

size_t SMVM_Program_get_current_codesection(struct SMVM_Program *p) {
    return p->currentCodeSectionIndex;
}

size_t SMVM_Program_get_current_ip(struct SMVM_Program *p) {
    return p->currentIp;
}

/*******************************************************************************
 *  Debugging
********************************************************************************/

#ifdef SMVM_DEBUG

void SMVM_RegisterVector_printStateBencoded(struct SMVM_RegisterVector * const v, FILE * const f) {
    fprintf(f, "l");
    for (size_t i = 0u; i < v->size; i++)
        fprintf(f, "i%" PRIu64 "e", v->data[i].uint64[0]);
    fprintf(f, "e");
}

void SMVM_StackFrame_printStateBencoded(struct SMVM_StackFrame * const s, FILE * const f) {
    assert(s);
    fprintf(f, "d");
    fprintf(f, "5:Stack");
    SMVM_RegisterVector_printStateBencoded(&s->stack, f);
    fprintf(f, "10:LastCallIpi%zue", s->lastCallIp);
    fprintf(f, "15:LastCallSectioni%zue", s->lastCallSection);
    fprintf(f, "e");
}

void SMVM_Program_printStateBencoded(struct SMVM_Program * const p, FILE * const f) {
    assert(p);
    fprintf(f, "d");
        fprintf(f, "5:Statei%de", p->state);
        fprintf(f, "5:Errori%de", p->error);
        fprintf(f, "18:CurrentCodeSectioni%zue", p->currentCodeSectionIndex);
        fprintf(f, "9:CurrentIPi%zue", p->currentIp);
        fprintf(f, "12:CurrentStackl");
        for (struct SMVM_FrameStack_item * s = p->frames.d; s != NULL; s = s->prev)
            SMVM_StackFrame_printStateBencoded(&s->value, f);
        fprintf(f, "e");
        fprintf(f, "9:NextStackl");
        if (p->nextFrame)
            SMVM_StackFrame_printStateBencoded(p->nextFrame, f);
        fprintf(f, "e");
    fprintf(f, "e");
}

static void printCodeSection(FILE * stream, const union SM_CodeBlock * code, size_t size, const char * linePrefix) {
    size_t skip = 0u;
    for (size_t i = 0u; i < size; i++) {
        fprintf(stream, "%s %08zx ", linePrefix, i);
        const uint8_t * b = &code[i].uint8[0];
        fprintf(stream, "%02x%02x %02x%02x %02x%02x %02x%02x",
               b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

        if (!skip) {
            const struct SMVMI_Instruction * instr = SMVMI_Instruction_from_code(code[i].uint64[0]);
            if (instr) {
                fprintf(stream, "  %s", SMVMI_Instruction_fullname_to_name(instr->fullname));
                skip = instr->numargs;
            } else {
                fprintf(stream, "  %s", "!!! UNKNOWN INSTRUCTION OR DATA !!!");
            }
        } else {
            skip--;
        }

        fprintf(stream, "\n");
    }
}

#endif /* SMVM_DEBUG */
