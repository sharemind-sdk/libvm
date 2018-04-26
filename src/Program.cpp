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
#include <istream>
#include <limits>
#include <new>
#include <sharemind/GlobalDeleter.h>
#include <sharemind/IntegralComparisons.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
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

template <typename Exception, typename ValueToRead>
std::istream & wrappedIstreamReadValue(std::istream & is, ValueToRead & v) {
    try {
        if (!(is >> v))
            throw Exception();
    } catch (Exception const &) {
        throw;
    } catch (...) {
        std::throw_with_nested(Exception());
    }
    return is;
}

template <typename Exception, typename SizeType>
std::istream & wrappedIstreamRead(std::istream & is,
                                  void * buffer,
                                  SizeType size)
{
    static constexpr auto const maxRead =
            std::numeric_limits<std::streamsize>::max();
    auto buf = static_cast<char *>(buffer);
    try {
        while (integralGreater(size, maxRead)) {
            if (!is.read(buf, maxRead))
                throw Exception();
            size -= maxRead;
            buf += maxRead;
        }
        if (!is.read(buf, static_cast<std::streamsize>(size)))
            throw Exception();
    } catch (Exception const &) {
        throw;
    } catch (...) {
        std::throw_with_nested(Exception());
    }
    return is;
}

class LimitedBufferFilter: public std::streambuf {

public: /* Methods: */

    LimitedBufferFilter(LimitedBufferFilter &&) = delete;
    LimitedBufferFilter(LimitedBufferFilter const &) = delete;

    LimitedBufferFilter(std::istream & srcStream, std::size_t size)
        : m_srcStream(srcStream)
        , m_sizeLeft(size)
    {}

protected: /* Methods: */

    int_type underflow() override {
        if (gptr() < egptr())
            return traits_type::to_int_type(m_buf);
        if (m_sizeLeft <= 0u)
            return traits_type::eof();
        auto r(m_srcStream.get());
        if (r == traits_type::eof())
            return r;
        --m_sizeLeft;
        m_buf = traits_type::to_char_type(std::move(r));
        this->setg(&m_buf, &m_buf, &m_buf + 1u);
        return r;
    }

private: /* Fields: */

    std::istream & m_srcStream;
    std::size_t m_sizeLeft;
    char m_buf;

};

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
            p.staticData->syscallBindings[c[(*i)+(argNum)].uint64[0]].get(); \
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
        auto const & cm = instructionCodeMap();
        for (std::size_t i = 0u; i < s->size(); i++, numInstrs++) {
            auto const instrIt(cm.find(c[i].uint64[0]));
            if (instrIt == cm.end())
                throw Program::InvalidInstructionException();
            auto const & instr = instrIt->second;
            if (i + instr.numArgs >= s->size())
                throw Program::InvalidInstructionArgumentsException();
            s->registerInstruction(i, numInstrs, instr);
            i += instr.numArgs;
        }

        for (std::size_t i = 0u; i < s->size(); i++) {
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
        } else if (signedToUnsigned(r) == fileSize) {
            break;
        }
        throw FileReadException();
    }

    return loadFromMemory(fileData.get(), fileSize);
}

void Program::loadFromMemory(void const * data, std::size_t dataSize) {
    assert(data);

    struct MemoryBuffer: std::streambuf {
        MemoryBuffer(char const * data, std::size_t size) {
            auto d = const_cast<char *>(data);
            this->setg(d, d, d + size);
        }
    };

    struct InputMemoryStream: virtual MemoryBuffer, std::istream {
        InputMemoryStream(void const * data, std::size_t size)
            : MemoryBuffer(static_cast<char const *>(data), size)
            , std::istream(static_cast<std::streambuf *>(this))
        {}
    };

    InputMemoryStream inStream(data, dataSize);
    return loadFromStream(inStream);
}

