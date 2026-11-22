#!/usr/bin/env python3
"""Chunked, resumable BLE OTA uploader for the eink-status panel.

Requires bleak (pip install bleak, or nix-shell -p python3Packages.bleak).
The device advertises as "eink-status-ota" and uses the same resumable session
as the HTTP uploader: disconnect at 40%, reconnect, run this again, and it
continues from the device's stored offset.

GATT protocol
-------------
control characteristic (write):
  0x01  u32 total (little endian) + u8 flags (bit0 restart)
  0x02  ASCII hex sha256, appended in <=20 byte writes
  0x03  commit: begin or resume with the accumulated size and hash
  0x04  finish: verify sha256 + ESP image, select for next boot
  0x05  abort
  0x06  ask for a status notification
  0x07  reboot (only after a verified image)
data characteristic (write / write without response):
  raw image bytes, appended at the current offset. The server rejects a chunk
  whose implicit offset does not match, so a reconnect always resyncs.
status characteristic (read + notify):
  read: JSON with state/offset/total/sha256/partition/error
  notify: 9 bytes [state, offset u32 LE, total u32 LE]
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import struct
import sys
import time
from pathlib import Path

SERVICE_UUID = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0001"
CONTROL_UUID = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0002"
DATA_UUID = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0003"
STATUS_UUID = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0004"

STATE_NAMES = ["idle", "active", "verifying", "done", "error"]

OP_BEGIN = 0x01
OP_SHA = 0x02
OP_COMMIT = 0x03
OP_FINISH = 0x04
OP_ABORT = 0x05
OP_STATUS = 0x06
OP_REBOOT = 0x07


def parse_progress(data: bytes) -> dict | None:
  # The characteristic can be read as JSON, but notifications are always this
  # 9-byte frame. Reject anything else instead of treating ASCII as offsets.
  if len(data) != 9:
    return None
  state, offset, total = struct.unpack("<BII", data)
  if state >= len(STATE_NAMES) or total > 32 * 1024 * 1024 or offset > total:
    return None
  return {"state": STATE_NAMES[state], "offset": offset, "total": total}


class Session:
  def __init__(self, client, data: bytes, sha: str, restart: bool, chunk: int):
    self.client = client
    self.data = data
    self.sha = sha
    self.restart = restart
    self.chunk = chunk
    self.latest: dict | None = None
    self.notifying = False
    self.already_done = False
    self.rebooted = False

  async def read_status(self, strict: bool = False) -> dict | None:
    try:
      raw = await self.client.read_gatt_char(STATUS_UUID)
      return json.loads(bytes(raw).decode("utf-8"))
    except Exception as error:  # noqa: BLE001 - short reads are expected
      # A short ATT MTU can truncate the JSON read; the 9-byte notification
      # still carries the offset. Never use that shortcut to decide whether
      # an image is already complete, because it has no sha256.
      if not strict and self.latest is not None:
        return self.latest
      raise RuntimeError(f"could not read BLE status: {error}") from error

  def on_notify(self, _characteristic, data) -> None:
    parsed = parse_progress(bytes(data))
    if parsed is not None:
      self.latest = parsed

  async def control(self, payload: bytes) -> None:
    await self.client.write_gatt_char(CONTROL_UUID, payload, response=True)

  async def wait_for(self, predicate, timeout: float, what: str) -> dict:
    deadline = time.monotonic() + timeout
    last: dict | None = None
    while time.monotonic() < deadline:
      if self.latest is not None:
        last = self.latest
        if predicate(last):
          return last
      status = await self.read_status()
      if status is not None:
        last = status
        if predicate(status):
          return status
      await asyncio.sleep(0.15)
    raise RuntimeError(f"timed out waiting for {what}; last state {last}")

  async def begin_or_resume(self) -> int:
    status = await self.read_status(strict=True)
    if status is None:
      raise RuntimeError("device did not return OTA status")

    if (status.get("state") == "done" and
        int(status.get("total", 0)) == len(self.data) and
        status.get("sha256") == self.sha):
      self.already_done = True
      return len(self.data)

    same = (status.get("state") == "active" and
            int(status.get("total", 0)) == len(self.data) and
            status.get("sha256") == self.sha and
            int(status.get("offset", 0)) > 0)
    if same and not self.restart:
      offset = int(status["offset"])
      print(f"resuming at {offset} bytes "
            f"({100 * offset / len(self.data):.1f}%)")
      return offset

    print(f"preparing {len(self.data)} bytes, sha {self.sha[:12]}…")
    flags = 1 if self.restart else 0
    await self.control(bytes([OP_BEGIN]) + struct.pack("<I", len(self.data)) +
                       bytes([flags]))
    for at in range(0, 64, 16):
      await self.control(bytes([OP_SHA]) + self.sha[at:at + 16].encode("ascii"))
    await self.control(bytes([OP_COMMIT]))
    status = await self.wait_for(lambda s: s.get("state") == "active", 10,
                                 "the upload to begin")
    offset = int(status.get("offset", 0))
    if offset:
      print(f"resuming at {offset} bytes "
            f"({100 * offset / len(self.data):.1f}%)")
    else:
      print("starting at 0")
    return offset

  async def upload(self) -> None:
    try:
      await self.client.start_notify(STATUS_UUID, self.on_notify)
      self.notifying = True
    except Exception as error:  # noqa: BLE001 - notifications are optional
      print(f"note: notifications unavailable ({error}); polling status")

    offset = await self.begin_or_resume()
    if self.already_done:
      print(f"device already has this image ({status.get('partition')})")
      if self.args.reboot:
        self.rebooted = True
        try:
          await self.control(bytes([OP_REBOOT]))
        except Exception:  # noqa: BLE001 - the link drops during reboot
          pass
        print("reboot requested over BLE")
      return
    total = len(self.data)
    started = time.monotonic()
    last_sync = offset

    next_report = 0.0
    while offset < total:
      if offset > total:
        raise RuntimeError(f"device offset {offset} is past the image end")
      end = min(offset + self.chunk, total)
      await self.client.write_gatt_char(DATA_UUID, self.data[offset:end],
                                        response=False)
      offset = end

      if offset - last_sync >= 8192 or offset == total:
        status = await self.wait_for(
            lambda s: int(s.get("offset", -1)) >= offset - self.chunk,
            8, "the device to acknowledge the chunk")
        acked = int(status.get("offset", 0))
        if acked > total:
          raise RuntimeError(f"device reported offset {acked} past {total}")
        if acked < offset - self.chunk:
          raise RuntimeError(f"device stopped at {acked}, sent {offset}")
        offset = acked
        last_sync = offset

      now = time.monotonic()
      if now >= next_report or offset == total:
        next_report = now + 0.5
        pct = 100.0 * offset / total
        rate = offset / max(0.001, now - started) / 1024.0
        sys.stdout.write(f"\r{pct:5.1f}%  {offset:>8} / {total} bytes  "
                         f"{rate:6.1f} KiB/s")
        sys.stdout.flush()

    if offset != total:
      raise RuntimeError(f"finish at {offset}, expected {total}")
    sys.stdout.write("\n")

    await self.control(bytes([OP_FINISH]))
    status = await self.wait_for(
        lambda s: s.get("state") in ("done", "error"), 30,
        "verification")
    if status.get("state") != "done":
      raise RuntimeError(f"device rejected the image: {status.get('error')}")
    # The progress frame has no partition field; read the JSON for the label.
    try:
      detailed = await self.read_status(strict=True)
      if detailed is not None:
        status = detailed
    except Exception:  # noqa: BLE001 - the image is still verified
      pass

    print(f"verified; next boot uses {status.get('partition')}")
    if self.args.reboot:
      # The device calls esp_restart() inside the GATT write, so the ATT
      # response is expected to fail (or never arrive). That is success.
      self.rebooted = True
      try:
        await self.control(bytes([OP_REBOOT]))
      except Exception:  # noqa: BLE001 - the link drops during reboot
        pass
      print("reboot requested over BLE")
      await asyncio.sleep(0.5)


async def run_once(device, args, data: bytes, sha: str) -> None:
  from bleak import BleakClient

  client = BleakClient(device, timeout=args.timeout)
  await client.connect()
  session = None
  try:
    # BlueZ negotiates the MTU but bleak only reads it when asked. Without
    # this the tool would fall back to 20-byte chunks.
    try:
      acquire = getattr(client, "_acquire_mtu", None)
      if acquire is None:
        acquire = getattr(getattr(client, "_backend", None), "_acquire_mtu", None)
      if acquire is not None:
        await acquire()
    except Exception as error:  # noqa: BLE001 - MTU query is best effort
      print(f"note: could not query MTU ({error})")
    try:
      mtu = client.mtu_size
    except Exception:  # noqa: BLE001 - older bleak / platform backends
      mtu = 23
    payload = max(20, min(args.chunk or 512, mtu - 3, 509))
    print(f"connected; MTU {mtu}, {payload} byte chunks")
    if mtu < 64:
      print("warning: small ATT MTU makes the upload slow; the device asks "
            "for 512 but the host must negotiate it")

    session = Session(client, data, sha, args.restart, payload)
    session.args = args
    await session.upload()
    await asyncio.sleep(0.3)
  finally:
    if session is None or not session.rebooted:
      try:
        await client.disconnect()
      except Exception:  # noqa: BLE001 - already gone
        pass


async def main_async(args) -> int:
  try:
    from bleak import BleakScanner
  except ImportError:
    print("bleak is not installed: pip install bleak "
          "(or nix-shell -p python3Packages.bleak)", file=sys.stderr)
    return 2

  data = args.firmware.read_bytes()
  sha = hashlib.sha256(data).hexdigest()

  device = args.address
  if not device:
    print(f"scanning for {args.name}…")
    device = await BleakScanner.find_device_by_name(args.name,
                                                    timeout=args.scan_timeout)
  if not device:
    print(f"no BLE device named {args.name}; is the panel powered?",
          file=sys.stderr)
    return 1

  failures = 0
  while True:
    try:
      await run_once(device, args, data, sha)
      return 0
    except Exception as error:  # noqa: BLE001 - reconnect on any BLE failure
      failures += 1
      if failures > args.retries:
        print(f"\nBLE upload failed: {error}", file=sys.stderr)
        return 1
      print(f"\nconnection failed at attempt {failures}: {error}; "
            f"reconnecting (the device kept its offset)…")
      await asyncio.sleep(2)


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__,
                                   formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument("firmware", type=Path)
  parser.add_argument("--name", default="eink-status-ota")
  parser.add_argument("--address", default=None,
                      help="BLE address; skips scanning")
  parser.add_argument("--chunk", type=int, default=0,
                      help="chunk size (default: MTU-3, max 509)")
  parser.add_argument("--restart", action="store_true",
                      help="discard any matching partial upload and start at 0")
  parser.add_argument("--reboot", action="store_true",
                      help="reboot over BLE after verification")
  parser.add_argument("--scan-timeout", type=float, default=12.0)
  parser.add_argument("--timeout", type=float, default=20.0,
                      help="BLE connect timeout in seconds")
  parser.add_argument("--retries", type=int, default=5)
  args = parser.parse_args()
  try:
    return asyncio.run(main_async(args))
  except KeyboardInterrupt:
    print("\ninterrupted; the device keeps the offset for the next run")
    return 130


if __name__ == "__main__":
  raise SystemExit(main())
