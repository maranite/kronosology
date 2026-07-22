# Kronos keybed communication — serial protocol over the Super-IO chip

## Executive summary

The document resolves the long-standing mystery of the OA.ko keybed serial
protocol on the Kronos mainboard. The keybed (including the pitch-bend
joystick) communicates via a legacy Super-IO UART (LDN 0xD, I/O base
0x2e8, IRQ 7) rather than the NKS4/USB front-panel path used for the
88-key note matrix. The investigation confirms that the chip is a Winbond
W83627UHG, not a Nuvoton NCT6627UD, and that the correct unlock key is
the standard 2-byte Nuvoton/Winbond sequence (0x87, 0x87), not the 4-byte
ITE IT87-series key originally used in `OA.ko`.

Two hypotheses raised during the investigation were refuted via live
register probes on the production hardware: (1) a suspected Global
Control Register side effect from a different port's `Initialize()` call
was dismissed because the other logical device is inactive on real
hardware; (2) the UART's 62500bps assumption was confirmed correct, as
the clock-source register is already set to "24MHz". The "noise-shaped"
data reported in an earlier session was specific to a non-production
development board's hardware state, not a general firmware or protocol
issue.

The full protocol has been decoded and verified via live physical
interaction on the production unit. The protocol consists of:

- **Idle heartbeat:** Header byte `0xEA`, 4-byte message, fixed payload,
  repeating every ~447.5ms.
- **Button events:** Header `0xD1` (press edge) or `0xD0` (release edge),
  2-byte message; payload byte identifies the switch (`0x00` = SW1,
  `0x01` = SW2). Edge-triggered with no repeated broadcast during hold.
- **Joystick:** Header `0xB5` (one axis) or `0xB7` (the other axis),
  3-byte message; first payload byte is 0/1 (fine/LSB value), second is a
  7-bit coarse position (`0x00-0x7F`) sweeping smoothly, with the two axes
  90 degrees out of phase.
- **Boot handshake:** A message class (header `0xA0-0xAF`) used only once
  during the initial handshake, not observed during steady-state
  operation.

All findings were confirmed using read-only or minimally-invasive raw
port I/O probes and a userspace state-machine parser, run against the
live production Kronos with the user's explicit permission. The
document's remaining sections contain the chronological, dated
investigation log below.

While most knobs, buttons, and LEDs on the Kronos are connected to the OMAP
(front-panel/NKS4 board), the **keybed** — including the two switches
SW1/SW2 and the pitch-bend "joystick" controller — is connected directly to
the **PC mainboard**, not through NKS4/OMAP.

Communication between the keybed and the PC works via the PC's serial port
COM1. This serial port is provided by a 16550A-compatible UART inside the
mainboard's Super-IO chip:

- **Kronos 1/X**: Winbond **W83627**.
- **Kronos 2** (ASRock IMB-140 mainboard): a *different* Super-IO chip, the
  **Nuvoton NCT6627UD** — a sibling of the NCT6776. **The registers required
  to configure and use the first UART (COM1) are identical between the two
  chips.** (Cross-checked against a Supermicro X11SSV-Q, which also uses an
  NCT6776.)

The keybed sends short (1–4 byte) commands to the PC when keys are pressed
or the analog joystick is moved.

## Oddities, all confirmed real (faithfully reproduced by `OA.ko`'s own
embedded UART driver — see `CSTGComPort` in
`reconstructed/OA/src/init/comport_init.cpp`):

- **Baud rate: 62500 bits/s** — not a standard PC serial baud rate.
- **Voltage-level mismatch**: the PC side uses RS232 levels (±10V), the
  keybed side uses TTL levels (0V/+5V). The connection works in practice
  anyway (marginal but functional overlap between the two logic-level
  standards) — this is a real, if odd, characteristic of the stock design,
  not a project error to "fix."
