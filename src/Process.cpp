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

#include "Process.h"
#include "Process_p.h"

#include <cassert>
#include <cinttypes>
#include <limits>
#include <new>
#include <sharemind/AssertReturn.h>
#include <sharemind/MakeUnique.h>
#include "Program.h"


#define SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c) \
    do { assert((c)); assert((c)->vm_internal); } while (false)
#define SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c) \
    (*static_cast<::sharemind::Detail::ProcessState *>( \
            ::sharemind::assertReturn((c)->vm_internal)))

extern "C"
SharemindModuleApi0x1PdpiInfo const * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (pd_index > SIZE_MAX)
        return nullptr;

    auto const & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.pdpiInfo(pd_index);
}

extern "C"
void * sharemind_processFacility(SharemindModuleApi0x1SyscallContext const * c,
                                 char const * facilityName)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);
    assert(facilityName);
    return SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c).processFacility(facilityName);
}

extern "C"
std::uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c,
                                     std::uint64_t nBytes)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.publicAlloc(nBytes);
}

extern "C"
bool sharemind_public_free(SharemindModuleApi0x1SyscallContext * c,
                           std::uint64_t ptr)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);

    auto const r = ps.publicFree(ptr);
    assert(r == sharemind::Detail::MemoryMap::Ok
           || r == sharemind::Detail::MemoryMap::MemorySlotInUse
           || r == sharemind::Detail::MemoryMap::InvalidMemoryHandle);
    return r != sharemind::Detail::MemoryMap::MemorySlotInUse;
}

extern "C"
std::size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c,
                                      std::uint64_t ptr)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(ptr == 0u))
        return 0u;

    auto const & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.publicSlotSize(ptr);
}

extern "C"
void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c,
                                std::uint64_t ptr)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(ptr == 0u))
        return nullptr;

    auto const & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.publicSlotPtr(ptr);
}

extern "C"
void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(nBytes == 0))
        return nullptr;

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.privateAlloc(nBytes);
}

extern "C"
void sharemind_private_free(SharemindModuleApi0x1SyscallContext * c,
                            void * ptr)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(!ptr))
        return;

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.privateFree(ptr);
}

extern "C"
bool sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(nBytes == 0u))
        return true;

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.privateReserve(nBytes);
}

