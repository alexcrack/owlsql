# SPDX-License-Identifier: GPL-3.0-or-later
# OwlCore: GUI-free core library (driver API, catalog, dialects...). QtCore only.

configure_file(include/owl/core/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/owl/core/version.h" @ONLY)

qt_add_library(owlcore STATIC
    include/owl/core/version.h.in
    include/owl/core/value.h
    include/owl/core/resultset.h
    include/owl/core/dberror.h
    include/owl/core/result.h
    include/owl/core/driver.h
    include/owl/core/catalog.h
    include/owl/core/connectionform.h
    include/owl/core/formbuilder.h
    include/owl/core/catalogsupport.h
    include/owl/core/catalogcache.h
    include/owl/core/dialect.h
    include/owl/core/sqltokenizer.h
    include/owl/core/driverregistry.h
    include/owl/core/queryjob.h
    include/owl/core/connectionworker.h
    src/cborutil.h
    src/version.cpp
    src/value.cpp
    src/resultset.cpp
    src/dberror.cpp
    src/driverregistry.cpp
    src/catalog.cpp
    src/connectionform.cpp
    src/formbuilder.cpp
    src/catalogsupport.cpp
    src/catalogcache.cpp
    src/dialect.cpp
    src/sqltokenizer.cpp
    src/queryjob.cpp
    src/connectionworker.cpp
)
add_library(Owl::Core ALIAS owlcore)

target_include_directories(owlcore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")

target_link_libraries(owlcore PUBLIC Qt6::Core tl::expected)
owl_set_target_options(owlcore)
