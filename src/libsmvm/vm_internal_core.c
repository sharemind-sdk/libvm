/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#include "vm_internal_core.h"

#include <stddef.h>
#include <string.h>
#include "vm_internal_helpers.h"


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

#define SMVM_UPDATESTATE \
    if (1) { \
        p->currentIp = ip - p->codeSections.data[p->currentCodeSectionIndex].data; \
    } else (void) 0

/* Micro-instructions (SMVM_MI_*:) */

#define SMVM_MI_NOP (void) 0
#define SMVM_MI_HALT(r) \
    if (1) { \
        p->returnValue = (r); \
        goto halt; \
    } else (void) 0
#define SMVM_MI_TRAP if (1) { goto trap; } else (void) 0

#define SMVM_TODO(msg)

#define SMVM_MI_DISPATCH(ip) \
    if (1) { \
        goto *((ip)->p[0]); \
    } else (void) 0

#define SMVM_MI_JUMP_ABS(a) \
    if (1) { \
        SMVM_MI_DISPATCH(ip = codeStart + (a)); \
    } else (void) 0
#define SMVM_MI_JUMP_REL(n) \
    if (1) { \
        SMVM_MI_DISPATCH(ip += ((n)->int64[0])); \
    } else (void) 0

#define SMVM_MI_IS_INSTR(i) \
    SMVM_InstrSet_contains(&p->codeSections.data[p->currentCodeSectionIndex].instrset,(i))

#define SMVM_MI_CHECK_JUMP_REL(reladdr) \
    if (1) { \
        union SM_CodeBlock * tip = ip + (reladdr); \
        SMVM_MI_TRY_EXCEPT(SMVM_MI_IS_INSTR(tip - codeStart), SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        SMVM_MI_DISPATCH(ip = tip); \
    } else (void) 0

#define SMVM_MI_DO_EXCEPT(e) \
    if (1) { \
        p->exceptionValue = (e); \
        goto except; \
    } else (void) 0

#define SMVM_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SMVM_MI_DO_EXCEPT((e)); \
    } else (void)0

#define SMVM_MI_TRY_OOM(e) SMVM_MI_TRY_EXCEPT((e),SMVM_E_OUT_OF_MEMORY)

#define SMVM_MI_CHECK_CREATE_NEXT_FRAME \
    if (!SMVM_MI_HAS_STACK) { \
        p->nextFrame = SMVM_FrameStack_push(&p->frames); \
        SMVM_MI_TRY_OOM(p->nextFrame); \
        SMVM_StackFrame_init(p->nextFrame, p->thisFrame); \
    } else (void)0

#define SMVM_MI_PUSH(v) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        union SM_CodeBlock * reg = SMVM_RegisterVector_push(&p->nextFrame->stack); \
        SMVM_MI_TRY_OOM(reg); \
        *reg = (v); \
    } else (void) 0

#define _SMVM_MI_PUSHREF_BLOCK(something,b,bOffset,rSize) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        struct SMVM_Reference * ref = SMVM_ReferenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pMemory = NULL; \
        ref->pBlock = (b); \
        ref->offset = (bOffset); \
        ref->size = (rSize); \
    } else (void) 0

#define SMVM_MI_PUSHREF_BLOCK_ref(b)  _SMVM_MI_PUSHREF_BLOCK(refstack, (b), 0u, sizeof(union SM_CodeBlock))
#define SMVM_MI_PUSHREF_BLOCK_cref(b) _SMVM_MI_PUSHREF_BLOCK(crefstack,(b), 0u, sizeof(union SM_CodeBlock))
#define SMVM_MI_PUSHREFPART_BLOCK_ref(b,o,s)  _SMVM_MI_PUSHREF_BLOCK(refstack, (b), (o), (s))
#define SMVM_MI_PUSHREFPART_BLOCK_cref(b,o,s) _SMVM_MI_PUSHREF_BLOCK(crefstack,(b), (o), (s))

