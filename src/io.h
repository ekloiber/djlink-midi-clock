// GPIO helpers and compile-time pin-map checks.
#pragma once

#include <Arduino.h>

#include "config.h"

// ------------------------------------------------------------ pin-map checks
namespace pincheck {

constexpr int kUsed[] = {
    CLOCK_OUT_PIN,    RUN_OUT_PIN,      BEAT_PULSE_PIN,   BAR_PULSE_PIN,    STATUS_LED_PIN,
    RESYNC_IN_PIN,    SELECT_PIN_MASTER, SELECT_PIN_DECK1, SELECT_PIN_DECK2, SELECT_PIN_DECK3,
    SELECT_PIN_DECK4, STATUS_PIXEL_PIN,
};
// TX/RX (MIDI), PSRAM CS, Ethernet CS + SPI.
constexpr int kReserved[] = {0, 1, 8, ETH_CS_PIN, 20, 22, 23};

constexpr bool noDuplicates() {
  for (size_t i = 0; i < sizeof(kUsed) / sizeof(kUsed[0]); ++i)
    for (size_t j = i + 1; j < sizeof(kUsed) / sizeof(kUsed[0]); ++j)
      if (kUsed[i] >= 0 && kUsed[i] == kUsed[j]) return false;
  return true;
}
constexpr bool noReserved() {
  for (int u : kUsed)
    for (int r : kReserved)
      if (u >= 0 && u == r) return false;
  return true;
}
static_assert(noDuplicates(), "Two I/O functions are assigned the same pin (see config.h)");
static_assert(noReserved(), "An I/O pin collides with MIDI, Ethernet or PSRAM pins");

}  // namespace pincheck

// ------------------------------------------------------------ outputs
inline int activeLevel()   { return OUTPUT_ACTIVE_HIGH ? HIGH : LOW; }
inline int inactiveLevel() { return OUTPUT_ACTIVE_HIGH ? LOW : HIGH; }

// Timed pulse (trigger / clock). fire() writes the edge immediately.
class PulseOut {
 public:
  explicit PulseOut(int pin) : pin_(pin) {}
  void begin() {
    if (pin_ < 0) return;
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, inactiveLevel());
  }
  void fire(uint64_t nowUs, uint32_t widthUs) {
    if (pin_ < 0) return;
    digitalWrite(pin_, activeLevel());
    offAtUs_ = nowUs + (widthUs ? widthUs : 1);
  }
  void service(uint64_t nowUs) {
    if (pin_ < 0 || !offAtUs_ || nowUs < offAtUs_) return;
    digitalWrite(pin_, inactiveLevel());
    offAtUs_ = 0;
  }

 private:
  int pin_;
  uint64_t offAtUs_ = 0;
};

// Level output (Run gate). Only touches the pin when the state changes.
class GateOut {
 public:
  explicit GateOut(int pin) : pin_(pin) {}
  void begin() {
    if (pin_ < 0) return;
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, inactiveLevel());
  }
  void set(bool on) {
    if (pin_ < 0 || on == state_) return;
    state_ = on;
    digitalWrite(pin_, on ? activeLevel() : inactiveLevel());
  }

 private:
  int pin_;
  bool state_ = false;
};

// ------------------------------------------------------------ inputs
// Debounced digital input with internal pull-up/down chosen from polarity.
class DebouncedIn {
 public:
  DebouncedIn(int pin, bool activeLow, uint32_t debounceUs)
      : pin_(pin), activeLow_(activeLow), debounceUs_(debounceUs) {}
  void begin() {
    if (pin_ < 0) return;
    pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT_PULLDOWN);
    stable_ = raw_ = readRaw();
  }
  // Returns true when the debounced state changed on this call.
  bool update(uint64_t nowUs) {
    if (pin_ < 0) return false;
    bool r = readRaw();
    if (r != raw_) {
      raw_ = r;
      changedAtUs_ = nowUs;
    }
    if (raw_ != stable_ && nowUs - changedAtUs_ >= debounceUs_) {
      stable_ = raw_;
      return true;
    }
    return false;
  }
  bool active() const { return stable_; }
  bool enabled() const { return pin_ >= 0; }

 private:
  bool readRaw() const { return (digitalRead(pin_) == HIGH) != activeLow_; }
  int pin_;
  bool activeLow_;
  uint32_t debounceUs_;
  bool raw_ = false, stable_ = false;
  uint64_t changedAtUs_ = 0;
};
