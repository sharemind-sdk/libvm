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

#ifndef SHAREMIND_LIBVM_PROCESS_H
#define SHAREMIND_LIBVM_PROCESS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sharemind/codeblock.h>
#include <sharemind/Exception.h>
#include <sharemind/ExceptionMacros.h>
#include <string>
#include <type_traits>


namespace sharemind {

class Program;

class Process {

private: /* Types: */

    struct Inner;

public: /* Types: */

    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(sharemind::Exception, Exception);
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(Exception, InvalidInputStateException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            InvalidInputStateException,
            NotInInitializedStateException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(InvalidInputStateException,
                                                   NotInTrappedStateException);
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(Exception, RuntimeException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(RuntimeException,
                                                   TrapException);
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(RuntimeException,
                                         RegularRuntimeException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(RegularRuntimeException,
                                                   InvalidStackIndexException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidRegisterIndexException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidReferenceIndexException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidConstReferenceIndexException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidSyscallIndexException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidMemoryHandleException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            OutOfBoundsReadException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            OutOfBoundsWriteException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            WriteDeniedException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            JumpToInvalidAddressException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            SystemCallErrorException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            InvalidArgumentException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            OutOfBoundsReferenceOffsetException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            OutOfBoundsReferenceSizeException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            IntegerDivideByZeroException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            IntegerOverflowException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            RegularRuntimeException,
            MemoryInUseException);

    class UserDefinedException: public RegularRuntimeException {

    public: /* Methods: */

        UserDefinedException();
        UserDefinedException(UserDefinedException && move)
                noexcept(std::is_nothrow_move_constructible<
                                RegularRuntimeException>::value);
        UserDefinedException(UserDefinedException const & copy)
                noexcept(std::is_nothrow_copy_constructible<
                                RegularRuntimeException>::value);

        ~UserDefinedException() noexcept override;

        UserDefinedException & operator=(UserDefinedException && move)
                noexcept(std::is_nothrow_move_assignable<
                                RegularRuntimeException>::value);

        UserDefinedException & operator=(UserDefinedException const & copy)
                noexcept(std::is_nothrow_copy_assignable<
                                RegularRuntimeException>::value);

        std::uint64_t errorCode() const noexcept;
        void setErrorCode(std::uint64_t const value) noexcept;

        const char * what() const noexcept final override;

    private: /* Methods: */

        std::shared_ptr<void> m_data;

    };
    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(Exception, FloatingPointException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            FloatingPointDivideByZeroException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            FloatingPointOverflowException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            FloatingPointUnderflowException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            FloatingPointInexactResultException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            FloatingPointInvalidOperationException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            FloatingPointException,
            UnknownFloatingPointException);

public: /* Methods: */

    /**
      \brief Constructs a process in an invalid state in which operations other
             than move-assignment and destruction result in undefined behavior.
    */
    Process() noexcept = default;

    /**
      \note The Process object which will be moved from will be left in an
            invalid state in which operations other than move-assignment and
            destruction result in undefined behavior.
    */
    Process(Process &&) noexcept = default;

    Process(Process const &) noexcept = delete;

    /**
       \brief Constructs a process from the given program.
       \param[in] program Reference to the program which the process should run.
    */
    Process(Program & program);

    virtual ~Process() noexcept;

    /**
      \note The Process object which will be moved from will be left in an
            invalid state in which operations other than move-assignment and
            destruction result in undefined behavior.
    */
    Process & operator=(Process &&) noexcept = default;

    Process & operator=(Process const &) noexcept = delete;

    void run();
    void resume();
    void pause() noexcept;

    SharemindCodeBlock returnValue() const noexcept;
    std::exception_ptr syscallException() const noexcept;
    std::size_t currentCodeSectionIndex() const noexcept;
    std::uintptr_t currentIp() const noexcept;

    void setFacility(std::string name, std::shared_ptr<void> facility);
    std::shared_ptr<void> facility(char const * const name) const noexcept;
    std::shared_ptr<void> facility(std::string const & name) const noexcept;
    bool unsetFacility(std::string const & name) noexcept;

private: /* Fields: */

    std::shared_ptr<Inner> m_inner;

}; /* class Process */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROCESS_H */
