# KorgUsbAudioVirtualDriver - software stand-in for KorgUsbAudioDriver.ko

A from-scratch stub that provides the same exported symbol surface as the
real USB audio+MIDI codec driver (`KorgUsbAudioDriver.ko`), so that `OA.ko`
can `insmod` and run without a real USB audio codec present. No source for
`KorgUsbAudioDriver.ko` exists elsewhere in this project; its exported
surface was reconstructed directly from the compiled binary. Source for this
stub lives at `kronosology/reconstructed/KorgUsbAudioVirtualDriver/`,
following the same dual host+Kbuild convention as
`reconstructed/AT88VirtualChip/`.

## Why the stub is small

The real driver's exported surface is only about 20 symbols, and every
audio-family function `OA.ko` actually calls takes no arguments at all -
established by disassembling the real functions directly:

```
KorgUsbAudioOutput        .text+0x310  (21B)  eax = base + stride*index (a pointer accessor)
KorgUsbAudioInput         .text+0x2d0  (21B)  same shape
KorgUsbAudioInitialized   .text+0x350  (8B)   eax = a byte flag, no args
KorgUsbAudioOutputDone    .text+0x330  (28B)  advances/wraps a ring index, no args
KorgUsbAudioInputStarving .text+0x270  (43B)  ring-fullness check, no args
KorgUsbAudioStart         .text+0x230  (49B)  0=success, else a status code (3="not initialized")
KorgUsbAudioDone          .text+0x630  (128B) status code, checks 3 internal flags
```

Every real call site into this family, from `CSTGAudioDriverInterfaceKorgUsb`
(reconstructed in `reconstructed/OA/include/oa_engine.h` and
`reconstructed/OA/src/engine/managers.cpp`), operates on the driver's own
internal static state, not on arguments passed in from `OA.ko`. This makes a
faithful-shape stub tractable without reverse-engineering a real USB audio
protocol: the stub only needs to reproduce each function's no-argument
signature and a plausible return value, not any actual audio I/O.

## init_module step 13 and this stub's role

`OA.ko`'s `init_module` sequence has a step (`CSTGAudioManager_StartAudioEngine`)
that hard-fails `insmod` if it does not succeed. It would be reasonable to
assume that step depends directly on the `KorgUsbAudio*` symbols this stub
provides, but disassembly of the real call chain
(`CSTGAudioManager::StartAudioEngine()` -> virtual dispatch through
`CSTGAudioDriverInterface::sInstance` -> `CSTGAudioDriverInterfaceKorgUsb::
Start()` -> `KorgUsbAudioStart`) shows that is not the case:

- `CSTGAudioDriverInterfaceKorgUsb::Start()`'s vtable slot is called
  unconditionally from `CSTGAudioManager::StartAudioEngine()`'s
  "everything else already succeeded" path, but the caller discards
  `Start()`'s return value entirely (`mov eax,0x1` immediately after the
  call, regardless of what it returned).
- The actual success or failure of `CSTGAudioManager::StartAudioEngine()`
  (and hence of `init_module` step 13) is gated on three internal
  `CSTGThread::CreateRealTimeWithCPUAffinity` calls succeeding - an
  OA.ko-internal RTAI real-time-thread-creation mechanism with no
  `KorgUsbAudio` dependency at all.

So this stub exists to satisfy `OA.ko`'s link-time symbol dependency
(`insmod` requires every undefined symbol to resolve against an
already-loaded module, or it refuses to load at all) and to provide
plausible behavior if these functions are ever actually called at runtime -
not because getting their return values "right" is what makes
`init_module` succeed. The real gate for step 13 is
`CSTGThread::CreateRealTimeWithCPUAffinity`, a separate dependency this
stub does not address.

## Scope

- **In scope, exported here**: the `KorgUsbAudio*` and
  `KorgUsbMidi*`/`KorgUsbRealtimeMidiOutput*` families. `readelf -sW` on the
  real binary shows these all belong to one combined audio+MIDI driver, not
  two separate modules.
- **Out of scope**: `USBMidiAccessory_SetDrumPadClient` and
  `USBMidiAccessory_SetMidiInClient` are absent from
  `KorgUsbAudioDriver.ko`'s real export list; per `loadoa/loadoa.c` these
  belong to the separate `USBMidiAccessory.ko` module and are not
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

The confirmed-shape audio family (`Initialize`/`Initialized`/`Start`/`Done`/
`Output`/`Input`/`OutputDone`/`InputDone`/`*Starving`) and the inferred-shape
MIDI family are implemented, all returning success/ready values, and are
exercised by `verify/test_korgusbaudio_stub.cpp`. Kernel-module scaffolding
(`module_main.cpp`, `EXPORT_SYMBOL` for all 20 exported symbols, a
Kbuild-compatible `Makefile`) is in place, matching
`reconstructed/AT88VirtualChip/`'s own build pattern.

## Known limitations

- **The real `.ko` build has not been verified against a real kernel
  tree/`KDIR`.** The host-side build only compiles each unit as a
  freestanding object for the known-answer tests; the Kbuild path that
  produces the real `.ko` has not been exercised against a kernel tree
  matching the Kronos's own module ABI. *To validate:* build with `make ko
  KDIR=<kronos-kernel-tree>` against a correctly configured
  Linux 2.6.32.11-korg tree and confirm the resulting module loads.
- **`CSTGThread::CreateRealTimeWithCPUAffinity` is not yet investigated.**
  This is the real gate on `init_module` step 13 (see above), and is a
  separate, self-contained dependency from this stub. *To validate:*
  disassemble `CSTGThread::CreateRealTimeWithCPUAffinity` and its RTAI
  real-time-thread-creation call chain to determine what conditions cause
  it to fail under emulation.
- **The real-hardware module load order is unconfirmed and possibly
  contradictory.** `loadoa/loadoa.c` shows `OA.ko` loading before
  `KorgUsbAudioDriver.ko` on real hardware, but `OA.ko`'s undefined
  `KorgUsbAudio*` symbols are `GLOBAL`, not `WEAK` - which should make that
  load order impossible for a normal Linux module load, since the kernel
  only resolves undefined symbols against already-loaded modules and
  otherwise refuses to `insmod`. This does not block using this stub (a
  test boot can simply load this module before `OA.ko`, regardless of what
  order real hardware uses), but the discrepancy itself is unresolved.
  *To validate:* re-check the real boot log's module load order and the
  real `KorgUsbAudioDriver.ko`/`OA.ko` symbol tables' bind types directly,
  to determine whether the apparent ordering is real or a
  misread of the evidence.
