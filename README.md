# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- A small object-oriented UI framework (pages, components, parameters) that
  renders 1 bpp straight into the panel's frame buffer. **No LVGL.**
- Dirty-region partial refreshes and full-screen refreshes.
- WiFi STA connect only (no network services yet).
- The same UI code runs on the panel, in host unit tests, and in a host
  preview that writes PNGs, so UI work does not need the hardware.

## UI framework

```
src/ui/
  types.hpp      Rect/Size/Align math, anchor placement, rect merging
  param.hpp      Param (typed value) + ParamSet + schema validation
  font.hpp       Font/Glyph tables and UTF-8 decoding
  canvas.hpp     1 bpp drawing surface: fill, stroke, text, blit, diff
  component.hpp  Component base class (params, layout, draw, update)
  components.hpp Label, Box, Rule, Container
  page.hpp       Page = a named Container that covers the screen
  app.hpp        UiApp: page stack + dirty-region computation
  theme.hpp      fonts, colours, screen size
  fonts/         generated Cozette tables (16 px, 96 px)
src/screens/
  status_page.hpp/.cpp   the actual screen
```

Everything above `epd.cpp` is platform-free: no ESP-IDF, no LVGL, no globals.
That is why the native test and preview builds can link it unchanged.

### Components and parameters

A component is configured exclusively through parameters, which is what makes
it testable without pixels or hardware:

```cpp
ui::ParamSet params{
    {ui::Label::kText, ui::Param("Success")},
    {ui::Label::kFont, ui::Param(&app::theme::headline_font())},
    {ui::Component::kAnchor, ui::Param(ui::Align::Center)},
    {ui::Component::kY, ui::Param(-30)},
};
page.add<ui::Label>("headline", params);
```

Common geometry parameters (accepted by every component):

| Parameter | Type | Meaning |
| --- | --- | --- |
| `x`, `y` | int | offset after anchoring |
| `w`, `h` | int | explicit size; omitted/0 means "measure the content" |
| `anchor` | Align | `top-left` … `bottom-right`, `center` (default `top-left`) |
| `visible` | bool | hidden components are neither laid out nor drawn |

Component-specific parameters, e.g. `Label`: `text`, `font`, `color`,
`halign`, `valign`; `Box`: `color` (omit for border-only), `border`,
`border_color`; `Container`/`Page`: `bg`, `border`, `border_color`.

`Component::schema()` declares the accepted parameters, so `validate()` reports
unknown keys, missing required keys, wrong types and out-of-range integers.
Both are covered by tests.

### Layout

`Component::layout(parent)` resolves `x/y/w/h/anchor` against the parent rect.
When `w`/`h` are not given, `measure()` supplies the content size — a `Label`
measures its text with the font that will draw it, so text is never clipped by
a hard-coded box. Children are laid out inside their parent's bounds.

### Pages and the app

`Page` is a named `Container` pinned to the screen. `UiApp` owns the pages:

```cpp
ui::UiApp app(648, 480);
app.add_page(std::make_unique<app::StatusPage>(wifi_source));
app.tick(now_ms);                    // time-based updates
auto dirty = app.present(canvas);    // draw + diff against the last frame
```

`present()` compares the freshly drawn frame with the previous one and returns
the regions that changed:

- leaf components are diffed per component bounds (a label that shrank still
  reports the area it used to cover),
- containers participate through a parameter *generation* counter, so changing
  a background is reported even though no child changed,
- a page switch or `invalidate()` reports the whole screen.

The device then issues one partial refresh per region, and falls back to a
full-screen refresh when a region is large or a partial update times out.

### Adding a screen

```cpp
class ClockPage : public ui::Page {
public:
  ClockPage() : ui::Page("clock") {
    add<ui::Label>("time", ui::ParamSet{
        {ui::Label::kText, ui::Param("12:00")},
        {ui::Label::kFont, ui::Param(&ui::fonts::kCozette96)},
        {ui::Component::kAnchor, ui::Param(ui::Align::Center)},
    });
  }
};
```

