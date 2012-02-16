/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#define SHAREMIND_INTERNAL__

#include "vm_internal_helpers.h"

#include <assert.h>
#include <limits.h>
#include <sharemind/libexecutable/libexecutable.h>
#include <sharemind/libexecutable/libexecutable_0x0.h>
#include <sharemind/libvmi/instr.h>
#include <sharemind/likely.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "vm_internal_core.h"

#ifdef SHAREMIND_DEBUG
#include <inttypes.h>
#include <sharemind/libvmi/instr.h>
#endif /* SHAREMIND_DEBUG */


/*******************************************************************************
 *  Forward declarations:
********************************************************************************/

static inline SharemindVmProcessException SharemindProgram_reinitialize_static_mem_slots(SharemindProgram * p);

static inline uint64_t SharemindProgram_public_alloc_slot(SharemindProgram * p, SharemindMemorySlot ** memorySlot) __attribute__ ((nonnull(1, 2)));

static uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c, uint64_t nBytes) __attribute__ ((nonnull(1)));

static int sharemind_public_free(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));

static size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));
static void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) __attribute__ ((nonnull(1)));

static void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));
static int sharemind_private_free(SharemindModuleApi0x1SyscallContext * c, void * ptr) __attribute__ ((nonnull(1)));
static int sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));
static int sharemind_private_release(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) __attribute__ ((nonnull(1)));

static int _sharemind_get_pd_process_handle(SharemindModuleApi0x1SyscallContext * c,
                                       uint64_t pd_index,
                                       void ** pdProcessHandle,
                                       size_t * pdkIndex,
                                       void ** moduleHandle) __attribute__ ((nonnull(1)));

/**
 * \brief Adds a code section to the program and prepares it for direct execution.
 * \pre codeSize > 0
 * \pre This function has been called less than program->numCodeSections on the program object.
 * \pre program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \post program->state == SHAREMIND_VM_PROCESS_INITIALIZED
 * \param program The program to prepare.
 * \param[in] code The program code, of which a copy is made and processed.
 * \param[in] codeSize The length of the code.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_addCodeSection(
        SharemindProgram * program,
        const SharemindCodeBlock * code,
        const size_t codeSize)
    __attribute__ ((nonnull(1, 2), warn_unused_result));

/**
 * \brief Prepares the program fully for execution.
 * \param program The program to prepare.
 * \returns an SharemindVmError.
 */
static SharemindVmError SharemindProgram_endPrepare(SharemindProgram * program) __attribute__ ((nonnull(1), warn_unused_result));


/*******************************************************************************
 *  SharemindVm
********************************************************************************/

struct SharemindVm_ {
    SharemindVirtualMachineContext * context;
};

SharemindVm * SharemindVm_new(SharemindVirtualMachineContext * context) {
    assert(context);
    SharemindVm * vm = (SharemindVm *) malloc(sizeof(SharemindVm));
    if (!vm)
        return NULL;
    vm->context = context;
    return vm;
}

void SharemindVm_free(SharemindVm * vm) {
    assert(vm);
    if (vm->context) {
        if (vm->context->destructor) {
            (*(vm->context->destructor))(vm->context);
        }
    }
    free(vm);
}

const SharemindSyscallBinding * SharemindVm_find_syscall(SharemindVm * vm, const char * signature) {
    if (!vm->context || !vm->context->find_syscall)
        return NULL;

    return (*(vm->context->find_syscall))(vm->context, signature);
}

const SharemindPdpiContext * ShremindVm_get_pd_process_instance(SharemindVm * vm, const char * pdName, SharemindProgram * process) {
    if (!vm->context || !vm->context->get_pd_process_instance_handle)
        return 0;

    return (*(vm->context->get_pd_process_instance_handle))(vm->context, pdName, process);
}


/*******************************************************************************
 *  Public enum methods
********************************************************************************/

SHAREMIND_ENUM_DEFINE_TOSTRING(SharemindVmProcessState, SHAREMIND_VM_PROCESS_STATE_ENUM);
SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmError, SHAREMIND_VM_ERROR_ENUM);
SHAREMIND_ENUM_CUSTOM_DEFINE_TOSTRING(SharemindVmProcessException, SHAREMIND_VM_PROCESS_EXCEPTION_ENUM);


/*******************************************************************************
 *  SharemindMemoryMap
********************************************************************************/

#ifdef SHAREMIND_FAST_BUILD
SHAREMIND_MEMORY_SLOT_INIT_DEFINE
SHAREMIND_MEMORY_SLOT_DESTROY_DEFINE

SHAREMIND_MAP_DEFINE(SharemindMemoryMap,uint64_t,const uint64_t,SharemindMemorySlot,(uint16_t)(key),SHAREMIND_MAP_KEY_EQUALS_uint64_t,SHAREMIND_MAP_KEY_LESS_THAN_uint64_t,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,)

SHAREMIND_MEMORY_MAP_FIND_UNUSED_PTR_DEFINE
#endif

static inline void SharemindMemoryMap_destroyer(const uint64_t * key, SharemindMemorySlot * value) {
    (void) key;
    SharemindMemorySlot_destroy(value);
}


