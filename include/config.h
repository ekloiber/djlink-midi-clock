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
// Activity LED on core 0: flashes when a beat arrives from the master (raw,
// unsmoothed). -1 = off. Off by default because GPIO7 (the on-board LED)
// carries the bar pulse.
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

// Status indicator: on-board NeoPixel (GPIO21). -1 = off.
//   red blink fast   W5500 not found          red solid    no Ethernet link
//   amber blink      link up, no DJ Link gear blue solid   gear seen, source stopped
//   cyan blink fast  playing, waiting for downbeat to start (also after resync release)
//   magenta solid    resync held
//   dim green        running; green flash each beat, white flash on bar downbeat
//   amber flash      running but beats lost (free-running at last tempo)
#ifndef STATUS_PIXEL_PIN
#define STATUS_PIXEL_PIN 21
#endif
#ifndef STATUS_PIXEL_BRIGHTNESS
#define STATUS_PIXEL_BRIGHTNESS 40   // 0-255; the NeoPixel is very bright at full scale
#endif

// ======================================================================
// GPIO I/O. Values are Arduino-Pico pin numbers, which equal RP2350 GPIO
// numbers. Feather RP2350 header labels -> GPIO:
//   D5=5  D6=6  D9=9  D10=10(Ethernet CS)  D11=11  D13=7(red LED)
//   D24=24  D25=25  A0=26  A1=27  A2=28  A3=29  SDA=2  SCL=3
//   TX=0 RX=1 (MIDI)  SCK=22 MO=23 MI=20 (Ethernet)  GPIO8 = PSRAM CS
// Avoid D4/D12 until their GPIO mapping is confirmed (Adafruit's guide lists
// both as GPIO4). Set any pin to -1 to disable that function.
// All levels are 3.3 V: see docs/eurorack.md for buffering/protection.
// ======================================================================

// ---------- Outputs (driven by core 1, phase-locked to the MIDI clock) ----------
#ifndef CLOCK_OUT_PIN
#define CLOCK_OUT_PIN 5              // D5: x24 clock (24 PPQN), same ticks as MIDI clock
#endif
#ifndef CLOCK_OUT_WIDTH_US
#define CLOCK_OUT_WIDTH_US 0         // 0 = 50% duty (DIN-sync style); else fixed width
#endif
#ifndef CLOCK_OUT_WHEN_STOPPED
#define CLOCK_OUT_WHEN_STOPPED 1     // 1 = x24 keeps running while Run is low (DIN-sync
                                     //     style); 0 = x24 only while Run is high.
                                     // Resync always stops it.
#endif
#ifndef RUN_OUT_PIN
#define RUN_OUT_PIN 6                // D6: Run gate. High while running, low when stopped/held
#endif
#ifndef BEAT_PULSE_PIN
#define BEAT_PULSE_PIN 9             // D9: x1 trigger on every beat (quarter note)
#endif
#ifndef BEAT_PULSE_WIDTH_MS
#define BEAT_PULSE_WIDTH_MS 10
#endif
#ifndef BAR_PULSE_PIN
#define BAR_PULSE_PIN 7              // GPIO7: on-board red LED, also header pin "D13"
#endif
#ifndef BAR_PULSE_WIDTH_MS
#define BAR_PULSE_WIDTH_MS 50        // long enough to see on the LED
#endif
#ifndef PULSE_WHEN_STOPPED
#define PULSE_WHEN_STOPPED 0         // 1 = beat/bar pulses also while Run is low
#endif
#ifndef OUTPUT_ACTIVE_HIGH
#define OUTPUT_ACTIVE_HIGH 1         // 0 if your output buffer inverts
#endif

// ---------- Resync input (read on core 1) ----------
// Active: Run goes low and all clocks stop. Released: outputs stay silent
// until the next bar downbeat of the followed deck, then Run goes high and
// the x24/MIDI clock restart from that downbeat.
#ifndef RESYNC_IN_PIN
#define RESYNC_IN_PIN 11             // D11
#endif
#ifndef RESYNC_ACTIVE_LOW
#define RESYNC_ACTIVE_LOW 1          // 1 = button to GND / NPN-inverted gate (internal pull-up)
#endif
#ifndef RESYNC_MODE
#define RESYNC_MODE 0                // 0 = hold while active (button or gate)
                                     // 1 = latch: each press toggles hold (Pamela's Disco style)
#endif
#ifndef INPUT_DEBOUNCE_MS
#define INPUT_DEBOUNCE_MS 5
#endif

// ---------- Deck selector (5-position switch, read on core 0) ----------
// One pin per position, common to GND (internal pull-ups). If no position (or
// more than one) reads active, FOLLOW_DEVICE above is used.
#ifndef SELECT_ACTIVE_LOW
#define SELECT_ACTIVE_LOW 1
#endif
#ifndef SELECT_PIN_MASTER
#define SELECT_PIN_MASTER 26         // A0: follow tempo master
#endif
#ifndef SELECT_PIN_DECK1
#define SELECT_PIN_DECK1 27          // A1: follow player 1
#endif
#ifndef SELECT_PIN_DECK2
#define SELECT_PIN_DECK2 28          // A2: follow player 2
#endif
#ifndef SELECT_PIN_DECK3
#define SELECT_PIN_DECK3 29          // A3: follow player 3
#endif
#ifndef SELECT_PIN_DECK4
#define SELECT_PIN_DECK4 24          // D24: follow player 4
#endif
#ifndef SELECT_DEBOUNCE_MS
#define SELECT_DEBOUNCE_MS 30        // rotary switches bounce through positions
#endif
