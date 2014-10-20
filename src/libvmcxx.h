/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_CXX_H
#define SHAREMIND_LIBVM_CXX_H

#include <cassert>
#include <exception>
#include <new>
#include <type_traits>
#include "libvm.h"


namespace sharemind {

/*******************************************************************************
  Some type aliases
*******************************************************************************/

using VmContext = ::SharemindVirtualMachineContext;
using VmInstruction = ::SharemindVmInstruction;


/*******************************************************************************
  VmError
*******************************************************************************/

using VmError = ::SharemindVmError;
inline const char * VmError_toString(const VmError e) noexcept
{ return ::SharemindVmError_toString(e); }


/*******************************************************************************
  Forward declarations:
*******************************************************************************/

class Vm;
class Program;
class Process;


/*******************************************************************************
  Some helper macros
*******************************************************************************/

#define SHAREMIND_LIBVM_CXX_EXCEPT_OVERRIDES_TEST(capture,error) \
    ([capture]{ \
        const VmError e = (error); \
        if (e == ::SHAREMIND_VM_OUT_OF_MEMORY) \
            throw std::bad_alloc(); \
        return e; \
    }())

#define SHAREMIND_LIBVM_CXX_DEFINE_PARENT_GETTER(ClassName,Parent,parent) \
    inline Parent * parent() const noexcept { \
        return Detail::libvm::mustTag( \
                ::Sharemind ## ClassName ## _ ## parent(m_c)); \
    }

#define SHAREMIND_LIBVM_CXX_DEFINE_CPTR_GETTERS(ClassName) \
    inline ::Sharemind ## ClassName * cPtr() noexcept { return m_c; } \
    inline const ::Sharemind ## ClassName * cPtr() const noexcept \
    { return m_c; }

#define SHAREMIND_LIBVM_CXX_DEFINE_EXCEPTION(ClassName) \
    class Exception: public VmExceptionBase { \
    public: /* Methods: */ \
        inline Exception(const ::Sharemind ## ClassName & c) \
            : VmExceptionBase( \
                      SHAREMIND_LIBVM_CXX_EXCEPT_OVERRIDES_TEST( \
                              &c, \
                              ::Sharemind ## ClassName ## _lastError(&c)), \
                      ::Sharemind ## ClassName ## _lastErrorString(&c)) \
        {} \
        inline Exception(const ClassName & c) : Exception(*(c.cPtr())) {} \
        inline Exception(const VmError error, const char * const errorStr) \
            : VmExceptionBase(error, errorStr) \
        {} \
        inline Exception(const VmError error, \
                         const ::Sharemind ## ClassName & c) \
            : VmExceptionBase(error, \
                              ::Sharemind ## ClassName ## _lastErrorString(&c))\
        {} \
        inline Exception(const VmError error, const ClassName & c) \
            : Exception(error, *(c.cPtr())) \
        {} \
    }


/*******************************************************************************
  Details
*******************************************************************************/

namespace Detail {
namespace libvm {

template <typename CType> struct TypeInv;

#define SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV(t) \
    template <> struct TypeInv<t> { using type = ::Sharemind ## t; };\
    template <> struct TypeInv<::Sharemind ## t> { using type = t; };

#define SHAREMIND_LIBVM_CXX_DEFINE_TAGGETTER(type) \
    inline type * getTag(const ::Sharemind ## type * const o) noexcept \
    { return static_cast<type *>(::Sharemind ## type ## _tag(o)); }

#define SHAREMIND_LIBVM_CXX_DEFINE_TYPE_STUFF(type) \
    SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV(type) \
    SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV(type const) \
    SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV(type volatile) \
    SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV(type const volatile) \
    SHAREMIND_LIBVM_CXX_DEFINE_TAGGETTER(type)

SHAREMIND_LIBVM_CXX_DEFINE_TYPE_STUFF(Vm)
SHAREMIND_LIBVM_CXX_DEFINE_TYPE_STUFF(Program)
SHAREMIND_LIBVM_CXX_DEFINE_TYPE_STUFF(Process)

#undef SHAREMIND_LIBVM_CXX_DEFINE_TYPE_STUFF
#undef SHAREMIND_LIBVM_CXX_DEFINE_TAGGETTER
#undef SHAREMIND_LIBVM_CXX_DEFINE_TYPEINV

template <typename CType>
inline typename TypeInv<CType>::type * mustTag(CType * const ssc) noexcept {
    assert(ssc);
    typename TypeInv<CType>::type * const sc = getTag(ssc);
    return (assert(sc), sc);
}

template <typename CType>
inline typename TypeInv<CType>::type * optChild(CType * const ssc) noexcept
{ return ssc ? mustTag(ssc) : nullptr; }

} /* namespace libvm { */
} /* namespace Detail { */


/*******************************************************************************
  VmExceptionBase
*******************************************************************************/

class VmExceptionBase: public std::exception {

public: /* Methods: */

    inline VmExceptionBase(const VmError errorCode, const char * const errorStr)
        : m_errorCode((assert(errorCode != ::SHAREMIND_VM_OK), errorCode))
        , m_errorStr(errorStr ? errorStr : VmError_toString(errorCode))
    {}

    inline VmError code() const noexcept { return m_errorCode; }
    inline const char * what() const noexcept override { return m_errorStr; }

private: /* Fields: */

    const VmError m_errorCode;
    const char * const m_errorStr;

}; /* class VmExceptionBase { */


/*******************************************************************************
  Process
*******************************************************************************/

class Process {

public: /* Types: */

    SHAREMIND_LIBVM_CXX_DEFINE_EXCEPTION(Process);

    class RuntimeException: public Exception {

    public: /* Methods: */

        inline RuntimeException(const ::SharemindProcess & process)
            : Exception(::SHAREMIND_VM_RUNTIME_EXCEPTION,
                        process)
            , m_code(::SharemindProcess_exception(&process))
        {}

        ::SharemindVmProcessException exception() const noexcept
        { return m_code; }

    private: /* Fields: */

        ::SharemindVmProcessException m_code;

    };

public: /* Methods: */

    Process() = delete;
    Process(Process &&) = delete;
    Process(const Process &) = delete;
    Process & operator=(Process &&) = delete;
    Process & operator=(const Process &) = delete;

    inline Process(Program & program);

    virtual inline ~Process() noexcept {
        if (m_c) {
            if (::SharemindProcess_tag(m_c) == this)
                ::SharemindProcess_releaseTag(m_c);
            ::SharemindProcess_free(m_c);
        }
    }

    SHAREMIND_LIBVM_CXX_DEFINE_CPTR_GETTERS(Process)
    SHAREMIND_LIBVM_CXX_DEFINE_PARENT_GETTER(Process,Program,program)
    SHAREMIND_LIBVM_CXX_DEFINE_PARENT_GETTER(Process,Vm,vm)

    size_t pdpiCount() const noexcept
    { return ::SharemindProcess_pdpiCount(m_c); }

    ::SharemindPdpi * pdpi(const size_t pdpiIndex) const noexcept
    { return ::SharemindProcess_pdpi(m_c, pdpiIndex); }

    void setPdpiFacility(const char * const name,
                         void * const facility,
                         void * const context = nullptr)
            __attribute__((nonnull(2)))
    {
        const VmError r = ::SharemindProcess_setPdpiFacility(m_c,
                                                             name,
                                                             facility,
                                                             context);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    void setInternal(void * const value) noexcept
    { ::SharemindProcess_setInternal(m_c, value); }

    void run() { run__<&::SharemindProcess_run>(); }

    void continueRun() { run__<&::SharemindProcess_continue>(); }

    void pause() {
        const VmError r = ::SharemindProcess_pause(m_c);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    int64_t returnValue() const noexcept
    { return ::SharemindProcess_returnValue(m_c); }

    size_t currentCodeSection() const noexcept
    { return ::SharemindProcess_currentCodeSection(m_c); }

    uintptr_t currentIp() const noexcept
    { return ::SharemindProcess_currentIp(m_c); }

private: /* Methods: */

    template <VmError (* runFn)(::SharemindProcess *)>
    void run__() {
        const VmError r = (*runFn)(m_c);
        if (r != ::SHAREMIND_VM_OK) {
            if (r == ::SHAREMIND_VM_RUNTIME_EXCEPTION)
                throw RuntimeException(*m_c);
            throw Exception(r, *m_c);
        }
    }

private: /* Fields: */

    ::SharemindProcess * m_c;

}; /* class Process { */


/*******************************************************************************
  Program
*******************************************************************************/

class Program {

    friend Process::Process(Program & program);

public: /* Types: */

    using Overrides = VmContext;
    SHAREMIND_LIBVM_CXX_DEFINE_EXCEPTION(Program);

public: /* Methods: */

    Program() = delete;
    Program(Program &&) = delete;
    Program(const Program &) = delete;
    Program & operator=(Program &&) = delete;
    Program & operator=(const Program &) = delete;

    inline Program(Vm & vm, Overrides * const overrides = nullptr);

    virtual inline ~Program() noexcept {
        if (m_c) {
            if (::SharemindProgram_tag(m_c) == this)
                ::SharemindProgram_releaseTag(m_c);
            ::SharemindProgram_free(m_c);
        }
    }

    SHAREMIND_LIBVM_CXX_DEFINE_CPTR_GETTERS(Program)
    SHAREMIND_LIBVM_CXX_DEFINE_PARENT_GETTER(Program,Vm,vm)

    void loadFromFile(const char * filename) {
        assert(filename);
        const VmError r = ::SharemindProgram_loadFromFile(m_c, filename);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    void loadFromCFile(FILE * file) {
        assert(file);
        const VmError r = ::SharemindProgram_loadFromCFile(m_c, file);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    void loadFromFileDescriptor(const int fd) {
        assert(fd >= 0);
        const VmError r = ::SharemindProgram_loadFromFileDescriptor(m_c, fd);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    void loadFromMemory(const void * const data, const size_t size) {
        assert(data);
        const VmError r = ::SharemindProgram_loadFromMemory(m_c, data, size);
        if (r != ::SHAREMIND_VM_OK)
            throw Exception(r, *m_c);
    }

    bool isReady() const noexcept { return ::SharemindProgram_isReady(m_c); }

    const void * lastParsePosition() const noexcept
    { return SharemindProgram_lastParsePosition(m_c); }

    const VmInstruction * instruction(const size_t codeSection,
                                      const size_t instructionIndex)
            const noexcept
    {
        return ::SharemindProgram_instruction(m_c,
                                              codeSection,
                                              instructionIndex);
    }

private: /* Methods: */

    ::SharemindProcess & newProcess() {
        ::SharemindProcess * const p = ::SharemindProgram_newProcess(m_c);
        if (p)
            return *p;
        throw Exception(*m_c);
    }

private: /* Fields: */

    ::SharemindProgram * m_c;

}; /* class Program { */


/*******************************************************************************
  Vm
*******************************************************************************/

class Vm {

    friend Program::Program(Vm & vm, Program::Overrides * const overrides);

public: /* Types: */

    using Context = VmContext;

    SHAREMIND_LIBVM_CXX_DEFINE_EXCEPTION(Vm);

public: /* Methods: */

    Vm() = delete;
    Vm(Vm &&) = delete;
    Vm(const Vm &) = delete;
    Vm & operator=(Vm &&) = delete;
    Vm & operator=(const Vm &) = delete;

    inline Vm(VmContext & context)
        : m_c([&context](){
            VmError error;
            const char * errorStr;
            ::SharemindVm * const vm =
                    ::SharemindVm_new(&context, &error, &errorStr);
            if (vm)
                return vm;
            throw Exception(
                    SHAREMIND_LIBVM_CXX_EXCEPT_OVERRIDES_TEST(=,error),
                    errorStr);
        }())
    {
        ::SharemindVm_setTagWithDestructor(
                    m_c,
                    this,
                    [](void * m) noexcept {
                        Vm * const vm = static_cast<Vm *>(m);
                        vm->m_c = nullptr;
                        delete vm;
                    });
    }

    virtual inline ~Vm() noexcept {
        if (m_c) {
            if (::SharemindVm_tag(m_c) == this)
                ::SharemindVm_releaseTag(m_c);
            ::SharemindVm_free(m_c);
        }
    }

    SHAREMIND_LIBVM_CXX_DEFINE_CPTR_GETTERS(Vm)

private: /* Methods: */

    ::SharemindProgram & newProgram(Program::Overrides * const overrides) {
        ::SharemindProgram * const p =
                ::SharemindVm_newProgram(m_c, overrides);
        if (p)
            return *p;
        throw Exception(*m_c);
    }

private: /* Fields: */

    ::SharemindVm * m_c;

}; /* class Vm { */


/*******************************************************************************
  Program methods
*******************************************************************************/

inline Program::Program(Vm & vm, Overrides * const overrides)
    : m_c(&vm.newProgram(overrides))
{
    try {
        ::SharemindProgram_setTagWithDestructor(
                    m_c,
                    this,
                    [](void * program) noexcept {
                        Program * const p = static_cast<Program *>(program);
                        p->m_c = nullptr;
                        delete p;
                    });
    } catch (...) {
        ::SharemindProgram_free(m_c);
        throw;
    }
}


/*******************************************************************************
  Process methods
*******************************************************************************/

inline Process::Process(Program & program)
    : m_c(&program.newProcess())
{
    try {
        ::SharemindProcess_setTagWithDestructor(
                    m_c,
                    this,
                    [](void * process) noexcept {
                        Process * const p = static_cast<Process *>(process);
                        p->m_c = nullptr;
                        delete p;
                    });
    } catch (...) {
        ::SharemindProcess_free(m_c);
        throw;
    }
}


/*******************************************************************************
  Clean up helper macros
*******************************************************************************/

#undef SHAREMIND_LIBVM_CXX_DEFINE_EXCEPTION
#undef SHAREMIND_LIBVM_CXX_DEFINE_CPTR_GETTERS
#undef SHAREMIND_LIBVM_CXX_DEFINE_PARENT_GETTER
#undef SHAREMIND_LIBVM_CXX_EXCEPT_OVERRIDES_TEST


} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_CXX_H */
