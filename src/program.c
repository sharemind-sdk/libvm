/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__

#include "program.h"

#include <assert.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <stdio.h>
#include <stdlib.h>
#include "core.h"
#include "innercommand.h"
#include "preparationblock.h"
#include "rodataspecials.h"
#include "rwdataspecials.h"


/*******************************************************************************
 *  Forward declarations
*******************************************************************************/


/**
 * \brief Adds a code section to the program and prepares it for direct
 *        execution.
 * \pre codeSize > 0
 * \pre This function has been called less than program->numCodeSections on the
 *      program object.
 * \pre program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \post program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \param program The program to prepare.
 * \param[in] code The program code, of which a copy is made and processed.
 * \param[in] codeSize The length of the code.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_addCodeSection(
        SharemindProgram * const program,
        const SharemindCodeBlock * const code,
        const size_t codeSize)
    __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SharemindVmError.
 */
static SharemindProgramLoadResult SharemindProgram_endPrepare(
        SharemindProgram * const program)
        __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
 *  SharemindProgram
*******************************************************************************/

static inline SharemindVmError SharemindProgram_init(
        SharemindProgram * p,
        SharemindVm * vm,
        SharemindVirtualMachineContext * overrides)
{
    assert(p);
    assert(vm);

    SharemindVmError error;
    if (unlikely(!SharemindVm_refs_ref(vm))) {
        error = SHAREMIND_VM_IMPLEMENTATION_LIMITS_REACHED;
        goto SharemindProgram_init_error_0;
    }

    SharemindCodeSectionsVector_init(&p->codeSections);
    SharemindDataSectionsVector_init(&p->dataSections);
    SharemindDataSectionsVector_init(&p->rodataSections);
    SharemindDataSectionSizesVector_init(&p->bssSectionSizes);
    SharemindSyscallBindingsVector_init(&p->bindings);
    SharemindPdBindings_init(&p->pdBindings);

    if (unlikely(SharemindMutex_init(&p->mutex) != SHAREMIND_MUTEX_OK)) {
        error = SHAREMIND_VM_MUTEX_ERROR;
        goto SharemindProgram_init_error_1;
    }

    SHAREMIND_REFS_INIT(p);

    p->vm = vm;
    p->error = SHAREMIND_VM_OK;
    p->ready = false;
    p->overrides = overrides;

    return SHAREMIND_VM_OK;

SharemindProgram_init_error_1:

    SharemindVm_refs_unref(vm);

SharemindProgram_init_error_0:

    return error;

}

SharemindProgram * SharemindProgram_new(
        SharemindVm * vm,
        SharemindVirtualMachineContext * overrides)
{
    assert(vm);

    SharemindProgram * const p =
            (SharemindProgram *) malloc(sizeof(SharemindProgram));
    if (unlikely(!p))
        goto SharemindProgram_new_error_0;

    if (unlikely(SharemindProgram_init(p, vm, overrides) != SHAREMIND_VM_OK))
        goto SharemindProgram_new_error_1;

    return p;

SharemindProgram_new_error_1:

    free(p);

SharemindProgram_new_error_0:

    return NULL;

}

static inline void SharemindProgram_destroy(SharemindProgram * const p) {
    assert(p);

    if (p->overrides && p->overrides->destructor)
        (*(p->overrides->destructor))(p->overrides);

    SHAREMIND_REFS_ASSERT_IF_REFERENCED(p);

    SharemindVm_refs_unref(p->vm);

    SharemindCodeSectionsVector_destroy_with(&p->codeSections,
                                             &SharemindCodeSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->dataSections,
                                             &SharemindDataSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->rodataSections,
                                             &SharemindDataSection_destroy);
    SharemindDataSectionSizesVector_destroy(&p->bssSectionSizes);
    SharemindSyscallBindingsVector_destroy(&p->bindings);
    SharemindPdBindings_destroy(&p->pdBindings);

    if (unlikely(SharemindMutex_destroy(&p->mutex) != SHAREMIND_MUTEX_OK))
        abort();
}

