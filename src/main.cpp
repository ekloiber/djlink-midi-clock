// DJ Link -> MIDI clock for Feather RP2350 + Ethernet FeatherWing + MIDI FeatherWing.
//
// Core 0: Ethernet (W5500), Pro DJ Link parsing, master/transport tracking,
//         keep-alives, USB-MIDI relay, diagnostics.
// Core 1: MIDI clock generation and 5-pin DIN output on Serial1. Nothing else
//         runs here, so DIN timing is never disturbed by networking or USB.

#include <Arduino.h>

#include "config.h"

#include <hardware/sync.h>
#include <pico/unique_id.h>
#include <pico/util/queue.h>

#if ENABLE_USB_MIDI
#include <Adafruit_TinyUSB.h>
#endif

#include "clock_engine.h"
#include "device_table.h"
#include "events.h"
#include "net.h"
#include "prolink_packets.h"

// ---------------------------------------------------------------- shared state
static queue_t g_events;          // core0 -> core1: ClockEvent
#if ENABLE_USB_MIDI
static queue_t g_usbBytes;        // core1 -> core0: MIDI bytes for USB
#endif
static volatile bool g_ready = false;
static ClockStatus g_status = {};
static volatile uint32_t g_droppedEvents = 0;

// ---------------------------------------------------------------- core 0 state
static DjLinkNet net;
static prolink::DeviceTable devices;
static uint8_t g_mac[6];
static uint8_t g_self = 0;
static uint8_t g_master = 0;
static bool g_masterPlaying = false;
static uint32_t g_ledOffAt = 0;
#if ENABLE_USB_MIDI
static Adafruit_USBD_MIDI usbMidi;
#endif

static void pushEvent(const ClockEvent& ev) {
  if (!queue_try_add(&g_events, &ev)) g_droppedEvents++;
}

static void makeMac(uint8_t mac[6]) {
  pico_unique_board_id_t id;
  pico_get_unique_board_id(&id);
  mac[0] = 0x02;  // locally administered, unicast
  for (int i = 1; i < 6; ++i) mac[i] = id.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 6 + i];
}

static void handlePacket(const uint8_t* buf, size_t len, uint16_t port, uint64_t rxUs) {
  const int64_t now = int64_t(rxUs);
  switch (prolink::packetType(buf, len)) {
    case prolink::kTypeBeat: {
      prolink::BeatPacket b;
      if (port != prolink::kPortBeat || !prolink::parseBeat(buf, len, b)) return;
      devices.onBeat(b, now);
      uint8_t master = devices.selectMaster(now, FOLLOW_DEVICE);
      if (master && b.device == master && b.bpm() > 0) {
        ClockEvent ev{};
        ev.type = kEvBeat;
        ev.device = b.device;
        ev.beatInBar = b.beatInBar;
        ev.bpm = float(b.bpm());
        ev.tUs = now;
        pushEvent(ev);
        digitalWrite(STATUS_LED_PIN, HIGH);
        g_ledOffAt = millis() + (b.beatInBar == 1 ? 120 : 30);
      }
      return;
    }
    case prolink::kTypeCdjStatus: {
      prolink::CdjStatus s;
      if (prolink::parseCdjStatus(buf, len, s)) devices.onCdjStatus(s, now);
      return;
    }
    case prolink::kTypeMixerStatus: {
      prolink::MixerStatus s;
      if (prolink::parseMixerStatus(buf, len, s)) devices.onMixerStatus(s, now);
      return;
    }
    case prolink::kTypeKeepAlive: {
      prolink::KeepAlive k;
      if (port == prolink::kPortAnnounce && prolink::parseKeepAlive(buf, len, k))
        devices.onKeepAlive(k, now);
      return;
    }
    default:
      return;
  }
}

static void pumpNetwork() {
  static uint8_t buf[1500];
  uint16_t port;
  uint64_t rxUs;
  for (int i = 0; i < 8; ++i) {  // bounded so housekeeping still runs
    size_t n = net.poll(buf, sizeof(buf), port, rxUs);
    if (!n) break;
    handlePacket(buf, n, port, rxUs);
  }
}

static void sendKeepAlive() {
#if ANNOUNCE_AS_PLAYER
  uint8_t pkt[prolink::kKeepAliveLen];
  IPAddress ip = net.localIP();
  uint8_t ipb[4] = {ip[0], ip[1], ip[2], ip[3]};
  int peers = devices.knownCount() + 1;
  size_t n = prolink::buildKeepAlive(pkt, sizeof(pkt), DEVICE_NAME, g_self, g_mac, ipb,
                                     uint8_t(peers > 255 ? 255 : peers));
  if (n) net.broadcast(prolink::kPortAnnounce, pkt, n);
#endif
}

static void updateMasterAndTransport(int64_t now) {
  uint8_t m = devices.selectMaster(now, FOLLOW_DEVICE);
  if (m != g_master) {
    if (m && g_master) {
      ClockEvent ev{};
      ev.type = kEvResync;
      ev.device = m;
      ev.tUs = now;
      pushEvent(ev);
    }
    g_master = m;
  }
  bool playing = m ? devices.isPlaying(m, now) : false;
  if (playing != g_masterPlaying) {
    g_masterPlaying = playing;
    ClockEvent ev{};
    ev.type = kEvTransport;
    ev.device = m;
    ev.playing = playing;
    ev.tUs = now;
    pushEvent(ev);
  }
}