void Program::loadFromStream(std::istream & is) {

    auto d(std::make_shared<Detail::ParseData>());

    ExecutableCommonHeader ch;
    wrappedIstreamReadValue<InvalidFileHeaderException>(is, ch);

    if (ch.fileFormatVersion() > 0u)
        throw VersionMismatchException();

    ExecutableHeader0x0 h;
    wrappedIstreamReadValue<InvalidFileHeader0x0Exception>(is, h);

    static std::size_t const extraPadding[8] =
            { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };
    char extraPaddingBuffer[8u];

    auto const readAndCheckExtraPadding =
            [&is, &extraPaddingBuffer](std::size_t size) {
                assert(sizeof(extraPaddingBuffer) >= size);
                static_assert(std::numeric_limits<std::streamsize>::max() >= 8,
                              "");
                if (!is.read(extraPaddingBuffer,
                             static_cast<std::streamsize>(size)))
                    throw InvalidInputFileException();
                for (unsigned i = 0u; i < size; ++i)
                    if (extraPaddingBuffer[i] != '\0')
                        throw InvalidInputFileException();
            };

    for (unsigned ui = 0; ui <= h.numberOfLinkingUnitsMinusOne(); ui++) {
        ExecutableLinkingUnitHeader0x0 uh;
        wrappedIstreamReadValue<InvalidLinkingUnitHeader0x0Exception>(is, uh);

        for (unsigned si = 0; si <= uh.numberOfSectionsMinusOne(); si++) {
            ExecutableSectionHeader0x0 sh;
            wrappedIstreamReadValue<InvalidSectionHeader0x0Exception>(is, sh);

            auto const type = sh.type();
            assert(type != ExecutableSectionHeader0x0::SectionType::Invalid);

            auto const sectionSize = sh.size();
            if (unlikely(integralGreater(
                             sectionSize,
                             std::numeric_limits<std::size_t>::max())))
                throw ImplementationLimitsReachedException();

#define LOAD_DATASECTION_CASE(utype,ltype,spec) \
    case ExecutableSectionHeader0x0::SectionType::utype: { \
        auto newSection(std::make_shared<Detail::spec>(sectionSize)); \
        wrappedIstreamRead<InvalidInputFileException>(is, \
                                                      newSection->data(), \
                                                      sectionSize); \
        readAndCheckExtraPadding(extraPadding[sectionSize % 8]); \
        d-> ltype ## Sections.emplace_back(std::move(newSection)); \
    } break;
#define LOAD_BINDSECTION_CASE(utype,e,missingBindsExceptionPrefix,...) \
    case ExecutableSectionHeader0x0::SectionType::utype: { \
        if (sectionSize <= 0) \
            break; \
        static_assert(std::numeric_limits<decltype(sectionSize)>::max() \
                      < std::numeric_limits<std::size_t>::max(), ""); \
        LimitedBufferFilter limitedBuffer(is, sectionSize); \
        std::istream is2(&limitedBuffer); \
        std::set<std::string> missingBinds; \
        std::string bindName; \
        while (std::getline(is2, bindName, '\0')) { \
            if (bindName.empty()) \
                throw InvalidInputFileException(); \
            { __VA_ARGS__ } \
        } \
        if (!missingBinds.empty()) { \
            std::ostringstream oss; \
            oss << missingBindsExceptionPrefix; \
            bool first = true; \
            for (auto & missingBind : missingBinds) { \
                if (first) { \
                    first = false; \
                } else { \
                    oss << ", "; \
                } \
                oss << std::move(missingBind); \
            } \
            throw e(oss.str()); \
        } \
        readAndCheckExtraPadding(extraPadding[sectionSize % 8]); \
    } break;

            switch (type) {

                case ExecutableSectionHeader0x0::SectionType::Text: {
                    assert(sectionSize);
                    d->staticData->codeSections.emplace_back(sectionSize);
                    auto & cs = d->staticData->codeSections.back();
                    wrappedIstreamRead<InvalidInputFileException>(
                                is,
                                cs.data(),
                                cs.size() * sizeof(SharemindCodeBlock));
                } break;

                LOAD_DATASECTION_CASE(RoData,staticData->rodata,RoDataSection)
                LOAD_DATASECTION_CASE(Data,data,RwDataSection)

                case ExecutableSectionHeader0x0::SectionType::Bss:
                    d->bssSectionSizes.emplace_back(sectionSize);
                    break;

                LOAD_BINDSECTION_CASE(Bind,
                    UndefinedSyscallBindException,
                    "Found bindings for undefined systems calls: ",
                    if (auto w = m_inner->m_vmInner->findSyscall(bindName)) {
                        d->staticData->syscallBindings.emplace_back(
                                    std::move(w));
                    } else {
                        missingBinds.emplace(std::move(bindName));
                    })

                LOAD_BINDSECTION_CASE(PdBind,
                    UndefinedPdBindException,
                    "Found bindings for undefined protection domains: ",
                    for (auto const & pdBinding : d->staticData->pdBindings)
                        if (SharemindPd_name(pdBinding) == bindName)
                            throw DuplicatePdBindException(
                                    "Duplicate bindings found for protection "
                                    "domain: " + std::move(bindName));
                    if (auto * const w = m_inner->m_vmInner->findPd(bindName)) {
                        d->staticData->pdBindings.emplace_back(w);
                    } else {
                        missingBinds.emplace(std::move(bindName));
                    })

                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(d->staticData->codeSections.size() == ui))
            throw NoCodeSectionsException();

        if (d->staticData->rodataSections.size() == ui)
            d->staticData->rodataSections.emplace_back(
                    std::make_shared<Detail::RoDataSection>(0u));
        if (d->dataSections.size() == ui)
            d->dataSections.emplace_back(
                    std::make_shared<Detail::RwDataSection>(0u));
        if (d->bssSectionSizes.size() == ui)
            d->bssSectionSizes.emplace_back(0u);
    }
    d->staticData->activeLinkingUnit = h.activeLinkingUnitIndex();

    endPrepare(*d);
    GUARD;
    m_inner->m_parseData = std::move(d);
}

VmInstructionInfo const * Program::instruction(
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
