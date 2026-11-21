#!/usr/bin/env python3
"""Verify that every UI source file is listed in src/CMakeLists.txt.

The ESP-IDF component lists its sources explicitly while the host test/preview
builds glob by directory, so a new file can silently be missing from the
device link. Run this after adding a file (and in CI).
"""

from __future__ import annotations

import pathlib
import re
import sys


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    cmake = (root / "src/CMakeLists.txt").read_text()
    listed = set(re.findall(r'"((?:ui|screens)/[^"]+\.cpp)"', cmake))

    found = {
        str(p.relative_to(root / "src"))
        for pattern in ("ui/**/*.cpp", "screens/**/*.cpp")
        for p in (root / "src").glob(pattern)
    }

    missing = sorted(found - listed)
    stale = sorted(listed - found)

    for path in missing:
        print("missing from src/CMakeLists.txt:", path)
    for path in stale:
        print("listed but does not exist:", path)

    if missing or stale:
        return 1
    print("src/CMakeLists.txt lists all %d UI sources" % len(found))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
