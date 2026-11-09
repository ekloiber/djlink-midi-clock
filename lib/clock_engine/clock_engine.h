// MIDI clock generator phase-locked to Pro DJ Link beat events.
//
// Portable C++ (no Arduino deps). Runs on core 1: call onBeat()/onTransport()
// when events arrive from core 0, and poll() in a tight loop; poll() returns the
// real-time MIDI bytes that are due.
//
// Invariant: tick_ (0..23) always equals the number of clocks emitted since the
// last Start, mod 24. Phase is corrected only by stretching/compressing tick
// intervals, never by dropping or inserting ticks, so downstream gear that
// counts clocks stays beat-aligned.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace midiclock {

constexpr uint8_t kMidiClock = 0xF8;
constexpr uint8_t kMidiStart = 0xFA;
constexpr uint8_t kMidiStop  = 0xFC;
constexpr int     kPpqn      = 24;
constexpr int     kBeatsPerBar = 4;  // DJ Link beat-in-bar is always 1..4

// Bits reported by poll() alongside the clock that starts a beat / bar.
constexpr uint8_t kEventBeat = 0x01;  // this clock is tick 0 of a beat
constexpr uint8_t kEventBar  = 0x02;  // ...and that beat is beat 1 of the bar

struct ClockConfig {
  int64_t offsetUs = 0;               // + delays clock vs. received beat, - advances it
  double  phaseGain = 0.3;            // fraction of measured phase error corrected per beat
  double  maxSlew = 0.03;             // max tick-period change in normal tracking (3%)
  double  resyncThresholdBeats = 0.1; // errors larger than this use the fast regime
  double  resyncGain = 1.0;
  double  resyncMaxSlew = 0.5;
  int64_t loseLockUs = 2000000;       // no beats for this long -> unlocked
  bool    runWhenStopped = true;      // keep sending clock when not locked/playing
  bool    sendTransport = true;       // send Start/Stop
  bool    startOnDownbeat = true;     // delay Start until beat 1 of a bar
};

class ClockEngine {
 public:
  explicit ClockEngine(const ClockConfig& cfg = ClockConfig()) : cfg_(cfg) {}

  void configure(const ClockConfig& cfg) { cfg_ = cfg; }

  // A beat from the followed device was received at rxUs.
  void onBeat(int64_t rxUs, double bpm, uint8_t beatInBar);

  // Followed device started/stopped playing.
  void onTransport(bool playing, int64_t nowUs);

  // Followed device changed (e.g. new tempo master): realign on next beat
  // using the fast regime.
  void requestResync() { resyncPending_ = true; }

  // Emits up to `cap` due bytes into `out`; returns count. Call often.
  // If `events` is given it receives kEventBeat/kEventBar for the clock just
  // emitted (0 otherwise). Bar events need beat-in-bar from the source; they
  // are phase-locked exactly like the clock itself.
  size_t poll(int64_t nowUs, uint8_t* out, size_t cap, uint8_t* events = nullptr);

  // Earliest time poll() could have something to do (for simulation/sleep).
  int64_t nextDeadline() const;

  bool    ticking() const { return ticking_; }
  bool    locked() const { return locked_; }
  bool    running() const { return running_; }  // between Start and Stop
  double  bpm() const { return bpm_; }
  double  lastPhaseErrorUs() const { return lastErrUs_; }
  int     tickInBeat() const { return tick_; }
  int     nextBar() const { return nextBar_; }  // bar position of next tick 0 (0 = unknown)

 private:
  void startStream(double atUs, uint8_t beatInBar);

  ClockConfig cfg_;
  bool   ticking_ = false;
  bool   locked_ = false;
  bool   running_ = false;
  bool   playing_ = false;
  bool   startArmed_ = false;
  bool   startPending_ = false;
  bool   stopPending_ = false;
  bool   resyncPending_ = false;
  bool   haveTicked_ = false;

  double bpm_ = 0.0;
  double nominalUs_ = 0.0;   // 60e6 / (bpm * 24)
  double periodUs_ = 0.0;    // current (possibly corrected) tick period
  int    correctionTicks_ = 0;
  double nextTickUs_ = 0.0;
  int    tick_ = 0;          // index of the NEXT tick within the beat
  int    nextBar_ = 0;       // beat-in-bar (1..4) of the next tick 0; 0 = unknown
  double lastTickUs_ = 0.0;  // scheduled time of the most recent clock
  int64_t lastBeatRxUs_ = 0;
  double lastErrUs_ = 0.0;
};

}  // namespace midiclock
