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

#ifndef SHAREMIND_LIBVM_STACKFRAME_H
#define SHAREMIND_LIBVM_STACKFRAME_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <cassert>
#include <cstdlib>
#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <vector>
#include "references.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindStackFrame_ {

/* Types: */

    using RegisterVector = std::vector<::SharemindCodeBlock>;

/* Fields: */

    RegisterVector stack;
    SharemindReferenceVector refstack;
    SharemindCReferenceVector crefstack;
    struct SharemindStackFrame_ * prev;

    SharemindCodeBlock const * returnAddr;
    SharemindCodeBlock * returnValueAddr;
};
typedef struct SharemindStackFrame_ SharemindStackFrame;

static inline void SharemindStackFrame_init(
        SharemindStackFrame * const f,
        SharemindStackFrame * const prev) __attribute__ ((nonnull(1)));
static inline void SharemindStackFrame_init(
        SharemindStackFrame * const f,
        SharemindStackFrame * const prev)
{
    assert(f);
    new (&f->stack) SharemindStackFrame::RegisterVector();
    SharemindReferenceVector_init(&f->refstack);
    SharemindCReferenceVector_init(&f->crefstack);
    f->prev = prev;
}

inline void SharemindStackFrame_destroy(SharemindStackFrame * const f)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindStackFrame_destroy(SharemindStackFrame * const f) {
    assert(f);
    f->stack.SharemindStackFrame::RegisterVector::~RegisterVector();
    SharemindReferenceVector_destroy(&f->refstack);
    SharemindCReferenceVector_destroy(&f->crefstack);
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_STACKFRAME_H */
