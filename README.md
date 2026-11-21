# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- LVGL 9.5 in 1-bit `I1` mode.
- Partial (dirty-window) updates and full-screen refreshes.
- WiFi STA connect only (no network services yet).

## Wiring

The panel is a Waveshare 5.83" 10-pin e-paper module. The connector order is:

| Pin | Signal | Notes |
| --- | --- | --- |
| 1 | PWR | panel power control (not the 3.3 V supply) |
| 2 | BUSY | active low |
| 3 | RST | active low |
| 4 | DC | command low, data high |
| 5 | CS_S | slave controller select |
| 6 | CS_M | master controller select |
| 7 | SCLK | SPI clock |
| 8 | DIN | SPI MOSI |
| 9 | GND | |
| 10 | VCC | 3.3 V |

**VCC (pin 10) and GND (pin 9) must be connected.** PWR is only a control
signal; it does not power the panel.

### Direct-GPIO mapping used by this firmware

| Panel | ESP32-S3 |
| --- | --- |
| DIN (pin 8) | GPIO9 |
| SCLK (pin 7) | GPIO7 |
| CS_M (pin 6) | GPIO2 |
| DC (pin 4) | GPIO3 |
| RST (pin 3) | GPIO4 |
| BUSY (pin 2) | GPIO5 |
| PWR (pin 1) | GPIO6 |

The earlier bench wiring had **CS on pin 5 (CS_S)** and left **CS_M floating**.
A dual module only refreshes when the master is addressed (and ideally both CS
lines are driven). Move CS to pin 6, or tie pins 5 and 6 together to GPIO2.

The working direct-GPIO half project in the Countr tree
(`arduino/half_display/working_half`) uses `RST=2, DC=3, CS=4, BUSY=1, PWR=5`
with `SPI.begin(7, 8, 9)`. That is a different physical header and is another
valid mapping if the board is wired that way.

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
