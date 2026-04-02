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

#include "PublicMemory.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <new>


namespace sharemind {
namespace Detail {

// SecreC code that creates arrays of zero size is considered valid.
// This code here allows allocations of 0 bytes to not fail.
// One byte will be allocated for data and the size will still be accurately reported as 0.

// the old hack that this behaviour replaces was:
//   always allocate one more byte (responsible was: SecreC compiler) to ensure that ALLOC with size zero works.
//   PublicMemory::size() -> useful_size + 1.

PublicMemory::PublicMemory(std::size_t const size) noexcept
    : m_data(size == 0u ? ::operator new(1) : ::operator new(size))
    , m_size(size)
{
    std::memset(m_data, 0, size == 0u ? 1 : size);
}

PublicMemory::~PublicMemory() noexcept { ::operator delete(m_data); }

void * PublicMemory::data() const noexcept { return m_data; }
std::size_t PublicMemory::size() const noexcept { return m_size; }

} // namespace Detail {
} // namespace sharemind {