#define _SMVM_MI_PUSHREF_REF(something,r,rOffset,rSize) \
    if (1) { \
        if ((r)->pMemory && (r)->pMemory->nrefs + 1u == 0u) { \
            SMVM_MI_DO_EXCEPT(SMVM_E_OUT_OF_MEMORY); \
        } \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        struct SMVM_Reference * ref = SMVM_ReferenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pMemory = (r)->pMemory; \
        if (ref->pMemory) \
            ref->pMemory->nrefs++; \
        ref->pBlock = (r)->pBlock; \
        ref->offset = (rOffset); \
        ref->size = (rSize); \
    } else (void) 0

#define SMVM_MI_PUSHREF_REF_ref(r)  _SMVM_MI_PUSHREF_REF(refstack,  (r), (r)->offset, (r)->size)
#define SMVM_MI_PUSHREF_REF_cref(r) _SMVM_MI_PUSHREF_REF(crefstack, (r), (r)->offset, (r)->size)
#define SMVM_MI_PUSHREFPART_REF_ref(r,o,s)  _SMVM_MI_PUSHREF_REF(refstack,  (r), (o), (s))
#define SMVM_MI_PUSHREFPART_REF_cref(r,o,s) _SMVM_MI_PUSHREF_REF(crefstack, (r), (o), (s))

#define _SMVM_MI_PUSHREF_MEM(something,slot,mOffset,rSize) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        struct SMVM_Reference * ref = SMVM_ReferenceVector_push(&p->nextFrame->something); \
        SMVM_MI_TRY_EXCEPT(ref, SMVM_E_OUT_OF_MEMORY); \
        ref->pMemory = (slot); \
        (slot)->nrefs++; \
        ref->pBlock = NULL; \
        ref->offset = (mOffset); \
        ref->size = (rSize); \
    } else (void) 0

#define SMVM_MI_PUSHREF_MEM_ref(slot)  _SMVM_MI_PUSHREF_MEM(refstack,  (slot), 0u, (slot)->size)
#define SMVM_MI_PUSHREF_MEM_cref(slot) _SMVM_MI_PUSHREF_MEM(crefstack, (slot), 0u, (slot)->size)
#define SMVM_MI_PUSHREFPART_MEM_ref(slot,o,s)  _SMVM_MI_PUSHREF_MEM(refstack,  (slot), (o), (s))
#define SMVM_MI_PUSHREFPART_MEM_cref(slot,o,s) _SMVM_MI_PUSHREF_MEM(crefstack, (slot), (o), (s))

#define SMVM_MI_RESIZE_STACK(size) \
    SMVM_MI_TRY_OOM(SMVM_RegisterVector_resize(&p->thisFrame->stack, (size)))

#define SMVM_MI_CLEAR_STACK \
    if (1) { \
        SMVM_RegisterVector_resize(&p->nextFrame->stack, 0u); \
        SMVM_ReferenceVector_foreach(&p->nextFrame->refstack, &SMVM_Reference_deallocator); \
        SMVM_ReferenceVector_foreach(&p->nextFrame->crefstack, &SMVM_Reference_deallocator); \
        SMVM_ReferenceVector_resize(&p->nextFrame->refstack, 0u); \
        SMVM_ReferenceVector_resize(&p->nextFrame->crefstack, 0u); \
    } else (void) 0

#define SMVM_MI_HAS_STACK (!!(p->nextFrame))

#define SMVM_MI_CALL(a,r,nargs) \
    if (1) { \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        SMVM_MI_JUMP_ABS((a)); \
    } else (void) 0

#define SMVM_MI_CHECK_CALL(a,r,nargs) \
    if (1) { \
        SMVM_MI_TRY_EXCEPT(SMVM_MI_IS_INSTR((a)->uint64[0]), SMVM_E_JUMP_TO_INVALID_ADDRESS); \
        SMVM_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->nextFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        SMVM_MI_JUMP_ABS((a)->uint64[0]); \
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
            SMVM_MI_DISPATCH(ip); \
        } else { \
            SMVM_MI_HALT((r)); \
        } \
    } else (void) 0

