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

#ifndef SHAREMIND_LIBVM_PROGRAM_P_H
#define SHAREMIND_LIBVM_PROGRAM_P_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <mutex>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <vector>
#include "CodeSection.h"
#include "DataSectionsVector.h"
#include "ProcessFacilityMap.h"
#include "Program.h"
#include "Vm.h"


namespace sharemind {
namespace Detail {

using DataSectionSizesVector =
        std::vector<decltype(SharemindExecutableSectionHeader0x0::length)>;
using CodeSectionsVector = std::vector<CodeSection>;
using SyscallBindingsVector = std::vector<SharemindSyscallWrapper>;
using PdBindingsVector = std::vector<SharemindPd *>;
struct __attribute__((visibility("internal"))) StaticData {
    CodeSectionsVector codeSections;
    DataSectionsVector rodataSections;
    SyscallBindingsVector syscallBindings;
    PdBindingsVector pdBindings;
    std::size_t activeLinkingUnit;
};
struct __attribute__((visibility("internal"))) DynamicSections {
    DataSectionsVector rwdata;
    DataSectionsVector bss;
};
struct __attribute__((visibility("internal"))) ParseData {
    std::shared_ptr<StaticData> staticData{std::make_shared<StaticData>()};
    DataSectionsVector dataSections;
    DataSectionSizesVector bssSectionSizes;
};

} /* namespace Detail { */

struct __attribute__((visibility("internal"))) Program::Inner {

/* Methods: */

    Inner(std::shared_ptr<Vm::Inner> vmInner);
    ~Inner() noexcept;

/* Fields: */

    SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS;
    std::shared_ptr<Vm::Inner> m_vmInner;

    mutable std::mutex m_mutex;
    std::shared_ptr<Detail::ParseData> m_parseData;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROGRAM_P_H */
