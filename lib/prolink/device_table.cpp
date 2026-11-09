#include "device_table.h"

#include <string.h>

namespace prolink {

DeviceState* DeviceTable::slot(uint8_t device) {
  if (device == 0 || device >= kMaxDevice || device == self_) return nullptr;
  return &d_[device];
}

void DeviceTable::onKeepAlive(const KeepAlive& k, int64_t nowUs) {
  DeviceState* s = slot(k.device);
  if (!s) return;
  s->known = true;
  memcpy(s->ip, k.ip, 4);
  memcpy(s->name, k.name, sizeof(s->name));
  s->lastAnnounceUs = nowUs;
}

void DeviceTable::onBeat(const BeatPacket& b, int64_t nowUs) {
  DeviceState* s = slot(b.device);
  if (!s) return;
  s->known = true;
  s->lastBeatUs = nowUs;
  s->beatInBar = b.beatInBar;
  double bpm = b.bpm();
  if (bpm > 0) s->bpm = bpm;
}

void DeviceTable::onCdjStatus(const CdjStatus& st, int64_t nowUs) {
  DeviceState* s = slot(st.device);
  if (!s) return;
  s->known = true;
  s->isMixer = false;
  s->lastStatusUs = nowUs;
  s->playing = st.playing();
  s->master = st.master();
  double bpm = st.bpm();
  if (bpm > 0) s->bpm = bpm;
}

void DeviceTable::onMixerStatus(const MixerStatus& st, int64_t nowUs) {
  DeviceState* s = slot(st.device);
  if (!s) return;
  s->known = true;
  s->isMixer = true;
  s->lastStatusUs = nowUs;
  s->master = st.master();
  s->playing = false;
}

void DeviceTable::expire(int64_t nowUs) {
  for (auto& s : d_) {
    if (!s.known) continue;
    int64_t last = s.lastAnnounceUs;
    if (s.lastStatusUs > last) last = s.lastStatusUs;
    if (s.lastBeatUs > last) last = s.lastBeatUs;
    if (nowUs - last > kDeviceGoneUs) s = DeviceState{};
  }
}

uint8_t DeviceTable::selectMaster(int64_t nowUs, uint8_t follow) const {
  if (follow) return follow;
  for (int i = 1; i < kMaxDevice; ++i) {
    const auto& s = d_[i];
    if (s.known && s.master && nowUs - s.lastStatusUs < kStatusFreshUs) return uint8_t(i);
  }
  for (int i = 1; i < kMaxDevice; ++i) {
    const auto& s = d_[i];
    if (s.known && s.lastBeatUs && nowUs - s.lastBeatUs < kBeatFreshUs) return uint8_t(i);
  }
  return 0;
}

bool DeviceTable::isPlaying(uint8_t device, int64_t nowUs) const {
  if (device == 0 || device >= kMaxDevice) return false;
  const auto& s = d_[device];
  if (!s.known) return false;
  if (s.lastStatusUs && nowUs - s.lastStatusUs < kStatusFreshUs && !s.isMixer) return s.playing;
  // No status (listen-only): infer play from beats. Treat 1.5 missed beat
  // intervals as stopped, bounded to [300 ms, 2 s].
  if (!s.lastBeatUs) return false;
  int64_t window = kBeatFreshUs;
  if (s.bpm > 0) {
    window = int64_t(1.5 * 60e6 / s.bpm);
    if (window < 300000) window = 300000;
    if (window > kBeatFreshUs) window = kBeatFreshUs;
  }
  return nowUs - s.lastBeatUs < window;
}

uint8_t DeviceTable::pickFreeNumber(uint8_t lo, uint8_t hi) const {
  for (int n = lo; n <= hi && n < kMaxDevice; ++n) {
    if (!d_[n].known) return uint8_t(n);
  }
  return 0;
}

int DeviceTable::knownCount() const {
  int n = 0;
  for (const auto& s : d_) n += s.known ? 1 : 0;
  return n;
}

int DeviceTable::countAtIp(const uint8_t ip[4]) const {
  int n = 0;
  for (const auto& s : d_) n += (s.known && memcmp(s.ip, ip, 4) == 0) ? 1 : 0;
  return n;
}

}  // namespace prolink
