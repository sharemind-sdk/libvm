/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include "program.h"

#include <assert.h>
#include <fcntl.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "core.h"
#include "innercommand.h"
#include "preparationblock.h"
#include "rodataspecials.h"
#include "rwdataspecials.h"


/*******************************************************************************
 *  SharemindCodeSectionsVector
*******************************************************************************/

SHAREMIND_VECTOR_DECLARE_INIT(SharemindCodeSectionsVector, static inline,)
SHAREMIND_VECTOR_DEFINE_INIT(SharemindCodeSectionsVector, static inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindCodeSectionsVector, static inline,)
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindCodeSectionsVector,
                                     static inline,,
                                     free,
                                     SharemindCodeSection_destroy(value);)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindCodeSectionsVector,
                                      static inline,)
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindCodeSectionsVector,
                                     static inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindCodeSectionsVector, static inline,)
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindCodeSectionsVector, static inline)
SHAREMIND_VECTOR_DECLARE_POP(SharemindCodeSectionsVector, static inline,)
SHAREMIND_VECTOR_DEFINE_POP(SharemindCodeSectionsVector, static inline)


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
        SharemindCodeBlock const * const code,
        size_t const codeSize)
    __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_endPrepare(
        SharemindProgram * const program)
        __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
 *  SharemindProgram
*******************************************************************************/

SharemindProgram * SharemindVm_newProgram(SharemindVm * vm) {
    assert(vm);

    SharemindProgram * const p =
            (SharemindProgram *) malloc(sizeof(SharemindProgram));
    if (unlikely(!p)) {
        SharemindVm_setErrorOom(vm);
        goto SharemindProgram_new_error_0;
    }

    SharemindCodeSectionsVector_init(&p->codeSections);
    SharemindDataSectionsVector_init(&p->dataSections);
    SharemindDataSectionsVector_init(&p->rodataSections);
    SharemindDataSectionSizesVector_init(&p->bssSectionSizes);
    SharemindSyscallBindingsVector_init(&p->bindings);
    SharemindPdBindings_init(&p->pdBindings);

    if (!SHAREMIND_RECURSIVE_LOCK_INIT(p)) {
        SharemindVm_setErrorMie(vm);
        goto SharemindProgram_new_error_1;
    }

    SharemindProcessFacilityMap_init(&p->processFacilityMap,
                                     &vm->processFacilityMap);
    SHAREMIND_LIBVM_LASTERROR_INIT(p);
    SHAREMIND_REFS_INIT(p);
    SHAREMIND_TAG_INIT(p);

    p->vm = vm;
    p->ready = false;
    p->lastParsePosition = NULL;

    SharemindVm_lock(vm);
    if (unlikely(!SharemindVm_refs_ref(vm))) {
        SharemindVm_setErrorOor(vm);
        SharemindVm_unlock(vm);
        goto SharemindProgram_new_error_2;
    }
    SharemindVm_unlock(vm);

    return p;

SharemindProgram_new_error_2:

    SharemindProcessFacilityMap_destroy(&p->processFacilityMap);
    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

SharemindProgram_new_error_1:

    SharemindPdBindings_destroy(&p->pdBindings);
    SharemindSyscallBindingsVector_destroy(&p->bindings);
    SharemindDataSectionSizesVector_destroy(&p->bssSectionSizes);
    SharemindDataSectionsVector_destroy(&p->rodataSections);
    SharemindDataSectionsVector_destroy(&p->dataSections);
    SharemindCodeSectionsVector_destroy(&p->codeSections);
    free(p);

SharemindProgram_new_error_0:

    return NULL;

}

static inline void SharemindProgram_destroy(SharemindProgram * const p) {
    assert(p);

    SHAREMIND_REFS_ASSERT_IF_REFERENCED(p);

    SharemindVm_refs_unref(p->vm);

    SharemindCodeSectionsVector_destroy(&p->codeSections);
    SharemindDataSectionsVector_destroy(&p->dataSections);
    SharemindDataSectionsVector_destroy(&p->rodataSections);
    SharemindDataSectionSizesVector_destroy(&p->bssSectionSizes);
    SharemindSyscallBindingsVector_destroy(&p->bindings);
    SharemindPdBindings_destroy(&p->pdBindings);

    SharemindProcessFacilityMap_destroy(&p->processFacilityMap);
    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);
}

void SharemindProgram_free(SharemindProgram * const p) {
    SharemindProgram_destroy((assert(p), p));
    free(p);
}