extern "C"
bool sharemind_private_release(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
{
    SHAREMIND_LIBVM_PROCESS_CHECK_CONTEXT(c);

    if (unlikely(nBytes == 0u))
        return true;

    auto & ps = SHAREMIND_LIBVM_STATE_FROM_CONTEXT(c);
    return ps.privateRelease(nBytes);
}

namespace sharemind {

#define GUARD_(g) std::lock_guard<decltype(g)> const guard(g)
#define GUARD GUARD_(m_mutex)
#define RUNSTATEGUARD GUARD_(m_runStateMutex)
#define DEFINE_BASE_EXCEPTION(base,e) \
    SHAREMIND_DEFINE_EXCEPTION_NOINLINE(base, Process::, e)
#define DEFINE_CONSTMSG_EXCEPTION(base,e,msg) \
    SHAREMIND_DEFINE_EXCEPTION_CONST_MSG_NOINLINE(base, Process::, e, msg)

DEFINE_BASE_EXCEPTION(sharemind::Exception, Exception);
DEFINE_BASE_EXCEPTION(Exception, InvalidInputStateException);
DEFINE_CONSTMSG_EXCEPTION(
        InvalidInputStateException,
        NotInInitializedStateException,
        "Process not in initialized (pre-run) state!");
DEFINE_CONSTMSG_EXCEPTION(
        InvalidInputStateException,
        NotInTrappedStateException,
        "Process not in trapped state!");
DEFINE_BASE_EXCEPTION(Exception, RuntimeException);
DEFINE_CONSTMSG_EXCEPTION(RuntimeException, TrapException, "Process trapped!");
DEFINE_BASE_EXCEPTION(RuntimeException, RegularRuntimeException);
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidStackIndexException,
                          "Invalid stack index!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidRegisterIndexException,
                          "Invalid register index!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidReferenceIndexException,
                          "Invalid reference index!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidConstReferenceIndexException,
                          "Invalid constant reference index!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidSyscallIndexException,
                          "Invalid system call index!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidMemoryHandleException,
                          "Invalid memory handle!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          OutOfBoundsReadException,
                          "Read out of bounds!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          OutOfBoundsWriteException,
                          "Write out of bounds!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          WriteDeniedException,
                          "Write denied!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          JumpToInvalidAddressException,
                          "Jump to invalid address!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          SystemCallErrorException,
                          "System call error!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          InvalidArgumentException,
                          "Invalid argument!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          OutOfBoundsReferenceOffsetException,
                          "Out of bounds reference offset!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          OutOfBoundsReferenceSizeException,
                          "Out of bounds reference size!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          IntegerDivideByZeroException,
                          "Integer divide by zero!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          IntegerOverflowException,
                          "Integer overflow!");
DEFINE_CONSTMSG_EXCEPTION(RegularRuntimeException,
                          MemoryInUseException,
                          "Attempted to free memory which is in use!");

#define SharemindUserDefinedExceptionMessage \
        "User-defined exception with (unsigned) value of %" PRIu64 "!"

namespace {

struct UserDefinedExceptionData {

/* Methods: */

    UserDefinedExceptionData() { setErrorCode(0u); }

    void setErrorCode(std::uint64_t const value) noexcept {
        errorCode = value;
        std::sprintf(message.get(),
                     SharemindUserDefinedExceptionMessage,
                     value);
    }

    static UserDefinedExceptionData & fromPtr(std::shared_ptr<void> const & ptr)
    { return *static_cast<UserDefinedExceptionData *>(ptr.get()); }

/* Fields: */

    std::unique_ptr<char[]> message{
            makeUnique<char[]>(
                        sizeof(SharemindUserDefinedExceptionMessage) + 21u)};
    std::uint64_t errorCode = 0u;
};

} // anonymous namespace

Process::UserDefinedException::UserDefinedException()
    : m_data(std::make_shared<UserDefinedExceptionData>())
{}

Process::UserDefinedException::UserDefinedException(
        UserDefinedException &&)
        noexcept(std::is_nothrow_move_constructible<
                        RegularRuntimeException>::value) = default;

Process::UserDefinedException::UserDefinedException(
        UserDefinedException const &)
        noexcept(std::is_nothrow_copy_constructible<
                        RegularRuntimeException>::value) = default;

Process::UserDefinedException::~UserDefinedException() noexcept = default;

Process::UserDefinedException & Process::UserDefinedException::operator=(
        UserDefinedException &&)
        noexcept(std::is_nothrow_move_assignable<
                        RegularRuntimeException>::value) = default;

Process::UserDefinedException & Process::UserDefinedException::operator=(
        UserDefinedException const &)
        noexcept(std::is_nothrow_copy_assignable<
                        RegularRuntimeException>::value) = default;

std::uint64_t Process::UserDefinedException::errorCode() const noexcept
{ return UserDefinedExceptionData::fromPtr(m_data).errorCode; }

void Process::UserDefinedException::setErrorCode(std::uint64_t const value)
        noexcept
{ return UserDefinedExceptionData::fromPtr(m_data).setErrorCode(value); }

char const * Process::UserDefinedException::what() const noexcept
{ return UserDefinedExceptionData::fromPtr(m_data).message.get(); }

DEFINE_BASE_EXCEPTION(Exception, FloatingPointException);
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        FloatingPointDivideByZeroException,
        "Floating point division by zero!");
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        FloatingPointOverflowException,
        "Floating point overflow!");
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        FloatingPointUnderflowException,
        "Floating point underflow!");
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        FloatingPointInexactResultException,
        "Inexact floating point result!");
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        FloatingPointInvalidOperationException,
        "Invalid floating point operation!");
DEFINE_CONSTMSG_EXCEPTION(
        FloatingPointException,
        UnknownFloatingPointException,
        "Unknown floating point exception!");

