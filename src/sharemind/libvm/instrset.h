/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_INSTRSET_H
#define SHAREMIND_LIBVM_INSTRSET_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <sharemind/simpleset.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


SHAREMIND_SIMPLESET_DECLARE(SharemindInstrSet,uintptr_t,static inline)
SHAREMIND_SIMPLESET_DEFINE(SharemindInstrSet,uintptr_t,malloc,free,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_INSTRSET_H */