/*******************************************************************************
 *  SharemindStackFrame
********************************************************************************/

#ifdef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DEFINE(SharemindRegisterVector,SharemindCodeBlock,malloc,free,realloc,)
#endif

void SharemindStackFrame_init(SharemindStackFrame * f, SharemindStackFrame * prev) {
    assert(f);
    SharemindRegisterVector_init(&f->stack);
    SharemindReferenceVector_init(&f->refstack);
    SharemindCReferenceVector_init(&f->crefstack);
    f->prev = prev;

#ifdef SHAREMIND_DEBUG
    f->lastCallIp = 0u;
    f->lastCallSection = 0u;
#endif
}

void SharemindStackFrame_destroy(SharemindStackFrame * f) {
    assert(f);
    SharemindRegisterVector_destroy(&f->stack);
    SharemindReferenceVector_destroy_with(&f->refstack, &SharemindReference_destroy);
    SharemindCReferenceVector_destroy_with(&f->crefstack, &SharemindCReference_destroy);
}

/*******************************************************************************
 *  SharemindCodeSection
********************************************************************************/

#ifdef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DEFINE(SharemindBreakpointVector,SharemindBreakpoint,malloc,free,realloc,)
SHAREMIND_INSTRSET_DEFINE(SharemindInstrSet,malloc,free,)
#endif

int SharemindCodeSection_init(SharemindCodeSection * s,
                          const SharemindCodeBlock * const code,
                          const size_t codeSize)
{
    assert(s);

    /* Add space for an exception pointer: */
    size_t realCodeSize = codeSize + 1;
    if (unlikely(realCodeSize < codeSize))
        return 0;

    size_t memSize = realCodeSize * sizeof(SharemindCodeBlock);
    if (unlikely(memSize / sizeof(SharemindCodeBlock) != realCodeSize))
        return 0;


    s->data = (SharemindCodeBlock *) malloc(memSize);
    if (unlikely(!s->data))
        return 0;

    s->size = codeSize;
    memcpy(s->data, code, memSize);

    SharemindInstrSet_init(&s->instrset);

    return 1;
}

void SharemindCodeSection_destroy(SharemindCodeSection * const s) {
    assert(s);
    free(s->data);
    SharemindInstrSet_destroy(&s->instrset);
}


/*******************************************************************************
 *  SharemindDataSection
********************************************************************************/

int SharemindDataSection_init(SharemindDataSection * ds, size_t size, SharemindMemorySlotSpecials * specials) {
    assert(ds);
    assert(specials);

    if (size != 0) {
        ds->pData = malloc(size);
        if (unlikely(!ds->pData))
            return 0;
    } else {
        ds->pData = NULL;
    }
    ds->size = size;
    ds->nrefs = 1u;
    ds->specials = specials;
    return 1;
}

void SharemindDataSection_destroy(SharemindDataSection * ds) {
    free(ds->pData);
}


/*******************************************************************************
 *  SharemindProgram
********************************************************************************/

#ifdef SHAREMIND_FAST_BUILD
SHAREMIND_VECTOR_DEFINE(SharemindCodeSectionsVector,SharemindCodeSection,malloc,free,realloc,)
SHAREMIND_VECTOR_DEFINE(SharemindDataSectionsVector,SharemindDataSection,malloc,free,realloc,)
SHAREMIND_STACK_DEFINE(SharemindFrameStack,SharemindStackFrame,malloc,free,)
SHAREMIND_MAP_DEFINE(SharemindPrivateMemoryMap,void*,void * const,size_t,fnv_16a_buf(key,sizeof(void *)),SHAREMIND_MAP_KEY_EQUALS_voidptr,SHAREMIND_MAP_KEY_LESS_THAN_voidptr,SHAREMIND_MAP_KEYCOPY_REGULAR,SHAREMIND_MAP_KEYFREE_REGULAR,malloc,free,inline)
#endif


static inline void SharemindPrivateMemoryMap_destroyer(void * const * key, size_t * value) {
    (void) value;
    free(*key);
}

static inline void SharemindMemoryInfoStatistics_init(SharemindMemoryInfoStatistics * mis) {
    mis->max = 0u;
}

static inline void SharemindMemoryInfo_init(SharemindMemoryInfo * mi) {
    mi->usage = 0u;
    mi->upperLimit = SIZE_MAX;
    SharemindMemoryInfoStatistics_init(&mi->stats);
}

