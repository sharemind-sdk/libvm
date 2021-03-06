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

#ifndef SHAREMIND_LIBVM_VM_H
#define SHAREMIND_LIBVM_VM_H

#include <functional>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sharemind/codeblock.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace sharemind {

class Program;

class Vm {

    friend class Program;

private: /* Types: */

    struct Inner;

public: /* Types: */

    struct Reference {

        /* Methods: */

        Reference() noexcept = default;
        Reference(Reference &&) noexcept = default;
        Reference(Reference const &) noexcept = default;

        Reference(std::shared_ptr<void> data_, std::size_t size_)
            : data(std::move(data_))
            , size(size_)
        {}

        Reference & operator=(Reference &&) noexcept = default;
        Reference & operator=(Reference const &) noexcept = default;

    /* Fields: */

        std::shared_ptr<void> data;
        std::size_t size;

    };
    using ReferenceVector = std::vector<Reference>;

    struct ConstReference {

    /* Methods: */

        ConstReference() noexcept = default;
        ConstReference(ConstReference &&) noexcept = default;
        ConstReference(ConstReference const &) noexcept = default;

        ConstReference(std::shared_ptr<void const> data_, std::size_t size_)
            : data(std::move(data_))
            , size(std::move(size_))
        {}

        ConstReference(Reference && move) noexcept
            : data(std::move(move.data))
            , size(std::move(move.size))
        {}

        ConstReference(Reference const & copy) noexcept
            : data(copy.data)
            , size(copy.size)
        {}

        ConstReference & operator=(ConstReference &&) noexcept = default;
        ConstReference & operator=(ConstReference const &) noexcept = default;

        ConstReference & operator=(Reference && move) noexcept {
            data = std::move(move.data);
            size = std::move(move.size);
            return *this;
        }

        ConstReference & operator=(Reference const & copy) noexcept {
            data = copy.data;
            size = copy.size;
            return *this;
        }

    /* Fields: */

        std::shared_ptr<const void> data;
        std::size_t size;

    };
    using ConstReferenceVector = std::vector<ConstReference>;

    struct SyscallContext {

    /* Types: */

        struct PublicMemoryPointer {
            std::uint64_t ptr;
        };

    /* Methods: */

        virtual ~SyscallContext() noexcept;

        virtual std::shared_ptr<void> processFacility(char const * facilityName)
                const noexcept = 0;

        /* Access to public dynamic memory inside the VM process: */
        virtual PublicMemoryPointer publicAlloc(std::uint64_t nBytes) = 0;
        virtual bool publicFree(PublicMemoryPointer ptr) = 0;
        virtual std::size_t publicMemPtrSize(PublicMemoryPointer ptr) = 0;
        virtual void * publicMemPtrData(PublicMemoryPointer ptr) = 0;

        /* Access to current code location: */
        virtual std::size_t currentLinkingUnitIndex() const noexcept = 0;
        virtual std::size_t currentInstructionIndex() const noexcept = 0;

    };

    struct SyscallWrapper {

    /* Methods: */

        virtual ~SyscallWrapper() noexcept;

        virtual void operator()(
                std::vector<::SharemindCodeBlock> & arguments,
                std::vector<Reference> & references,
                std::vector<ConstReference> & constReferences,
                ::SharemindCodeBlock * returnValue,
                Vm::SyscallContext & context) const = 0;

    };

    using SyscallFinder =
            std::shared_ptr<SyscallWrapper> (std::string const &);
    using SyscallFinderFun = std::function<SyscallFinder>;
    using SyscallFinderFunPtr = std::shared_ptr<SyscallFinderFun>;

    using FacilityFinder = std::shared_ptr<void> (char const *);
    using FacilityFinderFun = std::function<FacilityFinder>;
    using FacilityFinderFunPtr = std::shared_ptr<FacilityFinderFun>;

public: /* Methods: */

    Vm();

    /**
      \note The Vm object which will be moved from will be left in an invalid
            state in which operations other than move-assignment and destruction
            result in undefined behavior.
    */
    Vm(Vm &&) noexcept = default;

    Vm(Vm const &) noexcept = delete;

    virtual ~Vm() noexcept;

    /**
      \note The Vm object which will be moved from will be left in an invalid
            state in which operations other than move-assignment and destruction
            result in undefined behavior.
    */
    Vm & operator=(Vm &&) noexcept = default;

    Vm & operator=(Vm const &) noexcept = delete;

    void setSyscallFinder(SyscallFinderFunPtr f) noexcept;

    template <typename F>
    auto setSyscallFinder(F && f)
            -> typename std::enable_if<
                    !std::is_convertible<
                        typename std::decay<decltype(f)>::type,
                        SyscallFinderFunPtr
                    >::value,
                    void
                >::type
    {
        return setSyscallFinder(
                    std::make_shared<SyscallFinderFun>(std::forward<F>(f)));
    }

    std::shared_ptr<SyscallWrapper> findSyscall(std::string const & signature)
            const noexcept;

    void setProcessFacilityFinder(FacilityFinderFunPtr f) noexcept;

    template <typename F>
    auto setProcessFacilityFinder(F && f)
            -> typename std::enable_if<
                    !std::is_convertible<
                        typename std::decay<decltype(f)>::type,
                        FacilityFinderFunPtr
                    >::value,
                    void
                >::type
    {
        return setProcessFacilityFinder(
                    std::make_shared<FacilityFinderFun>(std::forward<F>(f)));
    }

    std::shared_ptr<void> findProcessFacility(char const * name) const noexcept;

private: /* Fields: */

    std::shared_ptr<Inner> m_inner;

}; /* class Vm */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_VM_H */
