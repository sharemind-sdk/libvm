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

#ifndef SHAREMIND_LIBVM_REFERENCES_H
#define SHAREMIND_LIBVM_REFERENCES_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cstddef>
#include <sharemind/module-apis/api_0x1.h>
#include <vector>
#include "MemorySlot.h"


namespace sharemind {
namespace Detail {

template <typename Base, typename DataType>
struct __attribute__((visibility("internal"))) ReferenceBase: Base {

    ReferenceBase(ReferenceBase && move) noexcept;
    ReferenceBase(ReferenceBase const &) = delete;

    ReferenceBase(void * const internal,
                  DataType * const data,
                  std::size_t const size);

    ~ReferenceBase() noexcept;

    ReferenceBase & operator=(ReferenceBase && move) noexcept;
    ReferenceBase & operator=(ReferenceBase const &) = delete;

};

extern template struct ReferenceBase<::SharemindModuleApi0x1Reference, void>;
using Reference = ReferenceBase<::SharemindModuleApi0x1Reference, void>;

extern template struct ReferenceBase<::SharemindModuleApi0x1CReference,
                                     void const>;
using CReference = ReferenceBase<::SharemindModuleApi0x1CReference, void const>;

using ReferenceVector = std::vector<Reference>;
using CReferenceVector = std::vector<CReference>;

} /* namespace Detail { */
} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_REFERENCES_H */
