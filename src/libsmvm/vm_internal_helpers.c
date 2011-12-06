/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define _SHAREMIND_INTERNAL

#include "vm_internal_helpers.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "../libsme/libsme.h"
#include "../libsme/libsme_0x0.h"
#include "../libsmvmi/instr.h"
#include "../likely.h"
#include "vm_internal_core.h"

#ifdef SMVM_DEBUG
#include <inttypes.h>
#include "../libsmvmi/instr.h"
#endif /* SMVM_DEBUG */


/*******************************************************************************
 *  SMVM
********************************************************************************/

struct _SMVM {
    SMVM_Context * context;
};

SMVM * SMVM_new(SMVM_Context * context) {
    assert(context);
    SMVM * smvm = (SMVM *) malloc(sizeof(SMVM));
    if (!smvm)
        return NULL;
    smvm->context = context;
    return smvm;
}

void SMVM_free(SMVM * smvm) {
    assert(smvm);
    if (smvm->context) {
        if (smvm->context->destructor) {
            (*(smvm->context->destructor))(smvm->context);
        }
    }
    free(smvm);
}

const SMVM_Context_Syscall * SMVM_find_syscall(SMVM * smvm, const char * signature) {
    if (!smvm->context || !smvm->context->find_syscall)
        return NULL;

    return (*(smvm->context->find_syscall))(smvm->context, signature);
}

void * SMVM_get_pd_process_instance(SMVM * smvm, const char * pdName) {
    if (!smvm->context || !smvm->context->get_pd_process_instance_handle)
        return NULL;

    return (*(smvm->context->get_pd_process_instance_handle))(smvm->context, pdName);
}


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SM_ENUM_DEFINE_TOSTRING(SMVM_State, SMVM_ENUM_State);
SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMVM_Error, SMVM_ENUM_Error);
SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMVM_Exception, SMVM_ENUM_Exception);


/*******************************************************************************
 *  SMVM_MemoryMap
********************************************************************************/

#ifdef SMVM_FAST_BUILD
SMVM_MemorySlot_init_DEFINE
SMVM_MemorySlot_destroy_DEFINE

SM_MAP_DEFINE(SMVM_MemoryMap,uint64_t,const uint64_t,SMVM_MemorySlot,(uint16_t),SM_MAP_KEY_EQUALS_uint64_t,SM_MAP_KEY_LESS_THAN_uint64_t,SM_MAP_KEYCOPY_REGULAR,SM_MAP_KEYFREE_REGULAR,malloc,free,)
#endif

static inline void SMVM_MemoryMap_destroyer(const uint64_t * key, SMVM_MemorySlot * value) {
    (void) key;
    SMVM_MemorySlot_destroy(value);
}


/*******************************************************************************
 *  SMVM_StackFrame
********************************************************************************/

#ifdef SMVM_FAST_BUILD
SM_VECTOR_DEFINE(SMVM_RegisterVector,SMVM_CodeBlock,malloc,free,realloc,)
#endif

void SMVM_StackFrame_init(SMVM_StackFrame * f, SMVM_StackFrame * prev) {
    assert(f);
    SMVM_RegisterVector_init(&f->stack);
    SMVM_ReferenceVector_init(&f->refstack);
    SMVM_CReferenceVector_init(&f->crefstack);
    f->prev = prev;

#ifdef SMVM_DEBUG
    f->lastCallIp = 0u;
    f->lastCallSection = 0u;
#endif
}

void SMVM_StackFrame_destroy(SMVM_StackFrame * f) {
    assert(f);
    SMVM_RegisterVector_destroy(&f->stack);
    SMVM_ReferenceVector_destroy_with(&f->refstack, &SMVM_Reference_destroy);
    SMVM_CReferenceVector_destroy_with(&f->crefstack, &SMVM_CReference_destroy);
}

/*******************************************************************************
 *  SMVM_CodeSection
********************************************************************************/

#ifdef SMVM_FAST_BUILD
SM_VECTOR_DEFINE(SMVM_BreakpointVector,SMVM_Breakpoint,malloc,free,realloc,)
SM_INSTRSET_DEFINE(SMVM_InstrSet,malloc,free,)
#endif

