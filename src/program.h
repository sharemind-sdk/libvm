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

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/recursive_locks.h>
#include <sharemind/refs.h>
#include <sharemind/tag.h>
#include "codesectionsvector.h"
#include "datasectionsvector.h"
#include "datasectionsizesvector.h"
#include "lasterror.h"
#include "pdbindingsvector.h"
#include "syscallbindingsvector.h"
#include "vm.h"


SHAREMIND_EXTERN_C_BEGIN


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

    SHAREMIND_RECURSIVE_LOCK_DECLARE_FIELDS;
    SHAREMIND_LIBVM_LASTERROR_FIELDS;
    SHAREMIND_REFS_DECLARE_FIELDS
    SHAREMIND_TAG_DECLARE_FIELDS;

    size_t activeLinkingUnit;

    size_t prepareCodeSectionIndex;
    uintptr_t prepareIp;

    SharemindVm * vm;
    SharemindVirtualMachineContext * overrides;

    bool ready;
    const void * lastParsePosition;

};

SHAREMIND_RECURSIVE_LOCK_FUNCTIONS_DECLARE_DEFINE(
        SharemindProgram,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_LIBVM_LASTERROR_PRIVATE_FUNCTIONS_DECLARE(SharemindProgram)

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif
SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindProgram)
#ifdef __GNUC__
#pragma GCC visibility pop
#endif


static inline SharemindSyscallWrapper SharemindProgram_findSyscall(
        SharemindProgram * const p,
        const char * const signature)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindSyscallWrapper SharemindProgram_findSyscall(
        SharemindProgram * const p,
        const char * const signature)
{
    assert(p);
    assert(signature);
    assert(signature[0u]);
    if (p->overrides && p->overrides->find_syscall)
        return (*(p->overrides->find_syscall))(p->overrides, signature);

    return SharemindVm_findSyscall(p->vm, signature);
}

static inline SharemindPd * SharemindProgram_findPd(SharemindProgram * p,
                                                    const char * pdName)
        __attribute__ ((nonnull(1, 2), warn_unused_result));
static inline SharemindPd * SharemindProgram_findPd(SharemindProgram * const p,
                                                    const char * const pdName)
{
    assert(p);
    assert(pdName);
    assert(pdName[0u]);
    if (p->overrides && p->overrides->find_pd)
        return (*(p->overrides->find_pd))(p->overrides, pdName);

    return SharemindVm_findPd(p->vm, pdName);
}


SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
