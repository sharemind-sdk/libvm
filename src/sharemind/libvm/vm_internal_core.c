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
#ifdef SHAREMIND_SOFT_FLOAT
#include <sharemind/3rdparty/softfloat/softfloat.h>
#endif
typedef sf_float32 SharemindFloat32;
#ifdef SHAREMIND_DEBUG
#include <stdio.h>
#endif
#include <string.h>


#ifdef SHAREMIND_DEBUG
#define SHAREMIND_DEBUG_PRINTSTATE \
    if (1) { \
        SHAREMIND_UPDATESTATE; \
        SharemindProgram_print_state_bencoded(p, stderr); \
        fprintf(stderr, "\n"); \
    } else (void) 0
#define SHAREMIND_DEBUG_PRINTINSTRUCTION(t) \
    if (1) { \
        fprintf(p->debugFileHandle, "%s\n", SHAREMIND_2S(SHAREMIND_INSTR_NAME(t))); \
    } else (void) 0
#else
#define SHAREMIND_DEBUG_PRINTSTATE (void) 0
#define SHAREMIND_DEBUG_PRINTINSTRUCTION(t) (void) 0
#endif

#ifndef SHAREMIND_SOFT_FLOAT

#ifndef SHAREMIND_NO_THREADS
#error The Sharemind VM does not yet fully support threads without softfloat.
#else
/**
  Thread-local pointer to the program being executed.
*/
static SharemindProgram * _SharemindProgram_SIGFPE_handler_p = NULL;
#endif

#ifdef __USE_POSIX
static void _SharemindProgram_SIGFPE_handler(int signalNumber,
                                         siginfo_t * signalInfo,
                                         void * context)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SharemindProgram_SIGFPE_handler(int signalNumber,
                                         siginfo_t * signalInfo,
                                         void * context)
{
    (void) signalNumber;
    (void) context;

    SharemindHardwareExceptionType e;
    switch (signalInfo->si_code) {
        case FPE_INTDIV: e = SHAREMIND_HET_FPE_INTDIV; break;
        case FPE_INTOVF: e = SHAREMIND_HET_FPE_INTOVF; break;
        case FPE_FLTDIV: e = SHAREMIND_HET_FPE_FLTDIV; break;
        case FPE_FLTOVF: e = SHAREMIND_HET_FPE_FLTOVF; break;
        case FPE_FLTUND: e = SHAREMIND_HET_FPE_FLTUND; break;
        case FPE_FLTRES: e = SHAREMIND_HET_FPE_FLTRES; break;
        case FPE_FLTINV: e = SHAREMIND_HET_FPE_FLTINV; break;
        default: e = SHAREMIND_HET_FPE_UNKNOWN; break;
    }

    assert(_SharemindProgram_SIGFPE_handler_p);
    siglongjmp(_SharemindProgram_SIGFPE_handler_p->safeJmpBuf[e], 1);
}
#else /* __USE_POSIX */
static void _SharemindProgram_SIGFPE_handler(int signalNumber)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SharemindProgram_SIGFPE_handler(int signalNumber) {
    signal(SIGFPE, _SharemindProgram_SIGFPE_handler);
    (void) signalNumber;
    assert(_SharemindProgram_SIGFPE_handler_p);
    longjmp(_SharemindProgram_SIGFPE_handler_p->safeJmpBuf[SHAREMIND_HET_FPE_UNKNOWN], 1);
}
#endif /* __USE_POSIX */

static inline void _SharemindProgram_setupSignalHandlers(SharemindProgram * program) {
    /* Register SIGFPE handler: */
    _SharemindProgram_SIGFPE_handler_p = program;
#ifdef __USE_POSIX
    struct sigaction newFpeAction;
    memset(&newFpeAction, '\0', sizeof(struct sigaction));
    newFpeAction.sa_sigaction = _SharemindProgram_SIGFPE_handler;
    newFpeAction.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGFPE, &newFpeAction, &program->oldFpeAction) != 0) {
#ifdef SHAREMIND_DEBUG
        fprintf(program->debugFileHandle, "Failed to set SIGFPE signal handler!");
#endif
    }
#else /* __USE_POSIX */
    program->oldFpeAction = signal(SIGFPE, _SharemindProgram_SIGFPE_handler);
#endif /* __USE_POSIX */
}

