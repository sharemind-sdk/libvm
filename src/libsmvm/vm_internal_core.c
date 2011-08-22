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
#include <stdlib.h>
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
    if (!p->nextFrame) { \
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

#define SMVM_MI_RESIZE_STACK(size) \
    SMVM_MI_TRY_OOM(SMVM_RegisterVector_resize(&p->thisFrame->stack, (size)))

#define SMVM_MI_CLEAR_STACK \
    if (likely(p->nextFrame)) { \
        SMVM_RegisterVector_resize(&p->nextFrame->stack, 0u); \
    } else (void) 0

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
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_REGISTER_INDEX); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SMVM_MI_GET_T_stack(d,t,i) \
    if (1) { \
        union SM_CodeBlock * r = SMVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT(r,SMVM_E_INVALID_STACK_INDEX); \
        (d) = & SMVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SMVM_MI_GET_reg(d,i) \
    if (1) { \
        (d) = SMVM_RegisterVector_get_pointer(&p->globalFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT(d,SMVM_E_INVALID_REGISTER_INDEX); \
    } else (void) 0

#define SMVM_MI_GET_stack(d,i) \
    if (1) { \
        (d) = SMVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SMVM_MI_TRY_EXCEPT(d,SMVM_E_INVALID_STACK_INDEX); \
    } else (void) 0

#define SMVM_MI_BLOCK_AS(b,t) (b->t[0])
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
                free(pData); \
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
                slot->nrefs = 0u; \
                slot->size = dataSize; \
                slot->pData = pData; \
            } \
        } \
    } else (void) 0

/* The following can be optimized: */
#define SMVM_MI_MEM_FREE(ptr) \
    if (1) { \
        struct SMVM_MemorySlot * slot = SMVM_MemoryMap_get(&p->memoryMap, (ptr)->uint64[0]); \
        SMVM_MI_TRY_EXCEPT(slot,SMVM_E_INVALID_MEMORY_POINTER); \
        free(slot->pData); \
        SMVM_MemoryMap_remove(&p->memoryMap, (ptr)->uint64[0]); \
    } else (void) 0

#define SMVM_MI_MEM_GETSIZE(ptr,sizedest) \
    if (1) { \
        struct SMVM_MemorySlot * slot = SMVM_MemoryMap_get(&p->memoryMap, (ptr)->uint64[0]); \
        SMVM_MI_TRY_EXCEPT(slot,SMVM_E_INVALID_MEMORY_POINTER); \
    (sizedest)->uint64[0] = slot->size; \
    } else (void) 0

int _SMVM(struct SMVM_Program * const p,
          const enum SMVM_InnerCommand c,
          void * const d)
{
    int returnCode = SMVM_OK;

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

    } else if (c == SMVM_I_RUN) {
        size_t codeSize = p->codeSections.data[p->currentCodeSectionIndex].size;
        union SM_CodeBlock * codeStart = p->codeSections.data[p->currentCodeSectionIndex].data;
        union SM_CodeBlock * ip = &codeStart[p->currentIp];
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
    } else if (c == SMVM_I_CONTINUE) {
        /* TODO */
        return returnCode;
    } else {
        abort();
    }
}
