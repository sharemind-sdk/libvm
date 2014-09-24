/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H
#define SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <assert.h>
#include <sharemind/fnv.h>
#include <sharemind/map.h>
#include <stddef.h>
#include <stdlib.h>


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
SHAREMIND_MAP_DECLARE_remove(SharemindPrivateMemoryMap,
                             inline,
                             void *,
                             visibility("internal"))
SHAREMIND_MAP_DEFINE_remove(SharemindPrivateMemoryMap,
                            inline,
                            void *,
                            ((uintptr_t) (key)),
                            SHAREMIND_MAP_KEY_EQUALS_voidptr,
                            SHAREMIND_MAP_KEY_LESS_voidptr,
                            free,
                            free((assert(v->key), v->key));)

#endif /* SHAREMIND_LIBVM_PRIVATEMEMORYMAP_H */
