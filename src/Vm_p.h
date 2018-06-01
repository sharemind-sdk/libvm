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
#include <string>


namespace sharemind {
namespace Detail {

class __attribute__((visibility("internal"))) VmState {

    friend class sharemind::Vm;

public: /* Methods: */

    ~VmState() noexcept;

    std::shared_ptr<Vm::SyscallWrapper> findSyscall(
                std::string const & signature) const noexcept;

    std::shared_ptr<void> findProcessFacility(char const * name) const noexcept;

private: /* Fields: */

    mutable std::recursive_mutex m_mutex;
    Vm::SyscallFinderFunPtr m_syscallFinder;
    Vm::FacilityFinderFunPtr m_processFacilityFinder;

}; /* struct VmState */

} /* namespace Detail { */

struct __attribute__((visibility("internal"))) Vm::Inner: Detail::VmState {};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_VM_P_H */
