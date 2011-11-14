/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include "api_0x1.h"
#include "modules.h"


typedef const struct {
    SMVM_Module_Error (*const module_load)(SMVM_Module * m, SMVM_SyscallMap * syscallMap);
    void (*const module_unload)(SMVM_Module * m, SMVM_SyscallMap * syscallMap);

    SMVM_Module_Error (*const module_init)(SMVM_Module * m);
    void (*const module_deinit)(SMVM_Module * m);
} API;

API apis[] = {
    { .module_load = &loadModule_0x1, .module_unload = &unloadModule_0x1,
      .module_init = &initModule_0x1, .module_deinit = &deinitModule_0x1 }
};

SM_ENUM_CUSTOM_DEFINE_TOSTRING(SMVM_Module_Error, SMVM_ENUM_Module_Error)

SMVM_Module_Error loadModule(SMVM_Module * m, const char * filename, SMVM_SyscallMap * syscallMap) {
    assert(m);
    assert(filename);

    SMVM_Module_Error status = SMVM_MOD_OK;
    const uint32_t (* modApiVersions)[];
    const char ** modName;
    const uint32_t * modVersion;
    size_t i;

    m->errorString = NULL;
    m->apiData = NULL;
    m->moduleHandle = NULL;

    m->filename = strdup(filename);
    if (!m->filename) {
        status = SMVM_MOD_OUT_OF_MEMORY;
        goto loadModule_fail_0;
    }

    /* Load module: */
    (void) dlerror();
    m->handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if (!m->handle) {
        m->errorString = dlerror();
        status = SMVM_MOD_UNABLE_TO_OPEN_MODULE;
        goto loadModule_fail_1;
    }

    /* Determine API version to use: */
    modApiVersions = (const uint32_t (*)[]) dlsym(m->handle, "SMVM_MOD_api_versions");
    if (!modApiVersions || (*modApiVersions)[0] == 0u) {
        m->errorString = dlerror();
        status = SMVM_MOD_INVALID_MODULE;
        goto loadModule_fail_2;
    }
    i = 0u;
    m->apiVersion = 0u;
    do {
        if ((*modApiVersions)[i] > m->apiVersion
            && (*modApiVersions)[i] >= SMVM_MOD_API_MIN_VERSION
            && (*modApiVersions)[i] <= SMVM_MOD_API_VERSION)
        {
            m->apiVersion = (*modApiVersions)[i];
        }
    } while ((*modApiVersions)[++i] == 0u);
    if (m->apiVersion <= 0u) {
        m->errorString = dlerror();
        status = SMVM_MOD_API_NOT_SUPPORTED;
        goto loadModule_fail_2;
    }

    /* Determine module name: */
    modName = (const char **) dlsym(m->handle, "SMVM_MOD_name");
    if (!modName) {
        m->errorString = dlerror();
        status = SMVM_MOD_INVALID_MODULE;
        goto loadModule_fail_2;
    }
    if (!*modName) {
        status = SMVM_MOD_INVALID_MODULE;
        goto loadModule_fail_2;
    }
    m->name = strdup(*modName);
    if (!m->name) {
        status = SMVM_MOD_OUT_OF_MEMORY;
        goto loadModule_fail_2;
    }

    /* Determine module version: */
    modVersion = (const uint32_t *) dlsym(m->handle, "SMVM_MOD_version");
    if (!modVersion) {
        m->errorString = dlerror();
        status = SMVM_MOD_INVALID_MODULE;
        goto loadModule_fail_3;
    }
    m->version = *modVersion;

    status = apis[m->apiVersion - 1u].module_load(m, syscallMap);
    if (status == SMVM_MOD_OK)
        return SMVM_MOD_OK;

loadModule_fail_3:

    free(m->name);

loadModule_fail_2:

    dlclose(m->handle);

loadModule_fail_1:

    free(m->filename);

loadModule_fail_0:

    return status;
}

void unloadModule(SMVM_Module * m, SMVM_SyscallMap * syscallMap) {
    apis[m->apiVersion - 1u].module_unload(m, syscallMap);
    dlclose(m->handle);
    free(m->filename);
    free(m->name);
}

SMVM_Module_Error initModule(SMVM_Module * m) {
    return apis[m->apiVersion - 1u].module_init(m);
}

void deinitModule(SMVM_Module * m) {
    apis[m->apiVersion - 1u].module_deinit(m);
}

void deinitAndUnloadModule(SMVM_Module * m, SMVM_SyscallMap * syscallMap) {
    deinitModule(m);
    unloadModule(m, syscallMap);
}
