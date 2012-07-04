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

ADD_DEFINITIONS("-D_XOPEN_SOURCE=500" "-D__STDC_LIMIT_MACROS")

################################################################################
# libvm
################################################################################

SET(LIBVM_DEFINES "SHAREMIND_NO_THREADS")
IF(NOT SHAREMIND_RELEASE_BUILD)
  SET(SHAREMIND_LIBVM_DEFINES "${SHAREMIND_LIBVM_DEFINES};SHAREMIND_FAST_BUILD")
ENDIF(NOT SHAREMIND_RELEASE_BUILD)
SET(SHAREMIND_LIBVM_SOURCES
    "libvm/core.c"
    "libvm/process.c"
    "libvm/program.c"
    "libvm/vm.c")
SET(SHAREMIND_LIBVM_DEFINES "${SHAREMIND_LIBVM_DEFINES};SHAREMIND_SOFT_FLOAT")

IF(NOT SHAREMIND_LIBVM_LINKING_MODE)
    SET(SHAREMIND_LIBVM_LINKING_MODE SHARED)
ENDIF()

ADD_LIBRARY("libvm" ${SHAREMIND_LIBVM_LINKING_MODE} ${SHAREMIND_LIBVM_SOURCES})
ADD_DEPENDENCIES("libvm" m4_preprocess_pass2_functions_h m4_static_label_structs_h m4_dispatches_h)
SET_TARGET_PROPERTIES("libvm" PROPERTIES
                      OUTPUT_NAME "sharemind_vm"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1"
                      COMPILE_FLAGS "-frounding-math -ftrapping-math -fsignaling-nans"
                      COMPILE_DEFINITIONS "${SHAREMIND_LIBVM_DEFINES}")
TARGET_LINK_LIBRARIES("libvm" "libexecutable" "libmodapi" "libsoftfloat" "libvmi" "m")

INSTALL(TARGETS "libvm"
        LIBRARY DESTINATION "lib")

INSTALL(FILES "libvm/libvm.h"
        DESTINATION "include/sharemind/libvm")
