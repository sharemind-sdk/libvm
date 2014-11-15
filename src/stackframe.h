/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_STACKFRAME_H
#define SHAREMIND_LIBVM_STACKFRAME_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <assert.h>
#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "references.h"
#include "registervector.h"


SHAREMIND_EXTERN_C_BEGIN

struct SharemindStackFrame_ {
    SharemindRegisterVector stack;
    SharemindReferenceVector refstack;
    SharemindCReferenceVector crefstack;
    struct SharemindStackFrame_ * prev;

    const SharemindCodeBlock * returnAddr;
    SharemindCodeBlock * returnValueAddr;
};
typedef struct SharemindStackFrame_ SharemindStackFrame;

static inline void SharemindStackFrame_init(
        SharemindStackFrame * const restrict f,
        SharemindStackFrame * const restrict prev) __attribute__ ((nonnull(1)));
static inline void SharemindStackFrame_init(
        SharemindStackFrame * const restrict f,
        SharemindStackFrame * const restrict prev)
{
    assert(f);
    SharemindRegisterVector_init(&f->stack);
    SharemindReferenceVector_init(&f->refstack);
    SharemindCReferenceVector_init(&f->crefstack);
    f->prev = prev;
}

inline void SharemindStackFrame_destroy(SharemindStackFrame * const f)
        __attribute__ ((nonnull(1), visibility("internal")));
inline void SharemindStackFrame_destroy(SharemindStackFrame * const f) {
    assert(f);
    SharemindRegisterVector_destroy(&f->stack);
    SharemindReferenceVector_destroy(&f->refstack);
    SharemindCReferenceVector_destroy(&f->crefstack);
}

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_STACKFRAME_H */
