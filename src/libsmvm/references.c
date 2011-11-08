/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "references.h"

#include "vm_internal_helpers.h"


void SMVM_Reference_destroy(SMVM_Reference * r) {
    if (r->pMemory)
        r->pMemory->nrefs--;
}

void SMVM_CReference_destroy(SMVM_CReference * r) {
    if (r->pMemory)
        r->pMemory->nrefs--;
}
