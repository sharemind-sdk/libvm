/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include "core.h"

#include <cstddef>
#ifdef SHAREMIND_DEBUG
#include <cstdio>
#endif
#include <cstring>
#include <limits.h>
#include <sharemind/AssertReturn.h>
#include <sharemind/libsoftfloat/softfloat.h>
#include <sharemind/null.h>
#include <sharemind/restrict.h>
#include "preparationblock.h"
#include "process.h"
#include "program.h"


typedef sf_float32 SharemindFloat32;
typedef sf_float64 SharemindFloat64;

#define SHAREMIND_T_CodeBlock SharemindCodeBlock
#define SHAREMIND_T_CReference sharemind::CReference
#define SHAREMIND_T_MemorySlot sharemind::MemorySlot
#define SHAREMIND_T_Reference sharemind::Reference

#ifdef SHAREMIND_DEBUG
#define SHAREMIND_DEBUG_PRINTSTATE \
    do { \
        SHAREMIND_UPDATESTATE; \
        SharemindProcess_print_state_bencoded(p, stderr); \
        fprintf(stderr, "\n"); \
    } while ((0))
#else
#define SHAREMIND_DEBUG_PRINTSTATE (void) 0
#endif

#define SHAREMIND_MI_BITS_ALL(type) ((type) (((type) 0) | ~((type) 0)))
#define SHAREMIND_MI_BITS_NONE(type) ((type) ~SHAREMIND_MI_BITS_ALL(type))

#define SHAREMIND_MI_ULOW_BIT_MASK(type,bits,n) \
    ((type) (SHAREMIND_MI_BITS_ALL(type) >> ((bits) - (n))))
#define SHAREMIND_MI_UHIGH_BIT_MASK(type,bits,n) \
    ((type) (SHAREMIND_MI_BITS_ALL(type) << ((bits) - (n))))
#define SHAREMIND_MI_ULOW_BIT_FILTER(type,bits,v,n) \
    ((type) (((type) (v)) & SHAREMIND_MI_ULOW_BIT_MASK(type, (bits), (n))))
#define SHAREMIND_MI_UHIGH_BIT_FILTER(type,bits,v,n) \
    ((type) (((type) (v)) & SHAREMIND_MI_UHIGH_BIT_MASK(type, (bits), (n))))

#define SHAREMIND_MI_USHIFT_1_LEFT_0(type,v) \
    ((type) (((type) (v)) << ((type) 1u)))
#define SHAREMIND_MI_USHIFT_1_RIGHT_0(type,v) \
    ((type) (((type) (v)) >> ((type) 1u)))

#define SHAREMIND_MI_USHIFT_1_LEFT_1(type,bits,v) \
    ((type) (SHAREMIND_MI_USHIFT_1_LEFT_0(type, (v)) \
             | SHAREMIND_MI_ULOW_BIT_MASK(type, (bits), 1u)))
#define SHAREMIND_MI_USHIFT_1_RIGHT_1(type,bits,v) \
    ((type) (SHAREMIND_MI_USHIFT_1_RIGHT_0(type, (v)) \
             | SHAREMIND_MI_UHIGH_BIT_MASK(type, (bits), 1u)))

#define SHAREMIND_MI_USHIFT_1_LEFT_EXTEND(type,bits,v) \
    ((type) (SHAREMIND_MI_USHIFT_1_LEFT_0(type, (v)) \
             | SHAREMIND_MI_ULOW_BIT_FILTER(type, (bits), (v), 1u)))
#define SHAREMIND_MI_USHIFT_1_RIGHT_EXTEND(type,bits,v) \
    ((type) (SHAREMIND_MI_USHIFT_1_RIGHT_0(type, (v)) \
             | SHAREMIND_MI_UHIGH_BIT_FILTER(type, (bits), (v), 1u)))

#define SHAREMIND_MI_USHIFT_LEFT_0(type,bits,v,s) \
    ((type) ((s) >= (bits) \
             ? SHAREMIND_MI_BITS_NONE(type) \
             : (type) (((type) (v)) << ((type) (s)))))
#define SHAREMIND_MI_USHIFT_RIGHT_0(type,bits,v,s) \
    ((type) ((s) >= (bits) \
             ? SHAREMIND_MI_BITS_NONE(type) \
             : (type) (((type) (v)) >> ((type) (s)))))

#define SHAREMIND_MI_USHIFT_LEFT_1(type,bits,v,s) \
    ((type) ((s) >= (bits) \
             ? SHAREMIND_MI_BITS_ALL(type) \
             : (type) (((type) (((type) (v)) << ((type) (s)))) \
                       | SHAREMIND_MI_ULOW_BIT_MASK(type, (bits), (s)))))
#define SHAREMIND_MI_USHIFT_RIGHT_1(type,bits,v,s) \
    ((type) ((s) >= (bits) \
             ? SHAREMIND_MI_BITS_ALL(type) \
             : (type) (((type) (((type) (v)) >> ((type) (s)))) \
                       | SHAREMIND_MI_UHIGH_BIT_MASK(type, (bits), (s)))))

