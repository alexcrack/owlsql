# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Test)

# owl_add_unit_test(<name> SOURCES ... [LIBS ...])
function(owl_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    owl_set_target_options(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_subdirectory(unit)
