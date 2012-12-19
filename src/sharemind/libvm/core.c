/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__
#include "core.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef SHAREMIND_SOFT_FLOAT
#include <sharemind/3rdparty/libsoftfloat/softfloat.h>
#endif
typedef sf_float32 SharemindFloat32;
typedef sf_float64 SharemindFloat64;
#ifdef SHAREMIND_DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include "preparationblock.h"
#include "process.h"
#include "program.h"
#include "registervector.h"

#ifndef SHAREMIND_VM_TIMING
#define SHAREMIND_VM_TIMING
#endif
#ifndef SHAREMIND_VM_TIMING_START
#define SHAREMIND_VM_TIMING_START
#endif
#ifndef SHAREMIND_VM_TIMING_STOP
#define SHAREMIND_VM_TIMING_STOP
#endif

#ifdef SHAREMIND_DEBUG
#define SHAREMIND_DEBUG_PRINTSTATE \
    if (1) { \
        SHAREMIND_UPDATESTATE; \
        SharemindProcess_print_state_bencoded(p, stderr); \
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
SHAREMIND_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t));
SHAREMIND_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));

#ifndef SHAREMIND_NO_THREADS
#error The Sharemind VM does not yet fully support threads without softfloat.
#else
/**
  Thread-local pointer to the program being executed.
*/
static SharemindProcess * _SharemindProcess_SIGFPE_handler_p = NULL;
#endif

#ifdef __USE_POSIX
static void _SharemindProcess_SIGFPE_handler(int signalNumber,
                                             siginfo_t * signalInfo,
                                             void * context)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SharemindProcess_SIGFPE_handler(int signalNumber,
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

    assert(_SharemindProcess_SIGFPE_handler_p);
    siglongjmp(_SharemindProcess_SIGFPE_handler_p->safeJmpBuf[e], 1);
}
#else /* __USE_POSIX */
static void _SharemindProcess_SIGFPE_handler(int signalNumber)
    __attribute__ ((__noreturn__, __nothrow__));
static void _SharemindProcess_SIGFPE_handler(int signalNumber) {
    signal(SIGFPE, _SharemindProcess_SIGFPE_handler);
    (void) signalNumber;
    assert(_SharemindProcess_SIGFPE_handler_p);
    longjmp(_SharemindProcess_SIGFPE_handler_p->safeJmpBuf[SHAREMIND_HET_FPE_UNKNOWN], 1);
}
#endif /* __USE_POSIX */

static inline void _SharemindProcess_setupSignalHandlers(SharemindProcess * program) {
    /* Register SIGFPE handler: */
    _SharemindProcess_SIGFPE_handler_p = program;
#ifdef __USE_POSIX
    struct sigaction newFpeAction;
    memset(&newFpeAction, '\0', sizeof(struct sigaction));
    newFpeAction.sa_sigaction = _SharemindProcess_SIGFPE_handler;
    newFpeAction.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGFPE, &newFpeAction, &program->oldFpeAction) != 0) {
#ifdef SHAREMIND_DEBUG
        fprintf(program->debugFileHandle, "Failed to set SIGFPE signal handler!");
#endif
    }
#else /* __USE_POSIX */
    program->oldFpeAction = signal(SIGFPE, _SharemindProcess_SIGFPE_handler);
#endif /* __USE_POSIX */
}

