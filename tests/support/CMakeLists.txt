# SPDX-License-Identifier: GPL-3.0-or-later
# Test support: the scriptable fake driver, as a static library and as plugin files.

qt_add_library(owl_test_fakedriver STATIC fakedriver.h fakedriver.cpp)
target_include_directories(owl_test_fakedriver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_test_fakedriver PUBLIC Owl::Core)
set_target_properties(owl_test_fakedriver PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(OWL_TEST_DRIVER_DIR "${CMAKE_BINARY_DIR}/test-drivers" CACHE INTERNAL "")

# owl_add_fake_plugin(<target> <source>): the fake driver as a loadable plugin module.
function(owl_add_fake_plugin target source)
    qt_add_library(${target} MODULE ${source})
    target_link_libraries(${target} PRIVATE owl_test_fakedriver Qt6::Core)
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${OWL_TEST_DRIVER_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${OWL_TEST_DRIVER_DIR}")
endfunction()

owl_add_fake_plugin(owl_fake_driver_plugin fakeplugin.cpp)
owl_add_fake_plugin(owl_fake_driver_plugin_badapi fakeplugin_badapi.cpp)

# PostgreSQL test server settings from the environment (header-only).
add_library(owl_test_pgserver INTERFACE pgtestserver.h)
target_include_directories(owl_test_pgserver INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_test_pgserver INTERFACE Owl::Core)
