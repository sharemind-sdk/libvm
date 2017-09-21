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

SHAREMIND_DEFINE_EXCEPTION_NOINLINE(std::exception,
                                    ProcessFacilityMap::,
                                    Exception);
SHAREMIND_DEFINE_EXCEPTION_CONST_MSG_NOINLINE(
        Exception,
        ProcessFacilityMap::,
        FacilityNameClashException,
        "Facility with this name already exists!");

void ProcessFacilityMap::setFacility(std::string name,
                                     ProcessFacility facility)
{
    auto const rp(m_inner.emplace(std::move(name), std::move(facility)));
    if (!rp.second)
        throw FacilityNameClashException();
}

ProcessFacility ProcessFacilityMap::facility(std::string const & name)
        const noexcept
{
    auto const it(m_inner.find(name));
    if (it != m_inner.end())
        return it->second;
    if (m_nextGetter)
        return m_nextGetter(name.c_str());
    return nullptr;
}

bool unsetFacility(std::string const & name) noexcept
{ return m_inner.erase(name); }
