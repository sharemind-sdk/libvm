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

#include "References.h"

#include <utility>


namespace sharemind {

template <typename Base, typename DataType>
ReferenceBase<Base, DataType>::ReferenceBase(ReferenceBase && move) noexcept
    : Base(std::move(move))
{
    move.pData = nullptr;
    move.size = 0u;
    move.internal = nullptr;
}

template <typename Base, typename DataType>
ReferenceBase<Base, DataType>::ReferenceBase(void * const internal,
                                             DataType * const data,
                                             std::size_t const size)
    : Base{internal, data, size}
{}

template <typename Base, typename DataType>
ReferenceBase<Base, DataType> & ReferenceBase<Base, DataType>::operator=(
        ReferenceBase && move) noexcept
{
    if (auto * const s = this->internal)
        static_cast<sharemind::MemorySlot *>(s)->deref();
    this->internal = move.internal;
    this->pData = move.pData;
    this->size = move.size;
    move.pData = nullptr;
    move.size = 0u;
    move.internal = nullptr;
    return *this;
}

template <typename Base, typename DataType>
ReferenceBase<Base, DataType>::~ReferenceBase() noexcept {
    if (auto * const s = this->internal)
        static_cast<sharemind::MemorySlot *>(s)->deref();
}

template struct ReferenceBase<::SharemindModuleApi0x1Reference, void>;
template struct ReferenceBase<::SharemindModuleApi0x1CReference, void const>;

} /* namespace sharemind { */