int SMVM_CodeSection_init(SMVM_CodeSection * s,
                          const SMVM_CodeBlock * const code,
                          const size_t codeSize)
{
    assert(s);

    /* Add space for an exception pointer: */
    size_t realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return 0;

    size_t memSize = realCodeSize * sizeof(SMVM_CodeBlock);
    if (unlikely(memSize / sizeof(SMVM_CodeBlock) != realCodeSize))
        return 0;


    s->data = (SMVM_CodeBlock *) malloc(memSize);
    if (unlikely(!s->data))
        return 0;

    s->size = codeSize;
    memcpy(s->data, code, memSize);

    SMVM_InstrSet_init(&s->instrset);

    return 1;
}

void SMVM_CodeSection_destroy(SMVM_CodeSection * const s) {
    assert(s);
    free(s->data);
    SMVM_InstrSet_destroy(&s->instrset);
}


/*******************************************************************************
 *  SMVM_DataSection
********************************************************************************/

int SMVM_DataSection_init(SMVM_DataSection * ds, size_t size, SMVM_MemorySlotSpecials * specials) {
    assert(ds);
    assert(specials);

    if (size != 0) {
        ds->pData = malloc(size);
        if (unlikely(!ds->pData))
            return 0;
    } else {
        ds->pData = NULL;
    }
    ds->size = size;
    ds->nrefs = 1u;
    ds->specials = specials;
    return 1;
}

void SMVM_DataSection_destroy(SMVM_DataSection * ds) {
    free(ds->pData);
}


/*******************************************************************************
 *  SMVM_Program
********************************************************************************/

#ifdef SMVM_FAST_BUILD
SM_VECTOR_DEFINE(SMVM_CodeSectionsVector,SMVM_CodeSection,malloc,free,realloc,)
SM_VECTOR_DEFINE(SMVM_DataSectionsVector,SMVM_DataSection,malloc,free,realloc,)
SM_STACK_DEFINE(SMVM_FrameStack,SMVM_StackFrame,malloc,free,)
#endif