static inline void _SharemindProgram_restoreSignalHandlers(SharemindProgram * program) {
    /* Restore old SIGFPE handler: */
#ifdef __USE_POSIX
    if (sigaction(SIGFPE, &program->oldFpeAction, NULL) != 0)
#else /* __USE_POSIX */
    if (signal(SIGFPE, program->oldFpeAction) == SIG_ERR)
#endif /* __USE_POSIX */
    {
#ifdef SHAREMIND_DEBUG
        fprintf(program->debugFileHandle, "Failed to restore previous SIGFPE signal handler!");
#endif
#ifdef __USE_POSIX
        memset(&program->oldFpeAction, '\0', sizeof(struct sigaction));
        program->oldFpeAction.sa_handler = SIG_DFL;
        if (sigaction(SIGFPE, &program->oldFpeAction, NULL) != 0) {
#else /* __USE_POSIX */
        if (signal(SIGFPE, SIG_DFL) == SIG_ERR) {
#endif /* __USE_POSIX */
#ifdef SHAREMIND_DEBUG
            fprintf(program->debugFileHandle, "Failed reset SIGFPE signal handler to default!");
#endif
        }
    }
    _SharemindProgram_SIGFPE_handler_p = NULL;
}

#define SHAREMIND_NO_SF_ENCLOSE(op) \
    if(1) { \
        feclearexcept(FE_ALL_EXCEPT); \
        op; \
        int exceptions = fetestexcept(FE_ALL_EXCEPT); \
        if (unlikely(exceptions)) { \
            if (exceptions & FE_DIVBYZERO) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (exceptions & FE_OVERFLOW) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW; \
            } else if (exceptions & FE_UNDERFLOW) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW; \
            } else if (exceptions & FE_INEXACT) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT; \
            } else if (exceptions & FE_INVALID) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SHAREMIND_E_UNKNOWN_FPE; \
            } \
            SHAREMIND_DO_EXCEPT; \
        } \
    } else (void) 0
#define SHAREMIND_MI_UNEG_FLOAT32(d) SHAREMIND_NO_SF_ENCLOSE((d) = -(d))
#define SHAREMIND_MI_UINC_FLOAT32(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) + 1.0)
#define SHAREMIND_MI_UDEC_FLOAT32(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) - 1.0)
#define SHAREMIND_MI_BNEG_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = -(s))
#define SHAREMIND_MI_BINC_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) + 1.0)
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - 1.0)
#define SHAREMIND_MI_BADD_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) += (s))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) -= (s))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - (d))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) *= (s))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) /= (s))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) / (d))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) %= (s))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) % (d))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) + (s2))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) - (s2))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) * (s2))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) / (s2))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) % (s2))
#else /* #ifndef SHAREMIND_SOFT_FLOAT */
#define SHAREMIND_RESTORE_FPE_ENV
#define SHAREMIND_SF_ENCLOSE(op) \
    if(1) { \
        sf_float_exception_flags = 0; \
        op; \
        if (unlikely(sf_float_exception_flags)) { \
            if (sf_float_exception_flags & sf_float_flag_divbyzero) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (sf_float_exception_flags & sf_float_flag_overflow) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW; \
            } else if (sf_float_exception_flags & sf_float_flag_underflow) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW; \
            } else if (sf_float_exception_flags & sf_float_flag_inexact) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT; \
            } else if (sf_float_exception_flags & sf_float_flag_invalid) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_UNKNOWN_FPE; \
            } \
            SHAREMIND_DO_EXCEPT; \
        } \
    } else (void) 0
#define SHAREMIND_MI_UNEG_FLOAT32(d) SHAREMIND_SF_ENCLOSE((d) = sf_float32_neg(d))
#define SHAREMIND_MI_UINC_FLOAT32(d) SHAREMIND_SF_ENCLOSE((d) = sf_float32_add((d), sf_float32_one))
#define SHAREMIND_MI_UDEC_FLOAT32(d) SHAREMIND_SF_ENCLOSE((d) = sf_float32_sub((d), sf_float32_one))
#define SHAREMIND_MI_BNEG_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_neg(s))
#define SHAREMIND_MI_BINC_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_add((s), sf_float32_one))
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_sub((s), sf_float32_one))
#define SHAREMIND_MI_BADD_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_add((d), (s)))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_sub((d), (s)))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_sub((s), (d)))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_mul((d), (s)))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_div((d), (s)))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_div((s), (d)))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_mod((d), (s)))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) SHAREMIND_SF_ENCLOSE((d) = sf_float32_mod((s), (d)))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) SHAREMIND_SF_ENCLOSE((d) = sf_float32_add((s1), (s2)))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) SHAREMIND_SF_ENCLOSE((d) = sf_float32_sub((s1), (s2)))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) SHAREMIND_SF_ENCLOSE((d) = sf_float32_mul((s1), (s2)))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) SHAREMIND_SF_ENCLOSE((d) = sf_float32_div((s1), (s2)))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) SHAREMIND_SF_ENCLOSE((d) = sf_float32_mod((s1), (s2)))
#endif /* #ifndef SHAREMIND_SOFT_FLOAT */

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DO_EXCEPT if (1) { goto except; } else (void) 0
#define SHAREMIND_DO_HALT if (1) { goto halt; } else (void) 0
#define SHAREMIND_DO_TRAP if (1) { goto trap; } else (void) 0
#else
typedef enum { HC_EOF, HC_EXCEPT, HC_HALT, HC_TRAP } HaltCode;
#define SHAREMIND_DO_EXCEPT if (1) { return HC_EXCEPT; } else (void) 0
#define SHAREMIND_DO_HALT if (1) { return HC_HALT; } else (void) 0
#define SHAREMIND_DO_TRAP if (1) { return HC_TRAP; } else (void) 0
#endif

