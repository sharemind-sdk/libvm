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

#ifndef SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H
#define SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/comma.h>
#include <sharemind/extern_c.h>
#include <sharemind/vector.h>
#include <stdint.h>
#include <stdlib.h>


SHAREMIND_EXTERN_C_BEGIN

// static inline
SHAREMIND_VECTOR_DECLARE_BODY(SharemindDataSectionSizesVector,uint32_t)
SHAREMIND_VECTOR_DEFINE_BODY(SharemindDataSectionSizesVector,)
SHAREMIND_VECTOR_DECLARE_INIT(SharemindDataSectionSizesVector,
                              inline,
                              SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_INIT(SharemindDataSectionSizesVector, inline)
SHAREMIND_VECTOR_DECLARE_DESTROY(SharemindDataSectionSizesVector,
                                 inline,
                                 SHAREMIND_COMMA visibility("internal"))
SHAREMIND_VECTOR_DEFINE_DESTROY(SharemindDataSectionSizesVector, inline, free)
SHAREMIND_VECTOR_DECLARE_FORCE_RESIZE(SharemindDataSectionSizesVector,
                                      inline,
                                      visibility("internal"))
SHAREMIND_VECTOR_DEFINE_FORCE_RESIZE(SharemindDataSectionSizesVector,
                                     inline,
                                     realloc)
SHAREMIND_VECTOR_DECLARE_PUSH(SharemindDataSectionSizesVector,
                              inline,
                              visibility("internal"))
SHAREMIND_VECTOR_DEFINE_PUSH(SharemindDataSectionSizesVector, inline)

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_DATASECTIONSIZESVECTOR_H */
