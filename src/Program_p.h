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
#include <sharemind/libexecutable/Executable.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <vector>
#include "CodeSection.h"
#include "DataSection.h"
#include "ProcessFacilityMap.h"
#include "Program.h"
#include "Vm.h"


namespace sharemind {
namespace Detail {

struct __attribute__((visibility("internal"))) PreparedSyscallBindings
    : std::vector<std::shared_ptr<Vm::SyscallWrapper> >
{

/* Methods: */

    PreparedSyscallBindings() = delete;
    PreparedSyscallBindings(PreparedSyscallBindings &&) noexcept;
    PreparedSyscallBindings(PreparedSyscallBindings const &);

    template <typename SyscallFinder>
    PreparedSyscallBindings(
            std::shared_ptr<Executable::SyscallBindingsSection> parsedBindings,
            SyscallFinder && syscallFinder);

    PreparedSyscallBindings & operator=(PreparedSyscallBindings &&) noexcept;
    PreparedSyscallBindings & operator=(PreparedSyscallBindings const &);

};

struct __attribute__((visibility("internal"))) PreparedPdBindings
    : std::vector<SharemindPd *>
{

/* Methods: */

    PreparedPdBindings() = delete;
    PreparedPdBindings(PreparedPdBindings &&) noexcept;
    PreparedPdBindings(PreparedPdBindings const &);

    template <typename PdFinder>
    PreparedPdBindings(
            std::shared_ptr<Executable::PdBindingsSection> parsedBindings,
            PdFinder && pdFinder);

    PreparedPdBindings & operator=(PreparedPdBindings &&) noexcept;
    PreparedPdBindings & operator=(PreparedPdBindings const &);

};

struct __attribute__((visibility("internal"))) PreparedLinkingUnit {

/* Methods: */

    template <typename SyscallFinder, typename PdFinder>
    PreparedLinkingUnit(Executable::LinkingUnit && parsedLinkingUnit,
                        SyscallFinder && syscallFinder,
                        PdFinder && pdFinder);

    PreparedLinkingUnit(PreparedLinkingUnit &&) noexcept;
    PreparedLinkingUnit(PreparedLinkingUnit const &) = delete;

    PreparedLinkingUnit & operator=(PreparedLinkingUnit &&) noexcept;
    PreparedLinkingUnit & operator=(PreparedLinkingUnit const &) = delete;

/* Fields: */

    CodeSection codeSection;
    RoDataSection roDataSection;
    RwDataSection rwDataSection;
    std::size_t bssSectionSize;
    PreparedSyscallBindings syscallBindings;
    PreparedPdBindings pdBindings;

};

struct __attribute__((visibility("internal"))) PreparedExecutable {

/* Methods: */

    PreparedExecutable() = delete;
    PreparedExecutable(PreparedExecutable &&) noexcept;
    PreparedExecutable(PreparedExecutable const &) noexcept = delete;

    template <typename SyscallFinder, typename PdFinder>
    PreparedExecutable(Executable parsedExecutable,
                       SyscallFinder && syscallFinder,
                       PdFinder && pdFinder);

    PreparedExecutable & operator=(PreparedExecutable &&) noexcept;
    PreparedExecutable & operator=(PreparedExecutable const &) noexcept =
            delete;

/* Fields: */

    std::vector<PreparedLinkingUnit> linkingUnits;
    std::size_t activeLinkingUnitIndex;

};

} /* namespace Detail { */

struct __attribute__((visibility("internal"))) Program::Inner {

/* Methods: */

    Inner(std::shared_ptr<Vm::Inner> vmInner);
    ~Inner() noexcept;

    void loadFromFile(char const * const filename);
    void loadFromCFile(FILE * const file);
    void loadFromFileDescriptor(int const fd);
    void loadFromMemory(void const * data, std::size_t dataSize);
    void loadFromStream(std::istream & inputStream);

/* Fields: */

    SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS;
    std::shared_ptr<Vm::Inner> m_vmInner;

    std::shared_ptr<Detail::PreparedExecutable> m_preparedExecutable;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROGRAM_P_H */
