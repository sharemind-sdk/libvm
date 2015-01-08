/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_SYSCALLBINDINGSVECTOR_H
#define SHAREMIND_LIBVM_SYSCALLBINDINGSVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>
#include "libvm.h"


SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_VECTOR_DECLARE_BODY(SharemindSyscallBindingsVector,
                             SharemindSyscallWrapper)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindSyscallBindingsVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindSyscallBindingsVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindSyscallBindingsVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindSyscallBindingsVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindSyscallBindingsVector, inline, free)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindSyscallBindingsVector,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindSyscallBindingsVector,
                                     inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindSyscallBindingsVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindSyscallBindingsVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_SYSCALLBINDINGSVECTOR_H */