SMVM_Program * SMVM_Program_new(SMVM * smvm) {
    SMVM_Program * const p = (SMVM_Program *) malloc(sizeof(SMVM_Program));
    if (likely(p)) {
        p->state = SMVM_INITIALIZED;
        p->error = SMVM_OK;
        p->smvm = smvm;
        p->syscallContext.publicAlloc = NULL; /** \todo */
        p->syscallContext.publicFree = NULL; /** \todo */
        p->syscallContext.publicMemPtrSize = NULL; /** \todo */
        p->syscallContext.publicMemPtrData = NULL; /** \todo */
        p->syscallContext.allocPrivate = NULL; /** \todo */
        p->syscallContext.freePrivate = NULL; /** \todo */
        p->syscallContext.get_pd_process_instance_handle = &_SMVM_get_pd_process_handle;
        p->syscallContext.internal = &p->syscallContextInternal;
        p->syscallContextInternal.program = p;
        SMVM_CodeSectionsVector_init(&p->codeSections);
        SMVM_DataSectionsVector_init(&p->dataSections);
        SMVM_DataSectionsVector_init(&p->rodataSections);
        SMVM_DataSectionsVector_init(&p->bssSections);
        SMVM_SyscallBindings_init(&p->bindings);
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

void SMVM_Program_free(SMVM_Program * const p) {
    assert(p);
    SMVM_CodeSectionsVector_destroy_with(&p->codeSections, &SMVM_CodeSection_destroy);
    SMVM_DataSectionsVector_destroy_with(&p->dataSections, &SMVM_DataSection_destroy);
    SMVM_DataSectionsVector_destroy_with(&p->rodataSections, &SMVM_DataSection_destroy);
    SMVM_DataSectionsVector_destroy_with(&p->bssSections, &SMVM_DataSection_destroy);
    SMVM_SyscallBindings_destroy(&p->bindings);
    SMVM_FrameStack_destroy_with(&p->frames, &SMVM_StackFrame_destroy);

    SMVM_MemoryMap_destroy_with(&p->memoryMap, &SMVM_MemoryMap_destroyer);
    free(p);
}

static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static SMVM_MemorySlotSpecials rwDataSpecials = { .writeable = 1, .readable = 1, .free = NULL };
static SMVM_MemorySlotSpecials roDataSpecials = { .writeable = 0, .readable = 1, .free = NULL };

SMVM_Error SMVM_Program_load_from_sme(SMVM_Program * p, const void * data, size_t dataSize) {
    assert(p);
    assert(data);

    if (dataSize < sizeof(SME_Common_Header))
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

    const SME_Common_Header * ch;
    if (SME_Common_Header_read(data, &ch) != SME_READ_OK)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

    if (ch->fileFormatVersion > 0u)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE; /** \todo new error code? */


    const void * pos = ((const uint8_t *) data) + sizeof(SME_Common_Header);

    const SME_Header_0x0 * h;
    if (SME_Header_0x0_read(pos, &h) != SME_READ_OK)
        return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;
    pos = ((const uint8_t *) pos) + sizeof(SME_Header_0x0);

    for (unsigned ui = 0; ui <= h->numberOfUnitsMinusOne; ui++) {
        const SME_Unit_Header_0x0 * uh;
        if (SME_Unit_Header_0x0_read(pos, &uh) != SME_READ_OK)
            return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

        pos = ((const uint8_t *) pos) + sizeof(SME_Unit_Header_0x0);
        for (unsigned si = 0; si <= uh->sectionsMinusOne; si++) {
            const SME_Section_Header_0x0 * sh;
            if (SME_Section_Header_0x0_read(pos, &sh) != SME_READ_OK)
                return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE;

            pos = ((const uint8_t *) pos) + sizeof(SME_Section_Header_0x0);

            SME_Section_Type type = SME_Section_Header_0x0_type(sh);
            assert(type != (SME_Section_Type) -1);

#if SIZE_MAX < UIN32_MAX
            if (unlikely(sh->length > SIZE_MAX))
                return SMVM_OUT_OF_MEMORY;
#endif

#define LOAD_DATASECTION_CASE(utype,ltype,copyCode,spec) \
    case SME_SECTION_TYPE_ ## utype: { \
        SMVM_DataSection * s = SMVM_DataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SMVM_OUT_OF_MEMORY; \
        if (unlikely(!SMVM_DataSection_init(s, sh->length, (spec)))) { \
            SMVM_DataSectionsVector_pop(&p->ltype ## Sections); \
            return SMVM_OUT_OF_MEMORY; \
        } \
        copyCode \
    } break;
#define COPYSECTION \
    memcpy(s->pData, pos, sh->length); \
    pos = ((const uint8_t *) pos) + sh->length + extraPadding[sh->length % 8];
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SME_SECTION_TYPE_ ## utype: { \
        if (sh->length <= 0) \
            break; \
        const char * endPos = ((const char *) pos) + sh->length; \
        /* Check for 0-termination: */ \
        if (unlikely(*(endPos - 1))) \
            return SMVM_PREPARE_ERROR_INVALID_INPUT_FILE; \
        do { \
            code \
            pos = ((const char *) pos) + strlen((const char *) pos) + 1; \
        } while (pos != endPos); \
        pos = ((const char *) pos) + extraPadding[sh->length % 8]; \
    } break;

            switch (type) {
                case SME_SECTION_TYPE_TEXT: {
                    SMVM_Error r = SMVM_Program_addCodeSection(p, (const SMVM_CodeBlock *) pos, sh->length);
                    if (r != SMVM_OK)
                        return r;

                    pos = ((const uint8_t *) pos) + sh->length * sizeof(SMVM_CodeBlock);
                } break;
                LOAD_DATASECTION_CASE(RODATA,rodata,COPYSECTION,&roDataSpecials)
                LOAD_DATASECTION_CASE(DATA,data,COPYSECTION,&rwDataSpecials)
                LOAD_DATASECTION_CASE(BSS,bss,memset(s->pData, 0, sh->length);,&rwDataSpecials)
                LOAD_BINDSECTION_CASE(BIND,
                    const SMVM_Context_Syscall ** binding = SMVM_SyscallBindings_push(&p->bindings); \
                    if (!binding) \
                        return SMVM_OUT_OF_MEMORY; \
                    (*binding) = SMVM_find_syscall(p->smvm, (const char *) pos); \
                    if (!*binding) { \
                        SMVM_SyscallBindings_pop(&p->bindings); \
                        fprintf(stderr, "No syscall with the signature: %s\n", (const char *) pos); \
                        return SMVM_PREPARE_UNDEFINED_BIND; \
                    })
                LOAD_BINDSECTION_CASE(PDBIND,
                    void ** pdBinding = SMVM_PdBindings_push(&p->pdBindings); \
                    if (!pdBinding) \
                        return SMVM_OUT_OF_MEMORY; \
                    (*pdBinding) = SMVM_get_pd_process_instance(p->smvm, (const char *) pos); \
                    if (!*pdBinding) { \
                        SMVM_PdBindings_pop(&p->pdBindings); \
                        fprintf(stderr, "No protection domain with the name: %s\n", (const char *) pos); \
                        return SMVM_PREPARE_UNDEFINED_PDBIND; \
                    })
                default:
                    /** \todo also add other sections (currently ignoring). */
                    break;
            }
        }

        if (unlikely(p->codeSections.size == ui))
            return SMVM_PREPARE_ERROR_NO_CODE_SECTION;

#define PUSH_EMPTY_DATASECTION(ltype,spec) \
    if (p->ltype ## Sections.size == ui) { \
        SMVM_DataSection * s = SMVM_DataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SMVM_OUT_OF_MEMORY; \
        SMVM_DataSection_init(s, 0u, (spec)); \
    }

        PUSH_EMPTY_DATASECTION(rodata,&roDataSpecials)
        PUSH_EMPTY_DATASECTION(data,&rwDataSpecials)
        PUSH_EMPTY_DATASECTION(bss,&rwDataSpecials)
    }
    p->currentCodeSectionIndex = h->activeLinkingUnit;

    return SMVM_Program_endPrepare(p);
}

#ifdef SMVM_DEBUG
static void printCodeSection(FILE * stream, const SMVM_CodeBlock * code, size_t size, const char * linePrefix);
#endif /* SMVM_DEBUG */

SMVM_Error SMVM_Program_addCodeSection(SMVM_Program * const p,
                                       const SMVM_CodeBlock * const code,
                                       const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);

    if (unlikely(p->state != SMVM_INITIALIZED))
        return SMVM_INVALID_INPUT_STATE;

    SMVM_CodeSection * s = SMVM_CodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SMVM_OUT_OF_MEMORY;

    if (unlikely(!SMVM_CodeSection_init(s, code, codeSize))) {
        SMVM_CodeSectionsVector_pop(&p->codeSections);
        return SMVM_OUT_OF_MEMORY;
    }

    #ifdef SMVM_DEBUG
    fprintf(stderr, "Added code section %zu:\n", p->codeSections.size - 1);
    printCodeSection(stderr, code, codeSize, "    ");
    #endif /* SMVM_DEBUG */

    return SMVM_OK;
}

