/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "vm_internal_core.h"

#include <limits.h>
#include <stddef.h>
#ifdef SMVM_SOFT_FLOAT
#include "../3rdparty/softfloat/softfloat.h"
#endif
#ifdef SMVM_DEBUG
#include <stdio.h>
#endif
#include <string.h>


#ifdef SMVM_DEBUG
#define SMVM_DEBUG_PRINTSTATE \
    if (1) { \
        SMVM_UPDATESTATE; \
        SMVM_Program_printStateBencoded(p, stderr); \
        fprintf(stderr, "\n"); \
    } else (void) 0
#define SMVM_DEBUG_PRINTINSTRUCTION(t) \
    if (1) { \
        fprintf(p->debugFileHandle, "%s\n", SM_2S(SMVM_INSTR_NAME(t))); \
    } else (void) 0
#else
#define SMVM_DEBUG_PRINTSTATE (void) 0
#define SMVM_DEBUG_PRINTINSTRUCTION(t) (void) 0
#endif

#ifndef SMVM_SOFT_FLOAT

#ifndef SMVM_NO_THREADS
#error The Sharemind VM does not yet fully support threads without softfloat.
#else
/**
  Thread-local pointer to the program being executed.
*/
static SMVM_Program * _SMVM_Program_SIGFPE_handler_p = NULL;
#endif

#ifdef __USE_POSIX
static void _SMVM_Program_SIGFPE_handler(int signalNumber,
                                         siginfo_t * signalInfo,
                                         void * context)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SMVM_Program_SIGFPE_handler(int signalNumber,
                                         siginfo_t * signalInfo,
                                         void * context)
{
    (void) signalNumber;
    (void) context;

    SMVM_HardwareExceptionType e;
    switch (signalInfo->si_code) {
        case FPE_INTDIV: e = SMVM_HET_FPE_INTDIV; break;
        case FPE_INTOVF: e = SMVM_HET_FPE_INTOVF; break;
        case FPE_FLTDIV: e = SMVM_HET_FPE_FLTDIV; break;
        case FPE_FLTOVF: e = SMVM_HET_FPE_FLTOVF; break;
        case FPE_FLTUND: e = SMVM_HET_FPE_FLTUND; break;
        case FPE_FLTRES: e = SMVM_HET_FPE_FLTRES; break;
        case FPE_FLTINV: e = SMVM_HET_FPE_FLTINV; break;
        default: e = SMVM_HET_FPE_UNKNOWN; break;
    }

    assert(_SMVM_Program_SIGFPE_handler_p);
    siglongjmp(_SMVM_Program_SIGFPE_handler_p->safeJmpBuf[e], 1);
}
#else /* __USE_POSIX */
static void _SMVM_Program_SIGFPE_handler(int signalNumber)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SMVM_Program_SIGFPE_handler(int signalNumber) {
    signal(SIGFPE, _SMVM_Program_SIGFPE_handler);
    (void) signalNumber;
    assert(_SMVM_Program_SIGFPE_handler_p);
    longjmp(_SMVM_Program_SIGFPE_handler_p->safeJmpBuf[SMVM_HET_FPE_UNKNOWN], 1);
}
#endif /* __USE_POSIX */

static inline void _SMVM_Program_setupSignalHandlers(SMVM_Program * program) {
    /* Register SIGFPE handler: */
    _SMVM_Program_SIGFPE_handler_p = program;
#ifdef __USE_POSIX
    struct sigaction newFpeAction;
    memset(&newFpeAction, '\0', sizeof(struct sigaction));
    newFpeAction.sa_sigaction = _SMVM_Program_SIGFPE_handler;
    newFpeAction.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGFPE, &newFpeAction, &program->oldFpeAction) != 0) {
#ifdef SMVM_DEBUG
        fprintf(program->debugFileHandle, "Failed to set SIGFPE signal handler!");
#endif
    }
#else /* __USE_POSIX */
    program->oldFpeAction = signal(SIGFPE, _SMVM_Program_SIGFPE_handler);
#endif /* __USE_POSIX */
}

static inline void _SMVM_Program_restoreSignalHandlers(SMVM_Program * program) {
    /* Restore old SIGFPE handler: */
#ifdef __USE_POSIX
    if (sigaction(SIGFPE, &program->oldFpeAction, NULL) != 0)
#else /* __USE_POSIX */
    if (signal(SIGFPE, program->oldFpeAction) == SIG_ERR)
#endif /* __USE_POSIX */
    {
#ifdef SMVM_DEBUG
        fprintf(program->debugFileHandle, "Failed to restore previous SIGFPE signal handler!");
#endif
#ifdef __USE_POSIX
        memset(&program->oldFpeAction, '\0', sizeof(struct sigaction));
        program->oldFpeAction.sa_handler = SIG_DFL;
        if (sigaction(SIGFPE, &program->oldFpeAction, NULL) != 0) {
#else /* __USE_POSIX */
        if (signal(SIGFPE, SIG_DFL) == SIG_ERR) {
#endif /* __USE_POSIX */
#ifdef SMVM_DEBUG
            fprintf(program->debugFileHandle, "Failed reset SIGFPE signal handler to default!");
#endif
        }
    }
    _SMVM_Program_SIGFPE_handler_p = NULL;
}

#define SMVM_NO_SF_ENCLOSE(op) \
    if(1) { \
        feclearexcept(FE_ALL_EXCEPT); \
        op; \
        int exceptions = fetestexcept(FE_ALL_EXCEPT); \
        if (unlikely(exceptions)) { \
            if (exceptions & FE_DIVBYZERO) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (exceptions & FE_OVERFLOW) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_OVERFLOW; \
            } else if (exceptions & FE_UNDERFLOW) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_UNDERFLOW; \
            } else if (exceptions & FE_INEXACT) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_INEXACT_RESULT; \
            } else if (exceptions & FE_INVALID) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SMVM_E_UNKNOWN_FPE; \
            } \
            SMVM_DO_EXCEPT; \
        } \
    } else (void) 0
