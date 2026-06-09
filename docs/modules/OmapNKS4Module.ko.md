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
| Versioned in Ghidra | No |
| Deep RE | Not pursued — the only thing we needed (the exported symbols) is well-known |

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
