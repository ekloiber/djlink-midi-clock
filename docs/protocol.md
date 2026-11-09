# Pro DJ Link subset used

All packets start with the 10-byte magic `51 73 70 74 31 57 6d 4a 4f 4c` ("Qspt1WmJOL"). Byte `0x0a` is the packet type. Multi-byte fields are big-endian.

Device names start at `0x0c` on port 50000 and at `0x0b` on 50001/50002.

**Confidence**
- ✅ = matches Beat Link source.
- ⚠️ = from the DJ Link analysis / memory. **Confirm against a Wireshark capture of your XDJ-XZ** (dysentery ships a Wireshark dissector).

## Beat — UDP 50001, type `0x28`, 0x60 bytes (broadcast) ✅

| Offset | Size | Field |
|---|---|---|
| 0x21 | 1 | device number |
| 0x24 | 4 | ms to next beat at +0% pitch |
| 0x28…0x38 | 4 each | 2nd beat, next bar, 4th beat, 2nd bar, 8th beat |
| 0x55 | 3 | pitch, `0x100000` = +0% |
| 0x5a | 2 | track BPM × 100 |
| 0x5c | 1 | beat within bar (1–4) |

Effective BPM = `bpm/100 × pitch/0x100000`. Players only send these while playing.

## CDJ status — UDP 50002, type `0x0a`, ≥0xd4 bytes (unicast to known devices) ⚠️

| Offset | Size | Field |
|---|---|---|
| 0x21 | 1 | device number |
| 0x89 | 1 | flags F: `0x40` playing, `0x20` master, `0x10` synced, `0x08` on air |
| 0x8d | 3 | pitch |
| 0x92 | 2 | track BPM × 100 (`0xffff` = no track) |
| 0x9f | 1 | master hand-off target (`0xff` = none) |
| 0xa0 | 4 | beat number in track |
| 0xa6 | 1 | beat within bar |

The flag constants match Beat Link's `CdjStatus`.

## Mixer status — UDP 50002, type `0x29`, 0x38 bytes ⚠️

The device number is at 0x21, flags (master bit `0x20`) at 0x27, BPM × 100 at 0x2e, and beat within bar at 0x37. The XDJ-XZ's mixer section may or may not send these. The firmware only uses them to avoid treating the mixer as a playing deck.

## Keep-alive — UDP 50000, type `0x06`, 0x36 bytes ⚠️

| Offset | Size | Value we send |
|---|---|---|
| 0x0c | 20 | name (`DEVICE_NAME`) |
| 0x20 | 4 | `01 02 00 36` |
| 0x24 | 1 | our device number |
| 0x25 | 1 | `01` (player) |
| 0x26 | 6 | MAC |
| 0x2c | 4 | IP |
| 0x30 | 1 | peers seen |
| 0x34 | 1 | `01` |

We broadcast this every 1.5 s to the subnet broadcast address. Beat Link's virtual CDJ uses numbers 5–15 and gets status packets without the full device-number claim handshake. That handshake is only needed for numbers 1–4, which we never use.

## XDJ-XZ specifics

- Both decks, and possibly the mixer section, announce from **one IP** with different device numbers. Everything here is keyed by device number. This matches Beat Link 0.6.0's XDJ-XZ fix.
- Use firmware ≥ 1.22, which fixed Pro DJ Link issues on the unit.
- Things to confirm in a capture: the device numbers the XZ uses for each deck and its mixer; whether it sends mixer status; and whether a deck's master flag behaves like a CDJ's when Sync/Master is pressed.
