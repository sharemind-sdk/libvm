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

#include <memory>
#include <sharemind/libmodapi/libmodapi.h>


namespace sharemind {

class Program;

class Vm {

    friend class Program;

private: /* Types: */

    struct Inner;

public: /* Methods: */

    using SyscallFinder = SharemindSyscallWrapper (std::string const &);
    using SyscallFinderFun = std::function<SyscallFinder>;
    using SyscallFinderFunPtr = std::shared_ptr<SyscallFinderFun>;

    using PdFinder = SharemindPd * (std::string const &);
    using PdFinderFun = std::function<PdFinder>;
    using PdFinderFunPtr = std::shared_ptr<PdFinderFun>;

    using ProcessFacilityFinder = void * (char const *);
    using ProcessFacilityFinderFun = std::function<ProcessFacilityFinder>;
    using ProcessFacilityFinderFunPtr =
            std::shared_ptr<ProcessFacilityFinderFun>;

public: /* Methods: */

    Vm();
    virtual ~Vm() noexcept;

    void setSyscallFinder(SyscallFinderFunPtr f) noexcept;
    void setPdFinder(PdFinderFunPtr f) noexcept;
    void setProcessFacilityFinder(ProcessFacilityFinderFunPtr f) noexcept;

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

    template <typename F>
    auto setPdFinder(F && f)
            -> typename std::enable_if<
                    !std::is_convertible<
                        typename std::decay<decltype(f)>::type,
                        PdFinderFunPtr
                    >::value,
                    void
                >::type
    { return setPdFinder(std::make_shared<PdFinderFun>(std::forward<F>(f))); }

    template <typename F>
    auto setProcessFacilityFinder(F && f)
            -> typename std::enable_if<
                    !std::is_convertible<
                        typename std::decay<decltype(f)>::type,
                        ProcessFacilityFinderFunPtr
                    >::value,
                    void
                >::type
    {
        return setProcessFacilityFinder(
                    std::make_shared<ProcessFacilityFinderFun>(
                        std::forward<F>(f)));
    }

    SharemindSyscallWrapper findSyscall(std::string const & signature)
            const noexcept;

    SharemindPd * findPd(std::string const & pdName) const noexcept;

    void setProcessFacility(std::string name, void * facility);
    void * processFacility(std::string const & name) const noexcept;
    bool unsetProcessFacility(std::string const & name) noexcept;

private: /* Fields: */

    std::shared_ptr<Inner> m_inner;

}; /* class Vm */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_VM_H */
