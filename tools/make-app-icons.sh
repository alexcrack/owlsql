#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Regenerates the OwlSql app icons (decision 175) from the SVG sources in resources/logo/:
#   resources/appicon/png/owlsql-<size>.png   window icon, Linux hicolor icons
#   resources/appicon/owlsql.ico              Windows executable icon
#   resources/appicon/owlsql.icns             macOS bundle icon
# 16 and 24 px come from the hand-tuned owlsql-app-small.svg, larger sizes from owlsql-app.svg.
# Needs Inkscape and Python 3 with Pillow. Run from anywhere; commit the results.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
logo="$root/resources/logo"
out="$root/resources/appicon"
mkdir -p "$out/png"

for size in 16 24 32 48 64 128 256 512 1024; do
    source="$logo/owlsql-app.svg"
    [ "$size" -le 24 ] && source="$logo/owlsql-app-small.svg"
    inkscape "$source" --export-width="$size" --export-height="$size" \
        --export-filename="$out/png/owlsql-$size.png" >/dev/null 2>&1
done

python3 - "$out" <<'PY'
import sys
from pathlib import Path
from PIL import Image

out = Path(sys.argv[1])
png = {s: Image.open(out / "png" / f"owlsql-{s}.png").convert("RGBA")
       for s in (16, 24, 32, 48, 64, 128, 256, 512, 1024)}

# Windows: the exact images for every size (no downscaling of the hand-tuned small ones).
ico_sizes = [16, 24, 32, 48, 64, 128, 256]
png[256].save(out / "owlsql.ico", sizes=[(s, s) for s in ico_sizes],
              append_images=[png[s] for s in ico_sizes if s != 256])

# macOS: 16 … 1024 (the @2x variants reuse the next size up).
icns_sizes = [16, 32, 64, 128, 256, 512]
png[1024].save(out / "owlsql.icns", append_images=[png[s] for s in icns_sizes])
PY

# The 1024 px image is only an input for the .icns.
rm -f "$out/png/owlsql-1024.png"
echo "App icons written to $out"