Then `app.add_page(std::make_unique<ClockPage>())` and
`app.show_page("clock")`. The next `present()` reports the whole screen and the
device does a full refresh.

## Testing without the panel

```sh
pio test -e native      # 49 Unity tests, no hardware
tools/preview.sh        # renders the real pages to PNG + ASCII
```

The suite covers geometry/anchor math, rect merging, parameter typing and
schema validation, UTF-8 (including `Æ Ø Å æ ø å`), the 1 bpp canvas (unaligned
fills, clipping, text alignment, hashing, region diffing), component layout and
measurement, page/app navigation, dirty-region computation, and the real status
page (counter ticks, heartbeat, WiFi text, exact dirty rects).

`test_status_page` also pins a golden hash of the boot frame. When a change to
the UI is intentional, refresh it with:

```sh
UPDATE_GOLDEN=1 pio test -e native -f test_status_page -v
```

`tools/preview.sh` builds `src/preview/preview_main.cpp` for the host, prints
the component tree and an ASCII frame, and writes `preview-*.png` for the boot
state, the counter ticking, the WiFi label and the "full" mode label.

## Fonts

`src/ui/fonts/` holds `lv_font_conv`-free, generated tables: 1 bpp bitmaps plus
per-glyph advance/offset, produced from `CozetteVector-nerd.ttf` with printable
ASCII and `Æ Ø Å æ ø å`.

```sh
python3 tools/gen_fonts.py ../CozetteVector-nerd.ttf src/ui/fonts
```

New files must be listed in `src/CMakeLists.txt` (the ESP-IDF component lists
its sources explicitly).

**Size matters on this panel:** the full waveform only turns ~4 px strokes
fully black, so 1-2 px stems read grey. The headline uses 96 px Cozette
(4 px stems); the 16 px labels are deliberately light.

## Wiring

This is the mapping that works on the bench, identical to the
`sample/working_half` Arduino sketch.

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
control, not the supply; the proven sketch drives it high. SPI runs at **2 MHz**
mode 0.

### Two things that are easy to get wrong

1. **RST low time must be a real millisecond.** `vTaskDelay(pdMS_TO_TICKS(1))`
   rounds to zero ticks at the default 100 Hz tick, so `delay_ms(1)` produces a
   ~0 us reset pulse. The UC8179 ignores that, never resets, keeps BUSY
   asserted and drops every command — the panel looks completely dead while
   SPI happily clocks data out. `epd.cpp` therefore uses
   `esp_rom_delay_us(1000)`.
2. **BUSY is polled with a `0x71` status read**, matching the working sketch:
   `send 0x71; read BUSY; repeat`.

## Panel init

Full mode (working_half sequence plus Seeed's UC8179 drive voltages):

```
0x01 07 07 3F 3F        VGH/VGL/VDH/VDL
0x06 17 17 28 17        booster soft start
0x00 1F                 panel setting
0x04                    power on, wait BUSY
0x50 21 07              VCOM / data interval (full waveform)
```

Partial mode: `0x00 1F; 0x04 (wait); 0x50 29 07; 0xE0 02; 0xE5 58`.

Update protocol: window with `0x50 A9 07`, `0x91`, `0x90 ... 01`, write new
data to `0x13`, `0x12`, wait, then write the same data to `0x10` as the "old"
image. Plane polarity is `0x00 = black`, `0xFF = white`. A full refresh writes
`0x10 = 0xFF`, `0x13 = frame`, then `0x12`.

## Build and flash

```sh
pio run -e xiao
```

The build is configured for the XIAO's 8 MB flash / 8 MB PSRAM.

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
The boot log prints the component tree, the frame hash and every dirty region,
which is usually enough to debug a layout change remotely.

## What the display shows

- "Success" at 96 px Cozette, white background / black text.
- Counter updated through a partial refresh every 2 s.
- A 36 x 36 heartbeat square toggling every 7 s.
- WiFi status (`wifi: <ssid> (<ip>)`) in the bottom-left.
- A full-screen full-mode refresh every 120 s to clear partial-update ghosting.
