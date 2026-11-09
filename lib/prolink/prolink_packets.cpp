#include "prolink_packets.h"

#include <string.h>

namespace prolink {

const uint8_t kMagic[10] = {0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c};

namespace {

uint16_t be16(const uint8_t* p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint32_t be24(const uint8_t* p) { return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2]; }
uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void copyName(char* dst, const uint8_t* src) {
  memcpy(dst, src, kNameLen);
  dst[kNameLen] = '\0';
}

}  // namespace

bool hasMagic(const uint8_t* p, size_t len) {
  return p && len >= sizeof(kMagic) + 1 && memcmp(p, kMagic, sizeof(kMagic)) == 0;
}

int packetType(const uint8_t* p, size_t len) {
  return hasMagic(p, len) ? p[0x0a] : -1;
}

double effectiveBpm(uint16_t bpmX100, uint32_t pitchRaw) {
  if (bpmX100 == kBpmUnknown || bpmX100 == 0) return 0.0;
  return (bpmX100 / 100.0) * (double(pitchRaw) / double(kPitchNormal));
}

// Beat packet, port 50001, type 0x28, 0x60 bytes.
// Offsets match Beat Link's Beat.java.
bool parseBeat(const uint8_t* p, size_t len, BeatPacket& out) {
  if (len < 0x60 || packetType(p, len) != kTypeBeat) return false;
  copyName(out.name, p + 0x0b);
  out.device     = p[0x21];
  out.nextBeatMs = be32(p + 0x24);
  out.pitchRaw   = be24(p + 0x55);
  out.bpmX100    = be16(p + 0x5a);
  out.beatInBar  = p[0x5c];
  return true;
}

// CDJ status, port 50002, type 0x0a. Nexus players send 0xd4 bytes, newer
// players send longer packets; we only read fields inside the first 0xd4.
bool parseCdjStatus(const uint8_t* p, size_t len, CdjStatus& out) {
  if (len < 0xd4 || packetType(p, len) != kTypeCdjStatus) return false;
  copyName(out.name, p + 0x0b);
  out.device        = p[0x21];
  out.flags         = p[0x89];
  out.pitchRaw      = be24(p + 0x8d);
  out.bpmX100       = be16(p + 0x92);
  out.masterHandoff = p[0x9f];
  out.beatNumber    = be32(p + 0xa0);
  out.beatInBar     = p[0xa6];
  return true;
}

// Mixer status, port 50002, type 0x29, 0x38 bytes.
bool parseMixerStatus(const uint8_t* p, size_t len, MixerStatus& out) {
  if (len < 0x38 || packetType(p, len) != kTypeMixerStatus) return false;
  copyName(out.name, p + 0x0b);
  out.device    = p[0x21];
  out.flags     = p[0x27];
  out.bpmX100   = be16(p + 0x2e);
  out.beatInBar = p[0x37];
  return true;
}

// Keep-alive, port 50000, type 0x06, 0x36 bytes. Note the name starts at 0x0c
// on port 50000 (vs 0x0b on 50001/50002).
bool parseKeepAlive(const uint8_t* p, size_t len, KeepAlive& out) {
  if (len < kKeepAliveLen || packetType(p, len) != kTypeKeepAlive) return false;
  copyName(out.name, p + 0x0c);
  out.device = p[0x24];
  memcpy(out.mac, p + 0x26, 6);
  memcpy(out.ip, p + 0x2c, 4);
  return true;
}

size_t buildKeepAlive(uint8_t* out, size_t cap, const char* name, uint8_t device,
                      const uint8_t mac[6], const uint8_t ip[4], uint8_t peersSeen) {
  if (cap < kKeepAliveLen) return 0;
  memset(out, 0, kKeepAliveLen);
  memcpy(out, kMagic, sizeof(kMagic));
  out[0x0a] = kTypeKeepAlive;
  out[0x0b] = 0x00;
  if (name) {
    size_t n = strlen(name);
    memcpy(out + 0x0c, name, n > kNameLen ? kNameLen : n);
  }
  out[0x20] = 0x01;
  out[0x21] = 0x02;
  out[0x22] = 0x00;
  out[0x23] = kKeepAliveLen;  // length
  out[0x24] = device;
  out[0x25] = 0x01;           // device type: player
  memcpy(out + 0x26, mac, 6);
  memcpy(out + 0x2c, ip, 4);
  out[0x30] = peersSeen ? peersSeen : 1;
  out[0x34] = 0x01;
  return kKeepAliveLen;
}

}  // namespace prolink
