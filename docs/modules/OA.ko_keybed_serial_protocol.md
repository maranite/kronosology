# Kronos keybed communication — serial protocol over the Super-IO chip

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
  clock — it uses an alternate 24 MHz clock instead.** This alternate clock
  is selected by writing to configuration registers on the Super-IO chip,
  and `OA.ko` performs this clock switch itself as part of bringing the
  UART up. ⚠️ **Not yet confirmed as reconstructed in this project's own
  `CSTGComPort::Initialize()`** — the currently-reconstructed sequence
  (chip unlock → LDN select → base-address/IRQ read → UART reset →
  DLAB/divisor baud-rate programming) does not visibly include a
  clock-source-switch write to the Super-IO config space. If real
  ground truth's ~2561-byte `Initialize()` does this switch and our
  reconstruction is missing it, the resulting effective baud rate would be
  wrong even after the chip and UART are both successfully found and
  configured. *To validate*: re-check `OA_real.ko`'s
  `CSTGComPort::Initialize()` disassembly specifically for a Super-IO
  config-register write that selects the alternate clock source, and
  confirm whether the currently-reconstructed baud-rate divisor constant
  already accounts for a 24 MHz reference (vs. the default 1.8461 MHz) —
  if it does, the clock switch may already be implicitly baked into the
  divisor math even without an explicit separate write being modeled.
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
and 10 retries. Given the above (K2's real Super-IO chip is a
register-compatible Nuvoton part, and the chip-ID check is already generic
enough to accept it), a missing/different Super-IO chip is *not* the
likely explanation. Two more likely candidates, not yet resolved:

1. The dev board was tested booted from a **rescue USB stick**, not its own
   internal SSD — production units always boot from the internal SSD, and
   BIOS/POST-level Super-IO LPC-decode-window initialization can plausibly
   differ by boot device on some mainboards. Untested as of 2026-07-21.
2. `CSTGComPort`'s reconstructed unlock sequence (`0x87, 0x01, 0x55,
   0x55`/`0xaa` depending on config port) was traced from `OA_real.ko`
   disassembly and is assumed to be a single, chip-family-agnostic
   sequence. If Nuvoton's NCT6627UD actually requires an unlock sequence
   the real `OA.ko` selects via some vendor-detection branch not yet found
   in the disassembly, that would explain a real-hardware-only gap in this
   reconstruction. Not yet checked.
