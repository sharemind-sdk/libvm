/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef MODULES_H
#define MODULES_H

#include <stdlib.h>
#include "../preprocessor.h"
#include "../vector.h"


struct _SMVM_SyscallMap;

#define SMVM_MOD_API_VERSION     1u
#define SMVM_MOD_API_MIN_VERSION 1u

#define SMVM_ENUM_Module_Error \
    ((SMVM_MOD_OK, = 0)) \
    ((SMVM_MOD_OUT_OF_MEMORY,)) \
    ((SMVM_MOD_UNABLE_TO_OPEN_MODULE,)) \
    ((SMVM_MOD_INVALID_MODULE,)) \
    ((SMVM_MOD_API_NOT_SUPPORTED,)) \
    ((SMVM_MOD_API_ERROR,)) \
    ((SMVM_MOD_DUPLICATE_SYSCALL,)) \
    ((SMVM_MOD_DUPLICATE_PROTECTION_DOMAIN,)) \
    ((SMVM_MOD_MODULE_ERROR,))
SM_ENUM_CUSTOM_DEFINE(SMVM_Module_Error, SMVM_ENUM_Module_Error);
SM_ENUM_DECLARE_TOSTRING(SMVM_Module_Error);



typedef struct _SMVM_Module {
    void * handle;
    char * filename;
    char * name;
    uint32_t api_version;
    uint32_t version;

    const char * error_string;
    void * api_data;
    void * moduleHandle;
} SMVM_Module;

SM_VECTOR_DECLARE(SMVM_Modules,SMVM_Module,,inline)
SM_VECTOR_DEFINE(SMVM_Modules,SMVM_Module,malloc,free,realloc,inline)

SM_VECTOR_DECLARE_FOREACH_WITH(SMVM_Modules,SMVM_Module,syscallMap,struct _SMVM_SyscallMap *,inline)
SM_VECTOR_DEFINE_FOREACH_WITH(SMVM_Modules,SMVM_Module,syscallMap,struct _SMVM_SyscallMap *,struct _SMVM_SyscallMap * map,map,inline)

SMVM_Module_Error loadModule(SMVM_Module * m, const char * filename, struct _SMVM_SyscallMap * syscallMap);
void unloadModule(SMVM_Module * m, struct _SMVM_SyscallMap * syscallMap);

SMVM_Module_Error initModule(SMVM_Module * m);
void deinitModule(SMVM_Module * m);

void deinitAndUnloadModule(SMVM_Module * m, struct _SMVM_SyscallMap * syscallMap);
inline int _deinitAndUnloadModule(SMVM_Module * m, struct _SMVM_SyscallMap * syscallMap) {
    deinitAndUnloadModule(m, syscallMap);
    return 1;
}

inline void deinitAndUnloadModules(SMVM_Modules * ms, struct _SMVM_SyscallMap * syscallMap) {
    SMVM_Modules_foreach_with_syscallMap(ms, &_deinitAndUnloadModule, syscallMap);
    SMVM_Modules_destroy(ms);
}

#endif /* MODULES_H */
