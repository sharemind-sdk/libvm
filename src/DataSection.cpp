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

#include "DataSection.h"


#include <cstring>
#include <new>
#include <sharemind/AssertReturn.h>


namespace sharemind {
namespace Detail {

BssDataSection::BssDataSection(BssDataSection &&) noexcept = default;

BssDataSection::BssDataSection(BssDataSection const & copy)
    : MemorySlot()
    , m_data(::operator new(copy.m_size))
    , m_size(copy.m_size)
{ std::memcpy(m_data.get(), copy.m_data.get(), m_size); }

BssDataSection::BssDataSection(std::size_t const size)
    : m_data(::operator new(size))
    , m_size(size)
{ std::memset(data(), 0, size); }

BssDataSection::~BssDataSection() noexcept = default;

BssDataSection & BssDataSection::operator=(BssDataSection &&) noexcept =
        default;

BssDataSection & BssDataSection::operator=(BssDataSection const & copy) {
    decltype(m_data) newData(::operator new(copy.m_size));
    std::memcpy(newData.get(), copy.m_data.get(), m_size);
    m_data = std::move(newData);
    m_size = copy.m_size;
    return *this;
}

void * BssDataSection::data() const noexcept { return m_data.get(); }

std::size_t BssDataSection::size() const noexcept { return m_size; }



RwDataSection::RwDataSection(RwDataSection &&) noexcept = default;

RwDataSection::RwDataSection(RwDataSection const &) = default;

RwDataSection::RwDataSection(Executable::DataSection && dataSection)
    : Executable::DataSection(std::move(dataSection))
{}

RwDataSection::~RwDataSection() noexcept = default;

RwDataSection & RwDataSection::operator=(RwDataSection &&) noexcept = default;

RwDataSection & RwDataSection::operator=(RwDataSection const &) = default;

void * RwDataSection::data() const noexcept
{ return static_cast<Executable::DataSection const *>(this)->data.get(); }

std::size_t RwDataSection::size() const noexcept { return this->sizeInBytes; }



RoDataSection::RoDataSection(RoDataSection &&) noexcept = default;

RoDataSection::RoDataSection(RoDataSection const &) = default;

RoDataSection::RoDataSection(Executable::DataSection && dataSection)
    : Executable::DataSection(std::move(dataSection))
{}

RoDataSection::~RoDataSection() noexcept = default;

RoDataSection & RoDataSection::operator=(RoDataSection &&) noexcept = default;

RoDataSection & RoDataSection::operator=(RoDataSection const &) = default;

void * RoDataSection::data() const noexcept
{ return static_cast<Executable::DataSection const *>(this)->data.get(); }

std::size_t RoDataSection::size() const noexcept { return this->sizeInBytes; }

bool RoDataSection::isWritable() const noexcept { return false; }

} // namespace Detail {
} // namespace sharemind {