SharemindProgram * SharemindProgram_new(SharemindVm * vm) {
    SharemindProgram * const p = (SharemindProgram *) malloc(sizeof(SharemindProgram));
    if (likely(p)) {
        p->state = SHAREMIND_VM_PROCESS_INITIALIZED;
        p->error = SHAREMIND_VM_OK;
        p->vm = vm;
        p->syscallContext.get_pd_process_instance_handle = &_sharemind_get_pd_process_handle;
        p->syscallContext.publicAlloc = &sharemind_public_alloc;
        p->syscallContext.publicFree = &sharemind_public_free;
        p->syscallContext.publicMemPtrSize = &sharemind_public_get_size;
        p->syscallContext.publicMemPtrData = &sharemind_public_get_ptr;
        p->syscallContext.allocPrivate = &sharemind_private_alloc;
        p->syscallContext.freePrivate = &sharemind_private_free;
        p->syscallContext.reservePrivate = &sharemind_private_reserve;
        p->syscallContext.releasePrivate = &sharemind_private_release;
        p->syscallContext.internal = p;
        SharemindMemoryInfo_init(&p->memPublicHeap);
        SharemindMemoryInfo_init(&p->memPrivate);
        SharemindMemoryInfo_init(&p->memReserved);
        SharemindMemoryInfo_init(&p->memTotal);
        SharemindCodeSectionsVector_init(&p->codeSections);
        SharemindDataSectionsVector_init(&p->dataSections);
        SharemindDataSectionsVector_init(&p->rodataSections);
        SharemindDataSectionsVector_init(&p->bssSections);
        SharemindSyscallBindings_init(&p->bindings);
        SharemindPdBindings_init(&p->pdBindings);
        SharemindFrameStack_init(&p->frames);
        SharemindMemoryMap_init(&p->memoryMap);
        SharemindPrivateMemoryMap_init(&p->privateMemoryMap);
        p->memorySlotNext = 1u;
        p->currentCodeSectionIndex = 0u;
        p->currentIp = 0u;
#ifdef SHAREMIND_DEBUG
        p->debugFileHandle = stderr;
#endif
    }
    return p;
}

void SharemindProgram_free(SharemindProgram * const p) {
    assert(p);
    SharemindCodeSectionsVector_destroy_with(&p->codeSections, &SharemindCodeSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->dataSections, &SharemindDataSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->rodataSections, &SharemindDataSection_destroy);
    SharemindDataSectionsVector_destroy_with(&p->bssSections, &SharemindDataSection_destroy);
    SharemindSyscallBindings_destroy(&p->bindings);
    SharemindPdBindings_destroy(&p->pdBindings);
    SharemindFrameStack_destroy_with(&p->frames, &SharemindStackFrame_destroy);

    SharemindMemoryMap_destroy_with(&p->memoryMap, &SharemindMemoryMap_destroyer);
    SharemindPrivateMemoryMap_destroy_with(&p->privateMemoryMap, &SharemindPrivateMemoryMap_destroyer);
    free(p);
}

static const size_t extraPadding[8] = { 0u, 7u, 6u, 5u, 4u, 3u, 2u, 1u };

static SharemindMemorySlotSpecials rwDataSpecials = { .ptr = 0u, .writeable = 1, .readable = 1, .free = NULL };
static SharemindMemorySlotSpecials roDataSpecials = { .ptr = 0u, .writeable = 0, .readable = 1, .free = NULL };

