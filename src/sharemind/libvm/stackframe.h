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
#include <sharemind/vector.h>
#include <stdlib.h>
#include "references.h"
#include "registervector.h"


#ifdef __cplusplus
extern "C" {
#endif


struct SharemindStackFrame_ {
    SharemindRegisterVector stack;
    SharemindReferenceVector refstack;
    SharemindCReferenceVector crefstack;
    struct SharemindStackFrame_ * prev;

    const SharemindCodeBlock * returnAddr;
    SharemindCodeBlock * returnValueAddr;
};
typedef struct SharemindStackFrame_ SharemindStackFrame;

static inline void SharemindStackFrame_init(SharemindStackFrame * f, SharemindStackFrame * prev) __attribute__ ((nonnull(1)));
static inline void SharemindStackFrame_init(SharemindStackFrame * f, SharemindStackFrame * prev) {
    assert(f);
    SharemindRegisterVector_init(&f->stack);
    SharemindReferenceVector_init(&f->refstack);
    SharemindCReferenceVector_init(&f->crefstack);
    f->prev = prev;
}

static inline void SharemindStackFrame_destroy(SharemindStackFrame * f) __attribute__ ((nonnull(1)));
static inline void SharemindStackFrame_destroy(SharemindStackFrame * f) {
    assert(f);
    SharemindRegisterVector_destroy(&f->stack);
    SharemindReferenceVector_destroy_with(&f->refstack, &SharemindReference_destroy);
    SharemindCReferenceVector_destroy_with(&f->crefstack, &SharemindCReference_destroy);
}


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_STACKFRAME_H */
