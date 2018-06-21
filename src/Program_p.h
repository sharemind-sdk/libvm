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
#include <vector>
#include <type_traits>
#include "CodeSection.h"
#include "DataSection.h"
#include "Program.h"
#include "Vm.h"
#include "Vm_p.h"


namespace sharemind {
namespace Detail {

struct __attribute__((visibility("internal"))) PreparedSyscallBindings
    : std::vector<std::shared_ptr<Vm::SyscallWrapper> >
{

/* Methods: */

    PreparedSyscallBindings() = delete;
    PreparedSyscallBindings(PreparedSyscallBindings &&)
            noexcept(std::is_nothrow_move_constructible<
                            std::vector<std::shared_ptr<Vm::SyscallWrapper> >
                        >::value);
    PreparedSyscallBindings(PreparedSyscallBindings const &);

    template <typename SyscallFinder>
    PreparedSyscallBindings(
            std::shared_ptr<Executable::SyscallBindingsSection> parsedBindings,
            SyscallFinder && syscallFinder);

    PreparedSyscallBindings & operator=(PreparedSyscallBindings &&)
            noexcept(std::is_nothrow_move_assignable<
                            std::vector<std::shared_ptr<Vm::SyscallWrapper> >
                        >::value);
    PreparedSyscallBindings & operator=(PreparedSyscallBindings const &);

};

struct __attribute__((visibility("internal"))) PreparedLinkingUnit {

/* Methods: */

    template <typename SyscallFinder>
    PreparedLinkingUnit(Executable::LinkingUnit && parsedLinkingUnit,
                        SyscallFinder && syscallFinder);

    PreparedLinkingUnit(PreparedLinkingUnit &&)
            noexcept(std::is_nothrow_move_constructible<CodeSection>::value
                     && std::is_nothrow_move_constructible<RoDataSection>::value
                     && std::is_nothrow_move_constructible<RwDataSection>::value
                     && std::is_nothrow_move_constructible<
                                PreparedSyscallBindings>::value);
    PreparedLinkingUnit(PreparedLinkingUnit const &) = delete;

    PreparedLinkingUnit & operator=(PreparedLinkingUnit &&)
            noexcept(std::is_nothrow_move_assignable<CodeSection>::value
                     && std::is_nothrow_move_assignable<RoDataSection>::value
                     && std::is_nothrow_move_assignable<RwDataSection>::value
                     && std::is_nothrow_move_assignable<
                                PreparedSyscallBindings>::value);
    PreparedLinkingUnit & operator=(PreparedLinkingUnit const &) = delete;

/* Fields: */

    CodeSection codeSection;
    RoDataSection roDataSection;
    RwDataSection rwDataSection;
    std::size_t bssSectionSize;
    PreparedSyscallBindings syscallBindings;

};

struct __attribute__((visibility("internal"))) PreparedExecutable {

/* Methods: */

    PreparedExecutable() = delete;
    PreparedExecutable(PreparedExecutable &&)
            noexcept(std::is_nothrow_move_constructible<
                            std::vector<PreparedLinkingUnit> >::value);
    PreparedExecutable(PreparedExecutable const &) = delete;

    template <typename SyscallFinder>
    PreparedExecutable(Executable parsedExecutable,
                       SyscallFinder && syscallFinder);

    PreparedExecutable & operator=(PreparedExecutable &&)
            noexcept(std::is_nothrow_move_assignable<
                            std::vector<PreparedLinkingUnit> >::value);
    PreparedExecutable & operator=(PreparedExecutable const &) = delete;

/* Fields: */

    std::vector<PreparedLinkingUnit> linkingUnits;
    std::size_t activeLinkingUnitIndex;

};

struct __attribute__((visibility("internal"))) ProgramState {

/* Methods: */

    ProgramState(
                std::shared_ptr<VmState> vmState,
                std::shared_ptr<Detail::PreparedExecutable> preparedExecutable);
    ~ProgramState() noexcept;

    std::shared_ptr<void> findProcessFacility(char const * name) const noexcept;

/* Fields: */

    mutable std::mutex m_mutex;
    Vm::FacilityFinderFunPtr m_processFacilityFinder;
    std::shared_ptr<VmState> m_vmState;

    std::shared_ptr<Detail::PreparedExecutable const> const
            m_preparedExecutable;

}; /* struct ProgramState */

} /* namespace Detail { */


struct __attribute__((visibility("internal"))) Program::Inner final
    : Detail::ProgramState
{

/* Methods: */

    Inner(std::shared_ptr<Vm::Inner> programInner,
          std::shared_ptr<Detail::PreparedExecutable> preparedExecutable);
    ~Inner() noexcept;

    static std::shared_ptr<Detail::PreparedExecutable> loadFromFile(
                Vm::Inner const & vmInner,
                char const * const filename);

    static std::shared_ptr<Detail::PreparedExecutable> loadFromCFile(
                Vm::Inner const & vmInner,
                FILE * const file);

    static std::shared_ptr<Detail::PreparedExecutable> loadFromFileDescriptor(
                Vm::Inner const & vmInner,
                int const fd);

    static std::shared_ptr<Detail::PreparedExecutable> loadFromMemory(
                Vm::Inner const & vmInner,
                void const * data,
                std::size_t dataSize);

    static std::shared_ptr<Detail::PreparedExecutable> loadFromStream(
                Vm::Inner const & vmInner,
                std::istream & inputStream);

    static std::shared_ptr<Detail::PreparedExecutable> loadFromExecutable(
                Vm::Inner const & vmInner,
                Executable && executable);

}; /* struct Program::Inner */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROGRAM_P_H */
