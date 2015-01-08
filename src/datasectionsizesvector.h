/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H
#define SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdint.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

// static inline
SHAREMIND_VECTOR_DECLARE_BODY(SharemindDataSectionSizesVector,uint32_t)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindDataSectionSizesVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindDataSectionSizesVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindDataSectionSizesVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindDataSectionSizesVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindDataSectionSizesVector, inline, free)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindDataSectionSizesVector,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindDataSectionSizesVector,
                                     inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindDataSectionSizesVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindDataSectionSizesVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H */
