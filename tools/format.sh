#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Formats all tracked C++ and QML sources in place.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
QMLFORMAT="${QMLFORMAT:-qmlformat}"

git ls-files -z -- '*.cpp' '*.h' '*.hpp' | xargs -0 -r "$CLANG_FORMAT" -i
git ls-files -z -- '*.qml' | xargs -0 -r -n1 "$QMLFORMAT" -i
echo "Formatted."