SharemindVmError SharemindProgram_load_from_sme(SharemindProgram * p, const void * data, size_t dataSize) {
    assert(p);
    assert(data);

    if (dataSize < sizeof(SharemindExecutableCommonHeader))
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

    const SharemindExecutableCommonHeader * ch;
    if (SharemindExecutableCommonHeader_read(data, &ch) != SHAREMIND_EXECUTABLE_READ_OK)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

    if (ch->fileFormatVersion > 0u)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE; /** \todo new error code? */


    const void * pos = ((const uint8_t *) data) + sizeof(SharemindExecutableCommonHeader);

    const SharemindExecutableHeader0x0 * h;
    if (SharemindExecutableHeader0x0_read(pos, &h) != SHAREMIND_EXECUTABLE_READ_OK)
        return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;
    pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableHeader0x0);

    for (unsigned ui = 0; ui <= h->numberOfUnitsMinusOne; ui++) {
        const SharemindExecutableUnitHeader0x0 * uh;
        if (SharemindExecutableUnitHeader0x0_read(pos, &uh) != SHAREMIND_EXECUTABLE_READ_OK)
            return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

        pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableUnitHeader0x0);
        for (unsigned si = 0; si <= uh->sectionsMinusOne; si++) {
            const SharemindExecutableSectionHeader0x0 * sh;
            if (SharemindExecutableSectionHeader0x0_read(pos, &sh) != SHAREMIND_EXECUTABLE_READ_OK)
                return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE;

            pos = ((const uint8_t *) pos) + sizeof(SharemindExecutableSectionHeader0x0);

            SHAREMIND_EXECUTABLE_SECTION_TYPE type = SharemindExecutableSectionHeader0x0_type(sh);
            assert(type != (SHAREMIND_EXECUTABLE_SECTION_TYPE) -1);

#if SIZE_MAX < UIN32_MAX
            if (unlikely(sh->length > SIZE_MAX))
                return SHAREMIND_VM_OUT_OF_MEMORY;
#endif

#define LOAD_DATASECTION_CASE(utype,ltype,copyCode,spec) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        SharemindDataSection * s = SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        if (unlikely(!SharemindDataSection_init(s, sh->length, (spec)))) { \
            SharemindDataSectionsVector_pop(&p->ltype ## Sections); \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        } \
        copyCode \
    } break;
#define COPYSECTION \
    memcpy(s->pData, pos, sh->length); \
    pos = ((const uint8_t *) pos) + sh->length + extraPadding[sh->length % 8];
#define LOAD_BINDSECTION_CASE(utype,code) \
    case SHAREMIND_EXECUTABLE_SECTION_TYPE_ ## utype: { \
        if (sh->length <= 0) \
            break; \
        const char * endPos = ((const char *) pos) + sh->length; \
        /* Check for 0-termination: */ \
        if (unlikely(*(endPos - 1))) \
            return SHAREMIND_VM_PREPARE_ERROR_INVALID_INPUT_FILE; \
        do { \
            code \
            pos = ((const char *) pos) + strlen((const char *) pos) + 1; \
        } while (pos != endPos); \
        pos = ((const char *) pos) + extraPadding[sh->length % 8]; \
    } break;

            switch (type) {
                case SHAREMIND_EXECUTABLE_SECTION_TYPE_TEXT: {
                    SharemindVmError r = SharemindProgram_addCodeSection(p, (const SharemindCodeBlock *) pos, sh->length);
                    if (r != SHAREMIND_VM_OK)
                        return r;

                    pos = ((const uint8_t *) pos) + sh->length * sizeof(SharemindCodeBlock);
                } break;
                LOAD_DATASECTION_CASE(RODATA,rodata,COPYSECTION,&roDataSpecials)
                LOAD_DATASECTION_CASE(DATA,data,COPYSECTION,&rwDataSpecials)
                LOAD_DATASECTION_CASE(BSS,bss,memset(s->pData, 0, sh->length);,&rwDataSpecials)
                LOAD_BINDSECTION_CASE(BIND,
                    SharemindSyscallBinding * binding = SharemindSyscallBindings_push(&p->bindings);
                    if (!binding)
                        return SHAREMIND_VM_OUT_OF_MEMORY;
                    const SharemindSyscallBinding * b = SharemindVm_find_syscall(p->vm, (const char *) pos);
                    if (!b) {
                        SharemindSyscallBindings_pop(&p->bindings);
                        fprintf(stderr, "No syscall with the signature: %s\n", (const char *) pos);
                        return SHAREMIND_VM_PREPARE_UNDEFINED_BIND;
                    }
                    (*binding) = *b;)
                LOAD_BINDSECTION_CASE(PDBIND,
                    const SharemindPdpiContext ** pdBinding = SharemindPdBindings_push(&p->pdBindings);
                    if (!pdBinding)
                        return SHAREMIND_VM_OUT_OF_MEMORY;

                    (*pdBinding) = ShremindVm_get_pd_process_instance(p->vm, (const char *) pos, p);
                    if (!*pdBinding) {
                        SharemindPdBindings_pop(&p->pdBindings);
                        fprintf(stderr, "No protection domain with the name: %s\n", (const char *) pos);
                        return SHAREMIND_VM_PREPARE_UNDEFINED_PDBIND;
                    })
                default:
                    /* Ignore other sections */
                    break;
            }
        }

        if (unlikely(p->codeSections.size == ui))
            return SHAREMIND_VM_PREPARE_ERROR_NO_CODE_SECTION;

#define PUSH_EMPTY_DATASECTION(ltype,spec) \
    if (p->ltype ## Sections.size == ui) { \
        SharemindDataSection * s = SharemindDataSectionsVector_push(&p->ltype ## Sections); \
        if (unlikely(!s)) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        SharemindDataSection_init(s, 0u, (spec)); \
    }

        PUSH_EMPTY_DATASECTION(rodata,&roDataSpecials)
        PUSH_EMPTY_DATASECTION(data,&rwDataSpecials)
        PUSH_EMPTY_DATASECTION(bss,&rwDataSpecials)
    }
    p->currentCodeSectionIndex = h->activeLinkingUnit;

    return SharemindProgram_endPrepare(p);
}

#ifdef SHAREMIND_DEBUG
static void printCodeSection(FILE * stream, const SharemindCodeBlock * code, size_t size, const char * linePrefix);
#endif /* SHAREMIND_DEBUG */

static SharemindVmError SharemindProgram_addCodeSection(SharemindProgram * const p,
                                              const SharemindCodeBlock * const code,
                                              const size_t codeSize)
{
    assert(p);
    assert(code);
    assert(codeSize > 0u);

    if (unlikely(p->state != SHAREMIND_VM_PROCESS_INITIALIZED))
        return SHAREMIND_VM_INVALID_INPUT_STATE;

    SharemindCodeSection * s = SharemindCodeSectionsVector_push(&p->codeSections);
    if (unlikely(!s))
        return SHAREMIND_VM_OUT_OF_MEMORY;

    if (unlikely(!SharemindCodeSection_init(s, code, codeSize))) {
        SharemindCodeSectionsVector_pop(&p->codeSections);
        return SHAREMIND_VM_OUT_OF_MEMORY;
    }

    #ifdef SHAREMIND_DEBUG
    fprintf(stderr, "Added code section %zu:\n", p->codeSections.size - 1);
    printCodeSection(stderr, code, codeSize, "    ");
    #endif /* SHAREMIND_DEBUG */

    return SHAREMIND_VM_OK;
}

