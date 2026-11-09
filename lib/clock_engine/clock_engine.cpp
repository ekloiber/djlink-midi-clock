#include "clock_engine.h"

#include <math.h>

namespace midiclock {

namespace {
double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
// Wrap any integer onto 1..kBeatsPerBar.
int wrapBar(int x) { return ((x - 1) % kBeatsPerBar + kBeatsPerBar) % kBeatsPerBar + 1; }
bool validBar(uint8_t b) { return b >= 1 && b <= kBeatsPerBar; }
}  // namespace

void ClockEngine::startStream(double atUs, uint8_t beatInBar) {
  // Never put two clocks closer than half a tick (e.g. a free-running clock
  // that fired just before the downbeat Start).
  if (haveTicked_ && atUs < lastTickUs_ + 0.5 * nominalUs_) atUs = lastTickUs_ + 0.5 * nominalUs_;
  nextTickUs_ = atUs;
  tick_ = 0;
  periodUs_ = nominalUs_;
  correctionTicks_ = 0;
  nextBar_ = validBar(beatInBar) ? beatInBar : 0;
  ticking_ = true;
  locked_ = true;
}

void ClockEngine::onBeat(int64_t rxUs, double bpm, uint8_t beatInBar) {
  if (!(bpm > 1.0) || !isfinite(bpm)) return;
  bpm_ = bpm;
  nominalUs_ = 60e6 / (bpm * kPpqn);
  lastBeatRxUs_ = rxUs;
  const double target = double(rxUs + cfg_.offsetUs);
  const double now = double(rxUs);
  const double atOrNow = target > now ? target : now;

  // Start: resets the downstream clock count, so we may realign hard here.
  if (startArmed_ && (!cfg_.startOnDownbeat || beatInBar == 1 || beatInBar == 0)) {
    startArmed_ = false;
    startPending_ = true;
    resyncPending_ = true;  // Start may be nudged late; snap fully on next beat
    startStream(atOrNow, beatInBar);
    return;
  }

  if (!ticking_) {
    startStream(atOrNow, beatInBar);
    resyncPending_ = false;
    return;
  }

  // R = intervals between the next scheduled clock and the tick 0 we steer.
  // If our next tick 0 is less than half a beat away there is too little room
  // to correct it smoothly (and if it is the very next clock it is already
  // committed), so steer the tick 0 after it instead.
  int R = (kPpqn - tick_) % kPpqn;
  const int corrected = correctionTicks_ < R ? correctionTicks_ : R;
  double nextB0 = nextTickUs_ + corrected * periodUs_ + (R - corrected) * nominalUs_;
  int skipped = 0;  // tick-0s between now and the one we steer
  if (R < kPpqn / 2) {
    R += kPpqn;
    nextB0 += kPpqn * nominalUs_;
    skipped = 1;
  }

  // Compare against the nearest point on the beat grid implied by this beat.
  const double beatUs = nominalUs_ * kPpqn;
  const double beatsAhead = round((nextB0 - target) / beatUs);
  const double grid = target + beatsAhead * beatUs;
  const double err = nextB0 - grid;  // > 0: our tick 0 would land late
  lastErrUs_ = err;

  // Bar position of the next tick 0 we will emit. The steered tick 0 sits
  // `beatsAhead` beats after this beat; the next one emitted may be one earlier.
  if (validBar(beatInBar)) nextBar_ = wrapBar(beatInBar + int(beatsAhead) - skipped);

  const bool fast = resyncPending_ || !locked_ ||
                    fabs(err) > cfg_.resyncThresholdBeats * beatUs;
  const double gain = fast ? cfg_.resyncGain : cfg_.phaseGain;
  const double slew = (fast ? cfg_.resyncMaxSlew : cfg_.maxSlew) * nominalUs_;

  // Spread the correction over the R intervals before that tick 0, then
  // return to the nominal period.
  periodUs_ = nominalUs_ + clampd(-gain * err / R, -slew, slew);
  correctionTicks_ = R;
  resyncPending_ = false;
  locked_ = true;
}

void ClockEngine::onTransport(bool playing, int64_t nowUs) {
  (void)nowUs;
  if (playing == playing_) return;
  playing_ = playing;
  if (!cfg_.sendTransport) return;
  if (playing) {
    startArmed_ = true;
  } else {
    startArmed_ = false;
    startPending_ = false;
    if (running_) stopPending_ = true;
    running_ = false;
    if (!cfg_.runWhenStopped) ticking_ = false;
  }
}

size_t ClockEngine::poll(int64_t nowUs, uint8_t* out, size_t cap, uint8_t* events) {
  size_t n = 0;
  if (events) *events = 0;
  if (stopPending_ && n < cap) {
    out[n++] = kMidiStop;
    stopPending_ = false;
  }

  if (locked_ && nowUs - lastBeatRxUs_ > cfg_.loseLockUs) {
    locked_ = false;
    resyncPending_ = true;
    if (!cfg_.runWhenStopped) ticking_ = false;
  }

  if (ticking_ && double(nowUs) >= nextTickUs_ && n + 2 <= cap) {
    if (startPending_) {
      out[n++] = kMidiStart;
      startPending_ = false;
      running_ = true;
    }
    out[n++] = kMidiClock;
    if (tick_ == 0) {
      uint8_t ev = kEventBeat;
      if (nextBar_ == 1) ev |= kEventBar;
      if (nextBar_) nextBar_ = nextBar_ % kBeatsPerBar + 1;
      if (events) *events = ev;
    }
    lastTickUs_ = nextTickUs_;
    haveTicked_ = true;
    tick_ = (tick_ + 1) % kPpqn;
    nextTickUs_ += periodUs_;
    if (correctionTicks_ > 0 && --correctionTicks_ == 0) periodUs_ = nominalUs_;
  }
  return n;
}

int64_t ClockEngine::nextDeadline() const {
  if (stopPending_) return 0;
  int64_t d = INT64_MAX;
  if (ticking_) d = int64_t(ceil(nextTickUs_));
  if (locked_) {
    int64_t lose = lastBeatRxUs_ + cfg_.loseLockUs + 1;
    if (lose < d) d = lose;
  }
  return d;
}

}  // namespace midiclock
