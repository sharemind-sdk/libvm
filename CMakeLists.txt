#
# This file is a part of the Sharemind framework.
# Copyright (C) Cybernetica AS
#
# All rights are reserved. Reproduction in whole or part is prohibited
# without the written consent of the copyright owner. The usage of this
# code is subject to the appropriate license agreement.
#

CMAKE_MINIMUM_REQUIRED(VERSION "2.8")
CMAKE_POLICY(SET CMP0015 NEW)
PROJECT("SHAREMIND_VM" "C")

FIND_PACKAGE(Boost)
IF (NOT Boost_FOUND)
    MESSAGE(FATAL_ERROR "Required Boost library headers not found!")
ENDIF (NOT Boost_FOUND)

LINK_DIRECTORIES("lib")
SET(SM_INSTALL_INCLUDE_DIR "include/sharemind/libs")
SET(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -O2 -fstrict-aliasing -Wstrict-aliasing=1")

SET(VM_OUT_OF_MEMORY_DEBUG FALSE CACHE BOOL "Enable out of memory debugging for the vm binary.")

################################################################################
# Load any custom configuration:
################################################################################

SET(OTHER_FILES "config.local.example"
                "src/codeblock.h"
                "src/instrset.h"
                "src/likely.h"
                "src/map.h"
                "src/preprocessor.h"
                "src/stack.h"
                "src/static_assert.h"
                "src/trie.h"
                "src/vector.h"
                "tests/benchmark.sa"
                "tests/divbyzero.sa"
                "tests/divoverflow.sa"
                "tests/float_divbyzero.sa"
                "tests/float_overflow.sa"
                "tests/float_underflow.sa"
                "tests/global_return.sa"
                "tests/mem.sa"
                "tests/proc.sa"
                "tests/run_test_assemble_execute.sh")
FUNCTION(PREPARE_OTHER_FILES)
    FOREACH(OTHER_FILE ${OTHER_FILES})
        SET(P ${P} "${CMAKE_CURRENT_SOURCE_DIR}/${OTHER_FILE}")
        SET(PREPARED_OTHER_FILES ${P} PARENT_SCOPE)
    ENDFOREACH(OTHER_FILE ${OTHER_FILES})
ENDFUNCTION(PREPARE_OTHER_FILES)
INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/config.local" OPTIONAL)
INCLUDE("${CMAKE_CURRENT_BINARY_DIR}/config.local" OPTIONAL)


################################################################################
# m4 files
################################################################################

SET(M4_DIR "src/m4")
SET(M4_DIR_S "${CMAKE_CURRENT_SOURCE_DIR}/${M4_DIR}")
IF(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  SET(M4_DIR_B "${CMAKE_CURRENT_BINARY_DIR}/generated_sources/${M4_DIR}")
  FILE(MAKE_DIRECTORY ${M4_DIR_B})
  INCLUDE_DIRECTORIES(BEFORE ${M4_DIR_B})
ELSE(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  SET(M4_DIR_B ${M4_DIR_S})
ENDIF(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")

SET(M4_2_H_SOURCES
    "dispatches.m4"
    "instruction_structs.m4"
    "static_label_structs.m4"
    "preprocess_pass2_functions.m4")
IF(VM_FAST_BUILD)
  SET(M4_2_H_SOURCES ${M4_2_H_SOURCES}
      "instruction_index.m4")
  SET(OTHER_FILES ${OTHER_FILES}
      "${M4_DIR}/instruction_from_code_cases.m4"
      "${M4_DIR}/instruction_from_name_cases.m4")
ELSE(VM_FAST_BUILD)
  SET(M4_2_H_SOURCES ${M4_2_H_SOURCES}
      "instruction_from_code_cases.m4"
      "instruction_from_name_cases.m4")
  SET(OTHER_FILES ${OTHER_FILES}
      "${M4_DIR}/instruction_index.m4")
ENDIF(VM_FAST_BUILD)
SET(OTHER_FILES ${OTHER_FILES}
                "${M4_DIR}/datatypebyte.m4"
                "${M4_DIR}/foreach.m4"
                "${M4_DIR}/forloop.m4"
                "${M4_DIR}/instr.m4"
                "${M4_DIR}/operandlocationbyte.m4"
                "${M4_DIR}/product.m4"
                "${M4_DIR}/stats.m4")

ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR_B}/instr.m4f"
                   COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "${M4_DIR_S}/instr.m4" "--freeze-state=${M4_DIR_B}/instr.m4f"
                   DEPENDS "${M4_DIR_S}/instr.m4"
                   COMMENT "Generating ${M4_DIR_B}/instr.m4f")
ADD_CUSTOM_COMMAND(TARGET "${M4_DIR_B}/instr.m4f" POST_BUILD
                   COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "--reload-state=${M4_DIR_B}/instr.m4f" "${M4_DIR_S}/stats.m4")
FOREACH(_H_SOURCE ${M4_2_H_SOURCES})
  SET(OTHER_FILES ${OTHER_FILES} "${M4_DIR}/${_H_SOURCE}")
  SET(M4_2_H_SOURCES_S ${M4_2_H_SOURCES_S} "${M4_DIR_S}/${_H_SOURCE}")
  STRING(REGEX REPLACE "\\.m4$" ".h" _H_TARGET ${_H_SOURCE})
  ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR_B}/${_H_TARGET}"
                     COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "--reload-state=${M4_DIR_B}/instr.m4f" "${M4_DIR_S}/${_H_SOURCE}" > "${M4_DIR_B}/${_H_TARGET}"
                     DEPENDS "${M4_DIR_S}/${_H_SOURCE}" "${M4_DIR_B}/instr.m4f")
