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
#include <sharemind/stack.h>
#include <stdlib.h>
#include "stackframe.h"

SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_STACK_DECLARE(SharemindFrameStack,
                        SharemindStackFrame,,
                        static inline)
SHAREMIND_STACK_DEFINE(SharemindFrameStack,
                       SharemindStackFrame,
                       malloc,
                       free,
                       static inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_FRAMESTACK_H */

