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

#ifndef SHAREMIND_LIBVM_MEMORYSLOT_H
#define SHAREMIND_LIBVM_MEMORYSLOT_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>


namespace sharemind {

class MemorySlot {

public: /* Methods: */

    MemorySlot() noexcept {}
    MemorySlot(MemorySlot &&) = delete;
    MemorySlot(MemorySlot const &) = delete;
    MemorySlot & operator=(MemorySlot &&) = delete;
    MemorySlot & operator=(MemorySlot const &) = delete;

    virtual ~MemorySlot() noexcept {}

    virtual void * data() const noexcept = 0;
    virtual std::size_t size() const noexcept = 0;
    virtual bool ref() noexcept = 0;
    virtual void deref() noexcept = 0;
    virtual bool canFree() const noexcept = 0;
    virtual bool isWritable() const noexcept { return true; }

};

class PublicMemory: public MemorySlot {

public: /* Methods: */

    PublicMemory(std::size_t const size) noexcept
        : m_data(::operator new(size))
        , m_size(size)
    { std::memset(m_data, 0, size); }

    ~PublicMemory() noexcept override { ::operator delete(m_data); }

    bool ref() noexcept final override {
        if (m_nrefs == std::numeric_limits<decltype(m_nrefs)>::max())
            return false;
        ++m_nrefs;
        return true;
    }

    void deref() noexcept final override {
        assert(m_nrefs);
        --m_nrefs;
    }

    bool canFree() const noexcept final override { return m_nrefs == 0u; }
    void * data() const noexcept final override { return m_data; }
    std::size_t size() const noexcept final override { return m_size; }

private: /* Fields: */

    void * const m_data;
    std::size_t const m_size;
    std::size_t m_nrefs = 0u;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_MEMORYSLOT_H */
