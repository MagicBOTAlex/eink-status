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
  // waveform mode. This is the reference escreen.h sequence verbatim: no TRES
  // and no 0x92, because this panel's controller is sensitive to extra
  // commands during bring-up.
  bool reset_and_init(bool partial);

  // Full-screen clear in full-waveform mode (0x10 and 0x13 both get `color`).
  bool clear(uint8_t color);

  // Full-waveform repaint of `frame`: init full, draw/refresh the whole frame,
  // then return to the partial configuration.
  bool full_refresh(const uint8_t *frame);

  // Partial update: write NEW (0x13), refresh (0x12), wait, then preload OLD
  // (0x10) with the same frame. This is exactly the reference draw_partial +
  // apply_old_buffer order.
  bool partial_update(const uint8_t *frame, const Rect &area);

  bool wait_idle(uint32_t timeout_ms, const char *what);
  bool is_busy() const { return gpio_get_level(kPinBusy) == 0; }
  int busy_level() const { return gpio_get_level(kPinBusy); }

  // Bench diagnostics for the panel power-enable net.
  void power_output(bool high);
  void power_high_z();

private:
  void reset_panel();
  void init_mode(bool partial);
  void set_window(int x1_byte, int x2_byte, int y1, int y2);
  bool draw_and_refresh(const uint8_t *frame, int x1_byte, int x2_byte,
                        int y1, int y2);
  void write_window(const uint8_t *src, int stride, int x1_byte, int x2_byte,
                    int y1, int y2);
  void write_fill(uint8_t value, size_t count);

  void send_cmd(uint8_t cmd);
  void send_data(uint8_t data);
  void start_stream(uint8_t cmd);
  void end_stream();
  void send_raw(const uint8_t *data, size_t len);

  spi_device_handle_t spi_ = nullptr;
  uint8_t *dma_buf_ = nullptr;
  bool partial_mode_ = true;
};

} // namespace epd
