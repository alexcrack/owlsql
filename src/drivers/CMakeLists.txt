# SPDX-License-Identifier: GPL-3.0-or-later
# Database driver plugins (decision 049). Built into <build>/drivers; the app and the conformance
# tests load them from there with DriverRegistry.
set(OWL_DRIVER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/drivers" CACHE INTERNAL "")

# owl_add_driver_plugin(<target> SOURCES ... LIBS ...)
function(owl_add_driver_plugin target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_library(${target} MODULE ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE Owl::Core Qt6::Core ${ARG_LIBS})
    owl_set_target_options(${target})
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${OWL_DRIVER_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${OWL_DRIVER_OUTPUT_DIR}")
endfunction()

add_subdirectory(sqlite)
add_subdirectory(postgres)
