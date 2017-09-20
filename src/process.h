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

#ifndef SHAREMIND_LIBVM_PROCESS_H
#define SHAREMIND_LIBVM_PROCESS_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cstddef>
#include <limits>
#include <list>
#include <memory>
#include <sharemind/extern_c.h>
#include <sharemind/libsoftfloat/softfloat.h>
#include <sharemind/recursive_locks.h>
#include <sharemind/stringmap.h>
#include <sharemind/tag.h>
#include <unordered_map>
#include "datasectionsvector.h"
#include "lasterror.h"
#include "libvm.h"
#include "memorymap.h"
#include "pdpicache.h"
#include "processfacilitymap.h"
#include "stackframe.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindProcess_ {

/* Types: */

    struct MemoryInfo {
        std::size_t usage = 0u;
        std::size_t max = 0u;
        std::size_t upperLimit = std::numeric_limits<std::size_t>::max();
    };

    class PrivateMemoryMap {

    public: /* Methods: */

        ~PrivateMemoryMap() noexcept {
            for (auto const & vp : m_data)
                ::operator delete(vp.first);
        }

        void * allocate(std::size_t const nBytes) {
            assert(nBytes > 0u);
            return m_data.emplace(::operator new(nBytes), nBytes).first->first;
        }

        std::size_t free(void * const ptr) noexcept {
            auto it(m_data.find(ptr));
            if (it == m_data.end())
                return 0u;
            auto const r(it->second);
            assert(r > 0u);
            m_data.erase(it);
            ::operator delete(ptr);
            return r;
        }

    private: /* Fields: */

        std::unordered_map<void *, std::size_t> m_data;

    };

/* Methods: */

    std::uint64_t publicAlloc(std::uint64_t size);
    void publicFree(std::uint64_t ptr);

/* Fields: */

    SharemindProgram * program;

    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;
    SHAREMIND_TAG_DECLARE_FIELDS;

    sharemind::DataSectionsVector dataSections;
    sharemind::DataSectionsVector bssSections;

    sharemind::PdpiCache pdpiCache;

    size_t currentCodeSectionIndex;
    uintptr_t currentIp;
    bool trapCond;

    sf_fpu_state fpuState;

    sharemind::MemoryMap memoryMap;
    uint64_t memorySlotNext;
    PrivateMemoryMap privateMemoryMap;

    SharemindCodeBlock returnValue;
    int64_t exceptionValue;
    SharemindModuleApi0x1Error syscallException;

    SharemindModuleApi0x1SyscallContext syscallContext;
    SharemindProcessFacilityMap facilityMap;

    MemoryInfo memPublicHeap;
    MemoryInfo memPrivate;
    MemoryInfo memReserved;
    MemoryInfo memTotal;

    std::list<sharemind::StackFrame> frames;
    sharemind::StackFrame * globalFrame;
    sharemind::StackFrame * nextFrame;
    sharemind::StackFrame * thisFrame;

    SharemindVmProcessState state;
};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE_DEFINE(
        SharemindProcess,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindProcess)

uint64_t SharemindProcess_public_alloc(SharemindProcess * const p,
                                       uint64_t const nBytes)
        __attribute__ ((visibility("internal"),
                        nonnull(1),
                        warn_unused_result));
SharemindVmProcessException SharemindProcess_public_free(
        SharemindProcess * const p,
        uint64_t const ptr)
        __attribute__ ((visibility("internal"),
                        nonnull(1),
                        warn_unused_result));

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PROCESS_H */
