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

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/stringmap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "libvm.h"


typedef void * SharemindProcessFacility;

SHAREMIND_STRINGMAP_DECLARE_BODY(SharemindProcessFacilityMapInner,
                                 SharemindProcessFacility)
SHAREMIND_STRINGMAP_DECLARE_init(SharemindProcessFacilityMapInner,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_init(SharemindProcessFacilityMapInner, inline)
SHAREMIND_STRINGMAP_DECLARE_destroy(SharemindProcessFacilityMapInner,
                                    inline,,
                                    SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_destroy(SharemindProcessFacilityMapInner,
                                   inline,
                                   SharemindProcessFacility,,
                                   free,)
SHAREMIND_STRINGMAP_DECLARE_get(SharemindProcessFacilityMapInner,
                                inline,
                                SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_get(SharemindProcessFacilityMapInner, inline)
SHAREMIND_STRINGMAP_DECLARE_insertHint(SharemindProcessFacilityMapInner,
                                       inline,
                                       SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_insertHint(SharemindProcessFacilityMapInner, inline)
SHAREMIND_STRINGMAP_DECLARE_emplaceAtHint(
        SharemindProcessFacilityMapInner,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_emplaceAtHint(SharemindProcessFacilityMapInner,
                                         inline)
SHAREMIND_STRINGMAP_DECLARE_insertAtHint(SharemindProcessFacilityMapInner,
                                         inline,
                                         SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_insertAtHint(SharemindProcessFacilityMapInner,
                                        inline,
                                        strdup,
                                        malloc,
                                        free)
SHAREMIND_STRINGMAP_DECLARE_remove(SharemindProcessFacilityMapInner,
                                   inline,
                                   SHAREMIND_COMMA visibility("internal"))
SHAREMIND_STRINGMAP_DEFINE_remove(SharemindProcessFacilityMapInner,
                                  inline,
                                  SharemindProcessFacility,
                                  free,)

SHAREMIND_EXTERN_C_BEGIN

typedef SharemindProcessFacility (* SharemindProcessFacilityMapNextGetter)(
        void * context,
        const char * name);

typedef struct SharemindProcessFacilityMap_ {
    SharemindProcessFacilityMapInner realMap;
    SharemindProcessFacilityMapNextGetter nextGetter;
    void * nextContext;
} SharemindProcessFacilityMap;

inline SharemindProcessFacility SharemindProcessFacilityMap_get(
            const SharemindProcessFacilityMap * fm,
            const char * name)
        __attribute__ ((nonnull(1, 2), visibility("internal")));
inline SharemindProcessFacility SharemindProcessFacilityMap_get(
        const SharemindProcessFacilityMap * fm,
        const char * name)
{
    assert(fm);
    assert(name);
    assert(name[0]);
    const SharemindProcessFacilityMapInner_value * const v =
            SharemindProcessFacilityMapInner_get(&fm->realMap, name);
    if (v)
        return v->value;
    if (fm->nextGetter)
        return (*(fm->nextGetter))(fm->nextContext, name);
    return NULL;
}

SharemindProcessFacility SharemindProcessFacilityMap_nextMapGetter(
        void * context,
        const char * name)
        __attribute__ ((nonnull(1, 2), visibility("internal")));

inline void SharemindProcessFacilityMap_init(
        SharemindProcessFacilityMap * fm,
        SharemindProcessFacilityMap * nextMap)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindProcessFacilityMap_init(
        SharemindProcessFacilityMap * fm,
        SharemindProcessFacilityMap * nextMap)
{
    assert(fm);
    if (nextMap) {
        fm->nextGetter = &SharemindProcessFacilityMap_nextMapGetter;
        fm->nextContext = nextMap;
    } else {
        fm->nextGetter = NULL;
        fm->nextContext = NULL;
    }
    SharemindProcessFacilityMapInner_init(&fm->realMap);
}

inline void SharemindProcessFacilityMap_init_with_getter(
        SharemindProcessFacilityMap * fm,
        SharemindProcessFacilityMapNextGetter nextGetter,
        void * context)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindProcessFacilityMap_init_with_getter(
        SharemindProcessFacilityMap * fm,
        SharemindProcessFacilityMapNextGetter nextGetter,
        void * context)
{
    assert(fm);
    fm->nextGetter = nextGetter;
    fm->nextContext = context;
    SharemindProcessFacilityMapInner_init(&fm->realMap);
}

inline void SharemindProcessFacilityMap_destroy(
        SharemindProcessFacilityMap * fm)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindProcessFacilityMap_destroy(
        SharemindProcessFacilityMap * fm)
{
    assert(fm);
    SharemindProcessFacilityMapInner_destroy(&fm->realMap);
}

SHAREMIND_EXTERN_C_END

#define SHAREMIND_DEFINE_FACILITYMAP_ACCESSORS_(CN,fF,FF) \
    SHAREMIND_EXTERN_C_BEGIN \
    SharemindVmError CN ## _set ## FF( \
            CN * c, \
            const char * name, \
            SharemindProcessFacility const facility) \
    { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        SharemindVmError r = SHAREMIND_VM_OK; \
        CN ## _lock(c); \
        SharemindProcessFacility const f = \
                SharemindProcessFacilityMap_get(&c->fF ## Map, name); \
        if (unlikely(f)) { \
            r = SHAREMIND_VM_FACILITY_ALREADY_EXISTS; \
            CN ## _setError(c, r, "Facility with this name already exists!"); \
            goto CN ## _set ## FF ## _exit; \
        } \
        void * const insertHint = \
                SharemindProcessFacilityMapInner_insertHint(\
                        &c->fF ## Map.realMap, \
                        name); \
        assert(insertHint); \
        SharemindProcessFacilityMapInner_value * const v = \
                SharemindProcessFacilityMapInner_insertAtHint( \
                        &c->fF ## Map.realMap, \
                        name, \
                        insertHint); \
        if (unlikely(!v)) { \
            r = SHAREMIND_VM_OUT_OF_MEMORY; \
            CN ## _setErrorOom(c); \
            goto CN ## _set ## FF ## _exit; \
        } \
        v->value = facility; \
    CN ## _set ## FF ## _exit: \
        CN ## _unlock(c); \
        return r; \
    } \
    bool CN ## _unset ## FF(CN * c, const char * name) { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        CN ## _lock(c); \
        const bool r = \
                SharemindProcessFacilityMapInner_remove(&c->fF ## Map.realMap, \
                                                        name); \
        CN ## _unlock(c); \
        return r; \
    } \
    SharemindProcessFacility \
    CN ## _ ## fF(const CN * c, const char * name) { \
        assert(c); \
        assert(name); \
        assert(name[0]); \
        CN ## _lockConst(c); \
        SharemindProcessFacility const r = \
                SharemindProcessFacilityMap_get(&c->fF ## Map, name); \
        CN ## _unlockConst(c); \
        return r; \
    } \
    SHAREMIND_EXTERN_C_END

#define SHAREMIND_DEFINE_FACILITYMAP_ACCESSORS(ClassName,fN,FN) \
    SHAREMIND_DEFINE_FACILITYMAP_ACCESSORS_(ClassName, \
                                             fN ## Facility, \
                                             FN ## Facility)

#define SHAREMIND_DEFINE_SELF_FACILITYMAP_ACCESSORS(ClassName) \
    SHAREMIND_DEFINE_FACILITYMAP_ACCESSORS_(ClassName, facility, Facility)

#endif /* SHAREMIND_LIBVM_FACILITYMAP_H */