#define SMVM_MI_UNEG_FLOAT32(d) SMVM_NO_SF_ENCLOSE((d) = -(d))
#define SMVM_MI_UINC_FLOAT32(d) SMVM_NO_SF_ENCLOSE((d) = (d) + 1.0)
#define SMVM_MI_UDEC_FLOAT32(d) SMVM_NO_SF_ENCLOSE((d) = (d) - 1.0)
#define SMVM_MI_BNEG_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = -(s))
#define SMVM_MI_BINC_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = (s) + 1.0)
#define SMVM_MI_BDEC_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = (s) - 1.0)
#define SMVM_MI_BADD_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) += (s))
#define SMVM_MI_BSUB_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) -= (s))
#define SMVM_MI_BSUB2_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = (s) - (d))
#define SMVM_MI_BMUL_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) *= (s))
#define SMVM_MI_BDIV_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) /= (s))
#define SMVM_MI_BDIV2_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = (s) / (d))
#define SMVM_MI_BMOD_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) %= (s))
#define SMVM_MI_BMOD2_FLOAT32(d,s) SMVM_NO_SF_ENCLOSE((d) = (s) % (d))
#define SMVM_MI_TADD_FLOAT32(d,s1,s2) SMVM_NO_SF_ENCLOSE((d) = (s1) + (s2))
#define SMVM_MI_TSUB_FLOAT32(d,s1,s2) SMVM_NO_SF_ENCLOSE((d) = (s1) - (s2))
#define SMVM_MI_TMUL_FLOAT32(d,s1,s2) SMVM_NO_SF_ENCLOSE((d) = (s1) * (s2))
#define SMVM_MI_TDIV_FLOAT32(d,s1,s2) SMVM_NO_SF_ENCLOSE((d) = (s1) / (s2))
#define SMVM_MI_TMOD_FLOAT32(d,s1,s2) SMVM_NO_SF_ENCLOSE((d) = (s1) % (s2))
#else /* #ifndef SMVM_SOFT_FLOAT */
#define SMVM_RESTORE_FPE_ENV
#define SMVM_SF_ENCLOSE(op) \
    if(1) { \
        sf_float_exception_flags = 0; \
        op; \
        if (unlikely(sf_float_exception_flags)) { \
            if (sf_float_exception_flags & sf_float_flag_divbyzero) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (sf_float_exception_flags & sf_float_flag_overflow) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_OVERFLOW; \
            } else if (sf_float_exception_flags & sf_float_flag_underflow) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_UNDERFLOW; \
            } else if (sf_float_exception_flags & sf_float_flag_inexact) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_INEXACT_RESULT; \
            } else if (sf_float_exception_flags & sf_float_flag_invalid) { \
                p->exceptionValue = SMVM_E_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SMVM_E_UNKNOWN_FPE; \
            } \
            SMVM_DO_EXCEPT; \
        } \
    } else (void) 0
#define SMVM_MI_UNEG_FLOAT32(d) SMVM_SF_ENCLOSE((d) = sf_float32_neg(d))
#define SMVM_MI_UINC_FLOAT32(d) SMVM_SF_ENCLOSE((d) = sf_float32_add((d), sf_float32_one))
#define SMVM_MI_UDEC_FLOAT32(d) SMVM_SF_ENCLOSE((d) = sf_float32_sub((d), sf_float32_one))
#define SMVM_MI_BNEG_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_neg(s))
#define SMVM_MI_BINC_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_add((s), sf_float32_one))
#define SMVM_MI_BDEC_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_sub((s), sf_float32_one))
#define SMVM_MI_BADD_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_add((d), (s)))
#define SMVM_MI_BSUB_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_sub((d), (s)))
#define SMVM_MI_BSUB2_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_sub((s), (d)))
#define SMVM_MI_BMUL_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_mul((d), (s)))
#define SMVM_MI_BDIV_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_div((d), (s)))
#define SMVM_MI_BDIV2_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_div((s), (d)))
#define SMVM_MI_BMOD_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_mod((d), (s)))
#define SMVM_MI_BMOD2_FLOAT32(d,s) SMVM_SF_ENCLOSE((d) = sf_float32_mod((s), (d)))
#define SMVM_MI_TADD_FLOAT32(d,s1,s2) SMVM_SF_ENCLOSE((d) = sf_float32_add((s1), (s2)))
#define SMVM_MI_TSUB_FLOAT32(d,s1,s2) SMVM_SF_ENCLOSE((d) = sf_float32_sub((s1), (s2)))
#define SMVM_MI_TMUL_FLOAT32(d,s1,s2) SMVM_SF_ENCLOSE((d) = sf_float32_mul((s1), (s2)))
#define SMVM_MI_TDIV_FLOAT32(d,s1,s2) SMVM_SF_ENCLOSE((d) = sf_float32_div((s1), (s2)))
#define SMVM_MI_TMOD_FLOAT32(d,s1,s2) SMVM_SF_ENCLOSE((d) = sf_float32_mod((s1), (s2)))
#endif /* #ifndef SMVM_SOFT_FLOAT */

