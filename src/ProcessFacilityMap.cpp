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

#include "ProcessFacilityMap.h"


namespace sharemind {
namespace Detail {

SHAREMIND_DEFINE_EXCEPTION_NOINLINE(sharemind::Exception,
                                    ProcessFacilityMap::,
                                    Exception);
SHAREMIND_DEFINE_EXCEPTION_CONST_MSG_NOINLINE(
        Exception,
        ProcessFacilityMap::,
        FacilityNameClashException,
        "Facility with this name already exists!");

#define GUARD std::lock_guard<decltype(m_mutex)> const guard(m_mutex)

void ProcessFacilityMap::setFacility(std::string name, void * facility) {
    GUARD;
    auto const rp(m_inner.emplace(std::move(name), std::move(facility)));
    if (!rp.second)
        throw FacilityNameClashException();
}

void ProcessFacilityMap::setNextGetter(NextGetterFunPtr nextGetter) noexcept {
    GUARD;
    m_nextGetter = std::move(nextGetter);
}

void * ProcessFacilityMap::facility(std::string const & name) const noexcept {
    decltype(m_nextGetter) nextGetter;
    {
        GUARD;
        auto const it(m_inner.find(name));
        if (it != m_inner.end())
            return it->second;
        if (!m_nextGetter)
            return nullptr;
        nextGetter = m_nextGetter;
    }
    return (*nextGetter)(name.c_str());
}

void * ProcessFacilityMap::facility(char const * const name) const noexcept {
    decltype(m_nextGetter) nextGetter;
    {
        GUARD;
        auto const it(m_inner.find(name));
        if (it != m_inner.end())
            return it->second;
        if (!m_nextGetter)
            return nullptr;
        nextGetter = m_nextGetter;
    }
    return (*nextGetter)(name);
}

bool ProcessFacilityMap::unsetFacility(std::string const & name) noexcept {
    GUARD;
    return m_inner.erase(name);
}

} // namespace Detail {
} // namespace sharemind {
