/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_INSTRMAP_H
#define SHAREMIND_LIBVM_INSTRMAP_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/map.h>
#include <sharemind/libvmi/instr.h>
#include <stdlib.h>


typedef struct SharemindInstrMapValue_ {
    size_t blockIndex;
    size_t instructionBlockIndex;
    const SharemindVmInstruction * description;
} SharemindInstrMapValue;

SHAREMIND_MAP_DECLARE_BODY(SharemindInstrMap, size_t, SharemindInstrMapValue)
SHAREMIND_MAP_DECLARE_init(SharemindInstrMap, inline, visibility("internal"))
SHAREMIND_MAP_DEFINE_init(SharemindInstrMap, inline)
SHAREMIND_MAP_DECLARE_destroy(SharemindInstrMap,
                              inline,,
                              visibility("internal"))
SHAREMIND_MAP_DEFINE_destroy(SharemindInstrMap, inline,, free,)
SHAREMIND_MAP_DECLARE_get(SharemindInstrMap,
                          inline,
                          size_t,
                          visibility("internal"))
SHAREMIND_MAP_DEFINE_get(SharemindInstrMap,
                         inline,
                         size_t,
                         ((uint16_t) (key)),
                         SHAREMIND_MAP_KEY_EQUALS_size_t,
                         SHAREMIND_MAP_KEY_LESS_size_t)
SHAREMIND_MAP_DECLARE_insertHint(SharemindInstrMap,
                                 inline,
                                 size_t,
                                 visibility("internal"))
SHAREMIND_MAP_DEFINE_insertHint(SharemindInstrMap,
                                inline,
                                size_t,
                                ((uintptr_t) (key)),
                                SHAREMIND_MAP_KEY_EQUALS_size_t,
                                SHAREMIND_MAP_KEY_LESS_size_t)
SHAREMIND_MAP_DECLARE_emplaceAtHint(SharemindInstrMap,
                                    inline,
                                    visibility("internal"))
SHAREMIND_MAP_DEFINE_emplaceAtHint(SharemindInstrMap,
                                   inline)
SHAREMIND_MAP_DECLARE_insertAtHint(SharemindInstrMap,
                                   inline,
                                   size_t,
                                   visibility("internal"))
SHAREMIND_MAP_DEFINE_insertAtHint(SharemindInstrMap,
                                  inline,
                                  size_t,
                                  SHAREMIND_MAP_KEY_COPY_size_t,
                                  malloc,
                                  free)
SHAREMIND_MAP_DECLARE_insertNew(SharemindInstrMap,
                                inline,
                                size_t,
                                visibility("internal"))
SHAREMIND_MAP_DEFINE_insertNew(SharemindInstrMap,
                               inline,
                               size_t)
SHAREMIND_MAP_DECLARE_remove(SharemindInstrMap,
                             inline,
                             const size_t,
                             visibility("internal"))
SHAREMIND_MAP_DEFINE_remove(SharemindInstrMap,
                            inline,
                            const size_t,
                            ((uint16_t) (key)),
                            SHAREMIND_MAP_KEY_EQUALS_size_t,
                            SHAREMIND_MAP_KEY_LESS_size_t,
                            free,)

SHAREMIND_MAP_DECLARE_BODY(SharemindInstrMapP, size_t, SharemindInstrMapValue *)
SHAREMIND_MAP_DECLARE_init(SharemindInstrMapP, inline, visibility("internal"))
SHAREMIND_MAP_DEFINE_init(SharemindInstrMapP, inline)
SHAREMIND_MAP_DECLARE_destroy(SharemindInstrMapP,
                              inline,,
                              visibility("internal"))
SHAREMIND_MAP_DEFINE_destroy(SharemindInstrMapP, inline,, free,)
SHAREMIND_MAP_DECLARE_get(SharemindInstrMapP,
                          inline,
                          size_t,
                          visibility("internal"))
SHAREMIND_MAP_DEFINE_get(SharemindInstrMapP,
                         inline,
                         size_t,
                         ((uint16_t) (key)),
                         SHAREMIND_MAP_KEY_EQUALS_size_t,
                         SHAREMIND_MAP_KEY_LESS_size_t)
SHAREMIND_MAP_DECLARE_insertHint(SharemindInstrMapP,
                                 inline,
                                 size_t,
                                 visibility("internal"))
SHAREMIND_MAP_DEFINE_insertHint(SharemindInstrMapP,
                                inline,
                                size_t,
                                ((uintptr_t) (key)),
                                SHAREMIND_MAP_KEY_EQUALS_size_t,
                                SHAREMIND_MAP_KEY_LESS_size_t)
SHAREMIND_MAP_DECLARE_emplaceAtHint(SharemindInstrMapP,
                                    inline,
                                    visibility("internal"))
SHAREMIND_MAP_DEFINE_emplaceAtHint(SharemindInstrMapP, inline)
SHAREMIND_MAP_DECLARE_insertAtHint(SharemindInstrMapP,
                                   inline,
                                   size_t,
                                   visibility("internal"))
SHAREMIND_MAP_DEFINE_insertAtHint(SharemindInstrMapP,
                                  inline,
                                  size_t,
                                  SHAREMIND_MAP_KEY_COPY_size_t,
                                  malloc,
                                  free)
SHAREMIND_MAP_DECLARE_insertNew(SharemindInstrMapP,
                                inline,
                                size_t,
                                visibility("internal"))
SHAREMIND_MAP_DEFINE_insertNew(SharemindInstrMapP,
                               inline,
                               size_t)

#endif /* SHAREMIND_LIBVM_INSTRMAP_H */
