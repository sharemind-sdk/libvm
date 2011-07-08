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


#ifdef SVM_DEBUG
#define SVM_DEBUG_PRINTSTATE \
    if (1) { \
        SVM_UPDATESTATE; \
        SVM_Program_printStateBencoded(p, stderr); \
        fprintf(stderr, "\n"); \
    } else (void) 0
#define SVM_DEBUG_PRINTINSTRUCTION(t) \
    if (1) { \
        fprintf(p->debugFileHandle, "%s\n", SVM_2S(SVM_INSTR_NAME(t))); \
    } else (void) 0
#else
#define SVM_DEBUG_PRINTSTATE (void) 0
#define SVM_DEBUG_PRINTINSTRUCTION(t) (void) 0
#endif

#define SVM_UPDATESTATE \
    if (1) { \
        p->currentIp = ip - p->codeSections.data[p->currentCodeSectionIndex].data; \
    } else (void) 0

/* Micro-instructions (SVM_MI_*:) */

#define SVM_MI_NOP (void) 0
#define SVM_MI_HALT(r) \
    if (1) { \
        p->returnValue = (r); \
        goto halt; \
    } else (void) 0
#define SVM_MI_TRAP if (1) { goto trap; } else (void) 0

#define SVM_TODO(msg)

#define SVM_MI_DISPATCH(ip) \
    if (1) { \
        goto *((ip)->p[0]); \
    } else (void) 0

#define SVM_MI_JUMP_ABS(a) \
    if (1) { \
        SVM_MI_DISPATCH(ip = codeStart + (a)); \
    } else (void) 0
#define SVM_MI_JUMP_REL_FORWARD(n,s) \
    if (1) { \
        SVM_MI_DISPATCH(ip += (n->uint64[0]) + (s) + 1); \
    } else (void) 0
#define SVM_MI_JUMP_REL_BACKWARD(n) \
    if (1) { \
        SVM_MI_DISPATCH(ip += (n->int64[0]) - 1); \
    } else (void) 0

#define SVM_MI_IS_INSTR(i) \
    SVM_InstrSet_contains(&p->codeSections.data[p->currentCodeSectionIndex].instrset,i)

#define SVM_MI_CHECK_JUMP_REL(reladdr,numargs) \
    if (1) { \
        uint64_t i = (uint64_t)(ip - codeStart); \
        if (reladdr->int64[0] > 0) { \
            uint64_t b = ((uint64_t) i) + (reladdr->uint64[0]); \
            if (likely(b >= reladdr->uint64[0])) { \
                b += numargs + 1; \
                if (likely((b >= numargs + 1) && (b < codeSize))) { \
                    SVM_MI_TRY_EXCEPT(SVM_MI_IS_INSTR(i+reladdr->uint64[0]+numargs+1), \
                                      SVM_E_JUMP_TO_INVALID_ADDRESS); \
                    SVM_MI_JUMP_REL_FORWARD(reladdr,numargs); \
                } \
            } \
        } else { \
            uint64_t abs = (uint64_t)(-reladdr->int64[0]); \
            if (likely(abs < i)) { \
                abs = i - abs - 1; \
                SVM_MI_TRY_EXCEPT(SVM_MI_IS_INSTR(i+reladdr->int64[0]-1), \
                                  SVM_E_JUMP_TO_INVALID_ADDRESS); \
                SVM_MI_JUMP_REL_BACKWARD(reladdr); \
            } \
        } \
        SVM_MI_DO_EXCEPT(SVM_E_JUMP_TO_INVALID_ADDRESS); \
    } else (void) 0

#define SVM_MI_DO_EXCEPT(e) \
    if (1) { \
        p->exceptionValue = (e); \
        goto except; \
    } else (void) 0

#define SVM_MI_TRY_EXCEPT(cond,e) \
    if (unlikely(!(cond))) { \
        SVM_MI_DO_EXCEPT((e)); \
    } else (void)0

#define SVM_MI_TRY_OOM(e) SVM_MI_TRY_EXCEPT((e),SVM_E_OUT_OF_MEMORY)

#define SVM_MI_CHECK_CREATE_NEXT_FRAME \
    if (!p->nextFrame) { \
        p->nextFrame = SVM_FrameStack_push(&p->frames); \
        SVM_MI_TRY_OOM(p->nextFrame); \
        SVM_StackFrame_init(p->nextFrame, 0); \
    } else (void)0

#define SVM_MI_PUSH(v) \
    if (1) { \
        SVM_MI_CHECK_CREATE_NEXT_FRAME; \
        union SVM_IBlock * reg = SVM_RegisterVector_push(&p->nextFrame->stack); \
        SVM_MI_TRY_OOM(reg); \
        *reg = (v); \
    } else (void) 0