void SharemindProgram_free(SharemindProgram * const p) {
    SharemindProgram_destroy((assert(p), p));
    free(p);
}

static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

#define DOLOCK(program,lock) \
    if (unlikely(SharemindMutex_ ## lock(&(program)->mutex) \
        != SHAREMIND_MUTEX_OK)) \
    { \
        abort(); \
    } else (void) 0
#define LOCK(program) DOLOCK((program),lock)
#define UNLOCK(program) DOLOCK((program),unlock)
#define LOCK_CONST(program) DOLOCK((program),lock_const)
#define UNLOCK_CONST(program) DOLOCK((program),unlock_const)

#define RETURN_SPLR(e,pos,p) \
    do { \
        SharemindProgramLoadResult r__; \
        r__.error = (e); \
        r__.position = (pos); \
        UNLOCK((p)); \
        return r__; \
    } while (0)

#define CONSTVOIDPTR_ADD(ptr,size) ((const void *)(((const char *) (ptr)) \
                                                   + (size)))
#define CONSTVOIDPTR_ADD_ASSIGN(dst,src,size) \
    do { (dst) = CONSTVOIDPTR_ADD((src),(size)); } while (0)
#define CONSTVOIDPTR_INCR(ptr,size) CONSTVOIDPTR_ADD_ASSIGN((ptr),(ptr),(size))

SharemindProgramLoadResult SharemindProgram_load_from_sme(SharemindProgram * p,
                                                          const void * data,
                                                          size_t dataSize)
{
    assert(p);
    assert(data);

    LOCK(p);
    assert(!p->ready);

    if (dataSize < sizeof(SharemindExecutableCommonHeader))
        RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p);

    SharemindExecutableCommonHeader ch;
    if (SharemindExecutableCommonHeader_read(data, &ch)
        != SHAREMIND_EXECUTABLE_READ_OK)
    { RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p); }

    if (ch.fileFormatVersion > 0u) /** \todo new error code? */
        RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p);

    const void * pos =
            CONSTVOIDPTR_ADD(data, sizeof(SharemindExecutableCommonHeader));

    SharemindExecutableHeader0x0 h;
    if (SharemindExecutableHeader0x0_read(pos, &h)
            != SHAREMIND_EXECUTABLE_READ_OK)
    { RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, pos, p); }

    CONSTVOIDPTR_INCR(pos, sizeof(SharemindExecutableHeader0x0));

    for (unsigned ui = 0; ui <= h.numberOfUnitsMinusOne; ui++) {
        SharemindExecutableUnitHeader0x0 uh;
        if (SharemindExecutableUnitHeader0x0_read(pos, &uh)
                != SHAREMIND_EXECUTABLE_READ_OK)
        { RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, pos, p); }

        CONSTVOIDPTR_INCR(pos, sizeof(SharemindExecutableUnitHeader0x0));
        for (unsigned si = 0; si <= uh.sectionsMinusOne; si++) {
            SharemindExecutableSectionHeader0x0 sh;
            if (SharemindExecutableSectionHeader0x0_read(pos, &sh)
                    != SHAREMIND_EXECUTABLE_READ_OK)
            { RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, pos, p); }

            CONSTVOIDPTR_INCR(pos, sizeof(SharemindExecutableSectionHeader0x0));

            const SHAREMIND_EXECUTABLE_SECTION_TYPE type =
                    SharemindExecutableSectionHeader0x0_type(&sh);
            assert(type != (SHAREMIND_EXECUTABLE_SECTION_TYPE) -1);

#if SIZE_MAX < UINT32_MAX
            if (unlikely(sh.length > SIZE_MAX))
                RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos);
#endif

