#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Formats all C++ and QML sources in place (tracked and new, non-ignored files).
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
QMLFORMAT="${QMLFORMAT:-qmlformat}"

git ls-files -z --cached --others --exclude-standard -- '*.cpp' '*.h' '*.hpp' | xargs -0 -r "$CLANG_FORMAT" -i
git ls-files -z --cached --others --exclude-standard -- '*.qml' | xargs -0 -r -n1 "$QMLFORMAT" -i
echo "Formatted."
