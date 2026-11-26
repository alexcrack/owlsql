# SPDX-License-Identifier: GPL-3.0-or-later
# Third-party libraries beyond Qt (decision 124). Versions are pinned here and in vcpkg.json.

# --- C libraries: vcpkg manifest (or system packages) ----------------------------------------
# libpq >= 17 for chunked-rows streaming (specs/driver-api.md §11).
find_package(PostgreSQL REQUIRED)
if(PostgreSQL_VERSION_STRING VERSION_LESS 17)
    message(FATAL_ERROR "libpq >= 17 is required, found ${PostgreSQL_VERSION_STRING}")
endif()
# SQLite >= 3.35 for RETURNING and DROP COLUMN (decision 048).
find_package(SQLite3 3.35 REQUIRED)

# --- Qt-based libraries: pinned source tags, built against our Qt --------------------------------
include(OwlExternal)

# KDE's CMake helpers, needed to build KSyntaxHighlighting. CMake files only.
owl_add_external(ECM
    GIT_REPOSITORY https://github.com/KDE/extra-cmake-modules.git
    GIT_TAG v6.32.0
    CMAKE_ARGS -DBUILD_HTML_DOCS=OFF -DBUILD_MAN_DOCS=OFF -DBUILD_QTHELP_DOCS=OFF)

# Linux: libsecret is how GNOME Keyring / KeePassXC / most desktops store secrets. Release and CI
# builds require it; a local build without libsecret-1-dev falls back to KWallet over D-Bus.
set(owl_keychain_args)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(OWL_REQUIRE_LIBSECRET "Fail if libsecret is not available (Linux)" OFF)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OWL_LIBSECRET QUIET libsecret-1>=0.18.0)
    if(OWL_LIBSECRET_FOUND)
        list(APPEND owl_keychain_args -DLIBSECRET_SUPPORT=ON)
    elseif(OWL_REQUIRE_LIBSECRET)
        message(FATAL_ERROR "libsecret-1 >= 0.18 not found (install libsecret-1-dev)")
    else()
        message(WARNING "libsecret-1 not found: passwords can only be stored in KWallet. "
                        "Install libsecret-1-dev for GNOME Keyring / Secret Service support.")
        list(APPEND owl_keychain_args -DLIBSECRET_SUPPORT=OFF)
    endif()
endif()

owl_add_external(QtKeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG 0.18.0
    CMAKE_ARGS -DBUILD_WITH_QT5=OFF -DBUILD_TRANSLATIONS=OFF -DBUILD_TEST_APPLICATION=OFF
               -DBUILD_QTQUICK_DEMO=OFF ${owl_keychain_args})

owl_add_external(KSyntaxHighlighting
    GIT_REPOSITORY https://github.com/KDE/syntax-highlighting.git
    GIT_TAG v6.32.0
    # Syntax definitions are compiled into the library as resources; no QML plugin needed.
    CMAKE_ARGS -DQRC_SYNTAX=ON -DBUILD_QCH=OFF -DBUILD_EXAMPLES=OFF
               "-DECM_DIR=${OWL_DEPS_PREFIX}/share/ECM/cmake")

find_package(Qt6Keychain CONFIG REQUIRED)
find_package(KF6SyntaxHighlighting CONFIG REQUIRED)
