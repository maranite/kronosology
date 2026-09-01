# Kronos keybed communication — serial protocol over the Super-IO chip

While most knobs, buttons, and LEDs on the Kronos are connected to the OMAP
(front-panel/NKS4 board), the **keybed** — including the two switches SW1/SW2 and
the pitch-bend "joystick" controller — is connected directly to the **PC
mainboard**, not through NKS4/OMAP.

Communication between the keybed and the PC works via the PC's serial port COM1.
This serial port is provided by a 16550A-compatible UART inside the mainboard's
Super-IO chip. The confirmed chip family is Winbond W83627(UHG); Korg's own docs
describe a Nuvoton NCT6627UD (a sibling of the NCT6776) on the Kronos 2 (ASRock
IMB-140 mainboard), but a live register probe against a running Kronos 2 identified
the actual installed chip as a Winbond W83627UHG via the kernel's own `w83627ehf`
binding — the register layout needed to configure and use the first UART (COM1) is
identical across this whole chip family regardless of which specific part is
installed, so this doesn't affect the protocol below.

`OA.ko` completely bypasses the kernel's own serial driver — there is a minimal
serial driver embedded directly inside `OA.ko` itself (`CSTGComPort`, this project's
own reconstruction, `reconstructed/OA/src/init/comport_init.cpp`).

---

## Oddities in the stock design (not project bugs)

- **Baud rate: 62500 bits/s** — not a standard PC serial baud rate. Reached via
  divisor `0x18` (24 decimal) against a **24MHz** UART reference clock, not the
  standard 1.8432MHz PC UART clock (`CSTGKeybedInterface_Startup` passes the literal
  constant `0x18` as `baudRateCode`, confirmed via disassembly). The 24MHz reference
  is a real, software-visible per-LDN register on this chip family (`CRF0h`, bits
  [1:0]: `00`=1.8462MHz, `01`=2MHz, `10`=24MHz) — but `OA.ko`'s
  `CSTGComPort::Initialize()` never writes it for any LDN (confirmed via full
  disassembly, ~780 lines). On real hardware `CRF0h` already reads `0x02` (bits
  `10` = 24MHz) before `OA.ko` runs, set by a hardware strap or an earlier boot
  stage — `OA.ko` simply assumes it's already correct rather than switching it
  itself.
- **Voltage-level mismatch**: the PC side uses RS232 levels (±10V), the keybed side
  uses TTL levels (0V/+5V). The connection works in practice anyway (marginal but
  functional overlap between the two logic-level standards) — a real, if odd,
  characteristic of the stock design, not something to "fix."
- **Generic chip-ID validity check, not vendor-specific.** `OA.ko` reads the
  Super-IO chip's ChipID register (register `0x20` after unlock) and refuses to
  start if nothing plausible answers, but the check itself
  (`(id - 1) <= 0xfd`) is a broad range check, not a hardcoded Winbond-specific ID
  match — consistent with the same `OA.ko` binary being designed to run against
  either chip family, correct unlock key assumed.
- **`comPortId 3`'s special init block is real but dead on real hardware.**
  `CSTGComPort::Initialize()` has two previously-unreconstructed special blocks
  (comPortId 0's alternate-config-port retry, and comPortId 3's GPIO/
  multi-function-pin sequence touching registers `0x10`/`0x11`/`0x13`/`0x14`/
  `0x22`/`0x25`/`0x26`). The comPortId 3 block writes real global (NOLDN) registers
  — `CR25=0x00` (Interface Tri-state Enable), `CR22=0xFF` (Device Power Down), and
  `CR10/11/13/14` (IRQ type/polarity), gated by the documented `CR26[bit4]`
  unlock/lock dance (matches the public Nuvoton NCT6776F/D datasheet
  instruction-for-instruction) — but on real hardware LDN 0x03 has `base=0x0000`
  (inactive), so `Initialize(3)`'s own base-address validity check fails before
  this block ever executes. Faithfully reconstructed, but confirmed unreachable in
  practice.

## The unlock-key mismatch

