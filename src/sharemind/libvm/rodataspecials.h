/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_RODATASPECIALS_H
#define SHAREMIND_LIBVM_RODATASPECIALS_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include "memoryslot.h"


#ifdef __cplusplus
extern "C" {
#endif


static SharemindMemorySlotSpecials roDataSpecials = { .writeable = 0, .readable = 1, .free = NULL };


#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_RODATASPECIALS_H */
