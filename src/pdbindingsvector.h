/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PDBINDINGSVECTOR_H
#define SHAREMIND_LIBVM_PDBINDINGSVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/vector.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_VECTOR_DEFINE_BODY(SharemindPdBindings, SharemindPd *,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindPdBindings,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindPdBindings, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindPdBindings,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindPdBindings, inline, free)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindPdBindings,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindPdBindings,
                                     inline,
                                     SharemindPd *,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindPdBindings,
                              inline,
                              SharemindPd *,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindPdBindings,
                             inline,
                             SharemindPd *)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PDBINDINGSVECTOR_H */