#define SHAREMIND_PREPARE_NOP (void)0
#define SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER SHAREMIND_PREPARE_ERROR_OUTER(SHAREMIND_VM_PREPARE_ERROR_INVALID_INSTRUCTION)
#define SHAREMIND_PREPARE_ERROR_OUTER(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = i; \
        goto prepare_codesection_return_error; \
    } else (void) 0
#define SHAREMIND_PREPARE_ERROR(e) \
    if (1) { \
        returnCode = (e); \
        p->currentIp = *i; \
        return returnCode; \
    } else (void) 0
#define SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR_OUTER(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_CHECK_OR_ERROR(c,e) \
    if (unlikely(!(c))) { \
        SHAREMIND_PREPARE_ERROR(e); \
    } else (void) 0
#define SHAREMIND_PREPARE_END_AS(index,numargs) \
    if (1) { \
        c[SHAREMIND_PREPARE_CURRENT_I].uint64[0] = (index); \
        SharemindPreparationBlock pb = { .block = &c[SHAREMIND_PREPARE_CURRENT_I], .type = 0 }; \
        if (sharemind_vm_run(p, SHAREMIND_I_GET_IMPL_LABEL, (void *) &pb) != SHAREMIND_VM_OK) \
            abort(); \
        SHAREMIND_PREPARE_CURRENT_I += (numargs); \
    } else (void) 0
#define SHAREMIND_PREPARE_ARG_AS(v,t) (c[(*i)+(v)].t[0])
#define SHAREMIND_PREPARE_CURRENT_I (*i)
#define SHAREMIND_PREPARE_CODESIZE (s)->size
#define SHAREMIND_PREPARE_CURRENT_CODE_SECTION s

#define SHAREMIND_PREPARE_IS_INSTR(addr) SharemindInstrSet_contains(&s->instrset, (addr))
#define SHAREMIND_PREPARE_IS_EXCEPTIONCODE(c) ((c) >= 0x00 && (c) <= SHAREMIND_VM_PROCESS_EXCEPTION_COUNT && (c) != SHAREMIND_VM_PROCESS_OK && SharemindVmProcessException_toString((SharemindVmProcessException) (c)) != NULL)
#define SHAREMIND_PREPARE_SYSCALL(argNum) \
    if (1) { \
        SHAREMIND_PREPARE_CHECK_OR_ERROR(c[(*i)+(argNum)].uint64[0] < p->bindings.size, \
                                    SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS); \
        c[(*i)+(argNum)].cp[0] = &p->bindings.data[(size_t) c[(*i)+(argNum)].uint64[0]]; \
    } else (void) 0

#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,code) \
    static SharemindVmError prepare_pass2_ ## name (SharemindProgram * const p, SharemindCodeSection * s, SharemindCodeBlock * c, size_t * i) { \
        (void) p; (void) s; (void) c; (void) i; \
        SharemindVmError returnCode = SHAREMIND_VM_OK; \
        code \
        return returnCode; \
    }
#include <sharemind/m4/preprocess_pass2_functions.h>

#undef SHAREMIND_PREPARE_PASS2_FUNCTION
#define SHAREMIND_PREPARE_PASS2_FUNCTION(name,bytecode,_) \
    { .code = bytecode, .f = prepare_pass2_ ## name},
struct preprocess_pass2_function {
    uint64_t code;
    SharemindVmError (*f)(SharemindProgram * const p, SharemindCodeSection * s, SharemindCodeBlock * c, size_t * i);
};
struct preprocess_pass2_function preprocess_pass2_functions[] = {
#include <sharemind/m4/preprocess_pass2_functions.h>
    { .code = 0u, .f = NULL }
};

static SharemindVmError SharemindProgram_endPrepare(SharemindProgram * const p) {
    assert(p);

    SharemindPreparationBlock pb;

    if (unlikely(p->state != SHAREMIND_VM_PROCESS_INITIALIZED))
        return SHAREMIND_VM_INVALID_INPUT_STATE;

    assert(p->codeSections.size > 0u);

    for (size_t j = 0u; j < p->codeSections.size; j++) {
        SharemindVmError returnCode = SHAREMIND_VM_OK;
        SharemindCodeSection * s = &p->codeSections.data[j];

        SharemindCodeBlock * c = s->data;
        assert(c);

        /* Initialize instructions hashmap: */
        for (size_t i = 0u; i < s->size; i++) {
            const SharemindVmInstruction * const instr = sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }

            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(i + instr->numArgs < s->size,
                                              SHAREMIND_VM_PREPARE_ERROR_INVALID_ARGUMENTS);
            SHAREMIND_PREPARE_CHECK_OR_ERROR_OUTER(SharemindInstrSet_insert(&s->instrset, i),
                                              SHAREMIND_VM_OUT_OF_MEMORY);
            i += instr->numArgs;
        }

        for (size_t i = 0u; i < s->size; i++) {
            const SharemindVmInstruction * const instr = sharemind_vm_instruction_from_code(c[i].uint64[0]);
            if (!instr) {
                SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
            }
            struct preprocess_pass2_function * ppf = &preprocess_pass2_functions[0];
            for (;;) {
                if (!(ppf->f)) {
                    SHAREMIND_PREPARE_INVALID_INSTRUCTION_OUTER;
                }
                if (ppf->code == c[i].uint64[0]) {
                    returnCode = (*(ppf->f))(p, s, c, &i);
                    if (returnCode != SHAREMIND_VM_OK)
                        goto prepare_codesection_return_error;
                    break;
                }
                ++ppf;
            }
        }

        /* Initialize exception pointer: */
        c[s->size].uint64[0] = 0u; /* && eof */
        pb.block = &c[s->size];
        pb.type = 2;
        if (sharemind_vm_run(p, SHAREMIND_I_GET_IMPL_LABEL, &pb) != SHAREMIND_VM_OK)
            abort();

        continue;

    prepare_codesection_return_error:
        p->currentCodeSectionIndex = j;
        return returnCode;
    }

    p->globalFrame = SharemindFrameStack_push(&p->frames);
    if (unlikely(!p->globalFrame))
        return SHAREMIND_VM_OUT_OF_MEMORY;

    /* Initialize global frame: */
    SharemindStackFrame_init(p->globalFrame, NULL);
    p->globalFrame->returnAddr = NULL; /* Triggers halt on return. */
    /* p->globalFrame->returnValueAddr = &(p->returnValue); is not needed. */
    p->thisFrame = p->globalFrame;
    p->nextFrame = NULL;

    /* Initialize section pointers: */

    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 0u));
    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 1u));
    assert(!SharemindMemoryMap_get_const(&p->memoryMap, 2u));
    assert(p->memorySlotNext == 1u);

    const SharemindVmProcessException e = SharemindProgram_reinitialize_static_mem_slots(p);
    if (e != SHAREMIND_VM_OK)
        return e;

    p->memorySlotNext += 3;

    p->state = SHAREMIND_VM_PROCESS_PREPARED;

    return SHAREMIND_VM_OK;
}

