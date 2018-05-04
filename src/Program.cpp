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

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <istream>
#include <limits>
#include <new>
#include <sharemind/GlobalDeleter.h>
#include <sharemind/InputMemoryStream.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/libexecutable/Executable.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <sharemind/MakeUnique.h>
#include <sharemind/SignedToUnsigned.h>
#include <streambuf>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include "CommonInstructionMacros.h"
#include "Core.h"
#include "PreparationBlock.h"
#include "Process.h"
#include "Vm_p.h"


namespace sharemind {

namespace {

template <typename Exception,
          typename BindingsVector,
          typename PredicateWithAction,
          typename Predicate,
          typename ExceptionPrefix>
void parseBindings(BindingsVector & r,
                   std::vector<std::string> & bindings,
                   PredicateWithAction predicateWithAction,
                   Predicate predicate,
                   ExceptionPrefix && exceptionPrefix)
{
    assert(r.empty());
    if (bindings.empty())
        return;
    r.reserve(bindings.size());
    auto it(bindings.begin());

    do {
        if (!predicateWithAction(r, *it)) {
            std::vector<std::string> missingBinds;
            missingBinds.emplace_back(std::move(*it));
            while (++it != bindings.end())
                if (!predicate(*it))
                    missingBinds.emplace_back(std::move(*it));
            std::ostringstream oss;
            oss << std::forward<ExceptionPrefix>(exceptionPrefix);
            bool isFirstElement = true;
            for (auto & bindName : missingBinds) {
                if (isFirstElement) {
                    isFirstElement = false;
                } else {
                    oss << ", ";
                }
                oss << std::move(bindName);
            }
            throw Exception(oss.str());
        }
    } while (++it != bindings.end());
}

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
#define SHAREMIND_PREPARE_CODESIZE (s.size())

#define SHAREMIND_PREPARE_IS_INSTR(addr) (s.isInstructionAtOffset(addr))
static_assert(std::numeric_limits<std::uint64_t>::max()
              <= std::numeric_limits<std::size_t>::max(), "");
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    do { \
        SHAREMIND_PREPARE_ARGUMENTS_CHECK( \
            c[(*i)+(argNum)].uint64[0] < syscallBindings.size()); \
        c[(*i)+(argNum)].cp[0] = \
                syscallBindings[c[(*i)+(argNum)].uint64[0]].get(); \
    } while ((0))

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,...) \
    static void prepare_pass2_ ## name ( \
            Detail::PreparedSyscallBindings & syscallBindings, \
            Detail::CodeSection & s, \
            SharemindCodeBlock * c, \
            std::size_t * i) \
    { \
        (void) syscallBindings; (void) s; (void) c; (void) i; \
        __VA_ARGS__ \
    }
#include <sharemind/m4/preprocess_pass2_functions.h>

#undef SHAREMIND_PREPARE_PASS2_FUNCTION
#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,_) \
    { bytecode, prepare_pass2_ ## name},
struct preprocess_pass2_function {
    std::uint64_t code;
    void (*f)(Detail::PreparedSyscallBindings & syscallBindings,
              Detail::CodeSection & s,
              SharemindCodeBlock * c,
              std::size_t * i);
};
static struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { 0u, nullptr }
};

} // anonymous namespace


Detail::PreparedSyscallBindings::PreparedSyscallBindings(
        PreparedSyscallBindings &&) noexcept = default;

Detail::PreparedSyscallBindings::PreparedSyscallBindings(
        PreparedSyscallBindings const &) = default;

template <typename SyscallFinder>
Detail::PreparedSyscallBindings::PreparedSyscallBindings(
        std::shared_ptr<Executable::SyscallBindingsSection> parsedBindings,
        SyscallFinder && syscallFinder)
{
    if (parsedBindings) {
        parseBindings<Program::UndefinedSyscallBindException>(
            *this,
            parsedBindings->syscallBindings,
            [&syscallFinder](PreparedSyscallBindings & r,
                       std::string const & bindName)
            {
                if (auto w = syscallFinder(bindName)) {
                    r.emplace_back(std::move(w));
                    return true;
                }
                return false;
            },
            [&syscallFinder](std::string const & bindName)
            { return static_cast<bool>(syscallFinder(bindName)); },
            "Found bindings for undefined systems calls: ");
    }
}