#define SVM_MI_RESIZE_STACK(size) \
    SVM_MI_TRY_OOM(SVM_RegisterVector_resize(&p->thisFrame->stack, (size)))

#define SVM_MI_CLEAR_STACK \
    if (likely(p->nextFrame)) { \
        SVM_RegisterVector_resize(&p->nextFrame->stack, 0u); \
    } else (void) 0

#define SVM_MI_CALL(a,r,nargs) \
    if (1) { \
        SVM_MI_CHECK_CREATE_NEXT_FRAME; \
        p->nextFrame->returnValueAddr = (r); \
        p->thisFrame->returnAddr = (ip + 1 + (nargs)); \
        p->thisFrame = p->nextFrame; \
        p->nextFrame = NULL; \
        SVM_MI_JUMP_ABS((a)); \
    } else (void) 0

#define SVM_MI_RETURN(r) \
    if (1) { \
        if (p->nextFrame) { \
            SVM_FrameStack_pop(&p->frames); \
            p->nextFrame = NULL; \
        } \
        if (p->thisFrame->returnValueAddr) \
            *p->thisFrame->returnValueAddr = (r); \
        ip = p->thisFrame->returnAddr; \
        p->thisFrame = p->thisFrame->prev; \
        SVM_FrameStack_pop(&p->frames); \
        SVM_MI_DISPATCH(ip); \
    } else (void) 0

#define SVM_MI_GET_T_reg(d,t,i) \
    if (1) { \
        union SVM_IBlock * r = SVM_RegisterVector_get_pointer(&p->globalFrame->stack, (i)); \
        SVM_MI_TRY_EXCEPT(r,SVM_E_INVALID_REGISTER_INDEX); \
        (d) = & SVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SVM_MI_GET_T_stack(d,t,i) \
    if (1) { \
        union SVM_IBlock * r = SVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SVM_MI_TRY_EXCEPT(r,SVM_E_INVALID_STACK_INDEX); \
        (d) = & SVM_MI_BLOCK_AS(r, t); \
    } else (void) 0

#define SVM_MI_GET_reg(d,i) \
    if (1) { \
        (d) = SVM_RegisterVector_get_pointer(&p->globalFrame->stack, (i)); \
        SVM_MI_TRY_EXCEPT(d,SVM_E_INVALID_REGISTER_INDEX); \
    } else (void) 0

#define SVM_MI_GET_stack(d,i) \
    if (1) { \
        (d) = SVM_RegisterVector_get_pointer(&p->thisFrame->stack, (i)); \
        SVM_MI_TRY_EXCEPT(d,SVM_E_INVALID_STACK_INDEX); \
    } else (void) 0

#define SVM_MI_BLOCK_AS(b,t) (b->t[0])
#define SVM_MI_ARG(n)        (* SVM_MI_ARG_P(n))
#define SVM_MI_ARG_P(n)      (ip + (n))
#define SVM_MI_ARG_AS(n,t)   (SVM_MI_BLOCK_AS(SVM_MI_ARG_P(n),t))
#define SVM_MI_ARG_AS_P(n,t) (& SVM_MI_BLOCK_AS(SVM_MI_ARG_P(n), t))

int64_t _SVM(struct SVM_Program * const p,
             const enum SVM_InnerCommand c,
             void * const d)
{
    int returnCode = SVM_OK;

    if (c == SVM_I_GET_IMPL_LABEL) {

#include "../m4/static_label_structs.h"

        static void * system_labels[] = { && eof, && except, && halt, && trap };
        // static void *(*labels[3])[] = { &instr_labels, &empty_impl_labels, &system_labels };

        struct SVM_Prepare_IBlock * pb = (struct SVM_Prepare_IBlock *) d;
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
        return SVM_OK;

    } else if (c == SVM_I_RUN) {
        size_t codeSize = p->codeSections.data[p->currentCodeSectionIndex].size;
        union SVM_IBlock * codeStart = p->codeSections.data[p->currentCodeSectionIndex].data;
        union SVM_IBlock * ip = &codeStart[p->currentIp];
        SVM_MI_DISPATCH(ip);

#include "../m4/dispatches.h"

        eof:
            p->exceptionValue = SVM_E_JUMP_TO_INVALID_ADDRESS;

        except:
            p->state = SVM_CRASHED;
            SVM_UPDATESTATE;
            SVM_DEBUG_PRINTSTATE;
            return SVM_RUNTIME_EXCEPTION;

        halt:
            SVM_UPDATESTATE;
            SVM_DEBUG_PRINTSTATE;
            return SVM_OK;

        trap:
            p->state = SVM_TRAPPED;
            SVM_UPDATESTATE;
            SVM_DEBUG_PRINTSTATE;
            return SVM_RUNTIME_TRAP;
    } else if (c == SVM_I_CONTINUE) {
        return returnCode;
    } else {
        abort();
    }
}
