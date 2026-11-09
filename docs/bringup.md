# Bring-up checklist

Do these in order. Each step isolates one layer.

## 0. Toolchain
- [ ] `pio boards rp2350`: confirm the board ID for the Feather RP2350 HSTX and fix `board =` in `platformio.ini` if it differs from `adafruit_feather_rp2350_hstx`.
- [ ] `make -C tests/host` passes.
- [ ] `pio run` builds both envs. The firmware has only been syntax-checked against stubs, so expect small API fixes. Likely spots are the TinyUSB setup and the `Ethernet.begin` overloads.

## 1. SPI / W5500
- [ ] Confirm that `SPI` in Arduino-Pico maps to the Feather's SCK/MO/MI header pins on this board. If it doesn't, set the pins with `SPI.setSCK/setTX/setRX` before `net.begin()`.
- [ ] Serial shows an IP. A fast-blinking LED means no W5500 was found.

## 2. Passive receive (no XDJ involvement needed beyond playing)
- [ ] Temporarily set `-DANNOUNCE_AS_PLAYER=0`.
- [ ] Play a track. The status line should show `master=N` and the BPM, and the LED should flash on beats with a long flash on beat 1.

## 3. Virtual player
- [ ] Re-enable announce. The XDJ-XZ link screen should list `midi-clock`.
- [ ] Status packets arrive: `play=` follows the play button, and pressing MASTER on the other deck changes `master=`.
- [ ] Capture with Wireshark through a mirrored switch port or a laptop on the same switch. Check every ⚠️ offset in `protocol.md`.

## 4. Clock on DIN
- [ ] Connect DIN out to a MIDI interface. In a MIDI monitor, confirm 0xF8 at 24 PPQN, Start on beat 1, and Stop on pause.
- [ ] Have a DAW sync to the clock and compare its BPM with the XDJ display at several pitch settings.
- [ ] Measure audio-vs-clock offset by recording XDJ output and a click from the DAW. Trim with `CLOCK_OFFSET_US`.

## 4b. Bar pulse
- [ ] With a correctly gridded track playing, the on-board LED (D7) blinks on beat 1 only, in time with the XDJ's beat counter.
- [ ] Switch masters to a deck at a different bar position. The LED should re-sync to the new deck's beat 1 within about 1–2 beats.
- [ ] If you move it to an external pin, scope the pulse against a DIN clock decode. The rising edge should coincide with the 0xF8 that starts the bar.

## 4c. Eurorack I/O (scope or logic analyser first, then buffered jacks)
- [ ] D5 shows 24 pulses per beat at 50% duty, edge-aligned with the DIN 0xF8 bytes.
- [ ] D6 Run rises on the first downbeat after play, before the first D5 edge of that bar, and falls on stop.
- [ ] D9 x1 pulses on every beat; Bar (D13/LED) on beat 1 only.
- [ ] Resync on D11 (short to GND): Run drops and D5/D9/Bar stop immediately. After release, everything restarts on the next bar downbeat. Repeat with `RESYNC_MODE=1`.
- [ ] Selector (A0–A3, D24 to GND one at a time): the serial log prints the new follow target, and the clock follows that deck's tempo/transport. Also confirm which player numbers the XDJ-XZ's decks use.
- [ ] Clock a Eurorack sequencer from x24 + Run and confirm it stays bar-aligned across stop/start and resync.

## 5. Edge cases
- [ ] Switch master between decks mid-track. The clock should realign within about 1–2 beats with no Start/Stop.
- [ ] Pull the Ethernet cable. The clock should free-run, then relock when it's reconnected.
- [ ] Track loops, hot cues, and reverse play. Watch for spurious Stop/Start.
- [ ] Power-cycle the XDJ while the box is running.

## 6. USB-MIDI (optional)
- [ ] Flash `feather_rp2350_usbmidi`. The device should enumerate as MIDI and as serial, and the DIN timing from step 4 should be unchanged.
