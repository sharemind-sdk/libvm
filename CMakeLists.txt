CMAKE_MINIMUM_REQUIRED(VERSION "2.8")
CMAKE_POLICY(SET CMP0015 NEW)
PROJECT("SHAREMIND_VM" "C")

LINK_DIRECTORIES("lib")

SET(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -O2 -ggdb -fstrict-aliasing -Wstrict-aliasing=1")

# m4
SET(M4_DIR "src/m4")
SET(M4_2_H_SOURCES
    "dispatches.m4"
    "instruction_from_code_cases.m4"
    "instruction_from_name_cases.m4"
    "instruction_structs.m4"
    "preprocess_pass1_cases.m4"
    "preprocess_pass2_cases.m4"
    "static_label_structs.m4")
ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR}/instr.m4f"
                   COMMAND m4 ARGS -E -P "instr.m4" "--freeze-state=instr.m4f"
                   WORKING_DIRECTORY "${M4_DIR}"
                   DEPENDS "${M4_DIR}/instr.m4")
ADD_CUSTOM_COMMAND(TARGET "${M4_DIR}/instr.m4f" POST_BUILD
                   COMMAND m4 ARGS -E -P "--reload-state=instr.m4f" "stats.m4"
                   WORKING_DIRECTORY "${M4_DIR}")
FOREACH(_H_SOURCE ${M4_2_H_SOURCES})
  STRING(REGEX REPLACE "\\.m4$" ".h" _H_TARGET ${_H_SOURCE})
  ADD_CUSTOM_COMMAND(OUTPUT "${M4_DIR}/${_H_TARGET}"
                     COMMAND m4 ARGS -E -P "--reload-state=instr.m4f" "${_H_SOURCE}" > "${_H_TARGET}"
                     WORKING_DIRECTORY "${M4_DIR}"
                     DEPENDS "${M4_DIR}/${_H_SOURCE}" "${M4_DIR}/instr.m4f")
ENDFOREACH(_H_SOURCE ${M4_2_H_SOURCES})

# libsmvmi
ADD_LIBRARY("libsmvmi" SHARED "src/libsmvmi/instr.c")
SET_TARGET_PROPERTIES("libsmvmi" PROPERTIES
                      OUTPUT_NAME "smvmi"
                      LIBRARY_OUTPUT_DIRECTORY "lib/")
SET_SOURCE_FILES_PROPERTIES("src/libsmvmi/instr.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR}/instruction_from_code_cases.h;${M4_DIR}/instruction_from_name_cases.h;${M4_DIR}/instruction_structs.h")


# libsme
ADD_LIBRARY("libsme" SHARED
            "src/libsme/libsme.c"
            "src/libsme/libsme_0x0.c")
SET_TARGET_PROPERTIES("libsme" PROPERTIES
                      OUTPUT_NAME "sme"
                      LIBRARY_OUTPUT_DIRECTORY "lib/")

# libsmas
ADD_LIBRARY("libsmas" SHARED
            "src/libsmas/assemble.c"
            "src/libsmas/linker.c"
            "src/libsmas/linkingunits.c"
            "src/libsmas/stdion.c"
            "src/libsmas/tokenizer.c"
            "src/libsmas/tokens.c")
SET_TARGET_PROPERTIES("libsmas" PROPERTIES
                      OUTPUT_NAME "smas"
                      LIBRARY_OUTPUT_DIRECTORY "lib/")
TARGET_LINK_LIBRARIES("libsmas" "libsmvmi" "libsme")

# libsmvm
ADD_LIBRARY("libsmvm" SHARED
            "src/libsmvm/vm_internal_core.c"
            "src/libsmvm/vm_internal_helpers.c")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_core.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR}/static_label_structs.h;${M4_DIR}/dispatches.h")
SET_SOURCE_FILES_PROPERTIES("src/libsmvm/vm_internal_helpers.c" PROPERTIES OBJECT_DEPENDS
                            "${M4_DIR}/preprocess_pass1_cases.h;${M4_DIR}/preprocess_pass2_cases.h")
SET_TARGET_PROPERTIES("libsmvm" PROPERTIES
                      OUTPUT_NAME "smvm"
                      LIBRARY_OUTPUT_DIRECTORY "lib/")
TARGET_LINK_LIBRARIES("libsmvm" "libsmvmi" "libsme")

# smas
ADD_EXECUTABLE("smas" "src/smas/main.c")
SET_TARGET_PROPERTIES("smas" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/")
TARGET_LINK_LIBRARIES("smas" "libsmas")

# vm
ADD_EXECUTABLE("vm" "src/vm/main.c")
SET_TARGET_PROPERTIES("vm" PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "bin/")
TARGET_LINK_LIBRARIES("vm" "libsmvmi" "libsmvm")

# Load any custom configuration:
INCLUDE("config.local" OPTIONAL)
