# OwlSql

A fast, modern database client for **PostgreSQL**, **MySQL / MariaDB** and **SQLite** —
HeidiSQL's speed and directness with pgAdmin's depth. Built with Qt 6 and QML.

> **Status:** early development (milestone M0 — foundation). Not usable yet.

## Building

Requirements: Qt **6.12** (Quick, Quick Controls 2), CMake ≥ 3.25, Ninja, a C++20 compiler
(GCC 13+, Clang 17+, MSVC 2022).

```sh
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

Dependencies beyond Qt are managed with a **vcpkg** manifest (`vcpkg.json`); the build uses vcpkg
automatically when `VCPKG_ROOT` is set. (M0 needs none.)

## Development

```sh
tools/setup-dev.sh   # enables the pre-commit format check
tools/format.sh      # formats all C++ (clang-format) and QML (qmlformat) sources
```

- Commits follow [Conventional Commits](https://www.conventionalcommits.org/); one PR per feature.
- Design and planning documents live in [`docs/`](docs/) — start with
  [`vision.md`](docs/vision.md), [`architecture.md`](docs/architecture.md) and
  [`roadmap.md`](docs/roadmap.md).

## License

OwlSql is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version. See [`LICENSE`](LICENSE).
