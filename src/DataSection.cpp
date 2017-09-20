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


namespace sharemind {

DataSection::~DataSection() noexcept {}
bool DataSection::ref() noexcept { return true; }
void DataSection::deref() noexcept {}
bool DataSection::canFree() const noexcept { return false; }
void * DataSection::data() const noexcept { return m_data; }
std::size_t DataSection::size() const noexcept { return m_size; }


BssDataSection::BssDataSection(std::size_t const size)
    : DataSection(::operator new(size), size)
{ std::memset(data(), 0, size); }

BssDataSection::~BssDataSection() noexcept { ::operator delete(data()); }


RegularDataSection::~RegularDataSection() noexcept
{ ::operator delete(data()); }

RegularDataSection::RegularDataSection(void const * const data,
                                       std::size_t const size)
    : DataSection(::operator new(size), size)
{ std::memcpy(this->data(), data, size); }


RwDataSection::RwDataSection(void const * const data, std::size_t const size)
    : RegularDataSection(data, size)
{}

RwDataSection::~RwDataSection() noexcept {}


RoDataSection::RoDataSection(void const * const data, std::size_t const size)
    : RegularDataSection(data, size)
{}

RoDataSection::~RoDataSection() noexcept {}

bool RoDataSection::isWritable() const noexcept { return false; }

} /* namespace sharemind { */
