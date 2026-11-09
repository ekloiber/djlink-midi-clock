#include "status_led.h"

namespace statusled {

namespace {
constexpr Rgb kOff{0, 0, 0};
constexpr Rgb kRed{255, 0, 0};
constexpr Rgb kAmber{255, 100, 0};
constexpr Rgb kBlue{0, 0, 255};
constexpr Rgb kMagenta{255, 0, 200};
constexpr Rgb kCyan{0, 200, 255};
constexpr Rgb kGreen{0, 255, 0};
constexpr Rgb kGreenDim{0, 40, 0};
constexpr Rgb kWhite{255, 255, 255};

constexpr uint32_t kBeatFlashMs = 60;
constexpr uint32_t kBarFlashMs = 120;

bool blink(uint32_t nowMs, uint32_t periodMs) { return (nowMs % periodMs) < periodMs / 2; }
}  // namespace

State classify(const Inputs& in) {
  if (!in.hardwareOk) return State::kNoHardware;
  if (!in.linkUp) return State::kNoLink;
  if (in.held) return State::kHeld;
  if (in.running) return in.locked ? State::kRunning : State::kFreewheel;
  if (in.devicesSeen == 0) return State::kSearching;
  if (in.sourcePlaying) return State::kArmed;
  return State::kIdle;
}

Rgb colour(const Inputs& in, uint32_t nowMs) {
  switch (classify(in)) {
    case State::kNoHardware: return blink(nowMs, 250) ? kRed : kOff;
    case State::kNoLink:     return kRed;
    case State::kSearching:  return blink(nowMs, 1000) ? kAmber : kOff;
    case State::kIdle:       return kBlue;
    case State::kHeld:       return kMagenta;
    case State::kArmed:      return blink(nowMs, 250) ? kCyan : kOff;
    case State::kRunning:
      if (in.msSinceBar < kBarFlashMs) return kWhite;
      if (in.msSinceBeat < kBeatFlashMs) return kGreen;
      return kGreenDim;
    case State::kFreewheel:
      return in.msSinceBeat < kBeatFlashMs ? kAmber : kOff;
  }
  return kOff;
}

const char* name(State s) {
  switch (s) {
    case State::kNoHardware: return "no-hardware";
    case State::kNoLink:     return "no-link";
    case State::kSearching:  return "searching";
    case State::kIdle:       return "idle";
    case State::kHeld:       return "held";
    case State::kArmed:      return "armed";
    case State::kRunning:    return "running";
    case State::kFreewheel:  return "freewheel";
  }
  return "?";
}

}  // namespace statusled
