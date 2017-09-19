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

#ifndef SHAREMIND_LIBVM_DATASECTION_H
#define SHAREMIND_LIBVM_DATASECTION_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cstring>
#include <sharemind/AssertReturn.h>
#include <sharemind/likely.h>
#include <utility>
#include "memoryslot.h"


namespace sharemind {

struct DataSection: SharemindMemorySlot {

/* Types: */

    enum ZeroInitT { ZeroInit };
    enum Permissions { Read, ReadWrite };

/* Methods: */

    DataSection(DataSection && move) noexcept
        : SharemindMemorySlot(std::move(move))
    {
        move.size = 0u;
        move.pData = nullptr;
    }

    DataSection(DataSection const &) = delete;

    DataSection(std::size_t const size,
                Permissions const perms)
        : SharemindMemorySlot{
            size ? ::operator new(size) : nullptr,
            size,
            1u,
            assertReturn(specials_(perms))}
    {}

    DataSection(std::size_t const size,
                Permissions const perms,
                ZeroInitT const)
        : DataSection(size, perms)
    { std::memset(this->pData, 0, size); }

    DataSection(void const * const data,
                std::size_t const size,
                Permissions const perms)
        : DataSection(size, perms)
    { std::memcpy(this->pData, data, size); }

    ~DataSection() noexcept { ::operator delete(this->pData); }

    DataSection & operator=(DataSection && move) noexcept {
        ::operator delete(this->pData);
        static_cast<SharemindMemorySlot &>(*this) = move;
        move.pData = nullptr;
        move.size = 0u;
        return *this;
    }

    DataSection & operator=(DataSection const &) = delete;

    SharemindMemorySlotSpecials * specials_(Permissions const perms) {
        static SharemindMemorySlotSpecials roDataSpecials{nullptr, true, false};
        static SharemindMemorySlotSpecials rwDataSpecials{nullptr, true, true};
        if (perms == Read)
            return &roDataSpecials;
        assert(perms == ReadWrite);
        return &rwDataSpecials;
    }

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_DATASECTION_H */

