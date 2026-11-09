// W5500 (Ethernet FeatherWing) setup and the three DJ Link UDP sockets.
#pragma once

#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

class DjLinkNet {
 public:
  // Brings up the W5500 and binds ports 50000-50002. Returns false if the
  // chip is missing.
  bool begin(const uint8_t mac[6]);

  // Non-blocking. Checks the beat port first since it is the most
  // time-critical. Returns packet length (0 if none) and the local port it
  // arrived on; rxUs is stamped as soon as the packet is seen.
  size_t poll(uint8_t* buf, size_t cap, uint16_t& port, uint64_t& rxUs);

  void broadcast(uint16_t port, const uint8_t* data, size_t len);

  bool linkUp() const;
  IPAddress localIP() const { return Ethernet.localIP(); }
  IPAddress broadcastIP() const { return bcast_; }
  bool usingDhcp() const { return dhcp_; }

 private:
  size_t readFrom(EthernetUDP& s, uint16_t port, uint8_t* buf, size_t cap,
                  uint16_t& outPort, uint64_t& rxUs);

  EthernetUDP announce_, beat_, status_;
  IPAddress bcast_;
  bool dhcp_ = false;
};
