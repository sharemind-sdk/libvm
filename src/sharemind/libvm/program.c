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
/*#include <limits.h>
#include <stdbool.h>*/
#include "innercommand.h"
#include "preparationblock.h"
#include "rodataspecials.h"
#include "rwdataspecials.h"


/*******************************************************************************
 *  Forward declarations
********************************************************************************/


/**
 * \brief Adds a code section to the program and prepares it for direct execution.
 * \pre codeSize > 0
 * \pre This function has been called less than program->numCodeSections on the program object.
 * \pre program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \post program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \param program The program to prepare.
 * \param[in] code The program code, of which a copy is made and processed.
 * \param[in] codeSize The length of the code.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_addCodeSection(
        SharemindProgram * program,
        const SharemindCodeBlock * code,
        const size_t codeSize)
    __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_endPrepare(SharemindProgram * program) __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
 *  SharemindProgram
********************************************************************************/

SharemindProgram * SharemindProgram_new(SharemindVm * vm) {
    SharemindProgram * const p = (SharemindProgram *) malloc(sizeof(SharemindProgram));
    if (likely(p)) {

        if (!SharemindVm_refs_ref(vm)) {
            free(p);
            return NULL;
        }

        p->ready = false;
        p->error = SHAREMIND_VM_OK;
        p->vm = vm;
        SharemindCodeSectionsVector_init(&p->codeSections);
        SharemindDataSectionsVector_init(&p->dataSections);
        SharemindDataSectionsVector_init(&p->rodataSections);
        SharemindDataSectionSizesVector_init(&p->bssSectionSizes);
        SharemindSyscallBindingsVector_init(&p->bindings);
        SharemindPdBindings_init(&p->pdBindings);
        SHAREMIND_REFS_INIT(p);
    }
    return p;
}

void SharemindProgram_free(SharemindProgram * const p) {
    assert(p);
    SHAREMIND_REFS_ASSERT_IF_REFERENCED(p);

    SharemindVm_refs_unref(p->vm);

    SharemindCodeSectionsVector_destroy_with(&p->codeSections, &SharemindCodeSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->dataSections, &SharemindDataSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->rodataSections, &SharemindDataSection_destroy);
    SharemindDataSectionSizesVector_destroy(&p->bssSectionSizes);
    SharemindSyscallBindingsVector_destroy(&p->bindings);
    SharemindPdBindings_destroy(&p->pdBindings);

    free(p);
}

static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

SharemindVmError SharemindProgram_load_from_sme(SharemindProgram * p, const void * data, size_t dataSize) {
    assert(p);
    assert(!p->ready);
    assert(data);

    if (dataSize < sizeof(SharemindExecutableCommonHeader))
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

    const SharemindExecutableCommonHeader * ch;
    if (SharemindExecutableCommonHeader_read(data, &ch) != SHAREMIND_EXECUTABLE_READ_OK)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

    if (ch->fileFormatVersion > 0u)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE; /** \todo new error code? */


    const void * pos = ((const uint8_t *) data) + sizeof(SharemindExecutableCommonHeader);

    const SharemindExecutableHeader0x0 * h;
    if (SharemindExecutableHeader0x0_read(pos, &h) != SHAREMIND_EXECUTABLE_READ_OK)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;
    pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableHeader0x0);

    for (unsigned ui = 0; ui <= h->numberOfUnitsMinusOne; ui++) {
        const SharemindExecutableUnitHeader0x0 * uh;
        if (SharemindExecutableUnitHeader0x0_read(pos, &uh) != SHAREMIND_EXECUTABLE_READ_OK)
            return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

        pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableUnitHeader0x0);
        for (unsigned si = 0; si <= uh->sectionsMinusOne; si++) {
            const SharemindExecutableSectionHeader0x0 * sh;
            if (SharemindExecutableSectionHeader0x0_read(pos, &sh) != SHAREMIND_EXECUTABLE_READ_OK)
                return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

            pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableSectionHeader0x0);

            SHAREMIND_EXECUTABLE_SECTION_TYPE type = SharemindExecutableSectionHeader0x0_type(sh);
            assert(type != (SHAREMIND_EXECUTABLE_SECTION_TYPE) -1);

