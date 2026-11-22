# eink-status firmware

Barebones PlatformIO + ESP-IDF firmware for a single Good Display
**GDEY0583T81** 5.83" 648 x 480 e-paper panel on a Seeed XIAO ESP32-S3.

- Direct-GPIO EPD driver (no MCP23X17 expander).
- A small object-oriented UI framework (pages, components, parameters) that
  renders 1 bpp straight into the panel's frame buffer. **No LVGL.**
- Dirty-region partial refreshes and full-screen refreshes.
- Network monitor: HTTP health checks, ICMP pings, up/down streaks, and this
  device's own power/network uptime.
- Configuration web UI on the device (Tailwind v4), stored in NVS.
- The same UI code runs on the panel, in host unit tests, and in a host
  preview that writes PNGs, so UI work does not need the hardware.

## UI framework

```
src/ui/
  types.hpp      Rect/Size/Align math, anchor placement, rect merging
  param.hpp      Param (typed value) + ParamSet + schema validation
  font.hpp       Font/Glyph tables and UTF-8 decoding
  canvas.hpp     1 bpp drawing surface: fill, stroke, text, blit, diff
  rotation.hpp   logical frame -> panel frame (90 degree rotation)
  component.hpp  Component base class (params, layout, draw, update)
  components.hpp Label, Box, Rule, Container
  page.hpp       Page = a named Container that covers the screen
  app.hpp        UiApp: page stack + dirty-region computation
  theme.hpp      fonts, colours, logical screen size
  fonts/         generated Cozette tables (16/24/32/96 px)
src/screens/
  monitor_page.*   the network monitor (default page)
  status_page.*    the earlier demo page (kept for page switching)
src/monitor/
  model.hpp        targets, streaks, formatting (platform-free)
  config.hpp       editable target list + validation (platform-free)
  prober.cpp       HTTP/ICMP probes, NVS config, snapshot publishing
src/web/
  server.cpp       esp_http_server routes + JSON API
  assets.cpp       generated: index.html and app.css
web/               Tailwind source project (index.html, input.css)
```

## Orientation

The panel is **648x480 natively** but is mounted **tall**, so the UI lays out in
a logical **480x648** canvas and `ui::rotate_canvas()` turns each frame into the
panel frame just before it is streamed. Dirty regions are rotated with
`ui::rotated_rect()`, so partial refreshes stay correct.

The direction is one constant in `main.cpp`:

```cpp
constexpr ui::Rotation kPanelRotation = ui::Rotation::Cw90;
```

With `Cw90`, the panel reads upright when it is mounted rotated 90 degrees
counter-clockwise (its native right edge at the top). If the image comes out
upside down, use `Ccw90`.

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

`Component::schema()` declares the accepted parameters (type, required, int
range). Components apply their own fallbacks with `ParamSet::get_*`, and
`validate()` reports unknown keys, missing **or empty** required keys, wrong
types and out-of-range integers. `UiApp::validation_errors()` collects those
for every registered page and the device logs them at boot.

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

Drawing is clipped to each component's bounds, and labels measure their **ink**
extent rather than the advance sum (a few Cozette glyphs, e.g. `4`, paint past
their advance). Together these guarantee that a component can never paint
outside the area the dirty tracker reports.

### Refresh policy

`src/panel_plan.hpp` turns UI rects into panel windows and decides what to do.
It is deliberately ESP-free so the host tests cover it:

```cpp
panel::Window w = panel::to_window(ui::Rect{284, 275, 80, 20}); // x1,y1,x2,y2 inclusive
panel::Action a = panel::choose(dirty, /*max_regions=*/4, /*max_area=*/648*480/2);
bool due = panel::due(now, last_full, 120000);                  // wrap-safe
```

`ui::Rect` is `(x, y, w, h)` while the UC8179 window is inclusive edges; mixing
them up silently invalidates every partial window (the driver now rejects an
invalid window instead of pretending success). The device issues one partial
refresh per region and falls back to a full refresh when a region is large, too
many regions changed, a partial update fails, or the 120 s ghost cleanup is due.
A failed full refresh keeps the panel dirty and retries after 5 s.

### Pages in this firmware

`monitor` (default) is a table of seven targets: name, latency, status chip and
up/down streak, plus a footer with this device's power and network uptime.
`status` is the earlier demo page. `UiApp::show_page()` is what a future button
would call.

### Network monitor

`src/monitor/` probes the configured targets every `probe_seconds` (default 30)
from a dedicated task:

| kind | check |
| --- | --- |
| `http` | GET, 2xx/3xx, optional body match (`{"status":true}` for `/health`) |
| `ping` | one ICMP echo, latency from the ping profile |

Uptime is **observed uptime**: how long the current up/down streak has lasted
since this device started watching. There is no remote uptime source for these
hosts, so the panel reports what it can see, and the device's own counters
(`power`, `net`) come from `esp_timer`.

Configuration lives in NVS as JSON and is validated in `monitor/config.cpp`
(max 14 targets, per-kind address rules, unique labels, interval clamped to
10..3600 s). Defaults: `webui`, `akupunktur`, `ha`, `deprived`, `immich`, the
jelly stack (`qbit`, `theoriarr`, `prowlarr`, `seer`, `jelly`) and the LAN
pings (`botserver`, `metal`, `botmain`).

### DNS

Resolution uses whatever DHCP advertises, and `/api/status` reports it:

```json
{"dns_main":"192.168.50.1","dns_secondary":"0.0.0.0"}
```

A private zone such as `*.deprived.internal` only exists on the LAN resolver,
so if DHCP ever hands out a public resolver first, those targets fail. The
config therefore accepts two optional resolvers:

