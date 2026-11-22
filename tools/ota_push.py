#!/usr/bin/env python3
"""Chunked, resumable HTTP OTA uploader for the eink-status panel.

The device keeps the write offset in NVS after every 8 KiB, so this script can
be interrupted (Ctrl-C, network drop, reboot) and run again: it asks
GET /api/ota where the session stopped and continues from there.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

DEFAULT_HOST = "http://192.168.50.7"
MAX_CHUNK = 16 * 1024


def request(base: str, path: str, data: bytes | None = None,
            headers: dict[str, str] | None = None,
            timeout: float = 120.0) -> tuple[int, dict]:
    req = urllib.request.Request(base + path, data=data,
                                 headers=headers or {},
                                 method="POST" if data is not None else "GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        try:
            return error.code, json.load(error)
        except Exception:
            return error.code, {"ok": False, "error": str(error)}


def status(base: str) -> dict:
    try:
        with urllib.request.urlopen(base + "/api/ota", timeout=15) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        raise SystemExit(f"device returned HTTP {error.code} for /api/ota") from error
    except OSError as error:
        raise SystemExit(f"cannot reach {base}: {error}") from error


def send_chunk(base: str, offset: int, blob: bytes, timeout: float) -> int:
    code, answer = request(base, "/api/ota/chunk", blob,
                           {"X-OTA-Offset": str(offset)}, timeout)
    if code == 200 and answer.get("ok"):
        return int(answer["offset"])
    if code == 409 and isinstance(answer.get("offset"), int):
        return int(answer["offset"])
    raise RuntimeError(f"chunk at {offset} rejected ({code}): "
                       f"{answer.get('error', answer)}")


def progress(done: int, total: int, started: float) -> None:
    pct = 100.0 * done / total if total else 0.0
    elapsed = max(0.001, time.monotonic() - started)
    rate = done / elapsed / 1024.0
    sys.stdout.write(f"\r{pct:5.1f}%  {done:>8} / {total} bytes  "
                     f"{rate:6.1f} KiB/s")
    sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"device base URL (default {DEFAULT_HOST})")
    parser.add_argument("--chunk", type=int, default=MAX_CHUNK,
                        help=f"chunk size in bytes (max {MAX_CHUNK})")
    parser.add_argument("--restart", action="store_true",
                        help="discard any matching partial upload and start at 0")
    parser.add_argument("--reboot", action="store_true",
                        help="reboot after the image is verified")
    parser.add_argument("--timeout", type=float, default=120.0,
                        help="per-request timeout in seconds")
    args = parser.parse_args()

    data = args.firmware.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    chunk = max(1, min(args.chunk, MAX_CHUNK))
    base = args.host.rstrip("/")

    info = status(base)
    if not info.get("available"):
        print("device reports no OTA partition", file=sys.stderr)
        return 1
    if len(data) > int(info.get("capacity", 0)):
        print(f"image is {len(data)} bytes, slot holds {info['capacity']}",
              file=sys.stderr)
        return 1

    if (info.get("state") == "done" and
            int(info.get("total", 0)) == len(data) and
            info.get("sha256") == sha):
        print(f"device already has this image ({info.get('partition')})")
        if args.reboot:
            request(base, "/api/control",
                    json.dumps({"action": "reboot"}).encode(),
                    {"Content-Type": "application/json"}, 15)
            print("reboot requested")
        return 0

    same = (info.get("state") == "active" and
            int(info.get("total", 0)) == len(data) and
            info.get("sha256") == sha and int(info.get("offset", 0)) > 0)
    if same and not args.restart:
        offset = int(info["offset"])
        print(f"resuming at {offset} bytes ({100 * offset / len(data):.1f}%)")
    else:
        code, answer = request(
            base, "/api/ota/begin",
            json.dumps({"size": len(data), "sha256": sha,
                        "restart": args.restart}).encode(),
            {"Content-Type": "application/json"}, args.timeout)
        if code != 200 or not answer.get("ok"):
            print(f"begin rejected ({code}): {answer.get('error', answer)}",
                  file=sys.stderr)
            return 1
        offset = int(answer.get("offset", 0))
        print(f"uploading {args.firmware} ({len(data)} bytes, sha {sha[:12]}…)")

    started = time.monotonic()
    failures = 0
    while offset < len(data):
        end = min(offset + chunk, len(data))
        try:
            acked = send_chunk(base, offset, data[offset:end], args.timeout)
        except (OSError, RuntimeError) as error:
            failures += 1
            if failures > 6:
                print(f"\nlost the upload: {error}", file=sys.stderr)
                return 1
            print(f"\nconnection problem at {offset}: {error}; asking the device…")
            time.sleep(min(5, failures))
            info = status(base)
            if (info.get("state") == "active" and
                    int(info.get("total", 0)) == len(data) and
                    info.get("sha256") == sha):
                offset = int(info.get("offset", 0))
                continue
            print(f"device state is {info.get('state')}; restart the upload",
                  file=sys.stderr)
            return 1

        if acked != end:
            # The device is ahead or behind; trust its offset and continue.
            offset = acked
        else:
            offset = end
            failures = 0
        progress(offset, len(data), started)

    sys.stdout.write("\n")
    code, answer = request(base, "/api/ota/finish", b"",
                           {"Content-Type": "application/json"}, args.timeout)
    if code != 200 or not answer.get("ok"):
        print(f"finish failed ({code}): {answer.get('error', answer)}",
              file=sys.stderr)
        return 1

    print(f"verified; next boot uses {answer.get('partition')}")
    if args.reboot:
        code, answer = request(
            base, "/api/control",
            json.dumps({"action": "reboot"}).encode(),
            {"Content-Type": "application/json"}, 15)
        print("reboot requested" if code == 200 else
              f"reboot rejected: {answer.get('error', answer)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