#define SHAREMIND_UPDATESTATE \
    if (1) { \
        p->currentIp = (uintptr_t) (ip - codeStart); \
    } else (void) 0

/* Micro-instructions (SHAREMIND_MI_*:) */

#define SHAREMIND_MI_NOP (void) 0
#define SHAREMIND_MI_HALT(r) \
    if (1) { \
        p->returnValue = (r); \
        SHAREMIND_DO_HALT; \
    } else (void) 0
#define SHAREMIND_MI_TRAP SHAREMIND_DO_TRAP

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DISPATCH(ip) goto *((ip)->p[0])
#define SHAREMIND_MI_DISPATCH(ip) if (1) { SHAREMIND_DISPATCH(ip); } else (void) 0
#else
#define SHAREMIND_DISPATCH_OTHERFRAME(ip,_thisStack,_thisRefStack,_thisCRefStack) \
    ((*((HaltCode (*)(SharemindProgram * const, \
                           const SharemindCodeBlock *, \
                           const SharemindCodeBlock *, \
                           SharemindRegisterVector * const, \
                           SharemindRegisterVector *, \
                           const SharemindReferenceVector *, \
                           const SharemindCReferenceVector *))((ip)->p[0])))(p,codeStart,ip,globalStack,_thisStack,_thisRefStack,_thisCRefStack))
#define SHAREMIND_DISPATCH(ip) SHAREMIND_DISPATCH_OTHERFRAME(ip,thisStack,thisRefStack,thisCRefStack)
#define SHAREMIND_MI_DISPATCH(newIp) if (1) { (void) (newIp); return SHAREMIND_DISPATCH(ip); } else (void) 0
#endif

#define SHAREMIND_MI_JUMP_REL(n) \
    if (1) { \
        SHAREMIND_MI_DISPATCH(ip += ((n)->int64[0])); \
    } else (void) 0

#define SHAREMIND_MI_IS_INSTR(i) \
    SharemindInstrSet_contains(&p->codeSections.data[p->currentCodeSectionIndex].instrset,(i))

