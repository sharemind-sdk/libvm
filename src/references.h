/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_REFERENCES_H
#define SHAREMIND_LIBVM_REFERENCES_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <assert.h>
#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/api_0x1.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "memoryslot.h"


SHAREMIND_EXTERN_C_BEGIN

typedef SharemindModuleApi0x1Reference SharemindReference;
typedef SharemindModuleApi0x1CReference SharemindCReference;

inline void SharemindReference_destroy(SharemindReference * const r)
        __attribute__((nonnull(1), visibility("internal")));
inline void SharemindReference_destroy(SharemindReference * const r) {
    assert(r);
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

inline void SharemindCReference_destroy(SharemindCReference * const r)
        __attribute__((nonnull(1), visibility("internal")));
inline void SharemindCReference_destroy(SharemindCReference * const r) {
    assert(r);
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

SHAREMIND_VECTOR_DECLARE_BODY(SharemindReferenceVector, SharemindReference)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindReferenceVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindReferenceVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindReferenceVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindReferenceVector,
                                     inline,,
                                     free,
                                     SharemindReference_destroy(value);)
SHAREMIND_VECTOR_DECLARE_GET_CONST_POINTER(SharemindReferenceVector,
                                           inline,
                                           SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_CONST_POINTER(SharemindReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindReferenceVector,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindReferenceVector, inline, realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindReferenceVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_POP(SharemindReferenceVector,
                             inline,
                             SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindReferenceVector, inline)

SHAREMIND_VECTOR_DECLARE_BODY(SharemindCReferenceVector, SharemindCReference)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindCReferenceVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindCReferenceVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindCReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindCReferenceVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindCReferenceVector,
                                     inline,,
                                     free,
                                     SharemindCReference_destroy(value);)
SHAREMIND_VECTOR_DECLARE_GET_CONST_POINTER(
        SharemindCReferenceVector,
        inline,
        SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_CONST_POINTER(SharemindCReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindCReferenceVector,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindCReferenceVector, inline, realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindCReferenceVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindCReferenceVector, inline)
SHAREMIND_VECTOR_DECLARE_POP(SharemindCReferenceVector,
                             inline,
                             SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindCReferenceVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_REFERENCES_H */
