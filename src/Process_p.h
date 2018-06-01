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

#ifndef SHAREMIND_LIBVM_PROCESS_P_H
#define SHAREMIND_LIBVM_PROCESS_P_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include "Process.h"

#include <atomic>
#include <cassert>
#include <limits>
#include <list>
#include <mutex>
#include <sharemind/libsoftfloat/softfloat.h>
#include <sharemind/likely.h>
#include <unordered_map>
#include <utility>
#include "Core.h"
#include "MemoryMap.h"
#include "Program_p.h"
#include "StackFrame.h"


namespace sharemind {
namespace Detail {

struct __attribute__((visibility("internal"))) MemoryInfo final {
    std::size_t usage = 0u;
    std::size_t max = 0u;
    std::size_t upperLimit = std::numeric_limits<std::size_t>::max();
};

class __attribute__((visibility("internal"))) PrivateMemoryMap final {

public: /* Methods: */

    ~PrivateMemoryMap() noexcept;

    void * allocate(std::size_t const nBytes);

    std::size_t free(void * const ptr) noexcept;

private: /* Fields: */

    std::unordered_map<void *, std::size_t> m_data;

};

struct __attribute__((visibility("internal"))) ProcessState {

/* Types: */

    struct SimpleMemoryMap: MemoryMap {
        SimpleMemoryMap(std::shared_ptr<RoDataSection const> rodataSection,
                        std::shared_ptr<RwDataSection> dataSection,
                        std::shared_ptr<BssDataSection> bssSection);
    };

    enum class State {
        Initialized,
        Starting,
        Running,
        Trapped,
        Finished,
        Crashed
    };

    struct SyscallContext final: Vm::SyscallContext {

    /* Methods: */

        SyscallContext(ProcessState & state) noexcept : m_state(state) {}

        std::shared_ptr<void> processFacility(char const * facilityName)
                const noexcept final override
        { return m_state.findProcessFacility(facilityName); }

        /* Access to public dynamic memory inside the VM process: */
        PublicMemoryPointer publicAlloc(std::uint64_t nBytes) final override
        { return {m_state.publicAlloc(nBytes)}; }

        bool publicFree(PublicMemoryPointer ptr) final override
        { return m_state.publicFree(ptr.ptr); }

        std::size_t publicMemPtrSize(PublicMemoryPointer ptr) final override
        { return m_state.publicSlotSize(ptr.ptr); }

        void * publicMemPtrData(PublicMemoryPointer ptr) final override
        { return m_state.publicSlotPtr(ptr.ptr); }

        std::size_t currentLinkingUnitIndex() const noexcept final override
        { return m_state.m_activeLinkingUnitIndex; }

        std::size_t currentInstructionIndex() const noexcept final override
        { return m_state.m_currentIp; }

    /* Fields: */

        ProcessState & m_state;

    };

/* Methods: */

    ProcessState(std::shared_ptr<PreparedExecutable const> preparedExecutable);

    virtual ~ProcessState() noexcept;

    void setPdpiFacility(char const * const name,
                         void * const facility,
                         void * const context);

    void setInternal(std::shared_ptr<void> value);

    void run();
    void resume();
    void execute(ExecuteMethod const executeMethod);
    void pause() noexcept;

    std::uint64_t publicAlloc(std::uint64_t const size);
    MemoryMap::ErrorCode publicFree(std::uint64_t const ptr) noexcept;
    std::size_t publicSlotSize(std::uint64_t const ptr) const noexcept;
    void * publicSlotPtr(std::uint64_t const ptr) const noexcept;
    void * privateAlloc(std::size_t const nBytes);
    void privateFree(void * const ptr);
    bool privateReserve(std::size_t const nBytes);
    bool privateRelease(std::size_t const nBytes);

    inline CodeSection const & currentCodeSection() const noexcept
    { return m_preparedLinkingUnit->codeSection; }

    template <typename F, typename ... Args>
    auto runStatefulSoftfloatOperation(F f, Args && ... args)
            -> decltype(
                f(std::forward<Args>(args)...,
                  std::declval<sf_fpu_state const &>()).result)
    {
        m_fpuState = m_fpuState & ~sf_fpu_state_exception_mask;
        auto r(f(std::forward<Args>(args)...,
                 const_cast<decltype(m_fpuState) const &>(m_fpuState)));
        m_fpuState = r.fpu_state;
        sf_fpu_state const e =
                (r.fpu_state & sf_fpu_state_exception_mask)
                 & ((r.fpu_state & sf_fpu_state_exception_crash_mask) << 5u);
        if (unlikely(e)) {
            if (e & sf_float_flag_divbyzero) {
                throw Process::FloatingPointDivideByZeroException();
            } else if (e & sf_float_flag_overflow) {
                throw Process::FloatingPointOverflowException();
            } else if (e & sf_float_flag_underflow) {
                throw Process::FloatingPointUnderflowException();
            } else if (e & sf_float_flag_inexact) {
                throw Process::FloatingPointInexactResultException();
            } else if (e & sf_float_flag_invalid) {
                throw Process::FloatingPointInvalidOperationException();
            } else {
                throw Process::UnknownFloatingPointException();
            }
        }
        return r.result;
    }

    std::shared_ptr<void> findProcessFacility(char const * name) const noexcept;

/* Fields: */

    Vm::FacilityFinderFunPtr m_processFacilityFinder;
    std::shared_ptr<ProgramState> m_programState;

    mutable std::mutex m_mutex;

    std::size_t m_activeLinkingUnitIndex;
    /// Keeps alive read-only data and code:
    std::shared_ptr<PreparedLinkingUnit const> m_preparedLinkingUnit;

    std::size_t m_currentIp = 0u;
    SharemindCodeBlock m_returnValue;
    Process::UserDefinedException m_userException;
    std::exception_ptr m_syscallException;

    std::atomic<bool> m_trapCond{false};

    // This state replicates default AMD64 behaviour.
    // NB! By default, we ignore any fpu exceptions.
    sf_fpu_state m_fpuState =
            static_cast<sf_fpu_state>(sf_float_tininess_after_rounding
                                      | sf_float_round_nearest_even);

    SimpleMemoryMap m_memoryMap;
    std::uint64_t m_memorySlotNext;
    PrivateMemoryMap m_privateMemoryMap;

    SyscallContext m_syscallContext{*this};

    MemoryInfo m_memPublicHeap;
    MemoryInfo m_memPrivate;
    MemoryInfo m_memReserved;
    MemoryInfo m_memTotal;

    std::list<StackFrame> m_frames;
    StackFrame * m_globalFrame{
            (static_cast<void>(m_frames.emplace_back()),
             /* Triggers halt on return: */
             static_cast<void>(m_frames.back().returnAddr = nullptr),
             &m_frames.back())};
    StackFrame * m_nextFrame = nullptr;
    StackFrame * m_thisFrame = m_globalFrame;

    mutable std::mutex m_runStateMutex;
    State m_state = State::Initialized;

}; /* class ProcessState */
} /* namespace Detail { */


struct __attribute__((visibility("internal"))) Process::Inner final
    : Detail::ProcessState
{

/* Methods: */

    Inner(std::shared_ptr<Program::Inner> programInner);
    ~Inner() noexcept;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROCESS_H */
