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
#include "Program_p.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <new>
#include <sharemind/GlobalDeleter.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/PotentiallyVoidTypeInfo.h>
#include <sharemind/SignedToUnsigned.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include "CommonIntructionMacros.h"
#include "Core.h"
#include "PreparationBlock.h"
#include "Process.h"
#include "Vm_p.h"


namespace sharemind {

namespace {

#define SHAREMIND_PREPARE_ARGUMENTS_CHECK(c) \
    if (unlikely(!(c))) { \
        throw Program::InvalidInstructionArgumentsException(); \
    } else (void) 0
#define SHAREMIND_PREPARE_END_AS(index,numargs) \
    do { \
        c[SHAREMIND_PREPARE_CURRENT_I].uint64[0] = (index); \
        Detail::PreparationBlock pb{ \
            &c[SHAREMIND_PREPARE_CURRENT_I], \
            Detail::PreparationBlock::InstructionLabel}; \
        Detail::vmRun(Detail::ExecuteMethod::GetInstruction, &pb); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } while ((0))
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size()

#define SHAREMIND_PREPARE_IS_INSTR(addr) (s->isInstructionAtOffset(addr))
static_assert(std::numeric_limits<std::uint64_t>::max()
              <= std::numeric_limits<std::size_t>::max(), "");
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    do { \
        SHAREMIND_PREPARE_ARGUMENTS_CHECK( \
            c[(*i)+(argNum)].uint64[0] < p.staticData->syscallBindings.size());\
        c[(*i)+(argNum)].cp[0] = \
                &p.staticData->syscallBindings[c[(*i)+(argNum)].uint64[0]]; \
    } while ((0))

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,...) \
    static void prepare_pass2_ ## name ( \
            Detail::ParseData & p, \
            Detail::CodeSection * s, \
            SharemindCodeBlock * c, \
            std::size_t * i) \
    { \
        (void) p; (void) s; (void) c; (void) i; \
        __VA_ARGS__ \
    }
#include <sharemind/m4/preprocess_pass2_functions.h>

#undef SHAREMIND_PREPARE_PASS2_FUNCTION
#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,_) \
    { bytecode, prepare_pass2_ ## name},
struct preprocess_pass2_function {
    std::uint64_t code;
    void (*f)(Detail::ParseData & p,
              Detail::CodeSection * s,
              SharemindCodeBlock * c,
              std::size_t * i);
};
static struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { 0u, nullptr }
};

/**
 * \brief Prepares the program fully for execution.
 * \param p The parse data to prepare.
 */
void endPrepare(Detail::ParseData & p) {
    assert(!p.staticData->codeSections.empty());

    for (std::size_t j = 0u; j < p.staticData->codeSections.size(); j++) {
        Detail::CodeSection * const s = &p.staticData->codeSections[j];

        SharemindCodeBlock * const c = s->data();
        assert(c);

        /* Initialize instructions hashmap: */
        std::size_t numInstrs = 0u;
        for (std::size_t i = 0u; i < s->size(); i++, numInstrs++) {
            SharemindVmInstruction const * const instr =
                    sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr)
                throw Program::InvalidInstructionException();

            if (i + instr->numArgs >= s->size())
                throw Program::InvalidInstructionArgumentsException();
            s->registerInstruction(i, numInstrs, instr);
            i += instr->numArgs;
        }

        for (std::size_t i = 0u; i < s->size(); i++) {
            SharemindVmInstruction const * const instr =
                    sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr)
                throw Program::InvalidInstructionException();
            struct preprocess_pass2_function * ppf =
                    &preprocess_pass2_functions[0];
            for (;;) {
                if (!(ppf->f))
                    throw Program::InvalidInstructionException();
                if (ppf->code == c[i].uint64[0]) {
                    (*(ppf->f))(p, s, c, &i);
                    break;
                }
                ++ppf;
            }
        }

        /* Initialize exception pointer: */
        {
            Detail::PreparationBlock pb{&c[s->size()],
                                        Detail::PreparationBlock::EofLabel};
            Detail::vmRun(Detail::ExecuteMethod::GetInstruction, &pb);
        }
    }
} // endPrepare()

} // anonymous namespace

