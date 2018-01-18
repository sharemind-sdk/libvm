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

#ifndef SHAREMIND_LIBVM_FACILITYMAP_H
#define SHAREMIND_LIBVM_FACILITYMAP_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <sharemind/Exception.h>
#include <sharemind/ExceptionMacros.h>
#include <sharemind/extern_c.h>
#include <sharemind/SimpleUnorderedStringMap.h>
#include <string>
#include <type_traits>
#include "Vm.h"


namespace sharemind {
namespace Detail {

class __attribute__((visibility("internal"))) ProcessFacilityMap {

private: /* Types: */

    using Inner = sharemind::SimpleUnorderedStringMap<void *>;

public: /* Types: */

    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(sharemind::Exception, Exception);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(Exception,
                                                   FacilityNameClashException);

    using NextGetter = Vm::ProcessFacilityFinder;
    using NextGetterFun = Vm::ProcessFacilityFinderFun;
    using NextGetterFunPtr = Vm::ProcessFacilityFinderFunPtr;

public: /* Methods: */

    void setFacility(std::string name, void * const facility);

    void setNextGetter(NextGetterFunPtr nextGetter) noexcept;

    template <typename F>
    auto setNextGetter(F && f)
            -> typename std::enable_if<
                    !std::is_convertible<
                        typename std::decay<decltype(f)>::type,
                        NextGetterFunPtr
                    >::value,
                    void
                >::type
    {
        return setNextGetter(
                    std::make_shared<NextGetterFun>(std::forward<F>(f)));
    }

    void * facility(char const * const name) const noexcept;
    void * facility(std::string const & name) const noexcept;

    bool unsetFacility(std::string const & name) noexcept;

private: /* Fields: */

    mutable std::mutex m_mutex;
    Inner m_inner;
    NextGetterFunPtr m_nextGetter;

};

} /* namespace Detail { */
} /* namespace sharemind { */

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS \
    std::shared_ptr<::sharemind::Detail::ProcessFacilityMap> \
            m_processFacilityMap{ \
                std::make_shared<::sharemind::Detail::ProcessFacilityMap>()}

#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(fNF,FN) \
    void set ## FN ## Facility(std::string name, void * facility); \
    void * fNF(char const * const name) const noexcept; \
    void * fNF(std::string const & name) const noexcept; \
    bool unset ## FN ## Facility(std::string const & name) noexcept;
#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS \
    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(processFacility, Process)
#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_SELF \
    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(facility,)

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,fNF,FN,prefix) \
    void CN::set ## FN ## Facility(std::string name, void * facility) { \
        return prefix m_processFacilityMap->setFacility(std::move(name), \
                                                        std::move(facility)); \
    } \
    void * CN::fNF(char const * const name) const noexcept \
    { return prefix m_processFacilityMap->facility(name); } \
    void * CN::fNF(std::string const & name) const noexcept \
    { return prefix m_processFacilityMap->facility(name); } \
    bool CN::unset ## FN ## Facility(std::string const & name) noexcept \
    { return prefix m_processFacilityMap->unsetFacility(name); }
#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(CN,pfx) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,processFacility,Process,pfx)
#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_SELF(CN,prefix) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,facility,,prefix)

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_INTERCLASS_CHAIN(self, other) \
    do { \
        auto const & smartPtr = (other).m_processFacilityMap; \
        (self).m_processFacilityMap->setNextGetter( \
                [smartPtr](std::string const & name) noexcept \
                { return smartPtr->facility(name); }); \
    } while(false)

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_ACCESSORS_(CN,cF,fF,FF) \
    SHAREMIND_EXTERN_C_BEGIN \
    SharemindVmError CN ## _set ## FF( \
            CN * c, \
            const char * name, \
            ::sharemind::Detail::ProcessFacility const facility) \
    { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        SharemindVmError r = SHAREMIND_VM_OK; \
        CN ## _lock(c); \
        using PFM = ::sharemind::Detail::ProcessFacilityMap; \
        using FNCE = PFM::FacilityNameClashException; \
        try { \
            c->cF setFacility(name, facility); \
        } catch (FNCE const & e) { \
            r = SHAREMIND_VM_FACILITY_ALREADY_EXISTS; \
            CN ## _setError(c, r, e.what()); \
        } catch (...) { \
            r = SHAREMIND_VM_OUT_OF_MEMORY; \
            CN ## _setErrorOom(c); \
        } \
        CN ## _unlock(c); \
        return r; \
    } \
    bool CN ## _unset ## FF(CN * c, const char * name) { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        CN ## _lock(c); \
        const bool r = c->cF unsetFacility(name); \
        CN ## _unlock(c); \
        return r; \
    } \
    void * CN ## _ ## fF(const CN * c, const char * name) { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        CN ## _lockConst(c); \
        auto const r = c->cF facility(name); \
        CN ## _unlockConst(c); \
        return r; \
    } \
    SHAREMIND_EXTERN_C_END

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_ACCESSORS(CN) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_ACCESSORS_(CN, \
                                                   m_processFacilityMap->, \
                                                   processFacility, \
                                                   ProcessFacility)

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_SELF_ACCESSORS(CN) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_ACCESSORS_(CN, \
                                                   m_processFacilityMap->, \
                                                   facility, \
                                                   Facility)

#endif /* SHAREMIND_LIBVM_FACILITYMAP_H */
