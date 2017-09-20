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

#include "Program.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Core.h"
#include "PreparationBlock.h"


namespace {

class SharemindProgramGuard {

public: /* Methods: */

    SharemindProgramGuard(SharemindProgramGuard &&) = delete;
    SharemindProgramGuard(SharemindProgramGuard const &) = delete;

    SharemindProgramGuard(SharemindProgram & program) noexcept
        : m_program(program)
    { SharemindProgram_lock(&program); }

    ~SharemindProgramGuard() noexcept { SharemindProgram_unlock(&m_program); }

    SharemindProgramGuard & operator=(SharemindProgramGuard &&) = delete;
    SharemindProgramGuard & operator=(SharemindProgramGuard const &) = delete;

private: /* Fields: */

    SharemindProgram & m_program;

};

class ConstSharemindProgramGuard {

public: /* Methods: */

    ConstSharemindProgramGuard(ConstSharemindProgramGuard &&) = delete;
    ConstSharemindProgramGuard(ConstSharemindProgramGuard const &) = delete;

    ConstSharemindProgramGuard(SharemindProgram const & program) noexcept
        : m_program(program)
    { SharemindProgram_lockConst(&program); }

    ~ConstSharemindProgramGuard() noexcept
    { SharemindProgram_unlockConst(&m_program); }

    ConstSharemindProgramGuard & operator=(ConstSharemindProgramGuard &&)
            = delete;
    ConstSharemindProgramGuard & operator=(ConstSharemindProgramGuard const &)
            = delete;

private: /* Fields: */

    SharemindProgram const & m_program;

};

} // anonymous namespace

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

    SharemindProgram * p;
    try {
        p = new SharemindProgram();
    } catch (...) { // Not enought error codes in VmError
        SharemindVm_setErrorOom(vm);
        goto SharemindProgram_new_error_0;
    }

    if (!SHAREMIND_RECURSIVE_LOCK_INIT(p)) {
        SharemindVm_setErrorMie(vm);
        goto SharemindProgram_new_error_1;
    }

    try {
        SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(*p, *vm);
    } catch (...) {
        SharemindVm_setErrorOom(vm);
        goto SharemindProgram_new_error_2;
    }

    SHAREMIND_LIBVM_LASTERROR_INIT(p);
    SHAREMIND_TAG_INIT(p);

    p->vm = vm;
    p->ready = false;
    p->lastParsePosition = nullptr;
    return p;

SharemindProgram_new_error_2:

    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);

SharemindProgram_new_error_1:

    delete p;

SharemindProgram_new_error_0:

    return nullptr;

}

void SharemindProgram_free(SharemindProgram * const p) {
    assert(p);
    p->dataSections.clear();
    p->rodataSections.clear();

    SHAREMIND_TAG_DESTROY(p);
    SHAREMIND_RECURSIVE_LOCK_DEINIT(p);
    delete p;
}

SharemindVm * SharemindProgram_vm(SharemindProgram const * const p)
{ return p->vm; /* No locking, vm is const after SharemindProgram_new(). */ }

static size_t const extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

#define RETURN_SPLR_OOM(pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setErrorOom((p)); \
        return SHAREMIND_VM_OUT_OF_MEMORY; \
    } while (0)

#if SIZE_MAX < UINT32_MAX
#define RETURN_SPLR_OOR(pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setErrorOor((p)); \
        return SHAREMIND_VM_OUT_OF_MEMORY; \
    } while (0)
#endif

#define RETURN_SPLR(e,pos,p) \
    do { \
        p->lastParsePosition = (pos); \
        SharemindProgram_setError((p), (e), nullptr); \
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

    SharemindProgramGuard const guard(*p);
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
        try { \
            p->ltype ## Sections.emplace_back( \
                    std::make_shared<sharemind::spec>(pos, sh.length)); \
        } catch (...) { \
            RETURN_SPLR_OOM(pos, p); \
        } \
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

                LOAD_DATASECTION_CASE(RODATA,rodata,RoDataSection)
                LOAD_DATASECTION_CASE(DATA,data,RwDataSection)

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS: {

                    try {
                        p->bssSectionSizes.emplace_back(sh.length);
                    } catch (...) {
                        RETURN_SPLR_OOM(pos, p);
                    }

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
                    try {
                        p->bindings.emplace_back(w);
                    } catch (...) {
                        RETURN_SPLR_OOM(pos, p);
                    })

                LOAD_BINDSECTION_CASE(PDBIND,
                    if (((char const *) pos)[0u] == '\0')
                        RETURN_SPLR(
                            SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE,
                            pos,
                            p);
                    for (size_t i = 0; i < p->pdBindings.size(); i++)
                        if (strcmp(SharemindPd_name(p->pdBindings[i]),
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
                    try {
                        p->pdBindings.emplace_back(w);
                    } catch (...) {
                        RETURN_SPLR_OOM(pos, p);
                    })

                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(p->codeSections.size() == ui))
            RETURN_SPLR(SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION, nullptr, p);

#define PUSH_EMPTY_DATASECTION(ltype,spec) \
    if (p->ltype ## Sections.size() == ui) { \
        try { \
            p->ltype ## Sections.emplace_back( \
                    std::make_shared<sharemind::spec>(nullptr, 0u)); \
        } catch (...) { \
            RETURN_SPLR_OOM(nullptr, p); \
        } \
    }

        PUSH_EMPTY_DATASECTION(rodata,RoDataSection)
        PUSH_EMPTY_DATASECTION(data,RwDataSection)
        if (p->bssSectionSizes.size() == ui) {
            try {
                p->bssSectionSizes.emplace_back(0u);
            } catch (...) {
                RETURN_SPLR_OOM(nullptr, p);
            }
        }
    }
    p->activeLinkingUnit = h.activeLinkingUnit;

    SharemindVmError const e = SharemindProgram_endPrepare(p);
    p->lastParsePosition = nullptr;
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

    try {
        p->codeSections.emplace_back(code, codeSize);
        return SHAREMIND_VM_OK;
    } catch (...) {
        return SHAREMIND_VM_OUT_OF_MEMORY;
    }
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
        sharemind::PreparationBlock pb = \
                { &c[SHAREMIND_PREPARE_CURRENT_I], 0u }; \
        if (runner(nullptr, SHAREMIND_I_GET_IMPL_LABEL, (void *) &pb) \
                != SHAREMIND_VM_OK) \
            abort(); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } while ((0))
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size()