static inline void _SharemindProcess_restoreSignalHandlers(SharemindProcess * program) {
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
    _SharemindProcess_SIGFPE_handler_p = NULL;
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
#define SHAREMIND_MI_UNEG_FLOAT64(d) SHAREMIND_NO_SF_ENCLOSE((d) = -(d))
#define SHAREMIND_MI_UINC_FLOAT32(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) + 1.0)
#define SHAREMIND_MI_UINC_FLOAT64(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) + 1.0)
#define SHAREMIND_MI_UDEC_FLOAT32(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) - 1.0)
#define SHAREMIND_MI_UDEC_FLOAT64(d) SHAREMIND_NO_SF_ENCLOSE((d) = (d) - 1.0)
#define SHAREMIND_MI_BNEG_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = -(s))
#define SHAREMIND_MI_BNEG_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = -(s))
#define SHAREMIND_MI_BINC_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) + 1.0)
#define SHAREMIND_MI_BINC_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) + 1.0)
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - 1.0)
#define SHAREMIND_MI_BDEC_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - 1.0)
#define SHAREMIND_MI_BADD_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) += (s))
#define SHAREMIND_MI_BADD_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) += (s))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) -= (s))
#define SHAREMIND_MI_BSUB_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) -= (s))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - (d))
#define SHAREMIND_MI_BSUB2_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) - (d))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) *= (s))
#define SHAREMIND_MI_BMUL_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) *= (s))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) /= (s))
#define SHAREMIND_MI_BDIV_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) /= (s))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) / (d))
#define SHAREMIND_MI_BDIV2_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) / (d))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) %= (s))
#define SHAREMIND_MI_BMOD_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) %= (s))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) % (d))
#define SHAREMIND_MI_BMOD2_FLOAT64(d,s) SHAREMIND_NO_SF_ENCLOSE((d) = (s) % (d))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) + (s2))
#define SHAREMIND_MI_TADD_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) + (s2))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) - (s2))
#define SHAREMIND_MI_TSUB_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) - (s2))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) * (s2))
#define SHAREMIND_MI_TMUL_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) * (s2))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) / (s2))
#define SHAREMIND_MI_TDIV_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) / (s2))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) % (s2))
#define SHAREMIND_MI_TMOD_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) % (s2))
#define SHAREMIND_MI_TEQ_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) == (s2))
#define SHAREMIND_MI_TEQ_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) == (s2))
#define SHAREMIND_MI_TNE_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) != (s2))
#define SHAREMIND_MI_TNE_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) != (s2))
#define SHAREMIND_MI_TLT_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) < (s2))
#define SHAREMIND_MI_TLT_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) < (s2))
#define SHAREMIND_MI_TLE_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) <= (s2))
#define SHAREMIND_MI_TLE_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) <= (s2))
#define SHAREMIND_MI_TGT_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) > (s2))
#define SHAREMIND_MI_TGT_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) > (s2))
#define SHAREMIND_MI_TGE_FLOAT32(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) >= (s2))
#define SHAREMIND_MI_TGE_FLOAT64(d,s1,s2) SHAREMIND_NO_SF_ENCLOSE((d) = (s1) >= (s2))
#else /* #ifndef SHAREMIND_SOFT_FLOAT */
#define SHAREMIND_MI_FPU_STATE (p->fpuState)
#define SHAREMIND_MI_FPU_STATE_SET(v) do { p->fpuState = (sf_fpu_state) (v); } while(0)
#define SHAREMIND_RESTORE_FPE_ENV
#define SHAREMIND_SF_E(type,dest,n,...) \
    if(1) { \
        p->fpuState = p->fpuState & ~sf_fpu_state_exception_mask; \
        const type r = __VA_ARGS__; \
        (dest) = n r.result; \
        p->fpuState = r.fpu_state; \
        const sf_fpu_state e = (r.fpu_state & sf_fpu_state_exception_mask) \
                             & ((r.fpu_state & sf_fpu_state_exception_crash_mask) << 5u); \
        if (unlikely(e)) { \
            if (e & sf_float_flag_divbyzero) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (e & sf_float_flag_overflow) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW; \
            } else if (e & sf_float_flag_underflow) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW; \
            } else if (e & sf_float_flag_inexact) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT; \
            } else if (e & sf_float_flag_invalid) { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_UNKNOWN_FPE; \
            } \
            SHAREMIND_DO_EXCEPT; \
        } \
    } else (void) 0
