/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_FRAMESTACK_H
#define SHAREMIND_LIBVM_FRAMESTACK_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "stackframe.h"

SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_VECTOR_DEFINE_BODY(SharemindFrameStack,SharemindStackFrame,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindFrameStack,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindFrameStack, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindFrameStack,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY_WITH(SharemindFrameStack,
                                     inline,
                                     SharemindStackFrame,,
                                     free,
                                     SharemindStackFrame_destroy(value);)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindFrameStack,
                                      inline,
                                      SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindFrameStack,
                                     inline,
                                     SharemindStackFrame,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindFrameStack,
                              inline,
                              SharemindStackFrame,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindFrameStack,
                             inline,
                             SharemindStackFrame)
SHAREMIND_VECTOR_DECLARE_POP(SharemindFrameStack,
                             inline,
                             SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_POP(SharemindFrameStack, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_FRAMESTACK_H */

