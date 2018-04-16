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

#include "MemoryMap.h"

#include <cassert>
#include <limits>
#include <sharemind/likely.h>
#include "DataSection.h"
#include "MemorySlot.h"
#include "PublicMemory.h"


namespace sharemind {
namespace Detail {

MemoryMap::ValueType const & MemoryMap::get(KeyType const ptr) const noexcept {
    auto const it(m_inner.find(ptr));
    return (it != m_inner.end()) ? it->second : m_notFound;
}

void MemoryMap::insertDataSection(KeyType ptr,
                                  std::shared_ptr<DataSection> section)
{
    assert(ptr);
    assert(ptr < numReservedPointers);
    assert(m_inner.find(ptr) == m_inner.end());
    if (!section)
        return;
    m_inner.emplace(std::move(ptr), std::move(section));
}

MemoryMap::KeyType MemoryMap::allocate(std::size_t const size) {
    /* All reserved pointers must have been allocated beforehand: */
    #ifndef NDEBUG
    for (KeyType i = 1u; i < numReservedPointers; ++i)
        assert(m_inner.find(i) != m_inner.end());
    #endif

    auto const ptr(findUnusedPtr());
    m_inner.emplace(ptr, std::make_shared<PublicMemory>(size));
    return ptr;
}

std::pair<MemoryMap::ErrorCode, std::size_t> MemoryMap::free(KeyType const ptr)
{
    using R = std::pair<ErrorCode, std::size_t>;
    if (ptr < numReservedPointers)
        return R(ptr ? Ok : InvalidMemoryHandle, 0u);
    auto const it(m_inner.find(ptr));
    if (it == m_inner.end())
        return R(InvalidMemoryHandle, 0u);
    R r(Ok, it->second->size());
    m_inner.erase(it);
    return r;
}

std::size_t MemoryMap::slotSize(KeyType const ptr) const noexcept {
    auto const it(m_inner.find(ptr));
    return (it == m_inner.end()) ? 0u : it->second->size();
}

void * MemoryMap::slotPtr(KeyType const ptr) const noexcept {
    auto const it(m_inner.find(ptr));
    return (it == m_inner.end()) ? nullptr : it->second->data();
}

MemoryMap::KeyType MemoryMap::findUnusedPtr() const noexcept {
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

} // namespace Detail {
} // namespace sharemind {
