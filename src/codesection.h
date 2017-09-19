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

#ifndef SHAREMIND_LIBVM_CODESECTION_H
#define SHAREMIND_LIBVM_CODESECTION_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <new>
#include <sharemind/codeblock.h>
#include <sharemind/DebugOnly.h>
#include <sharemind/libvmi/instr.h>
#include <unordered_map>
#include <vector>


namespace sharemind {

class CodeSection {

public: /* Methods: */

    CodeSection(SharemindCodeBlock const * const code,
                std::size_t const codeSize)
    {
        if (codeSize == std::numeric_limits<std::size_t>::max())
            throw std::bad_alloc();
        m_instrmap.resize(codeSize, false);
        m_data.resize(codeSize + 1u);
        std::copy(code, code + codeSize, m_data.data());
    }

    bool isInstructionAtOffset(std::size_t const offset) const noexcept
    { return (offset < m_instrmap.size()) && m_instrmap[offset]; }

    void registerInstruction(std::size_t const offset,
                             std::size_t const instructionBlockIndex,
                             SharemindVmInstruction const * const description)
    {
        assert(!m_instrmap[offset]);
        m_instrmap[offset] = true;
        SHAREMIND_DEBUG_ONLY(auto const r =)
                m_blockmap.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(instructionBlockIndex),
                    std::forward_as_tuple(description));
        assert(r.second);
    }

    SharemindVmInstruction const * instructionDescriptionAtOffset(
            std::size_t const offset) const noexcept
    {
        auto const it(m_blockmap.find(offset));
        return (it != m_blockmap.end()) ? it->second : nullptr;
    }

    SharemindCodeBlock * data() noexcept { return m_data.data(); }

    SharemindCodeBlock const * constData() const noexcept
    { return m_data.data(); }

    std::size_t size() const noexcept { return m_data.size() - 1u; }

private: /* Fields: */

    std::vector<SharemindCodeBlock> m_data;
    std::vector<bool> m_instrmap;
    std::unordered_map<std::size_t, SharemindVmInstruction const *> m_blockmap;

};

} /* namespace sharemind */

#endif /* SHAREMIND_LIBVM_CODESECTION_H */