SharemindVm * SharemindProgram_vm(SharemindProgram const * const p)
{ return p->vm; /* No locking, vm is const after SharemindProgram_new(). */ }

static size_t const extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

#define RETURN_SPLR_OOM(pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setErrorOom((p)); \
        SharemindProgram_unlock((p)); \
        return SHAREMIND_VM_OUT_OF_MEMORY; \
    } while (0)

#if SIZE_MAX < UINT32_MAX
#define RETURN_SPLR_OOR(pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setErrorOor((p)); \
        SharemindProgram_unlock((p)); \
        return SHAREMIND_VM_OUT_OF_MEMORY; \
    } while (0)
#endif

#define RETURN_SPLR(e,pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setError((p), (e), NULL); \
        SharemindProgram_unlock((p)); \
        return (e); \
    } while (0)

#define CONSTVOIDPTR_ADD(ptr,size) ((void const *)(((char const *) (ptr)) \
                                                   + (size)))
#define CONSTVOIDPTR_ADD_ASSIGN(dst,src,size) \
    do { (dst) = CONSTVOIDPTR_ADD((src),(size)); } while (0)
#define CONSTVOIDPTR_INCR(ptr,size) CONSTVOIDPTR_ADD_ASSIGN((ptr),(ptr),(size))

SharemindVmError SharemindProgram_loadFromFile(SharemindProgram * program,
                                               char const * filename)
{
    assert(program);
    assert(filename);

    /* Open input file: */
    int const fd = open(filename, O_RDONLY | O_CLOEXEC | O_NOCTTY);
    if (fd < 0) {
        SharemindProgram_setError(program,
                                  SHAREMIND_VM_IO_ERROR,
                                  "Unable to open() file!");
        return SHAREMIND_VM_IO_ERROR;
    }
    SharemindVmError const r =
            SharemindProgram_loadFromFileDescriptor(program, fd);
    close(fd);
    return r;
}

SharemindVmError SharemindProgram_loadFromCFile(SharemindProgram * program,
                                                FILE * file)
{
    assert(program);
    assert(file);
    int const fd = fileno(file);
    if (fd == -1) {
        SharemindProgram_setError(program,
                                  SHAREMIND_VM_IO_ERROR,
                                  "Failed fileno()!");
        return SHAREMIND_VM_IO_ERROR;
    }
    assert(fd >= 0);
    return SharemindProgram_loadFromFileDescriptor(program, fd);
}

SharemindVmError SharemindProgram_loadFromFileDescriptor(
        SharemindProgram * program,
        int fd)
{
    assert(program);
    assert(fd >= 0);

    /* Determine input file size: */
    struct stat inFileStat;
    if (fstat(fd, &inFileStat) != 0) {
        SharemindProgram_setError(program,
                                  SHAREMIND_VM_IO_ERROR,
                                  "Failed fstat()!");
        return SHAREMIND_VM_IO_ERROR;
    }
    if (inFileStat.st_size <= 0) /* Parse empty data block: */
        return SharemindProgram_loadFromMemory(program, &inFileStat, 0u);
    size_t const fileSize = (size_t) inFileStat.st_size;
    if (fileSize > SIZE_MAX) {
        SharemindProgram_setErrorOor(program);
        return SHAREMIND_VM_IMPLEMENTATION_LIMITS_REACHED;
    }

    /* Read file to memory: */
    void * const fileData = malloc(fileSize);
    if (!fileData) {
        SharemindProgram_setErrorOom(program);
        return SHAREMIND_VM_OUT_OF_MEMORY;
    }
    if (read(fd, fileData, fileSize) != inFileStat.st_size) {
        free(fileData);
        SharemindProgram_setError(program,
                                  SHAREMIND_VM_IO_ERROR,
                                  "Failed to read() all data from file!");
        return SHAREMIND_VM_IO_ERROR;
    }

    SharemindVmError const r =
            SharemindProgram_loadFromMemory(program, fileData, fileSize);
    free(fileData);
    return r;
}


