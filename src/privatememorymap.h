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

#ifndef SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H
#define SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <sharemind/fnv.h>
#include <sharemind/map.h>


SHAREMIND_MAP_DECLARE_BODY(SharemindPrivateMemoryMap, void *, size_t)
SHAREMIND_MAP_DECLARE_init(SharemindPrivateMemoryMap, inline, visibility("internal"))
SHAREMIND_MAP_DEFINE_init(SharemindPrivateMemoryMap, inline)
SHAREMIND_MAP_DECLARE_destroy(SharemindPrivateMemoryMap,
                              inline,,
                              visibility("internal"))
SHAREMIND_MAP_DEFINE_destroy(SharemindPrivateMemoryMap,
                             inline,,
                             free,
                             free((assert(v->key), v->key));)
SHAREMIND_MAP_DECLARE_get(SharemindPrivateMemoryMap,
                          inline,
                          void *,
                          visibility("internal"))
SHAREMIND_MAP_DEFINE_get(SharemindPrivateMemoryMap,
                         inline,
                         void *,
                         fnv_16a_buf(key, sizeof(void *)),
                         SHAREMIND_MAP_KEY_EQUALS_voidptr,
                         SHAREMIND_MAP_KEY_LESS_voidptr)
SHAREMIND_MAP_DECLARE_insertHint(SharemindPrivateMemoryMap,
                                 inline,
                                 void *,
                                 visibility("internal"))
SHAREMIND_MAP_DEFINE_insertHint(SharemindPrivateMemoryMap,
                                inline,
                                void *,
                                ((uintptr_t) (key)),
                                SHAREMIND_MAP_KEY_EQUALS_voidptr,
                                SHAREMIND_MAP_KEY_LESS_voidptr)
SHAREMIND_MAP_DECLARE_emplaceAtHint(SharemindPrivateMemoryMap,
                                    inline,
                                    visibility("internal"))
SHAREMIND_MAP_DEFINE_emplaceAtHint(SharemindPrivateMemoryMap, inline)
SHAREMIND_MAP_DECLARE_insertAtHint(SharemindPrivateMemoryMap,
                                   inline,
                                   void *,
                                   visibility("internal"))
SHAREMIND_MAP_DEFINE_insertAtHint(SharemindPrivateMemoryMap,
                                  inline,
                                  void *,
                                  SHAREMIND_MAP_KEY_COPY_voidptr,
                                  malloc,
                                  free)
SHAREMIND_MAP_DECLARE_insertNew(SharemindPrivateMemoryMap,
                                inline,
                                void *,
                                visibility("internal"))
SHAREMIND_MAP_DEFINE_insertNew(SharemindPrivateMemoryMap,
                               inline,
                               void *)
SHAREMIND_MAP_DECLARE_take(SharemindPrivateMemoryMap,
                           inline,
                           void *,
                           visibility("internal"))
SHAREMIND_MAP_DEFINE_take(SharemindPrivateMemoryMap,
                          inline,
                          void *,
                          ((uintptr_t) (key)),
                          SHAREMIND_MAP_KEY_EQUALS_voidptr,
                          SHAREMIND_MAP_KEY_LESS_voidptr)
SHAREMIND_MAP_DECLARE_remove(SharemindPrivateMemoryMap,
                             inline,
                             void *,
                             visibility("internal"))
SHAREMIND_MAP_DEFINE_remove(SharemindPrivateMemoryMap,
                            inline,
                            void *,
                            free,
                            free((assert(v->key), v->key));)

#endif /* SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H */
