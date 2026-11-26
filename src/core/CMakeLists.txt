# SPDX-License-Identifier: GPL-3.0-or-later
# OwlCore: GUI-free core library (driver API, catalog, dialects...). QtCore only.

configure_file(include/owl/core/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/owl/core/version.h" @ONLY)

qt_add_library(owlcore STATIC
    include/owl/core/version.h.in
    include/owl/core/value.h
    include/owl/core/resultset.h
    include/owl/core/dberror.h
    include/owl/core/result.h
    src/cborutil.h
    src/version.cpp
    src/value.cpp
    src/resultset.cpp
    src/dberror.cpp
)
add_library(Owl::Core ALIAS owlcore)

target_include_directories(owlcore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")

target_link_libraries(owlcore PUBLIC Qt6::Core tl::expected)
owl_set_target_options(owlcore)