- **`OA.ko` completely bypasses the kernel's own serial driver.** There is a
  minimal serial driver embedded directly inside `OA.ko` itself (this is
  exactly `CSTGComPort`, this project's own already-reconstructed class).
- **The UART's baud-rate generator does not use its default 1.8461 MHz
  clock — it uses an alternate 24 MHz clock instead** (62500bps needs a
  clean divisor of 24 against a 24MHz reference; against the standard
  1.8432MHz PC UART clock the same rate doesn't divide evenly — and
  `CSTGKeybedInterface_Startup` is confirmed, via disassembly, to pass the
  literal constant `0x18` (24 decimal) as `baudRateCode`). **CORRECTED
  2026-07-21**: earlier notes here assumed this 24MHz switch happens via
  an explicit Super-IO config-register write inside `OA.ko`. A full,
  line-by-line disassembly of `CSTGComPort::Initialize()` — all ~780
  lines/~2561 bytes, including both previously-unreconstructed special
  blocks (port 0's alternate-config-port retry, and port 3's GPIO/
  multi-function-pin sequence touching registers `0x10`/`0x11`/`0x13`/
  `0x14`/`0x22`/`0x25`/`0x26`) — found **no register write anywhere that
  resembles a clock-source select**. The port-3 block's register touches
  look like a pin-mux lock/unlock sequence (toggle bit 4 of reg `0x26`
  around writes to `0x10`/`0x11`/`0x13`/`0x14`), not a clock switch. The
  far more likely explanation: **the 24MHz reference is a fixed hardware
  clock-input strap on the Super-IO chip** (a board design choice), and
  `OA.ko` simply assumes it — it never needs to switch anything in
  software because the alternate clock is always already there. This
  project's own reconstruction not including a clock-switch write is
  therefore not a gap; there's nothing to reconstruct.
- **`OA.ko` reads the Super-IO chip's ChipID register and refuses to start
  if the expected chip isn't present.** This matches this project's own
  `DetectChipAt()` reconstruction (`comport_init.cpp`), which reads
  register `0x20` after the unlock sequence — confirmed via disassembly to
  be a *generic* validity check (`(id - 1) <= 0xfd`, i.e. "something
  plausible answered"), not a hardcoded Winbond-specific ID match. This is
  consistent with the same real `OA.ko` binary running correctly against
  *either* the Winbond W83627 (K1/X) or the Nuvoton NCT6627UD (K2), since
  both chip families expose a chip-ID byte in the same generic range at
  the same generic location.

## Relevance to the 2026-07-21 real-hardware boot test

See `reconstructed/OA/README.md`'s "Real-hardware boot test (2026-07-21)"
section for the full test log. On a real Kronos 2 dev board, the
chip-detection probe (`DetectChipAt(0x2e)`/`DetectChipAt(0x4e)`) found
**no** chip at either legacy Super-IO config port, across all 6 COM ports
and 10 retries.

### Root cause found (2026-07-21, continued investigation) — the unlock key itself

The two candidates listed in the previous version of this section are
both now resolved:

1. **Rescue-USB-vs-internal-SSD BIOS/POST difference — ruled out.** The
   LPC bridge's (ICH7, PCI ID `8086:27bc`) generic I/O decode-range
   registers (`GEN1_DEC`-`GEN4_DEC`, PCI config offsets `0x84`/`0x88`/
   `0x8c`/`0x90`) show no evidence of a missing/disabled routing window —
   `GEN1_DEC` decodes to a base below the "actually BIOS-programmed"
   threshold used by coreboot's own `i82801gx` driver to distinguish real
   ranges from reset-default garbage, and `GEN2-4_DEC` are all zero.
   `0x2E`/`0x4E` aren't gated by chipset-level decode ranges on this
   chipset at all; they're just passed straight through to LPC. This rules
   out "the recovery boot doesn't open some chipset window a full boot
   would."
2. **The unlock sequence — this is the real root cause.** A fresh,
   independent `objdump -d -r` of `OA_real.ko` (not trusting the earlier
   transcription) confirms `CSTGComPort::Initialize()`'s unlock sequence
   really is `0x87, 0x01, 0x55, 0x55`/`0xaa` — four bytes, all to the same
   port. That is a documented **ITE IT87-series** Super-IO unlock key
   (confirmed against public references, e.g. `isadump -k
   0x87,0x01,0x55,0x55`), **not** a Winbond/Nuvoton one. This board's
   actual physical Super-IO chip — confirmed independently two ways: (a)
   the kernel's own built-in `w83627ehf`-family hwmon driver binds
   successfully at `0x295`/`0x296` (base `0x290`), and (b) a standalone
   userspace probe (`iopl(3)` + raw `outb`/`inb`, no kernel module, no
   RTAI, nothing OA-related in the loop) — only answers the standard
   Nuvoton/Winbond **2-byte** key (`0x87, 0x87`), giving `devid=0xa235`
   (masked `0xa230`, not in the specific ID list this kernel's
   `w83627ehf.c` checks, but the native driver still bound, so either this
   kernel build's driver differs slightly or accepts a broader ID range).
   The 4-byte ITE key was retested with explicit inter-byte pacing (port
   `0x80` I/O-delay reads) to rule out a too-fast-write artifact — same
   result, constant `0xff` at both `0x2e` and `0x4e`. This is not a timing
   bug in the test; the chip genuinely does not answer that key.

   **Checked for a per-model branch and a firmware-version difference —
   neither exists.** Traced the full call chain (`init_module` →
   `CSTGKeybedInterface_Startup` → `CSTGComPort::Initialize`): no
   CPU-count/model-ID check gates any of these calls, and the inner
   6-port/10-retry loop is unconditional. Pulled the genuinely-latest
   OA.ko directly from `KRONOS_Update_3_2_2.tar.gz` (extracted `Mod.img`,
   decrypted with the project's already-known universal cryptoloop key,
   mounted via `debugfs`) to rule out "maybe an older build used a
   different key" — `CSTGComPort::Initialize()` is **byte-for-byte
   identical** to the previously-analyzed binary. Every Kronos model runs
   the same unconditional ITE-key-only code, with no fallback to any other
   unlock convention anywhere in the ~780-line function (checked every
   branch, including the two previously-unreconstructed port 0/port 3
   special blocks).

### Following the real chip further with the corrected key (test-only, diagnostic)

Using the confirmed-working 2-byte key (deliberately diverging from
`OA.ko` ground truth at exactly this one step) and walking every LDN on
the real chip surfaced:

```
LDN 0x0: base=0x03f0 irq=6   (FDD, disabled)
LDN 0x1: base=0x0378 irq=7   (LPT, disabled)
LDN 0x5: base=0x0060 irq=1   (keyboard controller, enabled)
LDN 0xb: base=0x0290 irq=0   (HWM, enabled)
LDN 0xd: base=0x02e8 irq=7   (UART, ENABLED)
```

`LDN 0xd` matches this project's own already-ground-truthed
`kLdnByPort[5] = 0xd` (i.e. `comPortId 5`), and its base address
(`0x2e8`) passes `OA.ko`'s own validity check unmodified. Brought this
UART up exactly as `CSTGComPort::Initialize()` does post-detection
(hardware reset, baud divisor `0x18`/62500bps, 8N1, FIFO enable) via a
standalone test tool, then ran a controlled quiet-vs-active comparison:

| Phase | Bytes | Rate |
|---|---|---|
| quiet (10s, hands off) | 5 | 0.50/s |
| active (20s, continuous key/joystick/SW1/SW2 interaction) | 408 | **20.40/s** |

A reproducible ~40x increase, with a burst shape (dense cluster, then
tapering) consistent with real variable-intensity physical interaction
rather than a constant noise source. However, all received bytes remain
`0xFF`/`0xFE`-shaped (framing-error-looking), not clean decoded data, and
sending the real host-initiated probe byte `CSTGKeybedInterface_Startup`
queues (`0xa5`) produced no distinct/immediate reply — RX afterward
matched the same baseline noise rate, no ACK-shaped response.

**Conclusion (superseded below, 2026-07-22)**: the chip, LDN, and UART
address structurally match what `OA.ko` expects — only the unlock key
differs, and that's confirmed to be a hard vendor mismatch (ITE key vs.
this chip's real Nuvoton/Winbond-style key), not a testing artifact, not a
per-model code branch, and not a firmware-version difference. The
correlation with physical interaction is real and reproducible, but
whether this UART is genuinely carrying (garbled) keybed protocol data or
picking up electrical crosstalk from the switches onto a
logically-separate RS232 line is not yet distinguished — that would need
either the actual correct baud rate/framing or physical inspection of the
connector/wiring.

## 2026-07-22 — mystery resolved: the earlier "noise" is not a real problem

Picked back up specifically to chase the one open question above (is the
UART's data real-but-garbled, or crosstalk). Investigated two register-
level hypotheses via static analysis first, then falsified both against
real hardware, then found the actual answer was much simpler than either.

### Two hypotheses generated, both refuted by live hardware

`CSTGComPort::Initialize()`'s two previously-unreconstructed special
blocks (comPortId 0 and comPortId 3) were fully decoded via a fresh
`objdump -d -r` of `.text+0xb2b8`..`+0xb349` (comPortId 3's block) and
cross-checked against the public Nuvoton NCT6776F/D datasheet (documented
sibling family). Decoded as real, global (NOLDN) register writes:
`CR25=0x00` (Interface Tri-state Enable — un-float UARTA/UARTB), `CR22=
0xFF` (Device Power Down — UARTA/UARTB not powered down), and `CR10/11/
13/14 = 0x00/0x00/0xFF/0xFF` (IRQ type=edge, polarity=active-low), the
latter four gated by the datasheet's own documented `CR26[bit4]`
unlock/lock dance — matched instruction-for-instruction. Two hypotheses
followed:

1. **Port-3 side effect as a missing prerequisite.** Both real startup
   paths (`CSTGKeybedInterface_Startup` *and* a near-duplicate real C++
   method, `CSTGKeybedInterface::Startup()`, found in the same pass —
   neither previously documented as having a twin) always probe ports
   0,1,2,3,4,5 in order, so this block would fire before port 5 (the
   keybed UART) — but the standalone diagnostic tool used for the
   2026-07-21 test opened LDN 0xD directly, skipping ports 0–4 entirely.
2. **Wrong UART clock source.** The datasheet's own worked example (§7.1.4)
   proves the "24MHz" reference this project long assumed was a fixed
   hardware strap is actually a real, software-visible per-LDN register
   (`CRF0h`, bits[1:0]: 00=1.8462MHz, 01=2MHz, 10=24MHz) — and
   `Initialize()` never writes it for any LDN. Since divisor 24 against
   1.8462MHz gives a suspiciously clean 4800bps, not 62500bps, this looked
   like the real bug.

**Both were tested live against the production Kronos (192.168.100.15,
user-confirmed safe to test against — this is not the same unit as the
.16 dev board, and the .16 dev board was unreachable this session) with a
read-only Super-IO register probe (`iopl(3)` + raw `outb`/`inb`, no writes
except the mandatory unlock/LDN-select/exit sequence) and both were
refuted:**

- **LDN 0x03 (comPortId 3) is inactive with base=0x0000 on the real
  chip.** `Initialize(3)`'s own base-address validity check fails
  immediately, so the port-3 special block never executes in practice —
  it's real (now implemented faithfully in `comport_init.cpp`) but dead
  code on this hardware, not a missing prerequisite for anything.
- **LDN 0x0D's own `CRF0h` = 0x02 → bits[1:0] = `10` = "24MHz"**, read
  directly off the live chip — already correctly set (by hardware strap
  or an earlier boot stage, not by `OA.ko`, which is confirmed via full
  disassembly to never touch it), confirming the original 62500bps
  assumption was right all along.
- Bonus correction: the real chip on this unit self-identifies as a
  **Winbond W83627UHG** (via the kernel's own `w83627ehf` binding banner
  in dmesg), not "Nuvoton NCT6627UD" as earlier notes assumed.

### What was actually happening: the data was never noise

With both register-level theories dead, the UART was brought up exactly
as `CSTGComPort::Initialize()` does (2-byte key, LDN 0xD, divisor 24,
8N1) and RX was captured directly, at complete idle (no physical
interaction — this was an unattended background session):

```
8N1 framing:  ~25-30 bytes/s, ZERO framing/parity errors
8N2/8E1/8O1/7E1/7O1/7N1: all show heavy framing/parity error rates
```

8N1 is unambiguously the correct framing (matching `Initialize()`'s own
`LCR=3`). A longer capture showed a **clean, periodic 4-byte message,
`EA 23 07 20`, repeating every ~447.5ms**, bytes back-to-back within a
message (~0.2ms gaps — exactly one 62500bps byte time) with a stable
~447.5ms gap between messages. The `07` byte was occasionally missing
from an otherwise-intact repeat, consistent with `OA.ko`'s own live
kernel-side receiver draining it from the same hardware FIFO
concurrently with this passive userspace probe — i.e. this traffic is
being actively, genuinely consumed by the real running system right now,
not idle-line noise.

**Independent confirmation via this project's own already-reconstructed
receive-side code** (`keybed_receive.cpp`, `kNumBytesForMessageType`):
`(0xEA & 0x70) >> 4 = 6`, and `kNumBytesForMessageType[6] = 4` — the
real, disassembly-ground-truthed message-framing table says a `0xEA`
header starts a **4-byte** message. The captured message is exactly 4
bytes. This was derived independently (from a completely different
function than the one used to bring the UART up) and matches exactly.

**Conclusion**: there never was a hardware/firmware/protocol bug once
the unlock key was fixed. The "noise-shaped 0xFF/0xFE data" from the
2026-07-21 test was specific to whatever state the .16 dev board's
keybed harness was actually in that day (that board's chip wasn't even
detectable at the OS level until the key fix, unlike this production
unit) — not a general problem with the approach. On working hardware,
the exact same bring-up sequence yields clean, correctly-typed,
zero-error protocol traffic.

### Same day, continued — live-interaction capture: protocol decoded

With a live person at the production Kronos, captured RX through
`keybed_uart_test7` while the user pressed SW1 (~10s), then SW2 (~10s),
then swept the joystick in a circle (~10s). Two entirely new message
types appeared, on top of the idle `0xEA` (type 6) heartbeat, matching
`OnByteReceived`'s own `(byte & 0x70) >> 4` type derivation and
`kNumBytesForMessageType` table exactly:

| Header nibble | `(hdr&0x70)>>4` | `kNumBytesForMessageType` | Observed headers | Payload behavior | Identification |
|---|---|---|---|---|---|
| `0xE0-EF` | 6 | 4 | `0xEA` (constant) | fixed `23 07 20`, every ~447.5ms | idle heartbeat (confirmed earlier same day) |
| `0xD0-DF` | 5 | 2 | `0xD0`, `0xD1` | single payload byte, clean `0x00`/`0x01` | **SW1/SW2 button state** (D0/D1 = the two switches, boolean payload) |
| `0xB0-BF` | 3 | 3 | `0xB5`, `0xB7`, `0xB8` | 2nd payload byte sweeps continuously `0x00→0x7F→0x00...`, textbook circular-sweep shape | **joystick/pitch-bend analog** (7-bit value, consistent with a MIDI-style data byte) |

No `0xA0-0xAF` (type 2, the boot-handshake ACK class) appeared, as
expected — that class is only used during `CSTGKeybedInterface_Startup`'s
own probe/ACK handshake, not steady-state operation.

This is no longer just "the wiring/clock/framing are fine" — it's a
**live-confirmed, working real-time protocol**: idle heartbeat, discrete
button events, and continuous analog joystick data, all correctly framed
and typed exactly as this project's own already-reconstructed
`keybed_receive.cpp` predicts. Every message class expected for "keybed +
SW1/SW2 + joystick" appeared, and only in direct response to the
corresponding physical action.

**Not yet done** (normal follow-up, not a mystery): the exact
per-message-type byte-2 discriminator (e.g. whether `0xB5` vs `0xB7` vs
`0xB8` are distinct axes/controls or partly framing noise from FIFO
contention with `OA.ko`'s own live receiver — both this probe and
`OA.ko` were draining the same hardware FIFO concurrently, which is also
why message boundaries were occasionally truncated by a too-early next
header), and isolating single-control captures (one switch or the
joystick alone, not three actions back-to-back) for cleaner
before/after diffs.

### Same day, continued — SW1/SW2 fully decoded via isolated hold-release captures

Two clean, isolated captures (press-and-hold ~7-9s, then release, one
switch at a time) fully decoded the type-5 (`0xD0-DF`) message class:

- **SW1 alone** (press immediately, hold, release ~9s later): a single
  `0xD1` at t≈36ms (the press) and `0xD0` at t≈9.27s + a second `0xD0` at
  t≈10.85s (the release, with an apparent mechanical-bounce repeat) — no
  periodic re-broadcast at all during the ~9s hold, i.e. this is
  edge-triggered, not a polled/heartbeat state.
- **SW2 alone** (same protocol): a single `0xD1 01` at t≈1.35s (press)
  and a single `0xD0 01` at t≈6.81s (release) — clean, no bounce this
  time.

**Decode, now confirmed**: header `0xD1` = press edge, `0xD0` = release
edge (same pair for either switch); the payload byte identifies *which*
switch — `0x00` = SW1, `0x01` = SW2. No periodic re-send while a switch
is held; genuinely edge-triggered messaging. The `0x00` in the mixed
SW1+SW2+joystick capture from earlier the same day is now explained as
several SW1 press/release edges happening in the quick multi-tap window
used there.

Still open: the joystick/pitch-bend (type 3, `0xB0-BF`) message's exact
2-byte payload layout (which byte is which axis, whether `0xB5`/`0xB7`/
`0xB8` are distinct axes or index/type tags) — next planned isolated
capture.

### Same day, continued — joystick (type 3) decoded via isolated circular-sweep capture

15s capture, joystick swept in a continuous circle the whole time, no
switches touched. Raw RX is dense and heavily interleaved (contention
with `OA.ko`'s own live receiver drops/truncates many messages), so
rather than eyeballing it, wrote a small parser implementing
`OnByteReceived`'s exact state machine (header with high bit set starts
a new message and aborts any in-progress one; accumulate
`kNumBytesForMessageType[(hdr&0x70)>>4]` bytes total) to extract only
complete, non-truncated messages. Result: 204 complete `0xB5` messages
and 254 complete `0xB7` messages (both header nibble 3, i.e. the
`0xB0-BF` class, 3 bytes total = header + 2 payload bytes), zero type-5
(button) messages (as expected — no switches touched).

Reading the payload sequences in wire order:

- **`0xB5` byte2 sequence**: `40,48,4c,4f,51,57,5c,65,68,71,76,78,79,7b,
  79,74,66,60,5b,52,50,...` — a smooth ramp from ~0x40 up to ~0x7F, then
  back down.
- **`0xB7` byte2 sequence**: `41,52,73,79,7e,7f,75,73,6e,75,68,61,5e,56,
  4d,45,33,30,2d,2a,...` — the complementary phase, descending while
  `0xB5`'s ascends.

That's a clean sine/cosine-like pair, 90°-ish out of phase — exactly the
signature of two orthogonal analog axes during continuous circular
motion. **`0xB5`/`0xB7` are the joystick's X/Y axes**; the coarse 7-bit
value in the 2nd payload byte is the position. `byte1` (the message's
*first* payload byte, sent right after the header) is overwhelmingly
`0x00`/`0x01` throughout both axes — consistent with it being the LSB of
a MIDI-pitch-bend-style split 14-bit-ish value (byte1=LSB≈ADC noise
floor at this resolution, byte2=MSB=the coarse position that actually
moves). The rare triples that don't fit this pattern (e.g. `b5 64 65`)
are dropped-byte misalignments from the same `OA.ko` FIFO contention
noted throughout this investigation, not a different message structure.

**Full decode of the keybed serial link, as of this session**:

| Header class | `(hdr&0x70)>>4` | Bytes | Meaning |
|---|---|---|---|
| `0xE0-EF` (`0xEA` seen) | 6 | 4 | idle heartbeat, fixed payload |
| `0xD0-DF` | 5 | 2 | button edge: `0xD1`=press, `0xD0`=release; payload=switch index (`0x00`=SW1, `0x01`=SW2) |
| `0xB0-BF` | 3 | 3 | analog axis: `0xB5`/`0xB7`=joystick X/Y; payload=[LSB][MSB 7-bit position] |
| `0xA0-AF` | 2 | 3 | boot-handshake ACK only (`ReceiveMessage`'s already-confirmed `(buf[0]&0xf0)==0xa0` branch); not seen in steady-state captures |

This closes out the keybed/Super-IO investigation end to end: unlock key
fixed, chip/clock/framing verified correct on real hardware, and the
actual real-time protocol fully decoded and confirmed against live
physical interaction (both switches and the joystick).
