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

#ifndef SHAREMIND_LIBVM_REGISTERVECTOR_H
#define SHAREMIND_LIBVM_REGISTERVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

SHAREMIND_VECTOR_DECLARE_BODY(SharemindRegisterVector, SharemindCodeBlock)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindRegisterVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindRegisterVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindRegisterVector,
                                 inline,
                                 visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindRegisterVector, inline, free)
SHAREMIND_VECTOR_DECLARE_GET_POINTER(SharemindRegisterVector,
                                     inline,
                                     visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_POINTER(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_GET_CONST_POINTER(SharemindRegisterVector,
                                           inline,
                                           visibility("internal"))
SHAREMIND_VECTOR_DEFINE_GET_CONST_POINTER(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindRegisterVector,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindRegisterVector, inline, realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindRegisterVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_RESIZE_NO_OCHECK(SharemindRegisterVector,
                                          inline,
                                          visibility("internal"))
SHAREMIND_VECTOR_DEFINE_RESIZE_NO_OCHECK(SharemindRegisterVector, inline)
SHAREMIND_VECTOR_DECLARE_RESIZE(SharemindRegisterVector,
                                inline,
                                visibility("internal"))
SHAREMIND_VECTOR_DEFINE_RESIZE(SharemindRegisterVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_REGISTERVECTOR_H */