#define SHAREMIND_MI_CHECK_JUMP_REL(reladdr) \
    if (1) { \
        uintptr_t unsignedIp = (uintptr_t) (ip - codeStart); \
        if ((reladdr) < 0) { \
            SHAREMIND_MI_TRY_EXCEPT(((uint64_t) -((reladdr) + 1)) < unsignedIp, \
                               SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } else { \
            SHAREMIND_MI_TRY_EXCEPT(((uint64_t) (reladdr)) < p->codeSections.data[p->currentCodeSectionIndex].size - unsignedIp - 1u, \
                               SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } \
        const SharemindCodeBlock * tip = ip + (reladdr); \
        SHAREMIND_MI_TRY_EXCEPT(SHAREMIND_MI_IS_INSTR((uintptr_t) (tip - codeStart)), SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        SHAREMIND_MI_DISPATCH(ip = tip); \
    } else (void) 0

#define SHAREMIND_MI_DO_EXCEPT(e) \
    if (1) { \
        p->exceptionValue = (e); \
        SHAREMIND_DO_EXCEPT; \
    } else (void) 0

#define SHAREMIND_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SHAREMIND_MI_DO_EXCEPT((e)); \
    } else (void) 0

#define SHAREMIND_MI_TRY_OOM(e) SHAREMIND_MI_TRY_EXCEPT((e),SHAREMIND_VM_PROCESS_OUT_OF_MEMORY)

#define SHAREMIND_MI_TRY_MEMRANGE(size,offset,numBytes,exception) \
    SHAREMIND_MI_TRY_EXCEPT(((offset) < (size)) \
                       && ((numBytes) <= (size)) \
                       && ((size) - (offset) >= (numBytes)), \
                       (exception))

#define SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME \
    if (!SHAREMIND_MI_HAS_STACK) { \
        p->nextFrame = SharemindFrameStack_push(&p->frames); \
        SHAREMIND_MI_TRY_OOM(p->nextFrame); \
        SharemindStackFrame_init(p->nextFrame, p->thisFrame); \
    } else (void)0

#define SHAREMIND_MI_PUSH(v) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        SharemindCodeBlock * reg = SharemindRegisterVector_push(&p->nextFrame->stack); \
        SHAREMIND_MI_TRY_OOM(reg); \
        *reg = (v); \
    } else (void) 0

#define _SHAREMIND_MI_PUSHREF_BLOCK(prefix,something,b,bOffset,rSize) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        Sharemind ## prefix ## erence * ref = Sharemind ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SHAREMIND_MI_TRY_EXCEPT(ref, SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
        ref->pData = (&(b)->uint8[0] + (bOffset)); \
        ref->size = (rSize); \
        ref->internal = NULL; \
    } else (void) 0

#define SHAREMIND_MI_PUSHREF_BLOCK_ref(b)  _SHAREMIND_MI_PUSHREF_BLOCK(Ref, refstack,  (b), 0u, sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREF_BLOCK_cref(b) _SHAREMIND_MI_PUSHREF_BLOCK(CRef,crefstack, (b), 0u, sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_ref(b,o,s)  _SHAREMIND_MI_PUSHREF_BLOCK(Ref, refstack,  (b), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_cref(b,o,s) _SHAREMIND_MI_PUSHREF_BLOCK(CRef,crefstack, (b), (o), (s))

#define _SHAREMIND_MI_PUSHREF_REF(prefix,something,constPerhaps,r,rOffset,rSize) \
    if (1) { \
        if ((r)->internal && ((SharemindMemorySlot *) (r)->internal)->nrefs + 1u == 0u) { \
            SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
        } \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        Sharemind ## prefix ## erence * ref = Sharemind ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SHAREMIND_MI_TRY_EXCEPT(ref, SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
        ref->pData = ((constPerhaps uint8_t *) (r)->pData) + (rOffset); \
        ref->size = (rSize); \
        ref->internal = (r)->internal; \
        if (ref->internal) \
            ((SharemindMemorySlot *) ref->internal)->nrefs++; \
    } else (void) 0

#define SHAREMIND_MI_PUSHREF_REF_ref(r)  _SHAREMIND_MI_PUSHREF_REF(Ref, refstack,,        (r), 0u, (r)->size)
#define SHAREMIND_MI_PUSHREF_REF_cref(r) _SHAREMIND_MI_PUSHREF_REF(CRef,crefstack, const, (r), 0u, (r)->size)
#define SHAREMIND_MI_PUSHREFPART_REF_ref(r,o,s)  _SHAREMIND_MI_PUSHREF_REF(Ref, refstack,,        (r), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_REF_cref(r,o,s) _SHAREMIND_MI_PUSHREF_REF(CRef,crefstack, const, (r), (o), (s))

#define _SHAREMIND_MI_PUSHREF_MEM(prefix,something,slot,mOffset,rSize) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        Sharemind ## prefix ## erence * ref = Sharemind ## prefix ## erenceVector_push(&p->nextFrame->something); \
        SHAREMIND_MI_TRY_EXCEPT(ref, SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
        ref->pData = ((uint8_t *) (slot)->pData) + (mOffset); \
        ref->size = (rSize); \
        ref->internal = (slot); \
        (slot)->nrefs++; \
    } else (void) 0

#define SHAREMIND_MI_PUSHREF_MEM_ref(slot)  _SHAREMIND_MI_PUSHREF_MEM(Ref, refstack,  (slot), 0u, (slot)->size)
#define SHAREMIND_MI_PUSHREF_MEM_cref(slot) _SHAREMIND_MI_PUSHREF_MEM(CRef,crefstack, (slot), 0u, (slot)->size)
#define SHAREMIND_MI_PUSHREFPART_MEM_ref(slot,o,s)  _SHAREMIND_MI_PUSHREF_MEM(Ref, refstack,  (slot), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_MEM_cref(slot,o,s) _SHAREMIND_MI_PUSHREF_MEM(CRef,crefstack, (slot), (o), (s))