ENDFOREACH(_H_SOURCE ${M4_2_H_SOURCES})


################################################################################
# libsmvmi
################################################################################

ADD_LIBRARY("libsmvmi" SHARED "src/libsmvmi/instr.c")
SET_TARGET_PROPERTIES("libsmvmi" PROPERTIES
                      OUTPUT_NAME "smvmi"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
IF(VM_FAST_BUILD)
  SET_TARGET_PROPERTIES("libsmvmi" PROPERTIES COMPILE_DEFINITIONS "SMVMI_FAST_BUILD")
  SET_SOURCE_FILES_PROPERTIES("src/libsmvmi/instr.c" PROPERTIES OBJECT_DEPENDS
                              "${M4_DIR_B}/instruction_index.h;${M4_DIR_B}/instruction_structs.h")
ELSE(VM_FAST_BUILD)
  SET_SOURCE_FILES_PROPERTIES("src/libsmvmi/instr.c" PROPERTIES OBJECT_DEPENDS
                              "${M4_DIR_B}/instruction_from_code_cases.h;${M4_DIR_B}/instruction_from_name_cases.h;${M4_DIR_B}/instruction_structs.h")
ENDIF(VM_FAST_BUILD)
INSTALL(DIRECTORY "src/libsmvmi/"
        DESTINATION "${SM_INSTALL_INCLUDE_DIR}/libsmvmi"
        FILES_MATCHING PATTERN "*.h")


################################################################################
# libsme
################################################################################

ADD_LIBRARY("libsme" SHARED
            "src/libsme/libsme.c"
            "src/libsme/libsme_0x0.c")
SET_TARGET_PROPERTIES("libsme" PROPERTIES
                      OUTPUT_NAME "sme"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
INSTALL(DIRECTORY "src/libsme/"
        DESTINATION "${SM_INSTALL_INCLUDE_DIR}/libsme"
        FILES_MATCHING PATTERN "*.h")


################################################################################
# libsmas
################################################################################

ADD_LIBRARY("libsmas" SHARED
            "src/libsmas/assemble.c"
            "src/libsmas/linker.c"
            "src/libsmas/linkingunits.c"
            "src/libsmas/stdion.c"
            "src/libsmas/tokenizer.c"
            "src/libsmas/tokens.c")
SET_TARGET_PROPERTIES("libsmas" PROPERTIES
                      OUTPUT_NAME "smas"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
TARGET_LINK_LIBRARIES("libsmas" "libsmvmi" "libsme")
INSTALL(DIRECTORY "src/libsmas/"
        DESTINATION "${SM_INSTALL_INCLUDE_DIR}/libsmas"
        FILES_MATCHING PATTERN "stdion.h" EXCLUDE
                       PATTERN "*.h")


################################################################################
# libsmvm
################################################################################

SET(LIBSMVM_DEFINES "SMVM_NO_THREADS")

IF(NOT VM_SOFT_FLOAT)
  ADD_LIBRARY("libsmvm" SHARED
              "src/libsmvm/vm_internal_core.c"
              "src/libsmvm/vm_internal_helpers.c")
ELSE(NOT VM_SOFT_FLOAT)
  SET(LIBSMVM_DEFINES "${LIBSMVM_DEFINES};SMVM_SOFT_FLOAT")
  ADD_LIBRARY("libsmvm" SHARED
              "src/libsmvm/vm_internal_core.c"
              "src/libsmvm/vm_internal_helpers.c"
              "src/3rdparty/softfloat/softfloat.c")
ENDIF(NOT VM_SOFT_FLOAT)
SET(OTHER_FILES ${OTHER_FILES} "src/libsmvm/vm.h")
SET_TARGET_PROPERTIES("libsmvm" PROPERTIES
                      OUTPUT_NAME "smvm"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1"
                      COMPILE_FLAGS "-frounding-math -ftrapping-math -fsignaling-nans")

IF(VM_FAST_BUILD)
  SET(LIBSMVM_DEFINES "${LIBSMVM_DEFINES};SMVM_FAST_BUILD")
ENDIF(VM_FAST_BUILD)
SET_TARGET_PROPERTIES("libsmvm" PROPERTIES COMPILE_DEFINITIONS "${LIBSMVM_DEFINES}")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_helpers.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/preprocess_pass2_functions.h")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_core.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/static_label_structs.h;${M4_DIR_B}/dispatches.h")
TARGET_LINK_LIBRARIES("libsmvm" "libsmvmi" "libsme" "m")
INSTALL(FILES "src/libsmvm/vm.h"
        DESTINATION "${SM_INSTALL_INCLUDE_DIR}/libsmvm")


################################################################################
# smas
################################################################################

ADD_EXECUTABLE("smas" "src/smas/main.c")
SET_TARGET_PROPERTIES("smas" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/"
                      VERSION "0.1.0.0")
TARGET_LINK_LIBRARIES("smas" "libsmas")


################################################################################
# vm
################################################################################

PREPARE_OTHER_FILES()
ADD_EXECUTABLE("vm" "src/vm/main.c"
               ${PREPARED_OTHER_FILES}) # <-- hack for Qt Creator to include other files
SET_TARGET_PROPERTIES("vm" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/"
                      VERSION "0.1.0.0")
IF(VM_OUT_OF_MEMORY_DEBUG)
  # This is because of http://gcc.gnu.org/bugzilla/show_bug.cgi?id=44736 :
  SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_FLAGS "-fno-builtin-malloc")
  IF(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
    SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_DEFINITIONS "VM_OUT_OF_MEMORY_DEBUG;VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE")
  ELSE(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
    SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_DEFINITIONS "VM_OUT_OF_MEMORY_DEBUG")
  ENDIF(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
ENDIF(VM_OUT_OF_MEMORY_DEBUG)
TARGET_LINK_LIBRARIES("vm" "libsmvmi" "libsmvm")


################################################################################
# Installs:
################################################################################

INSTALL(TARGETS "libsmvmi" "libsme" "libsmas" "libsmvm" "vm" "smas"
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        PUBLIC_HEADER DESTINATION "include")


################################################################################
# Tests:
################################################################################

SET(TESTS_DIR_B "${CMAKE_CURRENT_BINARY_DIR}/tests")
SET(TESTS_DIR_S "${CMAKE_CURRENT_SOURCE_DIR}/tests")
FILE(MAKE_DIRECTORY "${TESTS_DIR_B}")

FUNCTION(add_test_assemble_execute testfile)
    ADD_TEST(NAME "${testfile}"
             COMMAND "${TESTS_DIR_S}/run_test_assemble_execute.sh"
                 $<TARGET_FILE:smas> $<TARGET_FILE:vm> "${TESTS_DIR_S}/${testfile}.sa" "${TESTS_DIR_B}/${testfile}.sb")
ENDFUNCTION()

add_test_assemble_execute("benchmark")
add_test_assemble_execute("divbyzero")
SET_TESTS_PROPERTIES("divbyzero" PROPERTIES PASS_REGULAR_EXPRESSION "Exception (SMVM_E_INTEGER_DIVIDE_BY_ZERO|SMVM_E_UNKNOWN_FPE)")
add_test_assemble_execute("divoverflow")
add_test_assemble_execute("float_divbyzero")
SET_TESTS_PROPERTIES("float_divbyzero" PROPERTIES PASS_REGULAR_EXPRESSION "Exception (SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO|SMVM_E_FLOATING_POINT_INEXACT_RESULT|SMVM_E_UNKNOWN_FPE)")
add_test_assemble_execute("float_overflow")
SET_TESTS_PROPERTIES("float_overflow" PROPERTIES PASS_REGULAR_EXPRESSION "Exception (SMVM_E_FLOATING_POINT_OVERFLOW|SMVM_E_FLOATING_POINT_INEXACT_RESULT|SMVM_E_UNKNOWN_FPE)")
add_test_assemble_execute("float_underflow")
SET_TESTS_PROPERTIES("float_underflow" PROPERTIES PASS_REGULAR_EXPRESSION "Exception (SMVM_E_FLOATING_POINT_UNDERFLOW|SMVM_E_FLOATING_POINT_INEXACT_RESULT|SMVM_E_UNKNOWN_FPE)")
add_test_assemble_execute("float_invalid")
SET_TESTS_PROPERTIES("float_invalid" PROPERTIES PASS_REGULAR_EXPRESSION "Exception (SMVM_E_FLOATING_POINT_INVALID_OPERATION|SMVM_E_FLOATING_POINT_INEXACT_RESULT|SMVM_E_UNKNOWN_FPE)")
add_test_assemble_execute("global_return")
add_test_assemble_execute("mem")
add_test_assemble_execute("proc")