#define SHAREMIND_SF_FPU32F(dest,...) SHAREMIND_SF_E(sf_result32f,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPU64F(dest,...) SHAREMIND_SF_E(sf_result64f,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPU32I(dest,...) SHAREMIND_SF_E(sf_result32i,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPU64I(dest,...) SHAREMIND_SF_E(sf_result64i,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPUF(dest,...) SHAREMIND_SF_E(sf_resultFlag,(dest),,__VA_ARGS__)
#define SHAREMIND_MI_UNEG_FLOAT32(d) do { (d) = sf_float32_neg(d); } while (0)
#define SHAREMIND_MI_UNEG_FLOAT64(d) do { (d) = sf_float64_neg(d); } while (0)
#define SHAREMIND_MI_UINC_FLOAT32(d) SHAREMIND_SF_FPU32F((d),sf_float32_add((d),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_UINC_FLOAT64(d) SHAREMIND_SF_FPU64F((d),sf_float64_add((d),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_UDEC_FLOAT32(d) SHAREMIND_SF_FPU32F((d),sf_float32_sub((d),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_UDEC_FLOAT64(d) SHAREMIND_SF_FPU64F((d),sf_float64_sub((d),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BNEG_FLOAT32(d,s) do { (d) = sf_float32_neg(s); } while (0)
#define SHAREMIND_MI_BNEG_FLOAT64(d,s) do { (d) = sf_float64_neg(s); } while (0)
#define SHAREMIND_MI_BINC_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_add((s),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_BINC_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_add((s),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_sub((s),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_BDEC_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_sub((s),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BADD_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_add((d),(s),p->fpuState))
#define SHAREMIND_MI_BADD_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_add((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_sub((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_sub((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_sub((s),(d),p->fpuState))
#define SHAREMIND_MI_BSUB2_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_sub((s),(d),p->fpuState))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_mul((d),(s),p->fpuState))
#define SHAREMIND_MI_BMUL_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_mul((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_div((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_div((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_div((s),(d),p->fpuState))
#define SHAREMIND_MI_BDIV2_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_div((s),(d),p->fpuState))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_rem((d),(s),p->fpuState))
#define SHAREMIND_MI_BMOD_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_rem((d),(s),p->fpuState))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) SHAREMIND_SF_FPU32F((d),sf_float32_rem((s),(d),p->fpuState))
#define SHAREMIND_MI_BMOD2_FLOAT64(d,s) SHAREMIND_SF_FPU64F((d),sf_float64_rem((s),(d),p->fpuState))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) SHAREMIND_SF_FPU32F((d),sf_float32_add((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TADD_FLOAT64(d,s1,s2) SHAREMIND_SF_FPU64F((d),sf_float64_add((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) SHAREMIND_SF_FPU32F((d),sf_float32_sub((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TSUB_FLOAT64(d,s1,s2) SHAREMIND_SF_FPU64F((d),sf_float64_sub((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) SHAREMIND_SF_FPU32F((d),sf_float32_mul((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMUL_FLOAT64(d,s1,s2) SHAREMIND_SF_FPU64F((d),sf_float64_mul((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) SHAREMIND_SF_FPU32F((d),sf_float32_div((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TDIV_FLOAT64(d,s1,s2) SHAREMIND_SF_FPU64F((d),sf_float64_div((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) SHAREMIND_SF_FPU32F((d),sf_float32_rem((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMOD_FLOAT64(d,s1,s2) SHAREMIND_SF_FPU64F((d),sf_float64_rem((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TEQ_FLOAT32(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float32_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TEQ_FLOAT64(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float64_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TNE_FLOAT32(d,s1,s2) SHAREMIND_SF_E(sf_resultFlag,(d),!,sf_float32_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TNE_FLOAT64(d,s1,s2) SHAREMIND_SF_E(sf_resultFlag,(d),!,sf_float64_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLT_FLOAT32(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float32_lt((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLT_FLOAT64(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float64_lt((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLE_FLOAT32(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float32_le((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLE_FLOAT64(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float64_le((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TGT_FLOAT32(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float32_lt((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGT_FLOAT64(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float64_lt((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGE_FLOAT32(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float32_le((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGE_FLOAT64(d,s1,s2) SHAREMIND_SF_FPUF((d),sf_float64_le((s2),(s1),p->fpuState))
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
#define SHAREMIND_DISPATCH(ip) do { SHAREMIND_VM_TIMING goto *((ip)->p[0]); } while(0)
#define SHAREMIND_MI_DISPATCH(ip) if (1) { SHAREMIND_DISPATCH(ip); } else (void) 0
#else
#define SHAREMIND_DISPATCH_OTHERFRAME(ip,_thisStack,_thisRefStack,_thisCRefStack) \
    ((*((HaltCode (*)(SharemindProcess * const, \
                           const SharemindCodeBlock *, \
                           const SharemindCodeBlock *, \
                           SharemindRegisterVector * const, \
                           SharemindRegisterVector *, \
                           const SharemindReferenceVector *, \
                           const SharemindCReferenceVector *))((ip)->fp[0])))(p,codeStart,ip,globalStack,_thisStack,_thisRefStack,_thisCRefStack))
