/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBSMVM_API_0x1_H
#define SHAREMIND_LIBSMVM_API_0x1_H

#include "modules.h"


#ifdef __cplusplus
extern "C" {
#endif


SMVM_Module_Error loadModule_0x1(SMVM_Module * m);
void unloadModule_0x1(SMVM_Module * m);

SMVM_Module_Error initModule_0x1(SMVM_Module * m);
void deinitModule_0x1(SMVM_Module * m);

size_t getNumSyscalls_0x1(const SMVM_Module * m);
const SMVM_Syscall * getSyscall_0x1(const SMVM_Module * m, size_t index);
const SMVM_Syscall * findSyscall_0x1(const SMVM_Module * m, const char * signature);


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBSMVM_API_0x1_H */