#define LOAD_DATASECTION_CASE(utype,ltype,spec) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        SharemindDataSection * const s = \
            SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos, p); \
        if (unlikely(!SharemindDataSection_init(s, sh.length, (spec)))) { \
            SharemindDataSectionsVector_pop(&p->ltype ## Sections); \
            RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos, p); \
        } \
        memcpy(s->pData, pos, sh.length); \
        CONSTVOIDPTR_INCR(pos, (sh.length + extraPadding[sh.length % 8])); \
    } break;
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        if (sh.length <= 0) \
            break; \
        const void * const endPos = CONSTVOIDPTR_ADD(pos, sh.length); \
        /* Check for 0-termination: */ \
        if (unlikely(*(((const char *)(endPos)) - 1))) \
            RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE, \
                        pos, \
                        p); \
        do { \
            code \
            CONSTVOIDPTR_INCR(pos, strlen((const char *) pos) + 1); \
        } while (pos != endPos); \
        CONSTVOIDPTR_INCR(pos, extraPadding[sh.length % 8]); \
    } break;

            SHAREMIND_STATIC_ASSERT(sizeof(type) <= sizeof(int));
            switch ((int) type) {

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT: {
                    const SharemindVmError r =
                            SharemindProgram_addCodeSection(
                                p,
                                (const SharemindCodeBlock *) pos,
                                sh.length);
                    if (r != SHAREMIND_VM_OK)
                        RETURN_SPLR(r, pos, p);

                    CONSTVOIDPTR_INCR(pos,
                                      sh.length * sizeof(SharemindCodeBlock));
                } break;

                LOAD_DATASECTION_CASE(RODATA,rodata,&roDataSpecials)
                LOAD_DATASECTION_CASE(DATA,data,&rwDataSpecials)

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS: {

                    uint32_t * const s =
                            SharemindDataSectionSizesVector_push(
                                &p->bssSectionSizes);
                    if (unlikely(!s))
                        RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos, p);

                    SHAREMIND_STATIC_ASSERT(sizeof(*s) == sizeof(sh.length));
                    (*s) = sh.length;

                } break;

                LOAD_BINDSECTION_CASE(BIND,
                    if (((const char *) pos)[0u] == '\0')
                        RETURN_SPLR(
                                SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,
                                pos,
                                p);
                    const SharemindSyscallWrapper w =
                            SharemindProgram_findSyscall(p,
                                                          (const char *) pos);
                    if (!w.callable)
                        RETURN_SPLR(SHAREMIND_VM_PREPARE_UNDEFINED_BIND,
                                    pos,
                                    p);
                    SharemindSyscallWrapper * const binding =
                            SharemindSyscallBindingsVector_push(&p->bindings);
                    if (!binding)
                        RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos, p);
                    (*binding) = w;)

                LOAD_BINDSECTION_CASE(PDBIND,
                    if (((const char *) pos)[0u] == '\0')
                        RETURN_SPLR(
                            SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,
                            pos,
                            p);
                    for (size_t i = 0; i < p->pdBindings.size; i++)
                        if (strcmp(SharemindPd_name(p->pdBindings.data[i]),
                                   (const char *) pos) == 0)
                            RETURN_SPLR(SHAREMIND_VM_PREPARE_DUPLICATE_PDBIND,
                                        pos,
                                        p);
                    SharemindPd * const w =
                            SharemindProgram_findPd(p, (const char *) pos);
                    if (!w)
                        RETURN_SPLR(SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND,
                                    pos,
                                    p);
                    SharemindPd ** const pdBinding =
                            SharemindPdBindings_push(&p->pdBindings);
                    if (!pdBinding)
                        RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, pos, p);
                    (*pdBinding) = w;)

                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(p->codeSections.size == ui))
            RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION, NULL, p);

