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

#ifndef SHAREMIND_LIBVM_INNERCOMMAND_H
#define SHAREMIND_LIBVM_INNERCOMMAND_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <sharemind/extern_c.h>

SHAREMIND_EXTERN_C_BEGIN

typedef enum {
    SHAREMIND_I_GET_IMPL_LABEL,
    SHAREMIND_I_RUN,
    SHAREMIND_I_CONTINUE
} SharemindInnerCommand;

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_INNERCOMMAND_H */
