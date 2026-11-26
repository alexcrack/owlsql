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

add_subdirectory(support)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(conformance)
