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

#include "Vm.h"
#include "Vm_p.h"

#include <cassert>
#include <cstdlib>
#include <sharemind/AssertReturn.h>
#include <utility>


namespace sharemind {

#define GUARD_(g) std::lock_guard<decltype(g)> const guard(g)
#define INNERGUARD GUARD_(m_mutex)
#define GUARD GUARD_(m_inner->m_mutex)

Vm::Inner::Inner() {}

Vm::Inner::~Inner() noexcept {}

SharemindSyscallWrapper Vm::Inner::findSyscall(std::string const & signature)
        const noexcept
{
    INNERGUARD;
    if (m_syscallFinder && *m_syscallFinder)
        return (*m_syscallFinder)(signature);
    return SharemindSyscallWrapper{nullptr, nullptr};
}

SharemindPd * Vm::Inner::findPd(std::string const & pdName) const noexcept {
    INNERGUARD;
    if (m_pdFinder && *m_pdFinder)
        return (*m_pdFinder)(pdName);
    return nullptr;
}


Vm::Vm() : m_inner(std::make_shared<Inner>()) {}

Vm::~Vm() noexcept {}

void Vm::setSyscallFinder(SyscallFinderFunPtr f) noexcept {
    GUARD;
    m_inner->m_syscallFinder = std::move(f);
}

void Vm::setPdFinder(PdFinderFunPtr f) noexcept {
    GUARD;
    m_inner->m_pdFinder = std::move(f);
}

void Vm::setProcessFacilityFinder(ProcessFacilityFinderFunPtr f) noexcept {
    GUARD;
    m_inner->m_processFacilityMap->setNextGetter(std::move(f));
}

SharemindSyscallWrapper Vm::findSyscall(std::string const & signature)
        const noexcept
{ return m_inner->findSyscall(signature); }

SharemindPd * Vm::findPd(std::string const & pdName) const noexcept
{ return m_inner->findPd(pdName); }

SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(Vm,m_inner->)

} // namespace sharemind {