SharemindVmError SharemindProgram_run(SharemindProgram * const program) {
    assert(program);

    if (unlikely(program->state != SHAREMIND_VM_PROCESS_PREPARED))
        return SHAREMIND_VM_INVALID_INPUT_STATE;

    return sharemind_vm_run(program, SHAREMIND_I_RUN, NULL);
}

int64_t SharemindProgram_get_return_value(SharemindProgram *p) {
    return p->returnValue.int64[0];
}

SharemindVmProcessException SharemindProgram_get_exception(SharemindProgram *p) {
    assert(p->exceptionValue >= INT_MIN && p->exceptionValue <= INT_MAX);
    assert(SharemindVmProcessException_toString((SharemindVmProcessException) p->exceptionValue) != 0);
    return (SharemindVmProcessException) p->exceptionValue;
}

size_t SharemindProgram_get_current_code_section(SharemindProgram *p) {
    return p->currentCodeSectionIndex;
}

uintptr_t SharemindProgram_get_current_ip(SharemindProgram *p) {
    return p->currentIp;
}

static inline SharemindVmProcessException SharemindProgram_reinitialize_static_mem_slots(SharemindProgram * p) {

#define INIT_STATIC_MEMSLOT(index,dataSection) \
    if (1) { \
        SharemindDataSection * const restrict staticSlot = SharemindDataSectionsVector_get_pointer(&p->dataSection ## Sections, p->currentCodeSectionIndex); \
        assert(staticSlot); \
        SharemindMemorySlot * const restrict slot = SharemindMemoryMap_get_or_insert(&p->memoryMap, (index)); \
        if (unlikely(!slot)) \
            return SHAREMIND_VM_OUT_OF_MEMORY; \
        (*slot) = *staticSlot; \
    } else (void) 0

    INIT_STATIC_MEMSLOT(1u,rodata);
    INIT_STATIC_MEMSLOT(2u,data);
    INIT_STATIC_MEMSLOT(3u,bss);

    return SHAREMIND_VM_OK;
}

static inline uint64_t SharemindProgram_public_alloc_slot(SharemindProgram * p, SharemindMemorySlot ** memorySlot) {
    assert(p);
    assert(memorySlot);
    assert(p->memoryMap.size < (UINT64_MAX < SIZE_MAX ? UINT64_MAX : SIZE_MAX));

    /* Find a free memory slot: */
    const uint64_t index = SharemindMemoryMap_find_unused_ptr(&p->memoryMap, p->memorySlotNext);
    assert(index != 0u);

    /* Fill the slot: */
#ifndef NDEBUG
    size_t oldSize = p->memoryMap.size;
#endif
    SharemindMemorySlot * const slot = SharemindMemoryMap_get_or_insert(&p->memoryMap, index);
    if (unlikely(!slot))
        return 0u;
    assert(oldSize < p->memoryMap.size);

    /* Optimize next alloc: */
    p->memorySlotNext = index + 1u;
    /* skip "NULL" and static memory pointers: */
    if (unlikely(!p->memorySlotNext))
        p->memorySlotNext += 4u;

    (*memorySlot) = slot;

    return index;
}

uint64_t SharemindProgram_public_alloc(SharemindProgram * p, uint64_t nBytes, SharemindMemorySlot ** memorySlot) {
    assert(p);
    assert(p->memorySlotNext != 0u);

    /* Fail if allocating too little: */
    if (unlikely(nBytes <= 0u))
        return 0u;

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPublicHeap.upperLimit - p->memPublicHeap.usage < nBytes)))
        return 0u;

    /** \todo Check any other memory limits? */

    /* Fail if all memory slots are used. */
    SHAREMIND_STATIC_ASSERT(sizeof(uint64_t) >= sizeof(size_t));
    if (unlikely(p->memoryMap.size >= SIZE_MAX))
        return 0u;

    /* Allocate the memory: */
    void * const pData = malloc((size_t) nBytes);
    if (unlikely(!pData))
        return 0u;

    SharemindMemorySlot * slot;
    const uint64_t index = SharemindProgram_public_alloc_slot(p, &slot);
    if (unlikely(index == 0u)) {
        free(pData);
        return index;
    }

    /* Initialize the slot: */
    SharemindMemorySlot_init(slot, pData, (size_t) nBytes, NULL);

    /* Update memory statistics: */
    p->memPublicHeap.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memPublicHeap.usage > p->memPublicHeap.stats.max)
        p->memPublicHeap.stats.max = p->memPublicHeap.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    if (memorySlot)
        (*memorySlot) = slot;

    return index;
}

