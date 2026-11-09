# Eurorack / DIN-sync I/O

## Pin map (defaults, all in `include/config.h`)

| Function | Header pin | GPIO | Direction | Notes |
|---|---|---|---|---|
| x24 clock (24 PPQN) | D5 | 5 | out | Same ticks as MIDI clock, 50% duty |
| Run gate | D6 | 6 | out | High while running |
| x1 beat trigger | D9 | 9 | out | 10 ms |
| Bar trigger | D13 / on-board LED | 7 | out | 50 ms. Header D13 *is* GPIO7, so the LED mirrors the jack. |
| Resync in | D11 | 11 | in | Active-low, internal pull-up |
| Selector: master | A0 | 26 | in | Active-low, internal pull-up |
| Selector: deck 1 | A1 | 27 | in | |
| Selector: deck 2 | A2 | 28 | in | |
| Selector: deck 3 | A3 | 29 | in | |
| Selector: deck 4 | D24 | 24 | in | |

**Taken:** TX/RX (MIDI), SCK/MO/MI + D10 (Ethernet), GPIO8 (PSRAM CS), GPIO21 (NeoPixel). The firmware fails to compile if a configured pin collides with any of these or with another function.

**Spare:** D25, SDA/SCL (keep these for a future OLED on STEMMA QT), and the HSTX connector GPIO12–19.

**Avoid D4/D12** until their mapping is confirmed. Adafruit's guide lists both as GPIO4.

## Behaviour

- **x24** emits one pulse per MIDI clock. It is phase-locked to the followed deck's beats (≤ ~1 ms error with typical network jitter) and corrected only by stretching or compressing ticks. That means no clocks are ever dropped or added while running.
- **Run** goes high on the first downbeat (beat 1) after the followed deck starts playing, together with MIDI Start and the first x24 pulse of the bar.
  - It goes low as soon as the deck stops. With status packets this is effectively immediate. In listen-only mode, stop is inferred after about 1.5 missed beats.
  - Set `CLOCK_START_ON_DOWNBEAT=0` to start on the first beat instead, which is what Pamela's Disco does.
- **x24 while stopped:** it keeps running by default (`CLOCK_OUT_WHEN_STOPPED=1`), DIN-sync style. Set it to 0 for modules that have no Run input and would otherwise advance while the DJ is stopped.
- **x1 / Bar** fire only while Run is high, unless `PULSE_WHEN_STOPPED=1`.
- **Resync** (`RESYNC_MODE=0`, the default, works as a hold):
  - While the input is active, Run goes low (plus MIDI Stop), and x24, MIDI clock, x1 and Bar all stop.
  - After release, everything stays silent until the next bar downbeat of the followed deck. Then Run goes high and the clocks restart from that downbeat.
  - The engine keeps tracking phase internally while held, so the restart lands on the grid with no lock-in transient.
  - This works with a momentary button (one press = "restart on next bar") or with a gate held from another module.
  - `RESYNC_MODE=1` latches instead: one press stops, the next press re-arms. This is Pamela's Disco style, though theirs restarts on the next *beat* rather than the next bar.
- **Selector:**
  - The firmware expects a single-pole 5-position rotary switch, common to GND.
  - One active position selects tempo master or decks 1–4.
  - No active position (switch not fitted) falls back to `FOLLOW_DEVICE`. Several active at once (passing between positions) keeps the previous choice.
  - Switching to a deck at a different phase realigns within ~1–2 beats without a Start/Stop.
  - Deck numbers are DJ Link player numbers. On an XDJ-XZ, its two decks are expected to be players 1 and 2 (to confirm in bring-up). Linked CDJs take 3/4.

## Interface circuits (not yet designed — guidance)

The RP2350 is 3.3 V and **not 5 V tolerant**. Eurorack signals can be 0–10 V, and miswired patches can put ±12 V on a jack.

- **Outputs:** buffer each output.
  - Simplest option: a 74HCT-series buffer (e.g. 74HCT541) powered from 5 V. HCT inputs accept 3.3 V logic. Follow it with a ~1 kΩ series resistor to the jack.
  - This gives 5 V gates, which suit DIN sync and nearly all Eurorack clock inputs, and protects the MCU from output-to-output patching.
  - An NPN inverter stage also works. If you use one, set `OUTPUT_ACTIVE_HIGH=0`.
- **Resync input:**
  - NPN transistor: base through ~100 kΩ from the jack, ~100 kΩ base-to-ground, a reverse diode across base-emitter, and the collector to GPIO11. This uses the internal pull-up, so the input is active-low, matching the default.
  - A panel push-button from GPIO11 to GND can sit in parallel.
- **Selector:** connect the switch directly (GND common). Keep the leads short or add 100 nF to GND per pin if the panel wiring is long.
- **Power as a module:** you'll need +12 V → 5 V (buck or LDO). Feed the Feather's USB/VBUS pin per Adafruit's power notes, and make sure USB plugged into a computer can't back-feed the rack supply. Pamela's Disco draws 90 mA from +12 V for comparison. The W5500 alone is ~130 mA at 3.3 V, so budget roughly 200–250 mA at 5 V.

## Compared with ALM Pamela's Disco (manual v0.6)

| | Pamela's Disco | This design |
|---|---|---|
| Outputs | x24, Run, Bar, x1 | Same, plus MIDI DIN (+ USB-MIDI) |
| Run start | First beat received | First **downbeat** (configurable) |
| Stop detection | Absence of beats (can overrun 1–2 beats) | Player status play flag (immediate); 1.5-beat timeout in listen-only |
| Resync | Latch toggle; restart on next beat | Hold or latch; restart on next **bar** |
| Deck choice | Master only | Selector: master or deck 1–4 |
| Status LEDs | Link LED + beat LED | Bar LED only (see gaps) |
| Supported gear | Lists CDJ/XDJ players; says all-in-ones (e.g. XDJ-AZ) unlikely to work | Targets XDJ-XZ (Beat Link supports it since 0.6.0); unverified on hardware |
| Configurable | No | Compile-time |

Gaps relative to the Disco, worth considering:
- **Link/status indicator.** The Disco's red Link LED flashes while searching and goes solid when connected. The on-board NeoPixel (GPIO21) could show no link / searching / locked / held for zero extra pins.
- **Beat LED.** The Disco's green LED flashes every beat and stays solid while resync is latched. The x1 output could drive a panel LED through its buffer.
- **Reset trigger.** The Run rising edge doubles as reset for most modules. If a module needs a separate short reset trigger, `kEventStart` is already available to drive one on a spare pin.
