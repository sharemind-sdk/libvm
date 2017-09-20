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
#include <utility>
#include "memoryslot.h"


namespace sharemind {

class DataSection: public MemorySlot {

public: /* Methods: */

    bool ref() noexcept final override { return true; }
    void deref() noexcept final override {}
    bool canFree() const noexcept final override { return false; }

    void * data() const noexcept final override { return m_data; }
    std::size_t size() const noexcept final override { return m_size; }

protected: /* Methods: */

    DataSection(void * const data, std::size_t const size) noexcept
        : m_data(data)
        , m_size(size)
    {}

private: /* Fields: */

    void * const m_data;
    std::size_t const m_size;

};

class BssDataSection: public DataSection {

public: /* Methods: */

    BssDataSection(std::size_t const size)
        : DataSection(::operator new(size), size)
    { std::memset(data(), 0, size); }

    ~BssDataSection() noexcept override { ::operator delete(data()); }

};

class RegularDataSection: public DataSection {

public: /* Methods: */

    ~RegularDataSection() noexcept override { ::operator delete(data()); }

protected: /* Methods: */

    RegularDataSection(void const * const data, std::size_t const size)
        : DataSection(::operator new(size), size)
    { std::memcpy(this->data(), data, size); }

};

class RwDataSection: public RegularDataSection {

public: /* Methods: */

    RwDataSection(void const * const data, std::size_t const size)
        : RegularDataSection(data, size)
    {}

};

class RoDataSection: public RegularDataSection {

public: /* Methods: */

    RoDataSection(void const * const data, std::size_t const size)
        : RegularDataSection(data, size)
    {}

    bool isWritable() const noexcept final override { return false; }

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_DATASECTION_H */

