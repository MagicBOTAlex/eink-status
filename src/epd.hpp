#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"

namespace epd {

// Good Display GDEY0583T81: 648 x 480, 1 bpp, single controller.
inline constexpr int kWidth = 648;
inline constexpr int kHeight = 480;
inline constexpr int kStride = kWidth / 8;              // 81 bytes
inline constexpr size_t kFrameBytes =
    static_cast<size_t>(kStride) * kHeight;             // 38880 bytes

// Wiring (Seeed XIAO ESP32-S3 labels in parentheses).
inline constexpr gpio_num_t kPinMosi = GPIO_NUM_9;  // D10
inline constexpr gpio_num_t kPinSclk = GPIO_NUM_7;  // D8
inline constexpr gpio_num_t kPinCs = GPIO_NUM_2;    // D1
inline constexpr gpio_num_t kPinDc = GPIO_NUM_3;    // D2
inline constexpr gpio_num_t kPinRst = GPIO_NUM_4;   // D3
inline constexpr gpio_num_t kPinBusy = GPIO_NUM_5;  // D4
inline constexpr gpio_num_t kPinPwr = GPIO_NUM_6;   // D5

inline constexpr spi_host_device_t kSpiHost = SPI2_HOST;
inline constexpr int kSpiClockHz = 5 * 1000 * 1000;

struct Rect {
  int x1;
  int y1;
  int x2;
  int y2; // inclusive
};

class Driver {
public:
  bool begin();

  // Hard power/reset cycle, then program the controller in full or partial
  // waveform mode. Mirrors the verified reference sequence for this panel.
  bool reset_and_init(bool partial);

  // Full-screen update in full mode: old data is forced to white so every
  // pixel is driven to the new frame. Leaves the panel in partial mode.
  bool full_refresh(const uint8_t *frame);

  // Windowed update. Sends the previous frame as OLD and the new frame as NEW
  // for the same window, then issues 0x12.
  bool partial_update(const uint8_t *old_frame, const uint8_t *new_frame,
                      const Rect &area);

  bool wait_idle(uint32_t timeout_ms, const char *what);
  bool is_busy() const { return gpio_get_level(kPinBusy) == 0; }

  // Bring-up diagnostic: reset, initialise, write a full OLD/NEW frame and
  // issue 0x12, then report whether BUSY ever asserted. Used to probe the
  // power-enable polarity on the bench.
  bool refresh_liveness_test(const char *label, uint8_t old_fill,
                             uint8_t new_fill);

  // Same liveness test but clocking MOSI/SCLK by hand, to separate a hardware
  // SPI configuration problem from a panel/wiring/power problem.
  bool soft_spi_liveness_test();

  int busy_level() const { return gpio_get_level(kPinBusy); }

private:
  void reset_panel();
  void init_mode(bool partial);
  void set_window(int x1_byte, int x2_byte, int y1, int y2);
  void write_window(const uint8_t *src, int stride, int x1_byte, int x2_byte,
                    int y1, int y2);
  void write_fill(uint8_t value, size_t count);

  void send_cmd(uint8_t cmd);
  void send_data(uint8_t data);
  void start_stream(uint8_t cmd);
  void end_stream();
  void send_raw(const uint8_t *data, size_t len);

  void soft_init();

  spi_device_handle_t spi_ = nullptr;
  uint8_t *dma_buf_ = nullptr;
  bool partial_mode_ = true;
  bool soft_mode_ = false;
};

} // namespace epd
