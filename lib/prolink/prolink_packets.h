// Pro DJ Link packet parsing (the subset needed for tempo/beat following).
//
// Portable C++ with no Arduino dependencies so it can be unit-tested on a host.
// Offsets follow Deep Symmetry's DJ Link packet analysis and Beat Link sources.
// See docs/protocol.md for which offsets are verified vs. still to confirm on
// real hardware.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace prolink {

constexpr uint16_t kPortAnnounce = 50000;  // keep-alives / device discovery
constexpr uint16_t kPortBeat     = 50001;  // beat packets (broadcast)
constexpr uint16_t kPortStatus   = 50002;  // CDJ / mixer status (unicast to known devices)

extern const uint8_t kMagic[10];  // "Qspt1WmJOL"

enum PacketType : uint8_t {
  kTypeKeepAlive   = 0x06,  // port 50000
  kTypeCdjStatus   = 0x0a,  // port 50002
  kTypeBeat        = 0x28,  // port 50001
  kTypeMixerStatus = 0x29,  // port 50002
};

constexpr uint32_t kPitchNormal  = 0x100000;  // raw pitch value for +0.00%
constexpr uint16_t kBpmUnknown   = 0xffff;    // reported when no track is loaded
constexpr size_t   kNameLen      = 20;
constexpr size_t   kKeepAliveLen = 0x36;      // 54 bytes

// CDJ status flag byte (F, offset 0x89) bits.
constexpr uint8_t kFlagPlaying = 0x40;
constexpr uint8_t kFlagMaster  = 0x20;
constexpr uint8_t kFlagSynced  = 0x10;
constexpr uint8_t kFlagOnAir   = 0x08;

bool hasMagic(const uint8_t* p, size_t len);
int  packetType(const uint8_t* p, size_t len);  // -1 if not a DJ Link packet

// Track BPM x100 combined with raw pitch -> BPM actually being played.
// Returns 0 when the tempo is meaningless (no track loaded).
double effectiveBpm(uint16_t bpmX100, uint32_t pitchRaw);

struct BeatPacket {
  uint8_t  device;
  char     name[kNameLen + 1];
  uint32_t nextBeatMs;  // ms until next beat at +0% pitch (0xffffffff = past end)
  uint32_t pitchRaw;
  uint16_t bpmX100;
  uint8_t  beatInBar;   // 1..4
  double   bpm() const { return effectiveBpm(bpmX100, pitchRaw); }
};

struct CdjStatus {
  uint8_t  device;
  char     name[kNameLen + 1];
  uint8_t  flags;
  uint32_t pitchRaw;
  uint16_t bpmX100;
  uint32_t beatNumber;  // 0xffffffff when unknown
  uint8_t  beatInBar;
  uint8_t  masterHandoff;  // 0xff unless handing master role to another device
  bool playing() const { return flags & kFlagPlaying; }
  bool master()  const { return flags & kFlagMaster; }
  bool synced()  const { return flags & kFlagSynced; }
  double bpm() const { return effectiveBpm(bpmX100, pitchRaw); }
};

struct MixerStatus {
  uint8_t  device;
  char     name[kNameLen + 1];
  uint8_t  flags;
  uint16_t bpmX100;
  uint8_t  beatInBar;
  bool master() const { return flags & kFlagMaster; }
};

struct KeepAlive {
  uint8_t device;
  char    name[kNameLen + 1];
  uint8_t mac[6];
  uint8_t ip[4];
};

bool parseBeat(const uint8_t* p, size_t len, BeatPacket& out);
bool parseCdjStatus(const uint8_t* p, size_t len, CdjStatus& out);
bool parseMixerStatus(const uint8_t* p, size_t len, MixerStatus& out);
bool parseKeepAlive(const uint8_t* p, size_t len, KeepAlive& out);

// Builds the keep-alive we broadcast on port 50000 so players treat us as a
// (virtual) player and start sending us status packets. Returns bytes written
// (kKeepAliveLen) or 0 if cap is too small.
size_t buildKeepAlive(uint8_t* out, size_t cap, const char* name, uint8_t device,
                      const uint8_t mac[6], const uint8_t ip[4], uint8_t peersSeen);

}  // namespace prolink