static uint64_t sharemind_public_alloc(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) {
    assert(c);
    assert(c->internal);

    if (unlikely(nBytes > UINT64_MAX))
        return 0u;

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    return SharemindProgram_public_alloc(p, nBytes, NULL);
}

SharemindVmProcessException SharemindProgram_public_free(SharemindProgram * p, uint64_t ptr) {
    assert(p);
    assert(ptr != 0u);
    SharemindMemorySlot * slot = SharemindMemoryMap_get(&p->memoryMap, ptr);
    if (unlikely(!slot))
        return SHAREMIND_VM_PROCESS_INVALID_REFERENCE;

    if (unlikely(slot->nrefs != 0u))
        return SHAREMIND_VM_PROCESS_MEMORY_IN_USE;


    /* Update memory statistics: */
    assert(p->memPublicHeap.usage >= slot->size);
    assert(p->memTotal.usage >= slot->size);
    p->memPublicHeap.usage -= slot->size;
    p->memTotal.usage -= slot->size;

    /** \todo Update any other memory statistics? */

    /* Deallocate the memory and release the slot: */
    SharemindMemorySlot_destroy(slot);
    SharemindMemoryMap_remove(&p->memoryMap, ptr);

    return SHAREMIND_VM_PROCESS_OK;
}

static int sharemind_public_free(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) {
    assert(c);
    assert(c->internal);

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    return SharemindProgram_public_free(p, ptr) == SHAREMIND_VM_PROCESS_OK;
}

size_t sharemind_public_get_size(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) {
    assert(c);
    assert(c->internal);

    const SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    const SharemindMemorySlot * slot = SharemindMemoryMap_get_const(&p->memoryMap, ptr);
    if (!slot)
        return 0u;

    return slot->size;
}

void * sharemind_public_get_ptr(SharemindModuleApi0x1SyscallContext * c, uint64_t ptr) {
    assert(c);
    assert(c->internal);

    const SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    const SharemindMemorySlot * slot = SharemindMemoryMap_get_const(&p->memoryMap, ptr);
    if (!slot)
        return NULL;

    return slot->pData;
}

void * sharemind_private_alloc(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) {
    assert(c);
    assert(c->internal);

    if (unlikely(nBytes <= 0))
        return NULL;

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memPrivate.upperLimit - p->memPrivate.usage < nBytes)))
        return NULL;

    /** \todo Check any other memory limits? */

    /* Allocate the memory: */
    void * ptr = malloc(nBytes);
    if (unlikely(!ptr))
        return NULL;

    /* Add pointer to private memory map: */
#ifndef NDEBUG
    size_t oldSize = p->privateMemoryMap.size;
#endif
    size_t * s = SharemindPrivateMemoryMap_get_or_insert(&p->privateMemoryMap, ptr);
    if (unlikely(!s)) {
        free(ptr);
        return NULL;
    }
    assert(oldSize < p->privateMemoryMap.size);
    (*s) = nBytes;

    /* Update memory statistics: */
    p->memPrivate.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memPrivate.usage > p->memPrivate.stats.max)
        p->memPrivate.stats.max = p->memPrivate.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    return ptr;
}

int sharemind_private_free(SharemindModuleApi0x1SyscallContext * c, void * ptr) {
    assert(c);
    assert(c->internal);

    if (unlikely(!ptr))
        return false;

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    /* Check if pointer is in private memory map: */
    const size_t * s = SharemindPrivateMemoryMap_get_const(&p->privateMemoryMap, ptr);
    if (unlikely(!s))
        return false;

    /* Get allocated size: */
    const size_t nBytes = (*s);

    /* Free the memory: */
    free(ptr);

    /* Remove pointer from valid list: */
    int r = SharemindPrivateMemoryMap_remove(&p->privateMemoryMap, ptr);
    assert(r);

    /* Update memory statistics: */
    assert(p->memPrivate.usage >= nBytes);
    assert(p->memTotal.usage >= nBytes);
    p->memPrivate.usage -= nBytes;
    p->memTotal.usage -= nBytes;

    /** \todo Update any other memory statistics? */

    return true;
}

