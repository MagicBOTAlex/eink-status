# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- LVGL 9.5 in 1-bit `I1` mode.
- Partial (dirty-window) updates and full-screen refreshes.
- WiFi STA connect only (no network services yet).

## Wiring

This is the mapping that works on the bench, identical to the
`sample/working_half` Arduino sketch. It is **not** the Seeed XIAO eInk
Expansion Board pinout, and it is not the old bench guess
(`CS=2, DC=3, RST=4, BUSY=5, PWR=6`), which does not drive the panel.

| HAT / panel pin | ESP32-S3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| DIN / MOSI | GPIO9 (D10) |
| CLK / SCK | GPIO7 (D8) |
| RST | GPIO2 (D1) |
| DC | GPIO3 (D2) |
| CS | GPIO4 (D3) |
| PWR | GPIO5 (D4) |
| BUSY | GPIO1 (D0) |

VCC must be **3.3 V** to match the XIAO logic. PWR is the panel power-enable
control, not the supply; the proven sketch drives it high.

SPI runs at **2 MHz** (mode 0). The panel is rated for 5 MHz, but the proven
sketch uses 2 MHz and this HAT/dupont wiring is not happy above that.

## Two things that are easy to get wrong

1. **RST low time must be a real millisecond.** `vTaskDelay(pdMS_TO_TICKS(1))`
   rounds to zero ticks at the default 100 Hz tick, so `delay_ms(1)` produces a
   ~0 us reset pulse. The UC8179 ignores that, never resets, keeps BUSY
   asserted and drops every command — the panel looks completely dead while
   SPI happily clocks data out. `epd.cpp` therefore uses
   `esp_rom_delay_us(1000)`.
2. **BUSY is polled with a `0x71` status read**, matching the working sketch:
   `send 0x71; read BUSY; repeat`. Polling the pin alone also works, but the
   status read is what the proven code does.

## Panel init

The init is the `sample/working_half` sequence plus Seeed's UC8179 drive
voltages. Extra commands (TRES `0x61`, `0x15`, `0x60`) are deliberately absent.

Full mode:

```
0x01 07 07 3F 3F        VGH/VGL/VDH/VDL
0x06 17 17 28 17        booster soft start
0x00 1F                 panel setting
0x04                    power on, wait BUSY
0x50 21 07              VCOM / data interval (full waveform)
```

Partial mode:

```
0x00 1F
0x04                    power on, wait BUSY
0x50 29 07
0xE0 02                 cascade
0xE5 58                 forced temperature
```

Update protocol (same as `refrence/src/escreen.h`): window with
`0x50 A9 07`, `0x91`, `0x90 ... 01`, write new data to `0x13`, `0x12`,
wait, then write the same data to `0x10` as the "old" image. Plane polarity
is `0x00 = black`, `0xFF = white`.

Full refresh writes `0x10 = 0xFF` (white), `0x13 = frame`, then `0x12`.

## Fonts

`src/fonts/` holds `lv_font_conv` output from `CozetteVector-nerd.ttf` with
printable ASCII plus `Æ Ø Å æ ø å`. Reproduce with:

```sh
npx lv_font_conv --size 16 --bpp 1 --format lvgl --font CozetteVector-nerd.ttf \
  -r 0x20-0x7E -r 0xC5-0xC6 -r 0xD8 -r 0xE5-0xE6 -r 0xF8 --no-compress \
  --lv-include lvgl.h --lv-font-name cozette_vector_16 -o src/fonts/cozette_vector_16.c
```

(96 px for the headline, 16 px for the small labels.)

New font files must be added to `src/CMakeLists.txt` — the component lists its
sources explicitly.

**Size matters on this panel:** the full waveform only turns 1-2 px glyph stems
fully black, so they read as grey. 96 px Cozette has ~4 px stems and is solid
black; 16 px labels stay deliberately light.

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

- "Success" at 96 px Cozette, white background / black text.
- Counter updated through LVGL + partial refreshes every 2 s.
- A 36 x 36 tick box toggling every 7 s (a second independent partial update).
- WiFi status (`wifi: <ssid> (<ip>)`) in the bottom-left.
- A full-screen full-mode refresh every 120 s to clear partial-update ghosting.
