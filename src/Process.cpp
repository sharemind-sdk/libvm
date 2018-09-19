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

#define SharemindUserDefinedExceptionMessage_PREFIX \
    "User-defined exception with (unsigned) value of "
#define SharemindUserDefinedExceptionMessage_SUFFIX "!"
#define SharemindUserDefinedExceptionMessage \
    SharemindUserDefinedExceptionMessage_PREFIX "%" PRIu64 \
    SharemindUserDefinedExceptionMessage_SUFFIX

namespace {

struct UserDefinedExceptionData {

/* Constants: */

    static constexpr auto const minBufferSize =
            sizeof(SharemindUserDefinedExceptionMessage_PREFIX) - 1u
            + sizeof("18446744073709551615") - 1u // 2^64-1
            + sizeof(SharemindUserDefinedExceptionMessage_SUFFIX); // incl. '\0'

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

    std::unique_ptr<char[]> message{makeUnique<char[]>(minBufferSize)};
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

ProcessState::SimpleMemoryMap::SimpleMemoryMap(
        std::shared_ptr<RoDataSection const> rodataSection,
        std::shared_ptr<RwDataSection> dataSection,
        std::shared_ptr<BssDataSection> bssSection)
{
    insertSlot(1u, std::move(rodataSection));
    insertSlot(2u, std::move(dataSection));
    insertSlot(3u, std::move(bssSection));
}

ProcessState::ProcessState(
        std::shared_ptr<PreparedExecutable const> preparedExecutable)
    : m_activeLinkingUnitIndex(
          assertReturn(preparedExecutable)->activeLinkingUnitIndex)
    , m_preparedLinkingUnit(
        [](std::shared_ptr<PreparedExecutable const> exePtr) noexcept {
            assert(exePtr);
            auto const & activeLinkingUnit =
                    exePtr->linkingUnits[exePtr->activeLinkingUnitIndex];
            return std::shared_ptr<PreparedLinkingUnit const>(
                        std::move(exePtr),
                        &activeLinkingUnit);
        }(std::move(preparedExecutable)))
    , m_memoryMap(std::shared_ptr<RoDataSection const>(
                      m_preparedLinkingUnit,
                      &m_preparedLinkingUnit->roDataSection),
                  std::make_shared<RwDataSection>(
                      m_preparedLinkingUnit->rwDataSection),
                  std::make_shared<BssDataSection>(
                      m_preparedLinkingUnit->bssSectionSize))
{}

ProcessState::~ProcessState() noexcept {
    assert(m_state != State::Starting);
    assert(m_state != State::Running);
}

void ProcessState::run() {
    {
        RUNSTATEGUARD;
        if (unlikely(m_state != State::Initialized))
            throw Process::NotInInitializedStateException();
        m_state = State::Starting;
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
    assert((executeMethod == ExecuteMethod::Run)
           || (executeMethod == ExecuteMethod::Continue));

    auto const setState =
            [this](State const newState) noexcept {
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
        setState(State::Crashed);
        throw;
    }
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

std::shared_ptr<void> ProcessState::findProcessFacility(char const * name)
        const noexcept
{
    if (m_processFacilityFinder && *m_processFacilityFinder)
        return (*m_processFacilityFinder)(name);
    return m_programState->findProcessFacility(name);
}

} /* namespace Detail { */


Process::Inner::Inner(std::shared_ptr<Program::Inner> programInner)
    : ProcessState(assertReturn(programInner)->m_preparedExecutable)
{}

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

std::exception_ptr Process::syscallException() const noexcept
{ return m_inner->m_syscallException; }

std::size_t Process::currentCodeSectionIndex() const noexcept
{ return m_inner->m_activeLinkingUnitIndex; }

std::size_t Process::currentIp() const noexcept
{ return m_inner->m_currentIp; }

void Process::setFacilityFinder(Vm::FacilityFinderFunPtr f) noexcept
{ m_inner->m_processFacilityFinder = std::move(f); }

std::shared_ptr<void> Process::findFacility(char const * name) const noexcept
{ return m_inner->findProcessFacility(name); }

} // namespace sharemind {