#define PUSH_EMPTY_DATASECTION(ltype,spec) \
    if (p->ltype ## Sections.size == ui) { \
        SharemindDataSection * const s = \
                SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, NULL, p); \
        if (unlikely(!SharemindDataSection_init(s, 0u, (spec)))) \
            RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, NULL, p); \
    }

        PUSH_EMPTY_DATASECTION(rodata,&roDataSpecials)
        PUSH_EMPTY_DATASECTION(data,&rwDataSpecials)
        if (p->bssSectionSizes.size == ui) {
            uint32_t * const s =
                    SharemindDataSectionSizesVector_push(&p->bssSectionSizes);
            if (unlikely(!s))
                RETURN_SPLR(SHAREMIND_VM_OUT_OF_MEMORY, NULL, p);
            (*s) = 0u;
        }
    }
    p->activeLinkingUnit = h.activeLinkingUnit;

    return SharemindProgram_endPrepare(p);
}

static SharemindVmError SharemindProgram_addCodeSection(
        SharemindProgram * const p,
        const SharemindCodeBlock * const code,
        const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);
    assert(!p->ready);

    SharemindCodeSection * const s =
            SharemindCodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SHAREMIND_VM_OUT_OF_MEMORY;

    if (unlikely(!SharemindCodeSection_init(s, code, codeSize))) {
        SharemindCodeSectionsVector_pop(&p->codeSections);
        return SHAREMIND_VM_OUT_OF_MEMORY;
    }

    return SHAREMIND_VM_OK;
}

#define SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER \
    SHAREMIND_PREPARE_ERROR_OUTER(\
        SHAREMIND_VM_PREPARE_ERROR_INVALID_INSTRUCTION)
#define SHAREMIND_PREPARE_ERROR_OUTER(e) \
    if (1) { \
        returnCode = (e); \
        p->prepareIp = i; \
        goto prepare_codesection_return_error; \
    } else (void) 0
#define SHAREMIND_PREPARE_ERROR(e) \
    if (1) { \
        returnCode = (e); \
        p->prepareIp = *i; \
        return returnCode; \
    } else (void) 0
#define SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR_OUTER(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_END_AS(index,numargs) \
    if (1) { \
        c[SHAREMIND_PREPARE_CURRENT_I].uint64[0] = (index); \
        SharemindPreparationBlock pb = \
                { .block = &c[SHAREMIND_PREPARE_CURRENT_I], .type = 0 }; \
        if (runner(NULL, SHAREMIND_I_GET_IMPL_LABEL, (void *) &pb) \
                != SHAREMIND_VM_OK) \
            abort(); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } else (void) 0
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size

#define SHAREMIND_PREPARE_IS_INSTR(addr) \
    (SharemindInstrMap_get(&s->instrmap, (addr)) != NULL)
#define SHAREMIND_PREPARE_IS_EXCEPTIONCODE(c) \
    ((c) >= 0x00 \
     && (c) <= SHAREMIND_VM_PROCESS_EXCEPTION_COUNT \
     && (c) != SHAREMIND_VM_PROCESS_OK \
     && SharemindVmProcessException_toString( \
                (SharemindVmProcessException) (c)) != NULL)
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    if (1) { \
        SHAREMIND_PREPARE_CHECK_OR_ERROR( \
            c[(*i)+(argNum)].uint64[0] < p->bindings.size, \
            SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = \
                &p->bindings.data[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } else (void) 0

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,code) \
    static SharemindVmError prepare_pass2_ ## name ( \
            SharemindProgram * const p, \
            SharemindCodeSection * s, \
            SharemindCodeBlock * c, \
            size_t * i, \
            SharemindCoreVmRunner runner) \
    { \
        (void) p; (void) s; (void) c; (void) i; \
        SharemindVmError returnCode = SHAREMIND_VM_OK; \
        code \
        return returnCode; \
    }
#include <sharemind/m4/preprocess_pass2_functions.h>

#undef SHAREMIND_PREPARE_PASS2_FUNCTION
#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,_) \
    { .code = bytecode, .f = prepare_pass2_ ## name},
struct preprocess_pass2_function {
    uint64_t code;
    SharemindVmError (*f)(SharemindProgram * const p,
                          SharemindCodeSection * s,
                          SharemindCodeBlock * c,
                          size_t * i,
                          SharemindCoreVmRunner runner);
};
static struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { .code = 0u, .f = NULL }
};