#if SIZE_MAX < UINT32_MAX
            if (unlikely(sh->length > SIZE_MAX))
                return SHAREMIND_VM_OUT_OF_MEMORY;
#endif

#define LOAD_DATASECTION_CASE(utype,ltype,spec) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        SharemindDataSection * s = SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        if (unlikely(!SharemindDataSection_init(s, sh->length, (spec)))) { \
            SharemindDataSectionsVector_pop(&p->ltype ## Sections); \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        } \
        memcpy(s->pData, pos, sh->length); \
        pos = ((const uint8_t *) pos) + sh->length + extraPadding[sh->length % 8]; \
    } break;
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        if (sh->length <= 0) \
            break; \
        const char * endPos = ((const char *) pos) + sh->length; \
        /* Check for 0-termination: */ \
        if (unlikely(*(endPos - 1))) \
            return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE; \
        do { \
            code \
            pos = ((const char *) pos) + strlen((const char *) pos) + 1; \
        } while (pos != endPos); \
        pos = ((const char *) pos) + extraPadding[sh->length % 8]; \
    } break;

            switch (type) {

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT: {
                    SharemindVmError r = SharemindProgram_addCodeSection(p, (const SharemindCodeBlock *) pos, sh->length);
                    if (r != SHAREMIND_VM_OK)
                        return r;

                    pos = ((const uint8_t *) pos) + sh->length * sizeof(SharemindCodeBlock);
                } break;

                LOAD_DATASECTION_CASE(RODATA,rodata,&roDataSpecials)
                LOAD_DATASECTION_CASE(DATA,data,&rwDataSpecials)

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS: {

                    uint32_t * s = SharemindDataSectionSizesVector_push(&p->bssSectionSizes);
                    if (unlikely(!s))
                        return SHAREMIND_VM_OUT_OF_MEMORY;

                    SHAREMIND_STATIC_ASSERT(sizeof(*s) == sizeof(sh->length));
                    (*s) = sh->length;

                } break;

                LOAD_BINDSECTION_CASE(BIND,
                    SharemindSyscallBinding * binding = SharemindSyscallBindingsVector_push(&p->bindings);
                    if (!binding)
                        return SHAREMIND_VM_OUT_OF_MEMORY;
                    const SharemindSyscallBinding * b = SharemindVm_find_syscall(p->vm, (const char *) pos);
                    if (!b) {
                        SharemindSyscallBindingsVector_pop(&p->bindings);
                        fprintf(stderr, "No syscall with the signature: %s\n", (const char *) pos);
                        return SHAREMIND_VM_PREPARE_UNDEFINED_BIND;
                    }
                    (*binding) = *b;)

                LOAD_BINDSECTION_CASE(PDBIND,
                    for (size_t i = 0; i < p->pdBindings.size; i++)
                        if (strcmp(SharemindPd_get_name(p->pdBindings.data[i]), pos) == 0)
                            return SHAREMIND_VM_PREPARE_DUPLICATE_PDBIND;

                    SharemindPd ** pdBinding = SharemindPdBindings_push(&p->pdBindings);
                    if (!pdBinding)
                        return SHAREMIND_VM_OUT_OF_MEMORY;

                    SharemindPd * b = SharemindVm_find_pd(p->vm, (const char *) pos);
                    if (!b) {
                        SharemindPdBindings_pop(&p->pdBindings);
                        fprintf(stderr, "No protection domain with the name: %s\n", (const char *) pos);
                        return SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND;
                    }
                    (*pdBinding) = b;)

                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(p->codeSections.size == ui))
            return SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION;

#define PUSH_EMPTY_DATASECTION(ltype,spec) \
    if (p->ltype ## Sections.size == ui) { \
        SharemindDataSection * s = SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        if (unlikely(!SharemindDataSection_init(s, 0u, (spec)))) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
    }

        PUSH_EMPTY_DATASECTION(rodata,&roDataSpecials)
        PUSH_EMPTY_DATASECTION(data,&rwDataSpecials)
        if (p->bssSectionSizes.size == ui) {
            uint32_t * s = SharemindDataSectionSizesVector_push(&p->bssSectionSizes);
            if (unlikely(!s))
                return SHAREMIND_VM_OUT_OF_MEMORY;
            (*s) = 0u;
        }
    }
    p->activeLinkingUnit = h->activeLinkingUnit;

    return SharemindProgram_endPrepare(p);
}