#define SHAREMIND_DISPATCH(ip) SHAREMIND_DISPATCH_OTHERFRAME(ip,thisStack,thisRefStack,thisCRefStack)
#define SHAREMIND_MI_DISPATCH(newIp) if (1) { (void) (newIp); SHAREMIND_VM_TIMING return SHAREMIND_DISPATCH(ip); } else (void) 0
#endif

#define SHAREMIND_MI_JUMP_REL(n) \
    if (1) { \
        SHAREMIND_MI_DISPATCH(ip += ((n)->int64[0])); \
    } else (void) 0

#define SHAREMIND_MI_IS_INSTR(i) \
    (SharemindInstrMap_get(&p->program->codeSections.data[p->currentCodeSectionIndex].instrmap,((size_t) (i))) != NULL)

#define SHAREMIND_MI_CHECK_JUMP_REL(reladdr) \
    if (1) { \
        uintptr_t unsignedIp = (uintptr_t) (ip - codeStart); \
        if ((reladdr) < 0) { \
            SHAREMIND_MI_TRY_EXCEPT(((uint64_t) -((reladdr) + 1)) < unsignedIp, \
                               SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } else { \
            SHAREMIND_MI_TRY_EXCEPT(((uint64_t) (reladdr)) < p->program->codeSections.data[p->currentCodeSectionIndex].size - unsignedIp - 1u, \
                               SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } \
        const SharemindCodeBlock * tip = ip + (reladdr); \
        SHAREMIND_MI_TRY_EXCEPT(SHAREMIND_MI_IS_INSTR(tip - codeStart), SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
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
        assert(rSize > 0u); \
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
        assert(rSize > 0u); \
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
        assert(rSize > 0u); \
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
#define SHAREMIND_CALL_RETURN_DISPATCH(ip) do { SHAREMIND_VM_TIMING return SHAREMIND_DISPATCH_OTHERFRAME((ip), &p->thisFrame->stack, &p->thisFrame->refstack, &p->thisFrame->crefstack); } while(0)
#endif

#define SHAREMIND_MI_CALL(a,r,nargs) \
    if (1) { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        ip = codeStart + ((size_t) (a)); \
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
        SharemindStackFrame * nextFrame = p->nextFrame; \
        nextFrame->returnValueAddr = (r); \
        const SharemindSyscallCallable rc = ((const SharemindSyscallBinding *) sc)->wrapper.callable; \
        p->syscallContext.syscall_internal = ((const SharemindSyscallBinding *) sc)->wrapper.internal; \
        p->syscallContext.moduleHandle = ((const SharemindSyscallBinding *) sc)->moduleHandle; \
        SharemindReference * ref; \
        bool hasRefs = (nextFrame->refstack.size > 0u); \
        if (!hasRefs) { \
            ref = NULL; \
        } else { \
            ref = SharemindReferenceVector_push(&nextFrame->refstack); \
            SHAREMIND_MI_TRY_OOM(ref); \
            ref->pData = NULL; \
            ref = nextFrame->refstack.data; \
        } \
        bool hasCRefs = (nextFrame->crefstack.size > 0u); \
        SharemindCReference * cref; \
        if (!hasCRefs) { \
            cref = NULL; \
        } else { \
            cref = SharemindCReferenceVector_push(&nextFrame->crefstack); \
            SHAREMIND_MI_TRY_OOM(cref); \
            cref->pData = NULL; \
            cref = nextFrame->crefstack.data; \
        } \
        SharemindModuleApi0x1Error st; \
        st = (*rc)(nextFrame->stack.data, nextFrame->stack.size, \
                   ref, cref, \
                   (r), &p->syscallContext); \
        if (hasRefs) \
            SharemindReferenceVector_pop(&nextFrame->refstack); \
        if (hasCRefs) \
            SharemindCReferenceVector_pop(&nextFrame->crefstack); \
        SharemindStackFrame_destroy(nextFrame); \
        SharemindFrameStack_pop(&p->frames); \
        p->nextFrame = NULL; \
        switch (st) { \
            case SHAREMIND_MODULE_API_0x1_OK: \
                /* Success! */ \
                break; \
            case SHAREMIND_MODULE_API_0x1_OUT_OF_MEMORY: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_OUT_OF_MEMORY); \
                break; \
            case SHAREMIND_MODULE_API_0x1_SHAREMIND_ERROR: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_SHAREMIND_ERROR_IN_SYSCALL); \
                break; \
            case SHAREMIND_MODULE_API_0x1_MODULE_ERROR: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_MODULE_ERROR_IN_SYSCALL); \
                break; \
            case SHAREMIND_MODULE_API_0x1_INVALID_CALL: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_INVALID_SYSCALL_INVOCATION); \
                break; \
            case SHAREMIND_MODULE_API_0x1_GENERAL_ERROR: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_GENERAL_SYSCALL_FAILURE); \
                break; \
            default: \
                SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_UNKNOWN_SYSCALL_RETURN_CODE); \
                break; \
        } \
    } else (void) 0