#define SMVM_PREPARE_NOP (void)0
#define SMVM_PREPARE_INVALID_INSTRUCTION_OUTER SMVM_PREPARE_ERROR_OUTER(SMVM_PREPARE_ERROR_INVALID_INSTRUCTION)
#define SMVM_PREPARE_ERROR_OUTER(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = i; \
        goto smvm_prepare_codesection_return_error; \
    } else (void) 0
#define SMVM_PREPARE_ERROR(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = *i; \
        return returnCode; \
    } else (void) 0
#define SMVM_PREPARE_CHECK_OR_ERROR_OUTER(c,e) \
    if (unlikely(!(c))) { \
        SMVM_PREPARE_ERROR_OUTER(e); \
    } else (void) 0
#define SMVM_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SMVM_PREPARE_ERROR(e); \
    } else (void) 0
#define SMVM_PREPARE_END_AS(index,numargs) \
    if (1) { \
        c[SMVM_PREPARE_CURRENT_I].uint64[0] = (index); \
        SMVM_Prepare_IBlock pb = { .block = &c[SMVM_PREPARE_CURRENT_I], .type = 0 }; \
        if (_SMVM(p, SMVM_I_GET_IMPL_LABEL, (void *) &pb) != SMVM_OK) \
            abort(); \
        SMVM_PREPARE_CURRENT_I += (numargs); \
    } else (void) 0
