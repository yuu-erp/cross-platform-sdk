cmake_minimum_required(VERSION 3.24)
# Verifies the shared library exports exactly the symbols in expected_exports.txt (ADR-0002).
# Usage: cmake -DTOOL=<nm|dumpbin path> -DKIND=<elf|macho|pe> -DLIB=<lib> -DEXPECTED=<txt>
#              -P check_exports.cmake

if(KIND STREQUAL "elf")
    execute_process(COMMAND ${TOOL} -D --defined-only ${LIB} OUTPUT_VARIABLE out RESULT_VARIABLE rc)
    set(line_re "^[0-9a-fA-F]+ [A-Za-z] (.+)$")
elseif(KIND STREQUAL "macho")
    execute_process(COMMAND ${TOOL} -gU ${LIB} OUTPUT_VARIABLE out RESULT_VARIABLE rc)
    set(line_re "^[0-9a-fA-F]+ [A-Za-z] _(.+)$") # Mach-O adds a leading underscore
elseif(KIND STREQUAL "pe")
    execute_process(COMMAND ${TOOL} /nologo /exports ${LIB} OUTPUT_VARIABLE out RESULT_VARIABLE rc)
    set(line_re "^ +[0-9]+ +[0-9A-Fa-f]+ +[0-9A-Fa-f]+ +([A-Za-z_][A-Za-z0-9_@?$]*)")
else()
    message(FATAL_ERROR "KIND must be elf, macho or pe")
endif()
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${TOOL} failed on ${LIB}")
endif()

set(ignored_elf_symbols _init _fini _edata _end __bss_start)

string(REPLACE "\n" ";" lines "${out}")
set(actual "")
foreach(line IN LISTS lines)
    string(REGEX REPLACE "\r$" "" line "${line}")
    if(line MATCHES "${line_re}")
        set(sym "${CMAKE_MATCH_1}")
        if(NOT (KIND STREQUAL "elf" AND sym IN_LIST ignored_elf_symbols))
            list(APPEND actual "${sym}")
        endif()
    endif()
endforeach()
list(REMOVE_DUPLICATES actual)
list(SORT actual)

file(STRINGS ${EXPECTED} expected REGEX "^[^#]")
list(SORT expected)

if(NOT actual STREQUAL expected)
    set(unexpected ${actual})
    list(REMOVE_ITEM unexpected ${expected})
    set(missing ${expected})
    list(REMOVE_ITEM missing ${actual})
    message(FATAL_ERROR "Export list mismatch for ${LIB}\n"
                        "  unexpected (exported but not listed): ${unexpected}\n"
                        "  missing (listed but not exported, FIAISDK_API forgotten?): ${missing}")
endif()
list(LENGTH actual count)
message(STATUS "Exports OK: ${count} symbols match expected_exports.txt")