SHAREMIND_DEFINE_EXCEPTION_NOINLINE(sharemind::Exception, Program::, Exception);
SHAREMIND_DEFINE_EXCEPTION_NOINLINE(IoException, Program::, IoException);
#define EC(base,name,msg) \
    SHAREMIND_DEFINE_EXCEPTION_CONST_MSG_NOINLINE( \
        base ## Exception, Program::, name ## Exception, msg)
EC(Io, FileOpen, "Unable to open() file!");
EC(Io, FileNo, "Failed fileno()!");
EC(Io, FileFstat, "Failed fstat()!");
EC(Io, FileRead, "Failed to read() all data from file!");
EC(, ImplementationLimitsReached, "Implementation limits reached!");
SHAREMIND_DEFINE_EXCEPTION_NOINLINE(Exception, Program::,PrepareException);
EC(Prepare, InvalidHeader, "Invalid executable file header!");
EC(Prepare, VersionMismatch, "Executable file format version not supported!");
EC(Prepare, InvalidInputFile, "Invalid or absent data in executable file!");
EC(Prepare, UndefinedSyscallBind, "Binding for missing system call found!");
EC(Prepare, UndefinedPdBind, "Binding for missing protection domain found!");
EC(Prepare, DuplicatePdBind, "Duplicate protection domain bindings found!");
EC(Prepare, NoCodeSections, "No code sections found!");
EC(Prepare, InvalidInstruction, "Invalid instruction found!");
EC(Prepare, InvalidInstructionArguments,
   "Invalid arguments for instruction found!");
#undef EC

#define GUARD \
    std::lock_guard<decltype(m_inner->m_mutex)> const guard(m_inner->m_mutex)


Program::Inner::Inner(std::shared_ptr<Vm::Inner> vmInner)
    : m_vmInner(std::move(vmInner))
{ SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(*this, *m_vmInner); }

Program::Inner::~Inner() noexcept {}


Program::Program(Vm & vm)
    : m_inner(std::make_shared<Inner>(vm.m_inner))
{}

Program::~Program() noexcept {}

bool Program::isReady() const noexcept {
    GUARD;
    return static_cast<bool>(m_inner->m_parseData);
}

void Program::loadFromFile(char const * const filename) {
    assert(filename);

    /* Open input file: */
    int const fd = ::open(filename, O_RDONLY | O_CLOEXEC | O_NOCTTY);
    if (fd < 0)
        throw FileOpenException();
    try {
        loadFromFileDescriptor(fd);
        ::close(fd);
    } catch (...) {
        ::close(fd);
        throw;
    }
}

void Program::loadFromCFile(FILE * const file) {
    assert(file);
    int const fd = ::fileno(file);
    if (fd == -1)
        throw FileNoException();
    assert(fd >= 0);
    return loadFromFileDescriptor(fd);
}

void Program::loadFromFileDescriptor(int const fd) {
    assert(fd >= 0);

    /* Determine input file size: */
    std::size_t const fileSize =
            [](int const fdesc) -> std::size_t {
                struct ::stat inFileStat;
                if (::fstat(fdesc, &inFileStat) != 0)
                    throw FileFstatException();
                if (inFileStat.st_size <= 0) /* Parse empty data block: */
                    return 0u;
                auto const statFileSize = signedToUnsigned(inFileStat.st_size);
                if (statFileSize > std::numeric_limits<std::size_t>::max())
                    throw ImplementationLimitsReachedException();
                return statFileSize;
            }(fd);
    if (fileSize == 0u) // Parse empty data block:
        return loadFromMemory(&fileSize, 0u);

    /* Read file to memory: */
    std::unique_ptr<void, GlobalDeleter> fileData(::operator new(fileSize));
    for (;;) {
        auto const r = ::read(fd, fileData.get(), fileSize);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            throw FileReadException();
        }
        if (signedToUnsigned(r) != fileSize)
            throw FileReadException();
        break;
    }

    return loadFromMemory(fileData.get(), fileSize);
}

