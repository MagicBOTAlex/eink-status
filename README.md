# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- LVGL 9.5 in 1-bit `I1` mode.
- Partial (dirty-window) updates and full-screen refreshes.
- WiFi STA connect only (no network services yet).

## Wiring

This build targets the **9-pin single-panel HAT** (2 power + 7 GPIO). The
10-pin cable/header belongs to the full dual-screen setup and is not used here.

| HAT pin | ESP32-S3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| DIN / MOSI | GPIO9 (D10) |
| CLK / SCK | GPIO7 (D8) |
| CS | GPIO2 (D1) |
| DC | GPIO3 (D2) |
| RST | GPIO4 (D3) |
| BUSY | GPIO5 (D4) |
| PWR | GPIO6 (D5) |

VCC must be **3.3 V** to match the XIAO logic. PWR is the panel power-enable
control, not the supply.

The Seeed XIAO eInk Expansion Board V2 uses a different fixed pinout
(`RST=D0/GPIO1`, `CS=D1/GPIO2`, `BUSY=D2/GPIO3`, `DC=D3/GPIO4`,
`SCK=D8/GPIO7`, `MOSI=D10/GPIO9`) and has no PWR GPIO. The working
direct-GPIO half project in the Countr tree
(`arduino/half_display/working_half`) similarly uses
`RST=2, DC=3, CS=4, BUSY=1, PWR=5` with `SPI.begin(7, 8, 9)`.

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
