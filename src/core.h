/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_CORE_H
#define SHAREMIND_LIBVM_CORE_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/extern_c.h>
#include <stdint.h>
#include "libvm.h"
#include "innercommand.h"


SHAREMIND_EXTERN_C_BEGIN

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 5 )
#define _SHAREMIND_NOCLONE noclone,
#else
#define _SHAREMIND_NOCLONE
#endif

typedef SharemindVmError (*SharemindCoreVmRunner)(
        SharemindProcess * p,
        const SharemindInnerCommand c,
        void * d);

SharemindVmError sharemind_vm_run(
        SharemindProcess * p,
        const SharemindInnerCommand c,
        void * d)
    __attribute__
    ((
         _SHAREMIND_NOCLONE
         noinline
         warn_unused_result,
         #if !defined(SHAREMIND_FAST_BUILD) && !defined(__clang__)
         optimize("-fno-gcse",
                  "-fno-reorder-blocks",
                  "-fno-reorder-blocks-and-partition"),
         #endif
         hot,
         visibility("internal")
    ));

SharemindVmError sharemind_vm_profile(
        SharemindProcess * p,
        const SharemindInnerCommand c,
        void * d)
    __attribute__
    ((
         _SHAREMIND_NOCLONE
         noinline
         warn_unused_result,
         #if !defined(SHAREMIND_FAST_BUILD) && !defined(__clang__)
         optimize("-fno-gcse",
                  "-fno-reorder-blocks",
                  "-fno-reorder-blocks-and-partition"),
         #endif
         hot,
         visibility("internal")
    ));

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_CORE_H */
