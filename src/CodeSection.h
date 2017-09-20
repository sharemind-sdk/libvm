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

#include <cstddef>
#include <sharemind/codeblock.h>
#include <sharemind/libvmi/instr.h>
#include <unordered_map>
#include <vector>


namespace sharemind {

class CodeSection {

public: /* Methods: */

    CodeSection(CodeSection &&) = default;
    CodeSection(CodeSection const &) = delete;

    CodeSection(SharemindCodeBlock const * const code,
                std::size_t const codeSize);

    ~CodeSection() noexcept;

    CodeSection & operator=(CodeSection &&) = default;
    CodeSection & operator=(CodeSection const &) = delete;

    bool isInstructionAtOffset(std::size_t const offset) const noexcept;

    void registerInstruction(std::size_t const offset,
                             std::size_t const instructionBlockIndex,
                             SharemindVmInstruction const * const description);

    SharemindVmInstruction const * instructionDescriptionAtOffset(
            std::size_t const offset) const noexcept;

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