#ifndef SMVM_FAST_BUILD
#define SMVM_DO_EXCEPT if (1) { goto except; } else (void) 0
#define SMVM_DO_HALT if (1) { goto halt; } else (void) 0
#define SMVM_DO_TRAP if (1) { goto trap; } else (void) 0
#else
typedef enum { HC_EOF, HC_EXCEPT, HC_HALT, HC_TRAP } HaltCode;
#define SMVM_DO_EXCEPT if (1) { return HC_EXCEPT; } else (void) 0
#define SMVM_DO_HALT if (1) { return HC_HALT; } else (void) 0
#define SMVM_DO_TRAP if (1) { return HC_TRAP; } else (void) 0
#endif

#define SMVM_UPDATESTATE \
    if (1) { \
        p->currentIp = (uintptr_t) (ip - codeStart); \
    } else (void) 0

/* Micro-instructions (SMVM_MI_*:) */

#define SMVM_MI_NOP (void) 0
#define SMVM_MI_HALT(r) \
    if (1) { \
        p->returnValue = (r); \
        SMVM_DO_HALT; \
    } else (void) 0
#define SMVM_MI_TRAP SMVM_DO_TRAP

#ifndef SMVM_FAST_BUILD
#define SMVM_DISPATCH(ip) goto *((ip)->p[0])
#define SMVM_MI_DISPATCH(ip) if (1) { SMVM_DISPATCH(ip); } else (void) 0
#else
#define SMVM_DISPATCH_OTHERFRAME(ip,_thisStack,_thisRefStack,_thisCRefStack) \
    ((*((HaltCode (*)(SMVM_Program * const, \
                           const SMVM_CodeBlock *, \
                           const SMVM_CodeBlock *, \
                           SMVM_RegisterVector * const, \
                           SMVM_RegisterVector *, \
                           const SMVM_ReferenceVector *, \
                           const SMVM_CReferenceVector *))((ip)->p[0])))(p,codeStart,ip,globalStack,_thisStack,_thisRefStack,_thisCRefStack))
#define SMVM_DISPATCH(ip) SMVM_DISPATCH_OTHERFRAME(ip,thisStack,thisRefStack,thisCRefStack)
#define SMVM_MI_DISPATCH(newIp) if (1) { (void) (newIp); return SMVM_DISPATCH(ip); } else (void) 0
#endif

#define SMVM_MI_JUMP_REL(n) \
    if (1) { \
        SMVM_MI_DISPATCH(ip += ((n)->int64[0])); \
    } else (void) 0

#define SMVM_MI_IS_INSTR(i) \
    SMVM_InstrSet_contains(&p->codeSections.data[p->currentCodeSectionIndex].instrset,(i))

#define SMVM_MI_CHECK_JUMP_REL(reladdr) \
    if (1) { \
        uintptr_t unsignedIp = (uintptr_t) (ip - codeStart); \
        if ((reladdr) < 0) { \
            SMVM_MI_TRY_EXCEPT(((uint64_t) -((reladdr) + 1)) < unsignedIp, \
                               SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        } else { \
            SMVM_MI_TRY_EXCEPT(((uint64_t) (reladdr)) < p->codeSections.data[p->currentCodeSectionIndex].size - unsignedIp - 1u, \
                               SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        } \
        const SMVM_CodeBlock * tip = ip + (reladdr); \
        SMVM_MI_TRY_EXCEPT(SMVM_MI_IS_INSTR((uintptr_t) (tip - codeStart)), SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        SMVM_MI_DISPATCH(ip = tip); \
    } else (void) 0

#define SMVM_MI_DO_EXCEPT(e) \
    if (1) { \
        p->exceptionValue = (e); \
        SMVM_DO_EXCEPT; \
    } else (void) 0

#define SMVM_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SMVM_MI_DO_EXCEPT((e)); \
    } else (void) 0

#define SMVM_MI_TRY_OOM(e) SMVM_MI_TRY_EXCEPT((e),SMVM_E_OUT_OF_MEMORY)

#define SMVM_MI_TRY_MEMRANGE(size,offset,numBytes,exception) \
    SMVM_MI_TRY_EXCEPT(((offset) < (size)) \
                       && ((numBytes) <= (size)) \
                       && ((size) - (offset) >= (numBytes)), \
                       (exception))

#define SMVM_MI_CHECK_CREATE_NEXT_FRAME \
    if (!SMVM_MI_HAS_STACK) { \
        p->nextFrame = SMVM_FrameStack_push(&p->frames); \
        SMVM_MI_TRY_OOM(p->nextFrame); \
        SMVM_StackFrame_init(p->nextFrame, p->thisFrame); \
    } else (void)0

#define SMVM_MI_PUSH(v) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        SMVM_CodeBlock * reg = SMVM_RegisterVector_push(&p->nextFrame->stack); \
        SMVM_MI_TRY_OOM(reg); \
        *reg = (v); \
    } else (void) 0

#define _SMVM_MI_PUSHREF_BLOCK(prefix,something,b,bOffset,rSize) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        SMVM_ ## prefix ## erence * ref = SMVM_ ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pData = (&(b)->uint8[0] + (bOffset)); \
        ref->size = (rSize); \
        ref->internal = NULL; \
    } else (void) 0