#define SMVM_MI_GET_T_reg(d,t,i) \
    if (1) { \
        union SM_CodeBlock * r = SMVM_RegisterVector_get_pointer(&p->globalFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_INDEX_REGISTER); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SMVM_MI_GET_T_stack(d,t,i) \
    if (1) { \
        union SM_CodeBlock * r = SMVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_INDEX_STACK); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SMVM_MI_GET_reg(d,i) \
    if (1) { \
        (d) = SMVM_RegisterVector_get_pointer(&p->globalFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT((d),SMVM_E_INVALID_INDEX_REGISTER); \
    } else (void) 0

#define SMVM_MI_GET_stack(d,i) \
    if (1) { \
        (d) = SMVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT((d),SMVM_E_INVALID_INDEX_STACK); \
    } else (void) 0

#define SMVM_MI_GET_ref(r,i) \
    if (1) { \
        (r) = SMVM_ReferenceVector_get_pointer(&p->thisFrame->refstack, (i)); \
        SMVM_MI_TRY_EXCEPT((r),SMVM_E_INVALID_INDEX_REFERENCE); \
    } else (void) 0

#define SMVM_MI_GET_cref(r,i) \
    if (1) { \
        (r) = SMVM_ReferenceVector_get_pointer(&p->thisFrame->crefstack, (i)); \
        SMVM_MI_TRY_EXCEPT((r),SMVM_E_INVALID_INDEX_CONST_REFERENCE); \
    } else (void) 0

#define SMVM_MI_REFERENCE_GET_PTR(r) ((void *) ((r)->pMemory ? (r)->pMemory->pData : &(r)->pBlock->uint64[0]))
#define SMVM_MI_REFERENCE_GET_OFFSET(r) ((r)->offset)
#define SMVM_MI_REFERENCE_GET_SIZE(r) ((size_t) ((r)->pMemory ? (r)->pMemory->size : 8u))

#define SMVM_MI_BLOCK_AS(b,t) (b->t[0])
#define SMVM_MI_BLOCK_AS_P(b,t) (&b->t[0])
#define SMVM_MI_ARG(n)        (* SMVM_MI_ARG_P(n))
#define SMVM_MI_ARG_P(n)      (ip + (n))
#define SMVM_MI_ARG_AS(n,t)   (SMVM_MI_BLOCK_AS(SMVM_MI_ARG_P(n),t))
#define SMVM_MI_ARG_AS_P(n,t) (& SMVM_MI_BLOCK_AS(SMVM_MI_ARG_P(n), t))

#define SMVM_MI_MEM_ALLOC(dptr,sizereg) \
    if (1) { \
        if (p->memorySlotsUsed >= UINT64_MAX) { \
            (dptr)->uint64[0] = 0u; /* NULL */ \
        } else { \
            size_t dataSize = (sizereg)->uint64[0]; /* SIZE_MAX == UINT64_MAX */ \
            void * pData = malloc(dataSize); \
            if (!pData) { \
                (dptr)->uint64[0] = 0u; /* NULL */ \
            } else { \
                struct SMVM_MemorySlot * slot; \
                for (;;) { \
                    slot = SMVM_MemoryMap_get(&p->memoryMap, p->memorySlotNext); \
                    if (!slot) \
                        (dptr)->uint64[0] = p->memorySlotNext; \
                    if (++p->memorySlotNext == 0) \
                        p->memorySlotNext++; \
                    if (!slot) \
                        break; \
                } \
                slot = SMVM_MemoryMap_insert(&p->memoryMap, (dptr)->uint64[0]); \
                if (!slot) { \
                    free(pData); \
                    (dptr)->uint64[0] = 0u; /* NULL */ \
                } else { \
                    slot->nrefs = 0u; \
                    slot->size = dataSize; \
                    slot->pData = pData; \
                } \
            } \
        } \
    } else (void) 0

#define SMVM_MI_MEM_GET_SLOT_OR_EXCEPT(index,dest) \
    if (1) { \
        (dest) = SMVM_MemoryMap_get(&p->memoryMap, (index)); \
        SMVM_MI_TRY_EXCEPT((dest),SMVM_E_INVALID_MEMORY_POINTER); \
    } else (void) 0

/* The following can be optimized: */
#define SMVM_MI_MEM_FREE(ptr) \
    if (1) { \
        struct SMVM_MemorySlot * slot; \
        SMVM_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        SMVM_MI_TRY_EXCEPT(slot->nrefs == 0u, SMVM_E_MEMORY_POINTER_IN_USE); \
        free(slot->pData); \
        SMVM_MemoryMap_remove(&p->memoryMap, (ptr)->uint64[0]); \
    } else (void) 0

#define SMVM_MI_MEM_GET_SIZE(ptr,sizedest) \
    if (1) { \
        struct SMVM_MemorySlot * slot; \
        SMVM_MI_MEM_GET_SLOT_OR_EXCEPT((ptr)->uint64[0], slot); \
        (sizedest)->uint64[0] = slot->size; \
    } else (void) 0

#define SMVM_MI_MEM_GET_PTR(pSlot,dptr) \
    if (1) { \
        (dptr) = (pSlot)->pData; \
    } else (void) 0

#define SMVM_MI_MEMCPY memcpy
#define SMVM_MI_MEMMOVE memmove

int _SMVM(struct SMVM_Program * const p,
          const enum SMVM_InnerCommand c,
          void * const d)
{
    if (c == SMVM_I_GET_IMPL_LABEL) {

#include "../m4/static_label_structs.h"

        static void * system_labels[] = { && eof, && except, && halt, && trap };
        // static void *(*labels[3])[] = { &instr_labels, &empty_impl_labels, &system_labels };

        struct SMVM_Prepare_IBlock * pb = (struct SMVM_Prepare_IBlock *) d;
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

    } else if (c == SMVM_I_RUN || c == SMVM_I_CONTINUE) {

        union SM_CodeBlock * codeStart = p->codeSections.data[p->currentCodeSectionIndex].data;
        union SM_CodeBlock * ip = &codeStart[p->currentIp];

        /**
          \warning Note that the variable "ip" is not valid after one of the
                   following sigsetjmp's returns non-zero unless we declare it
                   volatile. However, declaring "ip" as volatile has a negative
                   performance impact. A remedy would be to update p->currentIp
                   before every instruction which could generate such signals.
        */
#ifdef	__USE_POSIX
    #define _SMVM_DECLARE_SIGJMP(h,e) \
            if (sigsetjmp(p->safeJmpBuf[(h)], 1)) { \
                p->exceptionValue = (e); \
                goto except; \
            } else (void) 0
#else
    #define _SMVM_DECLARE_SIGJMP(h,e) \
            if (setjmp(p->safeJmpBuf[(h)])) { \
                p->exceptionValue = (e); \
                goto except; \
            } else (void) 0
#endif
        _SMVM_DECLARE_SIGJMP(SMVM_HET_OTHER,      SMVM_E_OTHER_HARDWARE_EXCEPTION);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_INTDIV, SMVM_E_INTEGER_DIVIDE_BY_ZERO);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_INTOVF, SMVM_E_INTEGER_OVERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTDIV, SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTOVF, SMVM_E_FLOATING_POINT_OVERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTUND, SMVM_E_FLOATING_POINT_UNDERFLOW);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTRES, SMVM_E_FLOATING_POINT_INEXACT_RESULT);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTINV, SMVM_E_FLOATING_POINT_INVALID_OPERATION);
        _SMVM_DECLARE_SIGJMP(SMVM_HET_FPE_FLTSUB, SMVM_E_FLOATING_POINT_SUBSCRIPT_OUT_OF_RANGE);

        SMVM_MI_DISPATCH(ip);

#include "../m4/dispatches.h"

        eof:
            p->exceptionValue = SMVM_E_JUMP_TO_INVALID_ADDRESS;

        except:
            p->state = SMVM_CRASHED;
            SMVM_UPDATESTATE;
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_RUNTIME_EXCEPTION;

        halt:
            SMVM_UPDATESTATE;
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_OK;

        trap:
            p->state = SMVM_TRAPPED;
            SMVM_UPDATESTATE;
            SMVM_DEBUG_PRINTSTATE;
            return SMVM_RUNTIME_TRAP;
    } else {
        abort();
    }
}
