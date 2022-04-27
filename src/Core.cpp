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

#include "Core.h"

#include <cstddef>
#ifdef SHAREMIND_DEBUG
#include <cstdio>
#endif
#include <cstring>
#include <limits.h>
#include <new>
#include <sharemind/AssertReturn.h>
#include <sharemind/codeblock.h>
#include <sharemind/libsoftfloat/softfloat.h>
#include <sharemind/likely.h>
#include <sharemind/null.h>
#include <sharemind/PotentiallyVoidTypeInfo.h>
#include <sharemind/restrict.h>
#include "CommonInstructionMacros.h"
#include "MemorySlot.h"
#include "PreparationBlock.h"
#include "Process_p.h"
#include "Program.h"


namespace sharemind {
namespace Detail {
namespace {

#define SHAREMIND_RE(e) Process::e ## Exception
#define SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_READ SHAREMIND_RE(OutOfBoundsRead)
#define SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_WRITE SHAREMIND_RE(OutOfBoundsWrite)
#define SHAREMIND_VM_PROCESS_WRITE_DENIED SHAREMIND_RE(WriteDenied)
#define SHAREMIND_VM_PROCESS_INVALID_ARGUMENT SHAREMIND_RE(InvalidArgument)
#define SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_REFERENCE_SIZE \
    SHAREMIND_RE(OutOfBoundsReferenceSize)
#define SHAREMIND_VM_PROCESS_OUT_OF_BOUNDS_REFERENCE_INDEX \
    SHAREMIND_RE(OutOfBoundsReferenceOffset)
#define SHAREMIND_VM_PROCESS_INTEGER_DIVIDE_BY_ZERO \
    SHAREMIND_RE(IntegerDivideByZero)
#define SHAREMIND_VM_PROCESS_INTEGER_OVERFLOW SHAREMIND_RE(IntegerOverflow)

#define SHAREMIND_T_CodeBlock SharemindCodeBlock
#define SHAREMIND_T_CReference Vm::ConstReference
#define SHAREMIND_T_Reference Vm::Reference

#define SHAREMIND_MI_FPU_STATE (p->m_fpuState)
#define SHAREMIND_MI_FPU_STATE_SET(v) \
    do { p->m_fpuState = static_cast<sf_fpu_state>(v); } while(0)

#define SHAREMIND_SF_E(...) p->runStatefulSoftfloatOperation(__VA_ARGS__)
#define SHAREMIND_SF_E_CAST(destType,...) \
    static_cast<destType>(SHAREMIND_SF_E(__VA_ARGS__))
#define SHAREMIND_SF_FPUF(...) \
    SHAREMIND_SF_E_CAST(uint64_t,__VA_ARGS__)
template <typename T>
void SHAREMIND_MI_UNEG_FLOAT32(T & d) { d = sf_float32_neg(d); }
template <typename T>
void SHAREMIND_MI_UNEG_FLOAT64(T & d) { d = sf_float64_neg(d); }
#define SHAREMIND_MI_UINC_FLOAT32(d) \
    (d) = SHAREMIND_SF_E(sf_float32_add, (d), sf_float32_one)
#define SHAREMIND_MI_UINC_FLOAT64(d) \
    (d) = SHAREMIND_SF_E(sf_float64_add, (d), sf_float64_one)
#define SHAREMIND_MI_UDEC_FLOAT32(d) \
    (d) = SHAREMIND_SF_E(sf_float32_sub, (d), sf_float32_one)
#define SHAREMIND_MI_UDEC_FLOAT64(d) \
    (d) = SHAREMIND_SF_E(sf_float64_sub, (d), sf_float64_one)
template <typename T, typename T2>
void SHAREMIND_MI_BNEG_FLOAT32(T & d, T2 const & s) { d = sf_float32_neg(s); }
template <typename T, typename T2>
void SHAREMIND_MI_BNEG_FLOAT64(T & d, T2 const & s) { d = sf_float64_neg(s); }
#define SHAREMIND_MI_BINC_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_add, (s), sf_float32_one)
#define SHAREMIND_MI_BINC_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_add, (s), sf_float64_one)
#define SHAREMIND_MI_BDEC_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_sub, (s), sf_float32_one)
#define SHAREMIND_MI_BDEC_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_sub, (s), sf_float64_one)
#define SHAREMIND_MI_BADD_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_add, (d), (s))
#define SHAREMIND_MI_BADD_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_add, (d), (s))
#define SHAREMIND_MI_BSUB_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_sub, (d), (s))
#define SHAREMIND_MI_BSUB_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_sub, (d), (s))
#define SHAREMIND_MI_BSUB2_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_sub, (s), (d))
#define SHAREMIND_MI_BSUB2_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_sub, (s), (d))
#define SHAREMIND_MI_BMUL_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_mul, (d), (s))
#define SHAREMIND_MI_BMUL_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_mul, (d), (s))
#define SHAREMIND_MI_BDIV_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_div, (d), (s))
#define SHAREMIND_MI_BDIV_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_div, (d), (s))
#define SHAREMIND_MI_BDIV2_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_div, (s), (d))
#define SHAREMIND_MI_BDIV2_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_div, (s), (d))
#define SHAREMIND_MI_BMOD_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_rem, (d), (s))
#define SHAREMIND_MI_BMOD_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_rem, (d), (s))
#define SHAREMIND_MI_BMOD2_FLOAT32(d,s) \
    (d) = SHAREMIND_SF_E(sf_float32_rem, (s), (d))
#define SHAREMIND_MI_BMOD2_FLOAT64(d,s) \
    (d) = SHAREMIND_SF_E(sf_float64_rem, (s), (d))
#define SHAREMIND_MI_TADD_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float32_add, (s1), (s2))
#define SHAREMIND_MI_TADD_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float64_add, (s1), (s2))
#define SHAREMIND_MI_TSUB_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float32_sub, (s1), (s2))
#define SHAREMIND_MI_TSUB_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float64_sub, (s1), (s2))
#define SHAREMIND_MI_TMUL_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float32_mul, (s1), (s2))
#define SHAREMIND_MI_TMUL_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float64_mul, (s1), (s2))
#define SHAREMIND_MI_TDIV_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float32_div, (s1), (s2))
#define SHAREMIND_MI_TDIV_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float64_div, (s1), (s2))
#define SHAREMIND_MI_TMOD_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float32_rem, (s1), (s2))
#define SHAREMIND_MI_TMOD_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_E(sf_float64_rem, (s1), (s2))
#define SHAREMIND_MI_TEQ_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_eq, (s1), (s2))
#define SHAREMIND_MI_TEQ_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_eq, (s1), (s2))
#define SHAREMIND_MI_TNE_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_ne, (s1), (s2))
#define SHAREMIND_MI_TNE_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_ne, (s1), (s2))
#define SHAREMIND_MI_TLT_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_lt, (s1), (s2))
#define SHAREMIND_MI_TLT_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_lt, (s1), (s2))
#define SHAREMIND_MI_TLE_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_le, (s1), (s2))
#define SHAREMIND_MI_TLE_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_le, (s1), (s2))
#define SHAREMIND_MI_TGT_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_gt, (s1), (s2))
#define SHAREMIND_MI_TGT_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_gt, (s1), (s2))
#define SHAREMIND_MI_TGE_FLOAT32(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float32_ge, (s1), (s2))
#define SHAREMIND_MI_TGE_FLOAT64(d,s1,s2) \
    (d) = SHAREMIND_SF_FPUF(sf_float64_ge, (s1), (s2))

#define SHAREMIND_MI_CONVERT_float32_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_float32_to_float64, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_int8(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int8_t, sf_float32_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_int16(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int16_t, sf_float32_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_int32(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int32_t, sf_float32_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_int64(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int64_t, sf_float32_to_int64, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_uint8(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint8_t, sf_float32_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_uint16(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint16_t, sf_float32_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_uint32(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint32_t, sf_float32_to_int64, (v))
#define SHAREMIND_MI_CONVERT_float32_TO_uint64(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint64_t, sf_float32_to_uint64, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_float64_to_float32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_int8(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int8_t, sf_float64_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_int16(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int16_t, sf_float64_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_int32(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int32_t, sf_float64_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_int64(d,v) \
    (d) = SHAREMIND_SF_E_CAST(int64_t, sf_float64_to_int64, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_uint8(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint8_t, sf_float64_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_uint16(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint16_t, sf_float64_to_int32, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_uint32(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint32_t, sf_float64_to_int64, (v))
#define SHAREMIND_MI_CONVERT_float64_TO_uint64(d,v) \
    (d) = SHAREMIND_SF_E_CAST(uint64_t, sf_float64_to_uint64, (v))

#define SHAREMIND_MI_CONVERT_int8_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float32, (v))
#define SHAREMIND_MI_CONVERT_int8_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float64_fpu, (v))
#define SHAREMIND_MI_CONVERT_int16_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float32, (v))
#define SHAREMIND_MI_CONVERT_int16_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float64_fpu, (v))
#define SHAREMIND_MI_CONVERT_int32_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float32, (v))
#define SHAREMIND_MI_CONVERT_int32_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float64_fpu, (v))
#define SHAREMIND_MI_CONVERT_int64_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int64_to_float32, (v))
#define SHAREMIND_MI_CONVERT_int64_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int64_to_float64, (v))
#define SHAREMIND_MI_CONVERT_uint8_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float32, (v))
#define SHAREMIND_MI_CONVERT_uint8_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float64_fpu, (v))
#define SHAREMIND_MI_CONVERT_uint16_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float32, (v))
#define SHAREMIND_MI_CONVERT_uint16_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int32_to_float64_fpu, (v))
#define SHAREMIND_MI_CONVERT_uint32_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_int64_to_float32, (v))
#define SHAREMIND_MI_CONVERT_uint32_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_int64_to_float64, (v))
#define SHAREMIND_MI_CONVERT_uint64_TO_float32(d,v) \
    (d) = SHAREMIND_SF_E(sf_uint64_to_float32, (v))
#define SHAREMIND_MI_CONVERT_uint64_TO_float64(d,v) \
    (d) = SHAREMIND_SF_E(sf_uint64_to_float64, (v))

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DO_HALT   do { return; } while ((0))
#define SHAREMIND_DO_TRAP   do { throw Process::TrapException(); } while ((0))
#else
enum class HaltCode { Continue, Halt };
#define SHAREMIND_DO_HALT   do { return HaltCode::Halt;   } while ((0))
#define SHAREMIND_DO_TRAP   do { throw Process::TrapException(); } while ((0))
#endif

#define SHAREMIND_UPDATESTATE \
    do { \
        p->m_currentIp = static_cast<std::size_t>(ip - codeStart); \
    } while ((0))

/* Micro-instructions (SHAREMIND_MI_*:) */

#define SHAREMIND_MI_NOP (void) 0
#define SHAREMIND_MI_HALT(r) \
    do { \
        p->m_returnValue = (r); \
        SHAREMIND_DO_HALT; \
    } while ((0))
#define SHAREMIND_MI_TRAP SHAREMIND_DO_TRAP

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_DISPATCH(ip) do { goto *((ip)->p[0]); } while(0)
#define SHAREMIND_MI_DISPATCH(ip) \
    do { SHAREMIND_DISPATCH(ip); } while ((0))
#else
#define SHAREMIND_DISPATCH(ip) \
    do { \
        (void) (ip); \
        SHAREMIND_UPDATESTATE; \
        return HaltCode::Continue; \
    } while ((0))
#define SHAREMIND_MI_DISPATCH(ip) \
    do { \
        (void) (ip); \
        SHAREMIND_UPDATESTATE; \
        return HaltCode::Continue; \
    } while ((0))
#endif

#define SHAREMIND_TRAP_CHECK \
    (p->m_trapCond.exchange(false, std::memory_order_acquire))

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
    (p->currentCodeSection().isInstructionAtOffset(static_cast<std::size_t>(i)))

#define SHAREMIND_MI_CHECK_JUMP_REL(reladdr) \
    do { \
        std::size_t const unsignedIp = \
                static_cast<std::size_t>(ip - codeStart); \
        if ((reladdr) < 0) { \
            if (unlikely(static_cast<std::uint64_t>(-((reladdr) + 1)) \
                         >= unsignedIp)) \
                throw Process::JumpToInvalidAddressException(); \
        } else { \
            if (unlikely( \
                    static_cast<std::uint64_t>(reladdr) \
                    >= p->currentCodeSection().size() - unsignedIp - 1u)) \
                throw Process::JumpToInvalidAddressException(); \
        } \
        SharemindCodeBlock const * const tip = ip + (reladdr); \
        if (unlikely(!SHAREMIND_MI_IS_INSTR(tip - codeStart))) \
            throw Process::JumpToInvalidAddressException(); \
        ip = tip; \
        SHAREMIND_CHECK_TRAP_AND_DISPATCH(ip); \
    } while ((0))

#define SHAREMIND_MI_DO_USER_EXCEPT(e) \
    do { \
        p->m_userException.setErrorCode((e)); \
        throw Process::UserDefinedException(std::move(p->m_userException)); \
    } while ((0))

#define SHAREMIND_MI_DO_USER_EXCEPT_WITH_MESSAGE \
    do { \
        if ((!SHAREMIND_MI_HAS_STACK) || p->m_nextFrame->crefstack.empty()) \
            throw Process::InvalidArgumentException(); \
        auto const & cref = p->m_nextFrame->crefstack.front(); \
        p->m_userException.setUserErrorMessage(cref.data.get(), cref.size); \
        throw Process::UserDefinedException(std::move(p->m_userException)); \
    } while ((0))

#define SHAREMIND_MI_DO_EXCEPT(e) \
    do { \
        throw e(); \
    } while ((0))

#define SHAREMIND_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SHAREMIND_MI_DO_EXCEPT(e); \
    } else (void) 0

#define SHAREMIND_MI_TRY_MEMRANGE(size,offset,numBytes,exception) \
    SHAREMIND_MI_TRY_EXCEPT(((offset) < (size)) \
                       && ((numBytes) <= (size)) \
                       && ((size) - (offset) >= (numBytes)), \
                       exception)

#define SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME \
    if (!SHAREMIND_MI_HAS_STACK) { \
        p->m_frames.emplace_back(); \
        p->m_nextFrame = &p->m_frames.back(); \
    } else (void)0

#define SHAREMIND_MI_PUSH(v) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->m_nextFrame->stack.emplace_back(v); \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_BLOCK_(whichStack,maybeConst,b,bOffset,rSize) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->m_nextFrame->whichStack.emplace_back( \
            std::shared_ptr<void maybeConst>(std::shared_ptr<void>(), \
                                             &(b)->uint8[0] + (bOffset)), \
            (rSize)); \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_BLOCK_ref(b) \
    SHAREMIND_MI_PUSHREF_BLOCK_(refstack,, \
                                (b), \
                                0u, \
                                sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREF_BLOCK_cref(b) \
    SHAREMIND_MI_PUSHREF_BLOCK_(crefstack, \
                                const, \
                                (b), \
                                0u, \
                                sizeof(SharemindCodeBlock))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_ref(b,o,s) \
    SHAREMIND_MI_PUSHREF_BLOCK_(refstack,,        (b), (o), (s))
#define SHAREMIND_MI_PUSHREFPART_BLOCK_cref(b,o,s) \
    SHAREMIND_MI_PUSHREF_BLOCK_(crefstack, const, (b), (o), (s))

#define SHAREMIND_MI_PUSHREF_REF_(whichStack,...) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        p->m_nextFrame->whichStack.emplace_back(__VA_ARGS__); \
    } while ((0))

#define SHAREMIND_MI_PUSHREF_REF_ref(r) \
    SHAREMIND_MI_PUSHREF_REF_(refstack, (r))
#define SHAREMIND_MI_PUSHREF_REF_cref(r) \
    SHAREMIND_MI_PUSHREF_REF_(crefstack, (r))
#define SHAREMIND_MI_PUSHREFPART_REF_ref(r,o,s) \
    SHAREMIND_MI_PUSHREF_REF_( \
        refstack, \
        Vm::Reference{std::shared_ptr<void>((r)->data, \
                                            ptrAdd((r)->data.get(), (o))), \
                      (s)})
#define SHAREMIND_MI_PUSHREFPART_REF_cref(r,o,s) \
    SHAREMIND_MI_PUSHREF_REF_( \
        crefstack, \
        Vm::ConstReference{std::shared_ptr<void const>((r)->data, \
                                                       ptrAdd((r)->data.get(), \
                                                              (o))),\
                           (s)})

std::uint8_t emptyReferenceTarget = 0u;
std::uint8_t const emptyCReferenceTarget = 0u;

#define SHAREMIND_MI_PUSHREF_MEM_FULL_(slot, whichStack, type, maybeConst) \
    SHAREMIND_MI_PUSHREF_REF_( \
        whichStack, \
        Vm::type{std::shared_ptr<void maybeConst>((slot), (slot)->data()), \
                 (slot)->size()})
#define SHAREMIND_MI_PUSHREF_MEM_PART_(slot,whichStack,type,maybeConst,o,s) \
    SHAREMIND_MI_PUSHREF_REF_( \
        whichStack, \
        Vm::type{std::shared_ptr<void maybeConst>((slot), \
                                                  ptrAdd((slot)->data(), (o))),\
                 (s)})

#define SHAREMIND_MI_PUSHREF_MEM_ref(slot) \
    SHAREMIND_MI_PUSHREF_MEM_FULL_((slot), refstack, Reference,)
#define SHAREMIND_MI_PUSHREF_MEM_cref(slot) \
    SHAREMIND_MI_PUSHREF_MEM_FULL_((slot), crefstack, ConstReference, const)
#define SHAREMIND_MI_PUSHREFPART_MEM_ref(slot,o,s) \
    SHAREMIND_MI_PUSHREF_MEM_PART_((slot), refstack, Reference,,(o),(s))
#define SHAREMIND_MI_PUSHREFPART_MEM_cref(slot,o,s) \
    SHAREMIND_MI_PUSHREF_MEM_PART_((slot), crefstack, ConstReference, const, \
                                   (o),(s))

#define SHAREMIND_MI_RESIZE_STACK(size) \
    do { \
        thisStack->resize(size, SharemindCodeBlock{{0u}}); \
    } while (false)

#define SHAREMIND_MI_CLEAR_STACK \
    do { \
        p->m_nextFrame->stack.clear(); \
        p->m_nextFrame->refstack.clear(); \
        p->m_nextFrame->crefstack.clear(); \
    } while ((0))

#define SHAREMIND_MI_HAS_STACK (!!(p->m_nextFrame))

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
        p->m_nextFrame->returnValueAddr = (r); \
        p->m_nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->m_thisFrame = p->m_nextFrame; \
        p->m_nextFrame = nullptr; \
        ip = codeStart + static_cast<std::size_t>((a)); \
        SHAREMIND_CALL_RETURN_DISPATCH(ip); \
    } while ((0))

#define SHAREMIND_MI_CHECK_CALL(a,r,nargs) \
    do { \
        if (unlikely(!SHAREMIND_MI_IS_INSTR((a)->uint64[0]))) \
            throw Process::JumpToInvalidAddressException(); \
        SHAREMIND_MI_CALL((a)->uint64[0],r,nargs); \
    } while ((0))

#define SHAREMIND_MI_SYSCALL_(scPtr,r) \
    do { \
        SHAREMIND_MI_CHECK_CREATE_NEXT_FRAME; \
        auto & nextFrame = *p->m_nextFrame; \
        try { \
            (*scPtr)(nextFrame.stack, \
                     nextFrame.refstack, \
                     nextFrame.crefstack, \
                     (r), \
                     p->m_syscallContext); \
        } catch (...) { \
            p->m_frames.pop_back(); \
            p->m_nextFrame = nullptr; \
            p->m_syscallException = std::current_exception(); \
            std::throw_with_nested(Process::SystemCallErrorException()); \
        } \
        p->m_frames.pop_back(); \
        p->m_nextFrame = nullptr; \
        SHAREMIND_CHECK_TRAP; \
    } while ((0))

#define SHAREMIND_MI_SYSCALL(a,r) \
    SHAREMIND_MI_SYSCALL_(static_cast<Vm::SyscallWrapper const *>((a)->cp[0u]),\
                          (r))

#define SHAREMIND_MI_CHECK_SYSCALL(a,r) \
    do { \
        auto const & bindings = p->m_preparedLinkingUnit->syscallBindings; \
        if (unlikely((a)->uint64[0] >= bindings.size())) \
            throw Process::InvalidSyscallIndexException(); \
        SHAREMIND_MI_SYSCALL_( \
                bindings[static_cast<std::size_t>((a)->uint64[0])], \
                (r)); \
    } while ((0))

#define SHAREMIND_MI_RETURN(r) \
    do { \
        if (unlikely(p->m_nextFrame)) { \
            p->m_frames.pop_back(); \
            p->m_nextFrame = nullptr; \
        } \
        if (likely(p->m_thisFrame->returnAddr)) { \
            if (p->m_thisFrame->returnValueAddr) \
                *p->m_thisFrame->returnValueAddr = (r); \
            ip = p->m_thisFrame->returnAddr; \
            p->m_frames.pop_back(); \
            p->m_thisFrame = &p->m_frames.back(); \
            SHAREMIND_CALL_RETURN_DISPATCH(ip); \
        } else { \
            SHAREMIND_MI_HALT((r)); \
        } \
    } while ((0))

#define SHAREMIND_MI_GET_(d,i,source,exception) \
    do { \
        if ((i) >= (source)->size()) \
            throw exception(); \
        (d) = &(*(source))[(i)]; \
    } while ((0))

#define SHAREMIND_MI_GET_stack(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      thisStack, \
                      Process::InvalidStackIndexException)
#define SHAREMIND_MI_GET_reg(d,i) \
    SHAREMIND_MI_GET_((d), \
                      (i), \
                      globalStack, \
                      Process::InvalidRegisterIndexException)
#define SHAREMIND_MI_GET_CONST_stack(...) SHAREMIND_MI_GET_stack(__VA_ARGS__)
#define SHAREMIND_MI_GET_CONST_reg(...) SHAREMIND_MI_GET_reg(__VA_ARGS__)

#define SHAREMIND_MI_GET_REF_(r,i,type,exception) \
    do { \
        auto const & container = *this ## type ## Stack; \
        if (((i) < container.size())) { \
            (r) = &container[(i)]; \
        } else { \
            throw exception(); \
        } \
    } while ((0))

#define SHAREMIND_MI_GET_ref(r,i) \
    SHAREMIND_MI_GET_REF_((r), \
                          (i), \
                          Ref, \
                          Process::InvalidReferenceIndexException)
#define SHAREMIND_MI_GET_cref(r,i) \
    SHAREMIND_MI_GET_REF_((r), \
                          (i), \
                          CRef, \
                          Process::InvalidConstReferenceIndexException)

#define SHAREMIND_MI_REFERENCE_GET_PTR(r) ((r)->data.get())
#define SHAREMIND_MI_REFERENCE_GET_CONST_PTR(r) ((r)->data.get())
#define SHAREMIND_MI_REFERENCE_GET_SIZE(r) ((r)->size)

#define SHAREMIND_MI_BLOCK_AS(b,t) (b->t[0])
#define SHAREMIND_MI_BLOCK_AS_P(b,t) (&b->t[0])
#define SHAREMIND_MI_ARG_P(n)      (ip + (n))
#define SHAREMIND_MI_ARG_AS(n,t) \
    (SHAREMIND_MI_BLOCK_AS(SHAREMIND_MI_ARG_P(n),t))

#define SHAREMIND_MI_MEM_GET_SIZE_FROM_SLOT(slot) ((slot)->size())
#define SHAREMIND_MI_MEM_GET_DATA_FROM_SLOT(slot) ((slot)->data())
#define SHAREMIND_MI_MEM_CAN_WRITE(slot) ((slot)->isWritable())

#define SHAREMIND_MI_MEM_ALLOC(dptr,sizereg) \
    do { \
        (dptr)->uint64[0] = p->publicAlloc((sizereg)->uint64[0]); \
    } while ((0))

inline std::shared_ptr<MemorySlot const> getMemorySlotOrExcept(
        ProcessState & p,
        MemoryMap::KeyType index)
{
    if (!index)
        throw Process::InvalidMemoryHandleException();
    auto const & v = p.m_memoryMap.get(index);
    if (unlikely(!v))
        throw Process::InvalidMemoryHandleException();
    return v;
}

#define SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    auto dest(getMemorySlotOrExcept(*p, (index)))

inline void publicFree(ProcessState & p, SharemindCodeBlock const & ptr) {
    switch (p.publicFree(ptr.uint64[0])) {
    case MemoryMap::Ok:
        break;
    case MemoryMap::MemorySlotInUse:
        throw Process::MemoryInUseException();
    case MemoryMap::InvalidMemoryHandle:
        throw Process::InvalidMemoryHandleException();
    }
}

#define SHAREMIND_MI_MEM_FREE(ptr) do { publicFree(*p, *(ptr)); } while(false)

#define SHAREMIND_MI_MEM_GET_SIZE(ptr,sizedest) \
    do { \
        SHAREMIND_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        (sizedest)->uint64[0] = slot->size(); \
    } while ((0))

#ifndef SHAREMIND_FAST_BUILD
#define SHAREMIND_IMPL(name,...) \
    label_impl_ ## name : __VA_ARGS__
#else
#define SHAREMIND_IMPL_INNER(name,...) \
    inline HaltCode name(ProcessState * const p) { \
        auto const codeStart = p->currentCodeSection().constData();\
        SharemindCodeBlock const * ip = &codeStart[p->m_currentIp]; \
        auto * const globalStack = &p->m_globalFrame->stack; \
        auto * thisStack = &p->m_thisFrame->stack; \
        auto * thisRefStack = &p->m_thisFrame->refstack; \
        auto * thisCRefStack = &p->m_thisFrame->crefstack; \
        (void) codeStart; (void) ip; (void) globalStack; \
        (void) thisStack; (void) thisRefStack; (void) thisCRefStack; \
        __VA_ARGS__ \
    }
#define SHAREMIND_IMPL(name,code) SHAREMIND_IMPL_INNER(func_impl_ ## name, code)

#include <sharemind/m4/dispatches.h>

SHAREMIND_IMPL_INNER(_func_impl_eof,
                     throw Process::JumpToInvalidAddressException();)
#endif

} // anonymous namespace


void vmRun(CoreMethod const sharemind_vm_run_command,
           void * const sharemind_vm_run_data)
{
    assert(sharemind_vm_run_data);
    #ifdef SHAREMIND_FAST_BUILD
    using ImplLabelType = HaltCode (*)(ProcessState * const p);
    #endif
    if (sharemind_vm_run_command == CoreMethod::GetInstruction) {
#ifndef SHAREMIND_FAST_BUILD
        using ImplLabelType = void *;
#define SHAREMIND_CBPTR p
        using CbPtrType = void *;
#define SHAREMIND_IMPL_LABEL(name) && label_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static ImplLabelType const eofLabel = && eof;
#else
#define SHAREMIND_CBPTR fp
        using CbPtrType = void (*)(void);
#define SHAREMIND_IMPL_LABEL(name) & func_impl_ ## name ,
#include <sharemind/m4/static_label_structs.h>
        static ImplLabelType const eofLabel = &_func_impl_eof;
#endif

        auto * pb = static_cast<PreparationBlock *>(sharemind_vm_run_data);
        switch (pb->labelType) {
            case PreparationBlock::InstructionLabel:
                pb->block->SHAREMIND_CBPTR[0] =
                        reinterpret_cast<CbPtrType>(
                            instr_labels[pb->block->uint64[0]]);
                break;
            case PreparationBlock::EofLabel:
                pb->block->SHAREMIND_CBPTR[0] =
                        reinterpret_cast<CbPtrType>(eofLabel);
                break;
        }
        return;

    } else {
        assert(sharemind_vm_run_command == CoreMethod::Execute);
        assert(sharemind_vm_run_data);
        auto * const p =
                static_cast<ProcessState *>(sharemind_vm_run_data);

#ifndef __GNUC__
#pragma STDC FENV_ACCESS ON
#endif
        auto const codeStart = p->currentCodeSection().constData();

#ifndef SHAREMIND_FAST_BUILD
        SharemindCodeBlock const * ip = &codeStart[p->currentIp];
        auto * const globalStack = &p->globalFrame->stack;
        auto * thisStack = &p->thisFrame->stack;
        auto * thisRefStack = &p->thisFrame->refstack;
        auto * thisCRefStack = &p->thisFrame->crefstack;
#endif

        if (SHAREMIND_TRAP_CHECK)
            throw Process::TrapException();

#ifndef SHAREMIND_FAST_BUILD
        try {
            SHAREMIND_MI_DISPATCH(ip);

            #include <sharemind/m4/dispatches.h>
        } catch (...) {
            SHAREMIND_UPDATESTATE;
            throw;
        }
#else
        HaltCode r;
        do {
            r = (*(reinterpret_cast<ImplLabelType>(
                       codeStart[p->m_currentIp].fp[0])))(p);
            assert(r == HaltCode::Halt || r == HaltCode::Continue);
        } while (r == HaltCode::Continue);
#endif
    }
}

} // namespace Detail {
} // namespace sharemind {