#define SMVM_MI_PUSHREF_BLOCK_ref(b)  _SMVM_MI_PUSHREF_BLOCK(Ref, refstack,  (b), 0u, sizeof(SMVM_CodeBlock))
#define SMVM_MI_PUSHREF_BLOCK_cref(b) _SMVM_MI_PUSHREF_BLOCK(CRef,crefstack, (b), 0u, sizeof(SMVM_CodeBlock))
#define SMVM_MI_PUSHREFPART_BLOCK_ref(b,o,s)  _SMVM_MI_PUSHREF_BLOCK(Ref, refstack,  (b), (o), (s))
#define SMVM_MI_PUSHREFPART_BLOCK_cref(b,o,s) _SMVM_MI_PUSHREF_BLOCK(CRef,crefstack, (b), (o), (s))

#define _SMVM_MI_PUSHREF_REF(prefix,something,constPerhaps,r,rOffset,rSize) \
    if (1) { \
        if ((r)->internal && ((SMVM_MemorySlot *) (r)->internal)->nrefs + 1u == 0u) { \
            SMVM_MI_DO_EXCEPT(SMVM_E_OUT_OF_MEMORY); \
        } \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        SMVM_ ## prefix ## erence * ref = SMVM_ ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pData = ((constPerhaps uint8_t *) (r)->pData) + (rOffset); \
        ref->size = (rSize); \
        ref->internal = (r)->internal; \
        if (ref->internal) \
            ((SMVM_MemorySlot *) ref->internal)->nrefs++; \
    } else (void) 0

#define SMVM_MI_PUSHREF_REF_ref(r)  _SMVM_MI_PUSHREF_REF(Ref, refstack,,        (r), 0u, (r)->size)
#define SMVM_MI_PUSHREF_REF_cref(r) _SMVM_MI_PUSHREF_REF(CRef,crefstack, const, (r), 0u, (r)->size)
#define SMVM_MI_PUSHREFPART_REF_ref(r,o,s)  _SMVM_MI_PUSHREF_REF(Ref, refstack,,        (r), (o), (s))
#define SMVM_MI_PUSHREFPART_REF_cref(r,o,s) _SMVM_MI_PUSHREF_REF(CRef,crefstack, const, (r), (o), (s))

#define _SMVM_MI_PUSHREF_MEM(prefix,something,slot,mOffset,rSize) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        SMVM_ ## prefix ## erence * ref = SMVM_ ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pData = ((uint8_t *) (slot)->pData) + (mOffset); \
        ref->size = (rSize); \
        ref->internal = (slot); \
        (slot)->nrefs++; \
    } else (void) 0

#define SMVM_MI_PUSHREF_MEM_ref(slot)  _SMVM_MI_PUSHREF_MEM(Ref, refstack,  (slot), 0u, (slot)->size)
#define SMVM_MI_PUSHREF_MEM_cref(slot) _SMVM_MI_PUSHREF_MEM(CRef,crefstack, (slot), 0u, (slot)->size)
#define SMVM_MI_PUSHREFPART_MEM_ref(slot,o,s)  _SMVM_MI_PUSHREF_MEM(Ref, refstack,  (slot), (o), (s))
#define SMVM_MI_PUSHREFPART_MEM_cref(slot,o,s) _SMVM_MI_PUSHREF_MEM(CRef,crefstack, (slot), (o), (s))

#define SMVM_MI_RESIZE_STACK(size) \
    SMVM_MI_TRY_OOM(SMVM_RegisterVector_resize(thisStack, (size)))

#define SMVM_MI_CLEAR_STACK \
    if (1) { \
        SMVM_RegisterVector_resize(&p->nextFrame->stack, 0u); \
        SMVM_ReferenceVector_foreach_void(&p->nextFrame->refstack, &SMVM_Reference_destroy); \
        SMVM_CReferenceVector_foreach_void(&p->nextFrame->crefstack, &SMVM_CReference_destroy); \
        SMVM_ReferenceVector_resize(&p->nextFrame->refstack, 0u); \
        SMVM_CReferenceVector_resize(&p->nextFrame->crefstack, 0u); \
    } else (void) 0

#define SMVM_MI_HAS_STACK (!!(p->nextFrame))

#ifndef SMVM_FAST_BUILD
#define SMVM_CALL_RETURN_DISPATCH(ip) \
    if (1) { \
        thisStack = &p->thisFrame->stack; \
        thisRefStack = &p->thisFrame->refstack; \
        thisCRefStack = &p->thisFrame->crefstack; \
        SMVM_DISPATCH((ip)); \
    } else (void) 0
