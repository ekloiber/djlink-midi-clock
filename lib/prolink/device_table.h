// Per-device state, keyed by DJ Link device number (never by IP address).
//
// XDJ-XZ quirk: both of its decks (and its mixer section) report from a single
// IP address, so anything keyed by IP would merge them. Beat Link 0.6.0 had to
// rework its device matching for the same reason.
#pragma once

#include <stdint.h>

#include "prolink_packets.h"

namespace prolink {

constexpr int kMaxDevice = 64;

struct DeviceState {
  bool    known = false;
  bool    isMixer = false;
  uint8_t ip[4] = {0, 0, 0, 0};
  char    name[kNameLen + 1] = {0};
  int64_t lastAnnounceUs = 0;
  int64_t lastStatusUs = 0;
  int64_t lastBeatUs = 0;
  bool    playing = false;  // from status
  bool    master = false;   // from status
  double  bpm = 0.0;        // effective BPM, latest beat or status
  uint8_t beatInBar = 0;
};

class DeviceTable {
 public:
  // Freshness windows. Status arrives every ~200 ms; beats every beat.
  static constexpr int64_t kStatusFreshUs = 1000000;
  static constexpr int64_t kBeatFreshUs   = 2000000;
  static constexpr int64_t kDeviceGoneUs  = 10000000;

  void setSelf(uint8_t device) { self_ = device; }
  uint8_t self() const { return self_; }

  void onKeepAlive(const KeepAlive& k, int64_t nowUs);
  void onBeat(const BeatPacket& b, int64_t nowUs);
  void onCdjStatus(const CdjStatus& s, int64_t nowUs);
  void onMixerStatus(const MixerStatus& s, int64_t nowUs);
  void expire(int64_t nowUs);

  // Device to follow: `follow` if non-zero, else the device whose fresh status
  // has the master flag, else the lowest-numbered device sending beats.
  // Returns 0 if nothing suitable.
  uint8_t selectMaster(int64_t nowUs, uint8_t follow = 0) const;

  // Playing state: from fresh status if we have it, otherwise inferred from
  // recent beat packets (players only send beats while playing): stopped
  // after ~1.5 missed beats.
  bool isPlaying(uint8_t device, int64_t nowUs) const;

  // Lowest device number in [lo, hi] not currently seen on the network.
  uint8_t pickFreeNumber(uint8_t lo, uint8_t hi) const;

  int  knownCount() const;
  int  countAtIp(const uint8_t ip[4]) const;
  const DeviceState& get(uint8_t device) const { return d_[device % kMaxDevice]; }

 private:
  DeviceState* slot(uint8_t device);
  DeviceState d_[kMaxDevice];
  uint8_t self_ = 0;
};

}  // namespace prolink
