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

#ifndef SHAREMIND_LIBVM_VM_P_H
#define SHAREMIND_LIBVM_VM_P_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include "Vm.h"

#include <memory>
#include <mutex>
#include <sharemind/libmodapi/libmodapi.h>
#include <string>
#include "ProcessFacilityMap.h"


namespace sharemind {

struct __attribute__((visibility("internal"))) Vm::Inner {

/* Methods: */

    Inner();
    ~Inner() noexcept;

    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS

    std::shared_ptr<SyscallWrapper> findSyscall(std::string const & signature)
            const noexcept;

    SharemindPd * findPd(std::string const & pdName) const noexcept;

/* Fields: */

    mutable std::recursive_mutex m_mutex;
    SyscallFinderFunPtr m_syscallFinder;
    PdFinderFunPtr m_pdFinder;
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_VM_P_H */