#define SHAREMIND_PREPARE_IS_INSTR(addr) (s->isInstructionAtOffset(addr))
#define SHAREMIND_PREPARE_IS_EXCEPTIONCODE(c) \
    ((c) != SHAREMIND_VM_PROCESS_OK \
     && SharemindVmProcessException_toString( \
                (SharemindVmProcessException) (c)) != nullptr)
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    do { \
        SHAREMIND_PREPARE_CHECK_OR_ERROR( \
            c[(*i)+(argNum)].uint64[0] < p->bindings.size(), \
            SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = \
                &p->bindings[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } while ((0))

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,code) \
    static SharemindVmError prepare_pass2_ ## name ( \
            SharemindProgram * const p, \
            sharemind::CodeSection * s, \
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
    { bytecode, prepare_pass2_ ## name},
struct preprocess_pass2_function {
    uint64_t code;
    SharemindVmError (*f)(SharemindProgram * const p,
                          sharemind::CodeSection * s,
                          SharemindCodeBlock * c,
                          size_t * i,
                          SharemindCoreVmRunner runner);
};
static struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { 0u, nullptr }
};

static SharemindVmError SharemindProgram_endPrepare(
        SharemindProgram * const p)
{
    assert(p);
    assert(!p->ready);

    sharemind::PreparationBlock pb;

    assert(!p->codeSections.empty());

    for (size_t j = 0u; j < p->codeSections.size(); j++) {
        SharemindVmError returnCode = SHAREMIND_VM_OK;
        sharemind::CodeSection * const s = &p->codeSections[j];

        SharemindCodeBlock * const c = s->data();
        assert(c);

        /* Initialize instructions hashmap: */
        size_t numInstrs = 0u;
        for (size_t i = 0u; i < s->size(); i++, numInstrs++) {
            SharemindVmInstruction const * const instr =
                    sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }

            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(
                        i + instr->numArgs < s->size(),
                        SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS);
            try {
                s->registerInstruction(i, numInstrs, instr);
            } catch (...) {
                SHAREMIND_PREPARE_ERROR_OUTER(SHAREMIND_VM_OUT_OF_MEMORY);
            }
            i += instr->numArgs;
        }

        for (size_t i = 0u; i < s->size(); i++) {
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
        c[s->size()].uint64[0] = 0u; /* && eof */
        pb.block = &c[s->size()];
        pb.type = 2;
        if (sharemind_vm_run(nullptr, SHAREMIND_I_GET_IMPL_LABEL, &pb)
                != SHAREMIND_VM_OK)
            abort();

        continue;

    prepare_codesection_return_error:
        p->prepareCodeSectionIndex = j;
        RETURN_SPLR(returnCode, nullptr, p);
    }

    p->ready = true;

    return SHAREMIND_VM_OK;
}

bool SharemindProgram_isReady(SharemindProgram const * p) {
    assert(p);

    ConstSharemindProgramGuard const guard(*p);
    return p->ready;
}

void const * SharemindProgram_lastParsePosition(SharemindProgram const * p) {
    assert(p);

    ConstSharemindProgramGuard const guard(*p);
    return p->lastParsePosition;
}

SharemindVmInstruction const * SharemindProgram_instruction(
        SharemindProgram const * p,
        size_t codeSection,
        size_t instructionIndex)
{
    assert(p);
    {
        ConstSharemindProgramGuard const guard(*p);
        if (p->ready && codeSection < p->codeSections.size()) {
            return p->codeSections[codeSection].instructionDescriptionAtOffset(
                        instructionIndex);
        }
    }
    return nullptr;
}

size_t SharemindProgram_pdCount(SharemindProgram const * program) {
    assert(program);
    ConstSharemindProgramGuard const guard(*program);
    return program->pdBindings.size();
}

SharemindPd * SharemindProgram_pd(SharemindProgram const * program, size_t i) {
    assert(program);
    {
        ConstSharemindProgramGuard const guard(*program);
        if (i < program->pdBindings.size())
            return program->pdBindings[i];
    }
    return nullptr;
}

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DEFINE(SharemindProgram,)
SHAREMIND_LIBVM_LASTERROR_FUNCTIONS_DEFINE(SharemindProgram)
SHAREMIND_TAG_FUNCTIONS_DEFINE(SharemindProgram,)
SHAREMIND_DEFINE_PROCESSFACILITYMAP_ACCESSORS(SharemindProgram)