#define SHAREMIND_MI_RESIZE_STACK(size) \
    SHAREMIND_MI_TRY_OOM(SharemindRegisterVector_resize(thisStack, (size)))

#define SHAREMIND_MI_CLEAR_STACK \
    if (1) { \
        SharemindRegisterVector_resize(&p->nextFrame->stack, 0u); \
        SharemindReferenceVector_foreach_void(&p->nextFrame->refstack, &SharemindReference_destroy); \
        SharemindCReferenceVector_foreach_void(&p->nextFrame->crefstack, &SharemindCReference_destroy); \
        SharemindReferenceVector_resize(&p->nextFrame->refstack, 0u); \
        SharemindCReferenceVector_resize(&p->nextFrame->crefstack, 0u); \
    } else (void) 0

#define SHAREMIND_MI_HAS_STACK (!!(p->nextFrame))

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_CALL_RETURN_DISPATCH(ip) \
    if (1) { \
        thisStack = &p->thisFrame->stack; \
        thisRefStack = &p->thisFrame->refstack; \
        thisCRefStack = &p->thisFrame->crefstack; \
        SHAREMIND_DISPATCH((ip)); \
    } else (void) 0
#else
#define SHAREMIND_CALL_RETURN_DISPATCH(ip) return SHAREMIND_DISPATCH_OTHERFRAME((ip), &p->thisFrame->stack, &p->thisFrame->refstack, &p->thisFrame->crefstack)
#endif

#define SHAREMIND_MI_CALL(a,r,nargs) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        ip = codeStart + (a); \
        SHAREMIND_CALL_RETURN_DISPATCH(ip); \
    } else (void) 0

#define SHAREMIND_MI_CHECK_CALL(a,r,nargs) \
    if (1) { \
        SHAREMIND_MI_TRY_EXCEPT(SHAREMIND_MI_IS_INSTR((a)->uint64[0]), SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        SHAREMIND_MI_CALL((a)->uint64[0],r,nargs); \
    } else (void) 0

#define SHAREMIND_MI_SYSCALL(sc,r,nargs) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        const SharemindSyscallCallable rc = ((const SharemindSyscallBinding *) sc)->wrapper.callable; \
        p->syscallContext.libmodapi_internal = ((const SharemindSyscallBinding *) sc)->wrapper.internal; \
        p->syscallContext.moduleHandle = ((const SharemindSyscallBinding *) sc)->moduleHandle; \
        SharemindReference * ref; \
        if (p->nextFrame->refstack.size == 0u) { \
            ref = NULL; \
        } else { \
            ref = SharemindReferenceVector_push(&p->nextFrame->refstack); \
            SHAREMIND_MI_TRY_OOM(ref); \
            ref->pData = NULL; \
            ref = p->nextFrame->refstack.data; \
        } \
        SharemindCReference * cref; \
        if (p->nextFrame->crefstack.size == 0u) { \
            cref = NULL; \
        } else { \
            cref = SharemindCReferenceVector_push(&p->nextFrame->crefstack); \
            SHAREMIND_MI_TRY_OOM(cref); \
            cref->pData = NULL; \
            cref = p->nextFrame->crefstack.data; \
        } \
        SharemindModuleApi0x1SyscallCode st; \
        st = (*rc)(p->nextFrame->stack.data, p->nextFrame->stack.size, \
                   ref, cref, \
                   (r), &p->syscallContext); \
        SharemindStackFrame_destroy(SharemindFrameStack_top(&p->frames)); \
        SharemindFrameStack_pop(&p->frames); \
        p->nextFrame = NULL; \
        switch (st) { \
            case SHAREMIND_MODULE_API_0x1_SC_OK: \
                break; \
            case SHAREMIND_MODULE_API_0x1_SC_OUT_OF_MEMORY: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
                break; \
            case SHAREMIND_MODULE_API_0x1_SC_INVALID_CALL: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_INVALID_SYSCALL_INVOCATION); \
                break; \
            case SHAREMIND_MODULE_API_0x1_SC_GENERAL_FAILURE: /* Fall through: */ \
            default: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_SYSCALL_FAILURE); \
                break; \
        } \
    } else (void) 0

