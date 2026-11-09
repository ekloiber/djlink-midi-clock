# djlink-midi-clock

Standalone box that joins a Pioneer/AlphaTheta **Pro DJ Link** network (XDJ-XZ first) and outputs **MIDI clock** locked to the tempo master, on 5-pin DIN (primary) and optionally USB-MIDI.

> Status: **draft / pre-hardware.** The clock engine and protocol parser pass host tests; the firmware has not yet run on hardware. Work through [docs/bringup.md](docs/bringup.md) before relying on it.

## Hardware

| Part | Role |
|---|---|
| Adafruit Feather RP2350 with HSTX Port, 8 MB PSRAM (#6000) | MCU. Core 0: network. Core 1: clock + DIN. PSRAM/HSTX unused for now. |
| Adafruit Ethernet FeatherWing (W5500) | Pro DJ Link over wired Ethernet. CS on pin 10. |
| Adafruit MIDI FeatherWing kit | 5-pin DIN out (and in, unused) on Serial1 TX/RX. |

Stack all three. The Ethernet wing uses SPI + D10; the MIDI wing uses TX/RX. No pin conflicts. Power via USB-C.

## How it works

```
XDJ-XZ ──Ethernet──► W5500 ──SPI──► Core 0                              Core 1
                                    ├─ parse beat (50001) / status (50002) / keep-alive (50000)
                                    ├─ device table keyed by device number (XDJ-XZ: 2 decks, 1 IP)
                                    ├─ pick tempo master, track play state
                                    └─ queue: Beat / Transport / Resync ──► ClockEngine ──► Serial1 ──► DIN
                                                                                 │
                                    USB-MIDI (optional) ◄── byte queue ◄─────────┘
```

- **Virtual player:** after a 3 s listen it claims an unused device number (7–15 by default) and broadcasts keep-alives, so players send it status packets. That is how it learns which deck is master and whether it's playing. Set `ANNOUNCE_AS_PLAYER=0` for listen-only (beat packets only; follows the lowest-numbered deck that's beating).
- **Clock engine (core 1):** 24 PPQN. Phase is corrected only by stretching or compressing tick intervals, never by dropping or inserting clocks, so downstream gear that counts clocks stays bar-aligned.
  - Normal tracking: 30% of phase error per beat, ±3% max period change. With ±2 ms simulated network jitter, phase error stays ≤1.1 ms and clock-to-clock jitter stays ~0.04 ms.
  - Master change / relock / big error: fast regime (full correction within ~1 beat, ±50% period).
  - Start waits for beat 1 of the bar; Stop is sent when the master stops. Clock keeps running at the last tempo when stopped (configurable).
- **USB-MIDI** gets the same bytes via a queue drained on core 0, so USB can never delay DIN.

## Build

PlatformIO with the Arduino-Pico core:

```sh
pio run -e feather_rp2350                  # DIN only (default)
pio run -e feather_rp2350_usbmidi          # DIN + USB-MIDI
pio run -e feather_rp2350 -t upload        # hold BOOTSEL while plugging in if needed
pio device monitor                         # 1 Hz status line
```

Arduino IDE also works. Install the Philhower RP2040/RP2350 core, select "Adafruit Feather RP2350", and install the `Ethernet` library. For USB-MIDI, select the Adafruit TinyUSB stack and add `#define ENABLE_USB_MIDI 1` before `config.h` is included.

Host tests (no hardware needed):

```sh
make -C tests/host
```

## Configuration

All settings are in [include/config.h](include/config.h) and can be overridden with `-D` in `platformio.ini`. The ones you're most likely to touch:

| Setting | Default | Notes |
|---|---|---|
| `FOLLOW_DEVICE` | 0 | 0 = tempo master; 1–4 pins a deck |
| `CLOCK_OFFSET_US` | 0 | Latency trim. Negative sends clock earlier |
| `CLOCK_RUN_WHEN_STOPPED` | 1 | Keep clocking at last tempo when stopped |
| `CLOCK_SEND_TRANSPORT` | 1 | Start/Stop follow master play state |
| `CLOCK_START_ON_DOWNBEAT` | 1 | Hold Start until beat 1 |
| `VCDJ_DEVICE_NUMBER` | 0 | 0 = auto in `VCDJ_MIN..VCDJ_MAX` |
| `USE_DHCP` / `STATIC_IP` | 1 / 169.254.77.77 | DHCP first, link-local fallback |

## Layout

```
include/config.h             build-time settings
src/main.cpp                 core 0 + core 1 entry points
src/net.{h,cpp}              W5500 + DJ Link sockets
src/events.h                 inter-core messages
lib/prolink/                 packet parsing, keep-alive builder, device table (portable)
lib/clock_engine/            PLL clock generator (portable)
tests/host/                  host unit + simulation tests (ASan/UBSan)
docs/protocol.md             offsets used and what's verified
docs/bringup.md              hardware bring-up checklist
```

## Known gaps / roadmap

- Packet offsets for CDJ status, mixer status and keep-alive need confirming against an XDJ-XZ capture. See [docs/protocol.md](docs/protocol.md).
- No Song Position Pointer. Bar alignment relies on Start at the downbeat.
- No status display. HSTX or an I2C OLED on STEMMA QT would fit. The onboard NeoPixel is unused.
- No runtime config (web UI / SysEx). Everything is compile-time.
- DIN MIDI-in on the FeatherWing is unused (could take a tap-tempo or external transport).

## Prior art and credits

- Protocol knowledge comes from Deep Symmetry's [DJ Link packet analysis](https://djl-analysis.deepsymmetry.org/), [dysentery](https://github.com/Deep-Symmetry/dysentery) and [Beat Link](https://github.com/Deep-Symmetry/beat-link). This repo contains no code from those projects.
- [Beat Link Trigger](https://github.com/Deep-Symmetry/beat-link-trigger) already does DJ Link → MIDI clock on a computer, and [xdj-midi-beat](https://github.com/strikeout/xdj-midi-beat) is a host-side bridge with a similar goal. Use either one as a reference when debugging.
- Not affiliated with Pioneer DJ / AlphaTheta. Pro DJ Link is undocumented, and firmware updates can change it.

License: TBD (choose before publishing).
