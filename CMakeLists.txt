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
PROJECT("SHAREMIND_VM" "C" "CXX")

FIND_PACKAGE(Boost)
IF (NOT Boost_FOUND)
    MESSAGE(FATAL_ERROR "Required Boost library headers not found!")
ENDIF (NOT Boost_FOUND)

LINK_DIRECTORIES("lib")
SET(SM_INSTALL_INCLUDE_DIR "include/sharemind/libs")
SET(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -O2 -fstrict-aliasing -Wstrict-aliasing=1 -fwrapv -Wabi -Wbad-function-cast -Wc++-compat -Wformat -Wswitch-default -Wunused -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wconversion -Wsign-conversion -Wlogical-op -Wunused-macros")
SET(CMAKE_CXX_FLAGS "-std=gnu++98 -Wall -Wextra -O2 -fstrict-aliasing -Wstrict-aliasing=1 -fwrapv -Wabi -Wformat -Wswitch-default -Wunused -Wfloat-equal -Wshadow -Wpointer-arith -Wcast-qual -Wconversion -Wsign-conversion -Wlogical-op -Wunused-macros")
ADD_DEFINITIONS("-D_XOPEN_SOURCE=500" "-D__STDC_LIMIT_MACROS")

SET(VM_OUT_OF_MEMORY_DEBUG FALSE CACHE BOOL "Enable out of memory debugging for the vm binary.")

################################################################################
# Load any custom configuration:
################################################################################

SET(OTHER_FILES "config.local.example"
                "src/codeblock.h"
                "src/fnv.h"
                "src/instrset.h"
                "src/likely.h"
                "src/map.h"
                "src/preprocessor.h"
                "src/stack.h"
                "src/static_assert.h"
                "src/trie.h"
                "src/vector.h"
                "tests/alloc_zero.sa"
                "tests/benchmark.sa"
                "tests/except_bmodbyzero_int8.sa"
                "tests/except_bmodbyzero_int16.sa"
                "tests/except_bmodbyzero_int32.sa"
                "tests/except_bmodbyzero_int64.sa"
                "tests/except_bmodbyzero_uint8.sa"
                "tests/except_bmodbyzero_uint16.sa"
                "tests/except_bmodbyzero_uint32.sa"
                "tests/except_bmodbyzero_uint64.sa"
                "tests/except_bmod2byzero_int8.sa"
                "tests/except_bmod2byzero_int16.sa"
                "tests/except_bmod2byzero_int32.sa"
                "tests/except_bmod2byzero_int64.sa"
                "tests/except_bmod2byzero_uint8.sa"
                "tests/except_bmod2byzero_uint16.sa"
                "tests/except_bmod2byzero_uint32.sa"
                "tests/except_bmod2byzero_uint64.sa"
                "tests/except_divbyzero_float32.sa"
                "tests/except_divbyzero_int8.sa"
                "tests/except_divbyzero_int16.sa"
                "tests/except_divbyzero_int32.sa"
                "tests/except_divbyzero_int64.sa"
                "tests/except_divbyzero_uint8.sa"
                "tests/except_divbyzero_uint16.sa"
                "tests/except_divbyzero_uint32.sa"
                "tests/except_divbyzero_uint64.sa"
                "tests/except_divoverflow_int64.sa"
                "tests/except_invalid_float32.sa"
                "tests/except_overflow_float32.sa"
                "tests/except_tmodbyzero_int8.sa"
                "tests/except_tmodbyzero_int16.sa"
                "tests/except_tmodbyzero_int32.sa"
                "tests/except_tmodbyzero_int64.sa"
                "tests/except_tmodbyzero_uint8.sa"
                "tests/except_tmodbyzero_uint16.sa"
                "tests/except_tmodbyzero_uint32.sa"
                "tests/except_tmodbyzero_uint64.sa"
                "tests/except_underflow_float32.sa"
                "tests/except_write_to_rodata.sa"
                "tests/bmod.sa"
                "tests/bmod2.sa"
                "tests/factorial.sa"
                "tests/global_return.sa"
                "tests/mem.sa"
                "tests/proc.sa"
                "tests/run_test_assemble_execute.sh"
                "tests/syscall_putchar.sa"
                "tests/section_bss.sa"
                "tests/section_data.sa"
                "tests/section_rodata.sa"
                "tests/tmod.sa")
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
    "instruction_index.m4"
    "instruction_structs.m4"
    "static_label_structs.m4"
    "preprocess_pass2_functions.m4")
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
SET_SOURCE_FILES_PROPERTIES("src/libsmvmi/instr.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR_B}/instruction_structs.h;${M4_DIR_B}/instruction_index.h")
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
# libsmmod
################################################################################

ADD_LIBRARY("libsmmod" SHARED
            "src/libsmmod/modapi.c"
            "src/libsmmod/modapi_0x1.c"
            "src/libsmmod/pd.c"
            "src/libsmmod/pdk.c"
            "src/libsmmod/pdkmap.c"
            "src/libsmmod/pdpi.c"
            "src/libsmmod/syscall.c"
            "src/libsmmod/syscallmap.c")
