# OmapNKS4Module.ko — OMAP NKS4 Driver

The kernel module that owns the low-level hardware path to the OMAP-side peripherals,
most importantly the Atmel NV2AC chip. It exports the primitives every other module
uses to talk to that chip.

| Property | Value |
|---|---|
| Path on device | `/sbin/OmapNKS4Module.ko` |
| Source path | `dump from kronos/sbin/OmapNKS4Module.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | ~87 KB |
| Functions | 260 (per `nm`) / 425 in Ghidra |
| C++ mangled symbols | 87 |
| Compiler | GCC 4.5.0 |

---

## ⚠ Binary version warning (found 2026-07-17)

**There are two different builds of this module in the repo — do not conflate them.**

| File | Size | MD5 | Notes |
|---|---|---|---|
| `RestoreDVD_SystemMNT/mnt/sbin/OmapNKS4Module.ko`, and the byte-identical copies in `3.2.1 update contents/` and `3.2.2 update contents/mnt/sbin/` | 89849 | `461156bba7...` | **The correct, factory-current target.** |
| `docs/ASM Docs/OmapNKS4Module/OmapNKS4Module.ko` (paired with the raw `OmapNKS4Module_ASM.txt` Ghidra dump) | 79376 | `325923e47b...` | **A different, older build.** |

The older 79376-byte build is missing entire subsystems present in the real
89849-byte target: SCSI-based graceful SSD-shutdown-on-panel-power-button handling
(`ShutdownSSDRoutine`/`ShutdownSSDWait`/`SignalShutdownSSD`, `scsi_device_lookup`/
`scsi_device_put`), `COmapNKS4Driver_ApplyAftertouchTable`, `GetPanelLVersion`/
`GetPanelRVersion`/`GetJackVersion`, `SetNumberOfKeys`, and two
`OmapNKS4ProcRead*HardwareVersion/OmapVersion` proc handlers. It's ~90%
code-identical to the real target (same driver, earlier revision) but **is not
reliable ground truth for anything that differs** — confirmed by diffing `nm`
symbol tables, not just file size.

`kronosology/reconstructed/OmapNKS4Module/` and `KronosNKS4/docs/protocol.md` were
already built from/verified against the **correct** 89849-byte binary (confirmed by
their own cited source paths) — they remain the right starting point. No saved
Ghidra project existed anywhere in the repo for the correct binary until a fresh
import in the 2026-07-17 session (see `KronosNKS4/docs/gaps.md`'s
`CActiveSenseThread` writeup for what came out of it).

---

## Exported kernel symbols (the keys to the kingdom)

`OmapNKS4Module.ko` exports (`__ksymtab_*`) at least:

| Symbol | Used by | Purpose |
|---|---|---|
| `stgNV2AC_sync_cmd` | `GetPubIdMod.ko`, `loadmod.ko`, `OA.ko` (via `nv2ac_read_data`) | Send a command to the Atmel chip, get a response |
| `stgNV2AC_sync_read_cmd` | same | Read N bytes from a given chip address |

Any kernel module loaded after `OmapNKS4Module.ko` can call these symbols. This is the
intended extension point — and the one we propose using for a small `oa_authgen.ko`
helper module that exposes the chip secret (or a generated auth string) to userspace
without modifying `OA.ko`. See [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md).

---

## Role

This module is the **NKS4 driver** — NKS4 being Korg's name for the front-panel and
hardware-integration board. It:

- Drives the OMAP-side audio buffering, MIDI port, USB MIDI accessory
- Drives the front-panel scanner (key matrix, button matrix)
- Drives the LED progress bar (`COmapNKS4_AddToProgressBar` etc.)
- Provides the FIFO interface (`OmapNKS4InputFifo_ReadCommand`, `OmapNKS4OutputFifo_WriteCommand`)
- **Encapsulates the NV2AC chip protocol** — sets up the GPIO/CPLD register sequence,
  enforces the GPA cipher around payloads, exposes `stgNV2AC_sync_*` to other modules

---

## Crypto

This module *implements* the bottom half of the GPA cipher and the NV2AC handshake.
The studying of GPA was done largely against `GetPubIdMod.ko` (smaller and
self-contained) — see [`GetPubIdMod.ko.md`](GetPubIdMod.ko.md).

---

## Status

| Item | Status |
|---|---|
| Phase 1 prototypes | 56 applied (of 72 attempted; 16 errors are template instantiations) |
| Phase 2 struct layouts | 0 built (no class-pattern field-access evidence — mostly C-style code) |
| Phase 3a return types | 20 refined |
| Versioned in Ghidra | No — a fresh, unpersisted Ghidra MCP session was run against the correct 89849-byte binary on 2026-07-17 (see below), but no `.gpr` project file was saved as part of that pass |
| Deep RE | Mostly not pursued (the exported symbols are well-known), **except** `CActiveSenseThread`/`CSTGOmapNKS4Fifos::TriggerOutputInterrupt`, freshly and fully decompiled 2026-07-17 — see `KronosNKS4/docs/gaps.md` |

The full chip protocol is in scope for future work if we want a userspace re-implementation
of `nv2ac_read_data`. The cleaner solution remains a helper `.ko` that just calls the
already-exported `stgNV2AC_sync_read_cmd`.

---

## Operational quirk — the panel-chip wedge

The front-panel NKS4 controller (USB `0944:1005`) has a firmware-state failure mode that
this module reports as:

```
OmapNKS4:WaitForNKS4ReadEvent: line 1029: WaitForNKS4ReadEvent() timed out
OmapNKS4:CommunicationCheck: line 208: Comm check - bad response, sent 0x00ee0000, rcvd 0x00000000
OmapNKS4:OmapNKS4Init: Problem configuring OmapNKS4 in Init
```

USB enumeration succeeds (probe finds vendor `0944`, product `1005`) but the proprietary
protocol comm-check times out. When this happens, `OmapNKS4Init` returns failure,
`loadoa` bails at this insmod step, and the boot dies with the "system cannot start"
reauth screen.

**Soft reboots (`reboot`, `reboot -f`) do not clear it.** The chip retains its bad state
across host reboots. USB-level resets via sysfs `authorized` toggle don't help either.
**Only a full power-cycle with the unit unplugged for ~60 s reliably clears it.**

We tried patching this module's cleanup path to call `usb_reset_device(udev)` (via ELF
surgery to add the `usb_reset_device` symbol import — see
[`../../tools/patch_omapnks4_cleanup.py`](../../tools/patch_omapnks4_cleanup.py)). It works
mechanically and demonstrates how to add new symbol imports to a relocatable kernel
module without breaking the ELF, but it doesn't actually solve the wedge — the chip
state survives a USB-level reset too. The patch is left as a reference; not deployed.

Full writeup in [`../modules/OmapNKS4Module.ko_chip_wedge.md`](../modules/OmapNKS4Module.ko_chip_wedge.md).

---

## Patches

None deployed. The ELF-surgery `usb_reset_device` import experiment is preserved at
[`../../tools/patch_omapnks4_cleanup.py`](../../tools/patch_omapnks4_cleanup.py) for future work
on adding kernel-symbol imports to relocatable `.ko` files.

---

## The other end of the wire

The USB `0944:1005` device this module talks to is not a dumb chip — it's a full
standalone embedded system (its own TI OMAP-L1x CPU, LCD controller, touch panel,
PSoC button/LED scan chip, and possibly its own AT88 access) running the firmware
shipped as `KRONOS_Vxxxxx.VSB`. See
[`KRONOS_V06R06.VSB.md`](KRONOS_V06R06.VSB.md) for what a disassembly pass of that
firmware turned up, including the authoritative switch/LED name table and
confirmation that the panel receives an 8bpp palette-indexed framebuffer (matching
`KronosScreenRemoteDaemon`'s `/dev/fb1` model) rather than RGB.