static bool readStatus(ClockStatus& out) {
  for (int tries = 0; tries < 4; ++tries) {
    uint32_t s1 = g_status.seq;
    if (s1 & 1) continue;
    __dmb();
    out.bpm = g_status.bpm;
    out.phaseErrUs = g_status.phaseErrUs;
    out.locked = g_status.locked;
    out.running = g_status.running;
    out.ticking = g_status.ticking;
    __dmb();
    if (g_status.seq == s1) return true;
  }
  return false;
}

static void printStatus() {
#if DEBUG_SERIAL
  ClockStatus cs{};
  readStatus(cs);
  const auto& md = devices.get(g_master);
  IPAddress ip = net.localIP();
  Serial.printf("[%s] ip=%u.%u.%u.%u self=%u devs=%d master=%u(%s) play=%d | clock %.2f bpm "
                "lock=%d run=%d err=%.2fms drop=%lu\n",
                net.linkUp() ? "link" : "NO LINK", ip[0], ip[1], ip[2], ip[3], g_self,
                devices.knownCount(), g_master, g_master ? md.name : "-", g_masterPlaying,
                cs.bpm, cs.locked, cs.running, cs.phaseErrUs / 1000.0f,
                (unsigned long)g_droppedEvents);
#endif
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  queue_init(&g_events, sizeof(ClockEvent), 64);
#if ENABLE_USB_MIDI
  queue_init(&g_usbBytes, 1, 512);
  if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
  usbMidi.setStringDescriptor(DEVICE_NAME);
  usbMidi.begin();
  if (TinyUSBDevice.mounted()) {  // re-enumerate so the MIDI interface appears
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
#endif
  g_ready = true;  // core 1 may start consuming now

  Serial.begin(115200);
  makeMac(g_mac);

  if (!net.begin(g_mac)) {
    // No W5500 found: fast blink forever. Clock still free-runs on core 1.
    for (;;) {
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      delay(100);
    }
  }

  // Listen before claiming a device number so we don't collide.
  uint32_t until = millis() + DISCOVERY_WINDOW_MS;
  while (int32_t(millis() - until) < 0) pumpNetwork();

  g_self = VCDJ_DEVICE_NUMBER ? VCDJ_DEVICE_NUMBER : devices.pickFreeNumber(VCDJ_MIN, VCDJ_MAX);
  if (!g_self) g_self = VCDJ_MAX;
  devices.setSelf(g_self);
  sendKeepAlive();
}

void loop() {
  pumpNetwork();

  const uint32_t ms = millis();
  const int64_t now = int64_t(time_us_64());
  updateMasterAndTransport(now);

  static uint32_t nextKeepAlive = 0, nextHousekeeping = 0;
  if (int32_t(ms - nextKeepAlive) >= 0) {
    nextKeepAlive = ms + KEEPALIVE_INTERVAL_MS;
    sendKeepAlive();
  }
  if (int32_t(ms - nextHousekeeping) >= 0) {
    nextHousekeeping = ms + 1000;
    devices.expire(now);
    if (net.usingDhcp()) Ethernet.maintain();
    printStatus();
  }
  if (g_ledOffAt && int32_t(ms - g_ledOffAt) >= 0) {
    digitalWrite(STATUS_LED_PIN, LOW);
    g_ledOffAt = 0;
  }

#if ENABLE_USB_MIDI
#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif
  uint8_t b;
  while (queue_try_remove(&g_usbBytes, &b)) usbMidi.write(b);
#endif
}

// ===================================================================== core 1
static midiclock::ClockEngine engine;

void setup1() {
  while (!g_ready) tight_loop_contents();
  Serial1.begin(MIDI_DIN_BAUD);  // MIDI FeatherWing on TX/RX

  midiclock::ClockConfig cfg;
  cfg.offsetUs = CLOCK_OFFSET_US;
  cfg.phaseGain = CLOCK_PHASE_GAIN;
  cfg.maxSlew = CLOCK_MAX_SLEW;
  cfg.loseLockUs = int64_t(CLOCK_LOSE_LOCK_MS) * 1000;
  cfg.runWhenStopped = CLOCK_RUN_WHEN_STOPPED;
  cfg.sendTransport = CLOCK_SEND_TRANSPORT;
  cfg.startOnDownbeat = CLOCK_START_ON_DOWNBEAT;
  engine.configure(cfg);
}

static void publishStatus() {
  g_status.seq = g_status.seq + 1;  // odd: writing
  __dmb();
  g_status.bpm = float(engine.bpm());
  g_status.phaseErrUs = float(engine.lastPhaseErrorUs());
  g_status.locked = engine.locked();
  g_status.running = engine.running();
  g_status.ticking = engine.ticking();
  __dmb();
  g_status.seq = g_status.seq + 1;
}

void loop1() {
  ClockEvent ev;
  while (queue_try_remove(&g_events, &ev)) {
    switch (ev.type) {
      case kEvBeat:      engine.onBeat(ev.tUs, ev.bpm, ev.beatInBar); break;
      case kEvTransport: engine.onTransport(ev.playing, ev.tUs); break;
      case kEvResync:    engine.requestResync(); break;
    }
  }

  uint8_t out[4];
  size_t n = engine.poll(int64_t(time_us_64()), out, sizeof(out));
  if (n) {
    Serial1.write(out, n);  // DIN first; real-time bytes leave immediately
#if ENABLE_USB_MIDI
    for (size_t i = 0; i < n; ++i) queue_try_add(&g_usbBytes, &out[i]);
#endif
  }

  static uint64_t nextPublish = 0;
  uint64_t t = time_us_64();
  if (t >= nextPublish) {
    nextPublish = t + 100000;
    publishStatus();
  }
}