Detail::PreparedSyscallBindings & Detail::PreparedSyscallBindings::operator=(
        PreparedSyscallBindings &&) noexcept = default;

Detail::PreparedSyscallBindings & Detail::PreparedSyscallBindings::operator=(
        PreparedSyscallBindings const &) = default;



Detail::PreparedPdBindings::PreparedPdBindings(PreparedPdBindings &&) noexcept
        = default;

Detail::PreparedPdBindings::PreparedPdBindings(PreparedPdBindings const &)
        = default;

template <typename PdFinder>
Detail::PreparedPdBindings::PreparedPdBindings(
        std::shared_ptr<Executable::PdBindingsSection> parsedBindings,
        PdFinder && pdFinder)
{
    if (parsedBindings) {
        parseBindings<Program::UndefinedPdBindException>(
            *this,
            parsedBindings->pdBindings,
            [&pdFinder](PreparedPdBindings & r,
                        std::string const & bindName)
            {
                if (auto w = pdFinder(bindName)) {
                    r.emplace_back(std::move(w));
                    return true;
                }
                return false;
            },
            [&pdFinder](std::string const & bindName)
            { return static_cast<bool>(pdFinder(bindName)); },
            "Found bindings for undefined protection domains: ");
    }
}

Detail::PreparedPdBindings & Detail::PreparedPdBindings::operator=(
        PreparedPdBindings &&) noexcept = default;

Detail::PreparedPdBindings & Detail::PreparedPdBindings::operator=(
        PreparedPdBindings const &) = default;



Detail::PreparedLinkingUnit::PreparedLinkingUnit(PreparedLinkingUnit &&)
        noexcept = default;

template <typename SyscallFinder, typename PdFinder>
Detail::PreparedLinkingUnit::PreparedLinkingUnit(
        Executable::LinkingUnit && parsedLinkingUnit,
        SyscallFinder && syscallFinder,
        PdFinder && pdFinder)
    : codeSection(
        [](std::shared_ptr<Executable::TextSection> textSection) {
            if (unlikely(!textSection))
                throw Program::NoCodeSectionException();
            return std::move(textSection->instructions);
        }(std::move(parsedLinkingUnit.textSection)))
    , roDataSection(parsedLinkingUnit.roDataSection
                    ? std::move(*parsedLinkingUnit.roDataSection)
                    : Executable::DataSection())
    , rwDataSection(parsedLinkingUnit.rwDataSection
                    ? std::move(*parsedLinkingUnit.rwDataSection)
                    : Executable::DataSection())
    , bssSectionSize(parsedLinkingUnit.bssSection
                     ? parsedLinkingUnit.bssSection->sizeInBytes
                     : 0u)
    , syscallBindings(std::move(parsedLinkingUnit.syscallBindingsSection),
                      std::forward<SyscallFinder>(syscallFinder))
    , pdBindings(std::move(parsedLinkingUnit.pdBindingsSection),
                 std::forward<PdFinder>(pdFinder))
{
    // Prepare the linking unit fully for execution:
    SharemindCodeBlock * const c = codeSection.data();
    assert(c);

    /* Initialize instructions hashmap: */
    std::size_t numInstrs = 0u;
    auto const & cm = instructionCodeMap();
    auto const codeSectionSize(codeSection.size());
    for (std::size_t i = 0u; i < codeSectionSize; i++, numInstrs++) {
        auto const instrIt(cm.find(c[i].uint64[0]));
        if (instrIt == cm.end())
            throw Program::InvalidInstructionException();
        auto const & instr = instrIt->second;
        if (i + instr.numArgs >= codeSectionSize)
            throw Program::InvalidInstructionArgumentsException();
        codeSection.registerInstruction(i, numInstrs, instr);
        i += instr.numArgs;
    }

    for (std::size_t i = 0u; i < codeSectionSize; i++) {
        struct preprocess_pass2_function * ppf =
                &preprocess_pass2_functions[0];
        for (;;) {
            if (!(ppf->f))
                throw Program::InvalidInstructionException();
            if (ppf->code == c[i].uint64[0]) {
                (*(ppf->f))(syscallBindings, codeSection, c, &i);
                break;
            }
            ++ppf;
        }
    }

    /* Initialize exception pointer: */
    {
        Detail::PreparationBlock pb{&c[codeSection.size()],
                                    Detail::PreparationBlock::EofLabel};
        Detail::vmRun(Detail::ExecuteMethod::GetInstruction, &pb);
    }
}

