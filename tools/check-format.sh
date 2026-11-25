#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Verifies formatting of all tracked C++ and QML sources without changing them (used by CI).
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
QMLFORMAT="${QMLFORMAT:-qmlformat}"
failed=0

mapfile -t cpp_files < <(git ls-files -- '*.cpp' '*.h' '*.hpp')
if ((${#cpp_files[@]})); then
    "$CLANG_FORMAT" --version
    "$CLANG_FORMAT" --dry-run -Werror "${cpp_files[@]}" || failed=1
fi

mapfile -t qml_files < <(git ls-files -- '*.qml')
for f in "${qml_files[@]}"; do
    if ! diff -u "$f" <("$QMLFORMAT" "$f"); then
        echo "qmlformat: $f is not formatted"
        failed=1
    fi
done

if ((failed)); then
    echo "Formatting issues found — run tools/format.sh"
    exit 1
fi
echo "Formatting OK."