static SharemindVmError SharemindProgram_addCodeSection(SharemindProgram * const p,
                                                        const SharemindCodeBlock * const code,
                                                        const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);
    assert(!p->ready);

    SharemindCodeSection * s = SharemindCodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SHAREMIND_VM_OUT_OF_MEMORY;

    if (unlikely(!SharemindCodeSection_init(s, code, codeSize))) {
        SharemindCodeSectionsVector_pop(&p->codeSections);
        return SHAREMIND_VM_OUT_OF_MEMORY;
    }

    return SHAREMIND_VM_OK;
}

#define SHAREMIND_PREPARE_NOP (void)0
#define SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER SHAREMIND_PREPARE_ERROR_OUTER(SHAREMIND_VM_PREPARE_ERROR_INVALID_INSTRUCTION)
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
        SharemindPreparationBlock pb = { .block = &c[SHAREMIND_PREPARE_CURRENT_I], .type = 0 }; \
        if (sharemind_vm_run(NULL, SHAREMIND_I_GET_IMPL_LABEL, (void *) &pb) != SHAREMIND_VM_OK) \
            abort(); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } else (void) 0
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size
#define SHAREMIND_PREPARE_CURRENT_CODE_SECTION s

#define SHAREMIND_PREPARE_IS_INSTR(addr) SharemindInstrSet_contains(&s->instrset, (addr))
#define SHAREMIND_PREPARE_IS_EXCEPTIONCODE(c) ((c) >= 0x00 && (c) <= SHAREMIND_VM_PROCESS_EXCEPTION_COUNT && (c) != SHAREMIND_VM_PROCESS_OK && SharemindVmProcessException_toString((SharemindVmProcessException) (c)) != NULL)
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    if (1) { \
        SHAREMIND_PREPARE_CHECK_OR_ERROR(c[(*i)+(argNum)].uint64[0] < p->bindings.size, \
                                    SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = &p->bindings.data[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } else (void) 0

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,code) \
    static SharemindVmError prepare_pass2_ ## name (SharemindProgram * const p, SharemindCodeSection * s, SharemindCodeBlock * c, size_t * i) { \
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
    SharemindVmError (*f)(SharemindProgram * const p, SharemindCodeSection * s, SharemindCodeBlock * c, size_t * i);
};
static struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { .code = 0u, .f = NULL }
};

static SharemindVmError SharemindProgram_endPrepare(SharemindProgram * const p) {
    assert(p);
    assert(!p->ready);

    SharemindPreparationBlock pb;

    assert(p->codeSections.size > 0u);

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        SharemindVmError returnCode = SHAREMIND_VM_OK;
        SharemindCodeSection * s = &p->codeSections.data[j];

        SharemindCodeBlock * c = s->data;
        assert(c);

        /* Initialize instructions hashmap: */
        for (size_t i = 0u; i < s->size; i++) {
            const SharemindVmInstruction * const instr = sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }

            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(i + instr->numArgs < s->size,
                                              SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS);
            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(SharemindInstrSet_insert(&s->instrset, i),
                                              SHAREMIND_VM_OUT_OF_MEMORY);
            i += instr->numArgs;
        }

        for (size_t i = 0u; i < s->size; i++) {
            const SharemindVmInstruction * const instr = sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }
            struct preprocess_pass2_function * ppf = &preprocess_pass2_functions[0];
            for (;;) {
                if (!(ppf->f)) {
                    SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
                }
                if (ppf->code == c[i].uint64[0]) {
                    returnCode = (*(ppf->f))(p, s, c, &i);
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
        if (sharemind_vm_run(NULL, SHAREMIND_I_GET_IMPL_LABEL, &pb) != SHAREMIND_VM_OK)
            abort();

        continue;

    prepare_codesection_return_error:
        p->prepareCodeSectionIndex = j;
        return returnCode;
    }

    p->ready = true;

    return SHAREMIND_VM_OK;
}

bool SharemindProgram_is_ready(SharemindProgram * p) {
    assert(p);
    return p->ready;
}

SHAREMIND_REFS_DEFINE_FUNCTIONS(SharemindProgram)