SharemindVmError SharemindProgram_loadFromMemory(SharemindProgram * p,
                                                 void const * data,
                                                 size_t dataSize)
{
    assert(p);
    assert(data);

    SharemindProgram_lock(p);
    assert(!p->ready);

    if (dataSize < sizeof(SharemindExecutableCommonHeader))
        RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p);

    SharemindExecutableCommonHeader ch;
    if (SharemindExecutableCommonHeader_read(data, &ch)
        != SHAREMIND_EXECUTABLE_READ_OK)
    { RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p); }

    if (ch.fileFormatVersion > 0u) /** \todo new error code? */
        RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_HEADER, data, p);

    void const * pos =
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

            SHAREMIND_EXECUTABLE_SECTION_TYPE const type =
                    SharemindExecutableSectionHeader0x0_type(&sh);
            assert(type != (SHAREMIND_EXECUTABLE_SECTION_TYPE) -1);

#if SIZE_MAX < UINT32_MAX
            if (unlikely(sh.length > SIZE_MAX))
                RETURN_SPLR_OOR(pos, p);
#endif

#define LOAD_DATASECTION_CASE(utype,ltype,spec) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        SharemindDataSection * const s = \
            SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            RETURN_SPLR_OOM(pos, p); \
        if (unlikely(!SharemindDataSection_init(s, sh.length, (spec)))) { \
            SharemindDataSectionsVector_pop(&p->ltype ## Sections); \
            RETURN_SPLR_OOM(pos, p); \
        } \
        memcpy(s->pData, pos, sh.length); \
        CONSTVOIDPTR_INCR(pos, (sh.length + extraPadding[sh.length % 8])); \
    } break;
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        if (sh.length <= 0) \
            break; \
        void const * const endPos = CONSTVOIDPTR_ADD(pos, sh.length); \
        /* Check for 0-termination: */ \
        if (unlikely(*(((char const *)(endPos)) - 1))) \
            RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE, \
                        pos, \
                        p); \
        do { \
            code \
            CONSTVOIDPTR_INCR(pos, strlen((char const *) pos) + 1); \
        } while (pos != endPos); \
        CONSTVOIDPTR_INCR(pos, extraPadding[sh.length % 8]); \
    } break;

            SHAREMIND_STATIC_ASSERT(sizeof(type) <= sizeof(int));
            switch ((int) type) {

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT: {
                    SharemindVmError const r =
                            SharemindProgram_addCodeSection(
                                p,
                                (SharemindCodeBlock const *) pos,
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
                        RETURN_SPLR_OOM(pos, p);

                    SHAREMIND_STATIC_ASSERT(sizeof(*s) == sizeof(sh.length));
                    (*s) = sh.length;

                } break;

                LOAD_BINDSECTION_CASE(BIND,
                    if (((char const *) pos)[0u] == '\0')
                        RETURN_SPLR(
                                SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,
                                pos,
                                p);
                    SharemindSyscallWrapper const w =
                            SharemindVm_findSyscall(p->vm, (char const *) pos);
                    if (!w.callable)
                        RETURN_SPLR(SHAREMIND_VM_PREPARE_UNDEFINED_BIND,
                                    pos,
                                    p);
                    SharemindSyscallWrapper * const binding =
                            SharemindSyscallBindingsVector_push(&p->bindings);
                    if (!binding)
                        RETURN_SPLR_OOM(pos, p);
                    (*binding) = w;)

                LOAD_BINDSECTION_CASE(PDBIND,
                    if (((char const *) pos)[0u] == '\0')
                        RETURN_SPLR(
                            SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,
                            pos,
                            p);
                    for (size_t i = 0; i < p->pdBindings.size; i++)
                        if (strcmp(SharemindPd_name(p->pdBindings.data[i]),
                                   (char const *) pos) == 0)
                            RETURN_SPLR(SHAREMIND_VM_PREPARE_DUPLICATE_PDBIND,
                                        pos,
                                        p);
                    SharemindPd * const w =
                            SharemindVm_findPd(p->vm, (char const *) pos);
                    if (!w)
                        RETURN_SPLR(SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND,
                                    pos,
                                    p);
                    SharemindPd ** const pdBinding =
                            SharemindPdBindings_push(&p->pdBindings);
                    if (!pdBinding)
                        RETURN_SPLR_OOM(pos, p);
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
            RETURN_SPLR_OOM(NULL, p); \
        if (unlikely(!SharemindDataSection_init(s, 0u, (spec)))) \
            RETURN_SPLR_OOM(NULL, p); \
    }

        PUSH_EMPTY_DATASECTION(rodata,&roDataSpecials)
        PUSH_EMPTY_DATASECTION(data,&rwDataSpecials)
        if (p->bssSectionSizes.size == ui) {
            uint32_t * const s =
                    SharemindDataSectionSizesVector_push(&p->bssSectionSizes);
            if (unlikely(!s))
                RETURN_SPLR_OOM(NULL, p);
            (*s) = 0u;
        }
    }
    p->activeLinkingUnit = h.activeLinkingUnit;

    SharemindVmError const e = SharemindProgram_endPrepare(p);
    p->lastParsePosition = NULL;
    SharemindProgram_unlock(p);
    return e;
}

