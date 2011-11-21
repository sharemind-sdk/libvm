/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_VM_INTERNAL_CORE_H
#define SHAREMIND_LIBSMVM_VM_INTERNAL_CORE_H

#ifndef _SHAREMIND_INTERNAL
#define _SHAREMIND_INTERNAL
#endif

#include <stdint.h>
#include "vm_internal_helpers.h"


#ifdef __cplusplus
extern "C" {
#endif


#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5 )
#define _SMVM_NOCLONE noclone,
#else
#define _SMVM_NOCLONE
#endif

SMVM_Error _SMVM(SMVM_Program * p,
                 const SMVM_InnerCommand c,
                 void * d)
    __attribute__
    ((
         _SMVM_NOCLONE
         noinline
         warn_unused_result,
         optimize("O2",
                  "-fexpensive-optimizations",
                  "-fno-gcse"),
         hot,
         nonnull(1)
    ));


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_VM_INTERNAL_CORE_H */