#define SHAREMIND_MI_USHIFT_LEFT_EXTEND(type,bits,v,s) \
    ((type)(SHAREMIND_MI_ULOW_BIT_FILTER(type, (bits), (v), 1u) \
            ? SHAREMIND_MI_USHIFT_LEFT_1(type, (bits), (v), (s)) \
            : SHAREMIND_MI_USHIFT_LEFT_0(type, (bits), (v), (s))))
#define SHAREMIND_MI_USHIFT_RIGHT_EXTEND(type,bits,v,s) \
    ((type)(SHAREMIND_MI_UHIGH_BIT_FILTER(type, (bits), (v), 1u) \
            ? SHAREMIND_MI_USHIFT_RIGHT_1(type, (bits), (v), (s)) \
            : SHAREMIND_MI_USHIFT_RIGHT_0(type, (bits), (v), (s))))

#define SHAREMIND_MI_UROTATE_LEFT(type,bits,v,s) \
    (((s) % bits) \
     ? ((type) ((SHAREMIND_MI_UHIGH_BIT_FILTER(type, (bits), (v), \
                                               ((s) % (bits))) \
                 >> ((bits) - ((s) % bits))) \
                | (SHAREMIND_MI_ULOW_BIT_FILTER(type, (bits), (v), \
                                                ((bits) - ((s) % (bits)))) \
                   << ((s) % bits)))) \
     : ((type) (v)))
#define SHAREMIND_MI_UROTATE_RIGHT(type,bits,v,s) \
    (((s) % bits) \
     ? ((type) ((SHAREMIND_MI_UHIGH_BIT_FILTER(type, (bits), (v), \
                                               ((bits) - ((s) % (bits)))) \
                 >> ((s) % bits)) \
                | (SHAREMIND_MI_ULOW_BIT_FILTER(type, (bits), (v), \
                                                ((s) % (bits))) \
                   << ((bits) - ((s) % bits))))) \
     : ((type) (v)))


#define SHAREMIND_MI_FPU_STATE (p->fpuState)
#define SHAREMIND_MI_FPU_STATE_SET(v) \
    do { p->fpuState = (sf_fpu_state) (v); } while(0)
#define SHAREMIND_SF_E(type,dest,n,...) \
    do { \
        p->fpuState = p->fpuState & ~sf_fpu_state_exception_mask; \
        type const r = __VA_ARGS__; \
        (dest) = n r.result; \
        p->fpuState = r.fpu_state; \
        sf_fpu_state const e = \
                (r.fpu_state & sf_fpu_state_exception_mask) \
                 & ((r.fpu_state & sf_fpu_state_exception_crash_mask) << 5u); \
        if (unlikely(e)) { \
            if (e & sf_float_flag_divbyzero) { \
                p->exceptionValue = \
                    SHAREMIND_VM_PROCESS_FLOATING_POINT_DIVIDE_BY_ZERO; \
            } else if (e & sf_float_flag_overflow) { \
                p->exceptionValue = \
                    SHAREMIND_VM_PROCESS_FLOATING_POINT_OVERFLOW; \
            } else if (e & sf_float_flag_underflow) { \
                p->exceptionValue = \
                    SHAREMIND_VM_PROCESS_FLOATING_POINT_UNDERFLOW; \
            } else if (e & sf_float_flag_inexact) { \
                p->exceptionValue = \
                    SHAREMIND_VM_PROCESS_FLOATING_POINT_INEXACT_RESULT; \
            } else if (e & sf_float_flag_invalid) { \
                p->exceptionValue = \
                    SHAREMIND_VM_PROCESS_FLOATING_POINT_INVALID_OPERATION; \
            } else { \
                p->exceptionValue = SHAREMIND_VM_PROCESS_UNKNOWN_FPE; \
            } \
            SHAREMIND_DO_EXCEPT; \
        } \
    } while ((0))
#define SHAREMIND_SF_E_CAST(dest,destType,immedType,resultType,...) \
    do { \
        immedType r_; \
        SHAREMIND_SF_E(resultType,r_,__VA_ARGS__); \
        (dest) = (destType) r_; \
    } while(0)
