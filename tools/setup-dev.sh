#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# One-time setup of a development checkout.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

git config core.hooksPath tools/git-hooks
echo "Git hooks enabled (tools/git-hooks)."

if ! command -v qmlformat >/dev/null && [[ -z "${QMLFORMAT:-}" ]]; then
    echo "Note: qmlformat is not on PATH. Export QMLFORMAT=<Qt>/bin/qmlformat for the QML check."
fi