static SharemindProgramLoadResult SharemindProgram_endPrepare(
        SharemindProgram * const p)
{
    assert(p);
    assert(!p->ready);

    SharemindPreparationBlock pb;

    assert(p->codeSections.size > 0u);

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        SharemindVmError returnCode = SHAREMIND_VM_OK;
        SharemindCodeSection * const s = &p->codeSections.data[j];

        SharemindCodeBlock * const c = s->data;
        assert(c);

        /* Initialize instructions hashmap: */
        size_t numInstrs = 0u;
        for (size_t i = 0u; i < s->size; i++, numInstrs++) {
            const SharemindVmInstruction * const instr =
                    sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }

            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(
                        i + instr->numArgs < s->size,
                        SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS);
            SharemindInstrMap_value * const v =
                    SharemindInstrMap_insertNew(&s->instrmap, i);
            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(v,
                                                   SHAREMIND_VM_OUT_OF_MEMORY);
            SharemindInstrMapP_value * const v2 =
                    SharemindInstrMapP_insertNew(&s->blockmap, numInstrs);
            if (!v2) {
                SharemindInstrMap_remove(&s->instrmap, i);
                SHAREMIND_PREPARE_ERROR_OUTER(SHAREMIND_VM_OUT_OF_MEMORY);
            }
            v->value.blockIndex = i;
            v->value.instructionBlockIndex = numInstrs;
            v->value.description = instr;
            v2->value = &v->value;
            i += instr->numArgs;
        }

        for (size_t i = 0u; i < s->size; i++) {
            const SharemindVmInstruction * const instr =
                    sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }
            struct preprocess_pass2_function * ppf =
                    &preprocess_pass2_functions[0];
            for (;;) {
                if (!(ppf->f)) {
                    SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
                }
                if (ppf->code == c[i].uint64[0]) {
                    returnCode = (*(ppf->f))(p, s, c, &i, &sharemind_vm_run);
                    if (returnCode != SHAREMIND_VM_OK)
                        goto prepare_codesection_return_error;
                    break;
                }
                ++ppf;
            }
        }

        /* Initialize exception pointer: */
        c[s->size].uint64[0] = 0u; /* && eof */
        pb.block = &c[s->size];
        pb.type = 2;
        if (sharemind_vm_run(NULL, SHAREMIND_I_GET_IMPL_LABEL, &pb)
                != SHAREMIND_VM_OK)
            abort();

        continue;

    prepare_codesection_return_error:
        p->prepareCodeSectionIndex = j;
        RETURN_SPLR(returnCode, NULL, p);
    }

    p->ready = true;

    RETURN_SPLR(SHAREMIND_VM_OK, NULL, p);
}

bool SharemindProgram_is_ready(const SharemindProgram * p) {
    assert(p);

    LOCK_CONST(p);
    const bool r = p->ready;
    UNLOCK_CONST(p);
    return r;
}

const SharemindVmInstruction * SharemindProgram_get_instruction(
        const SharemindProgram * p,
        size_t codeSection,
        size_t instructionIndex)
{
    assert(p);
    LOCK_CONST(p);
    if (!p->ready || codeSection >= p->codeSections.size)
        goto SharemindProgram_get_instruction_error;

    {
        SharemindInstrMapP_value * const v =
                SharemindInstrMapP_get(
                    &p->codeSections.data[codeSection].blockmap,
                    instructionIndex);
        if (!v)
            goto SharemindProgram_get_instruction_error;

        UNLOCK_CONST(p);
        return v->value->description;
    }

SharemindProgram_get_instruction_error:

    UNLOCK_CONST(p);
    return NULL;
}

SHAREMIND_REFS_DEFINE_FUNCTIONS_WITH_MUTEX(SharemindProgram)