#define SHAREMIND_MI_CHECK_SYSCALL(a,r,nargs) \
    if (1) { \
        SHAREMIND_MI_TRY_EXCEPT((a)->uint64[0] < p->program->bindings.size, \
                                SHAREMIND_VM_PROCESS_INVALID_INDEX_SYSCALL); \
        SHAREMIND_MI_SYSCALL(&p->program->bindings.data[(size_t) (a)->uint64[0]],r,nargs); \
    } else (void) 0

#define SHAREMIND_MI_RETURN(r) \
    if (1) { \
        if (unlikely(p->nextFrame)) { \
            SharemindStackFrame_destroy(p->nextFrame); \
            SharemindFrameStack_pop(&p->frames); \
            p->nextFrame = NULL; \
        } \
        if (likely(p->thisFrame->returnAddr)) { \
            if (p->thisFrame->returnValueAddr) \
                *p->thisFrame->returnValueAddr = (r); \
            ip = p->thisFrame->returnAddr; \
            SharemindStackFrame_destroy(p->thisFrame); \
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
#define SHAREMIND_MI_CONVERT_float32_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_float32_TO_int16(a,b) a = (int16_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int32(a,b) a = (int32_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int64(a,b) a = (int64_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_int8(a,b) a = (int8_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(a,b) a = (uint16_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(a,b) a = (uint32_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(a,b) a = (uint64_t) b
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(a,b) a = (uint8_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_float64_TO_int16(a,b) a = (int16_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_int32(a,b) a = (int32_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_int64(a,b) a = (int64_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_int8(a,b) a = (int8_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_uint16(a,b) a = (uint16_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_uint32(a,b) a = (uint32_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_uint64(a,b) a = (uint64_t) b
#define SHAREMIND_MI_CONVERT_float64_TO_uint8(a,b) a = (uint8_t) b
#define SHAREMIND_MI_CONVERT_int16_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int16_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_int32_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int32_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_int64_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int64_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_int8_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_int8_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint16_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint32_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint64_TO_float64(a,b) a = (double) b
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(a,b) a = (float) b
#define SHAREMIND_MI_CONVERT_uint8_TO_float64(a,b) a = (double) b
#else
#define SHAREMIND_MI_CONVERT_float32_TO_float64(a,b) SHAREMIND_SF_FPU64F((a),sf_float32_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int16(a,b)  SHAREMIND_SF_FPU32I((a),sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int32(a,b)  SHAREMIND_SF_FPU32I((a),sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int64(a,b)  SHAREMIND_SF_FPU64I((a),sf_float32_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int8(a,b)   SHAREMIND_SF_FPU32I((a),sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(a,b) SHAREMIND_SF_FPU32I((a),sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(a,b) SHAREMIND_SF_FPU64I((a),sf_float32_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(a,b) SHAREMIND_SF_FPU64I((a),sf_float32_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(a,b)  SHAREMIND_SF_FPU32I((a),sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_float32(a,b) SHAREMIND_SF_FPU32F((a),sf_float64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int16(a,b)  SHAREMIND_SF_FPU32I((a),sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int32(a,b)  SHAREMIND_SF_FPU32I((a),sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int64(a,b)  SHAREMIND_SF_FPU64I((a),sf_float64_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int8(a,b)   SHAREMIND_SF_FPU32I((a),sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint16(a,b) SHAREMIND_SF_FPU32I((a),sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint32(a,b) SHAREMIND_SF_FPU64I((a),sf_float64_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint64(a,b) SHAREMIND_SF_FPU64I((a),sf_float64_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint8(a,b)  SHAREMIND_SF_FPU32I((a),sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int16_TO_float32(a,b)  SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int16_TO_float64(a,b)  SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int32_TO_float32(a,b)  SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int32_TO_float64(a,b)  SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int64_TO_float32(a,b)  SHAREMIND_SF_FPU32F((a),sf_int64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int64_TO_float64(a,b)  SHAREMIND_SF_FPU64F((a),sf_int64_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int8_TO_float32(a,b)   SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int8_TO_float64(a,b)   SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(a,b) SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint16_TO_float64(a,b) SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(a,b) SHAREMIND_SF_FPU32F((a),sf_int64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint32_TO_float64(a,b) SHAREMIND_SF_FPU64F((a),sf_int64_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(a,b) SHAREMIND_SF_FPU32F((a),sf_int64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint64_TO_float64(a,b) SHAREMIND_SF_FPU64F((a),sf_int64_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(a,b)  SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint8_TO_float64(a,b)  SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#endif

#define SHAREMIND_MI_MEM_CAN_READ(slot) ((slot)->specials == NULL || (slot)->specials->readable)
#define SHAREMIND_MI_MEM_CAN_WRITE(slot) ((slot)->specials == NULL || (slot)->specials->writeable)

#define SHAREMIND_MI_MEM_ALLOC(dptr,sizereg) \
    if (1) { \
        (dptr)->uint64[0] = SharemindProcess_public_alloc(p, (sizereg)->uint64[0], NULL); \
    } else (void) 0

#define SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    if (1) { \
        (dest) = SharemindMemoryMap_get(&p->memoryMap, (index)); \
        SHAREMIND_MI_TRY_EXCEPT((dest),SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE); \
    } else (void) 0

#define SHAREMIND_MI_MEM_FREE(ptr) \
    if (1) { \
        SharemindVmProcessException e = SharemindProcess_public_free(p, (ptr)->uint64[0]); \
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
        SharemindProcess * const p, \
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

SharemindVmError sharemind_vm_run(
        SharemindProcess * const p,
        const SharemindInnerCommand sharemind_vm_run_command,
        void * const sharemind_vm_run_data)
{
    if (sharemind_vm_run_command == SHAREMIND_I_GET_IMPL_LABEL) {

        assert(!p);

#ifndef SHAREMIND_FAST_BUILD
        typedef void * ImplLabelType;
#define SHAREMIND_CBPTR p
        typedef void * CbPtrType;
#define SHAREMIND_IMPL_LABEL(name) && label_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static const ImplLabelType system_labels[] = { && eof, && except, && halt, && trap };
#else
        typedef HaltCode (* ImplLabelType)(
                SharemindProcess * const p,
                const SharemindCodeBlock * const codeStart,
                const SharemindCodeBlock * ip,
                SharemindRegisterVector * const globalStack,
                SharemindRegisterVector * thisStack,
                const SharemindReferenceVector * thisRefStack,
                const SharemindCReferenceVector * thisCRefStack);
#define SHAREMIND_CBPTR fp
        typedef void (* CbPtrType)(void);
#define SHAREMIND_IMPL_LABEL(name) & func_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static const ImplLabelType system_labels[] = { &_func_impl_eof, &_func_impl_except, &_func_impl_halt, &_func_impl_trap };
#endif
        /* static void *(*labels[3])[] = { &instr_labels, &empty_impl_labels, &system_labels }; */

        SharemindPreparationBlock * pb = (SharemindPreparationBlock *) sharemind_vm_run_data;
        switch (pb->type) {
            case 0:
                pb->block->SHAREMIND_CBPTR[0] = (CbPtrType) instr_labels[pb->block->uint64[0]];
                break;
            case 1:
                pb->block->SHAREMIND_CBPTR[0] = (CbPtrType) empty_impl_labels[pb->block->uint64[0]];
                break;
            case 2:
                pb->block->SHAREMIND_CBPTR[0] = (CbPtrType) system_labels[pb->block->uint64[0]];
                break;
            default:
                abort();
        }
        return SHAREMIND_VM_OK;

    } else if (sharemind_vm_run_command == SHAREMIND_I_RUN || sharemind_vm_run_command == SHAREMIND_I_CONTINUE) {

        assert(p);

#pragma STDC FENV_ACCESS ON

        const SharemindCodeBlock * const codeStart = p->program->codeSections.data[p->currentCodeSectionIndex].data;
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

        _SharemindProcess_setupSignalHandlers(p);
#endif /* #ifndef SHAREMIND_SOFT_FLOAT */

        SHAREMIND_VM_TIMING_START

#ifndef SHAREMIND_FAST_BUILD
        SHAREMIND_MI_DISPATCH(ip);

        #include <sharemind/m4/dispatches.h>
#else
        SHAREMIND_VM_TIMING
        HaltCode haltCode = SHAREMIND_DISPATCH(ip);
        SHAREMIND_VM_TIMING_STOP
        SHAREMIND_STATIC_ASSERT(sizeof(haltCode) <= sizeof(int));
        switch ((int) haltCode) {
            case HC_EOF:
                goto eof;
            case HC_EXCEPT:
                goto except;
            case HC_HALT:
                goto halt;
            case HC_TRAP:
                goto trap;
            default:
                abort(); /* False positive with -Wunreachable-code. Used for debugging libvm. */
        }
#endif

        eof:
            p->exceptionValue = SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS;

        except:
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_VM_TIMING_STOP
#endif
            assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
            assert(p->exceptionValue != SHAREMIND_VM_PROCESS_OK);
            assert(SharemindVmProcessException_toString((SharemindVmProcessException) p->exceptionValue) != 0);
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProcess_restoreSignalHandlers(p);
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
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_VM_TIMING_STOP
#endif
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProcess_restoreSignalHandlers(p);
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
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_VM_TIMING_STOP
#endif
#ifndef SHAREMIND_SOFT_FLOAT
            _SharemindProcess_restoreSignalHandlers(p);
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
