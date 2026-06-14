# OmapNKS4Module.ko — reconstructed source

Reverse-engineered source for the Korg Kronos **`OmapNKS4Module.ko`** kernel module
(firmware 3.2.2). Target: **Linux 2.6.32.11 + RTAI**, x86-32, built with
**`g++ -mregparm=3 -fno-exceptions -fno-rtti`** (it is a C++ kernel module).

This is a *functionally faithful*, in-progress reconstruction recovered in Ghidra — not
a byte-for-byte rebuild. Struct layouts (field offsets/sizes) are exact; method bodies
are transcribed from the decompiler.

## What the module is

`OmapNKS4Module` is the **USB driver and real-time service task for the Kronos front
panel** — an OMAP-based "NKS4" board carrying the keybed, knobs/sliders/buttons, LEDs,
the colour LCD, the S/PDIF clock, and the **Atmel NV2AC security chip**. It:

- registers as a USB driver and binds the panel's bulk/interrupt endpoints;
- speaks a 32-bit-word **packet protocol** (`COmapNKS4Command`) to query versions,
  configure scanning/encoders/LEDs, drive the LCD, and read the security chip;
- decodes inbound event packets (`COmapNKS4Driver::ReceiveEventBuffer`), applies
  calibration + a sustain filter (`CNKS4EventFilter`), and forwards events to user
  space via **`/proc`** entries;
- runs an **RTAI real-time thread** (`CActiveSenseThread`) that paces output and fires
  the panel "active sense" tick;
- exposes a small **C-ABI** (`COmapNKS4Driver_*`, `stgNV2AC_*`) used by the rest of the
  Korg engine (e.g. the OA / GetPubId security path talks to the NV2AC chip through here).

## Architecture / subsystems

| Subsystem | Type(s) | File |
|---|---|---|
| Shared types / struct layouts | (all structs) | `omapnks4.h` ✅ |
| Framework externs (STG/RTAI/kernel), singletons | — | `omapnks4_internal.h` ✅ |
| Output FIFOs + active-sense RT thread + event filter | `CSTGOmapNKS4Fifos`, `CSTGOmapNKS4OutputFifo`, `CActiveSenseThread`, `CNKS4EventFilter` | `realtime.cpp` ✅ |
| Panel wire protocol (version/config/scan/LED/LCD/chip cmds) | `COmapNKS4Command` | `command.cpp` ✅ |
| Driver state machine, event decode, Atmel/NV2AC, C-ABI | `COmapNKS4Driver` | `driver.cpp` ✅ |
| Colour-LCD draw pipeline | `COmapNKS4VideoAPI` | `video.cpp` ✅ |
| USB probe/disconnect, URB write/interrupt callbacks | (free fns) | `usb.cpp` ✅ |
| `/proc` interface + event queue | (free fns) | `procfs.cpp` ✅ |
| URB submit/wait, signals, calibration | (free fns) | `submit.cpp` ✅ |
| `init_module`/`cleanup_module` + the two service threads | (free fns) | `main.cpp` ✅ |
| STG/RTAI veneer (`stg_*`, `rtwrap_*`, `CSTGThread`), C++ runtime | shared framework | imported, see notes |

All subsystems reconstructed. Everything is C++ (`.cpp`) — the `usb`/`procfs`/`submit`/
`main` units use only C-style code but call `Class::method`/singletons, so the whole
module compiles with one toolchain (matching the original `g++` build).

### Data model (recovered, exact)

- **`COmapNKS4Driver`** (40 B singleton) — hardware versions, key count, progress-bar
  state, flags (test mode, installer support, S/PDIF clock error, download/shutdown).
- **`COmapNKS4VideoAPI`** (12 740 B singleton) — a 384-entry ring of 33-byte LCD draw
  events + screen geometry; a worker pops events and emits USB video-bulk packets.
- **`CSTGOmapNKS4Fifos`** (1304 B singleton) — 256-deep host←panel event FIFO and
  64-deep host→panel command FIFO, drained by the kernel via an RTAI SRQ.
- **`CActiveSenseThread`** (28 B heap singleton) — TSC-paced 500 ms tick.
- **`OmapNKS4VideoAPIEvent`** / **`CNKS4EventFilter`** — see `omapnks4.h`.

## Building

This is one of the rare **C++ kernel modules**. It depends on the Korg "STG" support
layer (global ctor/dtor runner `init/cleanup_cpp_support`, `operator new/delete` over
`stg_kmalloc`, `__cxa_pure_virtual`) and on the `stg_*` / `rtwrap_*` symbols exported
by `STGEnabler.ko` and the RTAI core. Build flags mirror the original:

```make
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive
```

```sh
make KDIR=/mnt/tank/source/Kronos/linux-kronos
```

> **Status:** all eight translation units (~2150 lines) are reconstructed and
> brace-balanced. A handful of leaf helpers that belong to the shared STG layer
> (`kmalloc_buf`, `proc_set`, `wait_event*`, `block_all_signals`, `dev_shutdown`, the
> URB-config helpers in `usb.cpp`, the generic calibration curve) are referenced as
> externs / thin shims and noted inline — supply them from the STG framework headers,
> or replace with the stock kernel equivalents, to get a clean link.

## Fidelity notes

- The module is `-mregparm=3`: the first three int/pointer args are in EAX/EDX/ECX and
  C++ instance methods receive `this` in EAX. Reconstructed methods are plain C++; the
  ABI is reproduced by the build flags, not by hand.
- The `stg_*` / `rtwrap_*` / `CSTGThread` layer is the **shared** STG framework (same
  across `OA.ko`, `loadmod.ko`, …), so it is imported, not re-implemented here.
- Some panel-command payload bytes are assembled in the submit path; where the
  decompiler elides them they are reconstructed from the protocol behaviour and noted
  inline.
