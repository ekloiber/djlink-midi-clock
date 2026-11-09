// Host-side tests for the portable libraries. Build & run: make -C tests/host
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "clock_engine.h"
#include "device_table.h"
#include "prolink_packets.h"

using namespace midiclock;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (cond) { ++g_pass; }                                               \
    else { ++g_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

// ------------------------------------------------------------ simulation rig
struct Tick { int64_t t; int index; };  // index = clocks since last Start (or stream start)

struct Sim {
  ClockEngine eng;
  int64_t t = 0;
  std::vector<Tick> ticks;
  std::vector<int64_t> starts, stops;
  std::vector<size_t> startTickPos;  // ticks.size() when each Start was sent
  int index = 0;

  explicit Sim(const ClockConfig& c = ClockConfig()) : eng(c) {}

  void runUntil(int64_t until) {
    for (;;) {
      int64_t d = eng.nextDeadline();
      if (d < t) d = t;
      if (d > until) { t = until; return; }
      t = d;
      uint8_t out[4];
      size_t n = eng.poll(t, out, sizeof(out));
      for (size_t i = 0; i < n; ++i) {
        if (out[i] == kMidiStart) { starts.push_back(t); startTickPos.push_back(ticks.size()); index = 0; }
        else if (out[i] == kMidiStop) stops.push_back(t);
        else if (out[i] == kMidiClock) ticks.push_back({t, index++});
      }
      if (n == 0 && eng.nextDeadline() <= t) ++t;
    }
  }
  void beat(int64_t rx, double bpm, uint8_t bib) { runUntil(rx); eng.onBeat(rx, bpm, bib); }
};

struct Lcg {
  uint32_t s = 12345;
  double uniform(double lo, double hi) {  // [lo, hi)
    s = s * 1664525u + 1013904223u;
    return lo + (hi - lo) * (s >> 8) / double(1 << 24);
  }
};

// Max |tick0 - nearest beat| for beat-aligned ticks after `fromUs`.
static double maxPhaseErr(const Sim& s, const std::vector<int64_t>& beats, int64_t fromUs) {
  double worst = 0;
  for (const auto& tk : s.ticks) {
    if (tk.index % kPpqn != 0 || tk.t < fromUs) continue;
    double best = 1e18;
    for (int64_t b : beats) best = fmin(best, fabs(double(tk.t - b)));
    worst = fmax(worst, best);
  }
  return worst;
}

// Every interval between consecutive beats (after fromUs) holds exactly 24 clocks.
static bool exactly24PerBeat(const Sim& s, const std::vector<int64_t>& beats, int64_t fromUs,
                             int64_t tolUs) {
  for (size_t i = 0; i + 1 < beats.size(); ++i) {
    if (beats[i] < fromUs) continue;
    int c = 0;
    for (const auto& tk : s.ticks)
      if (tk.t >= beats[i] - tolUs && tk.t < beats[i + 1] - tolUs) ++c;
    if (c != kPpqn) { printf("    beat %zu had %d clocks\n", i, c); return false; }
  }
  return true;
}

// ------------------------------------------------------------ clock tests
static void testSteadyTempo() {
  printf("steady 120 BPM, no jitter\n");
  Sim s;
  std::vector<int64_t> beats;
  for (int k = 0; k < 40; ++k) {
    int64_t b = 1000000 + int64_t(k) * 500000;
    beats.push_back(b);
    s.beat(b, 120.0, uint8_t(k % 4 + 1));
  }
  s.runUntil(beats.back() + 400000);
  CHECK(maxPhaseErr(s, beats, beats[2]) < 50);
  CHECK(exactly24PerBeat(s, beats, beats[2], 1000));
  // tick spacing ~ 20833 us
  double dt = double(s.ticks[100].t - s.ticks[99].t);
  CHECK(fabs(dt - 20833.3) < 2);
}

static void testJitter(std::vector<int64_t>* outBeats = nullptr) {
  printf("120 BPM with +/-2 ms receive jitter\n");
  Sim s;
  Lcg rng;
  std::vector<int64_t> beats;
  for (int k = 0; k < 200; ++k) {
    int64_t b = 1000000 + int64_t(k) * 500000;
    beats.push_back(b);
    s.beat(b + int64_t(rng.uniform(-2000, 2000)), 120.0, uint8_t(k % 4 + 1));
  }
  s.runUntil(beats.back() + 400000);
  double err = maxPhaseErr(s, beats, beats[10]);
  printf("    max phase error %.2f ms\n", err / 1000);
  CHECK(err < 2500);
  CHECK(exactly24PerBeat(s, beats, beats[10], 5000));
  // Clock-to-clock jitter limited by maxSlew (3% of 20.8 ms = 0.63 ms)
  double worst = 0;
  for (size_t i = 300; i < s.ticks.size(); ++i)
    worst = fmax(worst, fabs(double(s.ticks[i].t - s.ticks[i - 1].t) - 20833.3));
  printf("    max tick interval deviation %.3f ms\n", worst / 1000);
  CHECK(worst < 700);
  if (outBeats) *outBeats = beats;
}

static void testTempoChange() {
  printf("tempo step 120 -> 128 BPM\n");
  Sim s;
  std::vector<int64_t> beats;
  int64_t b = 1000000;
  for (int k = 0; k < 80; ++k) {
    double bpm = k < 40 ? 120.0 : 128.0;
    beats.push_back(b);
    s.beat(b, bpm, uint8_t(k % 4 + 1));
    b += int64_t(60e6 / bpm);
  }
  s.runUntil(beats.back() + 300000);
  double err = maxPhaseErr(s, beats, beats[48]);
  printf("    phase error 8 beats after change %.2f ms\n", err / 1000);
  CHECK(err < 2000);
  CHECK(exactly24PerBeat(s, beats, beats[42], 3000));
}

static void testMasterSwitchPhaseJump() {
  printf("master change with 0.4-beat phase jump\n");
  Sim s;
  std::vector<int64_t> beats;
  int64_t b = 1000000;
  for (int k = 0; k < 70; ++k) {
    if (k == 40) { b += 200000; s.eng.requestResync(); }  // new deck, offset 0.4 beat
    beats.push_back(b);
    s.beat(b, 120.0, uint8_t(k % 4 + 1));
    b += 500000;
  }
  s.runUntil(beats.back() + 400000);
  double err = maxPhaseErr(s, beats, beats[43]);
  printf("    phase error 3 beats after switch %.2f ms\n", err / 1000);
  CHECK(err < 2000);
  // No dropped/inserted clocks: count since stream start stays continuous and
  // intervals stay within the fast-regime slew limit.
  double minDt = 1e9, maxDt = 0;
  for (size_t i = 1; i < s.ticks.size(); ++i) {
    CHECK(s.ticks[i].index == s.ticks[i - 1].index + 1);
    double dt = double(s.ticks[i].t - s.ticks[i - 1].t);
    minDt = fmin(minDt, dt);
    maxDt = fmax(maxDt, dt);
  }
  printf("    tick interval range %.2f..%.2f ms\n", minDt / 1000, maxDt / 1000);
  CHECK(minDt > 0.45 * 20833 && maxDt < 1.55 * 20833);
}

static void testStartOnDownbeat() {
  printf("Start waits for downbeat\n");
  Sim s;
  std::vector<int64_t> beats;
  uint8_t bib[] = {3, 4, 1, 2, 3, 4, 1, 2};
  s.eng.onTransport(true, 0);
  for (int k = 0; k < 8; ++k) {
    int64_t b = 1000000 + int64_t(k) * 500000;
    beats.push_back(b);
    s.beat(b, 120.0, bib[k]);
  }
  s.runUntil(beats.back() + 100000);
  CHECK(s.starts.size() == 1);
  if (!s.starts.empty()) {
    // Start lands on the bib==1 beat, delayed at most half a tick if a
    // free-running clock had just fired.
    CHECK(s.starts[0] >= beats[2] && s.starts[0] - beats[2] <= 10500);
    CHECK(s.eng.running());
    const Tick& first = s.ticks[s.startTickPos[0]];
    CHECK(first.index == 0 && first.t == s.starts[0]);
    // No clock pair closer than half a tick around the Start.
    CHECK(first.t - s.ticks[s.startTickPos[0] - 1].t >= 10416);
    // Locked to the beat grid again within a few beats.
    CHECK(maxPhaseErr(s, beats, beats[5]) < 2000);
  }
  // Ticks were running before Start (unlocked stream from first beat).
  CHECK(s.ticks.front().t < beats[2]);
}

static void testStopAndFreewheel() {
  printf("Stop and free-run\n");
  {
    ClockConfig c;
    c.runWhenStopped = false;
    Sim s(c);
    s.eng.onTransport(true, 0);
    for (int k = 0; k < 8; ++k) s.beat(1000000 + int64_t(k) * 500000, 120.0, uint8_t(k % 4 + 1));
    s.runUntil(4600000);
    s.eng.onTransport(false, 4600000);
    size_t before = s.ticks.size();
    s.runUntil(8000000);
    CHECK(s.stops.size() == 1);
    CHECK(s.ticks.size() == before);  // no clocks after Stop in this mode
    CHECK(!s.eng.running());
  }
  {
    Sim s;  // runWhenStopped = true
    for (int k = 0; k < 8; ++k) s.beat(1000000 + int64_t(k) * 500000, 120.0, uint8_t(k % 4 + 1));
    s.runUntil(10000000);  // beats stop at 4.5 s
    CHECK(!s.eng.locked());
    CHECK(s.eng.ticking());
    double dt = double(s.ticks.back().t - s.ticks[s.ticks.size() - 2].t);
    CHECK(fabs(dt - 20833.3) < 2);  // free-runs at last tempo, correction expired
  }
}

static void testRelockAfterDropout() {
  printf("relock after 5 s dropout keeps clock count continuous\n");
  Sim s;
  std::vector<int64_t> beats;
  for (int k = 0; k < 10; ++k) { int64_t b = 1000000 + int64_t(k) * 500000; beats.push_back(b); s.beat(b, 120.0, 1); }
  for (int k = 0; k < 20; ++k) {
    int64_t b = 11130000 + int64_t(k) * 468750;  // 128 BPM, arbitrary phase
    beats.push_back(b);
    s.beat(b, 128.0, 1);
  }
  s.runUntil(beats.back() + 300000);
  std::vector<int64_t> after(beats.begin() + 10, beats.end());
  CHECK(maxPhaseErr(s, after, after[3]) < 2000);
  for (size_t i = 1; i < s.ticks.size(); ++i) CHECK(s.ticks[i].index == s.ticks[i - 1].index + 1);
}

// ------------------------------------------------------------ protocol tests
static void put16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
static void put24(uint8_t* p, uint32_t v) { p[0] = v >> 16; p[1] = v >> 8; p[2] = v; }

static void testParseBeat() {
  printf("parse beat packet\n");
  uint8_t p[0x60] = {};
  memcpy(p, prolink::kMagic, 10);
  p[0x0a] = prolink::kTypeBeat;
  memcpy(p + 0x0b, "XDJ-XZ", 6);
  p[0x21] = 2;
  put24(p + 0x55, 0x100000 + 0x100000 / 50);  // +2%
  put16(p + 0x5a, 12500);
  p[0x5c] = 3;
  prolink::BeatPacket b;
  CHECK(prolink::parseBeat(p, sizeof(p), b));
  CHECK(b.device == 2);
  CHECK(b.beatInBar == 3);
  CHECK(strcmp(b.name, "XDJ-XZ") == 0);
  CHECK(fabs(b.bpm() - 127.5) < 0.01);
  CHECK(!prolink::parseBeat(p, 0x5f, b));       // too short
  p[0] = 0;
  CHECK(!prolink::parseBeat(p, sizeof(p), b));  // bad magic
}

static void testParseStatus() {
  printf("parse CDJ status packet\n");
  uint8_t p[0x11c] = {};
  memcpy(p, prolink::kMagic, 10);
  p[0x0a] = prolink::kTypeCdjStatus;
  p[0x21] = 1;
  p[0x89] = 0x84 | prolink::kFlagPlaying | prolink::kFlagMaster;
  put24(p + 0x8d, 0x100000);
  put16(p + 0x92, 12800);
  p[0xa6] = 4;
  prolink::CdjStatus s;
  CHECK(prolink::parseCdjStatus(p, sizeof(p), s));
  CHECK(s.device == 1 && s.playing() && s.master() && !s.synced());
  CHECK(fabs(s.bpm() - 128.0) < 0.01);
  put16(p + 0x92, prolink::kBpmUnknown);
  CHECK(prolink::parseCdjStatus(p, sizeof(p), s) && s.bpm() == 0.0);
}

static void testKeepAliveRoundTrip() {
  printf("keep-alive build/parse round trip\n");
  uint8_t mac[6] = {0x02, 1, 2, 3, 4, 5}, ip[4] = {169, 254, 77, 77};
  uint8_t p[64];
  size_t n = prolink::buildKeepAlive(p, sizeof(p), "midi-clock", 7, mac, ip, 3);
  CHECK(n == 0x36);
  CHECK(p[0x0a] == 0x06 && p[0x23] == 0x36 && p[0x24] == 7);
  prolink::KeepAlive k;
  CHECK(prolink::parseKeepAlive(p, n, k));
  CHECK(k.device == 7 && memcmp(k.ip, ip, 4) == 0 && memcmp(k.mac, mac, 6) == 0);
  CHECK(strcmp(k.name, "midi-clock") == 0);
  CHECK(prolink::buildKeepAlive(p, 10, "x", 7, mac, ip, 1) == 0);
}

static void testDeviceTableXdjXz() {
  printf("device table: XDJ-XZ decks share one IP\n");
  prolink::DeviceTable t;
  t.setSelf(7);
  uint8_t ip[4] = {169, 254, 10, 20};
  for (uint8_t dev : {1, 2, 33}) {
    prolink::KeepAlive k{};
    k.device = dev;
    memcpy(k.ip, ip, 4);
    t.onKeepAlive(k, 0);
  }
  CHECK(t.knownCount() == 3);
  CHECK(t.countAtIp(ip) == 3);
  CHECK(t.pickFreeNumber(1, 15) == 3);

  prolink::CdjStatus s1{}, s2{};
  s1.device = 1; s1.flags = prolink::kFlagPlaying; s1.bpmX100 = 12400; s1.pitchRaw = 0x100000;
  s2.device = 2; s2.flags = prolink::kFlagPlaying | prolink::kFlagMaster; s2.bpmX100 = 12600; s2.pitchRaw = 0x100000;
  t.onCdjStatus(s1, 100000);
  t.onCdjStatus(s2, 100000);
  CHECK(t.selectMaster(200000) == 2);
  CHECK(t.selectMaster(200000, 1) == 1);  // FOLLOW_DEVICE override
  CHECK(fabs(t.get(1).bpm - 124.0) < 0.01 && fabs(t.get(2).bpm - 126.0) < 0.01);

  // Status goes stale; only deck 1 still beating -> fall back to beats.
  prolink::BeatPacket b{};
  b.device = 1; b.bpmX100 = 12400; b.pitchRaw = 0x100000;
  t.onBeat(b, 3000000);
  CHECK(t.selectMaster(3100000) == 1);
  CHECK(t.isPlaying(1, 3100000));
  CHECK(!t.isPlaying(2, 3100000));

  // Our own number is ignored even if echoed back.
  prolink::KeepAlive me{};
  me.device = 7;
  t.onKeepAlive(me, 0);
  CHECK(!t.get(7).known);
}

int main() {
  testSteadyTempo();
  testJitter();
  testTempoChange();
  testMasterSwitchPhaseJump();
  testStartOnDownbeat();
  testStopAndFreewheel();
  testRelockAfterDropout();
  testParseBeat();
  testParseStatus();
  testKeepAliveRoundTrip();
  testDeviceTableXdjXz();
  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