#else
#define SMVM_CALL_RETURN_DISPATCH(ip) return SMVM_DISPATCH_OTHERFRAME((ip), &p->thisFrame->stack, &p->thisFrame->refstack, &p->thisFrame->crefstack)
#endif

#define SMVM_MI_CALL(a,r,nargs) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        ip = codeStart + (a); \
        SMVM_CALL_RETURN_DISPATCH(ip); \
    } else (void) 0

#define SMVM_MI_CHECK_CALL(a,r,nargs) \
    if (1) { \
        SMVM_MI_TRY_EXCEPT(SMVM_MI_IS_INSTR((a)->uint64[0]), SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        SMVM_MI_CALL((a)->uint64[0],r,nargs); \
    } else (void) 0

#define SMVM_MI_SYSCALL(sc,r,nargs) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        SMVM_MODAPI_0x1_Syscall rc = (SMVM_MODAPI_0x1_Syscall) ((const SMVM_Context_Syscall *) sc)->impl_or_wrapper; \
        p->syscallContext.moduleHandle = ((const SMVM_Context_Syscall *) sc)->moduleHandle; \
        p->syscallContextInternal.syscall = (const SMVM_Context_Syscall *) sc; \
        SMVM_MODAPI_0x1_Syscall_Code st; \
        st = (*rc)(p->nextFrame->stack.data, p->nextFrame->stack.size, \
                   p->nextFrame->refstack.data, p->nextFrame->refstack.size, \
                   p->nextFrame->crefstack.data, p->nextFrame->crefstack.size, \
                   (r), &p->syscallContext); \
        SMVM_StackFrame_destroy(SMVM_FrameStack_top(&p->frames)); \
        SMVM_FrameStack_pop(&p->frames); \
        p->nextFrame = NULL; \
        switch (st) { \
            case SMVM_MODAPI_0x1_SC_OK: \
                break; \
            case SMVM_MODAPI_0x1_SC_OUT_OF_MEMORY: \
                SMVM_MI_DO_EXCEPT(SMVM_E_OUT_OF_MEMORY); \
                break; \
            case SMVM_MODAPI_0x1_SC_INVALID_CALL: \
                SMVM_MI_DO_EXCEPT(SMVM_E_INVALID_SYSCALL_INVOCATION); \
                break; \
            case SMVM_MODAPI_0x1_SC_GENERAL_FAILURE: /* Fall through: */ \
            default: \
                SMVM_MI_DO_EXCEPT(SMVM_E_SYSCALL_FAILURE); \
                break; \
        } \
    } else (void) 0

#define SMVM_MI_CHECK_SYSCALL(a,r,nargs) \
    if (1) { \
        SMVM_MI_TRY_EXCEPT((a)->uint64[0] < p->bindings.size, \
                           SMVM_E_INVALID_INDEX_SYSCALL); \
        SMVM_MI_SYSCALL(p->bindings.data[(size_t) (a)->uint64[0]],r,nargs); \
    } else (void) 0

#define SMVM_MI_RETURN(r) \
    if (1) { \
        if (unlikely(p->nextFrame)) { \
            SMVM_StackFrame_destroy(SMVM_FrameStack_top(&p->frames)); \
            SMVM_FrameStack_pop(&p->frames); \
            p->nextFrame = NULL; \
        } \
        if (likely(p->thisFrame->returnAddr)) { \
            if (p->thisFrame->returnValueAddr) \
                *p->thisFrame->returnValueAddr = (r); \
            ip = p->thisFrame->returnAddr; \
            p->thisFrame = p->thisFrame->prev; \
            SMVM_FrameStack_pop(&p->frames); \
            SMVM_CALL_RETURN_DISPATCH(ip); \
        } else { \
            SMVM_MI_HALT((r)); \
        } \
    } else (void) 0

#define SMVM_MI_GET_T_reg(d,t,i) \
    if (1) { \
        const SMVM_CodeBlock * r = SMVM_RegisterVector_get_const_pointer(globalStack, (i)); \
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_INDEX_REGISTER); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SMVM_MI_GET_T_stack(d,t,i) \
    if (1) { \
        const SMVM_CodeBlock * r = SMVM_RegisterVector_get_const_pointer(thisStack, (i)); \
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_INDEX_STACK); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define _SMVM_MI_GET(d,i,fromwhere,exception,isconst) \
    if (1) { \
        (d) = SMVM_RegisterVector_get_ ## isconst ## pointer((fromwhere), (i)); \
        SMVM_MI_TRY_EXCEPT((d),(exception)); \
    } else (void) 0

#define SMVM_MI_GET_stack(d,i) _SMVM_MI_GET((d),(i),thisStack,SMVM_E_INVALID_INDEX_STACK,)
#define SMVM_MI_GET_reg(d,i)   _SMVM_MI_GET((d),(i),globalStack,SMVM_E_INVALID_INDEX_REGISTER,)
#define SMVM_MI_GET_CONST_stack(d,i) _SMVM_MI_GET((d),(i),thisStack,SMVM_E_INVALID_INDEX_STACK,const_)
#define SMVM_MI_GET_CONST_reg(d,i)   _SMVM_MI_GET((d),(i),globalStack,SMVM_E_INVALID_INDEX_REGISTER,const_)

