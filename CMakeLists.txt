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

SET(OTHER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config.local.example")
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
    "instruction_from_code_cases.m4"
    "instruction_from_name_cases.m4"
    "instruction_structs.m4"
    "preprocess_pass1_cases.m4"
    "preprocess_pass2_cases.m4"
    "static_label_structs.m4")
ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR_B}/instr.m4f"
                   COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "${M4_DIR_S}/instr.m4" "--freeze-state=${M4_DIR_B}/instr.m4f"
                   DEPENDS "${M4_DIR_S}/instr.m4"
                   COMMENT "Generating ${M4_DIR_B}/instr.m4f")
ADD_CUSTOM_COMMAND(TARGET "${M4_DIR_B}/instr.m4f" POST_BUILD
                   COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "--reload-state=${M4_DIR_B}/instr.m4f" "${M4_DIR_S}/stats.m4")
FOREACH(_H_SOURCE ${M4_2_H_SOURCES})
  SET(M4_2_H_SOURCES_S ${M4_2_H_SOURCES_S} "${M4_DIR_S}/${_H_SOURCE}")
  STRING(REGEX REPLACE "\\.m4$" ".h" _H_TARGET ${_H_SOURCE})
  ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR_B}/${_H_TARGET}"
                     COMMAND m4 ARGS -E -P "--include=${M4_DIR_S}" "--reload-state=${M4_DIR_B}/instr.m4f" "${M4_DIR_S}/${_H_SOURCE}" > "${M4_DIR_B}/${_H_TARGET}"
                     DEPENDS "${M4_DIR_S}/${_H_SOURCE}" "${M4_DIR_B}/instr.m4f")
ENDFOREACH(_H_SOURCE ${M4_2_H_SOURCES})
SET(OTHER_FILES ${OTHER_FILES} ${M4_2_H_SOURCES_S})


################################################################################
# libsmvmi
################################################################################

ADD_LIBRARY("libsmvmi" SHARED "src/libsmvmi/instr.c"
            ${OTHER_FILES}) # <-- hack for Qt Creator to include other files
SET_TARGET_PROPERTIES("libsmvmi" PROPERTIES
                      OUTPUT_NAME "smvmi"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
SET_SOURCE_FILES_PROPERTIES("src/libsmvmi/instr.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/instruction_from_code_cases.h;${M4_DIR_B}/instruction_from_name_cases.h;${M4_DIR_B}/instruction_structs.h")
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

ADD_LIBRARY("libsmvm" SHARED
            "src/libsmvm/vm_internal_core.c"
            "src/libsmvm/vm_internal_helpers.c")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_core.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/static_label_structs.h;${M4_DIR_B}/dispatches.h")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_helpers.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/preprocess_pass1_cases.h;${M4_DIR_B}/preprocess_pass2_cases.h")
SET_TARGET_PROPERTIES("libsmvm" PROPERTIES
                      OUTPUT_NAME "smvm"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
TARGET_LINK_LIBRARIES("libsmvm" "libsmvmi" "libsme")
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

ADD_EXECUTABLE("vm" "src/vm/main.c")
SET_TARGET_PROPERTIES("vm" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/"
                      VERSION "0.1.0.0")
IF(VM_OUT_OF_MEMORY_DEBUG)
  SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_DEFINITIONS "VM_OUT_OF_MEMORY_DEBUG")
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

FUNCTION(add_test_assemble_execute testfile)
    ADD_TEST(NAME "${testfile}"
             COMMAND tests/run_test_assemble_execute.sh
                 $<TARGET_FILE:smas> $<TARGET_FILE:vm> "tests/${testfile}.sa")
ENDFUNCTION()

add_test_assemble_execute("benchmark")
add_test_assemble_execute("divbyzero")
SET_TESTS_PROPERTIES("divbyzero" PROPERTIES PASS_REGULAR_EXPRESSION "Exception SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute("global_return")
add_test_assemble_execute("mem")
add_test_assemble_execute("proc")
