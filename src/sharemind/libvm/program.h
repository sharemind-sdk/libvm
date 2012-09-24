/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PROGRAM_H
#define SHAREMIND_LIBVM_PROGRAM_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif


#include <sharemind/refs.h>
#include "codesectionsvector.h"
#include "datasectionsvector.h"
#include "datasectionsizesvector.h"
#include "pdbindingsvector.h"
#include "syscallbindingsvector.h"
#include "vm.h"


#ifdef __cplusplus
extern "C" {
#endif


/*******************************************************************************
 *  SharemindProgram
********************************************************************************/

#ifndef SHAREMIND_SOFT_FLOAT
typedef enum {
    SHAREMIND_HET_FPE_UNKNOWN = 0x00, /* Unknown arithmetic or floating-point error */
    SHAREMIND_HET_FPE_INTDIV  = 0x01, /* integer divide by zero */
    SHAREMIND_HET_FPE_INTOVF  = 0x02, /* integer overflow */
    SHAREMIND_HET_FPE_FLTDIV  = 0x03, /* floating-point divide by zero */
    SHAREMIND_HET_FPE_FLTOVF  = 0x04, /* floating-point overflow */
    SHAREMIND_HET_FPE_FLTUND  = 0x05, /* floating-point underflow */
    SHAREMIND_HET_FPE_FLTRES  = 0x06, /* floating-point inexact result */
    SHAREMIND_HET_FPE_FLTINV  = 0x07, /* floating-point invalid operation */
    SHAREMIND_HET_COUNT
} SharemindHardwareExceptionType;
#endif


struct SharemindProgram_ {
    bool ready;
    SharemindExecutionStyle executionStyle;

    SharemindVmError error;

    SharemindCodeSectionsVector codeSections;
    SharemindDataSectionsVector rodataSections;
    SharemindDataSectionsVector dataSections;
    SharemindDataSectionSizesVector bssSectionSizes;

    SharemindSyscallBindingsVector bindings;
    SharemindPdBindings pdBindings;

    size_t activeLinkingUnit;

    size_t prepareCodeSectionIndex;
    uintptr_t prepareIp;

    SharemindVm * vm;

    SHAREMIND_REFS_DECLARE_FIELDS

};

SHAREMIND_REFS_DECLARE_FUNCTIONS(SharemindProgram)


#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
