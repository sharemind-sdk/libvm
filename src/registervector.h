/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_REGISTERVECTOR_H
#define SHAREMIND_LIBVM_REGISTERVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_VECTOR_DEFINE_BODY(SharemindRegisterVector, SharemindCodeBlock,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindRegisterVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindRegisterVector,
                                 inline,
                                 visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindRegisterVector, inline, free)
SHAREMIND_VECTOR_DECLARE_GET_POINTER(SharemindRegisterVector,
                                     inline,
                                     SharemindCodeBlock,
                                     visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_POINTER(SharemindRegisterVector,
                                    inline,
                                    SharemindCodeBlock)
SHAREMIND_VECTOR_DECLARE_GET_CONST_POINTER(SharemindRegisterVector,
                                           inline,
                                           SharemindCodeBlock,
                                           visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_CONST_POINTER(SharemindRegisterVector,
                                          inline,
                                          SharemindCodeBlock)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindRegisterVector,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindRegisterVector,
                                     inline,
                                     SharemindCodeBlock,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindRegisterVector,
                              inline,
                              SharemindCodeBlock,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindRegisterVector,
                             inline,
                             SharemindCodeBlock)
SHAREMIND_VECTOR_DECLARE_RESIZE_NO_OCHECK(SharemindRegisterVector,
                                          inline,
                                          visibility("internal"))
SHAREMIND_VECTOR_DEFINE_RESIZE_NO_OCHECK(SharemindRegisterVector,
                                         inline,
                                         SharemindCodeBlock)
SHAREMIND_VECTOR_DECLARE_RESIZE(SharemindRegisterVector,
                                inline,
                                visibility("internal"))
SHAREMIND_VECTOR_DEFINE_RESIZE(SharemindRegisterVector,
                               inline,
                               SharemindCodeBlock)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_REGISTERVECTOR_H */
