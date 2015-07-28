/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
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
#define SHAREMIND_NOCLONE_ noclone,
#else
#define SHAREMIND_NOCLONE_
#endif

typedef SharemindVmError (*SharemindCoreVmRunner)(
        SharemindProcess * p,
        SharemindInnerCommand const c,
        void * d);

SharemindVmError sharemind_vm_run(
        SharemindProcess * p,
        SharemindInnerCommand const c,
        void * d)
    __attribute__
    ((
         SHAREMIND_NOCLONE_
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
        SharemindInnerCommand const c,
        void * d)
    __attribute__
    ((
         SHAREMIND_NOCLONE_
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