#define SMVM_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SMVM_PREPARE_CURRENT_I (*i)
#define SMVM_PREPARE_CODESIZE (s)->size
#define SMVM_PREPARE_CURRENT_CODE_SECTION s

#define SMVM_PREPARE_IS_INSTR(addr) SMVM_InstrSet_contains(&s->instrset, (addr))
#define SMVM_PREPARE_IS_EXCEPTIONCODE(c) ((c) >= 0x00 && (c) <= SMVM_E_COUNT && (c) != SMVM_E_NONE && SMVM_Exception_toString((SMVM_Exception) (c)) != NULL)
#define SMVM_PREPARE_SYSCALL(argNum) \
    if (1) { \
        SMVM_PREPARE_CHECK_OR_ERROR(c[(*i)+(argNum)].uint64[0] < p->bindings.size, \
                                    SMVM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = p->bindings.data[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } else (void) 0

#define SMVM_PREPARE_PASS2_FUNCTION(name,bytecode,code) \
    static SMVM_Error prepare_pass2_ ## name (SMVM_Program * const p, SMVM_CodeSection * s, SMVM_CodeBlock * c, size_t * i) { \
        (void) p; (void) s; (void) c; (void) i; \
        SMVM_Error returnCode = SMVM_OK; \
        code \
        return returnCode; \
    }
#include "../m4/preprocess_pass2_functions.h"

#undef SMVM_PREPARE_PASS2_FUNCTION
#define SMVM_PREPARE_PASS2_FUNCTION(name,bytecode,_) \
    { .code = bytecode, .f = prepare_pass2_ ## name},
struct preprocess_pass2_function {
    uint64_t code;
    SMVM_Error (*f)(SMVM_Program * const p, SMVM_CodeSection * s, SMVM_CodeBlock * c, size_t * i);
};
struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include "../m4/preprocess_pass2_functions.h"
    { .code = 0u, .f = NULL }
};

SMVM_Error SMVM_Program_endPrepare(SMVM_Program * const p) {
    assert(p);

    SMVM_Prepare_IBlock pb;

    if (unlikely(p->state != SMVM_INITIALIZED))
        return SMVM_INVALID_INPUT_STATE;

    assert(p->codeSections.size > 0u);

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        SMVM_Error returnCode = SMVM_OK;
        SMVM_CodeSection * s = &p->codeSections.data[j];

        SMVM_CodeBlock * c = s->data;
        assert(c);

        /* Initialize instructions hashmap: */
        for (size_t i = 0u; i < s->size; i++) {
            const SMVMI_Instruction * const instr = SMVMI_Instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SMVM_PREPARE_INVALID_INSTRUCTION_OUTER;
            }

            SMVM_PREPARE_CHECK_OR_ERROR_OUTER(i + instr->numArgs < s->size,
                                              SMVM_PREPARE_ERROR_INVALID_ARGUMENTS);
            SMVM_PREPARE_CHECK_OR_ERROR_OUTER(SMVM_InstrSet_insert(&s->instrset, i),
                                              SMVM_OUT_OF_MEMORY);
            i += instr->numArgs;
        }

        for (size_t i = 0u; i < s->size; i++) {
            const SMVMI_Instruction * const instr = SMVMI_Instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SMVM_PREPARE_INVALID_INSTRUCTION_OUTER;
            }
            struct preprocess_pass2_function * ppf = &preprocess_pass2_functions[0];
            for (;;) {
                if (!(ppf->f)) {
                    SMVM_PREPARE_INVALID_INSTRUCTION_OUTER;
                }
                if (ppf->code == c[i].uint64[0]) {
                    returnCode = (*(ppf->f))(p, s, c, &i);
                    if (returnCode != SMVM_OK)
                        goto smvm_prepare_codesection_return_error;
                    break;
                }
                ++ppf;
            }
        }

        /* Initialize exception pointer: */
        c[s->size].uint64[0] = 0u; /* && eof */
        pb.block = &c[s->size];
        pb.type = 2;
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

