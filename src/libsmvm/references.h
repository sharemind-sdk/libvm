/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef REFERENCES_H
#define REFERENCES_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "../vector.h"


struct _SMVM_MemorySlot;
typedef struct _SMVM_MemorySlot SMVM_MemorySlot;

typedef struct {
    void * pData;
    size_t size;
    SMVM_MemorySlot * pMemory;
} SMVM_Reference;
typedef struct {
    const void * pData;
    size_t size;
    SMVM_MemorySlot * pMemory;
} SMVM_CReference;

void SMVM_Reference_destroy(SMVM_Reference * r);
void SMVM_CReference_destroy(SMVM_CReference * r);

SM_VECTOR_DECLARE(SMVM_ReferenceVector,SMVM_Reference,,inline)
SM_VECTOR_DEFINE(SMVM_ReferenceVector,SMVM_Reference,malloc,free,realloc,inline)
SM_VECTOR_DECLARE(SMVM_CReferenceVector,SMVM_CReference,,inline)
SM_VECTOR_DEFINE(SMVM_CReferenceVector,SMVM_CReference,malloc,free,realloc,inline)

#endif /* REFERENCES_H */
