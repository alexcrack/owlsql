# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Test)

# Test strings are not user-facing.
set_directory_properties(PROPERTIES QT_EXCLUDE_FROM_TRANSLATION ON)

# owl_add_unit_test(<name> SOURCES ... [LIBS ...])
function(owl_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    owl_set_target_options(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# owl_add_server_runs(<test> [LABELS ...] RUNS <name>=<URL variable> ...): labels <test> as an
# integration test and registers one more run per extra server; each run reads its URL from the
# named variable (OWL_TEST_URL_VAR, see tests/support/testserver.h).
function(owl_add_server_runs test)
    cmake_parse_arguments(ARG "" "" "LABELS;RUNS" ${ARGN})
    set(labels integration ${ARG_LABELS})
    set_tests_properties(${test} PROPERTIES LABELS "${labels}")
    foreach(run IN LISTS ARG_RUNS)
        string(REPLACE "=" ";" parts "${run}")
        list(GET parts 0 name)
        list(GET parts 1 variable)
        add_test(NAME ${name} COMMAND ${test})
        set_tests_properties(${name} PROPERTIES LABELS "${labels}"
            ENVIRONMENT "OWL_TEST_URL_VAR=${variable}")
    endforeach()
endfunction()

add_subdirectory(support)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(conformance)