SMVM_Error SMVM_Program_run(SMVM_Program * const program) {
    assert(program);

    if (unlikely(program->state != SMVM_PREPARED))
        return SMVM_INVALID_INPUT_STATE;

    return _SMVM(program, SMVM_I_RUN, NULL);
}

int64_t SMVM_Program_get_return_value(SMVM_Program *p) {
    return p->returnValue.int64[0];
}

SMVM_Exception SMVM_Program_get_exception(SMVM_Program *p) {
    assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
    assert(SMVM_Exception_toString((SMVM_Exception) p->exceptionValue) != 0);
    return (SMVM_Exception) p->exceptionValue;
}

size_t SMVM_Program_get_current_codesection(SMVM_Program *p) {
    return p->currentCodeSectionIndex;
}

uintptr_t SMVM_Program_get_current_ip(SMVM_Program *p) {
    return p->currentIp;
}

/*******************************************************************************
 *  Functions provided to system calls
********************************************************************************/

uint64_t _SMVM_publicAlloc(size_t nBytes, SMVM_MODAPI_0x1_Syscall_Context * c) {
    /** \todo */
    return 0u;
}

int _SMVM_publicFree(uint64_t ptr, SMVM_MODAPI_0x1_Syscall_Context * c) {
    /** \todo */
    return 1;
}

size_t _SMVM_publicMemPtrSize(uint64_t ptr, SMVM_MODAPI_0x1_Syscall_Context * c) {
    /** \todo */
    return 0u;
}

void * _SMVM_publicMemPtrData(uint64_t ptr, SMVM_MODAPI_0x1_Syscall_Context * c) {
    /** \todo */
    return NULL;
}

void * _SMVM_allocPrivate(size_t nBytes, SMVM_MODAPI_0x1_Syscall_Context * c) {
    (void) c;
    /** \todo add allocated pointer to valid list */
    return malloc(nBytes);
}

int _SMVM_freePrivate(void * ptr, SMVM_MODAPI_0x1_Syscall_Context * c) {
    (void) c;
    /** \todo check if ptr is valid */
    free(ptr);
    return 1;
}

void * _SMVM_get_pd_process_handle(uint64_t pd_index, SMVM_MODAPI_0x1_Syscall_Context * p) {
    const SMVM_SyscallContextInternal * i = (const SMVM_SyscallContextInternal *) p->internal;
    if (pd_index > SIZE_MAX)
        return NULL;
    void * const * const ppd = SMVM_PdBindings_get_const_pointer(&i->program->pdBindings, pd_index);
    if (ppd)
        return *ppd;
    return NULL;
}


/*******************************************************************************
 *  Debugging
********************************************************************************/

#ifdef SMVM_DEBUG

void SMVM_RegisterVector_printStateBencoded(SMVM_RegisterVector * const v, FILE * const f) {
    fprintf(f, "l");
    for (size_t i = 0u; i < v->size; i++)
        fprintf(f, "i%" PRIu64 "e", v->data[i].uint64[0]);
    fprintf(f, "e");
}

void SMVM_StackFrame_printStateBencoded(SMVM_StackFrame * const s, FILE * const f) {
    assert(s);
    fprintf(f, "d");
    fprintf(f, "5:Stack");
    SMVM_RegisterVector_printStateBencoded(&s->stack, f);
    fprintf(f, "10:LastCallIpi%zue", s->lastCallIp);
    fprintf(f, "15:LastCallSectioni%zue", s->lastCallSection);
    fprintf(f, "e");
}

void SMVM_Program_printStateBencoded(SMVM_Program * const p, FILE * const f) {
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

static void printCodeSection(FILE * stream, const SMVM_CodeBlock * code, size_t size, const char * linePrefix) {
    size_t skip = 0u;
    for (size_t i = 0u; i < size; i++) {
        fprintf(stream, "%s %08zx ", linePrefix, i);
        const uint8_t * b = &code[i].uint8[0];
        fprintf(stream, "%02x%02x %02x%02x %02x%02x %02x%02x",
               b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

        if (!skip) {
            const SMVMI_Instruction * instr = SMVMI_Instruction_from_code(code[i].uint64[0]);
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