#define SHAREMIND_SF_FPU32F(dest,...) \
    SHAREMIND_SF_E(sf_result32f,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPU64F(dest,...) \
    SHAREMIND_SF_E(sf_result64f,(dest),,__VA_ARGS__)
/*
#define SHAREMIND_SF_FPU32I(dest,...) \
    SHAREMIND_SF_E(sf_result32i,(dest),,__VA_ARGS__)
#define SHAREMIND_SF_FPU64I(dest,...) \
    SHAREMIND_SF_E(sf_result64i,(dest),,__VA_ARGS__)
*/
#define SHAREMIND_SF_FPUF(dest,...) \
    SHAREMIND_SF_E_CAST(dest,uint64_t,sf_flag,sf_resultFlag,__VA_ARGS__)
#define SHAREMIND_MI_UNEG_FLOAT32(d) do { (d) = sf_float32_neg(d); } while (0)
#define SHAREMIND_MI_UNEG_FLOAT64(d) do { (d) = sf_float64_neg(d); } while (0)
#define SHAREMIND_MI_UINC_FLOAT32(d) \
    SHAREMIND_SF_FPU32F((d),sf_float32_add((d),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_UINC_FLOAT64(d) \
    SHAREMIND_SF_FPU64F((d),sf_float64_add((d),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_UDEC_FLOAT32(d) \
    SHAREMIND_SF_FPU32F((d),sf_float32_sub((d),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_UDEC_FLOAT64(d) \
    SHAREMIND_SF_FPU64F((d),sf_float64_sub((d),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BNEG_FLOAT32(d,s) do { (d) = sf_float32_neg(s); } while (0)
#define SHAREMIND_MI_BNEG_FLOAT64(d,s) do { (d) = sf_float64_neg(s); } while (0)
#define SHAREMIND_MI_BINC_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_add((s),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_BINC_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_add((s),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_sub((s),sf_float32_one,p->fpuState))
#define SHAREMIND_MI_BDEC_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_sub((s),sf_float64_one,p->fpuState))
#define SHAREMIND_MI_BADD_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_add((d),(s),p->fpuState))
#define SHAREMIND_MI_BADD_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_add((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_sub((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_sub((d),(s),p->fpuState))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_sub((s),(d),p->fpuState))
#define SHAREMIND_MI_BSUB2_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_sub((s),(d),p->fpuState))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_mul((d),(s),p->fpuState))
#define SHAREMIND_MI_BMUL_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_mul((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_div((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_div((d),(s),p->fpuState))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_div((s),(d),p->fpuState))
#define SHAREMIND_MI_BDIV2_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_div((s),(d),p->fpuState))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_rem((d),(s),p->fpuState))
#define SHAREMIND_MI_BMOD_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_rem((d),(s),p->fpuState))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) \
    SHAREMIND_SF_FPU32F((d),sf_float32_rem((s),(d),p->fpuState))
#define SHAREMIND_MI_BMOD2_FLOAT64(d,s) \
    SHAREMIND_SF_FPU64F((d),sf_float64_rem((s),(d),p->fpuState))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPU32F((d),sf_float32_add((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TADD_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPU64F((d),sf_float64_add((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPU32F((d),sf_float32_sub((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TSUB_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPU64F((d),sf_float64_sub((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPU32F((d),sf_float32_mul((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMUL_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPU64F((d),sf_float64_mul((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPU32F((d),sf_float32_div((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TDIV_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPU64F((d),sf_float64_div((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPU32F((d),sf_float32_rem((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TMOD_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPU64F((d),sf_float64_rem((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TEQ_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float32_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TEQ_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float64_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TNE_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),!,sf_float32_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TNE_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),!,sf_float64_eq((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLT_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float32_lt((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLT_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float64_lt((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLE_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float32_le((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TLE_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float64_le((s1),(s2),p->fpuState))
#define SHAREMIND_MI_TGT_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float32_lt((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGT_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float64_lt((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGE_FLOAT32(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float32_le((s2),(s1),p->fpuState))
#define SHAREMIND_MI_TGE_FLOAT64(d,s1,s2) \
    SHAREMIND_SF_FPUF((d),,sf_float64_le((s2),(s1),p->fpuState))

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DO_EXCEPT do { goto except; } while ((0))
#define SHAREMIND_DO_HALT   do { goto halt;   } while ((0))
#define SHAREMIND_DO_TRAP   do { goto trap;   } while ((0))
#else
typedef enum { HC_EOF, HC_EXCEPT, HC_HALT, HC_TRAP, HC_NEXT } HaltCode;
#define SHAREMIND_DO_EXCEPT do { return HC_EXCEPT; } while ((0))
#define SHAREMIND_DO_HALT   do { return HC_HALT;   } while ((0))
#define SHAREMIND_DO_TRAP   do { return HC_TRAP;   } while ((0))
#endif

#define SHAREMIND_UPDATESTATE \
    do { \
        p->currentIp = (uintptr_t) (ip - codeStart); \
    } while ((0))

/* Micro-instructions (SHAREMIND_MI_*:) */

#define SHAREMIND_MI_NOP (void) 0
#define SHAREMIND_MI_HALT(r) \
    do { \
        p->returnValue = (r); \
        SHAREMIND_DO_HALT; \
    } while ((0))
#define SHAREMIND_MI_TRAP SHAREMIND_DO_TRAP

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DISPATCH(ip) do { goto *((ip)->p[0]); } while(0)
#define SHAREMIND_MI_DISPATCH(ip) \
    do { SHAREMIND_DISPATCH(ip); } while ((0))
#else
#define SHAREMIND_DISPATCH(ip) \
    do { (void) (ip); SHAREMIND_UPDATESTATE; return HC_NEXT; } while ((0))
#define SHAREMIND_MI_DISPATCH(ip) \
    do { (void) (ip); SHAREMIND_UPDATESTATE; return HC_NEXT; } while ((0))
#endif

#define SHAREMIND_TRAP_CHECK \
    (__atomic_exchange_n(&p->trapCond, false, __ATOMIC_ACQUIRE))

#define SHAREMIND_CHECK_TRAP \
    if (SHAREMIND_TRAP_CHECK) { SHAREMIND_DO_TRAP; } else (void) 0

#define SHAREMIND_CHECK_TRAP_AND_DISPATCH(ip) \
    do { SHAREMIND_CHECK_TRAP; SHAREMIND_DISPATCH((ip)); } while ((0))

#define SHAREMIND_MI_JUMP_REL(n) \
    do { \
        ip += (n)->int64[0]; \
        SHAREMIND_CHECK_TRAP_AND_DISPATCH(ip); \
    } while ((0))

#define SHAREMIND_MI_IS_INSTR(i) \
    (p->program->codeSections[p->currentCodeSectionIndex] \
            .isInstructionAtOffset((size_t) (i)))

#define SHAREMIND_MI_CHECK_JUMP_REL(reladdr) \
    do { \
        uintptr_t const unsignedIp = (uintptr_t) (ip - codeStart); \
        if ((reladdr) < 0) { \
            SHAREMIND_MI_TRY_EXCEPT( \
                ((uint64_t) -((reladdr) + 1)) < unsignedIp, \
                SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } else { \
            SHAREMIND_MI_TRY_EXCEPT( \
                ((uint64_t) (reladdr)) \
                < p->program->codeSections[p->currentCodeSectionIndex].size() \
                  - unsignedIp - 1u, \
                SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        } \
        SharemindCodeBlock const * const tip = ip + (reladdr); \
        SHAREMIND_MI_TRY_EXCEPT(SHAREMIND_MI_IS_INSTR(tip - codeStart), \
                                SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        ip = tip; \
        SHAREMIND_CHECK_TRAP_AND_DISPATCH(ip); \
    } while ((0))

#define SHAREMIND_MI_DO_EXCEPT(e) \
    do { \
        p->exceptionValue = (e); \
        SHAREMIND_DO_EXCEPT; \
    } while ((0))

#define SHAREMIND_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SHAREMIND_MI_DO_EXCEPT((e)); \
    } else (void) 0

#define SHAREMIND_MI_DO_OOM \
    SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_OUT_OF_MEMORY)

#define SHAREMIND_MI_TRY_OOM(e) \
    SHAREMIND_MI_TRY_EXCEPT((e),SHAREMIND_VM_PROCESS_OUT_OF_MEMORY)

#define SHAREMIND_MI_TRY_MEMRANGE(size,offset,numBytes,exception) \
    SHAREMIND_MI_TRY_EXCEPT(((offset) < (size)) \
                       && ((numBytes) <= (size)) \
                       && ((size) - (offset) >= (numBytes)), \
                       (exception))

#define SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME_(...) \
    if (!SHAREMIND_MI_HAS_STACK) { \
        try { \
            p->frames.emplace_back(); \
        } catch (...) { \
            __VA_ARGS__ \
            SHAREMIND_MI_DO_OOM; \
        } \
        p->nextFrame = &p->frames.back(); \
    } else (void)0

#define SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME \
    SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME_()

#define SHAREMIND_MI_PUSH(v) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        try { \
            p->nextFrame->stack.emplace_back(v); \
        } catch (...) { \
            SHAREMIND_MI_DO_OOM; \
        } \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_BLOCK_(prefix,value,b,bOffset,rSize) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        try { \
            p->nextFrame->value.emplace_back(nullptr, \
                                             &(b)->uint8[0] + (bOffset), \
                                             (rSize)); \
        } catch (...) { \
            SHAREMIND_MI_DO_OOM; \
        } \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_BLOCK_ref(b) \
    SHAREMIND_MI_PUSHREF_BLOCK_(Ref, \
                                refstack, \
                                (b), \
                                0u, \
                                sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREF_BLOCK_cref(b) \
    SHAREMIND_MI_PUSHREF_BLOCK_(CRef, \
                                crefstack, \
                                (b), \
                                0u, \
                                sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_ref(b,o,s) \
    SHAREMIND_MI_PUSHREF_BLOCK_(Ref, refstack,  (b), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_cref(b,o,s) \
    SHAREMIND_MI_PUSHREF_BLOCK_(CRef,crefstack, (b), (o), (s))

#define SHAREMIND_MI_PUSHREF_REF_(prefix,value,constPerhaps,r,rOffset,rSize) \
    do { \
        auto const internal = (r)->internal; \
        bool deref = false; \
        if (internal) { \
            if (!((SHAREMIND_T_MemorySlot *) (r)->internal)->ref()) \
                { SHAREMIND_MI_DO_OOM; }\
            deref = true; \
        } \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME_( \
                if (deref) \
                    ((SHAREMIND_T_MemorySlot *) (r)->internal)->deref(); \
        ); \
        try { \
            p->nextFrame->value.emplace_back( \
                internal, \
                static_cast<constPerhaps uint8_t *>((r)->pData) + (rOffset), \
                (rSize)); \
        } catch (...) { \
            if (deref) \
                ((SHAREMIND_T_MemorySlot *) (r)->internal)->deref(); \
            SHAREMIND_MI_DO_OOM; \
        } \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_REF_ref(r) \
    SHAREMIND_MI_PUSHREF_REF_(Ref, refstack,,        (r), 0u, (r)->size)
#define SHAREMIND_MI_PUSHREF_REF_cref(r) \
    SHAREMIND_MI_PUSHREF_REF_(CRef,crefstack, const, (r), 0u, (r)->size)
#define SHAREMIND_MI_PUSHREFPART_REF_ref(r,o,s) \
    SHAREMIND_MI_PUSHREF_REF_(Ref, refstack,,        (r), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_REF_cref(r,o,s) \
    SHAREMIND_MI_PUSHREF_REF_(CRef,crefstack, const, (r), (o), (s))

#define SHAREMIND_MI_PUSHREF_MEM_(prefix,value,slot,mOffset,rSize) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        if (!(slot)->ref()) \
            { SHAREMIND_MI_DO_OOM; } \
        try { \
            p->nextFrame->value.emplace_back( \
                    (slot), \
                    (rSize) \
                    ? (static_cast<uint8_t *>((slot)->data()) + (mOffset)) \
                    /* Non-NULL invalid pointer so as not to signal end of */ \
                    /* (c)refs: */ \
                    : sharemind::assertReturn( \
                                static_cast<uint8_t *>(nullptr) + 1u), \
                    (rSize)); \
        } catch (...) { \
            (slot)->deref(); \
            SHAREMIND_MI_DO_OOM; \
        } \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_MEM_ref(slot) \
    SHAREMIND_MI_PUSHREF_MEM_(Ref, refstack,  (slot), 0u, (slot)->size())
#define SHAREMIND_MI_PUSHREF_MEM_cref(slot) \
    SHAREMIND_MI_PUSHREF_MEM_(CRef,crefstack, (slot), 0u, (slot)->size())
#define SHAREMIND_MI_PUSHREFPART_MEM_ref(slot,o,s) \
    SHAREMIND_MI_PUSHREF_MEM_(Ref, refstack,  (slot), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_MEM_cref(slot,o,s) \
    SHAREMIND_MI_PUSHREF_MEM_(CRef,crefstack, (slot), (o), (s))

#define SHAREMIND_MI_RESIZE_STACK(size) \
    do { \
        try { \
            thisStack->resize(size, SharemindCodeBlock{0}); \
        } catch (...) { \
            SHAREMIND_MI_DO_OOM; \
        } \
    } while (false)

#define SHAREMIND_MI_CLEAR_STACK \
    do { \
        p->nextFrame->stack.clear(); \
        p->nextFrame->refstack.clear(); \
        p->nextFrame->crefstack.clear(); \
    } while ((0))

#define SHAREMIND_MI_HAS_STACK (!!(p->nextFrame))

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_CALL_RETURN_DISPATCH(ip) \
    do { \
        thisStack = &p->thisFrame->stack; \
        thisRefStack = &p->thisFrame->refstack; \
        thisCRefStack = &p->thisFrame->crefstack; \
        SHAREMIND_CHECK_TRAP_AND_DISPATCH((ip)); \
    } while ((0))
#else
#define SHAREMIND_CALL_RETURN_DISPATCH(ip) \
    SHAREMIND_CHECK_TRAP_AND_DISPATCH((ip))
#endif

#define SHAREMIND_MI_CALL(a,r,nargs) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = nullptr; \
        ip = codeStart + ((size_t) (a)); \
        SHAREMIND_CALL_RETURN_DISPATCH(ip); \
    } while ((0))

#define SHAREMIND_MI_CHECK_CALL(a,r,nargs) \
    do { \
        SHAREMIND_MI_TRY_EXCEPT( \
                SHAREMIND_MI_IS_INSTR((a)->uint64[0]), \
                SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS); \
        SHAREMIND_MI_CALL((a)->uint64[0],r,nargs); \
    } while ((0))

#define SHAREMIND_MI_SYSCALL(sc,r,nargs) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        auto * nextFrame = p->nextFrame; \
        nextFrame->returnValueAddr = (r); \
        SharemindSyscallCallable const rc = \
            ((SharemindSyscallWrapper const *) sc)->callable; \
        p->syscallContext.moduleHandle = \
            ((SharemindSyscallWrapper const *) sc)->internal; \
        SHAREMIND_T_Reference * ref; \
        bool hasRefs = !nextFrame->refstack.empty(); \
        if (!hasRefs) { \
            ref = nullptr; \
        } else { \
            try { \
                nextFrame->refstack.emplace_back(nullptr, nullptr, 0u); \
            } catch (...) { \
                SHAREMIND_MI_DO_OOM; \
            } \
            ref = nextFrame->refstack.data(); \
        } \
        bool hasCRefs = !nextFrame->crefstack.empty(); \
        sharemind::CReference * cref; \
        if (!hasCRefs) { \
            cref = nullptr; \
        } else { \
            try { \
                nextFrame->crefstack.emplace_back(nullptr, nullptr, 0u);\
            } catch (...) { \
                SHAREMIND_MI_DO_OOM; \
            } \
            cref = nextFrame->crefstack.data(); \
        } \
        SharemindModuleApi0x1Error const st = \
                (*rc)(nextFrame->stack.data(), nextFrame->stack.size(), \
                        ref, cref, \
                        (r), &p->syscallContext); \
        if (hasRefs) \
            nextFrame->refstack.pop_back(); \
        if (hasCRefs) \
            nextFrame->crefstack.pop_back(); \
        p->frames.pop_back(); \
        p->nextFrame = nullptr; \
        if (st != SHAREMIND_MODULE_API_0x1_OK) { \
            p->syscallException = st; \
            SHAREMIND_MI_DO_EXCEPT(SHAREMIND_VM_PROCESS_SYSCALL_ERROR); \
        } \
        SHAREMIND_CHECK_TRAP; \
    } while ((0))

#define SHAREMIND_MI_CHECK_SYSCALL(a,r,nargs) \
    do { \
        SHAREMIND_MI_TRY_EXCEPT((a)->uint64[0] < p->program->bindings.size(), \
                                SHAREMIND_VM_PROCESS_INVALID_INDEX_SYSCALL); \
        SHAREMIND_MI_SYSCALL( \
            &p->program->bindings[(size_t) (a)->uint64[0]], \
            r, \
            nargs); \
    } while ((0))

#define SHAREMIND_MI_RETURN(r) \
    do { \
        if (unlikely(p->nextFrame)) { \
            p->frames.pop_back(); \
            p->nextFrame = nullptr; \
        } \
        if (likely(p->thisFrame->returnAddr)) { \
            if (p->thisFrame->returnValueAddr) \
                *p->thisFrame->returnValueAddr = (r); \
            ip = p->thisFrame->returnAddr; \
            p->frames.pop_back(); \
            p->thisFrame = &p->frames.back(); \
            SHAREMIND_CALL_RETURN_DISPATCH(ip); \
        } else { \
            SHAREMIND_MI_HALT((r)); \
        } \
    } while ((0))

#define SHAREMIND_MI_GET_(d,i,source,exception,isconst) \
    do { \
        if ((i) >= (source)->size()) \
            SHAREMIND_MI_DO_EXCEPT((exception)); \
        (d) = &(*(source))[(i)]; \
    } while ((0))

#define SHAREMIND_MI_GET_stack(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      thisStack, \
                      SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK,)
#define SHAREMIND_MI_GET_reg(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      globalStack, \
                      SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER,)
#define SHAREMIND_MI_GET_CONST_stack(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      thisStack, \
                      SHAREMIND_VM_PROCESS_INVALID_INDEX_STACK, \
                      const_)
#define SHAREMIND_MI_GET_CONST_reg(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      globalStack, \
                      SHAREMIND_VM_PROCESS_INVALID_INDEX_REGISTER, \
                      const_)

#define SHAREMIND_MI_GET_REF_(r,i,type,exception) \
    do { \
        auto const & container = *this ## type ## Stack; \
        if (((i) < container.size())) { \
            (r) = &container[(i)]; \
        } else { \
            SHAREMIND_MI_DO_EXCEPT((exception)); \
        } \
    } while ((0))

#define SHAREMIND_MI_GET_ref(r,i) \
    SHAREMIND_MI_GET_REF_((r), \
                          (i), \
                          Ref, \
                          SHAREMIND_VM_PROCESS_INVALID_INDEX_REFERENCE)
#define SHAREMIND_MI_GET_cref(r,i) \
    SHAREMIND_MI_GET_REF_((r), \
                          (i), \
                          CRef, \
                          SHAREMIND_VM_PROCESS_INVALID_INDEX_CONST_REFERENCE)

#define SHAREMIND_MI_REFERENCE_GET_MEMORY_PTR(r) \
    ((SHAREMIND_T_MemorySlot *) (r)->internal)
#define SHAREMIND_MI_REFERENCE_GET_PTR(r) ((uint8_t *) (r)->pData)
#define SHAREMIND_MI_REFERENCE_GET_CONST_PTR(r) ((uint8_t const *) (r)->pData)
#define SHAREMIND_MI_REFERENCE_GET_SIZE(r) ((r)->size)

#define SHAREMIND_MI_BLOCK_AS(b,t) (b->t[0])
#define SHAREMIND_MI_BLOCK_AS_P(b,t) (&b->t[0])
#define SHAREMIND_MI_ARG_P(n)      (ip + (n))
#define SHAREMIND_MI_ARG_AS(n,t) \
    (SHAREMIND_MI_BLOCK_AS(SHAREMIND_MI_ARG_P(n),t))

#define SHAREMIND_MI_CONVERT_float32_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_float32_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int8(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int8_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int16(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int16_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int32(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int32_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_int64(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int64_t, \
                        sf_int64, \
                        sf_result64i,, \
                        sf_float32_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint8_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint16_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float32_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint32_t, \
                        sf_int64, \
                        sf_result64i,, \
                        sf_float32_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint64_t, \
                        sf_uint64, \
                        sf_result64ui,, \
                        sf_float32_to_uint64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_float64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int8(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int8_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int16(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int16_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int32(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int32_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_int64(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        int64_t, \
                        sf_int64, \
                        sf_result64i,, \
                        sf_float64_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint8(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint8_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint16(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint16_t, \
                        sf_int32, \
                        sf_result32i,, \
                        sf_float64_to_int32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint32(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint32_t, \
                        sf_int64, \
                        sf_result64i,, \
                        sf_float64_to_int64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_float64_TO_uint64(a,b) \
    SHAREMIND_SF_E_CAST((a), \
                        uint64_t, \
                        sf_uint64, \
                        sf_result64ui,, \
                        sf_float64_to_uint64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int8_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int8_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int16_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int16_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int32_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int32_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int64_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_int64_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int64_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint8_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int32_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint16_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int32_to_float64_fpu((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_int64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint32_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_int64_to_float64((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(a,b) \
    SHAREMIND_SF_FPU32F((a),sf_uint64_to_float32((b),p->fpuState))
#define SHAREMIND_MI_CONVERT_uint64_TO_float64(a,b) \
    SHAREMIND_SF_FPU64F((a),sf_uint64_to_float64((b),p->fpuState))

#define SHAREMIND_MI_MEM_GET_SIZE_FROM_SLOT(slot) ((slot)->size())
#define SHAREMIND_MI_MEM_GET_DATA_FROM_SLOT(slot) ((slot)->data())
#define SHAREMIND_MI_MEM_CAN_WRITE(slot) ((slot)->isWritable())

#define SHAREMIND_MI_MEM_ALLOC(dptr,sizereg) \
    do { \
        (dptr)->uint64[0] = \
            SharemindProcess_public_alloc(p, (sizereg)->uint64[0]); \
    } while ((0))

#define SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    do { \
        SHAREMIND_MI_TRY_EXCEPT(index != 0u, \
                                SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE); \
        auto const & v = p->memoryMap.get((index)); \
        SHAREMIND_MI_TRY_EXCEPT(v, SHAREMIND_VM_PROCESS_INVALID_MEMORY_HANDLE);\
        (dest) = v.get(); \
    } while ((0))

#define SHAREMIND_MI_MEM_FREE(ptr) \
    do { \
        SharemindVmProcessException const e = \
                SharemindProcess_public_free(p, (ptr)->uint64[0]); \
        if (unlikely(e != SHAREMIND_VM_PROCESS_OK)) { \
            SHAREMIND_MI_DO_EXCEPT(e); \
        } \
    } while ((0))

#define SHAREMIND_MI_MEM_GET_SIZE(ptr,sizedest) \
    do { \
        SHAREMIND_T_MemorySlot * slot; \
        SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        (sizedest)->uint64[0] = slot->size(); \
    } while ((0))

#define SHAREMIND_MI_MEMCPY memcpy
#define SHAREMIND_MI_MEMMOVE memmove

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_IMPL(name,code) \
    label_impl_ ## name : code
#else
#define SHAREMIND_IMPL_INNER(name,code) \
    static inline HaltCode name (SharemindProcess * const p) \
    { \
        auto const codeStart = \
            p->program->codeSections[p->currentCodeSectionIndex].constData();\
        SharemindCodeBlock const * ip = &codeStart[p->currentIp]; \
        auto * const globalStack = &p->globalFrame->stack; \
        auto * thisStack = &p->thisFrame->stack; \
        auto * thisRefStack = &p->thisFrame->refstack; \
        auto * thisCRefStack = &p->thisFrame->crefstack; \
        (void) codeStart; (void) ip; (void) globalStack; \
        (void) thisStack; (void) thisRefStack; (void) thisCRefStack; \
        code \
    }
#define SHAREMIND_IMPL(name,code) SHAREMIND_IMPL_INNER(func_impl_ ## name, code)

#include <sharemind/m4/dispatches.h>

SHAREMIND_IMPL_INNER(_func_impl_eof,return HC_EOF;)
SHAREMIND_IMPL_INNER(_func_impl_except,return HC_EXCEPT;)
SHAREMIND_IMPL_INNER(_func_impl_halt,return HC_HALT;)
SHAREMIND_IMPL_INNER(_func_impl_trap,return HC_TRAP;)

#endif

static char const * SharemindProcess_exceptionString(int64_t const e) {
    switch ((SharemindVmProcessException) e) {
        BOOST_PP_SEQ_FOR_EACH(
                    SHAREMIND_ENUM_CUSTOM_DEFINE_CUSTOM_TOSTRING_ELEM,
                    ("Process exited with an ")(" exception!"),
                    SHAREMIND_VM_PROCESS_EXCEPTION_ENUM)
        default: return "Process exited with an unknown exception!";
    }
}


SharemindVmError sharemind_vm_run(
        SharemindProcess * const p,
        SharemindInnerCommand const sharemind_vm_run_command,
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
        static ImplLabelType const system_labels[] =
                { && eof, && except, && halt, && trap };
#else
        typedef HaltCode (* ImplLabelType)(SharemindProcess * const p);
#define SHAREMIND_CBPTR fp
        typedef void (* CbPtrType)(void);
#define SHAREMIND_IMPL_LABEL(name) & func_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static ImplLabelType const system_labels[] = {&_func_impl_eof,
                                                      &_func_impl_except,
                                                      &_func_impl_halt,
                                                      &_func_impl_trap};
#endif

        SharemindPreparationBlock * pb =
                (SharemindPreparationBlock *) sharemind_vm_run_data;
        switch (pb->type) {
            case 0:
                pb->block->SHAREMIND_CBPTR[0] =
                        (CbPtrType) instr_labels[pb->block->uint64[0]];
                break;
            case 1:
                pb->block->SHAREMIND_CBPTR[0] =
                        (CbPtrType) empty_impl_labels[pb->block->uint64[0]];
                break;
            case 2:
                pb->block->SHAREMIND_CBPTR[0] =
                        (CbPtrType) system_labels[pb->block->uint64[0]];
                break;
            default:
                abort();
        }
        return SHAREMIND_VM_OK;

    } else if (sharemind_vm_run_command == SHAREMIND_I_RUN
               || sharemind_vm_run_command == SHAREMIND_I_CONTINUE)
    {
        assert(p);

#ifndef __GNUC__
#pragma STDC FENV_ACCESS ON
#endif
        auto const codeStart =
            p->program->codeSections[p->currentCodeSectionIndex].constData();

#ifndef SHAREMIND_FAST_BUILD
        SharemindCodeBlock const * ip = &codeStart[p->currentIp];
        auto * const globalStack = &p->globalFrame->stack;
        auto * thisStack = &p->thisFrame->stack;
        auto * thisRefStack = &p->thisFrame->refstack;
        auto * thisCRefStack = &p->thisFrame->crefstack;
#endif

        if (SHAREMIND_TRAP_CHECK)
            goto trap;

#ifndef SHAREMIND_FAST_BUILD
        SHAREMIND_MI_DISPATCH(ip);

        #include <sharemind/m4/dispatches.h>
#else
        {
            HaltCode haltCode;
            do {
                typedef HaltCode (* F)(SharemindProcess * const);
                haltCode = (*((F) (codeStart[p->currentIp].fp[0])))(p);
            } while (haltCode == HC_NEXT);
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
                    abort(); /* False positive with -Wunreachable-code.
                                Used for debugging libvm. */
            }
        }
#endif

        eof:
            p->exceptionValue = SHAREMIND_VM_PROCESS_JUMP_TO_INVALID_ADDRESS;

        except:
            assert(p->exceptionValue != SHAREMIND_VM_PROCESS_OK);
            assert(SharemindVmProcessException_toString(
                       (SharemindVmProcessException) p->exceptionValue));
            p->state = SHAREMIND_VM_PROCESS_CRASHED;
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            SharemindProcess_setError(
                        p,
                        SHAREMIND_VM_RUNTIME_EXCEPTION,
                        SharemindProcess_exceptionString(p->exceptionValue));
            return SHAREMIND_VM_RUNTIME_EXCEPTION;

        halt:
            p->state = SHAREMIND_VM_PROCESS_FINISHED;
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            return SHAREMIND_VM_OK;

        trap:
            p->state = SHAREMIND_VM_PROCESS_TRAPPED;
#ifndef SHAREMIND_FAST_BUILD
            SHAREMIND_UPDATESTATE;
#endif
            SHAREMIND_DEBUG_PRINTSTATE;
            SharemindProcess_setError(
                        p,
                        SHAREMIND_VM_RUNTIME_TRAP,
                        "The process returned due to a runtime trap!");
            return SHAREMIND_VM_RUNTIME_TRAP;
    } else {
        abort();
    }
}