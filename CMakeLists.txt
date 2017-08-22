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

CMAKE_MINIMUM_REQUIRED(VERSION "2.8.12")
PROJECT("SHAREMIND_LIBVM" "C")

SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "5")
SET(CPACK_PACKAGE_VERSION_PATCH "0")
SET(LIBVM_SOVERSION
    "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
SET(LIBVM_VERSION "${LIBVM_SOVERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
SET(LIBVM_COMPAT_VERSION "${LIBVM_VERSION}")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindCHeaders REQUIRED)
FIND_PACKAGE(SharemindLibExecutable REQUIRED)
FIND_PACKAGE(SharemindLibmodapi 0.3.0 REQUIRED)
FIND_PACKAGE(SharemindLibSoftfloat REQUIRED)
FIND_PACKAGE(SharemindLibVmi REQUIRED)
FIND_PACKAGE(SharemindVmM4 REQUIRED)

INCLUDE_DIRECTORIES(
    ${SharemindCHeaders_INCLUDE_DIRS}
    ${SharemindLibExecutable_INCLUDE_DIRS}
    ${SharemindLibmodapi_INCLUDE_DIRS}
    ${SharemindLibSoftfloat_INCLUDE_DIRS}
    ${SharemindLibVmi_INCLUDE_DIRS}
    ${SharemindVmM4_INCLUDE_DIRS}
)

ADD_COMPILE_OPTIONS(
    "-std=c99" "-O2" "-fwrapv" "-pthread" "-Wall" "-Wextra" "-Wunused" "-Wabi"
    "-Wstrict-aliasing=1" "-Wbad-function-cast" "-Wformat" "-Wswitch-default"
    "-Wfloat-equal" "-Wshadow" "-Wpointer-arith" "-Wcast-qual"
    "-Wstrict-prototypes" "-Wconversion" "-Wsign-conversion" "-Wunused-macros"
# Uncomment these to benchmark compilation:
#    "-Q" "-ftime-report" "-fmem-report" "-fmem-report-wpa"
#    "-fpre-ipa-mem-report" "-fpost-ipa-mem-report" "-fopt-info-optall-all"
    "$<$<NOT:$<STREQUAL:$<CONFIGURATION>,Release>>:-fno-omit-frame-pointer>"
    "$<$<STREQUAL:$<CONFIGURATION>,Release>:-DNDEBUG>"
    "$<$<STREQUAL:$<CONFIGURATION>,Release>:-fomit-frame-pointer>"
)

INCLUDE(CheckCCompilerFlag)
FUNCTION(CHECK_ADD_COMPILER_FLAG flag)
    STRING(SUBSTRING "${flag}" 1 -1 FlagName)
    CHECK_C_COMPILER_FLAG("${flag}" C_COMPILER_HAS_${FlagName}_FLAG)
    IF(C_COMPILER_HAS_${FlagName}_FLAG)
        ADD_COMPILE_OPTIONS("${flag}")
    ENDIF()
ENDFUNCTION()
CHECK_ADD_COMPILER_FLAG("-Weverything")
CHECK_ADD_COMPILER_FLAG("-Wlogical-op")
CHECK_ADD_COMPILER_FLAG("-Wno-covered-switch-default")
CHECK_ADD_COMPILER_FLAG("-Wno-padded")

SET(LIBVM_INSTALL_DEFINITIONS "-D__STDC_LIMIT_MACROS" "-D_XOPEN_SOURCE=700")
ADD_DEFINITIONS(
    ${SharemindCHeaders_DEFINITIONS}
    ${SharemindLibExecutable_DEFINITIONS}
    ${SharemindLibmodapi_DEFINITIONS}
    ${SharemindLibSoftfloat_DEFINITIONS}
    ${SharemindLibVmi_DEFINITIONS}
    ${SharemindVmM4_DEFINITIONS}
    ${LIBVM_INSTALL_DEFINITIONS}
    "-DSHAREMIND_INTERNAL_=1"
)

SET(LIBVM_LINK_LIBRARIES
    ${SharemindLibExecutable_LIBRARIES}
    ${SharemindLibmodapi_LIBRARIES}
    ${SharemindLibSoftfloat_LIBRARIES}
    ${SharemindLibVmi_LIBRARIES}
    "m"
    "pthread"
)

################################################################################
# libvm
################################################################################

SET(LIBVM_DEFINES "SHAREMIND_NO_THREADS")
IF(NOT SHAREMIND_RELEASE_BUILD)
    ADD_DEFINITIONS("-DSHAREMIND_FAST_BUILD")
ENDIF(NOT SHAREMIND_RELEASE_BUILD)
FILE(GLOB_RECURSE SHAREMIND_LIBVM_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
FILE(GLOB_RECURSE SHAREMIND_LIBVM_HEADERS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
ADD_CUSTOM_TARGET("include_libvm_headers_in_IDE"
                  SOURCES ${SHAREMIND_LIBVM_HEADERS})

IF(NOT SHAREMIND_LIBVM_LINKING_MODE)
    SET(SHAREMIND_LIBVM_LINKING_MODE SHARED)
ENDIF()

ADD_LIBRARY("libvm" ${SHAREMIND_LIBVM_LINKING_MODE} ${SHAREMIND_LIBVM_SOURCES})
SET_TARGET_PROPERTIES("libvm" PROPERTIES
                      OUTPUT_NAME "sharemind_vm"
                      VERSION "${LIBVM_VERSION}"
                      SOVERSION "${LIBVM_SOVERSION}")

TARGET_LINK_LIBRARIES("libvm" ${LIBVM_LINK_LIBRARIES})

IF(NOT "${SHAREMIND_LIBVM_LINKING_MODE}" STREQUAL "STATIC")
    FIND_PROGRAM(LIBVM_STRIP strip)
    IF(NOT LIBVM_STRIP)
        MESSAGE(WARNING "\"strip\" not found.")
    ELSE()
        ADD_CUSTOM_COMMAND(TARGET "libvm" POST_BUILD COMMAND
                           "${LIBVM_STRIP}" --strip-all "$<TARGET_FILE:libvm>"
                           || "${LIBVM_STRIP}" "$<TARGET_FILE:libvm>"
                           || "${CMAKE_COMMAND}" -E echo "STRIP FAILED!!!" || true)
    ENDIF()
ENDIF()

INSTALL(TARGETS "libvm"
        COMPONENT lib
        LIBRARY DESTINATION "lib")

INSTALL(FILES "src/libvm.h"
        DESTINATION "include/sharemind/libvm"
        COMPONENT dev)

# Install cmake files:
FILE(GLOB_RECURSE sharemind_libvm_in_files "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
ADD_CUSTOM_TARGET("include_sharemind_libvm_in_files_in_IDE"
                  SOURCES ${sharemind_libvm_in_files})
SET(LIBVM_INSTALL_LINK_LIBRARIES
    ${LIBVM_LINK_LIBRARIES} "-L${CMAKE_INSTALL_PREFIX}/lib" "-lsharemind_vm")
SET(LIBVM_INSTALL_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake" @ONLY)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfigVersion.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake"
        DESTINATION "lib/SharemindLibvm"
        COMPONENT dev)


# Packaging:
SET(CPACK_PACKAGE_VENDOR "Cybernetica AS")
SET(CPACK_PACKAGE_CONTACT "Sharemind packaging <sharemind-packaging@cyber.ee>")
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_NAME "libsharemind-vm")
SET(CPACK_DEBIAN_PACKAGE_RELEASE "cyber1")
SET(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
SET(CPACK_DEB_COMPONENT_INSTALL "ON")
SET(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

SET(CPACK_DEBIAN_LIB_PACKAGE_NAME "libsharemind-vm")
SET(CPACK_COMPONENT_DEV_DESCRIPTION
    "Sharemind VM library development headers")
SET(CPACK_COMPONENT_LIB_DESCRIPTION "Sharemind VM library")
SET(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libsharemind-vm (= ${LIBVM_VERSION}), libsharemind-vmi-dev (>= 0.1.0), libsharemind-modapi-dev (>= 0.3.0), libsharemind-cheaders-dev (>= 0.1.0), libc6-dev (>= 2.19)")
SET(CPACK_DEBIAN_LIB_PACKAGE_DEPENDS "libsharemind-vmi (>= 0.1.0), libsharemind-executable (>= 0.1.0), libsharemind-modapi (>= 0.3.0), libc6 (>= 2.19)")
SET(CPACK_DEBIAN_DEV_PACKAGE_SECTION "libdevel")
SET(CPACK_DEBIAN_LIB_PACKAGE_SECTION "libs")
INCLUDE(CPack)
