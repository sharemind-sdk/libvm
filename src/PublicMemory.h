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

#ifndef SHAREMIND_LIBVM_PUBLICMEMORY_H
#define SHAREMIND_LIBVM_PUBLICMEMORY_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include "MemorySlot.h"


namespace sharemind {

class PublicMemory: public MemorySlot {

public: /* Methods: */

    PublicMemory(std::size_t const size) noexcept;
    ~PublicMemory() noexcept override;

    bool ref() noexcept final override;
    void deref() noexcept final override;
    bool canFree() const noexcept final override;
    void * data() const noexcept final override;
    std::size_t size() const noexcept final override;

private: /* Fields: */

    void * const m_data;
    std::size_t const m_size;
    std::size_t m_nrefs = 0u;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PUBLICMEMORY_H */