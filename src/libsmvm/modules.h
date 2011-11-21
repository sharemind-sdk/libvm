/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_MODULES_H
#define SHAREMIND_LIBSMVM_MODULES_H

#include <stdlib.h>
#include "../preprocessor.h"
#include "../vector.h"
#include "syscall.h"


#ifdef __cplusplus
extern "C" {
#endif


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
    uint32_t apiVersion;
    uint32_t version;

    const char * errorString;
    void * apiData;
    void * moduleHandle;

    int isInitialized;
} SMVM_Module;

SMVM_Module_Error SMVM_Module_load(SMVM_Module * m, const char * filename);
void SMVM_Module_unload(SMVM_Module * m);

SMVM_Module_Error SMVM_Module_mod_init(SMVM_Module * m);
void SMVM_Module_mod_deinit(SMVM_Module * m);

const SMVM_Syscall * SMVM_Module_get_syscall(const SMVM_Module * m, const char * signature);


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_MODULES_H */