`CSTGComPort::Initialize()`'s Super-IO unlock sequence is 4 bytes,
`0x87, 0x01, 0x55, 0x55`/`0xaa` — a documented **ITE IT87-series** Super-IO unlock
key. The real Super-IO chip on Kronos hardware only answers the standard 2-byte
**Nuvoton/Winbond** unlock key (`0x87, 0x87`) — confirmed two independent ways: the
kernel's own built-in `w83627ehf`-family hwmon driver binds successfully at
`0x295`/`0x296` (base `0x290`) using that key, and a standalone userspace raw-I/O
probe (`iopl(3)` + raw `outb`/`inb`, no kernel module involved) gets a response
(`devid=0xa235`) only from the 2-byte key — the 4-byte ITE key gets a constant
`0xff` response at both `0x2e` and `0x4e`, even with explicit inter-byte pacing to
rule out a too-fast-write artifact.

This mismatch is **unconditional across every Kronos model** — no per-CPU-count or
model-ID check gates the call chain (`init_module` → `CSTGKeybedInterface_Startup`
→ `CSTGComPort::Initialize`), the inner 6-port/10-retry probe loop is unconditional,
and the latest available OA.ko build (extracted from `KRONOS_Update_3_2_2.tar.gz`)
has a byte-for-byte identical `CSTGComPort::Initialize()` to earlier analyzed
builds — every branch checked, including the two special blocks above. Two nearly
identical real startup paths exist in the binary
(`CSTGKeybedInterface_Startup` and a near-duplicate C++ method,
`CSTGKeybedInterface::Startup()`), both probing ports 0–5 in order with the same
wrong key.

On a real Kronos 2 dev board, this mismatch caused `OA.ko`'s own chip-detection
probe (`DetectChipAt(0x2e)`/`DetectChipAt(0x4e)`) to find no chip at either legacy
Super-IO config port, across all 6 COM ports and 10 retries — consistent with
`OA.ko`'s documented behavior of refusing to start when the expected chip isn't
found.

**Not reconciled**: despite the above, a live production Kronos was observed
receiving genuine, actively-consumed keybed protocol traffic during testing (bytes
occasionally dropped in a pattern consistent with a concurrent kernel-side reader
draining the same UART FIFO). How that unit's running `OA.ko` is successfully
talking to the keybed despite the wrong unlock key is not established — possibilities
include the Super-IO LDN already being enabled by BIOS/POST before `OA.ko` runs (so
`OA.ko`'s own config-unlock step isn't strictly required for basic UART I/O once the
port is already active), or some other difference between that unit and the dev
board not otherwise characterized. Flagged as open, not resolved.

---

## Wire protocol

Framing: **8N1 at 62500bps** (LDN 0xD, I/O base `0x2e8`, IRQ 7 — matches this
project's own ground-truthed `kLdnByPort[5] = 0xd`, `comPortId 5`). Every message
starts with a header byte (high bit set); the message length is fixed per header
class per `kNumBytesForMessageType[(hdr&0x70)>>4]` (this project's own
disassembly-ground-truthed table in `keybed_receive.cpp`).

| Header class | `(hdr&0x70)>>4` | Bytes | Meaning |
|---|---|---|---|
| `0xE0-EF` (`0xEA` observed) | 6 | 4 | Idle heartbeat — fixed payload `23 07 20`, repeats every ~447.5ms. |
| `0xD0-DF` | 5 | 2 | Button edge: `0xD1`=press, `0xD0`=release; payload byte = switch index (`0x00`=SW1, `0x01`=SW2). Edge-triggered — no periodic re-broadcast while a switch is held. |
| `0xB0-BF` (`0xB5`/`0xB7` observed) | 3 | 3 | Analog axis: `0xB5`/`0xB7` = joystick X/Y axes (a sine/cosine-like pair, ~90° out of phase during circular motion). Payload = `[byte1][byte2]`; byte2 is the 7-bit coarse position (`0x00-0x7F`) that sweeps smoothly with movement; byte1 is overwhelmingly `0x00`/`0x01`, consistent with the LSB of a MIDI-pitch-bend-style split value (byte1=LSB, byte2=MSB/coarse position). |
| `0xA0-AF` | 2 | 3 | Boot-handshake ACK class (`ReceiveMessage`'s `(buf[0]&0xf0)==0xa0` branch) — used only during `CSTGKeybedInterface_Startup`'s own probe/ACK handshake, not observed during steady-state operation. |

`0xB8` has also been observed in the `0xB0-BF` class alongside `0xB5`/`0xB7`; its
exact role (a third axis/control vs. an artifact of FIFO contention between this
project's probe tooling and `OA.ko`'s own live receiver) is not distinguished.
Occasional malformed/misaligned triples (e.g. a captured `b5 64 65`) are consistent
with dropped-byte misalignment from that same FIFO contention, not a different
message structure.
