// Build-time configuration. Override any of these with -D flags in platformio.ini.
#pragma once

// ---------- Identity on the DJ Link network ----------
#ifndef DEVICE_NAME
#define DEVICE_NAME "midi-clock"     // max 20 chars, shown on players' link screens
#endif
#ifndef VCDJ_DEVICE_NUMBER
#define VCDJ_DEVICE_NUMBER 0         // 0 = auto-pick unused number in [VCDJ_MIN, VCDJ_MAX]
#endif
#ifndef VCDJ_MIN
#define VCDJ_MIN 7                   // 5/6 avoided by default: CDJ-3000s can use them
#endif
#ifndef VCDJ_MAX
#define VCDJ_MAX 15
#endif
#ifndef ANNOUNCE_AS_PLAYER
#define ANNOUNCE_AS_PLAYER 1         // 0 = listen-only (beats only, no status/master info)
#endif
#ifndef DISCOVERY_WINDOW_MS
#define DISCOVERY_WINDOW_MS 3000     // listen before choosing a device number
#endif
#ifndef KEEPALIVE_INTERVAL_MS
#define KEEPALIVE_INTERVAL_MS 1500
#endif

// ---------- Network ----------
#ifndef ETH_CS_PIN
#define ETH_CS_PIN 10                // Ethernet FeatherWing default CS
#endif
#ifndef USE_DHCP
#define USE_DHCP 1                   // try DHCP first, fall back to static/link-local
#endif
#ifndef DHCP_TIMEOUT_MS
#define DHCP_TIMEOUT_MS 4000
#endif
// Fallback address. Pioneer gear self-assigns 169.254.x.x when there is no DHCP.
#ifndef STATIC_IP
#define STATIC_IP 169, 254, 77, 77
#endif
#ifndef STATIC_MASK
#define STATIC_MASK 255, 255, 0, 0
#endif

// ---------- What to follow ----------
#ifndef FOLLOW_DEVICE
#define FOLLOW_DEVICE 0              // 0 = tempo master; 1..4 = always follow that deck
#endif

// ---------- Clock behaviour ----------
#ifndef CLOCK_OFFSET_US
#define CLOCK_OFFSET_US 0            // + delays clock, - sends it earlier (latency comp)
#endif
#ifndef CLOCK_PHASE_GAIN
#define CLOCK_PHASE_GAIN 0.3
#endif
#ifndef CLOCK_MAX_SLEW
#define CLOCK_MAX_SLEW 0.03
#endif
#ifndef CLOCK_RUN_WHEN_STOPPED
#define CLOCK_RUN_WHEN_STOPPED 1     // keep sending 0xF8 at last tempo when stopped
#endif
#ifndef CLOCK_SEND_TRANSPORT
#define CLOCK_SEND_TRANSPORT 1       // Start/Stop follow the master deck's play state
#endif
#ifndef CLOCK_START_ON_DOWNBEAT
#define CLOCK_START_ON_DOWNBEAT 1
#endif
#ifndef CLOCK_LOSE_LOCK_MS
#define CLOCK_LOSE_LOCK_MS 2000
#endif

// ---------- MIDI outputs ----------
// Primary output: 5-pin DIN via the MIDI FeatherWing on Serial1 (UART TX).
#ifndef MIDI_DIN_BAUD
#define MIDI_DIN_BAUD 31250
#endif
// Secondary output: USB-MIDI (TinyUSB). Built only in the *_usbmidi env.
// USB bytes are relayed through core 0 so they can never delay DIN output.
#ifndef ENABLE_USB_MIDI
#define ENABLE_USB_MIDI 0
#endif

// ---------- Diagnostics ----------
#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL 1               // status line once per second over USB serial
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN LED_BUILTIN   // red LED (GPIO 7) on Feather RP2350
#endif
