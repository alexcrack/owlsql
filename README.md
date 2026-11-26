# OwlSql

[![CI](https://github.com/alexcrack/owlsql/actions/workflows/ci.yml/badge.svg)](https://github.com/alexcrack/owlsql/actions/workflows/ci.yml)

A fast, modern database client for **PostgreSQL**, **MySQL / MariaDB** and **SQLite** —
HeidiSQL's speed and directness with pgAdmin's depth. Built with Qt 6 and QML.

> **Status:** early development (milestone M0 — foundation). Not usable yet.

## Building

Requirements: Qt **6.12** (Quick, Quick Controls 2), CMake ≥ 3.30, Ninja, a C++20 compiler
(GCC 13+, Clang 17+, MSVC 2022), Git, Perl, and **vcpkg**. On Linux also `libsecret-1-dev`
(without it passwords can only be stored in KWallet).

Dependencies (decision 124):
- **libpq, sqlite3** come from the vcpkg manifest (`vcpkg.json`, pinned `builtin-baseline`).
- **QtKeychain, KSyntaxHighlighting** (and KDE's ECM) are downloaded at pinned tags and built
  against your Qt during the **first configure**, which therefore takes a few minutes; later
  configures reuse them (`<build>/_owl_deps`, or `$OWL_DEPS_DIR`).

```sh
# One-time: vcpkg (any checkout that contains the baseline commit in vcpkg.json)
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg

# Point CMake at Qt (or create a CMakeUserPresets.json, see below)
export QT_ROOT_DIR=~/Qt/6.12.0/gcc_64

cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/src/app/owlsql
```

`CMakeUserPresets.json` (git-ignored) is the easiest way to keep your local Qt path:

```json
{
  "version": 6,
  "configurePresets": [{
    "name": "dev", "inherits": "debug",
    "cacheVariables": { "CMAKE_PREFIX_PATH": "/home/you/Qt/6.12.0/gcc_64" }
  }],
  "buildPresets": [{ "name": "dev", "configurePreset": "dev" }],
  "testPresets": [{ "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } }]
}
```

Qt Creator: open `CMakeLists.txt` and pick a preset or your Qt 6.12 kit.

### Integration tests

Tests against real servers skip themselves unless a server URL is set (CI runs them on Linux):

```sh
docker run -d --rm --name owl-pg -e POSTGRES_PASSWORD=owlsql -p 55432:5432 postgres:18
OWL_TEST_PG_URL=postgresql://postgres:owlsql@localhost:55432/postgres ctest --preset debug -L integration
docker stop owl-pg
```

## Development

```sh
tools/setup-dev.sh   # enables the pre-commit format check
tools/format.sh      # formats all C++ (clang-format) and QML (qmlformat) sources

./build/debug/src/owlsql --gallery                    # component gallery
./build/debug/src/owlsql --data-dir /tmp/owl --new-instance   # throwaway settings and data
cmake --build --preset debug --target update_translations      # refresh i18n/*.ts (lupdate)
```

- Commits follow [Conventional Commits](https://www.conventionalcommits.org/); one PR per feature.
- Design and planning documents live in [`docs/`](docs/) — start with
  [`vision.md`](docs/vision.md), [`architecture.md`](docs/architecture.md) and
  [`roadmap.md`](docs/roadmap.md).

## License

OwlSql is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version. See [`LICENSE`](LICENSE).