```json
{"dns_main":"192.168.50.1","dns_secondary":"1.1.1.1"}
```

They are applied to the STA interface with `esp_netif_set_dns_info()` on the
next probe round (leave them blank to keep DHCP). Clearing them again takes
effect on the next reconnect, because lwIP keeps the last servers until DHCP
re-runs.

### Configuration web UI

The device serves a Tailwind page on port 80:

| route | purpose |
| --- | --- |
| `GET /` | the config page (embedded) |
| `GET /app.css` | built Tailwind CSS (embedded) |
| `GET /api/config` | current config as JSON |
| `POST /api/config` | validate, store in NVS, re-probe immediately |
| `GET /api/status` | live snapshot (latency, streaks, uptime) |
| `POST /api/control` | `{"action": ...}` see below |

Control actions (the page has buttons for each):

| action | effect |
| --- | --- |
| `{"action":"refresh"}` | force a full-screen repaint |
| `{"action":"probe"}` | run a probe round now instead of waiting |
| `{"action":"page","page":"monitor"}` | switch to the monitor page |
| `{"action":"page","page":"status"}` | switch to the demo page |
| `{"action":"reboot"}` | `esp_restart()` |

The HTTP task never touches the UI directly: it posts to a single-slot queue
(`src/control/`) that the main loop drains once per iteration, so a button press
cannot race the frame buffer. The latest request wins.

The page is deliberately minimal: it lists targets, lets you add/remove rows,
edit the interval, shows the live summary (WiFi, up count, uptime) and has the
control buttons. Design comes later.

Tailwind v4 lives in `web/`:

```sh
cd web
npm install          # once
npm run build        # tailwind -> web/app.css, then embeds both files
```

`npm run build` regenerates `src/web/assets.cpp` via `tools/embed_web.py`, so
the firmware builds without node. `web/app.css` is committed.

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
pio test -e native      # 108 Unity tests, no hardware
tools/preview.sh        # real pages + the rotated panel frame, PNG + ASCII
python3 tools/check_sources.py   # component sources match src/CMakeLists.txt
```

The preview writes `preview-monitor.png` (logical 480x648),
`preview-panel.png` (what the panel receives) and the demo pages.

The suite covers the control queue, geometry/anchor math, rect merging,
parameter typing and
schema validation, UTF-8 (including malformed, truncated, overlong and
embedded-NUL input and `Æ Ø Å æ ø å`), the 1 bpp canvas (unaligned and
partly-used edge bytes, clipping, invert strokes, multi-byte glyphs, hashing,
region diffing), component layout/measurement/clipping, page/app navigation,
dirty-region computation (shrink, hide, move, two regions, container
background, page switch and back, canvas mismatch), the panel refresh policy
(inclusive windows, region/area thresholds, `uint32` wrap) and the real status
page (counter ticks and wrap, heartbeat, throttled WiFi polling, exact dirty
rects, old-pixel erasure, boot-frame hash).

`test_status_page` also pins a golden hash of the boot frame. On mismatch it
writes `test_boot.pgm` plus an ASCII dump (`.gitignore`d) so you can see what
changed. When a change is intentional:

```sh
PRINT_GOLDEN=1 pio test -e native -f test_status_page -v   # prints the new hash
```

then paste it into `test_boot_frame_golden`. `PRINT_GOLDEN` is ignored when
`CI` is set, so a CI run can never skip the assertion.

`tools/preview.sh` builds `src/preview/preview_main.cpp` for the host, prints
the component tree and an ASCII frame, and writes `preview-*.png` for the boot
state, the counter ticking, the WiFi label and the "full" mode label.

## Fonts

`src/ui/fonts/` holds generated tables: 1 bpp bitmaps plus per-glyph
advance/offset, produced from `CozetteVector-nerd.ttf` with printable ASCII and
`Æ Ø Å æ ø å`.

```sh
python3 tools/gen_fonts.py ../CozetteVector-nerd.ttf src/ui/fonts
```

The TTF itself is not tracked here (it lives next to the repo); regeneration
requires it, and the generated tables are committed so normal builds never need
it. Expected font SHA-256:

```
0113c020314250e1e794ccb034103a8da80488ac69ad72bd32d5bfcf8cf5a5f5
```

The generator asserts that glyph fields fit the C++ types and prints how many
glyphs overhang their advance (`5` at 16 px, `2` at 96 px today); measurement
uses the ink extent so those are handled rather than clipped.

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
image, so the next partial update is driven against what is actually on glass.
Plane polarity is `0x00 = black`, `0xFF = white`.

A full refresh first pulses RST and re-runs the full init (a partial update
leaves the controller in partial mode with a small `0x90` window that `0x04`
does not clear), then writes `0x10 = 0xFF`, `0x13 = frame`, `0x12`, waits, and
preloads `0x10` with the frame. Every operation returns `false` if a BUSY wait
times out or an SPI transfer failed.

## WiFi credentials

The station credentials are **not** in git. Copy the example and fill it in:

```sh
cp src/wifi_credentials.example.hpp src/wifi_credentials.hpp   # gitignored
```

`src/wifi_seeds.hpp` includes that file when present; without it the firmware
still builds and runs, logs a warning and simply has no networks to try. The
bench AP list lives only in your local `wifi_credentials.hpp`.

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

- A seven-row network monitor (status, latency, streak) repainted by partial
  refreshes when a probe result changes.
- Header summary (`7/7 up` / `OFFLINE` / `starting`), footer with power and
  network uptime, IP and probe interval.
- Configuration at `http://<device-ip>/`.
- A full-screen full-mode refresh every 15 minutes to clear ghosting.