Detail::PreparedLinkingUnit & Detail::PreparedLinkingUnit::operator=(
        PreparedLinkingUnit &&) noexcept = default;



Detail::PreparedExecutable::PreparedExecutable(PreparedExecutable &&) noexcept
        = default;

template <typename SyscallFinder, typename PdFinder>
Detail::PreparedExecutable::PreparedExecutable(Executable parsedExecutable,
                                               SyscallFinder && syscallFinder,
                                               PdFinder && pdFinder)
    : activeLinkingUnitIndex(std::move(parsedExecutable.activeLinkingUnitIndex))
{
    linkingUnits.reserve(parsedExecutable.linkingUnits.size());
    for (auto & parsedLinkingUnit : parsedExecutable.linkingUnits)
        linkingUnits.emplace_back(std::move(parsedLinkingUnit),
                                  syscallFinder,
                                  pdFinder);
}

Detail::PreparedExecutable & Detail::PreparedExecutable::operator=(
        PreparedExecutable &&) noexcept = default;


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
EC(Prepare, InvalidFileHeader, "Invalid executable file header!");
EC(Prepare, VersionMismatch, "Executable file format version not supported!");
EC(Prepare, InvalidFileHeader0x0, "Invalid version 0x0 specific file header!");
EC(Prepare, InvalidLinkingUnitHeader0x0,
   "Invalid version 0x0 specific linking unit header!");
EC(Prepare, InvalidSectionHeader0x0,
   "Invalid version 0x0 specific section header!");
EC(Prepare, InvalidInputFile, "Invalid or absent data in executable file!");
SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING_NOINLINE(
        PrepareException,
        Program::,
        UndefinedSyscallBindException)
SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING_NOINLINE(
        PrepareException,
        Program::,
        UndefinedPdBindException)
SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING_NOINLINE(
        PrepareException,
        Program::,
        DuplicatePdBindException)
EC(Prepare, NoCodeSection, "No code section in linking unit found!");
EC(Prepare, InvalidInstruction, "Invalid instruction found!");
EC(Prepare, InvalidInstructionArguments,
   "Invalid arguments for instruction found!");
#undef EC


Program::Inner::Inner(
        std::shared_ptr<Vm::Inner> vmInner,
        std::shared_ptr<Detail::PreparedExecutable> preparedExecutable)
    : m_vmInner(std::move(vmInner))
    , m_preparedExecutable(std::move(preparedExecutable))
{ SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(*this, *m_vmInner); }

Program::Inner::~Inner() noexcept {}

std::shared_ptr<Detail::PreparedExecutable> Program::Inner::loadFromFile(
        Vm::Inner const & vmInner,
        char const * const filename)
{
    assert(filename);

    /* Open input file: */
    int const fd = ::open(filename, O_RDONLY | O_CLOEXEC | O_NOCTTY);
    if (fd < 0)
        throw FileOpenException();
    try {
        auto r = loadFromFileDescriptor(vmInner, fd);
        ::close(fd);
        return r;
    } catch (...) {
        ::close(fd);
        throw;
    }
}

std::shared_ptr<Detail::PreparedExecutable> Program::Inner::loadFromCFile(
        Vm::Inner const & vmInner,
        FILE * const file)
{
    assert(file);
    int const fd = ::fileno(file);
    if (fd == -1)
        throw FileNoException();
    assert(fd >= 0);
    return loadFromFileDescriptor(vmInner, fd);
}

