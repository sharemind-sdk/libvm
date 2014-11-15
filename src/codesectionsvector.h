/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_CODESECTIONSVECTOR_H
#define SHAREMIND_LIBVM_CODESECTIONSVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "codesection.h"


SHAREMIND_EXTERN_C_BEGIN

// static inline
SHAREMIND_VECTOR_DEFINE_BODY(SharemindCodeSectionsVector,SharemindCodeSection,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindCodeSectionsVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindCodeSectionsVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindCodeSectionsVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindCodeSectionsVector,
                                     inline,
                                     SharemindCodeSection,,
                                     free,
                                     SharemindCodeSection_destroy(value);)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindCodeSectionsVector,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindCodeSectionsVector,
                                     inline,
                                     SharemindCodeSection,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindCodeSectionsVector,
                              inline,
                              SharemindCodeSection,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindCodeSectionsVector,
                             inline,
                             SharemindCodeSection)
SHAREMIND_VECTOR_DECLARE_POP(SharemindCodeSectionsVector,
                             inline,
                             SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindCodeSectionsVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_CODESECTIONSVECTOR_H */
