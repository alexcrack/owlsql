# SPDX-License-Identifier: GPL-3.0-or-later
# Helpers shared by the driver plugins (linked statically into each).
qt_add_library(owl_driver_common STATIC textvalues.h textvalues.cpp)
target_include_directories(owl_driver_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_driver_common PUBLIC Owl::Core)
set_target_properties(owl_driver_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
owl_set_target_options(owl_driver_common)
