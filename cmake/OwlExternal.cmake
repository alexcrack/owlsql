# SPDX-License-Identifier: GPL-3.0-or-later
#
# owl_add_external(<name> GIT_REPOSITORY <url> GIT_TAG <tag> [CMAKE_ARGS <args>...])
#
# Downloads a CMake project at a pinned tag (FetchContent) and builds + installs it at configure
# time into ${OWL_DEPS_DIR}/install, in its own CMake process. Used for Qt-based libraries that
# vcpkg cannot provide against our Qt (decision 124). Building in isolation keeps the dependency's
# global settings (KDE's compiler flags, git hooks, install rules) out of our project.
#
# The result is reused while a stamp file matches the tag, arguments, compiler, build type and Qt
# location; CI caches ${OWL_DEPS_DIR}/install. find_package() then finds the installed package.

include_guard(GLOBAL)
include(FetchContent)

if(DEFINED ENV{OWL_DEPS_DIR})
    set(owl_default_deps_dir "$ENV{OWL_DEPS_DIR}") # CI: a cached directory shared by jobs
else()
    set(owl_default_deps_dir "${CMAKE_BINARY_DIR}/_owl_deps")
endif()
set(OWL_DEPS_DIR "${owl_default_deps_dir}" CACHE PATH
    "Where Qt-based third-party libraries are built and installed")
set(OWL_DEPS_PREFIX "${OWL_DEPS_DIR}/install")
list(PREPEND CMAKE_PREFIX_PATH "${OWL_DEPS_PREFIX}")

function(owl_add_external name)
    cmake_parse_arguments(ARG "" "GIT_REPOSITORY;GIT_TAG" "CMAKE_ARGS" ${ARGN})

    # Build these exactly like our own code: same generator, compiler, build type and Qt.
    set(args
        -G "${CMAKE_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_INSTALL_PREFIX=${OWL_DEPS_PREFIX}"
        # Our Qt must come first in the default search: KDE's CMake helpers call
        # find_package(Qt6Core) without hints and would otherwise pick up a distribution Qt.
        # (Other dependencies are passed explicitly, e.g. ECM_DIR.)
        "-DCMAKE_PREFIX_PATH=${QT6_INSTALL_PREFIX}"
        "-DQt6_DIR=${Qt6_DIR}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTING=OFF
        ${ARG_CMAKE_ARGS})
    foreach(var CMAKE_MAKE_PROGRAM CMAKE_C_COMPILER CMAKE_OSX_ARCHITECTURES
                CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_MSVC_RUNTIME_LIBRARY)
        if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
            list(APPEND args "-D${var}=${${var}}")
        endif()
    endforeach()

    string(SHA256 fingerprint "${ARG_GIT_REPOSITORY}|${ARG_GIT_TAG}|${args}|${CMAKE_CXX_COMPILER_VERSION}")
    set(stamp "${OWL_DEPS_PREFIX}/.owl-${name}.stamp")
    if(EXISTS "${stamp}")
        file(READ "${stamp}" previous)
        if(previous STREQUAL fingerprint)
            message(STATUS "OwlSql dependency ${name} ${ARG_GIT_TAG}: up to date")
            return()
        endif()
    endif()

    message(STATUS "OwlSql dependency ${name} ${ARG_GIT_TAG}: downloading and building...")
    FetchContent_Declare(owl_ext_${name}
        GIT_REPOSITORY "${ARG_GIT_REPOSITORY}"
        GIT_TAG "${ARG_GIT_TAG}"
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR "__owl_do_not_add_subdirectory__") # download only
    FetchContent_MakeAvailable(owl_ext_${name})
    string(TOLOWER "owl_ext_${name}" lower)

    set(build_dir "${OWL_DEPS_DIR}/build/${name}")
    set(log "${OWL_DEPS_DIR}/${name}.log")
    file(REMOVE_RECURSE "${build_dir}")
    foreach(step configure build install)
        if(step STREQUAL "configure")
            set(command "${CMAKE_COMMAND}" -S "${${lower}_SOURCE_DIR}" -B "${build_dir}" ${args})
        elseif(step STREQUAL "build")
            set(command "${CMAKE_COMMAND}" --build "${build_dir}" --parallel)
        else()
            set(command "${CMAKE_COMMAND}" --install "${build_dir}")
        endif()
        execute_process(COMMAND ${command}
            RESULT_VARIABLE result
            OUTPUT_FILE "${log}.${step}" ERROR_FILE "${log}.${step}")
        if(NOT result EQUAL 0)
            file(READ "${log}.${step}" output)
            message(FATAL_ERROR "Building dependency ${name} failed at '${step}':\n${output}")
        endif()
    endforeach()

    file(WRITE "${stamp}" "${fingerprint}")
    message(STATUS "OwlSql dependency ${name} ${ARG_GIT_TAG}: installed into ${OWL_DEPS_PREFIX}")
endfunction()
