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

SHAREMIND_VECTOR_DECLARE_BODY(SharemindPdBindings, SharemindPd *)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindPdBindings,)
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
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindPdBindings, inline, realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindPdBindings,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindPdBindings, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PDBINDINGSVECTOR_H */
