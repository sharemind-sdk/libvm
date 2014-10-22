#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8.12")
PROJECT("SHAREMIND_LIBVM" "C")

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)

FIND_PACKAGE(SharemindLibexecutable REQUIRED)
FIND_PACKAGE(SharemindLibmodapi 0.2.0 REQUIRED)
FIND_PACKAGE(SharemindLibsoftfloat REQUIRED)
FIND_PACKAGE(SharemindLibvmi REQUIRED)
FIND_PACKAGE(SharemindVmGlobals REQUIRED)
FIND_PACKAGE(SharemindVmM4 REQUIRED)

INCLUDE_DIRECTORIES(
    ${SharemindLibexecutable_INCLUDE_DIRS}
    ${SharemindLibmodapi_INCLUDE_DIRS}
    ${SharemindLibsoftfloat_INCLUDE_DIRS}
    ${SharemindLibvmi_INCLUDE_DIRS}
    ${SharemindVmGlobals_INCLUDE_DIRS}
    ${SharemindVmM4_INCLUDE_DIRS}
)

ADD_COMPILE_OPTIONS(
    "-std=c99" "-O2" "-fwrapv" "-pthread" "-Wall" "-Wextra" "-Wunused" "-Wabi"
    "-Wstrict-aliasing=1" "-Wbad-function-cast" "-Wformat" "-Wswitch-default"
    "-Wfloat-equal" "-Wshadow" "-Wpointer-arith" "-Wcast-qual"
    "-Wstrict-prototypes" "-Wconversion" "-Wsign-conversion" "-Wunused-macros"
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

SET(LIBVM_INSTALL_DEFINITIONS "-D__STDC_LIMIT_MACROS" "-D_XOPEN_SOURCE=700")
ADD_DEFINITIONS(
    ${SharemindLibexecutable_DEFINITIONS}
    ${SharemindLibmodapi_DEFINITIONS}
    ${SharemindLibsoftfloat_DEFINITIONS}
    ${SharemindLibvmi_DEFINITIONS}
    ${SharemindVmGlobals_DEFINITIONS}
    ${SharemindVmM4_DEFINITIONS}
    ${LIBVM_INSTALL_DEFINITIONS}
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
ADD_DEPENDENCIES("libvm"
                 m4_preprocess_pass2_functions_h
                 m4_static_label_structs_h
                 m4_dispatches_h)

SET(LIBVM_VERSION "0.3.0.0")
SET(LIBVM_COMPAT_VERSION "0.3.0.0")
SET_TARGET_PROPERTIES("libvm" PROPERTIES
                      OUTPUT_NAME "sharemind_vm"
                      VERSION "${LIBVM_VERSION}"
                      SOVERSION "0.2")

TARGET_LINK_LIBRARIES("libvm"
                      ${SharemindLibexecutable_LIBRARIES}
                      ${SharemindLibmodapi_LIBRARIES}
                      ${SharemindLibsoftfloat_LIBRARIES}
                      ${SharemindLibvmi_LIBRARIES}
                      "m")

FIND_PROGRAM(LIBVM_STRIP strip)
IF(NOT LIBVM_STRIP)
    MESSAGE(WARNING "\"strip\" not found.")
ELSE()
    ADD_CUSTOM_COMMAND(TARGET "libvm" POST_BUILD COMMAND
                       "${LIBVM_STRIP}" --strip-all "$<TARGET_FILE:libvm>"
                       || "${LIBVM_STRIP}" "$<TARGET_FILE:libvm>"
                       || "${CMAKE_COMMAND}" -E echo "STRIP FAILED!!!" || true)
ENDIF()

INSTALL(TARGETS "libvm"
        LIBRARY DESTINATION "lib")

INSTALL(FILES "src/libvm.h" "src/libvmcxx.h"
        DESTINATION "include/sharemind/libvm")

# Install cmake files:
FILE(GLOB_RECURSE sharemind_libvm_in_files "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
ADD_CUSTOM_TARGET("include_sharemind_libvm_in_files_in_IDE"
                  SOURCES ${sharemind_libvm_in_files})
SET(LIBVM_INSTALL_LINK_LIBRARIES
    "-L${CMAKE_INSTALL_PREFIX}/lib" "-lsharemind_vm")
SET(LIBVM_INSTALL_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake" @ONLY)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfigVersion.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake"
        DESTINATION "lib/SharemindLibvm")
