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


#include <sharemind/instrset.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif


SHAREMIND_INSTRSET_DECLARE(SharemindInstrSet,static inline)
SHAREMIND_INSTRSET_DEFINE(SharemindInstrSet,malloc,free,static inline)


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_INSTRSET_H */
