/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_DATASECTIONSVECTOR_H
#define SHAREMIND_LIBVM_DATASECTIONSVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "datasection.h"


SHAREMIND_EXTERN_C_BEGIN

// static inline
SHAREMIND_VECTOR_DECLARE_BODY(SharemindDataSectionsVector, SharemindDataSection)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindDataSectionsVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindDataSectionsVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindDataSectionsVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindDataSectionsVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindDataSectionsVector,
                                     inline,,
                                     free,
                                     SharemindDataSection_destroy(value);)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindDataSectionsVector,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindDataSectionsVector,
                                     inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindDataSectionsVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindDataSectionsVector, inline)
SHAREMIND_VECTOR_DECLARE_POP(SharemindDataSectionsVector,
                             inline,
                             SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindDataSectionsVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_DATASECTIONSVECTOR_H */
