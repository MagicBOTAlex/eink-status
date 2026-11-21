#!/usr/bin/env bash
# Render the UI on the host: builds the preview env, prints the component tree
# and the ASCII frame, and converts the PGM snapshots to PNG.
#
#   tools/preview.sh
#
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v python3 >/dev/null 2>&1; then
  echo "preview: python3 is required" >&2
  exit 1
fi
if ! python3 -c "import PIL" >/dev/null 2>&1; then
  echo "preview: Pillow is required (pip install pillow)" >&2
  exit 1
fi

rm -f preview-*.pgm preview-*.png

pio run -e preview >/dev/null
.pio/build/preview/program

python3 - <<'PY'
import glob

from PIL import Image

for path in sorted(glob.glob("preview-*.pgm")):
    png = path[:-4] + ".png"
    Image.open(path).convert("L").save(png)
    print("png:", png)
PY
