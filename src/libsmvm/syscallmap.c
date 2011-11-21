/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "syscallmap.h"

#include <stdlib.h>
#include <string.h>
#include "../fnv.h"


static int SMVM_SyscallMap_key_equals(const char * k1, const char * k2) {
    return strcmp(k1, k2) == 0;
}

static int SMVM_SyscallMap_key_less_than(const char * k1, const char * k2) {
    return strcmp(k1, k2) < 0;
}

static int SMVM_SyscallMap_key_copy(char ** const pDest, const char * src) {
    (*pDest) = strdup(src);
    return (*pDest) != NULL;
}

SM_MAP_DEFINE(SMVM_SyscallMap,char *,const char * const,SMVM_Syscall,fnv_16a_str,SMVM_SyscallMap_key_equals,SMVM_SyscallMap_key_less_than,SMVM_SyscallMap_key_copy,free,malloc,free,)
