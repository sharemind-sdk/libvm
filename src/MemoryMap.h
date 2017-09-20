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

#ifndef SHAREMIND_LIBVM_MEMORYMAP_H
#define SHAREMIND_LIBVM_MEMORYMAP_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <cassert>
#include <limits>
#include <memory>
#include <sharemind/likely.h>
#include <unordered_map>
#include "DataSection.h"
#include "libvm.h"
#include "MemorySlot.h"


namespace sharemind {


class MemoryMap {

public: /* Types: */

    using KeyType = std::uint64_t;
    using ValueType = std::shared_ptr<MemorySlot>;

private: /* Constants: */

    constexpr static KeyType numDataSections = 3u;
    constexpr static KeyType numReservedPointers = numDataSections + 1u;

public: /* Methods: */

    ValueType const & get(KeyType const ptr) const noexcept {
        static ValueType const notFound;
        auto const it(m_inner.find(ptr));
        return (it != m_inner.end()) ? it->second : notFound;
    }

    void insertDataSection(KeyType ptr,
                           std::shared_ptr<DataSection> section)
    {
        assert(ptr);
        assert(ptr < numReservedPointers);
        assert(m_inner.find(ptr) == m_inner.end());
        m_inner.emplace(std::move(ptr), std::move(section));
    }

    KeyType allocate(std::size_t const size) {
        /* All reserved pointers must have been allocated beforehand: */
        #ifndef NDEBUG
        for (KeyType i = 1u; i < numReservedPointers; ++i)
            assert(m_inner.find(i) != m_inner.end());
        #endif

        auto const ptr(findUnusedPtr());
        m_inner.emplace(ptr, std::make_shared<PublicMemory>(size));
        return ptr;
    }

    std::pair<SharemindVmProcessException, std::size_t> free(KeyType const ptr)
    {
        using R = std::pair<SharemindVmProcessException, std::size_t>;
        if (ptr < numReservedPointers)
            return R(ptr
                     ? SHAREMIND_VM_PROCESS_OK
                     : SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE,
                     0u);
        auto const it(m_inner.find(ptr));
        if (it == m_inner.end())
            return R(SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE, 0u);
        if (!it->second->canFree())
            return R(SHAREMIND_VM_PROCESS_MEMORY_IN_USE, 0u);
        R r(SHAREMIND_VM_PROCESS_OK, it->second->size());
        m_inner.erase(it);
        return r;
    }

private: /* Methods: */

    KeyType findUnusedPtr() const noexcept {
        assert(m_nextTryPtr >= numReservedPointers);
        assert(m_inner.size() < std::numeric_limits<std::size_t>::max());
        assert(m_inner.size() < std::numeric_limits<KeyType>::max());
        auto index = m_nextTryPtr;
        for (;;) {
            /* Check if slot is free: */
            if (likely(m_inner.find(index) == m_inner.end()))
                break;
            /* Increment index, skip "NULL" and static memory pointers: */
            if (unlikely(!++index))
                index = numReservedPointers;
            /* This shouldn't trigger because (m->size < UINT64_MAX) */
            assert(index != m_nextTryPtr);
        }
        assert(index != 0u);
        m_nextTryPtr = index;
        return index;
    }

private: /* Fields: */

    std::unordered_map<KeyType, ValueType> m_inner;
    mutable KeyType m_nextTryPtr = numReservedPointers;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_MEMORYMAP_H */
