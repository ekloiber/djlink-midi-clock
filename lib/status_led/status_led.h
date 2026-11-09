// Status indicator logic for the on-board NeoPixel. Portable (host-tested):
// takes a snapshot of system state and the time, returns a colour.
#pragma once

#include <stdint.h>

namespace statusled {

enum class State : uint8_t {
  kNoHardware,   // W5500 not found           red, fast blink
  kNoLink,       // Ethernet cable/link down  red, solid
  kSearching,    // link up, no DJ Link gear  amber, slow blink
  kIdle,         // gear seen, source stopped blue, solid
  kHeld,         // resync held               magenta, solid
  kArmed,        // playing, waiting for downbeat to start   cyan, fast blink
  kRunning,      // running, locked           dim green; green flash per beat, white per bar
  kFreewheel,    // running but beats lost    amber flash per beat
};

struct Inputs {
  bool hardwareOk = true;
  bool linkUp = false;
  int  devicesSeen = 0;
  bool sourcePlaying = false;  // followed deck playing
  bool held = false;           // resync hold active
  bool running = false;        // run gate high
  bool locked = false;         // clock engine locked to beats
  uint32_t msSinceBeat = UINT32_MAX;  // since last emitted beat (x1) event
  uint32_t msSinceBar = UINT32_MAX;   // since last emitted bar event
};

struct Rgb {
  uint8_t r, g, b;
  bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
  bool operator!=(const Rgb& o) const { return !(*this == o); }
};

State classify(const Inputs& in);

// Colour at full scale; the caller applies global brightness.
Rgb colour(const Inputs& in, uint32_t nowMs);

const char* name(State s);

}  // namespace statusled
