# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- LVGL 9.5 in 1-bit `I1` mode.
- Partial (dirty-window) updates and full-screen refreshes.
- WiFi STA connect only (no network services yet).

## Wiring

| Panel | ESP32-S3 |
| --- | --- |
| DIN / MOSI | GPIO9 |
| CLK / SCK | GPIO7 |
| CS | GPIO2 |
| DC | GPIO3 |
| RST | GPIO4 |
| BUSY | GPIO5 |
| PWR | GPIO6 |

## Build

```sh
pio run
```

The build is configured for the XIAO's 8 MB flash / 8 MB PSRAM.

## Flash (bench)

The board is on `botmain` at `/dev/ttyACM0` (USB-JTAG). `pyserial` is available
there through Nix:

```sh
# first flash / partition change
nix-shell -p python3Packages.pyserial --run \
  "python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
   --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
   write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin"

# application-only updates
nix-shell -p python3Packages.pyserial --run \
  "python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
   --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
   write_flash 0x10000 firmware.bin"
```

Serial console: `stty -F /dev/ttyACM0 115200 raw -echo; cat /dev/ttyACM0`.

## What the display shows

- Counter updated through LVGL + partial refreshes every 2 s.
- A 36 x 36 tick box toggling every 7 s (a second independent partial update).
- WiFi status (`wifi: <ssid> (<ip>)`) in the bottom-left.
- A full-screen full-mode refresh every 120 s to clear partial-update ghosting.
