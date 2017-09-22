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
#include <sharemind/extern_c.h>
#include <string>
#include <unordered_map>
#include "libvm.h"


namespace sharemind {

using ProcessFacility = void *;

class ProcessFacilityMap {

private: /* Types: */

    using Inner = std::unordered_map<std::string, ProcessFacility>;

public: /* Types: */

    using NextGetter =
            std::function<ProcessFacility (std::string const &) noexcept>;

    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(std::exception, Exception);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(Exception,
                                                   FacilityNameClashException);

public: /* Methods: */

    void setFacility(std::string name, ProcessFacility facility);

    void setNextGetter(std::shared_ptr<NextGetter> nextGetter) noexcept;

    template <typename ... Args>
    void setNextGetter(Args && ... args) {
        return setNextGetter(
                    std::make_shared<NextGetter>(std::forward<Args>(args)...));
    }

    ProcessFacility facility(std::string const & name) const noexcept;

    bool unsetFacility(std::string const & name) noexcept;

private: /* Fields: */

    mutable std::mutex m_mutex;
    Inner m_inner;
    std::shared_ptr<NextGetter> m_nextGetter;

};

} /* namespace sharemind { */

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_FIELDS \
    std::shared_ptr<::sharemind::ProcessFacilityMap> \
            m_processFacilityMap{ \
                std::make_shared<::sharemind::ProcessFacilityMap>()}

#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(fNF,FN) \
    void set ## FN ## Facility(std::string name, ProcessFacility facility); \
    ProcessFacility fNF(std::string const & name) const noexcept; \
    bool unset ## FN ## Facility(std::string const & name) noexcept;
#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS \
    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(processFacility, Process)
#define SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_SELF \
    SHAREMIND_DECLARE_PROCESSFACILITYMAP_METHODS_(facility,)

#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,fNF,FN) \
    void CN::set ## FN ## Facility(std::string name, \
                                   ProcessFacility facility) \
    { \
        return m_processFacilityMap->setFacility(std::move(name), \
                                                 std::move(facility));\
    } \
    ProcessFacility CN::fNF(std::string const & name) const noexcept \
    { return m_processFacilityMap->facility(name); } \
    bool CN::unset ## FN ## Facility(std::string const & name) noexcept \
    { return m_processFacilityMap->unsetFacility(name); }
#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS(CN) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,processFacility, Process)
#define SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_SELF(CN) \
    SHAREMIND_DEFINE_PROCESSFACILITYMAP_METHODS_(CN,facility,)

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
            sharemind::ProcessFacility const facility) \
    { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        SharemindVmError r = SHAREMIND_VM_OK; \
        CN ## _lock(c); \
        using FNCE = sharemind::ProcessFacilityMap::FacilityNameClashException;\
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
    sharemind::ProcessFacility \
    CN ## _ ## fF(const CN * c, const char * name) { \
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