#define _SMVM_MI_GET_REF(r,i,type,exception) \
    if (1) { \
        (r) = SMVM_ ## type ## erenceVector_get_const_pointer(this ## type ## Stack, (i)); \
        SMVM_MI_TRY_EXCEPT((r),(exception)); \
    } else (void) 0

#define SMVM_MI_GET_ref(r,i) _SMVM_MI_GET_REF((r),(i),Ref,SMVM_E_INVALID_INDEX_REFERENCE)
#define SMVM_MI_GET_cref(r,i) _SMVM_MI_GET_REF((r),(i),CRef,SMVM_E_INVALID_INDEX_CONST_REFERENCE)

#define SMVM_MI_REFERENCE_GET_MEMORY_PTR(r) ((SMVM_MemorySlot *) (r)->internal)
#define SMVM_MI_REFERENCE_GET_PTR(r) ((uint8_t *) (r)->pData)
#define SMVM_MI_REFERENCE_GET_CONST_PTR(r) ((const uint8_t *) (r)->pData)
#define SMVM_MI_REFERENCE_GET_SIZE(r) ((r)->size)

#define SMVM_MI_REF_CAN_READ(ref) ((SMVM_MemorySlot *) ref->internal == NULL || SMVM_MI_MEM_CAN_READ((SMVM_MemorySlot *) ref->internal))
#define SMVM_MI_REF_CAN_WRITE(ref) ((SMVM_MemorySlot *) ref->internal == NULL || SMVM_MI_MEM_CAN_WRITE((SMVM_MemorySlot *) ref->internal))

#define SMVM_MI_BLOCK_AS(b,t) (b->t[0])
#define SMVM_MI_BLOCK_AS_P(b,t) (&b->t[0])
#define SMVM_MI_ARG(n)        (* SMVM_MI_ARG_P(n))
#define SMVM_MI_ARG_P(n)      (ip + (n))
#define SMVM_MI_ARG_AS(n,t)   (SMVM_MI_BLOCK_AS(SMVM_MI_ARG_P(n),t))
#define SMVM_MI_ARG_AS_P(n,t) (& SMVM_MI_BLOCK_AS(SMVM_MI_ARG_P(n), t))

#ifndef SMVM_SOFT_FLOAT
#define SMVM_MI_CONVERT_float32_TO_int8(a,b) a = (int8_t) b
#define SMVM_MI_CONVERT_float32_TO_int16(a,b) a = (int16_t) b
#define SMVM_MI_CONVERT_float32_TO_int32(a,b) a = (int32_t) b
#define SMVM_MI_CONVERT_float32_TO_int64(a,b) a = (int64_t) b
#define SMVM_MI_CONVERT_float32_TO_uint8(a,b) a = (uint8_t) b
#define SMVM_MI_CONVERT_float32_TO_uint16(a,b) a = (uint16_t) b
#define SMVM_MI_CONVERT_float32_TO_uint32(a,b) a = (uint32_t) b
#define SMVM_MI_CONVERT_float32_TO_uint64(a,b) a = (uint64_t) b
#define SMVM_MI_CONVERT_int8_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_int16_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_int32_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_int64_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_uint8_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_uint16_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_uint32_TO_float32(a,b) a = (float) b
#define SMVM_MI_CONVERT_uint64_TO_float32(a,b) a = (float) b
#else
#define SMVM_MI_CONVERT_float32_TO_int8(a,b)   a = (int8_t)   sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_int16(a,b)  a = (int16_t)  sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_int32(a,b)  a = (int32_t)  sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_int64(a,b)  a = (int64_t)  sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_uint8(a,b)  a = (uint8_t)  sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_uint16(a,b) a = (uint16_t) sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_uint32(a,b) a = (uint32_t) sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_float32_TO_uint64(a,b) a = (uint64_t) sf_float32_to_int64(b)
#define SMVM_MI_CONVERT_int8_TO_float32(a,b)   a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_int16_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_int32_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_int64_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_uint8_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_uint16_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_uint32_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#define SMVM_MI_CONVERT_uint64_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#endif

#define SMVM_MI_MEM_CAN_READ(slot) ((slot)->specials == NULL || (slot)->specials->readable)
#define SMVM_MI_MEM_CAN_WRITE(slot) ((slot)->specials == NULL || (slot)->specials->writeable)

#define SMVM_MEM_ALLOC_REF(dref,slot) \
    if (1) { \
        const uint64_t startFrom = p->memorySlotNext; \
        for (;;) { \
            (slot) = SMVM_MemoryMap_get(&p->memoryMap, p->memorySlotNext); \
            if (!(slot)) { \
                (dref) = p->memorySlotNext; \
                p->memorySlotNext++; \
                if (p->memorySlotNext == 0) \
                    p->memorySlotNext++; \
                break; /* LOOP EXIT 1 */ \
            } else { \
                p->memorySlotNext++; \
                if (p->memorySlotNext == 0) \
                    p->memorySlotNext++; \
                if (p->memorySlotNext == startFrom) { \
                    (dref) = 0; /* NULL */ \
                    break; /* LOOP EXIT 2 */ \
                } \
            } \
        } \
    } (void) 0

