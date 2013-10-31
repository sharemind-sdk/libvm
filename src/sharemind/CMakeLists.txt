#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8")
PROJECT("SHAREMIND_LIBVM" "C")

FOREACH(dir IN LISTS CMAKE_PREFIX_PATH)
  INCLUDE_DIRECTORIES(BEFORE SYSTEM "${dir}/include")
ENDFOREACH()

FUNCTION(SHAREMIND_FIND_TARGETLIB libname)
  IF(NOT DEFINED SHAREMIND_TARGETLIB_${libname})
    IF(TARGET "lib${libname}")
      SET(SHAREMIND_TARGETLIB_${libname} "lib${libname}" PARENT_SCOPE)
    ELSE()
      FIND_LIBRARY(libfn "sharemind_${libname}")
      IF(NOT libfn)
        IF("${ARGN}" STREQUAL "OPTIONAL")
          MESSAGE(WARNING "Library \"sharemind_${libname}\" not found!")
        ELSE()
          MESSAGE(FATAL_ERROR "Required library \"sharemind_${libname}\" not found!")
        ENDIF()
      ELSE()
        SET(libs)
        GET_FILENAME_COMPONENT(libpath "${libfn}" PATH)
        LIST(APPEND libs "-L${libpath}")
        LIST(APPEND libs "sharemind_${libname}")
        SET(SHAREMIND_TARGETLIB_${libname} "${libs}" PARENT_SCOPE)
      ENDIF()
    ENDIF()
  ENDIF()
ENDFUNCTION()

# Find boost - seems to be required since the variables aren't populated otherwise
FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

SET(CMAKE_C_FLAGS "-std=c99 -Wall -Wextra -O2 -fstrict-aliasing -Wstrict-aliasing=1 -fwrapv -Wabi -Wbad-function-cast -Wc++-compat -Wformat -Wswitch-default -Wunused -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wconversion -Wsign-conversion -Wunused-macros")

INCLUDE(CheckCCompilerFlag)
MACRO(FLAGS_MAGIC flags)
    CHECK_C_COMPILER_FLAG("-${flags}" SHAREMIND_HAS_CFLAGS__${flags})
    IF(SHAREMIND_HAS_CFLAGS__${flags})
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -${flags}")
    ENDIF(SHAREMIND_HAS_CFLAGS__${flags})
ENDMACRO(FLAGS_MAGIC)
FLAGS_MAGIC("Weverything")
FLAGS_MAGIC("Wlogical-op")

SET(LIBVM_INSTALL_DEFINITIONS "-D__STDC_LIMIT_MACROS")
ADD_DEFINITIONS("${LIBVM_INSTALL_DEFINITIONS}")

################################################################################
# libvm
################################################################################

SET(LIBVM_DEFINES "SHAREMIND_NO_THREADS")
IF(NOT SHAREMIND_RELEASE_BUILD)
  SET(SHAREMIND_LIBVM_DEFINES "${SHAREMIND_LIBVM_DEFINES};SHAREMIND_FAST_BUILD")
ENDIF(NOT SHAREMIND_RELEASE_BUILD)
FILE(GLOB_RECURSE SHAREMIND_LIBVM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/libvm/*.c")
FILE(GLOB_RECURSE SHAREMIND_LIBVM_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/libvm/*.h")
ADD_CUSTOM_TARGET("include_libvm_headers_in_IDE" SOURCES ${SHAREMIND_LIBVM_HEADERS})

SET(SHAREMIND_LIBVM_DEFINES "${SHAREMIND_LIBVM_DEFINES};SHAREMIND_SOFT_FLOAT")

IF(NOT SHAREMIND_LIBVM_LINKING_MODE)
    SET(SHAREMIND_LIBVM_LINKING_MODE SHARED)
ENDIF()

ADD_LIBRARY("libvm" ${SHAREMIND_LIBVM_LINKING_MODE} ${SHAREMIND_LIBVM_SOURCES})
ADD_DEPENDENCIES("libvm" m4_preprocess_pass2_functions_h m4_static_label_structs_h m4_dispatches_h)

SET(LIBVM_VERSION "0.1.0.0")
SET_TARGET_PROPERTIES("libvm" PROPERTIES
                      OUTPUT_NAME "sharemind_vm"
                      VERSION "${LIBVM_VERSION}"
                      SOVERSION "0.1"
                      COMPILE_FLAGS "-frounding-math -ftrapping-math -fsignaling-nans"
                      COMPILE_DEFINITIONS "${SHAREMIND_LIBVM_DEFINES}")

SHAREMIND_FIND_TARGETLIB(executable)
SHAREMIND_FIND_TARGETLIB(modapi)
SHAREMIND_FIND_TARGETLIB(softfloat)
SHAREMIND_FIND_TARGETLIB(vmi)

TARGET_LINK_LIBRARIES("libvm"
                      ${SHAREMIND_TARGETLIB_executable}
                      ${SHAREMIND_TARGETLIB_modapi}
                      ${SHAREMIND_TARGETLIB_softfloat}
                      ${SHAREMIND_TARGETLIB_vmi}
                      "m")

INSTALL(TARGETS "libvm"
        LIBRARY DESTINATION "lib")

INSTALL(FILES "libvm/libvm.h"
        DESTINATION "include/sharemind/libvm")

# Install cmake files:
FILE(GLOB_RECURSE sharemind_libvm_in_files "${CMAKE_CURRENT_SOURCE_DIR}/*.in")
ADD_CUSTOM_TARGET("include_sharemind_libvm_in_files_in_IDE" SOURCES ${sharemind_libvm_in_files})
SET(LIBVM_INSTALL_LINK_LIBRARIES "-L${CMAKE_INSTALL_PREFIX}/lib" "-lsharemind_vm")
SET(LIBVM_INSTALL_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfig.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake" @ONLY)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/SharemindLibvmConfigVersion.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake" @ONLY)
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/SharemindLibvmConfigVersion.cmake"
        DESTINATION "lib/SharemindLibvm")
