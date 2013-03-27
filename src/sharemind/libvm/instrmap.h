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


#ifdef __cplusplus
extern "C" {
#endif

typedef struct SharemindInstrMapValue_ {
    size_t blockIndex;
    size_t instructionBlockIndex;
    const SharemindVmInstruction * description;
} SharemindInstrMapValue;

SHAREMIND_MAP_DECLARE(SharemindInstrMap,size_t,const size_t,SharemindInstrMapValue,static inline)
SHAREMIND_MAP_DEFINE(SharemindInstrMap,size_t,const size_t,SharemindInstrMapValue,key,SHAREMIND_MAP_KEY_EQUALS_size_t,SHAREMIND_MAP_KEY_LESS_THAN_size_t,SHAREMIND_MAP_KEYINIT_REGULAR,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,static inline)

SHAREMIND_MAP_DECLARE(SharemindInstrMapP,size_t,const size_t,SharemindInstrMapValue *,static inline)
SHAREMIND_MAP_DEFINE(SharemindInstrMapP,size_t,const size_t,SharemindInstrMapValue *,key,SHAREMIND_MAP_KEY_EQUALS_size_t,SHAREMIND_MAP_KEY_LESS_THAN_size_t,SHAREMIND_MAP_KEYINIT_REGULAR,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,static inline)

#ifdef __cplusplus
} /* extern "C" { */
#endif


#endif /* SHAREMIND_LIBVM_INSTRMAP_H */
