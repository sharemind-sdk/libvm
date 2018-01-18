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
#include <unordered_map>
#include <utility>
#include "Core.h"
#include "DataSectionsVector.h"
#include "MemoryMap.h"
#include "PdpiCache.h"
#include "ProcessFacilityMap.h"
#include "Program_p.h"
#include "StackFrame.h"


extern "C"
SharemindModuleApi0x1PdpiInfo const * sharemind_get_pdpi_info(
        SharemindModuleApi0x1SyscallContext * c,
        uint64_t pd_index) __attribute__((visibility("internal")));

extern "C"
void * sharemind_processFacility(SharemindModuleApi0x1SyscallContext const * c,
                                 char const * facilityName)
        __attribute__((visibility("internal")));

extern "C"
std::uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c,
                                     std::uint64_t nBytes)
        __attribute__((visibility("internal")));

extern "C"
bool sharemind_public_free(SharemindModuleApi0x1SyscallContext * c,
                           std::uint64_t ptr)
        __attribute__((visibility("internal")));

extern "C"
std::size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c,
                                      std::uint64_t ptr)
        __attribute__((visibility("internal")));

extern "C"
void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c,
                                std::uint64_t ptr)
        __attribute__((visibility("internal")));

extern "C"
void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
        __attribute__((visibility("internal")));

extern "C"
void sharemind_private_free(SharemindModuleApi0x1SyscallContext * c,
                            void * ptr)
        __attribute__((visibility("internal")));

extern "C"
bool sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
        __attribute__((visibility("internal")));

extern "C"
bool sharemind_private_release(SharemindModuleApi0x1SyscallContext * c,
                               std::size_t nBytes)
        __attribute__((visibility("internal")));

namespace sharemind {
namespace Detail {

struct ParseData;

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

    struct DataSections: DataSectionsVector {
        DataSections(DataSectionsVector & originalDataSections);
    };

    struct BssSections: DataSectionsVector {
        BssSections(DataSectionSizesVector & sizes);
    };

    struct SimpleMemoryMap: MemoryMap {
        SimpleMemoryMap(std::shared_ptr<DataSection> rodataSection,
                        std::shared_ptr<DataSection> dataSection,
                        std::shared_ptr<DataSection> bssSection);
    };

    enum class State {
        Initialized,
        Starting,
        Running,
        Trapped,
        Finished,
        Crashed
    };

/* Methods: */

    ProcessState(std::shared_ptr<ParseData> parseData);
    virtual ~ProcessState() noexcept;

    void setPdpiFacility(char const * const name,
                         void * const facility,
                         void * const context);

    void setInternal(void * const value);

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

    auto pdpiInfo(std::size_t const index) const noexcept
            -> decltype(std::declval<PdpiCache &>().info(index))
    { return m_pdpiCache.info(index); }

    inline CodeSection & currentCodeSection() const noexcept
    { return m_staticProgramData->codeSections[m_currentCodeSectionIndex]; }

    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS

/* Fields: */

    SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS;

    mutable std::mutex m_mutex;

    /// Keeps alive read-only data and code:
    std::shared_ptr<StaticData> m_staticProgramData;

    DataSections m_dataSections;
    BssSections m_bssSections;

    PdpiCache m_pdpiCache;

    std::size_t const m_currentCodeSectionIndex;
    std::size_t m_currentIp = 0u;
    SharemindCodeBlock m_returnValue;
    Process::UserDefinedException m_userException;
    SharemindModuleApi0x1Error m_syscallException = SHAREMIND_MODULE_API_0x1_OK;

    std::atomic<bool> m_trapCond{false};

    // This state replicates default AMD64 behaviour.
    // NB! By default, we ignore any fpu exceptions.
    sf_fpu_state m_fpuState =
            static_cast<sf_fpu_state>(sf_float_tininess_after_rounding
                                      | sf_float_round_nearest_even);

    SimpleMemoryMap m_memoryMap;
    std::uint64_t m_memorySlotNext;
    PrivateMemoryMap m_privateMemoryMap;

    SharemindModuleApi0x1SyscallContext m_syscallContext;

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

    Inner(std::shared_ptr<Program::Inner> programInner);
    ~Inner() noexcept;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROCESS_H */