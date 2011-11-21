/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_MODULES_CXX_H
#define SHAREMIND_LIBSMVM_MODULES_CXX_H

#include "modules.h"


class SMVM_Module_CXX  {

public: /* Methods: */

    inline SMVM_Module_CXX(const char * const fname,
                           SMVM_Module_Error * loadError)
    {
        SMVM_Module_Error e = SMVM_Module_load(&m_mod, fname);
        if (loadError)
            *loadError = e;
    }

    inline ~SMVM_Module_CXX(void) { SMVM_Module_unload(&m_mod); }

    inline SMVM_Module_Error init(void) { return SMVM_Module_mod_init(&m_mod); }
    inline void deinit(void) { SMVM_Module_mod_deinit(&m_mod); }

    void * handle() { return m_mod.handle; }
    const void * handle() const { return m_mod.handle; }
    const char * name() const { return m_mod.name; }
    const char * filename() const { return m_mod.filename; }
    uint32_t apiVersion() const { return m_mod.apiVersion; }
    uint32_t version() const { return m_mod.version; }
    const char * errorString() const { return m_mod.errorString; }
    void * apiData() { return m_mod.apiData; }
    const void * apiData() const { return m_mod.apiData; }
    void * moduleHandle() { return m_mod.moduleHandle; }
    const void * moduleHandle() const { return m_mod.moduleHandle; }
    bool isInitialized() const { return m_mod.isInitialized; }

    const SMVM_Syscall * getSyscall(const char * signature) const {
        return SMVM_Module_get_syscall(&m_mod, signature);
    }

private: /* Fields: */

    SMVM_Module m_mod;

};

#endif // SHAREMIND_LIBSMVM_MODULES_CXX_H
