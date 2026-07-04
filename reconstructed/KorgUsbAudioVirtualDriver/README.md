# KorgUsbAudioVirtualDriver — software stand-in for KorgUsbAudioDriver.ko

A **new** component (no source ever existed for `KorgUsbAudioDriver.ko` in this
project before now) — a from-scratch stub providing the same exported symbol
surface as the real USB audio+MIDI codec driver, built so **`OA.ko` can `insmod`
and run without a real USB audio codec present**. Matches the convention of
`AT88VirtualChip/` alongside it: source lives here
(`kronosology/reconstructed/KorgUsbAudioVirtualDriver/`), the compiled `.ko`
gets copied/symlinked into `kronos_vm/` for actual VM boot testing.

## Why this is small

`KorgUsbAudioDriver.ko`'s real exported surface (confirmed via direct
inspection of the real binary,
`ARCHIVE/Ignored/DecryptedImages/MOD_Extracted/KorgUsbAudioDriver.ko`, never
decompiled anywhere in this project before) is only ~20 symbols, and every one
of the audio-family functions OA.ko actually calls takes **no arguments at
all** — confirmed by disassembling several of the real functions directly:

```
KorgUsbAudioOutput      .text+0x310  (21B)  eax = base + stride*index (a pointer accessor)
KorgUsbAudioInput       .text+0x2d0  (21B)  same shape
KorgUsbAudioInitialized .text+0x350  (8B)   eax = a byte flag, no args
KorgUsbAudioOutputDone  .text+0x330  (28B)  advances/wraps a ring index, no args
KorgUsbAudioInputStarving .text+0x270 (43B) ring-fullness check, no args
KorgUsbAudioStart       .text+0x230  (49B)  0=success, else a status code (3="not initialized")
KorgUsbAudioDone        .text+0x630  (128B) status code, checks 3 internal flags
```

Every real call site into this family (from `CSTGAudioDriverInterfaceKorgUsb`,
already reconstructed in `reconstructed/OA/include/oa_engine.h`/`src/engine/
managers.cpp`) operates on the driver's own internal static state, not
arguments passed in from OA.ko. This makes a faithful-shape stub tractable
without reverse-engineering a real USB audio protocol.

## The critical finding: this stub doesn't actually gate `init_module`'s success

`OA.ko`'s `init_module` step 13 (`CSTGAudioManager_StartAudioEngine`, a hard
`insmod`-time fail per `MASTER_REFERENCE.md` sec 10.17) was assumed to depend
directly on `KorgUsbAudio*` symbols. Full disassembly of the real call chain
(`CSTGAudioManager::StartAudioEngine()` → virtual dispatch through
`CSTGAudioDriverInterface::sInstance` → `CSTGAudioDriverInterfaceKorgUsb::
Start()` → `KorgUsbAudioStart`) shows this is **not the case**:

- `CSTGAudioDriverInterfaceKorgUsb::Start()`'s vtable slot is called
  unconditionally from `CSTGAudioManager::StartAudioEngine()`'s "everything
  else already succeeded" path — but the CALLER discards `Start()`'s own
  return value entirely (`mov eax,0x1` immediately after the call, regardless
  of what it returned).
- The actual success/failure of `CSTGAudioManager::StartAudioEngine()` (and
  hence `init_module` step 13) is gated on **three internal
  `CSTGThread::CreateRealTimeWithCPUAffinity` calls succeeding** — an
  OA.ko-internal RTAI real-time-thread-creation mechanism with **no
  KorgUsbAudio dependency at all**.

So this stub exists to satisfy `OA.ko`'s **link-time** symbol dependency
(`insmod` needs every undefined symbol resolved by an already-loaded module,
or it refuses to load at all) and to provide plausible behavior if these
functions are ever actually *called* at runtime — not because getting their
return values "right" is what makes `init_module` succeed. The real remaining
gate for step 13 is `CSTGThread::CreateRealTimeWithCPUAffinity`, a separate,
not-yet-investigated dependency worth checking next.

## A real discrepancy found, not resolved

Real extracted source (`kronosology/loadoa/loadoa.c`, step comments 15/16)
shows `OA.ko` loading **before** `KorgUsbAudioDriver.ko` on real hardware —
but `OA.ko`'s undefined `KorgUsbAudio*` symbols are `GLOBAL` (not `WEAK`),
which should make that ordering impossible for a normal Linux module load
(the kernel resolves undefined symbols against already-loaded modules only,
refusing to `insmod` otherwise). This contradicts `MASTER_REFERENCE.md` sec 6's
own load-order table, which claims the opposite order. Not resolved in this
pass — doesn't block this stub (a VM/test boot can simply load this module
*before* `OA.ko`, regardless of what order real hardware uses), but worth
investigating if real-hardware-accurate load ordering ever matters.

## Scope

- **In scope, exported here**: the `KorgUsbAudio*` and `KorgUsbMidi*`/
  `KorgUsbRealtimeMidiOutput*` families — confirmed via `readelf -sW` on the
  real binary to be ONE combined audio+MIDI driver, not two separate modules.
- **Out of scope**: `USBMidiAccessory_SetDrumPadClient`/
  `USBMidiAccessory_SetMidiInClient` — confirmed absent from
  `KorgUsbAudioDriver.ko`'s real export list; these belong to the separate
  `USBMidiAccessory.ko` module (per `loadoa.c`/`MASTER_REFERENCE.md`), not
  reconstructed here.

## Layout

```
KorgUsbAudioVirtualDriver/
  README.md              this file
  korgusbaudio_stub.h     shared state + declarations (freestanding, host-testable)
  korgusbaudio_stub.cpp   the no-op/always-ready implementations
  module_main.cpp         kernel-only glue (EXPORT_SYMBOL, module_init/exit)
  Makefile                same dual host+Kbuild convention as AT88VirtualChip
  verify/test_korgusbaudio_stub.cpp
```

## Status

**Done and KAT-verified** (`verify/test_korgusbaudio_stub.cpp`, all checks
passing): the confirmed-shape audio family (`Initialize`/`Initialized`/
`Start`/`Done`/`Output`/`Input`/`OutputDone`/`InputDone`/`*Starving`) and the
inferred-shape MIDI family, all returning success/ready values. Kernel-module
scaffolding (`module_main.cpp`, `EXPORT_SYMBOL` for all 20 exported symbols,
Kbuild-compatible `Makefile`) done, matching `AT88VirtualChip`'s own
established pattern.

**Not yet done**: the real `.ko` build has not been verified against a real
kernel tree/`KDIR` (same honest scope boundary `AT88VirtualChip` flagged for
itself). The `CSTGThread::CreateRealTimeWithCPUAffinity` dependency this
investigation surfaced as the REAL step-13 gate has not been investigated —
that, not this stub, is the natural next piece of the boot-to-UI effort.