SET(OTHER_FILES ${OTHER_FILES}
    "src/libsmmod/modapi.h"
    "src/libsmmod/modapi_internal.h"
    "src/libsmmod/modapi_0x1.h"
    "src/libsmmod/modapi_0x1_internal.h"
    "src/libsmmod/pd.h"
    "src/libsmmod/pdk.h"
    "src/libsmmod/pdkmap.h"
    "src/libsmmod/pdpi.h"
    "src/libsmmod/syscall.h"
    "src/libsmmod/syscallmap.h")
SET_TARGET_PROPERTIES("libsmmod" PROPERTIES
                      OUTPUT_NAME "smmod"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")
INSTALL(FILES "src/libsmmod/modapi.h"
        DESTINATION "${SM_INSTALL_INCLUDE_DIR}/libsmmod")


################################################################################
# libsmvm
################################################################################

SET(LIBSMVM_DEFINES "SMVM_NO_THREADS")
IF(VM_FAST_BUILD)
  SET(LIBSMVM_DEFINES "${LIBSMVM_DEFINES};SMVM_FAST_BUILD")
ENDIF(VM_FAST_BUILD)
SET(LIBSMVM_SOURCES
    "src/libsmvm/vm_internal_core.c"
    "src/libsmvm/vm_internal_helpers.c")
IF(VM_SOFT_FLOAT)
  SET(LIBSMVM_SOURCES ${LIBSMVM_SOURCES} "src/3rdparty/softfloat/softfloat.c")
  SET(LIBSMVM_DEFINES "${LIBSMVM_DEFINES};SMVM_SOFT_FLOAT")
ENDIF(VM_SOFT_FLOAT)
SET(OTHER_FILES ${OTHER_FILES}
    "src/libsmvm/vm.h"
    "src/libsmvm/vm_internal_helpers.h"
    "src/libsmvm/vm_internal_core.h")
ADD_LIBRARY("libsmvm" SHARED ${LIBSMVM_SOURCES})
SET_TARGET_PROPERTIES("libsmvm" PROPERTIES
                      OUTPUT_NAME "smvm"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1"
                      COMPILE_FLAGS "-frounding-math -ftrapping-math -fsignaling-nans"
                      COMPILE_DEFINITIONS "${LIBSMVM_DEFINES}")
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

SET(VM_FILES "src/vm/lasterror.cpp"
             "src/vm/main.cpp"
             "src/vm/vm/modules.cpp"
             "src/vm/vm/pd.cpp"
             "src/vm/vm/pdi.cpp"
             "src/vm/vm/vm.cpp")
IF(VM_OUT_OF_MEMORY_DEBUG)
  SET(VM_FILES ${VM_FILES} "src/vm/oomd.c")
ENDIF(VM_OUT_OF_MEMORY_DEBUG)
ADD_EXECUTABLE("vm" ${VM_FILES})
SET_TARGET_PROPERTIES("vm" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/"
                      VERSION "0.1.0.0")
SET(OTHER_FILES ${OTHER_FILES}
    "src/vm/lasterror.h"
    "src/vm/oomd.h"
    "src/vm/stringscopedobjectmap.h"
    "src/vm/vm/modules.h"
    "src/vm/vm/pd.h"
    "src/vm/vm/pdi.h"
    "src/vm/vm/pdk.h"
    "src/vm/vm/vm.h")
IF(VM_OUT_OF_MEMORY_DEBUG)
  # This is because of http://gcc.gnu.org/bugzilla/show_bug.cgi?id=44736 :
  SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_FLAGS "-fno-builtin-malloc")
  IF(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
    SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_DEFINITIONS "VM_OUT_OF_MEMORY_DEBUG;VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE")
  ELSE(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
    SET_TARGET_PROPERTIES("vm" PROPERTIES COMPILE_DEFINITIONS "VM_OUT_OF_MEMORY_DEBUG")
  ENDIF(VM_OUT_OF_MEMORY_DEBUG_FAIL_BACKTRACE)
ENDIF(VM_OUT_OF_MEMORY_DEBUG)
TARGET_LINK_LIBRARIES("vm" "dl" "libsmmod" "libsmvmi" "libsmvm")


################################################################################
# libmod_putchar
################################################################################

ADD_LIBRARY("libmod_putchar" SHARED
            "src/mod_putchar/mod_putchar.c")
SET_TARGET_PROPERTIES("libmod_putchar" PROPERTIES
                      OUTPUT_NAME "smvm_mod_putchar"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")


################################################################################
# libmod_pd_getconfig
################################################################################

PREPARE_OTHER_FILES()
ADD_LIBRARY("libmod_pd_getconfig" SHARED
            "src/mod_pd_getconfig/mod_pd_getconfig.c"
               ${PREPARED_OTHER_FILES}) # <-- hack for Qt Creator to include other files
SET_TARGET_PROPERTIES("libmod_pd_getconfig" PROPERTIES
                      OUTPUT_NAME "smvm_mod_pd_getconfig"
                      LIBRARY_OUTPUT_DIRECTORY "lib/"
                      VERSION "0.1.0.0"
                      SOVERSION "0.1")


################################################################################
# Installs:
################################################################################

INSTALL(TARGETS "libsmvmi" "libsme" "libsmas" "libsmvm" "vm" "smas" "libmod_putchar"
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        PUBLIC_HEADER DESTINATION "include")


################################################################################
# Tests:
################################################################################

SET(TESTS_DIR_B "${CMAKE_CURRENT_BINARY_DIR}/tests")
SET(TESTS_DIR_S "${CMAKE_CURRENT_SOURCE_DIR}/tests")
FILE(MAKE_DIRECTORY "${TESTS_DIR_B}")

FUNCTION(add_test_assemble_execute smas_args vm_args testfile suffix)
    ADD_TEST(NAME "${testfile}${suffix}"
             COMMAND "${TESTS_DIR_S}/run_test_assemble_execute.sh"
                 $<TARGET_FILE:smas> "${smas_args}" $<TARGET_FILE:vm> "${vm_args}" "${TESTS_DIR_S}/${testfile}.sa" "${TESTS_DIR_B}/${testfile}.sb" ${ARGN})
    SET_TESTS_PROPERTIES("${testfile}${suffix}" PROPERTIES COST "1.0")
ENDFUNCTION()

FUNCTION(add_test_assemble_execute_regex smas_args vm_args testfile suffix regex)
    add_test_assemble_execute("${smas_args}" "${vm_args}" "${testfile}" "${suffix}" "${ARGN}")
    SET_TESTS_PROPERTIES("${testfile}${suffix}" PROPERTIES PASS_REGULAR_EXPRESSION "${regex}")
ENDFUNCTION()

FUNCTION(add_test_assemble_execute_exception smas_args vm_args testfile suffix exceptions)
    add_test_assemble_execute_regex("${smas_args}" "${vm_args}" "${testfile}" "${suffix}" "Exception (${exceptions})" "${ARGN}")
ENDFUNCTION()


add_test_assemble_execute("" "" "benchmark" "")
SET_TESTS_PROPERTIES("benchmark" PROPERTIES COST "0.0")

add_test_assemble_execute_exception("" "" "except_bmodbyzero_int8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_int16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_int32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_int64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_uint8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_uint16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_uint32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmodbyzero_uint64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_int8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_int16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_int32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_int64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_uint8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_uint16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_uint32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_bmod2byzero_uint64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_float32" "" "SMVM_E_FLOATING_POINT_DIVIDE_BY_ZERO|SMVM_E_UNKNOWN_FPE")
add_test_assemble_execute_exception("" "" "except_divbyzero_int8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_int16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_int32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_int64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_uint8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_uint16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_uint32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divbyzero_uint64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_divoverflow_int64" "" "SMVM_E_INTEGER_OVERFLOW")
add_test_assemble_execute_exception("" "" "except_invalid_float32" "" "SMVM_E_FLOATING_POINT_INVALID_OPERATION|SMVM_E_UNKNOWN_FPE")
add_test_assemble_execute_exception("" "" "except_overflow_float32" "" "SMVM_E_FLOATING_POINT_OVERFLOW|SMVM_E_UNKNOWN_FPE")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_int8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_int16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_int32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_int64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_uint8" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_uint16" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_uint32" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_tmodbyzero_uint64" "" "SMVM_E_INTEGER_DIVIDE_BY_ZERO")
add_test_assemble_execute_exception("" "" "except_underflow_float32" "" "SMVM_E_FLOATING_POINT_UNDERFLOW|SMVM_E_UNKNOWN_FPE")
add_test_assemble_execute_exception("" "" "except_write_to_rodata" "" "SMVM_E_WRITE_DENIED")
add_test_assemble_execute("" "" "alloc_zero" "")
add_test_assemble_execute("" "" "bmod" "")
add_test_assemble_execute("" "" "bmod2" "")
add_test_assemble_execute("" "" "factorial" "")
add_test_assemble_execute("" "" "global_return" "")
add_test_assemble_execute("" "" "mem" "")
add_test_assemble_execute("" "" "proc" "")
add_test_assemble_execute("" "" "section_bss" "")
add_test_assemble_execute("" "" "section_data" "")
add_test_assemble_execute("" "" "section_rodata" "")
add_test_assemble_execute("" "-l $<TARGET_FILE:libmod_putchar>" "syscall_putchar" "_1")
add_test_assemble_execute_regex("" "-l $<TARGET_FILE:libmod_putchar>" "syscall_putchar" "_2" "Tere, Maailm!")
add_test_assemble_execute_regex("" "-l $<TARGET_FILE:libmod_pd_getconfig> -p first:GetConfigPD=FIRST -p second:GetConfigPD=SECOND -p third:GetConfigPD=THIRD" "pd_getconfig" "" "SECOND")
add_test_assemble_execute("" "" "tmod" "")
