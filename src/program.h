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

#ifndef SHAREMIND_LIBVM_PROGRAM_H
#define SHAREMIND_LIBVM_PROGRAM_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/recursive_locks.h>
#include <sharemind/refs.h>
#include <sharemind/tag.h>
#include <sharemind/vector.h>
#include "codesection.h"
#include "datasectionsvector.h"
#include "datasectionsizesvector.h"
#include "lasterror.h"
#include "pdbindingsvector.h"
#include "processfacilitymap.h"
#include "syscallbindingsvector.h"
#include "vm.h"


SHAREMIND_EXTERN_C_BEGIN


/*******************************************************************************
 *  SharemindCodeSectionsVector
*******************************************************************************/

SHAREMIND_VECTOR_DECLARE_BODY(SharemindCodeSectionsVector,SharemindCodeSection)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindCodeSectionsVector,)


/*******************************************************************************
 *  SharemindProgram
*******************************************************************************/

struct SharemindProgram_ {
    SharemindCodeSectionsVector codeSections;
    SharemindDataSectionsVector rodataSections;
    SharemindDataSectionsVector dataSections;
    SharemindDataSectionSizesVector bssSectionSizes;

    SharemindSyscallBindingsVector bindings;
    SharemindPdBindings pdBindings;

    SharemindProcessFacilityMap processFacilityMap;
    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;
    SHAREMIND_REFS_DECLARE_FIELDS
    SHAREMIND_TAG_DECLARE_FIELDS;

    size_t activeLinkingUnit;

    size_t prepareCodeSectionIndex;
    uintptr_t prepareIp;

    SharemindVm * vm;

    bool ready;
    void const * lastParsePosition;

};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE(
        SharemindProgram,,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindProgram)

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif
SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindProgram)
#ifdef __GNUC__
#pragma GCC visibility pop
#endif

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