#define SMVM_MI_MEM_ALLOC(dptr,sizereg) \
    if (1) { \
        if (p->memorySlotsUsed >= UINT64_MAX \
            || (sizereg)->uint64[0] == 0u \
            || (sizereg)->uint64[0] > SIZE_MAX) \
        { \
            (dptr)->uint64[0] = 0u; /* NULL */ \
        } else { \
            size_t dataSize = (size_t) (sizereg)->uint64[0]; /* SIZE_MAX == UINT64_MAX */ \
            void * pData = malloc(dataSize); \
            if (!pData) { \
                (dptr)->uint64[0] = 0u; /* NULL */ \
            } else { \
                SMVM_MemorySlot * slot; \
                SMVM_MEM_ALLOC_REF((dptr)->uint64[0],slot); \
                if (slot) { /* LOOP EXIT 2 */ \
                    free(pData); \
                } else { /* LOOP EXIT 1 */ \
                    slot = SMVM_MemoryMap_insert(&p->memoryMap, (dptr)->uint64[0]); \
                    if (!slot) { \
                        free(pData); \
                        (dptr)->uint64[0] = 0u; /* NULL */ \
                    } else { \
                        assert(p->memorySlotsUsed < UINT64_MAX); \
                        p->memorySlotsUsed++; \
                        SMVM_MemorySlot_init(slot, pData, dataSize, NULL); \
                    } \
                } \
            } \
        } \
    } else (void) 0

#define SMVM_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    if (1) { \
        (dest) = SMVM_MemoryMap_get(&p->memoryMap, (index)); \
        SMVM_MI_TRY_EXCEPT((dest),SMVM_E_INVALID_REFERENCE); \
    } else (void) 0

/* The following can be optimized: */
#define SMVM_MI_MEM_FREE(ptr) \
    if (1) { \
        SMVM_MemorySlot * slot; \
        SMVM_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        SMVM_MI_TRY_EXCEPT(slot->nrefs == 0u, SMVM_E_MEMORY_IN_USE); \
        SMVM_MemorySlot_destroy(slot); \
        SMVM_MemoryMap_remove(&p->memoryMap, (ptr)->uint64[0]); \
        assert(p->memorySlotsUsed > 0); \
        p->memorySlotsUsed--; \
    } else (void) 0

#define SMVM_MI_MEM_GET_SIZE(ptr,sizedest) \
    if (1) { \
        SMVM_MemorySlot * slot; \
        SMVM_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        (sizedest)->uint64[0] = slot->size; \
    } else (void) 0

#define _SMVM_MI_MEM_GETSEGMENT(ptrDest,sectionContainer) \
    if (1) { \
        SMVM_MemorySlot * slot; \
        SMVM_MEM_ALLOC_REF((ptrDest)->uint64[0],slot); \
        if (!slot) { /* LOOP EXIT 1 */ \
            slot = SMVM_MemoryMap_insert(&p->memoryMap, (ptrDest)->uint64[0]); \
            if (!slot) { \
                (ptrDest)->uint64[0] = 0u; /* NULL */ \
            } else { \
                assert(p->memorySlotsUsed < UINT64_MAX); \
                p->memorySlotsUsed++; \
                const SMVM_DataSection * const restrict staticSlot = SMVM_DataSectionsVector_get_const_pointer((sectionContainer),p->currentCodeSectionIndex); \
                slot->pData = staticSlot->pData; \
                slot->size = staticSlot->size; \
                slot->nrefs = 0u; /* We CAN special-free this */ \
                slot->specials = staticSlot->specials; \
            } \
        } \
    } else (void) 0
#define SMVM_MI_MEM_GETSEGMENT_bss(ptrDest) _SMVM_MI_MEM_GETSEGMENT((ptrDest), &p->bssSections)
#define SMVM_MI_MEM_GETSEGMENT_rodata(ptrDest) _SMVM_MI_MEM_GETSEGMENT((ptrDest), &p->rodataSections)
#define SMVM_MI_MEM_GETSEGMENT_data(ptrDest) _SMVM_MI_MEM_GETSEGMENT((ptrDest), &p->dataSections)


#define SMVM_MI_MEMCPY memcpy
#define SMVM_MI_MEMMOVE memmove

#ifndef SMVM_FAST_BUILD
#define SMVM_IMPL(name,code) \
    label_impl_ ## name : code
#else
#define SMVM_IMPL_INNER(name,code) \
    static inline HaltCode name ( \
        SMVM_Program * const p, \
        const SMVM_CodeBlock * const codeStart, \
        const SMVM_CodeBlock * ip, \
        SMVM_RegisterVector * const globalStack, \
        SMVM_RegisterVector * thisStack, \
        const SMVM_ReferenceVector * thisRefStack, \
        const SMVM_CReferenceVector * thisCRefStack) \
    { \
        (void) codeStart; (void) globalStack; (void) thisStack; \
        (void) thisRefStack; (void) thisCRefStack; \
        SMVM_UPDATESTATE; \
        code \
    }
#define SMVM_IMPL(name,code) SMVM_IMPL_INNER(func_impl_ ## name, code)

#include "../m4/dispatches.h"

SMVM_IMPL_INNER(_func_impl_eof,return HC_EOF;)
SMVM_IMPL_INNER(_func_impl_except,return HC_EXCEPT;)
SMVM_IMPL_INNER(_func_impl_halt,return HC_HALT;)
SMVM_IMPL_INNER(_func_impl_trap,return HC_TRAP;)

