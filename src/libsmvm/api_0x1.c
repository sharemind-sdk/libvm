/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "api_0x1.h"

#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include "../modapi_0x1.h"
#include "../vector.h"


/* Functions for fast union of disjoint syscall maps: */
static int disjoint_syscallmap_inserter(const char * const * key, SMVM_Syscall * value, SMVM_SyscallMap * dest) {
    assert(!SMVM_SyscallMap_get(dest, key));
    SMVM_Syscall * sc = SMVM_SyscallMap_insert(dest, *key);
    return SMVM_Syscall_copy(sc, value) != NULL;
}
static int disjoint_syscallmap_subtractor(const char * const * key, SMVM_Syscall * value, SMVM_SyscallMap * from) {
    (void) value;
    return SMVM_SyscallMap_remove_with(from, *key, &SMVM_Syscall_destroy);
}
static void SMVM_SyscallMap_destroyer(const char * const * key, SMVM_Syscall * value) {
    (void) key;
    SMVM_Syscall_destroy(value);
}
int SMVM_SyscallMap_unite(SMVM_SyscallMap * dest, SMVM_SyscallMap * src) {
    if (SMVM_SyscallMap_foreach_with_syscallMap(src, &disjoint_syscallmap_inserter, dest))
        return 1;
    (void) SMVM_SyscallMap_foreach_with_syscallMap(src, &disjoint_syscallmap_subtractor, dest); /* Rollback: */
    return 0;
}

/** \todo Protection domains */


/* API 0x1 data */

typedef struct {
    SMVM_MODAPI_0x1_Module_Context context;
    SMVM_MODAPI_0x1_Module_Initializer initializer;
    SMVM_MODAPI_0x1_Module_Deinitializer deinitializer;
    SMVM_SyscallMap syscalls;
} ApiData;


SMVM_Module_Error loadModule_0x1(SMVM_Module * m, SMVM_SyscallMap * syscallMap) {
    assert(m);

    SMVM_Module_Error status;
    SMVM_MODAPI_0x1_Syscall_Definition (*scs)[];

    ApiData * apiData;
    SMVM_MODAPI_0x1_Module_Initializer initializer;
    SMVM_MODAPI_0x1_Module_Deinitializer deinitializer;

    apiData = (ApiData *) malloc(sizeof(ApiData));
    if (!apiData) {
        status = SMVM_MOD_OUT_OF_MEMORY;
        goto loadModule_0x1_fail_0;
    }

    /* Handle loader function: */
    initializer = (SMVM_MODAPI_0x1_Module_Initializer) dlsym(m->handle, "SMVM_MOD_0x1_init");
    if (!initializer) {
        status = SMVM_MOD_API_ERROR;
        goto loadModule_0x1_fail_1;
    }
    apiData->initializer = initializer;

    /* Handle unloader function: */
    deinitializer = (SMVM_MODAPI_0x1_Module_Deinitializer) dlsym(m->handle, "SMVM_MOD_0x1_deinit");
    if (!deinitializer) {
        status = SMVM_MOD_API_ERROR;
        goto loadModule_0x1_fail_1;
    }
    apiData->deinitializer = deinitializer;

    /* Handle system call definitions: */
    SMVM_SyscallMap_init(&apiData->syscalls);
    scs = (SMVM_MODAPI_0x1_Syscall_Definition (*)[]) dlsym(m->handle, "SMVM_MOD_0x1_syscalls");
    if (scs) {
        size_t i = 0u;
        while ((*scs)[i].name) {
            SMVM_Syscall * sc;

            if (!(*scs)[i].syscall_f) {
                status = SMVM_MOD_API_ERROR;
                goto loadModule_0x1_fail_2;
            }

            if (SMVM_SyscallMap_get(&apiData->syscalls, (*scs)[i].name)
                || SMVM_SyscallMap_get(syscallMap, (*scs)[i].name))
            {
                status = SMVM_MOD_DUPLICATE_SYSCALL;
                goto loadModule_0x1_fail_2;
            }
            sc = SMVM_SyscallMap_insert(&apiData->syscalls, (*scs)[i].name);
            if (!sc) {
                status = SMVM_MOD_OUT_OF_MEMORY;
                goto loadModule_0x1_fail_2;
            }
            if (!SMVM_Syscall_init(sc, (*scs)[i].name, (*scs)[i].syscall_f, NULL, m)) {
                status = SMVM_MOD_OUT_OF_MEMORY;
                int r = SMVM_SyscallMap_remove(&apiData->syscalls, (*scs)[i].name);
                assert(r == 1); (void) r;
                goto loadModule_0x1_fail_2;
            }

            i++;
        }
        if ((*scs)[i].syscall_f) {
            status = SMVM_MOD_API_ERROR;
            goto loadModule_0x1_fail_2;
        }
    }

    /** \todo Handle protection domain definitions: */

    if (!SMVM_SyscallMap_unite(syscallMap, &apiData->syscalls)) {
        status = SMVM_MOD_OUT_OF_MEMORY;
        goto loadModule_0x1_fail_2;
    }

    m->apiData = (void *) apiData;

    return SMVM_MOD_OK;

loadModule_0x1_fail_2:

    SMVM_SyscallMap_destroy_with(&apiData->syscalls, &SMVM_SyscallMap_destroyer);

loadModule_0x1_fail_1:

    free(apiData);

loadModule_0x1_fail_0:

    return status;
}

void unloadModule_0x1(SMVM_Module * const m, SMVM_SyscallMap * syscallMap) {
    assert(m);
    assert(m->apiData);

    ApiData * const apiData = (ApiData *) m->apiData;

    int r = SMVM_SyscallMap_foreach_with_syscallMap(&apiData->syscalls, &disjoint_syscallmap_subtractor, syscallMap);
    assert(r); (void) r;

    SMVM_SyscallMap_destroy_with(&apiData->syscalls, &SMVM_SyscallMap_destroyer);
    free(apiData);
}

SMVM_Module_Error initModule_0x1(SMVM_Module * const m) {
    ApiData * const data = (ApiData *) m->apiData;

    SMVM_MODAPI_0x1_Initializer_Code r = data->initializer(&data->context);
    switch (r) {
        case SMVM_MODAPI_0x1_IC_OK:
            m->moduleHandle = data->context.moduleHandle;
            return SMVM_MOD_OK;
        case SMVM_MODAPI_0x1_IC_OUT_OF_MEMORY:
            return SMVM_MOD_OUT_OF_MEMORY;
        case SMVM_MODAPI_0x1_IC_ERROR:
            return SMVM_MOD_MODULE_ERROR;
        default:
            return SMVM_MOD_API_ERROR;
    }
}

void deinitModule_0x1(SMVM_Module * const m) {
    ApiData * const data = (ApiData *) m->apiData;

    data->deinitializer(&data->context);
}