#define SHAREMIND_MI_CHECK_SYSCALL(a,r,nargs) \
    if (1) { \
        SHAREMIND_MI_TRY_EXCEPT((a)->uint64[0] < p->bindings.size, \
                                SHAREMIND_VM_PROCESS_INVALID_INDEX_SYSCALL); \
        SHAREMIND_MI_SYSCALL(&p->bindings.data[(size_t) (a)->uint64[0]],r,nargs); \
    } else (void) 0

#define SHAREMIND_MI_RETURN(r) \
    if (1) { \
        if (unlikely(p->nextFrame)) { \
            SharemindStackFrame_destroy(SharemindFrameStack_top(&p->frames)); \
            SharemindFrameStack_pop(&p->frames); \
            p->nextFrame = NULL; \
        } \
        if (likely(p->thisFrame->returnAddr)) { \
            if (p->thisFrame->returnValueAddr) \
                *p->thisFrame->returnValueAddr = (r); \
            ip = p->thisFrame->returnAddr; \
            p->thisFrame = p->thisFrame->prev; \
            SharemindFrameStack_pop(&p->frames); \
            SHAREMIND_CALL_RETURN_DISPATCH(ip); \
        } else { \
            SHAREMIND_MI_HALT((r)); \
        } \
    } else (void) 0