#endif

SMVM_Error _SMVM(SMVM_Program * const p,
                 const SMVM_InnerCommand _SMVM_command,
                 void * const _SMVM_data)
{
    if (_SMVM_command == SMVM_I_GET_IMPL_LABEL) {

#ifndef SMVM_FAST_BUILD
#define SMVM_IMPL_LABEL(name) && label_impl_ ## name ,
#include "../m4/static_label_structs.h"
        static void * system_labels[] = { && eof, && except, && halt, && trap };
#else
#define SMVM_IMPL_LABEL(name) & func_impl_ ## name ,
#include "../m4/static_label_structs.h"
        static void * system_labels[] = { &_func_impl_eof, &_func_impl_except, &_func_impl_halt, &_func_impl_trap };
#endif
        // static void *(*labels[3])[] = { &instr_labels, &empty_impl_labels, &system_labels };

        SMVM_Prepare_IBlock * pb = (SMVM_Prepare_IBlock *) _SMVM_data;
        switch (pb->type) {
            case 0:
                pb->block->p[0] = instr_labels[pb->block->uint64[0]];
                break;
            case 1:
                pb->block->p[0] = empty_impl_labels[pb->block->uint64[0]];
                break;
            case 2:
                pb->block->p[0] = system_labels[pb->block->uint64[0]];
                break;
            default:
                abort();
        }
        return SMVM_OK;

    } else if (_SMVM_command == SMVM_I_RUN || _SMVM_command == SMVM_I_CONTINUE) {

#pragma STDC FENV_ACCESS ON

        const SMVM_CodeBlock * const codeStart = p->codeSections.data[p->currentCodeSectionIndex].data;
        const SMVM_CodeBlock * ip = &codeStart[p->currentIp];
        SMVM_RegisterVector * const globalStack = &p->globalFrame->stack;
        SMVM_RegisterVector * thisStack = &p->thisFrame->stack;
        SMVM_ReferenceVector * thisRefStack = &p->thisFrame->refstack;
        SMVM_CReferenceVector * thisCRefStack = &p->thisFrame->crefstack;

#ifndef SMVM_SOFT_FLOAT
        p->hasSavedFpeEnv = (fegetenv(&p->savedFpeEnv) == 0);

        /**
          \warning Note that the variable "ip" is not valid after one of the
                   following sigsetjmp's returns non-zero unless we declare it
                   volatile. However, declaring "ip" as volatile has a negative
                   performance impact. A remedy would be to update p->currentIp
                   before every instruction which could generate such signals.
        */
#ifdef __USE_POSIX
    #define _SMVM_DECLARE_SIGJMP(h,e) \
            if (sigsetjmp(p->safeJmpBuf[(h)], 1)) { \
                p->exceptionValue = (e); \
                SMVM_DO_EXCEPT; \
            } else (void) 0
#else
    #define _SMVM_DECLARE_SIGJMP(h,e) \
            if (setjmp(p->safeJmpBuf[(h)])) { \
                p->exceptionValue = (e); \
                SMVM_DO_EXCEPT; \
            } else (void) 0
#endif
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_UNKNOWN, SMVM_E_UNKNOWN_FPE);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_INTDIV,  SMVM_E_INTEGER_DIVIDE_BY_ZERO);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_INTOVF,  SMVM_E_INTEGER_OVERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTDIV,  SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTOVF,  SMVM_E_FLOATING_POINT_OVERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTUND,  SMVM_E_FLOATING_POINT_UNDERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTRES,  SMVM_E_FLOATING_POINT_INEXACT_RESULT);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTINV,  SMVM_E_FLOATING_POINT_INVALID_OPERATION);

        _SMVM_Program_setupSignalHandlers(p);
#endif /* #ifndef SMVM_SOFT_FLOAT */

#ifndef SMVM_FAST_BUILD
        SMVM_MI_DISPATCH(ip);

        #include "../m4/dispatches.h"
#else
        switch (SMVM_DISPATCH(ip)) {
            case HC_EOF:
                goto eof;
            case HC_EXCEPT:
                goto except;
            case HC_HALT:
                goto halt;
            case HC_TRAP:
                goto trap;
            default:
                abort();
        }
#endif

        eof:
            p->exceptionValue = SMVM_E_JUMP_TO_INVALID_ADDRESS;

        except:
            assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
            assert(p->exceptionValue != SMVM_E_NONE);
            assert(SMVM_Exception_toString((SMVM_Exception) p->exceptionValue) != 0);
#ifndef SMVM_SOFT_FLOAT
            _SMVM_Program_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
            p->state = SMVM_CRASHED;
#ifndef SMVM_FAST_BUILD
            SMVM_UPDATESTATE;
#endif
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_RUNTIME_EXCEPTION;

        halt:
#ifndef SMVM_SOFT_FLOAT
            _SMVM_Program_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
#ifndef SMVM_FAST_BUILD
            SMVM_UPDATESTATE;
#endif
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_OK;

        trap:
#ifndef SMVM_SOFT_FLOAT
            _SMVM_Program_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
            p->state = SMVM_TRAPPED;
#ifndef SMVM_FAST_BUILD
            SMVM_UPDATESTATE;
#endif
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_RUNTIME_TRAP;
    } else {
        abort();
    }
}
