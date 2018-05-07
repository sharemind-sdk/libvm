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
#include <istream>
#include <memory>
#include <sharemind/Exception.h>
#include <sharemind/ExceptionMacros.h>
#include <sharemind/libexecutable/Executable.h>
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
                                                   InvalidFileHeaderException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   VersionMismatchException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            PrepareException,
            InvalidFileHeader0x0Exception);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            PrepareException,
            InvalidLinkingUnitHeader0x0Exception);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            PrepareException,
            InvalidSectionHeader0x0Exception);
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
                                                   NoCodeSectionException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(PrepareException,
                                                   InvalidInstructionException);
    SHAREMIND_DECLARE_EXCEPTION_CONST_MSG_NOINLINE(
            PrepareException,
            InvalidInstructionArgumentsException);

public: /* Methods: */

    /**
      \brief Constructs a program in an invalid state in which operations other
             than move-assignment and destruction result in undefined behavior.
    */
    Program() noexcept = default;

    /**
      \note The Program object which will be moved from will be left in an
            invalid state in which operations other than move-assignment and
            destruction result in undefined behavior.
    */
    Program(Program &&) noexcept = default;

    Program(Program const &) noexcept = delete;

    /**
      \brief Loads the program from the file with the given filename.
      \param[in] vm Reference to the Vm instance.
      \param[in] filename The filename to load the program from.
    */
    Program(Vm & vm, char const * filename);

    /**
      \brief Loads the program from the given FILE object.
      \param[in] vm Reference to the Vm instance.
      \param[in] file The FILE object to load the program from.
    */
    Program(Vm & vm, FILE * file);

    /**
      \brief Loads the program from the given file descriptor.
      \param[in] vm Reference to the Vm instance.
      \param[in] fd The file descriptor to load the program from.
    */
    Program(Vm & vm, int fd);

    /**
      \brief Loads the program from the given memory data area.
      \param[in] vm Reference to the Vm instance.
      \param[in] data Pointer to the memory area to load the program from.
      \param[in] dataSize Size of the memory area to load the program from.
    */
    Program(Vm & vm, void const * data, std::size_t dataSize);

    /**
      \brief Loads the program from the given input stream.
      \param[in] vm Reference to the Vm instance.
      \param[in] inputStream The input stream to load the program from.
    */
    Program(Vm & vm, std::istream & inputStream);

    /**
      \brief Loads the program from the given executable.
      \param[in] vm Reference to the Vm instance.
      \param[in] executable The executable to load the program from.
    */
    Program(Vm & vm, Executable executable);

    virtual ~Program() noexcept;

    /**
      \note The Program object which will be moved from will be left in an
            invalid state in which operations other than move-assignment and
            destruction result in undefined behavior.
    */
    Program & operator=(Program &&) noexcept = default;

    Program & operator=(Program const &) noexcept = delete;

    VmInstructionInfo const * instruction(std::size_t codeSection,
                                          std::size_t instructionIndex)
            const noexcept;

    /// \returns the number of PDs in the FIRST linking unit.
    std::size_t pdCount() const noexcept;

    /// \returns the given PD in the FIRST linking unit.
    SharemindPd * pd(std::size_t const i) const noexcept;

    void setProcessFacility(std::string name, std::shared_ptr<void> facility);
    std::shared_ptr<void> processFacility(char const * const name)
            const noexcept;
    std::shared_ptr<void> processFacility(std::string const & name)
            const noexcept;
    bool unsetProcessFacility(std::string const & name) noexcept;

private: /* Fields: */

    std::shared_ptr<Inner> m_inner;

}; /* class Program */

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PROGRAM_H */
