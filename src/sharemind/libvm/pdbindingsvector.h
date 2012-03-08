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


#include <sharemind/vector.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


SHAREMIND_VECTOR_DECLARE(SharemindPdBindings,SharemindPd *,,static inline)
SHAREMIND_VECTOR_DEFINE(SharemindPdBindings,SharemindPd *,malloc,free,realloc,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_PDBINDINGSVECTOR_H */
