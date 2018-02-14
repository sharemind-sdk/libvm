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

#ifndef SHAREMIND_LIBVM_PROGRAM_H
#define SHAREMIND_LIBVM_PROGRAM_H

#include <cstddef>
#include <memory>
#include <sharemind/Exception.h>
#include <sharemind/ExceptionMacros.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/libvmi/instr.h>


namespace sharemind {
class Vm;

class Program {

    friend class Process;

private: /* Types: */

    struct Inner;

public: /* Types: */

    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(sharemind::Exception, Exception);
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(Exception, IoException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(IoException,
                                                   FileOpenException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(IoException,
                                                   FileNoException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(IoException,
                                                   FileFstatException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(IoException,
                                                   FileReadException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            Exception,
            ImplementationLimitsReachedException);
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(Exception, PrepareException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   InvalidHeaderException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   VersionMismatchException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   InvalidInputFileException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_STDSTRING_NOINLINE(
            PrepareException,
            UndefinedSyscallBindException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_STDSTRING_NOINLINE(
            PrepareException,
            UndefinedPdBindException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_STDSTRING_NOINLINE(
            PrepareException,
            DuplicatePdBindException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   NoCodeSectionsException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   InvalidInstructionException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            PrepareException,
            InvalidInstructionArgumentsException);

public: /* Methods: */

    Program(Vm & vm);
    virtual ~Program() noexcept;

    bool isReady() const noexcept;

    void loadFromFile(char const * const filename);
    void loadFromCFile(FILE * const file);
    void loadFromFileDescriptor(int const fd);
    void loadFromMemory(void const * data, std::size_t dataSize);

    SharemindVmInstruction const * instruction(std::size_t codeSection,
                                               std::size_t instructionIndex)
            const noexcept;

    std::size_t pdCount() const noexcept;

    SharemindPd * pd(std::size_t const i) const noexcept;

    void setProcessFacility(std::string name, void * facility);
    void * processFacility(char const * const name) const noexcept;
    void * processFacility(std::string const & name) const noexcept;
    bool unsetProcessFacility(std::string const & name) noexcept;

private: /* Fields: */

    std::shared_ptr<Inner> m_inner;

}; /* class Program */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
