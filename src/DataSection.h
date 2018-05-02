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

#include "MemorySlot.h"

#include <cstddef>
#include <memory>
#include <sharemind/GlobalDeleter.h>
#include <sharemind/libexecutable/Executable.h>


namespace sharemind {
namespace Detail {

class __attribute__((visibility("internal"))) BssDataSection
    : public MemorySlot
{

public: /* Methods: */

    BssDataSection(BssDataSection &&) noexcept;
    BssDataSection(BssDataSection const &);

    BssDataSection(std::size_t const size);

    ~BssDataSection() noexcept override;

    BssDataSection & operator=(BssDataSection &&) noexcept;
    BssDataSection & operator=(BssDataSection const &);

    void * data() const noexcept final override;
    std::size_t size() const noexcept final override;

private: /* Fields: */

    std::unique_ptr<void, GlobalDeleter> m_data;
    std::size_t m_size;

};

class __attribute__((visibility("internal"))) RwDataSection
    : private Executable::DataSection
    , public MemorySlot
{

public: /* Methods: */

    RwDataSection(RwDataSection &&) noexcept;
    RwDataSection(RwDataSection const &);

    RwDataSection(Executable::DataSection && dataSection);

    ~RwDataSection() noexcept override;

    RwDataSection & operator=(RwDataSection &&) noexcept;
    RwDataSection & operator=(RwDataSection const &);

    void * data() const noexcept final override;
    std::size_t size() const noexcept final override;

};

class __attribute__((visibility("internal"))) RoDataSection
    : private Executable::DataSection
    , public MemorySlot
{

public: /* Methods: */

    RoDataSection(RoDataSection &&) noexcept;
    RoDataSection(RoDataSection const &);

    RoDataSection(Executable::DataSection && dataSection);

    ~RoDataSection() noexcept override;

    RoDataSection & operator=(RoDataSection &&) noexcept;
    RoDataSection & operator=(RoDataSection const &);

    void * data() const noexcept final override;
    std::size_t size() const noexcept final override;
    bool isWritable() const noexcept final override;

};

} /* namespace Detail { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_DATASECTION_H */

