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
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/api_0x1.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "memoryslot.h"


SHAREMIND_EXTERN_C_BEGIN

typedef SharemindModuleApi0x1Reference SharemindReference;
typedef SharemindModuleApi0x1CReference SharemindCReference;

inline void SharemindReference_destroy(SharemindReference * const r) {
    assert(r);
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

inline void SharemindCReference_destroy(SharemindCReference * const r) {
    assert(r);
    if (r->internal)
        ((SharemindMemorySlot *) r->internal)->nrefs--;
}

SHAREMIND_VECTOR_DECLARE(SharemindReferenceVector,SharemindReference,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindReferenceVector,
                        SharemindReference,
                        malloc,
                        free,
                        realloc,
                        inline)
SHAREMIND_VECTOR_DECLARE(SharemindCReferenceVector,SharemindCReference,,inline)
SHAREMIND_VECTOR_DEFINE(SharemindCReferenceVector,
                        SharemindCReference,
                        malloc,
                        free,
                        realloc,
                        inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_REFERENCES_H */