void Program::loadFromMemory(void const * data, std::size_t dataSize) {
    assert(data);

    auto d(std::make_shared<Detail::ParseData>());

    if (dataSize < sizeof(SharemindExecutableCommonHeader))
        throw InvalidHeaderException();

    SharemindExecutableCommonHeader ch;
    if (SharemindExecutableCommonHeader_read(data, &ch)
            != SHAREMIND_EXECUTABLE_READ_OK)
        throw InvalidHeaderException();

    if (ch.fileFormatVersion > 0u)
        throw VersionMismatchException();

    auto pos = ptrAdd(data, sizeof(SharemindExecutableCommonHeader));

    SharemindExecutableHeader0x0 h;
    if (SharemindExecutableHeader0x0_read(pos, &h)
            != SHAREMIND_EXECUTABLE_READ_OK)
        throw InvalidHeaderException();

    pos = ptrAdd(pos, sizeof(SharemindExecutableHeader0x0));

    static std::size_t const extraPadding[8] =
            { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

    for (unsigned ui = 0; ui <= h.numberOfUnitsMinusOne; ui++) {
        SharemindExecutableUnitHeader0x0 uh;
        if (SharemindExecutableUnitHeader0x0_read(pos, &uh)
                != SHAREMIND_EXECUTABLE_READ_OK)
            throw InvalidHeaderException();

        pos = ptrAdd(pos, sizeof(SharemindExecutableUnitHeader0x0));
        for (unsigned si = 0; si <= uh.sectionsMinusOne; si++) {
            SharemindExecutableSectionHeader0x0 sh;
            if (SharemindExecutableSectionHeader0x0_read(pos, &sh)
                    != SHAREMIND_EXECUTABLE_READ_OK)
                throw InvalidHeaderException();

            pos = ptrAdd(pos, sizeof(SharemindExecutableSectionHeader0x0));

            SHAREMIND_EXECUTABLE_SECTION_TYPE const type =
                    SharemindExecutableSectionHeader0x0_type(&sh);
            assert(type != static_cast<SHAREMIND_EXECUTABLE_SECTION_TYPE>(-1));

            if (unlikely(sh.length > std::numeric_limits<std::size_t>::max()))
                throw ImplementationLimitsReachedException();

#define LOAD_DATASECTION_CASE(utype,ltype,spec) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        d-> ltype ## Sections.emplace_back( \
                std::make_shared<Detail::spec>(pos, sh.length)); \
        pos = ptrAdd(pos, (sh.length + extraPadding[sh.length % 8])); \
    } break;
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        if (sh.length <= 0) \
            break; \
        void const * const endPos = ptrAdd(pos, sh.length); \
        /* Check for 0-termination: */ \
        if (unlikely(*(static_cast<char const *>(endPos) - 1))) \
            throw InvalidInputFileException(); \
        do { \
            code \
            pos = ptrAdd(pos, ::strlen(static_cast<char const *>(pos)) + 1); \
        } while (pos != endPos); \
        pos = ptrAdd(pos, extraPadding[sh.length % 8]); \
    } break;

            SHAREMIND_STATIC_ASSERT(sizeof(type) <= sizeof(int));
            switch (static_cast<int>(type)) {

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT: {
                    assert(pos);
                    assert(sh.length);
                    d->staticData->codeSections.emplace_back(
                                static_cast<::SharemindCodeBlock const *>(pos),
                                sh.length);
                    pos = ptrAdd(pos,
                                 sh.length * sizeof(::SharemindCodeBlock));
                } break;

                LOAD_DATASECTION_CASE(RODATA,staticData->rodata,RoDataSection)
                LOAD_DATASECTION_CASE(DATA,data,RwDataSection)

                case SHAREMIND_EXECUTABLE_SECTION_TYPE_BSS:
                    d->bssSectionSizes.emplace_back(sh.length);
                    break;

                LOAD_BINDSECTION_CASE(BIND,
                    if (static_cast<char const *>(pos)[0u] == '\0')
                        throw InvalidInputFileException();
                    ::SharemindSyscallWrapper const w =
                            m_inner->m_vmInner->findSyscall(
                                    static_cast<char const *>(pos));
                    if (!w.callable)
                        throw UndefinedSyscallBindException();
                    d->staticData->syscallBindings.emplace_back(w);)

                LOAD_BINDSECTION_CASE(PDBIND,
                    if (static_cast<char const *>(pos)[0u] == '\0')
                        throw InvalidInputFileException();
                    for (std::size_t i = 0; i < d->staticData->pdBindings.size(); i++)
                        if (::strcmp(
                                SharemindPd_name(d->staticData->pdBindings[i]),
                                static_cast<char const *>(pos)) == 0)
                            throw DuplicatePdBindException();
                    ::SharemindPd * const w =
                            m_inner->m_vmInner->findPd(
                                static_cast<char const *>(pos));
                    if (!w)
                        throw UndefinedPdBindException();
                    d->staticData->pdBindings.emplace_back(w);)

                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(d->staticData->codeSections.size() == ui)) {
            pos = nullptr;
            throw NoCodeSectionsException();
        }

        if (d->staticData->rodataSections.size() == ui)
            d->staticData->rodataSections.emplace_back(
                    std::make_shared<Detail::RoDataSection>(nullptr, 0u));
        if (d->dataSections.size() == ui)
            d->dataSections.emplace_back(
                    std::make_shared<Detail::RwDataSection>(nullptr, 0u));
        if (d->bssSectionSizes.size() == ui)
            d->bssSectionSizes.emplace_back(0u);
    }
    d->staticData->activeLinkingUnit = h.activeLinkingUnit;

    endPrepare(*d);
    GUARD;
    m_inner->m_parseData = std::move(d);
}

SharemindVmInstruction const * Program::instruction(
        std::size_t codeSection,
        std::size_t instructionIndex) const noexcept
{
    {
        GUARD;
        auto const & parseData = m_inner->m_parseData;
        assert(parseData);
        assert(parseData->staticData);
        auto & codeSections = parseData->staticData->codeSections;
        if (parseData && codeSection < codeSections.size()) {
            return codeSections[codeSection].instructionDescriptionAtOffset(
                        instructionIndex);
        }
    }
    return nullptr;
}

std::size_t Program::pdCount() const noexcept {
    GUARD;
    auto const & parseData = m_inner->m_parseData;
    assert(parseData);
    assert(parseData->staticData);
    return parseData->staticData->pdBindings.size();
}

SharemindPd * Program::pd(std::size_t const i) const noexcept {
    {
        GUARD;
        auto const & parseData = m_inner->m_parseData;
        assert(parseData);
        assert(parseData->staticData);
        auto & pdBindings = parseData->staticData->pdBindings;
        if (i < pdBindings.size())
            return pdBindings[i];
    }
    return nullptr;
}

SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(Program,m_inner->)

} // namespace sharemind {