namespace Detail {

PrivateMemoryMap::~PrivateMemoryMap() noexcept {
    for (auto const & vp : m_data)
        ::operator delete(vp.first);
}

void * PrivateMemoryMap::allocate(std::size_t const nBytes) {
    assert(nBytes > 0u);
    auto const r = ::operator new(nBytes);
    try {
        m_data.emplace(r, nBytes);
    } catch (...) {
        ::operator delete(r);
        throw;
    }
    return r;
}

std::size_t PrivateMemoryMap::free(void * const ptr) noexcept {
    auto it(m_data.find(ptr));
    if (it == m_data.end())
        return 0u;
    auto const r(it->second);
    assert(r > 0u);
    m_data.erase(it);
    ::operator delete(ptr);
    return r;
}

ProcessState::DataSections::DataSections(
        DataSectionsVector & originalDataSections)
{
    for (auto const & s : originalDataSections)
        emplace_back(std::make_shared<RwDataSection>(s->data(),
                                                     s->size()));
}

ProcessState::BssSections::BssSections(DataSectionSizesVector & sizes) {
    for (auto const size : sizes) {
        static_assert(std::numeric_limits<decltype(size)>::max()
                      <= std::numeric_limits<std::size_t>::max(), "");
        emplace_back(std::make_shared<BssDataSection>(size));
    }
}

ProcessState::SimpleMemoryMap::SimpleMemoryMap(
        std::shared_ptr<DataSection> rodataSection,
        std::shared_ptr<DataSection> dataSection,
        std::shared_ptr<DataSection> bssSection)
{
    insertDataSection(1u, std::move(rodataSection));
    insertDataSection(2u, std::move(dataSection));
    insertDataSection(3u, std::move(bssSection));
}

ProcessState::ProcessState(std::shared_ptr<ParseData> parseData)
    : m_staticProgramData(assertReturn(assertReturn(parseData)->staticData))
    , m_dataSections(parseData->dataSections)
    , m_bssSections(parseData->bssSectionSizes)
    , m_pdpiCache(parseData->staticData->pdBindings)
    , m_currentCodeSectionIndex(parseData->staticData->activeLinkingUnit)
    , m_memoryMap(
          parseData->staticData->rodataSections[m_currentCodeSectionIndex],
          m_dataSections[m_currentCodeSectionIndex],
          m_bssSections[m_currentCodeSectionIndex])
    , m_syscallContext{this,    // vm_internal
                       nullptr, // process_internal
                       nullptr, // module_handle
                       &sharemind_get_pdpi_info,
                       &sharemind_processFacility,
                       &sharemind_public_alloc,
                       &sharemind_public_free,
                       &sharemind_public_get_size,
                       &sharemind_public_get_ptr,
                       &sharemind_private_alloc,
                       &sharemind_private_free,
                       &sharemind_private_reserve,
                       &sharemind_private_release}
{}

ProcessState::~ProcessState() noexcept {
    assert(m_state != State::Starting);
    assert(m_state != State::Running);
    if (m_state == State::Trapped)
        m_pdpiCache.stopPdpis();

    m_frames.clear();
    m_pdpiCache.clear();
}

void ProcessState::setPdpiFacility(char const * const name,
                                   void * const facility,
                                   void * const context)
{
    assert(name);
    GUARD;
    m_pdpiCache.setPdpiFacility(name, facility, context);
}

void ProcessState::setInternal(void * const value) {
    GUARD;
    m_syscallContext.process_internal = value;
}

void ProcessState::run() {
    {
        RUNSTATEGUARD;
        if (unlikely(m_state != State::Initialized))
            throw Process::NotInInitializedStateException();
        m_state = State::Starting;
    }

    try {
        m_pdpiCache.startPdpis();
    } catch (...) {
        RUNSTATEGUARD;
        assert(m_state == State::Starting);
        m_state = State::Initialized;
        throw;
    }

    {
        RUNSTATEGUARD;
        assert(m_state == State::Starting);
        m_state = State::Running;
    }
    return execute(ExecuteMethod::Run);
}

void ProcessState::resume() {
    {
        RUNSTATEGUARD;
        if (unlikely(m_state != State::Trapped))
            throw Process::NotInTrappedStateException();
        m_state = State::Running;
    }
    return execute(ExecuteMethod::Continue);
}

void ProcessState::execute(ExecuteMethod const executeMethod) {
    auto const setState =
            [this](State const newState) {
                RUNSTATEGUARD;
                assert(m_state == State::Running);
                m_state = newState;
            };

    try {
        vmRun(executeMethod, this);
    } catch (Process::TrapException const &) {
        setState(State::Trapped);
        throw;
    } catch (...) {
        m_pdpiCache.stopPdpis();
        setState(State::Crashed);
        throw;
    }
    m_pdpiCache.stopPdpis();
    setState(State::Finished);
}

void ProcessState::pause() noexcept
{ m_trapCond.store(true, std::memory_order_release); }

std::uint64_t ProcessState::publicAlloc(std::uint64_t const nBytes) {
    /* Check memory limits: */
    if (unlikely((m_memTotal.upperLimit - m_memTotal.usage < nBytes)
                 || (m_memPublicHeap.upperLimit - m_memPublicHeap.usage
                     < nBytes)))
        return 0u;

    try {
        auto const index(m_memoryMap.allocate(nBytes));

        /* Update memory statistics: */
        m_memPublicHeap.usage += nBytes;
        m_memTotal.usage += nBytes;
        if (m_memPublicHeap.usage > m_memPublicHeap.max)
            m_memPublicHeap.max = m_memPublicHeap.usage;
        if (m_memTotal.usage > m_memTotal.max)
            m_memTotal.max = m_memTotal.usage;

        return index;
    } catch (...) {
        return 0u;
    }
}

MemoryMap::ErrorCode ProcessState::publicFree(std::uint64_t const ptr) noexcept
{
    auto const r(m_memoryMap.free(ptr));
    if (r.first == MemoryMap::Ok) {
        /* Update memory statistics: */
        assert(m_memPublicHeap.usage >= r.second);
        assert(m_memTotal.usage >= r.second);
        m_memPublicHeap.usage -= r.second;
        m_memTotal.usage -= r.second;
    }
    return r.first;
}

std::size_t ProcessState::publicSlotSize(std::uint64_t const ptr)
        const noexcept
{ return m_memoryMap.slotSize(ptr); }

void * ProcessState::publicSlotPtr(std::uint64_t const ptr) const noexcept
{ return m_memoryMap.slotPtr(ptr); }

void * ProcessState::privateAlloc(std::size_t const nBytes) {
    assert(nBytes > 0u);

    /* Check memory limits: */
    if (unlikely((m_memTotal.upperLimit - m_memTotal.usage < nBytes)
                 || (m_memPrivate.upperLimit - m_memPrivate.usage < nBytes)))
        return nullptr;

    /** \todo Check any other memory limits? */

    /* Allocate the memory: */
    try {
        auto const ptr = m_privateMemoryMap.allocate(nBytes);

        /* Update memory statistics: */
        m_memPrivate.usage += nBytes;
        m_memTotal.usage += nBytes;
        if (m_memPrivate.usage > m_memPrivate.max)
            m_memPrivate.max = m_memPrivate.usage;
        if (m_memTotal.usage > m_memTotal.max)
            m_memTotal.max = m_memTotal.usage;

        return ptr;
    } catch (...) {
        return nullptr;
    }
}

void ProcessState::privateFree(void * const ptr) {
    if (auto const bytesDeleted = m_privateMemoryMap.free(ptr)) {
        /* Update memory statistics: */
        assert(m_memPrivate.usage >= bytesDeleted);
        assert(m_memTotal.usage >= bytesDeleted);
        m_memPrivate.usage -= bytesDeleted;
        m_memTotal.usage -= bytesDeleted;
    }
}

bool ProcessState::privateReserve(std::size_t const nBytes) {
    assert(nBytes > 0u);

    /* Check memory limits: */
    if (unlikely((m_memTotal.upperLimit - m_memTotal.usage < nBytes)
                 || (m_memReserved.upperLimit - m_memReserved.usage
                     < nBytes)))
        return false;

    /* Update memory statistics */
    m_memReserved.usage += nBytes;
    m_memTotal.usage += nBytes;
    if (m_memReserved.usage > m_memReserved.max)
        m_memReserved.max = m_memReserved.usage;
    if (m_memTotal.usage > m_memTotal.max)
        m_memTotal.max = m_memTotal.usage;
    return true;
}

extern "C"
bool ProcessState::privateRelease(std::size_t const nBytes) {
    assert(nBytes > 0u);

    /* Check for underflow: */
    if (m_memReserved.usage < nBytes)
        return false;

    assert(m_memTotal.usage >= nBytes);
    m_memReserved.usage -= nBytes;
    m_memTotal.usage -= nBytes;
    return true;
}

SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(ProcessState,)

} /* namespace Detail { */