#define SHAREMIND_MI_GET_T_reg(d,t,i) \
    if (1) { \
        const SharemindCodeBlock * r = SharemindRegisterVector_get_const_pointer(globalStack, (i)); \
        SHAREMIND_MI_TRY_EXCEPT(r,SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER); \
        (d) = & SHAREMIND_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SHAREMIND_MI_GET_T_stack(d,t,i) \
    if (1) { \
        const SharemindCodeBlock * r = SharemindRegisterVector_get_const_pointer(thisStack, (i)); \
        SHAREMIND_MI_TRY_EXCEPT(r,SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK); \
        (d) = & SHAREMIND_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define _SHAREMIND_MI_GET(d,i,fromwhere,exception,isconst) \
    if (1) { \
        (d) = SharemindRegisterVector_get_ ## isconst ## pointer((fromwhere), (i)); \
        SHAREMIND_MI_TRY_EXCEPT((d),(exception)); \
    } else (void) 0

#define SHAREMIND_MI_GET_stack(d,i) _SHAREMIND_MI_GET((d),(i),thisStack,SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK,)
#define SHAREMIND_MI_GET_reg(d,i)   _SHAREMIND_MI_GET((d),(i),globalStack,SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER,)
#define SHAREMIND_MI_GET_CONST_stack(d,i) _SHAREMIND_MI_GET((d),(i),thisStack,SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK,const_)
#define SHAREMIND_MI_GET_CONST_reg(d,i)   _SHAREMIND_MI_GET((d),(i),globalStack,SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER,const_)

#define _SHAREMIND_MI_GET_REF(r,i,type,exception) \
    if (1) { \
        (r) = Sharemind ## type ## erenceVector_get_const_pointer(this ## type ## Stack, (i)); \
        SHAREMIND_MI_TRY_EXCEPT((r),(exception)); \
    } else (void) 0

#define SHAREMIND_MI_GET_ref(r,i) _SHAREMIND_MI_GET_REF((r),(i),Ref,SHAREMIND_VM_PROCESS_INVALID_INDEX_REFERENCE)
#define SHAREMIND_MI_GET_cref(r,i) _SHAREMIND_MI_GET_REF((r),(i),CRef,SHAREMIND_VM_PROCESS_INVALID_INDEX_CONST_REFERENCE)

#define SHAREMIND_MI_REFERENCE_GET_MEMORY_PTR(r) ((SharemindMemorySlot *) (r)->internal)
#define SHAREMIND_MI_REFERENCE_GET_PTR(r) ((uint8_t *) (r)->pData)
#define SHAREMIND_MI_REFERENCE_GET_CONST_PTR(r) ((const uint8_t *) (r)->pData)
#define SHAREMIND_MI_REFERENCE_GET_SIZE(r) ((r)->size)

#define SHAREMIND_MI_REF_CAN_READ(ref) ((SharemindMemorySlot *) ref->internal == NULL || SHAREMIND_MI_MEM_CAN_READ((SharemindMemorySlot *) ref->internal))
#define SHAREMIND_MI_REF_CAN_WRITE(ref) ((SharemindMemorySlot *) ref->internal == NULL || SHAREMIND_MI_MEM_CAN_WRITE((SharemindMemorySlot *) ref->internal))

#define SHAREMIND_MI_BLOCK_AS(b,t) (b->t[0])
#define SHAREMIND_MI_BLOCK_AS_P(b,t) (&b->t[0])
#define SHAREMIND_MI_ARG(n)        (* SHAREMIND_MI_ARG_P(n))
#define SHAREMIND_MI_ARG_P(n)      (ip + (n))
#define SHAREMIND_MI_ARG_AS(n,t)   (SHAREMIND_MI_BLOCK_AS(SHAREMIND_MI_ARG_P(n),t))
#define SHAREMIND_MI_ARG_AS_P(n,t) (& SHAREMIND_MI_BLOCK_AS(SHAREMIND_MI_ARG_P(n), t))

#ifndef SHAREMIND_SOFT_FLOAT
#define SHAREMIND_MI_CONVERT_float32_TO_int8(a,b) a = (int8_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int16(a,b) a = (int16_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int32(a,b) a = (int32_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int64(a,b) a = (int64_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(a,b) a = (uint8_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(a,b) a = (uint16_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(a,b) a = (uint32_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(a,b) a = (uint64_t) b
#define SHAREMIND_MI_CONVERT_int8_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int16_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int32_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int64_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(a,b) a = (float) b
#else
#define SHAREMIND_MI_CONVERT_float32_TO_int8(a,b)   a = (int8_t)   sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_int16(a,b)  a = (int16_t)  sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_int32(a,b)  a = (int32_t)  sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_int64(a,b)  a = (int64_t)  sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(a,b)  a = (uint8_t)  sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(a,b) a = (uint16_t) sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(a,b) a = (uint32_t) sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(a,b) a = (uint64_t) sf_float32_to_int64(b)
#define SHAREMIND_MI_CONVERT_int8_TO_float32(a,b)   a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_int16_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_int32_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_int64_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(a,b)  a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(a,b) a = sf_int64_to_float32((int64_t) b)
#endif

#define SHAREMIND_MI_MEM_CAN_READ(slot) ((slot)->specials == NULL || (slot)->specials->readable)
#define SHAREMIND_MI_MEM_CAN_WRITE(slot) ((slot)->specials == NULL || (slot)->specials->writeable)

#define SHAREMIND_MI_MEM_ALLOC(dptr,sizereg) \
    if (1) { \
        (dptr)->uint64[0] = SharemindProgram_public_alloc(p, (sizereg)->uint64[0], NULL); \
    } else (void) 0

#define SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    if (1) { \
        (dest) = SharemindMemoryMap_get(&p->memoryMap, (index)); \
        SHAREMIND_MI_TRY_EXCEPT((dest),SHAREMIND_VM_PROCESS_INVALID_REFERENCE); \
    } else (void) 0

#define SHAREMIND_MI_MEM_FREE(ptr) \
    if (1) { \
        SharemindVmProcessException e = SharemindProgram_public_free(p, (ptr)->uint64[0]); \
        if (unlikely(e != SHAREMIND_VM_PROCESS_OK)) { \
            SHAREMIND_MI_DO_EXCEPT(e); \
        } \
    } else (void) 0

#define SHAREMIND_MI_MEM_GET_SIZE(ptr,sizedest) \
    if (1) { \
        SharemindMemorySlot * slot; \
        SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        (sizedest)->uint64[0] = slot->size; \
    } else (void) 0

#define SHAREMIND_MI_MEMCPY memcpy
#define SHAREMIND_MI_MEMMOVE memmove

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_IMPL(name,code) \
    label_impl_ ## name : code
#else
#define SHAREMIND_IMPL_INNER(name,code) \
    static inline HaltCode name ( \
        SharemindProgram * const p, \
        const SharemindCodeBlock * const codeStart, \
        const SharemindCodeBlock * ip, \
        SharemindRegisterVector * const globalStack, \
        SharemindRegisterVector * thisStack, \
        const SharemindReferenceVector * thisRefStack, \
        const SharemindCReferenceVector * thisCRefStack) \
    { \
        (void) codeStart; (void) globalStack; (void) thisStack; \
        (void) thisRefStack; (void) thisCRefStack; \
        SHAREMIND_UPDATESTATE; \
        code \
    }
#define SHAREMIND_IMPL(name,code) SHAREMIND_IMPL_INNER(func_impl_ ## name, code)

#include <sharemind/m4/dispatches.h>

SHAREMIND_IMPL_INNER(_func_impl_eof,return HC_EOF;)
SHAREMIND_IMPL_INNER(_func_impl_except,return HC_EXCEPT;)
SHAREMIND_IMPL_INNER(_func_impl_halt,return HC_HALT;)
SHAREMIND_IMPL_INNER(_func_impl_trap,return HC_TRAP;)

#endif

SharemindVmError sharemind_vm_run(SharemindProgram * const p,
                 const SharemindInnerCommand sharemind_vm_run_command,
                 void * const sharemind_vm_run_data)
{
    if (sharemind_vm_run_command == SHAREMIND_I_GET_IMPL_LABEL) {

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_IMPL_LABEL(name) && label_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static void * system_labels[] = { && eof, && except, && halt, && trap };
#else
#define SHAREMIND_IMPL_LABEL(name) & func_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static void * system_labels[] = { &_func_impl_eof, &_func_impl_except, &_func_impl_halt, &_func_impl_trap };
#endif
        // static void *(*labels[3])[] = { &instr_labels, &empty_impl_labels, &system_labels };

        SharemindPreparationBlock * pb = (SharemindPreparationBlock *) sharemind_vm_run_data;
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
        return SHAREMIND_VM_OK;

    } else if (sharemind_vm_run_command == SHAREMIND_I_RUN || sharemind_vm_run_command == SHAREMIND_I_CONTINUE) {

#pragma STDC FENV_ACCESS ON

        const SharemindCodeBlock * const codeStart = p->codeSections.data[p->currentCodeSectionIndex].data;
        const SharemindCodeBlock * ip = &codeStart[p->currentIp];
        SharemindRegisterVector * const globalStack = &p->globalFrame->stack;
        SharemindRegisterVector * thisStack = &p->thisFrame->stack;
        SharemindReferenceVector * thisRefStack = &p->thisFrame->refstack;
        SharemindCReferenceVector * thisCRefStack = &p->thisFrame->crefstack;

#ifndef SHAREMIND_SOFT_FLOAT
        p->hasSavedFpeEnv = (fegetenv(&p->savedFpeEnv) == 0);

        /**
          \warning Note that the variable "ip" is not valid after one of the
                   following sigsetjmp's returns non-zero unless we declare it
                   volatile. However, declaring "ip" as volatile has a negative
                   performance impact. A remedy would be to update p->currentIp
                   before every instruction which could generate such signals.
        */
#ifdef __USE_POSIX
    #define _SHAREMIND_DECLARE_SIGJMP(h,e) \
            if (sigsetjmp(p->safeJmpBuf[(h)], 1)) { \
                p->exceptionValue = (e); \
                SHAREMIND_DO_EXCEPT; \
            } else (void) 0
#else
    #define _SHAREMIND_DECLARE_SIGJMP(h,e) \
            if (setjmp(p->safeJmpBuf[(h)])) { \
                p->exceptionValue = (e); \
                SHAREMIND_DO_EXCEPT; \
            } else (void) 0
#endif
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_UNKNOWN, SHAREMIND_VM_PROCESS_UNKNOWN_FPE);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_INTDIV,  SHAREMIND_VM_PROCESS_INTEGER_DIVIDE_BY_ZERO);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_INTOVF,  SHAREMIND_VM_PROCESS_INTEGER_OVERFLOW);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_FLTDIV,  SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_FLTOVF,  SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_FLTUND,  SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_FLTRES,  SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT);
        _SHAREMIND_DECLARE_SIGJMP(SHAREMIND_HET_FPE_FLTINV,  SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION);

        _SharemindProgram_setupSignalHandlers(p);
#endif /* #ifndef SHAREMIND_SOFT_FLOAT */

#ifndef SHAREMIND_FAST_BUILD
        SHAREMIND_MI_DISPATCH(ip);

        #include <sharemind/m4/dispatches.h>
#else
        switch (SHAREMIND_DISPATCH(ip)) {
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
            p->exceptionValue = SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS;

        except:
            assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
            assert(p->exceptionValue != SHAREMIND_VM_PROCESS_OK);
            assert(SharemindVmProcessException_toString((SharemindVmProcessException) p->exceptionValue) != 0);
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProgram_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
            p->state = SHAREMIND_VM_PROCESS_CRASHED;
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            return SHAREMIND_VM_RUNTIME_EXCEPTION;

        halt:
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProgram_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            return SHAREMIND_VM_OK;

        trap:
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProgram_restoreSignalHandlers(p);
            /** \todo Check for errors */
            if (p->hasSavedFpeEnv)
                fesetenv(&p->savedFpeEnv);
#endif
            p->state = SHAREMIND_VM_PROCESS_TRAPPED;
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            return SHAREMIND_VM_RUNTIME_TRAP;
    } else {
        abort();
    }
}
