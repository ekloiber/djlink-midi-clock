// Messages passed between cores through pico_util queues.
#pragma once

#include <stdint.h>

enum ClockEventType : uint8_t {
  kEvBeat = 1,       // beat from the followed device
  kEvTransport = 2,  // followed device started/stopped
  kEvResync = 3,     // followed device changed
};

struct ClockEvent {
  uint8_t type;
  uint8_t beatInBar;
  uint8_t playing;
  uint8_t device;
  float   bpm;
  int64_t tUs;       // time_us_64() when the packet was received
};

// Snapshot published by core 1 for diagnostics (written with a sequence lock).
struct ClockStatus {
  volatile uint32_t seq;
  float    bpm;
  float    phaseErrUs;
  uint8_t  locked;
  uint8_t  running;
  uint8_t  ticking;
  uint8_t  held;
};