Process::Inner::Inner(std::shared_ptr<Program::Inner> programInner)
    : ProcessState(assertReturn(programInner->m_parseData)) /// \todo throw instead of assert?
{ SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(*this, *programInner); }

Process::Inner::~Inner() noexcept {}


Process::Process(Program & program)
    : m_inner(std::make_shared<Inner>(assertReturn(program.m_inner)))
{}

Process::~Process() noexcept {}

void Process::run() { return m_inner->run(); }

void Process::resume() { return m_inner->resume(); }

void Process::pause() noexcept { return m_inner->pause(); }

SharemindCodeBlock Process::returnValue() const noexcept
{ return m_inner->m_returnValue; }

SharemindModuleApi0x1Error Process::syscallException() const noexcept
{ return m_inner->m_syscallException; }

std::size_t Process::currentCodeSectionIndex() const noexcept
{ return m_inner->m_currentCodeSectionIndex; }

std::size_t Process::currentIp() const noexcept
{ return m_inner->m_currentIp; }

void Process::setPdpiFacility(char const * const name,
                              void * const facility,
                              void * const context)
{ return m_inner->setPdpiFacility(name, facility, context); }

void Process::setInternal(void * const value)
{ return m_inner->setInternal(value); }

SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_SELF(Process,m_inner->)

} // namespace sharemind {