std::shared_ptr<Detail::PreparedExecutable>
Program::Inner::loadFromFileDescriptor(Vm::Inner const & vmInner,
                                       int const fd)
{
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
        return loadFromMemory(vmInner, &fileSize, 0u);

    /* Read file to memory: */
    std::unique_ptr<void, GlobalDeleter> fileData(::operator new(fileSize));
    for (;;) {
        auto const r = ::read(fd, fileData.get(), fileSize);
        if (r < 0) {
            if (errno == EINTR)
                continue;
        } else if (signedToUnsigned(r) == fileSize) {
            break;
        }
        throw FileReadException();
    }

    return loadFromMemory(vmInner, fileData.get(), fileSize);
}

std::shared_ptr<Detail::PreparedExecutable> Program::Inner::loadFromMemory(
        Vm::Inner const & vmInner,
        void const * data,
        std::size_t dataSize)
{
    assert(data);
    InputMemoryStream inStream(static_cast<char const *>(data), dataSize);
    return loadFromStream(vmInner, inStream);
}

std::shared_ptr<Detail::PreparedExecutable> Program::Inner::loadFromStream(
        Vm::Inner const & vmInner,
        std::istream & is)
{
    Executable parsedExecutable;
    {
        auto oldExceptionMask(is.exceptions());
        is.exceptions(std::ios_base::failbit
                      | std::ios_base::badbit
                      | std::ios_base::eofbit);
        try {
            is >> parsedExecutable;
        } catch (...) {
            is.exceptions(std::move(oldExceptionMask));
            throw;
        }
        is.exceptions(std::move(oldExceptionMask));
    }
    return loadFromExecutable(vmInner, std::move(parsedExecutable));
}

std::shared_ptr<Detail::PreparedExecutable> Program::Inner::loadFromExecutable(
            Vm::Inner const & vmInner,
            Executable && executable)
{
    assert(!executable.linkingUnits.empty());
    assert(executable.activeLinkingUnitIndex < executable.linkingUnits.size());

    return std::make_shared<Detail::PreparedExecutable>(
                std::move(executable),
                [&vmInner](std::string const & syscallSignature)
                { return vmInner.findSyscall(syscallSignature); },
                [&vmInner](std::string const & pdSignature)
                { return vmInner.findPd(pdSignature); });
}


Program::Program(Vm & vm, char const * filename)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromFile(*vm.m_inner,
                                                          filename)))
{}

Program::Program(Vm & vm, FILE * file)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromCFile(*vm.m_inner, file)))
{}

Program::Program(Vm & vm, int fd)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromFileDescriptor(*vm.m_inner,
                                                                    fd)))
{}

Program::Program(Vm & vm, void const * data, std::size_t dataSize)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromMemory(*vm.m_inner,
                                                            data,
                                                            dataSize)))
{}

Program::Program(Vm & vm, std::istream & inputStream)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromStream(*vm.m_inner,
                                                            inputStream)))
{}

Program::Program(Vm & vm, Executable executable)
    : m_inner(std::make_shared<Inner>(vm.m_inner,
                                      Inner::loadFromExecutable(
                                          *vm.m_inner,
                                          std::move(executable))))
{}

Program::~Program() noexcept {}

VmInstructionInfo const * Program::instruction(
        std::size_t codeSection,
        std::size_t instructionIndex) const noexcept
{
    {
        assert(m_inner->m_preparedExecutable);
        auto const & preparedExecutable = *m_inner->m_preparedExecutable;
        if (codeSection < preparedExecutable.linkingUnits.size()) {
            auto const & cs =
                    preparedExecutable.linkingUnits[codeSection].codeSection;
            return cs.instructionDescriptionAtOffset(instructionIndex);
        }
    }
    return nullptr;
}

std::size_t Program::pdCount() const noexcept {
    assert(m_inner->m_preparedExecutable);
    assert(!m_inner->m_preparedExecutable->linkingUnits.empty());
    return m_inner->m_preparedExecutable->linkingUnits[0u].pdBindings.size();
}

SharemindPd * Program::pd(std::size_t const i) const noexcept {
    {
        assert(m_inner->m_preparedExecutable);
        assert(!m_inner->m_preparedExecutable->linkingUnits.empty());
        auto const & pdBindings =
                m_inner->m_preparedExecutable->linkingUnits[0u].pdBindings;
        if (i < pdBindings.size())
            return pdBindings[i];
    }
    return nullptr;
}

SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(Program,m_inner->)

} // namespace sharemind {
