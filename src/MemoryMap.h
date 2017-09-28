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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>


namespace sharemind {
namespace Detail {

class DataSection;
class MemorySlot;

class __attribute__((visibility("internal"))) MemoryMap {

public: /* Types: */

    using KeyType = std::uint64_t;
    using ValueType = std::shared_ptr<MemorySlot>;

    enum ErrorCode { Ok, MemorySlotInUse, InvalidMemoryHandle };

private: /* Constants: */

    constexpr static KeyType numDataSections = 3u;
    constexpr static KeyType numReservedPointers = numDataSections + 1u;

public: /* Methods: */

    ValueType const & get(KeyType const ptr) const noexcept;

    void insertDataSection(KeyType ptr, std::shared_ptr<DataSection> section);

    KeyType allocate(std::size_t const size);

    std::pair<ErrorCode, std::size_t> free(KeyType const ptr);

    std::size_t slotSize(KeyType const ptr) const noexcept;
    void * slotPtr(KeyType const ptr) const noexcept;

private: /* Methods: */

    KeyType findUnusedPtr() const noexcept;

private: /* Fields: */

    ValueType const m_notFound;
    std::unordered_map<KeyType, ValueType> m_inner;
    mutable KeyType m_nextTryPtr = numReservedPointers;

};

} /* namespace Detail { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_MEMORYMAP_H */
