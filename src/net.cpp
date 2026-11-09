#include "net.h"

#include <SPI.h>

#include "config.h"
#include "prolink_packets.h"

bool DjLinkNet::begin(const uint8_t mac[6]) {
  Ethernet.init(ETH_CS_PIN);
  uint8_t macCopy[6];
  memcpy(macCopy, mac, 6);

  dhcp_ = false;
#if USE_DHCP
  if (Ethernet.begin(macCopy, DHCP_TIMEOUT_MS, 1000)) dhcp_ = true;
#endif
  if (Ethernet.hardwareStatus() == EthernetNoHardware) return false;
  if (!dhcp_) {
    IPAddress ip(STATIC_IP);
    IPAddress mask(STATIC_MASK);
    IPAddress none(0, 0, 0, 0);
    Ethernet.begin(macCopy, ip, none, none, mask);
  }

  IPAddress ip = Ethernet.localIP();
  IPAddress mask = Ethernet.subnetMask();
  for (int i = 0; i < 4; ++i) bcast_[i] = ip[i] | uint8_t(~mask[i]);

  announce_.begin(prolink::kPortAnnounce);
  beat_.begin(prolink::kPortBeat);
  status_.begin(prolink::kPortStatus);
  return true;
}

bool DjLinkNet::linkUp() const { return Ethernet.linkStatus() == LinkON; }

size_t DjLinkNet::readFrom(EthernetUDP& s, uint16_t port, uint8_t* buf, size_t cap,
                           uint16_t& outPort, uint64_t& rxUs) {
  int len = s.parsePacket();
  if (len <= 0) return 0;
  rxUs = time_us_64();
  outPort = port;
  // Any bytes beyond cap are discarded by the next parsePacket().
  int n = s.read(buf, cap);
  return n > 0 ? size_t(n) : 0;
}

size_t DjLinkNet::poll(uint8_t* buf, size_t cap, uint16_t& port, uint64_t& rxUs) {
  size_t n = readFrom(beat_, prolink::kPortBeat, buf, cap, port, rxUs);
  if (n) return n;
  n = readFrom(status_, prolink::kPortStatus, buf, cap, port, rxUs);
  if (n) return n;
  return readFrom(announce_, prolink::kPortAnnounce, buf, cap, port, rxUs);
}

void DjLinkNet::broadcast(uint16_t port, const uint8_t* data, size_t len) {
  // Send from the socket bound to this port so the source port matches.
  EthernetUDP& s = port == prolink::kPortAnnounce ? announce_
                 : port == prolink::kPortBeat     ? beat_
                                                  : status_;
  s.beginPacket(bcast_, port);
  s.write(data, len);
  s.endPacket();
}