static SharemindVmError SharemindProgram_addCodeSection(
        SharemindProgram * const p,
        SharemindCodeBlock const * const code,
        size_t const codeSize)
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
    do { \
        returnCode = (e); \
        p->prepareIp = i; \
        goto prepare_codesection_return_error; \
    } while ((0))
#define SHAREMIND_PREPARE_ERROR(e) \
    do { \
        returnCode = (e); \
        p->prepareIp = *i; \
        return returnCode; \
    } while ((0))
#define SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR_OUTER(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_END_AS(index,numargs) \
    do { \
        c[SHAREMIND_PREPARE_CURRENT_I].uint64[0] = (index); \
        SharemindPreparationBlock pb = \
                { .block = &c[SHAREMIND_PREPARE_CURRENT_I], .type = 0 }; \
        if (runner(NULL, SHAREMIND_I_GET_IMPL_LABEL, (void *) &pb) \
                != SHAREMIND_VM_OK) \
            abort(); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } while ((0))
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size

#define SHAREMIND_PREPARE_IS_INSTR(addr) \
    (SharemindInstrMap_get(&s->instrmap, (addr)) != NULL)
#define SHAREMIND_PREPARE_IS_EXCEPTIONCODE(c) \
    ((c) != SHAREMIND_VM_PROCESS_OK \
     && SharemindVmProcessException_toString( \
                (SharemindVmProcessException) (c)) != NULL)
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    do { \
        SHAREMIND_PREPARE_CHECK_OR_ERROR( \
            c[(*i)+(argNum)].uint64[0] < p->bindings.size, \
            SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = \
                &p->bindings.data[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } while ((0))

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

static SharemindVmError SharemindProgram_endPrepare(
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
            SharemindVmInstruction const * const instr =
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
            SharemindVmInstruction const * const instr =
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

    return SHAREMIND_VM_OK;
}

bool SharemindProgram_isReady(SharemindProgram const * p) {
    assert(p);

    SharemindProgram_lockConst(p);
    bool const r = p->ready;
    SharemindProgram_unlockConst(p);
    return r;
}

void const * SharemindProgram_lastParsePosition(SharemindProgram const * p) {
    assert(p);

    SharemindProgram_lockConst(p);
    void const * const pos = p->lastParsePosition;
    SharemindProgram_unlockConst(p);
    return pos;
}

SharemindVmInstruction const * SharemindProgram_instruction(
        SharemindProgram const * p,
        size_t codeSection,
        size_t instructionIndex)
{
    assert(p);
    SharemindProgram_lockConst(p);
    if (!p->ready || codeSection >= p->codeSections.size)
        goto SharemindProgram_get_instruction_error;

    {
        SharemindInstrMapP_value * const v =
                SharemindInstrMapP_get(
                    &p->codeSections.data[codeSection].blockmap,
                    instructionIndex);
        if (!v)
            goto SharemindProgram_get_instruction_error;

        SharemindProgram_unlockConst(p);
        return v->value->description;
    }

SharemindProgram_get_instruction_error:

    SharemindProgram_unlockConst(p);
    return NULL;
}

size_t SharemindProgram_pdCount(SharemindProgram const * program) {
    assert(program);
    SharemindProgram_lockConst(program);
    size_t const r = program->pdBindings.size;
    SharemindProgram_unlockConst(program);
    return r;
}

SharemindPd * SharemindProgram_pd(SharemindProgram const * program, size_t i) {
    assert(program);
    SharemindProgram_lockConst(program);
    SharemindPd * const r = i < program->pdBindings.size
                          ? program->pdBindings.data[i]
                          : NULL;
    SharemindProgram_unlockConst(program);
    return r;
}

SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindProgram)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindProgram,)
SHAREMIND_REFS_DEFINE_FUNCTIONS_WITH_RECURSIVE_MUTEX(SharemindProgram)
SHAREMIND_DEFINE_FACILITYMAP_ACCESSORS(SharemindProgram,process,Process)