static int sharemind_private_reserve(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) {
    assert(c);
    assert(c->internal);

    if (unlikely(nBytes <= 0u))
        return true;

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    /* Check memory limits: */
    if (unlikely((p->memTotal.upperLimit - p->memTotal.usage < nBytes)
                 || (p->memReserved.upperLimit - p->memReserved.usage < nBytes)))
        return false;

    /** \todo Check any other memory limits? */

    /* Update memory statistics */
    p->memReserved.usage += nBytes;
    p->memTotal.usage += nBytes;

    if (p->memReserved.usage > p->memReserved.stats.max)
        p->memReserved.stats.max = p->memReserved.usage;

    if (p->memTotal.usage > p->memTotal.stats.max)
        p->memTotal.stats.max = p->memTotal.usage;

    /** \todo Update any other memory statistics? */

    return true;
}

static int sharemind_private_release(SharemindModuleApi0x1SyscallContext * c, size_t nBytes) {
    assert(c);
    assert(c->internal);

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    /* Check for underflow: */
    if (p->memReserved.usage < nBytes)
        return false;

    assert(p->memTotal.usage >= nBytes);
    p->memReserved.usage -= nBytes;
    p->memTotal.usage -= nBytes;
    return true;
}

int _sharemind_get_pd_process_handle(SharemindModuleApi0x1SyscallContext * c,
                                uint64_t pd_index,
                                void ** pdProcessHandle,
                                size_t * pdkIndex,
                                void ** moduleHandle)
{
    assert(c);
    assert(c->internal);

    if (pd_index > SIZE_MAX)
        return 1;

    SharemindProgram * const p = (SharemindProgram *) c->internal;
    assert(p);

    const SharemindPdpiContext * const * const ppd = SharemindPdBindings_get_const_pointer(&p->pdBindings, pd_index);
    if (!ppd && !*ppd)
        return 1;

    if (pdProcessHandle)
        *pdProcessHandle = (*ppd)->pdProcessHandle;
    if (pdkIndex)
        *pdkIndex = (*ppd)->pdkIndex;
    if (moduleHandle)
        *moduleHandle = (*ppd)->moduleHandle;

    return 0;
}


/*******************************************************************************
 *  Debugging
********************************************************************************/

#ifdef SHAREMIND_DEBUG

void SharemindRegisterVector_printStateBencoded(SharemindRegisterVector * const v, FILE * const f) {
    fprintf(f, "l");
    for (size_t i = 0u; i < v->size; i++)
        fprintf(f, "i%" PRIu64 "e", v->data[i].uint64[0]);
    fprintf(f, "e");
}

void SharemindStackFrame_printStateBencoded(SharemindStackFrame * const s, FILE * const f) {
    assert(s);
    fprintf(f, "d");
    fprintf(f, "5:Stack");
    SharemindRegisterVector_printStateBencoded(&s->stack, f);
    fprintf(f, "10:LastCallIpi%zue", s->lastCallIp);
    fprintf(f, "15:LastCallSectioni%zue", s->lastCallSection);
    fprintf(f, "e");
}

void SharemindProgram_printStateBencoded(SharemindProgram * const p, FILE * const f) {
    assert(p);
    fprintf(f, "d");
        fprintf(f, "5:Statei%de", p->state);
        fprintf(f, "5:Errori%de", p->error);
        fprintf(f, "18:CurrentCodeSectioni%zue", p->currentCodeSectionIndex);
        fprintf(f, "9:CurrentIPi%zue", p->currentIp);
        fprintf(f, "12:CurrentStackl");
        for (struct SharemindFrameStack_item * s = p->frames.d; s != NULL; s = s->prev)
            SharemindStackFrame_printStateBencoded(&s->value, f);
        fprintf(f, "e");
        fprintf(f, "9:NextStackl");
        if (p->nextFrame)
            SharemindStackFrame_printStateBencoded(p->nextFrame, f);
        fprintf(f, "e");
    fprintf(f, "e");
}

static void printCodeSection(FILE * stream, const SharemindCodeBlock * code, size_t size, const char * linePrefix) {
    size_t skip = 0u;
    for (size_t i = 0u; i < size; i++) {
        fprintf(stream, "%s %08zx ", linePrefix, i);
        const uint8_t * b = &code[i].uint8[0];
        fprintf(stream, "%02x%02x %02x%02x %02x%02x %02x%02x",
               b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

        if (!skip) {
            const SharemindVmInstruction * instr = sharemind_vm_instruction_from_code(code[i].uint64[0]);
            if (instr) {
                fprintf(stream, "  %s", sharemind_vm_instruction_fullname_to_name(instr->fullName));
                skip = instr->numArgs;
            } else {
                fprintf(stream, "  %s", "!!! UNKNOWN INSTRUCTION OR DATA !!!");
            }
        } else {
            skip--;
        }

        fprintf(stream, "\n");
    }
}

#endif /* SHAREMIND_DEBUG */