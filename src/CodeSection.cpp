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

#include "CodeSection.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <new>
#include <sharemind/DebugOnly.h>


namespace sharemind {
namespace Detail {

CodeSection::CodeSection(std::vector<SharemindCodeBlock> data)
    : m_data(std::move(data))
{
    auto numInstructions = m_data.size();
    m_instrmap.resize(numInstructions, false);
    SharemindCodeBlock terminator;
    terminator.uint64[0u] = 0u;
    m_data.emplace_back(std::move(terminator));
}

CodeSection::~CodeSection() noexcept {}

bool CodeSection::isInstructionAtOffset(std::size_t const offset) const noexcept
{ return (offset < m_instrmap.size()) && m_instrmap[offset]; }

void CodeSection::registerInstruction(
        std::size_t const offset,
        std::size_t const instructionBlockIndex,
        VmInstructionInfo const & description)
{
    assert(!m_instrmap[offset]);
    m_instrmap[offset] = true;
    SHAREMIND_DEBUG_ONLY(auto const r =)
            m_blockmap.emplace(instructionBlockIndex, description);
    assert(r.second);
}

VmInstructionInfo const * CodeSection::instructionDescriptionAtOffset(
        std::size_t const offset) const noexcept
{
    auto const it(m_blockmap.find(offset));
    return (it != m_blockmap.end()) ? &it->second : nullptr;
}

} // namespace Detail {
} // namespace sharemind {
