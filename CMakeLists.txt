#
# Copyright (C) 2015 Cybernetica
#
# Research/Commercial License Usage
# Licensees holding a valid Research License or Commercial License
# for the Software may use this file according to the written
# agreement between you and Cybernetica.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3.0 as published by the Free Software
# Foundation and appearing in the file LICENSE.GPL included in the
# packaging of this file.  Please review the following information to
# ensure the GNU General Public License version 3.0 requirements will be
# met: http://www.gnu.org/copyleft/gpl-3.0.html.
#
# For further information, please contact us at sharemind@cyber.ee.
#

CMAKE_MINIMUM_REQUIRED(VERSION "3.0")
PROJECT(SharemindLibVm VERSION "0.10.0" LANGUAGES "CXX")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCMakeHelpers 1.6 REQUIRED)

FIND_PACKAGE(SharemindCHeaders 1.3.0 REQUIRED)
FIND_PACKAGE(SharemindCxxHeaders 0.9.0 REQUIRED)
FIND_PACKAGE(SharemindLibExecutable 0.4.0 REQUIRED)
FIND_PACKAGE(SharemindLibSoftfloat 0.3.0 REQUIRED)
FIND_PACKAGE(SharemindLibVmi 0.5.0 REQUIRED)
FIND_PACKAGE(SharemindVmM4 0.5.0 REQUIRED)


# LibVm:
INSTALL(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Process.h"
              "${CMAKE_CURRENT_SOURCE_DIR}/src/Vm.h"
              "${CMAKE_CURRENT_SOURCE_DIR}/src/Program.h"
        DESTINATION "include/sharemind/libvm"
        COMPONENT "dev")
FILE(GLOB_RECURSE SharemindLibVm_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
FILE(GLOB_RECURSE SharemindLibVm_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
SharemindAddSharedLibrary(LibVm
    OUTPUT_NAME "sharemind_vm"
    SOURCES
        ${SharemindLibVm_SOURCES}
        ${SharemindLibVm_HEADERS}
)
TARGET_COMPILE_OPTIONS(LibVm PRIVATE "-fwrapv")
TARGET_INCLUDE_DIRECTORIES(LibVm
    INTERFACE
        # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> # TODO
        $<INSTALL_INTERFACE:include>
    )
TARGET_LINK_LIBRARIES(LibVm
    PRIVATE
        Sharemind::LibSoftfloat
        Sharemind::VmM4
    PUBLIC
        m
        Sharemind::CHeaders
        Sharemind::CxxHeaders
        Sharemind::LibExecutable
        Sharemind::LibVmi
    )
TARGET_COMPILE_DEFINITIONS(LibVm
    PRIVATE
        "SHAREMIND_INTERNAL_=1"
    PUBLIC
        "_XOPEN_SOURCE=700"
        "__STDC_LIMIT_MACROS"
    )
IF(NOT SHAREMIND_RELEASE_BUILD)
    TARGET_COMPILE_DEFINITIONS(LibVm PRIVATE "SHAREMIND_FAST_BUILD")
ENDIF(NOT SHAREMIND_RELEASE_BUILD)
SharemindCreateCMakeFindFilesForTarget(LibVm
    DEPENDENCIES
        "SharemindCHeaders 1.3.0"
        "SharemindCxxHeaders 0.9.0"
        "SharemindLibExecutable 0.4.0"
        "SharemindLibVmi 0.5.0"
)


# Packaging:
SharemindSetupPackaging()
SharemindAddComponentPackage("lib"
    NAME "libsharemind-vm"
    DESCRIPTION "Sharemind VM library"
    DEB_SECTION "libs"
    DEB_DEPENDS
        "libsharemind-vmi (>= 0.5.0)"
        "libsharemind-executable (>= 0.4.0)"
        "libsharemind-softfloat (>= 0.3.0)"
        "libc6 (>= 2.19)"
        "libstdc++6 (>= 4.8.0)"
)
SharemindAddComponentPackage("dev"
    NAME "libsharemind-vm-dev"
    DESCRIPTION "Sharemind VM library development headers"
    DEB_SECTION "libdevel"
    DEB_DEPENDS
        "libsharemind-vm (= ${SharemindLibVm_DEB_lib_PACKAGE_VERSION})"
        "libsharemind-vmi-dev (>= 0.5.0)"
        "libsharemind-cheaders-dev (>= 1.3.0)"
        "libsharemind-cxxheaders-dev (>= 0.9.0)"
        "libc6-dev (>= 2.19)"
        "libstdc++-dev"
)
SharemindAddComponentPackage("debug"
    NAME "libsharemind-vm-dbg"
    DESCRIPTION "Sharemind VM library debug symbols"
    DEB_SECTION "debug"
    DEB_DEPENDS "libsharemind-vm (= ${SharemindLibVm_DEB_lib_PACKAGE_VERSION})"
)
SharemindPackagingFinalize()
