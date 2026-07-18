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
>
> **`verify/` added 2026-07-15** (`make verify`, no kernel tree needed) — the first
> time `command.cpp`/`driver.cpp` were actually compiled since transcription. Caught
> and fixed two real pre-existing compile bugs (a macro collision, an arity mismatch
> in `COmapNKS4Driver_ShutDown`) plus locked in five setter-word-encoding fixes and a
> `ReceiveEventBuffer` byte-packing fix, all found via a fresh Ghidra decompile +
> disassembly pass the same day. See `verify/README.md` and
> `KronosNKS4/docs/gaps.md` for the full writeups.
>
> **First real Kbuild attempt, same day** (build server `192.168.3.92`,
> `/home/build/linux-kronos`) — this module had never actually been run through the
> real Kbuild/toolchain before. Found and fixed three genuine header-compatibility
> bugs blocking EVERY translation unit (`omapnks4.h` unconditionally included
> `<linux/usb.h>` and `<linux/types.h>`, neither of which anything in this module
> actually uses — both pull in constructs this kernel's headers can't parse as C++;
> `omapnks4_internal.h` was missing a placement-`new` declaration `video.cpp` needs).
> With those fixed, `realtime.o`/`command.o`/`driver.o`/`video.o` (4 of 8 units)
> compile clean via the real toolchain for the first time. Also found and fixed two
> genuinely MISSING function implementations (not just declarations) that blocked
> `usb.o`: `COmapNKS4Driver_ReceiveEventBuffer` (a real, separate exported symbol —
> confirmed via fresh decompile to be a compiler-cloned duplicate of
> `ReceiveEventBuffer`'s own logic, which independently re-confirmed this session's
> earlier `ReceiveEventBuffer` fixes) and `COmapNKS4_SetMaxBulkOutMsgSize` (only ever
> forward-declared, never implemented — turns out to set the video pixel-chunk size
> to the real negotiated USB max-packet-size at runtime, refining the "fixed
> **Still blocking a full link**: `usb.cpp` (and likely `procfs.cpp`/`submit.cpp`/
> `main.cpp`, not yet attempted) reference ~15 more STG-framework/real-kernel
> functions (`stg_usb_submit_urb`/`_alloc_urb`/`_free_urb`/`_deregister`,
> `kmalloc_buf`, `__wake_up`, `remove_proc_entry`, `__init_waitqueue_head`,
> `complete`, `rt_free_srq`) that were never declared anywhere in this
> reconstruction, plus three local helper functions (`configure_interrupt_urb`,
> `alloc_command_urbs`, `alloc_video_urbs`) that are CALLED but never actually
> implemented at all.
>
> **Continued the same day**: declared the ~13 remaining STG-framework/real-kernel
> externs `usb.cpp` needed using this project's own already-boot-tested pattern for
> real kernel primitives with C++-unparseable types (opaque `void*` + raw byte
> storage sized to match, established in `OA/src/init/daemon_lifecycle.cpp` - see
> that file for the precedent this reused verbatim). Also fully reconstructed the
> three URB-pool helper functions (`configure_interrupt_urb`, `alloc_command_urbs`/
> `alloc_video_urbs`, unified into one shared `alloc_urb_pool`) from a fresh
> decompile of `OmapNKS4Probe` itself - ground truth showed the real binary has
> **no separate helper functions at all**, this logic is fully inlined into
> `OmapNKS4Probe`; this reconstruction's own decision to factor it into named
> helpers was reasonable, the bodies were just never written. **Result: `usb.o`
> now compiles clean except for exactly one deliberately-deferred error**
> (`stg_usb_deregister` needs a real `struct usb_driver` object this codebase has
> never constructed anywhere - confirmed by also finding `main.cpp`'s
> `stg_usb_register_driver()` call is equally incomplete). That's 5 of 8
> translation units effectively done pending that one open architectural question.
>
> **THE MILESTONE, same session: `OmapNKS4Module.ko` links successfully for the
> first time ever.** Finished the remaining three files:
>
> - **Diagnosed and fixed the `main.cpp` parse error**: `__init`/`__exit` and
>   `module_init`/`module_exit`/`MODULE_LICENSE`/`MODULE_DESCRIPTION`/
>   `MODULE_AUTHOR` are normally macros from `<linux/init.h>`/`<linux/module.h>` -
>   unusable here for the same C++-incompatibility reason as every other real
>   kernel header this session. Replaced with minimal, functionally-correct local
>   definitions (`__init`/`__exit` as real section-placement attributes matching
>   this build's own confirmed `.init.text`/`.exit.text` ELF sections;
>   `module_init`/`module_exit` as thin `init_module()`/`cleanup_module()` wrapper
>   functions, the real simplified `-DMODULE` expansion Kbuild always uses for an
>   out-of-tree module).
> - **Fully reconstructed this module's own `struct usb_driver` object** - the real
>   architectural gap flagged in the previous entry. Ground truth: disassembled
>   `OmapNKS4Init`'s real `stg_usb_register_driver(driver, owner, mod_name)` call
>   site to find `driver`'s fixed static address, then `read_memory`'d that address
>   directly and cross-checked every field byte-for-byte against `linux-kronos`'s
>   real `<linux/usb.h>`/`<linux/device.h>` struct layouts (not guessed): `name`
>   ("OmapNKS4"), `probe`/`disconnect` (confirmed to match `OmapNKS4Probe`/
>   `OmapNKS4Disconnect`'s real addresses exactly), one real `usb_device_id` entry
>   (vendor `0x0944`/product `0x1005`/vendor-specific interface class `0xff`), and
>   confirmed every field from `usb_dynids` onward is zero in the real binary too
>   (populated by `usb_register_driver()` itself, not the driver author) - computed
>   the exact total struct size (112 bytes) from the real kernel source rather than
>   over-provisioning a guess, since a too-small buffer here would have been a real
>   memory-safety risk once `usb_register_driver()` started writing into it.
> - **`rt_request_srq`/`wait_for_completion_timeout`'s real arguments**, recovered
>   via the same disassembly (SRQ label `"NKS4"` packed as a 4-char tag - the same
>   convention already established in `OA.ko`'s own daemon-watchdog code; SRQ
>   handler = `COmapNKS4Driver_HandleOutputSysReq`; probe-wait timeout = 10000
>   jiffies, confirmed exactly via the real `MOV EDX,0x2710` immediate).
> - **Declared the real `current`-task accessor** by reusing `OA.ko`'s own
>   already-boot-tested `stg_get_current_task()` (`reconstructed/OA/src/stub/
>   bar2_stubs_c.cpp`) verbatim rather than re-deriving it - that file's own
>   extensive comment explains why it references the real `per_cpu__current_task`
>   kernel symbol by name (portable across kernel rebuilds) instead of a
>   hardcoded per-CPU offset.
> - **`procfs.cpp` and `submit.cpp`** needed ~30 more declarations between them -
>   real libc/kernel primitives (`sprintf`, `copy_from_user`, `strstr`, `kfree`,
>   `simple_strtoul`, `strcspn`, this kernel's own `_spin_lock`/`_spin_unlock`
>   spinlock primitives), a from-scratch `create_proc_entry`/`proc_set`
>   implementation (real `struct proc_dir_entry`'s `read_proc`/`write_proc` field
>   offsets computed from the real kernel header's field layout), `struct urb_node`
>   moved from `usb.cpp`-local to shared (submit.cpp needed the complete type, not
>   a forward declaration), and several `__wake_up`/`schedule_timeout_wait`/
>   `sleep_on_timeout` call sites modeled as short-sleep polling (consistent with
>   this file's own already-established `WaitForFreeBulkWriteURB` polling
>   simplification) rather than real wait-queue blocking, clearly flagged as
>   lower-confidence than the disassembly-verified fixes above.
> - **One more genuine pre-existing bug found only at link time**:
>   `sMaxWritePacketSize` was defined as a real (non-`extern`) global in BOTH
>   `usb.cpp` and `submit.cpp` - a duplicate-definition linker error that could
>   never have been caught before since the module had never actually been linked.
>
> **Result**: `make ko KDIR=/home/build/linux-kronos` produces a complete,
> valid `OmapNKS4Module.ko` (35608 bytes, ELF 32-bit relocatable, `vermagic:
> 2.6.32.11-korg SMP preempt mod_unload ATOM` - exactly matching the real target
> kernel) for the first time in this reconstruction's history. Its 75 unresolved
> symbols are all legitimate external dependencies resolved at `insmod` time
> against the real kernel/`STGEnabler.ko`/RTAI (plus GCC's own `-msoft-float`
> runtime helpers, e.g. `__muldf3`) - no accidental/unintended externs. A handful
> of items remain honestly flagged as lower-confidence best-effort (the SCSI
> SSD-shutdown enum values, some thread-join completion wiring, `proc_dir_entry`'s
> field offsets computed from struct layout math rather than disassembly-verified -
> **all three were revisited and resolved 2026-07-17, see "Continued RE" below**) -
> see `KronosNKS4/docs/gaps.md` "Real Kbuild build attempt" for the
> complete, itemized confidence breakdown. Host `verify/` suite re-run clean throughout, with
> one incidental fix: the new libc-primitive declarations needed an `__KERNEL__`
> guard to avoid colliding with the host's real glibc when building `verify/`'s
> host test binaries.
>
> **Live VM insmod test, same day (`kronosvm`, 192.168.3.87)**: attempted to
> actually load the freshly-linked `OmapNKS4Module.ko` in the project's existing
> `kronos_vm` boot-test environment (the same one `OA.ko`'s own milestone boot
> used - see `MASTER_REFERENCE.md` sec 10.237). Result: **a genuine, well-diagnosed
> blocker was found, distinct from anything in this module's own source** -
> `insmod` cleanly reports ~37 unresolved symbols by exact name (not a crash, not
> a hang - proof the `.ko` itself is well-formed). Root-caused via
> `/proc/kallsyms`: `stg_kmalloc`/`stg_msleep`/`stg_get_cpu_khz`/
> `stg_sched_setscheduler`'s full argument set/the `rtwrap_*` RTAI wrapper family/
> the C++ runtime shim (`init_cpp_support`/`cleanup_cpp_support`/`operator new`/
> `delete`/`CSTGThread`)/`create_thread` are **never actually implemented or
> exported anywhere in this repo's `kronosology/reconstructed/` tree** -
> `STGEnabler.ko` (the module `omapnks4_internal.h`'s own header comment
> attributes these to) only implements a *different* subset (USB pass-through,
> scheduler/cpumask, RTAI timer bring-up, VFS helpers - confirmed by reading its
> full source, zero matches for any of the above). The only place `stg_kmalloc`
> exists at all in a live kernel is as `OA.ko`'s own **private, non-exported**
> (`t`, lowercase - local, not global) internal copy, useless to any other
> module. This is a real, previously-undocumented gap in the wider project's
> shared "STG core runtime" reconstruction - out of scope to fix from within
> `OmapNKS4Module` itself, since it's foundational infrastructure `OA.ko`,
> `loadmod.ko`, and every other STG-based module would also need. Two smaller,
> genuinely-fixed-live findings along the way: (1) `STGEnabler.ko` itself failed
> to load under this VM's `FAST_RTAI` fast-boot mode because `stg_rtai_setup()`
> calls three real RTAI primitives (`start_rt_timer`/`rt_linux_use_fpu`/
> `rt_set_oneshot_mode`) the existing `rtai_stub.ko` doesn't provide - worked
> around with a throwaway 3-symbol stub module (not committed, VM-scratch only);
> (2) confirmed real (non-`FAST_RTAI`) RTAI calibration under this project's
> standard `qemu-system-i386`/TCG boot reproduces the same 10+ minute stall this
> project's own `MASTER_REFERENCE.md` had already documented (sec 10.232-era
> issue) - unrelated to this session's work, a pre-existing environment
> characteristic. VM scratch directory removed after use; no changes made to the
> canonical `kronos_vm/kronos.img`.

## Continued RE, 2026-07-17

Fresh Ghidra decompile + disassembly pass against real physical NKS4 hardware access
(see `KronosNKS4/docs/gaps.md` for the full session writeup), picking off items this
README had previously flagged as lower-confidence best-effort:

- **`SendPixelDataRegion`'s "third field" discrepancy, resolved**: the 2026-07-15 pass
  had found the decompiler typing the pixel-region event's offset+9 field as a
  `COmapNKS4VideoAPI*` (`this`) and concluded the reconstruction's 3rd parameter
  (`rowBytes`) was "likely wrong/meaningless." Fresh raw disassembly of both compiled
  variants of the function (the thiscall C++ method and the plain regparm3 C wrapper)
  proves the decompiler simply mistyped that one stack/register slot - it's a genuine
  3rd `int` argument in both call shapes, and tracing it back to `OmapVideoModule.ko`'s
  `omapfb_ioctl`/`OMAPFB_FLUSH` call site confirms it's exactly the row-width field of
  the already-reverse-engineered `struct omapfb_flush {count, offset, width}`. This
  file's own `SendPixelDataRegion(int width, int offset, int rowBytes)` and
  `ContinueProcessingEvent`'s row-wraparound math (`video.cpp`) were already correct -
  built right independent of this ambiguity ever being resolved by disassembly.
- **SCSI SSD-shutdown, corrected**: `ShutdownSSDRoutine` (`main.cpp`) previously
  modeled an open-ended "walk every SCSI host, shut down every device" loop with
  guessed `scsi_device_set_state` values (6, 1). Ground truth is different: exactly
  four fixed host indices are tried in order (0,1,2,3, not a loop bound by
  `scsi_host_lookup` returning null), stopping at the first one with a device at
  channel/id/lun 0; the two state values are `SDEV_CANCEL=3` then `SDEV_DEL=4`
  (confirmed against `linux-kronos`'s own `scsi_device.h` enum) - the same two-step
  sequence the kernel's own `__scsi_remove_device()` uses. The final `msleep` was also
  corrected from a guessed 500ms to the real, disassembly-confirmed 1000ms.
- **`OmapNKS4Probe`'s URB pool sizes, pinned down**: 16 command URBs, 256 video URBs
  (this project had never counted them exactly before) - see `KronosNKS4/docs/gaps.md`
  for why this matters to the real-hardware display-stall investigation.
- **`proc_dir_entry`'s `read_proc`/`write_proc` offsets, corrected**: were `+0x3c`/
  `+0x40` (`procfs.cpp`/`omapnks4_internal.h`), off by exactly one dword. The
  original struct-layout math assumed `mode_t`/`nlink_t`/`uid_t`/`gid_t` were all
  4 bytes; `linux-kronos`'s own `asm/posix_types_32.h` has `mode_t`/`nlink_t` as
  2-byte `unsigned short` (only `uid_t`/`gid_t` are the 4-byte `_uid32_t`/`_gid32_t`
  variants), which explains the +4 error exactly. Confirmed two ways: a host-compiled
  `offsetof()` test against the kernel's own exact typedefs gives `0x38`/`0x3c`, and
  fresh disassembly of the real binary's own `OmapNKS4ProcInitialize` shows the
  literal `MOV [EAX+0x38],<read_fn>; MOV [EAX+0x3c],<write_fn>` store pattern after
  each of its four `create_proc_entry()` calls - no longer struct-layout math alone,
  disassembly-verified like the rest of this session's fixes.
- **Thread-join completion wiring, resolved**: the "known simplification, not a
  fully ground-truthed thread-join protocol" this file used to flag turned out to be
  an unfounded worry, not a real bug - `create_thread`'s startup-sync completion is
  always a stack-local object per invocation, structurally distinct from the static
  `sMsgThreadComplete`/`sSsdThreadComplete` used at real exit, so no race between
  "thread started" and "thread exited" signals was ever possible. What genuinely was
  wrong: `OmapNKS4Exit`'s two `wait_for_completion_timeout()` calls were guessed at
  10000 jiffies (10s) by analogy with the real, disassembly-confirmed 10000-jiffy
  `sProbeComplete` wait elsewhere - fresh disassembly of `OmapNKS4Exit@0x18f1d` shows
  the real immediate is `0x7d0` (2000 jiffies = 2s) for both exit-join waits, an
  unrelated constant from the probe-wait one. Fixed in `main.cpp`.
- Rebuilt clean against the real toolchain (`make ko KDIR=/home/build/linux-kronos`,
  build server `192.168.3.92`) after each of the four fixes above - `OmapNKS4Module.ko`
  links with the correct vermagic (`2.6.32.11-korg SMP preempt mod_unload ATOM`), only
  the same pre-existing `-fpermissive` warnings as before, no new errors. Build-server
  artifacts cleaned up after each build. **All three items this README's "Fidelity
  notes"/Kbuild-attempt changelog had flagged as lower-confidence best-effort are now
  resolved** (SCSI enum values, `proc_dir_entry` offsets, thread-join wiring) -
  nothing outstanding from that list.

## Live VM boot test, 2026-07-17 (session 4): the real module's own init_module() runs to completion

Following the re-verification pass below, attempted this module's own first-ever live
insmod test with a REAL RTAI-substitute stack (not the `OmapNKS4VirtualDriver.ko`
stand-in stub, which exists specifically because this exact test was believed
infeasible - see that module's own header comment). Load order:
`RTAIVirtualDriver.ko` → `STGEnabler.ko` → `STGGmp.ko` → `OmapNKS4Module.ko` (this
project's own dedicated `kronosvm` sandbox, 192.168.3.87).

**Two real build-time bugs found and fixed before the module would even link clean**:
- A self-inflicted comment-termination bug (`rt_*/nano2count_cpuid` inside a `/* */`
  block - the literal `*/` sequence closed the comment early, turning the rest into
  malformed code) from the earlier re-verification pass's own correction comments.
- Two dangling `extern`s introduced by that same pass (`sOmapNKS4DriverShutdownArg`,
  `sRtheapOwnerToken`) had no real definition anywhere - a genuine "Unknown symbol"
  insmod failure waiting to happen. Fixed: the first is real internal BSS state
  (defined locally); the second's real address falls inside this binary's own
  `EXTERNAL` segment and resolves to `&__this_module` (a symbol this project's own
  code already uses elsewhere).

**RTAIVirtualDriver.ko extended** with ~17 additional real RTAI symbols this module's
own `rtwrap.cpp` needs beyond OA.ko's original 26+3 (`rt_cond_wait` family,
`rt_sem_broadcast`, `rt_sem_wait_timed`/`_barrier`, `rt_sched_lock`/`unlock`,
`rt_get_priorities`/`_time_cpuid`, `nano2count`/`_cpuid`, `rt_sleep`,
`set_debug_traps_in_rt_task`, `rt_task_masked_unblock`, plus the `_nano2count_cpuid`/
`rtai_cpu_lock` per-CPU data symbols `rtwrap.cpp`'s save/restore-flags functions
reference directly) - see that module's own README for the full list and honest
regparm-convention caveats. Also found and fixed a real `rtheap_alloc` ABI mismatch:
this project's own re-verification pass had corrected it to 2 arguments, but
`RTAIVirtualDriver.ko`'s already-tested, shared implementation (relied on by OA.ko too)
exports the real 3-argument form (`heap, size, flags`) - aligned this module's call
sites to match rather than diverge from tested shared infrastructure.

**One real "Unknown symbol cpu_online_map" build-environment gap**: confirmed real
ground truth for the actual shipped Kronos kernel, but this project's own
`/home/build/linux-kronos` tree doesn't export it as a standalone symbol on its current
config. Fixed by reading through `cpu_online_mask` (this tree's own real, exported
replacement) instead - the identical "count of online CPUs" value, not a fidelity
regression, an environment accommodation.

**One real 64-bit-division kernel-build bug** in `RTAIVirtualDriver.c`'s own new
`rt_sleep` addition: plain `long long / long long` on a runtime value needs GCC's
`__divdi3` helper, unavailable in 32-bit kernel space - a genuine "Unknown symbol
__divdi3" insmod failure on the first live attempt, fixed with the kernel's own
`do_div()` macro.

**Result, after all of the above**: `OmapNKS4Module.ko` insmod's real `init_module()`
runs its ENTIRE reconstructed logic correctly on real (virtual) RTAI infrastructure:
C++ runtime init, completion-struct init, RTAI SRQ request, real USB core driver
registration (`usbcore: registered new interface driver OmapNKS4` - genuine kernel USB
subsystem output, not this project's own printk), the 10-second
`wait_for_completion_timeout(sProbeComplete, 10000)` probe-wait runs its real, full
duration and times out exactly as designed (no real/virtual NKS4 USB device was
attached in this test - see `OmapNKS4VirtualBoard/README.md` for why the gadget-based
virtual device attempt hit its own, separate TCG-compatibility blocker), and the module
fails **gracefully** with the real, reconstructed cleanup path
(`OmapNKS4: probe failed` → `usbcore: deregistering interface driver OmapNKS4`) - no
crash, no hang, no kernel oops. Confirmed via two independent boot tests (with and
without the gadget layer in the load order) that this exact result is reproducible and
not an artifact of the gadget attempt's own separate stall.

This is the module's own real reconstructed `init_module()`/probe-wait/cleanup logic
running correctly against real (virtual) infrastructure for the first time - the
practical ceiling reachable without a working virtual USB device attached, and a
genuinely meaningful "reasonably boots in a VM" result on its own merits.

## `vm_virtual_probe`: a working virtual NKS4 board, later the same day

The gadget-based virtual device (`OmapNKS4VirtualBoard.ko` + `dummy_hcd.ko`) hit a
genuine TCG-incompatibility wall: `dummy_hcd.ko`'s own root-hub enumeration hangs in
this VM/QEMU-TCG environment (confirmed via two independent isolation tests - see
`OmapNKS4VirtualBoard/README.md`). Rather than keep debugging that blind (no
symbol-resolved `vmlinux` available for GDB), this module gained a **self-contained**
alternative: a `vm_virtual_probe` module parameter (hand-rolled `module_param(int)` -
see main.cpp's own comment; default 0, completely inert on real hardware) that, when
set, synthesizes a virtual NKS4 device **in-process** and feeds it to the real,
unmodified `OmapNKS4Probe()` - no dummy_hcd, no second module, no real USB bus at all.

**Why not a separate injector module (tried first, and why it can't work)**: a first
attempt (`OmapNKS4ProbeInject.ko`, this project's own directory, superseded) exported
`OmapNKS4Probe` and tried to invoke it from a second module while
`OmapNKS4Module.ko`'s own `init_module()` was still blocked in its 10-second
probe-wait. This hit a real kernel message on the first live test: `"OmapNKS4ProbeInject:
gave up waiting for init of module OmapNKS4Module."` - the kernel's own module loader
(`kernel/module.c`) refuses to resolve a symbol exported by a module still in
`MODULE_STATE_COMING` (i.e. still inside its own `init_module()`, which is exactly the
function doing the waiting). No amount of timing/retry fixes that; it's a hard rule.
Calling `OmapNKS4Probe()` **inline**, from inside `OmapNKS4Init()` itself, before its
own `wait_for_completion_timeout()`, sidesteps it entirely - `OmapNKS4Probe()`'s own
`complete(&sProbeComplete)` just pre-satisfies the completion the wait is about to
check (a well-defined complete-before-wait ordering, not a race).

**Why a hand-built fake `usb_interface` works at all**: `OmapNKS4Probe()`'s own logic
(usb.cpp) never dereferences a real kernel `usb_interface`/`usb_device` through their
real accessors - it reinterprets the pointer as raw ints at fixed offsets recovered
from the shipping binary (`intf[0]`=altsetting, `intf[7]`=usb_device*, `dev+0xb8/+0xba`
=idVendor/idProduct, `altsetting+4`=bNumEndpoints, `altsetting+0xc`=endpoint array,
0x2c bytes/entry). Correctly-shaped plain `kmalloc`'d memory satisfies it exactly as
well as a real enumerated device would. `vm_virtual_probe_inject()` (usb.cpp) builds
that memory with vendor 0x0944 / product 0x1005 / one interrupt-IN (0x81) + one
bulk-OUT (0x02), matching `OmapNKS4VirtualBoard.c`'s own descriptors.

**Live VM boot test, 2026-07-17, `insmod OmapNKS4Module.ko vm_virtual_probe=1`**
(after `RTAIVirtualDriver.ko`/`STGEnabler.ko`/`STGGmp.ko`, no gadget layer at all):

```
OmapNKS4: vm_virtual_probe=1, synthesizing a virtual NKS4 board
OmapNKS4: vm_virtual_probe: calling OmapNKS4Probe() with a synthetic vendor=0944 product=1005, 1 int-IN + 1 bulk-OUT ep
OmapNKS4: probe success
OmapNKS4: vm_virtual_probe: OmapNKS4Probe() returned 0
OmapNKS4: Waited for OmapNKS4Probe(). driver state is 1        <- first time ever "1", not "0"
OmapNKS4: DIAG interrupt submit rc=0
OmapNKS4: vm_virtual_probe: queuing CommunicationCheck reply
OmapNKS4: DIAG submit_urb_words: ... buf=00 00 f1 01           <- proceeds to the NEXT real config step
OmapNKS4: WaitForNKS4ReadEvent() timed out                     <- expected: that command isn't stubbed
OmapNKS4: Problem configuring OmapNKS4 in Init
OmapNKS4: about to emergency stop
OmapNKS4: done!
usbcore: deregistering interface driver OmapNKS4
```

Getting there required two more real fixes, both found live and both genuine
correctness bugs (not VM-only issues):

- **`vm_usb_submit_urb()` wrapper** (usb.cpp), replacing all 5 of this module's own
  `stg_usb_submit_urb()` call sites: a real submit against the synthetic device always
  fails (no real backing `usb_device`/bus/HCD, by design). The wrapper reports success
  without completing the URB for the interrupt endpoint (identified by comparing
  against `sInterruptURB`) - deliberately **not** synchronously invoking its completion
  callback, since `InterruptCallback` unconditionally resubmits itself on every
  completion, which would recurse without bound on the same stack. Falls through to the
  real `stg_usb_submit_urb()` unchanged whenever `vm_virtual_probe` is 0.
- **A real, latent NULL-pointer oops**: the first attempt at echoing a reply to
  `CommunicationCheck` (matching the wire bytes `00 00 ee 00` - `ReverseMessage()`'s
  full byte-reversal of `SubmitNKS4CommandWrite(0x00ee0000)`) crashed with `BUG: unable
  to handle kernel NULL pointer dereference` in `__wake_up_common`, called from
  `SendNKS4EventToLinuxReader()`'s own `__wake_up(sReadEventWaitQueue, ...)`.
  `submit.cpp`'s own four wait queues (`sReadEventWaitQueue`/`sAtmelReadWaitQueue`/
  `sVideoMsgWaitQueue`/`sShutdownSsdWaitQueue`) were declared as raw
  `wait_queue_head_t`-sized storage but **never `__init_waitqueue_head()`'d anywhere** -
  a genuinely empty `wait_queue_head_t` must self-point (`next==prev==&head`), not sit
  zeroed, or `__wake_up_common`'s list walk NULL-derefs. This was the first test ever to
  actually exercise that `__wake_up()` call (every earlier test's `CommunicationCheck`
  always timed out before reaching it) - a real device's first genuine interrupt-IN
  reply would hit this identical crash on real hardware. Fixed by
  `__init_waitqueue_head()`-ing all four in `OmapNKS4Probe()`, alongside the three
  usb.cpp already initializes there.
- **A synchronous ordering race**: the very first CommunicationCheck-echo attempt called
  `SendNKS4EventToLinuxReader()` directly from inside `vm_usb_submit_urb()`, but at that
  point `WaitForNKS4ReadEvent()` (the function that sets `sWaitReadPtr`, the pointer
  `SendNKS4EventToLinuxReader()` writes through) hasn't even been called yet -
  `SubmitNKS4CommandWrite()` fully returns first, on the same single synchronous call
  chain. `SendNKS4EventToLinuxReader()` against a NULL `sWaitReadPtr` is a silent no-op,
  so the reply was dropped and `CommunicationCheck` still timed out. Fixed by stashing
  the reply (`sVmPendingReply`/`sVmPendingReplyValid`, usb.cpp) and having
  `WaitForNKS4ReadEvent()` (submit.cpp) deliver it right after setting `sWaitReadPtr`,
  before entering its own poll loop.

## `vm_virtual_probe`, continued: every wire command, board stays running

Extended the same pass to close the gap the section above deliberately left open. Two
more real bugs surfaced doing this - both genuine correctness gaps a real device's
first full configuration exchange would also have hit, not VM-only issues:

- **`vm_usb_submit_urb()` now completes every command/video URB**, not just accepts it.
  `WaitForNKS4CommandWrite` (submit.cpp - every setter in `command.cpp` routes through
  it: `SetNumberOfAnalogInputs`, `SetAllAnalogInputFilter`, `SetNumberOfLEDs`,
  `ConfigureRotaryEncoders`, `SetRotaryEncoderSampleSpeed`, `ConfigureScanning`, ...)
  blocks in a `for(;;)` on `sDoingWait4Write`, which only `WriteCallback`'s own
  in-use-count-reaches-zero check clears. The prior "accept, never complete" design
  (correct for the interrupt-IN URB, which `InterruptCallback` unconditionally
  resubmits on completion - completing it synchronously would recurse without bound)
  was wrong for command/video URBs, whose own completion handler (`WriteCallback`)
  never resubmits anything - completing those synchronously is both safe and required.
  Without this, the very first setter call would have spun in
  `WaitForNKS4CommandWrite`'s own poll loop forever - a real hang, not a graceful
  timeout, and exactly the failure mode this session's `/goal` explicitly ruled out.
- **A real NULL-pointer oops in `__sched_setscheduler`**, the first time either worker
  thread (`ProcessMsgRoutine`/`kOmapNKS4MsgRoutine`, `ShutdownSSDRoutine`/
  `kShutdownSSDRoutine`) ever actually ran in any test this session (every earlier test
  failed/unloaded before `OmapNKS4Init` reached `create_thread()`). Both threads called
  `stg_sched_setscheduler(task, 2 /* SCHED_RR */, 0)` - a NULL third argument, which
  real `sched_setscheduler()` dereferences (`param->sched_priority`) unconditionally.
  Ground truth (fresh Ghidra disassembly, `ProcessMsgRoutine@0x10450` and
  `ShutdownSSDRoutine@0x10570`, both confirmed byte-identical in this respect): the real
  binary constructs a genuine on-stack `struct sched_param{.sched_priority=2}` and
  passes its address - not NULL - at both call sites. Fixed by reproducing that struct
  (a real kernel primitive that project convention has always avoided pulling in via
  `<linux/sched.h>` in these C++ translation units - a bare `{int sched_priority;}` is
  the whole real layout, safe to hand-declare) and constructing it properly at both
  sites.

**Query dispatch, extended to cover every query command `COmapNKS4Driver_Configure`'s
real, confirmed sequence can reach** (`vm_usb_submit_urb`, usb.cpp) - `CommunicationCheck`
(`0x00ee0000`→tag `0x0066`), `ReadPortConfiguration`/`GetRawDipSwitches` (`0x01f10000`→tag
`0x0171`, `hwVer=1`/`is88=0`, matching this project's own confirmed real hardware), and
`GetVersion`'s 4-out no-index form (`0x00f00000`→tag `0x0070`, `OMAP V01 R08, PSoC V00
R05`, matching the real dmesg reference already in `driver.cpp`'s own comments) - plus a
generic reg-byte (`0xf0`) fallback for `GetVersion`'s indexed 1-arg form (used by the
`hwVer==2/3` branches, honestly unconfirmed on real hardware per `driver.cpp`'s own "not
independently verified" comment) so that path can't leave a caller blocked either, without
claiming its specific reply bytes are ground-truthed. Every setter command, whatever its
reg byte, is now safely completed with no reply (matching `WaitForNKS4CommandWrite`'s own
real contract of not expecting one).

**Live VM boot test, 2026-07-17, after both fixes**: `COmapNKS4Driver_Configure()` runs
its ENTIRE real sequence to completion (`CommunicationCheck` → `ReadPortConfiguration` →
`GetVersion` → `SetNumberOfAnalogInputs` → `SetAllAnalogInputFilter` → `SetNumberOfLEDs`
→ `ConfigureRotaryEncoders` ×3 words → `SetRotaryEncoderSampleSpeed` →
`ConfigureScanning`), `OmapNKS4Init()` starts both worker threads
(`kOmapNKS4MsgRoutine`, `kShutdownSSDRoutine`) and the RTAI active-sense thread
(`CActiveSenseThread_Setup()`), and **returns success** - confirmed via `/proc/modules`
showing `OmapNKS4Module ... Live` (not unloaded) and both worker threads alive and
idling (`ps` state `D`, inside their own `stg_msleep(20)` poll loops - healthy, not
stuck) with **zero kernel oopses** across the full boot. Reproduced twice (once
confirming the fix, once confirming the `GetVersion(index)` generalization didn't
regress it).

This is the goal this session's second `/goal` directive set - "every wire command
implemented and the board stays running without crashing" - met with live, reproducible
evidence: the real driver's entire init/probe/configure sequence runs against the
`vm_virtual_probe` virtual board and stays up, not just handshakes once and gracefully
exits. Deep wire-protocol fidelity for commands OUTSIDE this real, confirmed boot
sequence (e.g. `SetLCDBrightness`, `ResetModule`, runtime event traffic once the
front-panel keys/knobs are actually "pressed") remains unimplemented - those aren't
commands the driver's own real startup path sends, so they were out of this pass's
scope, not silently skipped.

## Adversarial re-verification pass, 2026-07-17 (session 3)

Every prior pass above established *coverage* (a function exists under the right
name) but never systematically re-checked *fidelity* (the function's logic actually
matches ground truth) across the whole module. Five independent agents adversarially
re-verified all 255 real functions across all 10 translation units against fresh
decompiles of the real target binary (`OmapNKS4Module.ko`, firmware 3.2.2,
`/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko`), organized one agent
per file or small file group (`driver.cpp` alone given its size/centrality;
`usb.cpp`+`command.cpp`; `rtwrap.cpp`+`softfloat.cpp`; `main.cpp`+`procfs.cpp`;
`submit.cpp`+`realtime.cpp`+`video.cpp`). Real, previously-unreported bugs found and
fixed (each also noted in-file at its correction site):

- **`video.cpp` (highest severity)**: `ContinueProcessingEvent`'s packet buffer was
  0x88 dwords, not the real 0x80 - every data chunk sent 32 bytes of extra
  uninitialized stack contents over USB. The end-of-region marker sent the whole
  buffer instead of the real fixed 4-byte header-only length.
- **`driver.cpp`**: the Atmel NV2AC read-payload byte-swap did a 16-bit-pair swap
  ([b0,b1,b2,b3]->[b1,b0,b3,b2]) instead of a true 32-bit reversal
  ([b0,b1,b2,b3]->[b3,b2,b1,b0]); the payload length byte needed sign-extension
  before widening, not a plain unsigned cast.
- **`command.cpp`**: `GetVersion`'s 4-out variant had the wrong wire protocol
  entirely (two reads decoded as whole bytes, vs. the real one read decoded as four
  nibbles); `ReadPortConfiguration` sent a fabricated command word with no
  ground-truth support (the real command is the same wire command as
  `GetRawDipSwitches`, differing only in response decode).
- **`usb.cpp`**: the emergency-stop path in `CleanupOmapNKS4Driver` was missing two
  real writes (zero the buffer, set length=4) that its own comment already claimed
  happened but the code never did; the command URB pool is 16 entries, not 15, and
  the real allocator deliberately never links the first allocated URB onto the free
  list (reserved for the emergency-stop path) - the previous version could have
  handed that URB out while in-flight.
- **`submit.cpp`**: `pop_free_urb()` had no locking at all, a real unaddressed race
  against the completion-handler (interrupt) context pushing URBs back onto the same
  list - ground truth wraps every pop in `_spin_lock_irqsave`/`_irqrestore`.
- **`procfs.cpp`**: the entire `/proc` write command-keyword table was wrong - none
  of the previous keywords exist in the real binary, replaced with the real table
  recovered from the string pool + disassembled `strstr` chain; `OmapNKS4ProcReadEvent`
  was missing the real `_spin_lock_irqsave`/`_irqrestore` pair around its dequeue.
- **`main.cpp`**: `ShutdownSSDRoutine`'s two `shutdown_fn(...)` calls each passed the
  wrong argument (function-pointer lookup object vs. the actually-passed object were
  conflated); the mutex lock was a private placeholder instead of the real
  `Scsi_Host`-embedded lock field; `COmapNKS4Driver_ShutDown(0)`'s literal argument
  was wrong (real code loads a global); `OmapNKS4Exit` was missing real `__wake_up`
  calls before each thread-join wait.
- **`rtwrap.cpp`**: a real silent ABI/calling-convention mismatch - several RTAI
  primitives (`rt_typed_sem_init`, `rt_sem_wait`, `rt_sem_broadcast`, `rt_sem_delete`)
  are called with stack-passed (cdecl) arguments in ground truth but were declared
  under this file's ambient `-mregparm=3`, silently register-passing garbage;
  `rtheap_alloc`/`rtheap_free` were missing a real first argument (a fixed owner
  token); the RT-task heap allocation under-sized by up to 0x40 bytes (a real heap
  overrun risk); `posix_wrapper_fun`'s trampoline never delivered the stored task
  argument to the user function despite storing it correctly.

Also confirmed correct under specific adversarial re-check (nothing changed): the
SPI/USB dual-bit-test-style claims already documented above, `SendPixelDataRegion`'s
3-argument shape, `proc_dir_entry`'s corrected offsets, the SCSI shutdown sequence's
host-index/state-value claims, `CActiveSenseThread`'s 28-byte layout, and
`CSTGOmapNKS4Fifos`'s 1304-byte layout with its 256/64-deep FIFOs.

Two host-side known-answer test suites (`make verify`) were re-run after every fix
and pass clean (0 failures) - along the way, found and fixed a duplicate stale
`_DAT_0000af38` definition and two C-linkage/signature mismatches in the test
harness's own stub file (`verify/host_stubs.cpp`) that had silently prevented
`test_driver_receive_event_buffer` from ever actually linking successfully, plus a
missing `command.cpp` link dependency in the Makefile itself. `usb.cpp`, `main.cpp`,
`procfs.cpp`, `rtwrap.cpp`, `softfloat.cpp`, `submit.cpp`, `realtime.cpp`, and
`video.cpp` still require a real kernel build (`make ko KDIR=...`) to validate beyond
compile-clean, not re-run this pass.

Genuinely still open, left honestly unresolved rather than guessed at: the exact
semantic intent of `rtwrap_global_save_flags_and_cli`/`_restore_flags`'s
`_nano2count_cpuid` retry-loop counter pair (narrowed considerably this pass - a
missing LOCK prefix, a missing retry loop, and a real variable-mixup bug were all
identified - but the loop's actual purpose is still not pinned down); whether the
newly-declared `regparm(0)` fix needs to extend to the ~20 other `rt_*`/RTAI externs
in `rtwrap.cpp` not individually re-checked this pass; `HandleOutputSysReq`'s exact
0x0900-marker re-fetch loop shape (low severity - end-visible behavior is unchanged);
and `softfloat.cpp`, which couldn't be checked at all this pass - none of its
functions exist as named symbols in the target binary (Korg's real libgcc statically
links them without preserving symbol names), so it remains unverified against ground
truth by any means tried so far.

## Continued RE, 2026-07-17 (session 2): full functional coverage

Following the earlier "Continued RE" pass above, a systematic sweep cross-referenced
every one of the 255 real, non-external functions in the correct 89849-byte binary
(everything below its 0x20000 extern-thunk boundary - genuine kernel/RTAI imports
above that address are all exactly 1-byte Ghidra stubs, a reliable signature) against
this reconstruction's existing 8 translation units. 92 were genuinely unaccounted for.
All 92 are now decompiled and reconstructed; every real function in the target binary
has a corresponding implementation somewhere in this tree.

**The biggest finding: the entire `stg_*`/`rtwrap_*`/`CSTGThread` "STG framework"
layer is NOT external.** Every module doc in this project (including this file, until
now) assumed `stg_*`/`rtwrap_*` symbols were imported from `STGEnabler.ko`/RTAI at
insmod time, "same across OA.ko, loadmod.ko, this one - imported, not reimplemented."
Ghidra's own function-size accounting disproves this: genuine externs (the real
kernel/RTAI imports, `printk`/`rt_sem_wait`/`scsi_*`/etc.) are all exactly 1-byte
stubs at 0x20000+; every `stg_*`/`rtwrap_*` symbol and `CSTGThread` method has a real,
substantial size and address *inside* `OmapNKS4Module.ko` itself (0x17f50-0x18c71).
The real picture: this is a thin, per-module statically-linked pthread-style veneer
over RTAI's own C primitives, most likely compiled from a shared Korg SDK source file
linked into every STG module rather than a symbol resolved at load time - each module
carries its own private copy. New file `rtwrap.cpp` (~460 lines) reconstructs all
~65 of these functions; see its own header comment for the full writeup. **This same
wrong assumption likely exists in other `kronosology/` project docs (e.g. `OA/`'s
own) and is worth rechecking there too.**

Other new/corrected pieces this pass, each rebuilt clean against the real toolchain
(`make ko KDIR=/home/build/linux-kronos`) after every change:

- **`SendPixelDataRegion`'s "third field"** (flagged 2026-07-15 as a possible bug in
  `video.cpp`) - RESOLVED, was a decompiler mistype, not a real discrepancy; traced
  the real caller (`OmapVideoModule.ko`'s `omapfb_ioctl`) to confirm the field really
  is a row-width int, exactly as this reconstruction's `video.cpp` already assumed.
- **`OmapNKS4Probe`'s URB pool sizes** - pinned down at 16 command / 256 video URBs
  (previously undocumented).
- **SCSI SSD-shutdown** (`ShutdownSSDRoutine`) - corrected from an open-ended
  "walk every host" model to the real fixed 4-host-try sequence, and from guessed
  `scsi_device_set_state` values (6, 1) to the real `SDEV_CANCEL`/`SDEV_DEL` (3, 4).
- **`proc_dir_entry` field offsets** - `read_proc`/`write_proc` corrected from
  `+0x3c`/`+0x40` to the real `+0x38`/`+0x3c` (a wrong `mode_t`/`nlink_t` size
  assumption in the original struct-layout math).
- **Thread-join completion wiring** - the suspected race was unfounded (provably
  distinct objects by construction); the real bug was a wrong exit-timeout constant
  (10000 jiffies guessed vs. real 2000).
- **`CActiveSenseThread_Setup`/`_Cleanup`/`_Ping`** - renamed from C++ static methods
  (`CActiveSenseThread::Setup`/`::Cleanup`/`::DoPing`) to plain free functions,
  matching the real exported symbol names (no mangled form exists in the binary).
- **`COmapNKS4Driver::SendNKS4EventToSTG`** and **`COmapNKS4Command::GetRawDipSwitches`**
  - both turned out to be real class methods, not free functions as their bare Ghidra
  names suggested; added using this file's existing `driver_filter()`/`push_event()`
  helpers and the established query-method pattern in `command.cpp` respectively.
- A full family of thin C-ABI wrapper functions (FIFO free functions in `realtime.cpp`,
  a second `COmapNKS4_*` progress-bar/title-screen family in `driver.cpp` that
  `OmapVideoModule.ko`'s `omapfb_ioctl` calls directly, `SetupNKS4Calibration`/
  `CleanupNKS4Calibration` in `submit.cpp`, `OmapNKS4ProcReadEvent`/`ProcDone`/
  `ProcInitialized` in `procfs.cpp` - the first of these three already existed under
  the wrong name `proc_read_event`, renamed to match).

**Result**: `OmapNKS4Module.ko` still links clean (55820 bytes, up from 46340 -
~9.5 KB of new code across `rtwrap.cpp` plus the additions above; vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM`, unchanged). 83 unresolved symbols
(up from 75 - the new `rtwrap.cpp` genuinely needs more real RTAI primitives:
`rt_task_init`, `rt_typed_sem_init`, `rtheap_alloc`/`free`, the `rt_sem_*`/`rt_cond_*`
family, `rt_task_*`, `rt_sched_lock`/`unlock`, `rt_whoami`, the `rt_request_irq`
family - all genuine, expected externs, none accidental).

**Honestly-flagged lower-confidence items from this pass** (not blocking, but worth a
future look): `rtwrap_global_save_flags_and_cli`/`_restore_flags`'s exact
`_nano2count_cpuid` mode-switch counter semantics (the interrupt-flag save/restore
itself is unambiguous; the counter's precise intent isn't independently verified);
`CSTGThread`'s own `Delete`/`Wait`/`GetMaxRealTimePriority` still use the older
simplified `kernel_thread()`-based model in `realtime.cpp` rather than the real
`rtwrap_pthread_*`-based implementation `rtwrap.cpp` now makes possible - deliberately
not swapped in this pass since the per-instance task-handle storage offset within
`CSTGThread` needs one more disassembly pass to pin down safely first.

## `HandleOutputSysReq`'s 0x0900-marker loop, pinned down (2026-07-18)

A prior adversarial re-verification pass had flagged one remaining open item as low
severity: `HandleOutputSysReq`'s exact 0x0900-marker re-fetch loop shape wasn't fully
pinned down, even though the end-visible behavior was believed unchanged either way.
Resolved via fresh disassembly of both compiled clones of the function in the correct
89849-byte target (`/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko`) -
the free-function clone at `0x131d0` and the `__thiscall` clone at `0x13d60` - which
agree instruction for instruction:

- On hitting a 0x0900 marker in slot `n`, the real loop does **not** discard the
  batch immediately. It decrements `avail` (the same bound variable the outer loop
  tests, not a private copy), calls `SetShutdownDelay()`, flags
  `fShutdownRequested`, and then either re-fetches the next FIFO entry into the
  **same slot `n`** and re-tests it against `0x0900` (if `n` is still less than the
  now-shrunk `avail`), or abandons the rest of the batch immediately once the
  shrinking `avail` catches up to `n`. Disassembly: `0x13257`
  (fetch-if-available + `CMP word ptr [...],0x900`) is the single shared entry
  point for both "first visit to slot `n`" and "retry slot `n` after a marker" -
  confirmed by `JNZ 0x13284` (non-marker: `n++`, compare `n:avail`, continue or
  finish) vs. fall-through into the marker path (`LEA EDI,[EDI-1]`; `CALL
  SetShutdownDelay`; `CMP ESI,EDI; JC 0x13257` retries the same `n` iff
  `n < avail`).
- **A second, previously-unreported bug found in the same branch**:
  `SetShutdownDelay()`'s real argument is the marker word's own low 16 bits
  (`buf[n] & 0xffff`), not the constant `0` this reconstruction previously passed -
  confirmed via disassembly identically in both clones (`MOVZX EAX,byte
  [buf+n*4+1]; SHL EAX,8; MOVZX EDX,byte [buf+n*4]; ADD EAX,EDX`, i.e. the same
  32-bit FIFO entry whose high half-word was just tested against `0x900`).
- The final `SubmitNKS4CommandMultipleWriteNONBLOCKING()` call in ground truth
  always passes the (possibly-shrunk) `avail`, not an index - but since
  `fShutdownRequested` is set unconditionally on every marker hit and always
  triggers the `continue` above it, that call is only ever actually reached when no
  marker fired anywhere in the batch (`avail` never shrunk), so this reconstruction's
  previous `n ? n : avail` was already behavior-equivalent to always passing
  `avail`; changed anyway to literally match ground truth's own computation.

Fixed in `driver.cpp`'s `COmapNKS4Driver::HandleOutputSysReq`. Rebuilt clean via
`make ko KDIR=/home/build/linux-kronos` (vermagic `2.6.32.11-korg SMP preempt
mod_unload ATOM`, no new warnings/errors beyond the pre-existing `-fpermissive`/
`-Wcomment` set) after the fix.

## softfloat.cpp verification pass, 2026-07-18

`softfloat.cpp` reconstructs libgcc-style soft-float helpers
(`__mulsf3`/`__addsf3`/`__divsf3`/`__muldf3`/`__floatsisf`/`__fixsfsi`/`__fixdfsi`/
etc.) that this reconstruction's own build needs under the kernel's inherited
`-msoft-float` default, but that the file's own header comment flagged as unverified:
"none of its functions exist as named symbols in the target binary... it remains
unverified against ground truth by any means tried so far." Direct symbol-name
matching is indeed impossible (Korg's own toolchain, whatever it is, doesn't preserve
these names either) - so this pass verified indirectly instead, by finding and
disassembling the real call sites that *should* need these helpers if the premise
("Korg's real toolchain links a genuine soft-float-capable libgcc") were true.

All three of this module's genuine float/double use sites were identified from this
reconstruction's own source (`grep -n float\|double` across every `.cpp`) and traced to
their real-binary counterparts via Ghidra (`OmapNKS4Module.ko`, firmware 3.2.2, 89849
bytes): `ApplyGenericCalibration.clone.0@0x17960` (aftertouch calibration curve),
`CActiveSenseThread::CActiveSenseThread@0x17b50` plus its tick-wait helper@0x17be0
(500 ms active-sense pacing), and `COmapNKS4Driver::SetProgressBarPercent.clone.3@0x13060`
(progress-bar pixel-fill percentage). Full disassembly of all three shows **zero CALL
instructions to anything float-related** - every one does its arithmetic with real x87
hardware instructions directly (`FILD`/`FMUL`/`FDIVR`/`FADD`/`FISTTP`/`FSTP`).
`get_function_info` on `ApplyGenericCalibration.clone.0` and the `CActiveSenseThread`
constructor independently confirms this via their full callee lists (empty, and
`stg_get_cpu_khz` only, respectively).

This isn't just "didn't find a counterexample" - it's exhaustive. Every one of this
binary's 85 real `EXTERNAL`-segment thunks was enumerated by name; none is any libgcc
soft-float symbol. All 255 real functions inside `.text`/`.init.text`/`.exit.text` carry
real names (zero anonymous `FUN_` entries anywhere in the whole binary), so there is no
function, named or anonymous, that any soft-float helper call could even resolve to.
**Conclusion: the real `OmapNKS4Module.ko` never calls a soft-float helper anywhere, for
any of its floating-point arithmetic** - it was built without `-msoft-float` taking
effect for its FP-using code (or with it overridden), the same way this project's
`OA.ko` reconstruction independently found and already fixed for its own engine-startup/
global/scale/smoother/midi-clock-sync/etc. translation units (see `OA/Makefile`'s
`CFLAGS_engine_startup_bits.o` and ~9 sibling overrides, and `OA/src/engine/
audio_input_mixer.cpp`'s `FMul()`/`FAdd()` comment) - a second, independent
confirmation of the same underlying phenomenon, found before this pass and reusable
verbatim as precedent.

**Fix applied**: `Makefile` now overrides `CFLAGS_submit.o`/`CFLAGS_realtime.o`/
`CFLAGS_driver.o` to `-mhard-float` (plain, not OA.ko's `-msse2 -mfpmath=sse` - ground
truth here is confirmed x87, not SSE2, so gcc's default `-mfpmath=387` is the more
faithful match), so these three files emit real FPU instructions instead of soft-float
calls, matching ground truth's confirmed instruction shapes rather than working around
them with a hand-rolled library. `softfloat.cpp`'s functions become dead code after this
fix (no translation unit calls them any more, and none in the real binary ever did) -
left in place as an inert fallback for any future translation unit that does float math
without its own `CFLAGS_*.o` override, with its header comment rewritten to document
this finding and retract the file's earlier (incorrect) claim that hardware FPU use here
would be unfaithful to ground truth.

**Honest confidence assessment**: the three confirmed call sites above are now backed by
direct disassembly (high confidence - not a guess). `softfloat.cpp`'s own arithmetic
correctness was never verified against ground truth by any means tried, and per this
finding, no ground truth of that shape exists anywhere in this binary to verify it
against - not a gap further RE effort on this specific binary can close. Not scanned:
whether any *other* Kronos module besides `OA.ko` also independently hits this same
kernel-Makefile-`-msoft-float`-vs-reconstruction-source issue - worth checking next time
a new C++ kernel-module reconstruction adds float/double arithmetic.


## `vm_virtual_probe`, further continued: SetLCDBrightness/ResetModule + one live runtime event

The "every wire command, board stays running" pass above deliberately left two gaps
open: `SetLCDBrightness`/`ResetModule` (real commands, but never sent by this module's
own `COmapNKS4Driver_Configure()` boot sequence, so nothing drove `vm_usb_submit_urb`'s
dispatch for them), and the whole INBOUND direction - every VM test so far only ever
exercised host->panel command words and the panel's replies to them, never a real
interrupt-IN *event* packet (a key/knob/button being touched) through
`InterruptCallback()`/`COmapNKS4Driver::ReceiveEventBuffer()`. This pass closes both.

**`SetLCDBrightness`/`ResetModule` dispatch** (`vm_usb_submit_urb`, usb.cpp): both
encode as `opcode<<24 | level_or_mode<<16` (command.cpp, re-verified 2026-07-15:
`0xC7000000 | level<<16` and `0x06000000 | mode<<16` respectively - the level/mode byte
rides in the REG position, not a fixed data byte), so unlike the three literal-word
query matches already there, these can only be matched by opcode byte
(`(cmd>>24)&0xff == 0xc7` / `== 0x06`) - the same style already used for `GetVersion`'s
indexed `0xf0` fallback. Both are setters (no reply expected, same as every other
setter), so the only real change is a named printk confirming the command was decoded
correctly, rather than silently falling through the generic "unrecognized word, no
reply" catch-all indistinguishably from an actual unrecognized command. Since neither
command is on this module's own real boot path, nothing would ever call them in a VM
test on its own - `vm_virtual_probe_test_setters()` (usb.cpp, called from the end of
`OmapNKS4Init()`, main.cpp) calls the real, unmodified
`COmapNKS4Command::SetLCDBrightness(0x80)`/`ResetModule(0x00)` directly, exactly as any
real external caller would.

**One live runtime event** (`vm_virtual_probe_inject_event()`, usb.cpp, called right
before `OmapNKS4Init()`'s own `return 0`, i.e. against a fully probed/configured/running
board): writes a 12-byte, 3-record synthetic interrupt-IN packet directly into
`sInterruptURB`'s own transfer buffer (exactly where a real HCD would have DMA'd inbound
data) and calls the real, unmodified `InterruptCallback()` on it, so the whole
decode -> calibration/filter -> queue chain runs through its actual production code
path:

- record 0 `[0x01,0x00,0x50,0x01]` - an ordinary button/key event
  (`ReceiveEventBuffer`'s `(idx & 0xf0) == 0x50` branch) - byte-for-byte the same tuple
  `verify/test_driver_receive_event_buffer.cpp`'s own "Button/idx=0x50 does not wake the
  reader" known-answer case already established as ground truth. With installer support
  turned on first (`COmapNKS4Driver_SetInstallerSupportOn(1)`), this also reaches the
  real `/proc/nks4` read()-side event queue (`OmapNKS4ProcAddEvent`/
  `OmapNKS4ProcReadEvent`, procfs.cpp), not just the general host<-panel `inputFifo`
  every branch feeds.
- record 1 `[0x34,0x12,0x01,0x03]` - an aftertouch event, byte-for-byte the same record
  that test file's own "Aftertouch (non-test-mode) byte packing" case feeds through
  `ApplyNKS4Calibration()` - included so this test also covers the calibration leg of
  the decode chain, not just the installer-support/button leg.
- record 2 `[0x00,0x00,0x00,0x87]` - the Sync terminator (read to end
  `ReceiveEventBuffer`'s loop, never itself dispatched).

**Live VM boot test, 2026-07-17→18** (fresh scratch dir on `kronosvm`, a clean copy of
the prior session's own proven-good disk image + the freshly rebuilt
`OmapNKS4Module.ko` swapped into `/korg/rw/oa_recon` via a loop-mounted partition 6,
same `RTAIVirtualDriver.ko`/`STGEnabler.ko`/`STGGmp.ko` load order as every earlier
`vm_virtual_probe` test): the ENTIRE real boot/probe/configure sequence from the prior
passes reproduces unchanged, immediately followed by:

```
OmapNKS4: vm_virtual_probe: enabling installer support and injecting one synthetic interrupt-IN event packet (button idx=0x50 + aftertouch idx=0x01)
OmapNKS4: DIAG InterruptCallback fired, status=0 len=12
OmapNKS4: DIAG raw interrupt-IN bytes (len=12): 01 00 50 01 34 12 01 03 00 00 00 87
OmapNKS4: vm_virtual_probe: calling SetLCDBrightness(0x80)/ResetModule(0x00) directly to exercise their new vm_usb_submit_urb dispatch coverage
OmapNKS4: DIAG submit_urb_words: len=4 flags=0x100 pipe=0xc0010100 buf=00 00 80 c7
OmapNKS4: vm_virtual_probe: SetLCDBrightness accepted, level=128 (cmd=0xc7800000)
OmapNKS4: DIAG WriteCallback fired, status=0 tag=0
OmapNKS4: DIAG submit_urb_words: len=4 flags=0x100 pipe=0xc0010100 buf=00 00 00 06
OmapNKS4: vm_virtual_probe: ResetModule accepted, mode=0 (cmd=0x06000000)
OmapNKS4: DIAG WriteCallback fired, status=0 tag=0
OmapNKS4: vm_virtual_probe: SetLCDBrightness(0x80) -> ok, ResetModule(0x00) -> ok
```

`cmd=0xc7800000`/`cmd=0x06000000` match `command.cpp`'s own encoding exactly for
`level=0x80`/`mode=0x00`, and both calls returned `true` - confirming the dispatch
byte-matches on the real wire encoding, not just "some word came through". The injected
event's raw bytes echo back byte-for-byte what was written, confirming
`InterruptCallback` read the right buffer/length and handed it to
`COmapNKS4Driver_ReceiveEventBuffer` unmolested. No kernel oops/BUG/panic anywhere in
the boot. Confirmed via the VM's second serial console (an interactive busybox shell,
independent of the captured `-serial file:` boot log) that the module is still fully
`Live` afterwards, not unloaded or wedged:

```
sh-3.2# cat /proc/modules
OmapNKS4Module 61792 0 - Live 0xf81a5000
STGGmp 57665 0 - Live 0xf817e000
STGEnabler 2506 1 OmapNKS4Module, Live 0xf8165000
RTAIVirtualDriver 12286 2 OmapNKS4Module,STGEnabler, Live 0xf8158000
```

Host-side `make verify` re-run clean throughout (`test_command`'s own known-answer
table already covers `SetLCDBrightness(0x7f)==0xc77f0000`/`ResetModule(0x02)==0x06020000`
from the earlier command.cpp re-verification pass - unchanged by this pass, just now
also exercised live). VM scratch directory (`omapnks4_evt_test_20260718`, a fresh copy
made for this pass, never reusing another session's live state in place) and the test
QEMU process removed after use; no changes made to `kronos_local.img` or any other
session's own scratch copy.

Still genuinely out of scope, unchanged from the prior pass's own note: deep
wire-protocol fidelity for runtime traffic beyond this one synthetic packet (multiple
events per interrupt-IN buffer beyond 2, the `0xe1` Atmel-read-payload sub-format, S/PDIF
clock-status events, shutdown-request `0x08` events) - one format-correct button +
aftertouch event confirms the decode/calibration/queue *path* works end-to-end, not
that every event *shape* on it has been independently exercised.



## Adversarial re-verification pass #2, 2026-07-18: every remaining `rt_*`/RTAI extern in `rtwrap.cpp` checked

Closed the exact gap session 3 flagged above ("whether the newly-declared `regparm(0)`
fix needs to extend to the ~20 other `rt_*`/RTAI externs in `rtwrap.cpp` not
individually re-checked"). Every extern in that file's own `extern "C"` RTAI-primitive
block (plus the three cross-checked in `omapnks4_internal.h`: `nano2count`, `rt_sleep`,
`rt_pend_linux_srq`) was checked the same way session 3's four fixes were: disassemble
every real call site in `/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko`
(89849 bytes, confirmed the correct target - not the 79376-byte decoy under
`docs/ASM Docs/`) and read whether the outgoing argument setup right before each `CALL`
is a register `MOV` (regparm-style) or a `MOV [ESP+n],reg` stack store (cdecl).

**Fourteen more genuine silent ABI mismatches found and fixed** (same class of bug as
session 3's four - compiles and links fine, corrupts arguments at runtime): `rt_sem_wait_if`,
`rt_sem_wait_timed`, `rt_sem_wait_barrier`, `rt_sem_signal`, `rt_cond_wait`,
`rt_cond_wait_timed`, `rt_cond_wait_until`, `rt_task_suspend`, `rt_task_resume`,
`rt_task_masked_unblock`, `rt_get_priorities`, `rt_get_time_cpuid`, `start_rt_timer`,
`rt_set_runnable_on_cpuid` - each confirmed stack-passed (cdecl) at every real call site,
now given the same `__attribute__((regparm(0)))` override as session 3's four fixes.
Notably `rt_task_resume` was already *named* in session 3's own header comment as
needing this fix but the declaration itself was never actually changed - a real gap in
the previous pass, not a new regression, now closed. For `rt_sem_wait_timed`,
`rt_cond_wait_timed`, and `rt_cond_wait_until` specifically, ground truth shows even
their *leading pointer arguments* (which would normally be register-eligible on their
own) are stack-passed - more than "the 64-bit delay/until argument forces stack from
that point on" would explain by itself, confirming these needed the full cdecl override
rather than relying on the 64-bit-argument fallout alone.

**Confirmed already correct, no change needed**: `rt_task_init` (full 7-arg regparm(3)
shape: task/fn/data in EAX/EDX/ECX, stacksize/priority/uses_fpu/signal on the stack, an
exact match); `rtheap_alloc` (independently confirmed 3-arg regparm(3) directly from
this binary's own `rtwrap_pthread_create` call site - previously only inferred by
analogy from `OA.ko`, now upgraded to directly verified); `rtheap_free` and `msleep`
(both confirmed register-passed at every call site); `rt_pend_linux_srq`, `rt_free_srq`,
`rt_release_irq`, `rt_assign_irq_to_cpu`, `rt_startup_irq`, `rt_shutdown_irq` (each a
zero-instruction register tail-forward - no argument marshalling instructions at all
between prologue and `CALL`, only possible if the real callee also expects register
args); `nano2count`, `nano2count_cpuid`, `rt_sleep` (each has a 64-bit `long long` as
its sole or leading argument - GCC's regparm attribute never register-assigns a 64-bit
value on i386, so plain ambient `-mregparm=3` already produces the observed all-stack
call for these exact signatures with no override needed); `rt_printk` (variadic, never
eligible for regparm to begin with).

**One left genuinely inconclusive, not guessed at**: `rt_request_irq`. Its wrapper
(`rtwrap_request_irq`) has zero callers anywhere in this binary (dead code, confirmed
via Ghidra), and its own body only sets up ONE outgoing argument (loaded from its own
`[EBP+8]`, an incoming-stack-argument slot that shouldn't exist at all if both of its
own two parameters were register-eligible under ambient regparm(3)) before calling
`rt_request_irq` - the second argument is never set up at all. Internally inconsistent
and, being unreachable, impossible to cross-check against a second real call site the
way every fix above was independently corroborated. Left exactly as declared (ambient
regparm(3)) rather than guessed at, consistent with this project's established
precedent of reproducing confirmed-dead code faithfully (see `rtwrap_count2timespec`'s
infinite loop, same file, same reasoning) instead of inventing a fix with no way to
validate it.

**One bonus value-level bug found and fixed along the way** (not a calling-convention
issue, found incidentally while reading the same disassembly): `rtwrap_pthread_join`'s
Linux-context poll loop called `msleep(1)` in this reconstruction, but its real call
site (`0x18428`) loads `MOV EAX,0xa` before `CALL msleep` - the real value is 10, not 1.

All findings and the disassembly evidence for each are recorded in `rtwrap.cpp`'s own
header comment and inline at the `rtwrap_pthread_join` fix site, plus a short
confirmation note next to `nano2count`/`rt_sleep`/`rt_pend_linux_srq`/`rt_free_srq` in
`omapnks4_internal.h`. Rebuilt via the build server (`192.168.3.92`,
`make ko KDIR=/home/build/linux-kronos`) after the fixes: clean link, vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM`, no new compile errors beyond the
pre-existing `-fpermissive`/`-Wcomment` warnings already known-good from session 3.



## `submit.cpp` wait-helper polling audit, 2026-07-18

Followed up on session 2's own hedge ("several `__wake_up`/`schedule_timeout_wait`/
`sleep_on_timeout` call sites modeled as short-sleep polling ... clearly flagged as
lower-confidence than the disassembly-verified fixes above") by disassembling every
`submit.cpp` wait helper against the real 89849-byte
`/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko` (not the 79376-byte
decoy under `docs/ASM Docs/`). Verdict: **the hedge was too pessimistic - all three
real functions genuinely block**, none of them poll in the real binary. Fixed all
three.

- **`WaitForNKS4ReadEvent`** (`@0x12a40`): real `wait_event_timeout()`-shaped loop -
  `prepare_to_wait(sReadEventWaitQueue, &wait, TASK_UNINTERRUPTIBLE)` /
  check-condition / `schedule_timeout(remaining)` / `finish_wait()`, budget 0x3e8=1000
  jiffies (the literal `MOV EBX,0x3e8` immediate at `+0x35`) - this file's own prior
  1000-jiffy *value* was already right, only the *mechanism* (a `stg_msleep(1)`-loop
  shim previously shadowing the name `schedule_timeout_wait`) was wrong.
- **`WaitOnAtmelRead`** (`@0x12070`): not a loop at all - a single tail-call to the
  real kernel `sleep_on_timeout(sAtmelReadWaitQueue, 0x7d0)`. Two bugs fixed at once:
  the polling shim is gone (now calls the genuine kernel import), and the timeout was
  wrong - 0x7d0=**2000** jiffies, not the previously-guessed 1000.
- **`WaitForNKS4CommandWrite`** (`@0x128f0`): same `prepare_to_wait`/`schedule_timeout`/
  `finish_wait` shape, budget 0xa=10 jiffies, freshly re-armed on each pass of the
  already-correct outer 0x33 (51) retry-and-log loop (~510 jiffies of real blocking
  before the first "waiting for..." log line, not 51×20ms of `stg_msleep` polling).
- **Surprising real finding, confirmed via `get_xrefs_to` on the wait-queue address**:
  `WaitForNKS4ReadEvent` and `WaitForNKS4CommandWrite` block on the exact *same* queue
  (real address `0x1b668` in both, plus `OmapNKS4Probe`'s one `__init_waitqueue_head`
  call and `SendNKS4EventToLinuxReader`'s one `__wake_up` call - 8 xrefs total, no
  others) - i.e. `sReadEventWaitQueue`. There is no separate "command write done" queue
  in the real binary; `sDoingWait4Write`'s completion (cleared by `WriteCallback`,
  `usb.cpp`) is signaled on the same queue as a read event.
- New `struct omap_wait_entry` (`submit.cpp`) mirrors the real 20-byte i386
  `wait_queue_t` (`flags`/`private`/`func`/`task_list{next,prev}`) exactly as both real
  functions construct it on the stack, using the same `%fs:per_cpu__current_task`
  access as `stg_get_current_task_nks4()` for the `private` field. One field is
  best-effort, not disassembly-name-confirmed: the `func` wake-callback constant is an
  unnamed data relocation (`0x20148`, absent from this binary's own `get_imports`
  list) - modeled as `autoremove_wake_function`, the real kernel's own
  `DEFINE_WAIT()` default and the only plausible candidate for this construction
  pattern, flagged inline as the one non-disassembly-confirmed part of this fix.
- `prepare_to_wait`/`finish_wait`/`schedule_timeout`/`sleep_on_timeout` added to
  `omapnks4_internal.h`'s existing "real Linux 2.6.32 kernel primitives" block
  (alongside `__wake_up`/`__init_waitqueue_head`) - all four are genuine imports
  already present in this binary's own `get_imports` list, now declared and linked as
  `U` (undefined, kernel-resolved-at-insmod) symbols, confirmed via `nm` post-build.
- **Explicitly NOT touched, out of this pass's scope**: `WaitForFreeBulkWriteURB`
  (`usb.cpp`) - a stray disassembly check while tracing `0x1b668`'s neighbors
  (`0x1b674`/`0x1b680`) turned up the same real `prepare_to_wait`/`schedule_timeout(10)`/
  `finish_wait` shape there too (ground truth even left itself a breadcrumb: `usb.cpp`'s
  own existing comment already says "`stg_msleep(20); /* (binary uses
  prepare_to_wait/schedule_timeout) */`"), and `main.cpp`'s `ProcessMsgRoutine`/
  `ShutdownSSDRoutine` still poll unchanged too - both real, not yet fixed, and worth a
  dedicated follow-up pass since they live outside `submit.cpp`.
- Rebuilt clean via the build server (`192.168.3.92`,
  `make ko KDIR=/home/build/linux-kronos`): vermagic
  `2.6.32.11-korg SMP preempt mod_unload ATOM`, only the same pre-existing
  `-fpermissive`/`-Wcomment` warnings as every prior session, no new errors; `nm` shows
  all five new externs (`prepare_to_wait`, `finish_wait`, `schedule_timeout`,
  `sleep_on_timeout`, `autoremove_wake_function`) as clean `U` symbols. `make verify`
  re-run clean (0 failures) - unaffected either way, since `submit.cpp` was never part
  of the host verify suite (`verify/host_stubs.cpp` stubs these exact functions out for
  `test_command`/`test_driver_receive_event_buffer`).




## `CSTGThread`'s real per-instance layout, pinned down, and the `rtwrap_pthread_*` swap (2026-07-18)

Closed the exact gap "session 2" above deferred: `CSTGThread`'s own
`CreateRealTimeWithCPUAffinity`/`Delete`/`Wait`/`GetMaxRealTimePriority` used a
simplified `kernel_thread()` + file-scope-static stand-in in `realtime.cpp`, blocked on
one more disassembly pass to find the real per-instance task-handle storage offset
before it was safe to swap in `rtwrap.cpp`'s already-reconstructed `rtwrap_pthread_*`
layer. Disassembled all four methods directly (`CreateRealTimeWithCPUAffinity@0x18b20`,
`Delete@0x18bf0`, `Wait@0x18c20`, `GetMaxRealTimePriority@0x18c50`) in the correct
89849-byte target, plus their sole caller `CActiveSenseThread_Setup@0x17da0`.

**The layout**: `CSTGThread` is not the empty, method-only base this reconstruction
modeled it as (with `omapnks4.h`'s `CActiveSenseThread` separately carrying a "pVTable"
field to cover the gap at its own offset 0). There is no vtable anywhere - proof:
`CActiveSenseThread`'s constructor (`@0x17b50`) writes `bActive=0` at `[this+4]` but
never touches `[this+0]` at all, which a real polymorphic class's constructor would
have to. `CSTGThread` in fact owns two real fields of its own, populated later:

- `+0x00 pTask` - opaque RTAI task handle. `CreateRealTimeWithCPUAffinity` passes
  `EAX=this` directly as `rtwrap_pthread_create`'s `void **out_task` argument
  (`mov eax,esi` / `call <rtwrap_pthread_create>` @0x18b84-0x18b89, `esi==this`) - i.e.
  `rtwrap_pthread_create` does `*(void**)this = new_task`, using the `CSTGThread`
  object itself as its own out-parameter storage. Every later access (post-create
  debug-trap/cpu-affinity setup, `Delete`, `Wait`, the create-failure rollback path)
  re-reads it with a plain `mov eax,[this]`.
- `+0x04 bActive` - byte flag, 1 only after a fully successful create (task spawned
  AND `rtwrap_set_debug_traps_in_rt_task` also succeeded), 0 otherwise. `Delete`
  (`@0x18bfd`) and `Wait` (`@0x18c2d`) both gate their entire body behind
  `cmp byte[this+4],0 / jz <skip>` - calling either on a never-started or
  already-stopped thread is a real, deliberate no-op.

Total size 8 (pTask + bActive + 3 bytes padding), consistent with
`CActiveSenseThread`'s own first real field (`qwNextTickCycles`) already sitting at
`+0x08`. Fixed the stale "pVTable" label in `omapnks4.h` to say what it actually is
(`CSTGThread::pTask`/`::bActive`, flattened in at offset 0 the same way this codebase
already represents every other base-class relationship) - no layout change, just a
wrong comment corrected.

**`CreateRealTimeWithCPUAffinity`'s real parameter order is `(fn, priority, cpumask,
arg)`**, not `(fn, arg, priority, stack, cpumask)` as this reconstruction had guessed -
there is no caller-supplied stack-size parameter at all; the RT task's stack is
hardcoded to `0x5000` bytes internally (`rtwrap_pthread_attr_setstacksize(attr,
0x5000)` @0x18b73, an immediate, not derived from any argument). Confirmed at both the
function's own prologue and its only caller: `this=EAX`, `fn=EDX` (the entry-point
address), `priority=ECX`, then `cpumask`/`arg` on the stack in that order. Also fixed a
real bug in `CActiveSenseThread_Setup` while updating its call site to match: it called
`GetMaxRealTimePriority()` twice via a comma-operator with no ground-truth basis - the
real call site (`@0x17e10-0x17e2a`) calls it exactly once, and the priority actually
passed is `GetMaxRealTimePriority()-10` (`lea ecx,[eax-0xa]` @0x17e15), not the bare max.

**One knock-on return-type fix**: `CreateRealTimeWithCPUAffinity` genuinely branches on
`rtwrap_set_debug_traps_in_rt_task()`'s return value (`test eax,eax; jnz <rollback>`
@0x18bbb) to decide whether to tear a just-created task back down - but that wrapper
(and the raw external it forwards) were declared `void` in both `omapnks4_internal.h`
and `rtwrap.cpp`. Ground truth (`@0x18220`) shows the external's `EAX` result flows
untouched all the way through to the wrapper's own `ret`, so the `void` declarations
were silently discarding a load-bearing value. Corrected both to `int`, propagating the
real return - the sibling `rtwrap_clear_debug_traps_in_rt_task` shows the identical
passthrough shape but is left `void` since nothing anywhere (`Delete`, `Wait`, or the
`Create` rollback path) ever tests its result, consistent with this project's existing
"flag, don't fix, when nothing depends on it" precedent.

No change was needed to `CActiveSenseThread`'s constructor or destructor: the
constructor never touches `pTask` (populated later, by `CreateRealTimeWithCPUAffinity`)
and the destructor's existing `sInstance=0; Delete();` sequence already matches the
real destructor (`@0x17bc0`) instruction-for-instruction.

**Nothing left unresolved**: every method had a live caller to cross-check against
(`CreateRealTimeWithCPUAffinity`/`Delete` via `CActiveSenseThread_Setup`/`_Cleanup`,
`Wait` has no caller anywhere in this binary but its own body was fully disassembled
directly, same confidence level as the others), so none of this pass's findings are
guesses.

Rebuilt clean via the build server (`192.168.3.92`, `make ko KDIR=/home/build/linux-kronos`)
after the swap: same vermagic (`2.6.32.11-korg SMP preempt mod_unload ATOM`), no new
compile errors beyond the pre-existing warnings. `make verify` re-runs clean (0
failures, both `test_command`/`test_driver_receive_event_buffer`). Live VM boot test on
`kronosvm` (fresh scratch dir, `RTAIVirtualDriver.ko` → `STGEnabler.ko` → `STGGmp.ko` →
`OmapNKS4Module.ko vm_virtual_probe=1`, same load order as every earlier
`vm_virtual_probe` session): the entire real boot/probe/`Configure`/`SetLCDBrightness`/
`ResetModule` sequence from the prior sessions reproduces byte-for-byte identical, no
kernel oops/BUG/panic anywhere - `OmapNKS4Init()` (which calls
`CActiveSenseThread_Setup()`, now exercising the real `rtwrap_pthread_create` path for
the first time in a live boot) runs through without incident. VM scratch directory
cleaned up after use; no changes made to the canonical `kronos_vm` image.

## Full-coverage audit, `driver.cpp`, 2026-07-18 (task #7, session 5)

Independent full re-check of all ~30 `driver.cpp`-scope functions against the correct
89849-byte target, part of a final systematic pass over all 255 real functions in the
module. ~26 were already byte-exact (confirmed via instruction-level disassembly, not
just decompile skimming). Two genuine, previously-unfound bugs, both fixed:

- **`ReceiveEventBuffer`'s installer-support button-echo branch (op==1, idx&0xf0==0x50)
  sent a spurious proc event for `v==0`.** Disassembly (@0x13a9a-0x13b92) shows `v==0`
  skips **both** `OmapNKS4ProcAddEvent` call sites entirely - only `(short)v>0` (event
  `0xd`) and `v<0` (event `0xe`) call it. Fixed with an explicit `if`/`else if`, no call
  in the `v==0` case.
- **`COmapNKS4Driver_Configure`'s `ConfigureScanning` call used the wrong struct fields
  for its last 3 boolean args, including in the branch confirmed on real hardware.** A
  fresh, independent decompile of `COmapNKS4Driver_Configure`@0x14570 shows the 3 locals
  feeding this call are assigned separately per `hwVer` branch: `bOmapRevision!=0`
  (offset 0x01) is arg5 in **every** branch (hwVer==2/3/default alike), while arg6/arg7
  are `bPanelLVersion`/`bPanelLRevision` only for hwVer==2, and `bPsocVersion`/
  `bPsocRevision` for hwVer==3 *and* the default branch. The real-hardware-confirmed
  default branch (Kronos 2/D2550) was therefore wrong before this fix too - it used
  `bPsocRevision`/`bPanelLVersion`/`bPanelLRevision`, none of which match. Fixed by
  capturing branch-specific `bScanArgB`/`bScanArgC` locals and using
  `bOmapRevision != 0` unconditionally for arg5.

This audit's own worktree predated several already-merged fixes (the `HandleOutputSysReq`
retry-loop shape, the Atmel-payload 32-bit byte-swap, `SendNKS4EventToSTG`, the
`COmapNKS4_*` progress-bar family) and independently re-derived a couple of them on that
stale base, landing on conclusions already superseded here - those parts of its report
were not applied, only the two genuinely new findings above. Rebuilt clean (`make ko
KDIR=/home/build/linux-kronos`, vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`)
and `make verify` passes 0 failures on both suites after these two fixes.

## Full-coverage audit, `usb.cpp` + `command.cpp`, 2026-07-18 (task #7, session 5)

Independent full re-check against the correct 89849-byte target, part of the same final
systematic sweep. `command.cpp`'s entire "6 bugs" list from this pass turned out to
already be fixed in this tree (its own worktree predated the `ReadPortConfiguration`/
`GetVersion` wire-protocol corrections and `GetRawDipSwitches` addition already recorded
above) - no changes needed there. `usb.cpp` had three genuinely new, previously-unfound
bugs, all fixed with disassembly evidence:

- **`WriteCallback`'s command-URB completion call passed no arguments.** Ground truth
  (@0x10063) passes `EAX=urb->transfer_buffer` (+0x40), `EDX=urb->actual_length>>2`
  (+0x50, byte length converted to word count) to `COmapNKS4Driver_NotifyTransmittedCommandComplete`
  - not a no-arg call. Fixed the call site plus the free-function C-ABI wrapper's
  signature (driver.cpp) and its declaration (omapnks4_internal.h) to match the
  already-correct 2-arg `COmapNKS4Driver::NotifyTransmittedCommandComplete` instance
  method signature that existed alongside the stale no-arg wrapper.
- **`WriteCallback`'s error path was missing a second, unconditional `printk`** that all
  three status branches converge on in ground truth (@0x101ba) before clearing status -
  recovered the exact format string via `read_memory` and added it; also corrected the
  three existing message texts to the real `"%s: line %d:  ERROR..."` prefix format.
- **`free_all_urbs()` never reset `sBulkCommandURBsInUse`/`sBulkVideoURBsInUse`**, unlike
  every one of ground truth's four occurrences of this reset block (relied on BSS
  zero-init only being correct on the very first load, not on a re-probe after a prior
  disconnect/failure).

Also added a missing diagnostic printk in `OmapNKS4Probe` (real "Probe() found:
vendor/product/version" statement was entirely absent from this reconstruction).

This audit's own worktree predated the `vm_virtual_probe` VM-test scaffolding, the
16-command-URB pool-size fix, the `skip_first_on_freelist` emergency-stop reservation,
and the four `submit.cpp` wait-queue `__init_waitqueue_head()` calls - its report
included "fixes" for several of these that were actually regressions back to
already-superseded stale states (reverting the pool to 15 entries, removing the
wait-queue inits that a live VM boot test found necessary to avoid a real
`__wake_up_common` NULL-pointer oops, reverting `vm_usb_submit_urb` calls back to
`stg_usb_submit_urb`). None of those were applied - only the three genuinely new
findings above. Rebuilt clean (`make ko KDIR=/home/build/linux-kronos`, vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM`) and `make verify` passes 0 failures on
both suites after these fixes.

## Full-coverage audit, `main.cpp` + `procfs.cpp`, 2026-07-18 (task #7, session 5)

Independent full re-check against the correct 89849-byte target, part of the same final
systematic sweep. This audit's own worktree predated nearly the entire day's session
(missing the `vm_virtual_probe` scaffolding, `stg_ksize`/`cpu_online_mask` fixes, the
SCSI-shutdown 4-host/SDEV_CANCEL-DEL fix, the `CActiveSenseThread_*` free-function
rename, and `sOmapNKS4DriverShutdownArg`'s real definition), so most of its "bugs" were
either already fixed here or, worse, would have been regressions if applied blind (e.g.
reverting the SCSI shutdown block to an open-ended host loop with wrong
`scsi_device_set_state` enum values, reverting `msleep(1000)` back to a guessed `500`,
reverting `COmapNKS4Driver_ShutDown`'s real backing global to a literal `0`). None of
those were applied. Five genuinely new findings survived scrutiny against current
master and were fixed, all with disassembly evidence:

- **`block_all_signals()` is not a real external call in this binary at all.** Ground
  truth (@0x1048c-0x104b5 and its `ShutdownSSDRoutine` twin @0x105ac-0x105d5) shows the
  classic "block every signal" idiom fully inlined instead
  (`spin_lock_irq(&current->sighand->siglock)`; `blocked.sig[0]/[1] = ~0`;
  `recalc_sigpending()`; `spin_unlock_irq(...)`) - the real 2.6.32 `block_all_signals()`
  is an unrelated 3-arg NFS-lockd helper, so this reconstruction's zero-arg extern call
  was a genuine "Unknown symbol" insmod-failure risk, not just an inefficiency. Fixed
  with a `block_all_signals_nks4()` inline helper using disassembly-confirmed
  task-struct offsets (`sighand`@+0x2a0, `blocked.sig[0]/[1]`@+0x2a4/+0x2a8,
  `sighand_struct.siglock`@+0x504).
- **Two "TEMP diagnostic" additions (a 1000ms sleep + an interrupt-URB field dump) in
  `OmapNKS4Init`, never grounded in disassembly evidence, don't exist in the real
  binary** - confirmed by measuring only 10 bytes / two instructions between the real
  `OmapNKS4ProcInitialize()` and `stg_usb_submit_urb()` call sites, no room for either.
  Removed, along with a defensive `if (OmapNKS4ProcInitialize() != 0) goto cleanup;`
  check ground truth doesn't perform either (no TEST/JZ between the two calls - the
  real driver ignores this return value entirely).
- **`ProcessMsgRoutine`/`ShutdownSSDRoutine` (main.cpp) still polled** with
  `stg_msleep(20)` loops - the same class of gap the `submit.cpp` wait-queue audit
  (above) explicitly flagged as out of its own scope. Ground truth open-codes the
  classic pre-2.6.35 `wait_event_timeout()` expansion for both (real per-thread
  timeouts: 3 jiffies for `ProcessMsgRoutine`, 10000 for `ShutdownSSDRoutine`). Added
  two new wait queues (`sVideoMsgWaitQueue`/`sShutdownSSDWaitQueue`, matching ground
  truth's own `.bss` layout immediately before `sMsgThreadComplete`/`sSsdThreadComplete`)
  and switched both loops to real `prepare_to_wait`/`schedule_timeout`/`finish_wait`
  blocking; `OmapNKS4Exit`'s two `__wake_up()` calls (previously targeting `0`/NULL,
  a no-op against a polling loop) now target these real queues so an exiting module
  kicks both threads immediately instead of waiting for their own poll tick.
- **`OmapNKS4ProcReadEvent` (procfs.cpp) polled at 1ms, not the real 40ms** - confirmed
  via the real `MOV EAX,0x28` immediate before its `stg_msleep()` call.
- **`OmapNKS4ProcDone` had two "enter"/"exit" printks that don't exist in the real
  binary** - removed.

Rebuilt clean (`make ko KDIR=/home/build/linux-kronos`, vermagic `2.6.32.11-korg SMP
preempt mod_unload ATOM`) and `make verify` passes 0 failures on both suites after all
five fixes.


## Fresh full-body wrapper audit, 2026-07-18

Independent, from-scratch pass over every `rtwrap_*` wrapper FUNCTION BODY in
`rtwrap.cpp` (not the extern calling-convention audit - that was already done twice,
see "Adversarial re-verification pass #2, 2026-07-18" above - this pass is about what
each wrapper's own body *does* with its arguments). All ~64 `rtwrap_*` functions plus
`posix_wrapper_fun`, the `get_sizeof_*`/`get_pthread_recursive_attr_constant` family,
and the two `rtwrap_global_*` functions were checked against fresh Ghidra decompiles
and, where the decompiler's regparm-argument elision made that ambiguous, raw
`read_memory` byte-level disassembly of the real 89849-byte target (the MCP server's
own `get_disassembly` tool was unreliable for several address ranges this session -
returned "Ghidra script produced no output" repeatedly for specific spans even on
retry; `read_memory` + manual x86 decode was used as a fallback and cross-checked
against whatever `get_disassembly` output *did* come through, with full agreement
everywhere both were available).

**Item (1), the `_nano2count_cpuid` retry-loop mystery flagged 2026-07-17 - PINNED
DOWN.** `rtwrap_global_save_flags_and_cli`/`rtwrap_global_restore_flags` were fully
re-disassembled byte-for-byte (raw bytes read directly via `read_memory` at
0x18a60-0x18b1b, hand-decoded since `get_disassembly` kept failing on this exact
range). The real mechanism is a two-level lock:
- `rtai_cpu_lock` is a genuine `unsigned long` (dword) bitmask, one bit per CPU,
  manipulated with real `LOCK BTS`/`LOCK BTR` (opcodes `0F AB`/`0F B3`, no 0x66
  prefix - confirmed dword operand size) - a per-CPU **recursion guard**: only the
  outermost save/restore call on a given CPU touches the second level below; nested
  calls on the same CPU no-op straight through it.
- `_nano2count_cpuid` (the same 16-bit variable this file already declared - Ghidra's
  separate `nano2count_cpuid` byte-symbol the 2026-07-17 pass suspected was "a second,
  distinctly-named global" is in fact **the same storage**, just Ghidra's own alias
  for the low byte, per that decompile's own overlap warning) is a
  `[serving_lo][ticket_hi]` pair backing a genuine **cross-CPU ticket spinlock**:
  `LOCK XADD word ptr [_nano2count_cpuid], 0x100` hands out the next ticket (old high
  byte) and returns the pre-increment pair; the caller spins (`PAUSE` + re-read the
  live low/serving byte) until serving==ticket. This is exactly the classic ticket-lock
  idiom, serializing whichever CPUs are simultaneously the outermost holder on their
  own CPU into RTAI's "global cli" critical section one at a time.

Real, confirmed bugs this closes relative to the 2026-07-17 reconstruction:
- **(a) non-atomic lockbyte RMW** - ground truth is a genuine atomic `LOCK BTS`/
  `LOCK BTR`; the previous `*lockbyte |= ...` was a plain, non-atomic read-modify-write.
  Fixed via two new local helpers (`nks4_test_and_set_bit`/`nks4_test_and_clear_bit`)
  that reproduce Linux 2.6.32's own `arch/x86/include/asm/bitops.h`
  `test_and_set_bit`/`test_and_clear_bit` instruction-for-instruction (not pulled in
  from the real kernel header, for the same C++-parseability reason as this file's
  other hand-rolled kernel primitives).
- **(b) the retry loop itself** - was entirely absent; now implemented as the real
  ticket-lock wait (XADD + spin-compare-reread), described above.
- **(c) release-path counter bump target** - ground truth is a plain (non-locked)
  `INC byte ptr` on the ticket word's LOW byte only (safe because the ticket lock's
  own mutual exclusion already guarantees at most one CPU is ever in that branch, plus
  a local `cli`); the previous `_nano2count_cpuid += 1` was a 16-bit add that would
  incorrectly carry into the ticket (high) byte on a 0xff→0x00 wrap of the serving
  byte. Fixed via a byte-pointer-cast increment.
- **(d) "missing unconditional full memory barrier"** - this 2026-07-17 finding was a
  **misdiagnosis**, not a real gap: the decompiler's bare `LOCK();UNLOCK();` at
  `rtwrap_global_restore_flags`'s entry is simply how Ghidra renders the real
  `LOCK BTR [stack-local],0` instruction that tests/clears bit 0 of the `flags`
  parameter's own private stack copy (i.e. it *is* the `flags & 1` test, just done
  atomically for no functional reason since the target is a private stack slot). No
  separate fence exists; nothing needed fixing here, just re-flagging so a future pass
  doesn't re-report it as missing.
- **(e) NEW this pass**: `per_cpu__cpu_number` must be read via the same `%fs:`-relative
  per-CPU access this file's own header comment already predicted (matching
  `stg_get_current_task_nks4_local()`'s established idiom) - ground truth shows real
  `MOV reg,FS:[...]` loads at every call site (`64 8b 15 f8 00 02 00` /
  `64 a1 f8 00 02 00`), but the code read it as a plain global, which is simply wrong
  on this kernel's per-CPU data model. Fixed via a new `nks4_this_cpu_number()` helper.
- **(f) NEW this pass**: both functions gate their second-level work with a local
  `cli` immediately before touching `rtai_cpu_lock`/the ticket word (both in the
  restore release path and its mirrored re-acquire path) - entirely absent from the
  previous reconstruction. Fixed (`asm volatile("cli")` at both sites, matching ground
  truth's instruction order exactly).

What's still open: the exact upstream-RTAI rationale for this precise scheme (ticket
lock + per-CPU recursion bit for "global cli") is inferred from the mechanism alone,
not from any surviving RTAI source comment in this repo - plausible, not confirmed
against real RTAI source (none is vendored here to cross-check against).

**Two further genuine, previously-undetected logic bugs found and fixed in the sweep
of the remaining ~62 wrapper bodies** (both confirmed via raw `read_memory` bytes, not
decompiler pseudo-c alone, since Ghidra's branchless bit-arithmetic and CMOV rendering
are easy to misread at a glance):
- **`rtwrap_pthread_mutex_init`'s sem_type polarity was inverted.** Ground truth
  (`and edx,2; cmp edx,1; sbb ecx,ecx; and ecx,2; sub ecx,1` @0x185f7-0x18604) computes
  `(*attr & 2) == 0 → sem_type = 1`, `(*attr & 2) != 0 → sem_type = -1` - i.e.
  RECURSIVE (settype's case 1 sets bit2=4, clears bit1, so `*attr & 2` reads 0) gets
  +1, ERRORCHECK (case 2, sets bit1=2) gets -1. The code had
  `(*attr & 2) ? 1 : -1`, exactly backwards.
- **`rtwrap_pthread_barrier_destroy`'s `rc == 5` polarity was inverted.** Ground truth
  (`mov eax,0xffffffea; ...; cmp edx,5; mov edx,0; cmovnz eax,edx` @0x1875a-0x18769)
  only overwrites the preloaded -EINVAL with 0 (success) when `rt_sem_delete()`'s
  return value is **NOT** 5 - i.e. `rc == 5` is the FAILURE case. The code had
  `(rc == 5) ? 0 : -EINVAL`, exactly backwards.

Also independently re-confirmed (no change needed, already correctly documented by
earlier passes): `rtwrap_request_irq`'s dead-code/inconsistent-arity status;
`posix_wrapper_fun`'s argument-delivery fix; `rtwrap_set_debug_traps_in_rt_task`'s
EDX=0x1b080 buffer-argument gap (raw bytes `ba 80 b0 01 00` @0x18220 reproduce this
exactly - still left unfixed, since the real buffer's size/layout can't be recovered
and a fabricated replacement would be an unverifiable guess, not a real fix).

Everything else - every pure 1:1 register/stack tail-forward, every `get_*` RT_TASK
accessor, every `get_sizeof_*`/`get_pthread_recursive_attr_constant` constant,
`rtwrap_pthread_attr_init/setrtpriority/setstacksize`, `rtwrap_pthread_create/join/
cancel`, the stack-depth-measurement pair, `rtwrap_pthread_mutexattr_settype`,
`rtwrap_pthread_mutex_destroy/lock/lock_timed/unlock`, `rtwrap_pthread_barrier_init/
wait`, `rtwrap_pthread_cond_init/destroy/signal/broadcast/wait/timedwait`,
`rtwrap_rt_cond_wait_timed`, the sched/task/sleep/IRQ one-liners, and
`rtwrap_nano2count`/`_cpuid`/`rtwrap_rt_get_time_cpuid`/`rtwrap_start_rt_timer` - was
checked and found to already match ground truth exactly; `rtwrap_pthread_create` in
particular got a full byte-by-byte trace (allocation, task-struct field offsets,
`rt_task_init`'s 7-argument regparm(3) shape, the join-semaphore init, `rt_task_resume`,
error-path cleanup) with zero discrepancies found.

Rebuilt clean via the build server (`192.168.3.92`,
`make ko KDIR=/home/build/linux-kronos`) after all of the above: `OmapNKS4Module.ko`
(60628 bytes) links with vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`, exactly
matching the target kernel, no new warnings beyond this file's pre-existing ones.



## Fresh independent adversarial pass: `submit.cpp`/`realtime.cpp`/`video.cpp`, 2026-07-18

A new full-coverage pass over exactly these three files (the same partition "session 3"
covered as one of its five parallel agents), run fresh against the same correct
89849-byte `/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko` target, to
catch anything session 3 missed and anything the same-day wait-queue/`CSTGThread`
swaps disturbed. Checked essentially every real function in the three files (all of
`submit.cpp`'s URB submit/wait/calibration functions, all of `realtime.cpp`'s
`CSTGThread`/`CSTGOmapNKS4Fifos`/`CActiveSenseThread`/`CNKS4EventFilter` methods and
free-function wrappers, and `video.cpp`'s full `COmapNKS4VideoAPI` surface). Most
already matched ground truth exactly and needed no change (both `CActiveSenseThread`'s
constructor/destructor/`ThreadRoutine`/`Sleep`/`Ping`, all four `CSTGThread` methods,
`CNKS4EventFilter::FilterEvent`, the video ring helpers `GetNextEventToProcess`/
`AddFifoEvent`/`GetNextFreeFifoEvent`, the constructor, `UpdateScreenInfo`, and the
already-fixed `ContinueProcessingEvent`/`pop_free_urb` locking from earlier passes all
re-verified correct as-is). Several genuine, previously-unreported bugs found and
fixed, ranging from cosmetic to critical:

- **`video.cpp` `ProcessEvents` (highest severity finding of this pass, likely the
  highest-severity finding in this file's whole history)**: every one of the five
  non-streaming draw opcodes (`0x81`/`0xc0`/`0xc2`/`0xc4`/`0xc5`) previously called
  `SubmitOmapNKS4VideoWrite(0, 0)` - a completely empty submit with no payload. Ground
  truth (disassembled fresh, all five branches individually) builds a real, specific
  packed wire header for each opcode and submits *that* - meaning this driver's
  reconstruction never actually sent a single LCD register-init, x-axis-size,
  pixel-region-start, fill, or palette-update command's real parameters to the panel;
  every such command silently went out as a bare, contentless submit. Reverse-engineered
  and fixed all five packing formats from scratch (see `ProcessEvents`' own header
  comment in `video.cpp` for the exact byte layout of each, including which specific
  byte positions ground truth itself leaves as uninitialized stack garbage - zeroed here
  instead for determinism, since there is no single "correct" value to reproduce for
  content the real code never initializes).
- **`submit.cpp` `ApplyNKS4Calibration`**: the entire function was wrong. It was modeled
  as a generic `table = sCalibrationData + chan*9` lookup gated on `sCalibrationData`
  alone; ground truth is a hardcoded per-channel-number dispatch (channels 5/6/0x1d go
  through three independent, non-uniformly-spaced calibration-table slots at
  `sCalibrationData+0x5c/0x70/0x84`; channels 0x10-0x18 use a direct scale/offset float
  formula with real `.rodata` constants; channel 0x1b indexes a real, previously-unknown
  256-entry foot-pedal response-curve table baked into the binary's own `.rodata`
  `@0x19680`, transcribed verbatim via `read_memory`; every other channel passes
  through unchanged) gated on **both** `sCalibrationData` *and*
  `sCalibrationMsgCallbackFunc` - which, contrary to this file's own long-standing
  comment claiming it was "never actually invoked anywhere", *is* called unconditionally
  every time, with `(chan, raw)`. Also reproduced ground truth's real per-call
  CLTS/FXSAVE/FNINIT/FXRSTOR FPU-enable dance (this function's real callers run from USB
  interrupt-completion context, unlike this module's other two float use sites).
- **`submit.cpp` `ApplyGenericCalibration`**: both range comparisons were off-by-one
  (`<` where ground truth uses `<=` on both the low- and mid-threshold gates) - one
  boundary was value-identical by coincidence, the other (`raw==table[1]`) produced a
  real, different result (0x200 baseline instead of the correct ramp value).
- **`submit.cpp` `pop_free_urb`**: the previous pass's lock fix was still incomplete.
  Ground truth performs a genuine `hlist_del()` (the free-list's `struct urb_node` is a
  real Linux `hlist_node` with a `next`/`pprev` pair, not a plain singly-linked stack) -
  the popped node's poison values (`LIST_POISON1`/`2`) and the *next* node's `pprev`
  fixup were both missing, a latent list-corruption risk once the free list holds more
  than one node. Ground truth also uses **two separate lock objects** (one per free
  list) where this file shared one, and increments the in-use counter **inside** the
  same critical section as the pop, not after releasing it (a real lost-update race
  between concurrent submitters). All fixed together; `SubmitOmapNKS4BulkWrite` also
  no longer routes through the word-rounding `submit_urb_words()` helper, since ground
  truth sets an exact byte-count length and does a precise dword+remainder copy, not a
  round-up-to-4 one (latent for every current caller, all of which already pass 4-aligned
  lengths, but a real divergence for the general case).
- **`realtime.cpp` `CSTGOmapNKS4Fifos::TriggerOutputInterrupt` / `CSTGOmapNKS4OutputFifo::WriteCommand`**:
  both called `rt_pend_linux_srq(0)` directly; ground truth calls `rtwrap_pend_linux_srq(dwEnabled)`
  in both - not just a different wrapper (already misdocumented as intentional in a
  prior pass's comment), but a **different argument**: `dwEnabled` isn't a plain
  boolean here, it doubles as the real SRQ id `rt_request_srq()` returned at setup time,
  so the previous code was raising SRQ 0 (almost certainly wrong) on every real
  invocation instead of this module's actual registered SRQ.
- **`submit.cpp` printk text**: four message-text-only bugs, all confirmed via
  `search_strings`/disassembly against the real `.rodata`: `SubmitNKS4CommandWrite`'s
  and `WaitForNKS4CommandWrite`'s "no free urbs" messages were missing/wrong (ground
  truth's real string embeds `"SubmitNKS4CommandWrite"` and line `836` in *both*
  functions - an apparent Korg-side copy-paste, now reproduced exactly);
  `WaitForNKS4CommandWrite`'s "waiting for..." message carries no `<6>OmapNKS4:` prefix
  in the real string, unlike every other message in this file; `WaitForNKS4ReadEvent`'s
  timeout message was missing its real `%s`/`%d`/`[%llu]` rdtsc-timestamp fields and its
  double trailing newline.

Rebuilt via the build server (`192.168.3.92`, `make ko KDIR=/home/build/linux-kronos`)
after all fixes: clean link, `OmapNKS4Module.ko` 62552 bytes, vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM`, no new warnings/errors beyond the
pre-existing `-fpermissive` `void*`-to-`urb*` and `-Wcomment` set already known-good
from every prior session. `make verify` re-run clean (0 failures, both
`test_command`/`test_driver_receive_event_buffer` - neither touches the three files in
this pass's scope directly, but this confirms nothing else regressed). Not independently
re-verified this pass against a live VM boot (out of scope given the volume of fresh
findings above; the `ProcessEvents` fix in particular is worth a dedicated live-boot
LCD-drawing test in a future session, since it changes what actually goes out over USB
for every draw command).

## `COmapNKS4Driver::Initialize`, a second exported member function, added (2026-07-18)

A follow-up question to the completed `driver.cpp` full-coverage audit turned up one
more real, previously-unreproduced function: `COmapNKS4Driver::Initialize(unsigned int)`
@0x13330, a genuine member method setting the exact same 7 fields, same values, as the
free-function `COmapNKS4Driver_Initialize` already implemented here - a second,
distinctly-named exported symbol carrying identical logic, not a different
initialization path. Zero internal callers (confirmed via `get_xrefs_to`: none inside
`OmapNKS4Module.ko` itself calls it) - exported for some other module to call, same
pattern as `SendNKS4EventToSTG`. Added as a plain member method alongside the existing
free function. (The same follow-up also re-confirmed `SendNKS4EventToSTG` itself is
already correctly implemented here, matching ground truth exactly - an earlier report
claiming it was missing was based on stale/cached information from earlier in that
agent's own long-running session, not an actual gap.) Rebuilt clean (`make ko
KDIR=/home/build/linux-kronos`, vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`)
and `make verify` passes 0 failures on both suites.


## Residual-list zero-blind-spots audit, `driver.cpp`, 2026-07-18 (session 6)

A prior full-coverage audit of `driver.cpp` had left four items only "structurally
trusted" rather than individually re-derived from fresh disassembly: two GCC clones
Ghidra's decompiler had failed on outright, one clone characterized by decompile only
(not disassembly), and the whole family of trivial C-ABI getter/setter one-liners.
This pass resolved all four against the correct 89849-byte
`/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko` target, individually,
per function - and found two genuine bugs along the way, one of them a real
previously-unknown correctness bug rather than a cosmetic one.

**1. `SetProgressBarPercent`'s second clone (`@0x13f50`, `__thiscall`) - CONFIRMED
CORRECT, no bug.** `decompile_function` failed on this address again (same as the
prior pass); `get_disassembly` also failed intermittently but eventually succeeded in
small windows. Traced the full body (`this`/`EBX`=EAX at entry, `pct`=DL) instruction
by instruction: same three-way `x0`/`w`/`y` branch-select reusing one `CL` flag
(`hwVer==1`) computed once for both the initial select and the later
`hwVer==1 && *(ushort*)this==0` guard (deliberate reuse, not dead code - confirmed by
tracing that `CL` is never recomputed between the two tests, so it's still valid at
the second one along both control-flow paths that reach it), the same
`x0=(x0+1)-w`/`FILD`/`FMUL _DAT_0000af38`/`FISTTP` percentage computation, and the
same 4 `OmapNKS4VideoAPI_SendFillData()` calls in the same order with the same
color/width/base/height arguments as the already-transcribed `@0x13060` clone. One
purely cosmetic difference: the "return" path for `hwVer==1 && ushort==0` computes an
unused alternate constant set before re-testing the same (still-zero) condition and
falling through to `RET` - dead computation, zero behavioral difference. Documented
inline at the function.

**2. `HandleOutputSysReq`'s second clone (`@0x13d60`, `__thiscall`) - CONFIRMED
CORRECT, no bug; independently re-verified, not just re-stated.** The prior pass's
"RESOLVED (2026-07-18)" comment already claimed both clones agree, but that
characterization was re-checked here from scratch per this session's own "don't just
assume" instruction, since `get_disassembly` failed outright on the exact address
range containing the marker-handling logic (`0x13d93` onward) even after retries.
Fell back to raw `read_memory` + manual x86 byte decode (confirmed byte-alignment via
a small bracketing `read_memory` call first). Full manual trace confirms
instruction-for-instruction equivalent control flow to `@0x131d0`: same
`buf[n]&0xffff` argument to `SetShutdownDelay()`, same `this+0x21`
(`fShutdownRequested`) store, same `avail--`/retry-same-slot-if-`avail>n` shape, same
final `avail` (not an index) passed to `SubmitNKS4CommandMultipleWriteNONBLOCKING()`.
Only difference is register allocation, forced by `ESI` being pinned to `this` in this
clone: `avail` lives in `EDX` (spilled to `[EBP-0x2c]` and reloaded around the
`SetShutdownDelay()` call, since `EDX` isn't callee-saved) where the free clone kept it
in callee-saved `EDI` throughout - a compiler artifact of the calling convention, not a
behavioral difference. Documented inline at the retry-loop comment.

**3. `COmapNKS4Driver_ReceiveEventBuffer`'s clone (`@0x14a90`) - re-confirmed via fresh
disassembly (previously decompile-only characterization from 2026-07-15), and this
re-check is what surfaced item 4 below.** Traced the full dispatch chain from entry
(numInts==0/op==0x87 early returns, the op==8/op<9/op>=9 split, op==1/idx&0xf0==0x50,
idx==0x71, op==7, op==0x1f) and confirmed it matches the member body's own
already-fixed logic exactly. While cross-checking the op==3 (aftertouch) branch's
calibration call against the member body's own equivalent call site, found:

**A genuine, previously-unknown bug: `ApplyNKS4Calibration`'s "raw" argument was
wrong in BOTH real compiled instances of `ReceiveEventBuffer`.** This reconstruction
called `ApplyNKS4Calibration(idx, (short)(dLo | (dHi << 8)))` - a naive 16-bit
little-endian combine. Fresh disassembly of the exported member function
`COmapNKS4Driver::ReceiveEventBuffer@0x13360` (op==3 case, `@0x13618-0x1363c`:
`SHR CL,6; MOVZX EAX,dLo; SHL EAX,2; OR ECX,EAX; MOVSX EDX,CX; CALL
ApplyNKS4Calibration`, with `EDI` holding `idx` - loaded at `@0x13397` and preserved
unclobbered to the call site) AND its clone `COmapNKS4Driver_ReceiveEventBuffer@0x14a90`
(op==3 case, `@0x14d08-0x14d2d`, same shape with `EDI` holding `dHi` instead) agree
exactly: the real "raw" argument is `raw10 = (dLo<<2) | (dHi>>6)` - the SAME 10-bit ADC
reassembly this file's own test-mode branch already computed correctly (and had
already correctly commented!) for building the `0x61` diagnostic event, but never
connected to the calibration input itself. Two independently-compiled instances of the
same logic agreeing, against a third quantity (`raw10`) this file had already
independently derived elsewhere for an unrelated purpose, is about as strong a
confirmation as disassembly evidence gets. Fixed in `ReceiveEventBuffer`'s op==3
branch (`driver.cpp`); the test-mode branch's own `raw10` computation is now shared
rather than duplicated.

**4. Every trivial C-ABI getter/setter wrapper, individually confirmed against its own
real exported address** (offsets cross-consistent with every other confirmed struct
offset in this file): `GetTestMode`/`SetTestMode`@0x14990/0x149a0 (`this+0x10`),
`GetOmapVersion`@0x149b0 (`+0x00/+0x01`), `GetPSocVersion`@0x149d0 (`+0x02/+0x03`),
`GetJackVersion`@0x149f0 (`+0x08/+0x09`), `GetPanelLVersion`@0x14a10 (`+0x04/+0x05`),
`GetPanelRVersion`@0x14a30 (`+0x06/+0x07`), `Is88Key`@0x14a50 (`+0x0a`),
`GetHardwareVersion`@0x14a60 (`+0x0b`), `StartScanning`@0x15560 (`+0x1d`),
`SetSTGInDownload`@0x15570 (`+0x1e`), `GetSPDIFClockError`@0x15580 (`+0x1c`),
`EnableShutdownByDriver`@0x15590 (`+0x20`, unconditional), `SetNumberOfKeys`@0x155a0
(`+0x24`/`+0x0a`, cross-validating the `Is88Key` offset), `SendAtmelCommand`
(member)@0x13ec0 (tail-calls `SubmitOmapNKS4CmdBulkWrite(0xe0,...)`, `this` genuinely
discarded), the free-function `COmapNKS4Driver_Initialize`@0x14540 (all 7 field
writes/values), and `Cleanup`@0x14980 (bare `RET`) - all CONFIRMED CORRECT, byte-exact.
`IncProgressBar`/`SetProgressBarColor1`/`SetProgressBarColor2`'s C-ABI wrappers
(`@0x15510`/`@0x15480`/`@0x15490`) turned out to INLINE their member's logic directly
rather than emit a `CALL` to it, but the inlined logic is instruction-for-instruction
identical to what the member does, so this file's existing "call the member" C source
is behaviorally indistinguishable from ground truth - no change needed.

Two real discrepancies found and fixed:
- **`COmapNKS4Driver_ApplyAftertouchTable`@0x155b0 is NOT a forward to
  `COmapNKS4Driver::ApplyAftertouchTable`, despite sharing a name suffix** - found
  incidentally while confirming the adjacent `SetNumberOfKeys`, not originally on the
  checklist, but chased down given this session's "zero blind spots" bar. Ground truth
  is a genuinely different, larger body: it first calls
  `ApplyNKS4Calibration(chan=7, v)` (hardcoded channel 7 - the same aftertouch
  calibration channel `ReceiveEventBuffer`'s own idx==7 special case uses), THEN
  applies the `sAfterTouch*ConvertTable` lookup to the CALIBRATED result - not to the
  raw input at all. The member function `COmapNKS4Driver::ApplyAftertouchTable`
  (`@0x14500`, used elsewhere in this file) really is the plain table-only lookup with
  no calibration call - confirmed correct, unaffected. Fixed the free function to
  reproduce the calibrate-then-lookup logic.
- **`COmapNKS4Driver_NotifyTransmittedCommandComplete`@0x15400 is a bare `RET`** - it
  does not call the (also-empty) member function at all, unlike this section's other
  wrappers. Behaviorally indistinguishable either way (the member is a no-op too), but
  corrected to a literal no-op for exactness rather than implying a delegation that
  doesn't exist in the binary.

**Host verify suite**: fixing the `ApplyNKS4Calibration` raw-argument bug changed
`ReceiveEventBuffer`'s real output for the two aftertouch known-answer cases in
`verify/test_driver_receive_event_buffer.cpp` (`feed_one_record(0x34,0x12,0x01,0x03)`:
old formula gave raw=0x1234, new formula gives the correct raw10=0xd0) - the test's
hardcoded "want" values had baked in the old, buggy formula and needed updating to
match; confirmed the new "got" values before the fix matched what the corrected code
now produces exactly, then updated the test's own expected-value derivation to compute
`raw10` the same way `driver.cpp` does rather than hardcoding a stale constant.

Rebuilt via the build server (`192.168.3.92`, `make ko KDIR=/home/build/linux-kronos`)
after all fixes: clean link, vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`,
no new warnings beyond this file's pre-existing `-fpermissive`/`-Wcomment` set (this
worktree's own copy of every non-`driver.cpp` file had gone stale relative to the
canonical tree - missing `rtwrap.cpp` entirely and running ~10 sessions behind on
`main.cpp`/`submit.cpp`/`usb.cpp`/etc - refreshed from canonical before this build,
per this session's own "sync fresh, don't trust the worktree" instruction). `make
verify` passes 0 failures on both suites after the test-file update above.



## Zero-blind-spots re-sweep, `main.cpp` + `procfs.cpp`, 2026-07-18 (task #7, session 6)

A second, independent full-coverage pass over exactly these two files, this time
holding every function to a stricter bar than the "task #7, session 5" audit above:
every printk format string, every constant, every helper "trivial" enough to have been
structurally trusted rather than individually disassembled had to be confirmed against
fresh Ghidra output for the correct 89849-byte target
(`/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko`), not decompile-skimmed.
Went through every function named in this session's brief plus every remaining
`OmapNKS4Proc*`/`stg_*` one-liner. Real, previously-unfound bugs, most severe first:

- **`OmapNKS4ProcInitialize`/`OmapNKS4ProcDone`: all four `/proc` entry names were
  wrong.** Fresh disassembly of `OmapNKS4ProcInitialize@0x122f0` plus `read_memory` of
  the real name-string pool (`0x1a946`-`0x1a993`) shows the real names are `OmapNKS4`,
  `OmapNKS4ProgressBar`, `OmapNKS4HardwareVersion`, `OmapNKS4OmapVersion` - not `nks4`/
  `nks4progress`/`nks4hwversion`/`nks4omapversion` as this file's own top-of-file comment
  had assumed (that comment described the four entries' *roles*, correctly, but its
  literal path names were never disassembly-checked). Cross-confirmed via
  `OmapNKS4ProcDone`'s four `remove_proc_entry` calls, which load the exact same string
  addresses. Real `/proc` paths are `/proc/OmapNKS4`, `/proc/OmapNKS4ProgressBar`,
  `/proc/OmapNKS4HardwareVersion`, `/proc/OmapNKS4OmapVersion`. Also found: entry
  **modes are not a uniform 0644** - ground truth uses `0666` (root + progress) and
  `0444`, read-only (hwversion + omapversion), confirmed via the real `MOV EDX,0x81b6`/
  `MOV EDX,0x8124` immediates before each `create_proc_entry` call. `make_proc_entry()`
  now takes a `mode` parameter instead of hardcoding one.
- **`OmapNKS4ProcDone`'s "enter"/"exit" printks: a same-day earlier fix (session 5,
  "Full-coverage audit, main.cpp + procfs.cpp") that REMOVED these two printks, claiming
  they "don't exist in the real binary," was itself wrong - reverted.** Fresh,
  independent re-disassembly this pass proves they're real:
  `get_function_info` on `OmapNKS4ProcDone@0x12420` lists `printk` as a real callee
  (twice), and full byte-level `read_memory` of the function body confirms both calls
  explicitly - `printk("<6>OmapNKS4:%s: line %d: enter\n", "OmapNKS4ProcDone", 0x1e7)`
  before the four `remove_proc_entry` calls, `printk("<6>OmapNKS4:%s: line %d: exit\n",
  "OmapNKS4ProcDone", 499)` after nulling the four handles. Both format strings were
  already sitting, unexplained, in this binary's own confirmed string set
  (`0x19c34`/`0x19c54`) - the earlier fix apparently mis-attributed them to a different
  function (likely `OmapNKS4Probe`/`Disconnect` in `usb.cpp`, out of that session's
  scope) instead of checking `OmapNKS4ProcDone` itself byte-for-byte. Restored.
- **Every printk in `OmapNKS4Init` (`main.cpp`) was missing the module's own
  `"%s: line %d:"` log-prefix convention** (function name + a real embedded
  source-line-number literal as the first two args) - already established/fixed
  elsewhere in this project (`usb.cpp`'s `WriteCallback`, `submit.cpp`'s
  `SubmitNKS4CommandWrite`/`WaitForNKS4CommandWrite`) but never checked against this
  file before. Confirmed via `search_strings` against the real `.rodata` pool
  (`0x1a29c`-`0x1a460`) and cross-checked instruction-by-instruction; every real line
  number is now reproduced (`enter`=0x571, `could not get srq!`=0x578, `Cannot register
  nks4 usb driver!`=0x581, `probe failed`=0x58f, `error stg_usb_submit_urb for interrupt
  xfer`=0x598 - also fixing that message's *wording*, previously "error submitting
  interrupt xfer" - `Problem configuring OmapNKS4 in Init`=0x5a1). Same missing prefix
  fixed in `procfs.cpp`'s `OmapNKS4ProcWrite`/`OmapNKS4ProcWriteProgress` (`cannot
  allocate memory`/`copy from user`, real lines 0xc5/0xcb and 0x13e/0x144) and
  `OmapNKS4ProcInitialize`'s four proc-entry-failure messages, which ALSO had wrong
  wording beyond the missing prefix (`"cannot create progress bar proc entry"`, not
  `"...progress proc entry"`; `"...hardware version proc entry"`, not
  `"...hwversion..."`; `"...omap version proc entry"`, not `"...omapversion..."`).
- **`OmapNKS4Init` is missing two entire diagnostic printk lines ground truth has**,
  bracketing the `wait_for_completion_timeout(sProbeComplete, ...)` call in a pair of
  `rdtsc()` reads: `"Waited %lu cycles for OmapNKS4Probe(). driver state is %d\n"`
  (line 0x58a) and `"current = %llu, before = %llu\n"` (line 0x58b) - confirmed via a
  fresh decompile showing the real arg count/shape for each (2 args for the two-specifier
  messages elsewhere; this one genuinely consumes 4 args, that one genuinely consumes 6,
  matching each format string's real specifier count exactly - decompiler stack-slot
  noise on OTHER call sites was carefully distinguished from real args here). Added both,
  using the same `"=A"` 32-bit-pair `rdtsc` idiom ground truth's own crude
  low-dword-only cycle delta implies.
- **`create_thread()`'s own two printks incorrectly carry a `"<6>OmapNKS4:"` prefix that
  ground truth's real strings do NOT have** - confirmed via direct `read_memory` of both
  format strings (`0x00019a58`="%s: create_thread() failed. err %ld\n",
  `0x0001a884`="%s thread failed in some way\n", neither with the module's usual
  prefix). This is `create_thread()`'s own message convention, distinct from the rest of
  the file - previously assumed-by-analogy, now disassembly-confirmed wrong and fixed.
- **`cleanup_cpp_support()`'s no-op simplification was based on a false premise.** The
  file's own header comment justified skipping the real `stg_cpp_exit()` call with
  "adding a new unresolved extern isn't worth the risk" - but `stg_cpp_exit` is not an
  extern at all: fresh decompile confirms it's a real LOCAL function inside this exact
  `.ko` (`0x17f30`, entire body `{ return; }`, 1 byte), same category as
  `stg_kmalloc`/`stg_msleep`. Zero risk; now called unconditionally, matching ground
  truth exactly (which calls it after an, also-empty-here, `__DTOR_END__` walk).
- **`init_omap_nks4_usb_driver()` has no counterpart in the real binary at all**
  (documented, not restructured): `OmapNKS4Init`'s real disassembly loads
  `sOmapNKS4UsbDriver`'s address as a bare immediate directly before
  `stg_usb_register_driver`, with zero calls to any setup helper in between (confirmed
  via `get_function_info`'s own callee list). Ground truth's struct is
  compile-time-initialized static data; this reconstruction's runtime-population
  function produces byte-identical content before first use (every field value was
  already `read_memory`-confirmed in an earlier session) so it's behaviorally faithful,
  just structured differently - left as-is with this note rather than risking a
  raw-byte-array-with-relocations rewrite for a purely cosmetic difference.
- **`OmapNKS4ProcRead` does not call `OmapNKS4ProcReadEvent()` in ground truth** -
  confirmed via `get_xrefs_to` on `OmapNKS4ProcReadEvent` (exactly one reference in the
  whole binary, an `EXTERNAL`/ksymtab entry point - zero internal callers) and a fresh
  decompile of `OmapNKS4ProcRead` itself showing its own fully-inlined copy of the
  identical dequeue logic. Same "compiler-cloned duplicate, exported separately, never
  called internally" pattern already established elsewhere in this project (e.g.
  `COmapNKS4Driver_ReceiveEventBuffer` vs `ReceiveEventBuffer`). Documented, not
  restructured into a literal duplicate - the call-based factoring here is behaviorally
  identical and matches this project's own precedent for exactly this situation.
- **`OmapNKS4ProcReadEvent`'s poll loop was a `for(;;)` re-checking `sBlockOnRead` at
  the top of every iteration; ground truth is a `do { sleep; check queue; } while
  (sBlockOnRead);`** - confirmed via `OmapNKS4ProcRead`'s own inlined copy of the same
  logic. Behaviorally near-identical (both stop polling within one 40ms tick of
  `sBlockOnRead` going false) but tightened to the real do-while shape for full
  fidelity.

**Re-confirmed correct, no change needed** (each individually decompiled and/or
disassembled this pass, not assumed): `create_thread()`'s completion-struct
initialization and overall control flow (full byte-level trace of all 4 fields:
done/lock/list.next/list.prev, exactly matching `init_completion_struct`);
`ProcessMsgRoutine`/`ShutdownSSDRoutine` (fully re-verified via a single merged decompile
covering both real functions back-to-back - wait-queue mechanics, the 4-host SCSI
shutdown sequence, `shutdown_fn` call arguments, `msleep(1000)`, all byte-for-byte as
currently coded); `OmapNKS4Exit` (exact call sequence match); `OmapNKS4ProcAddEvent`,
`OmapNKS4ClearProcEventQueue`, `OmapNKS4InitProcEventQueue`, `OmapNKS4ProcInitialized`,
`OmapNKS4ProcRead`'s own off>0/sprintf logic, `OmapNKS4ProcReadProgress`,
`OmapNKS4ProcReadHardwareVersion`, `OmapNKS4ProcReadOmapVersion` (all exact decompile
matches); `OmapNKS4ProcWrite`'s entire 11-keyword dispatch table (`clear`/`enable`/
`disable`/`unblock`/`block`/`async`/`sync`/`allow_shutdown_by_driver`/`61key`/`73key`/
`88key`) - independently re-verified byte-for-byte via every `strstr` call site's real
needle-string address and resulting action, re-confirming session 3's 2026-07-17
rewrite with zero further changes needed; `OmapNKS4ProcWriteProgress`'s `inc`/`set`/
`add` dispatch (needle strings `0x1a8e1`="set"/`0x1a8e5`="add" confirmed directly,
`inc` by elimination/call order); `stg_kmalloc`/`stg_kfree`/`stg_get_cpu_khz`/
`stg_ksize`/`stg_is_linux_context`/`stg_hweight32`/`stg_num_online_cpus` (each
individually decompiled: all thin, exact tail-calls/reads over their real kernel
primitive, matching this file's existing implementations exactly); `init_cpp_support`
(confirmed empty, matching). `stg_msleep` specifically could not be re-decompiled this
pass (intermittent Ghidra MCP server failures on that one address after repeated
retries) but is unchanged and was already disassembly-confirmed in the 2026-07-17
session per this file's own header comment; treated as still-good rather than
re-flagged without new evidence.

Rebuilt clean via the build server (`192.168.3.92`, `make ko
KDIR=/home/build/linux-kronos`) after all fixes above: `OmapNKS4Module.ko` (64384
bytes), vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`, no new errors beyond the
same pre-existing `-fpermissive` `void*`-to-`urb*` and `-Wcomment` warnings as every
prior session. `make verify` re-run clean (0 failures, both `test_command`/
`test_driver_receive_event_buffer`) - neither test suite directly exercises
`main.cpp`/`procfs.cpp`, but this confirms nothing else in the shared header regressed.
Not independently re-verified this pass against a live VM boot (the `/proc` entry-name
fix in particular changes real userspace-visible paths - `KronosScreenRemoteDaemon` or
any other tool that reads `/proc/nks4*` today would need updating to the real
`/proc/OmapNKS4*` names; worth a dedicated live-boot + userspace-tooling check in a
future session).



## Second full-coverage audit, `usb.cpp` + `command.cpp`, 2026-07-18 (zero-blind-spots pass)

A prior same-day "Full-coverage audit, usb.cpp + command.cpp" pass (task #7, session 5,
above) found 3 real usb.cpp bugs and 0 command.cpp bugs, but worked from decompile-level
review rather than exhaustively cross-checking every trivial setter/helper against raw
disassembly. This pass re-audited both files from scratch under a stricter bar - every
function, including every one-line setter and every small usb.cpp helper, individually
confirmed via `get_disassembly`/`read_memory` byte tracing, not decompile skimming alone
(decompile was used to orient, then every non-trivial claim independently disassembly-
verified; several genuine bugs here were only visible in the raw instruction stream after
Ghidra's own decompiler had already elided or misrepresented the relevant behaviour).
Files freshly re-synced from the canonical tree first (this worktree had a stale
snapshot, confirmed by file size - matches the project's own recurring warning about this
tool). Target confirmed correct: `/home/share/3.2.2 update contents/mnt/sbin/
OmapNKS4Module.ko`, 89849 bytes.

**`command.cpp` - all 15 real functions individually disassembly-confirmed**
(`CommunicationCheck`, both `GetVersion` overloads, `ReadPortConfiguration`,
`GetRawDipSwitches`, `SetNumberOfAnalogInputs`, `SetNumberOfLEDs`,
`SetAllAnalogInputFilter`, `SetRotaryEncoderSampleSpeed`, `ConfigureRotaryEncoders`,
`ConfigureScanning`, `SetLCDBrightness`, `ResetModule`, and all three `Is*Response`
classifiers) - byte-exact matches confirmed for every setter's wire-word encoding/bit-
shift/validation-range claim already documented above. One genuine, previously-unfound
bug:

- **`ReadPortConfiguration` was missing a real diagnostic printk on its success path.**
  Ground truth (@0x12dae-0x12de0) unconditionally logs
  `"<6>OmapNKS4:%s: line %d: sw1 %02x, sw2 %02x\n"` (name="ReadPortConfiguration",
  line=0x13d) right after the 0x0171 tag validates, using byte1/byte0 of the response -
  before ever computing `*hwVer`/`*is88`. The "sw1/sw2" wording is a genuine Korg-side
  copy-paste from `GetRawDipSwitches`' own debug message (confirmed `GetRawDipSwitches`
  itself, @0x12e10-0x12e8f, has NO printk at all - the copy-paste is one-directional).
  Reproduced verbatim rather than "corrected" to say hwVer/is88, since it's real,
  disassembly-confirmed ground truth. Also upgraded `GetVersion`'s 4-out nibble-mapping
  comment from "high-confidence, not fully ground-truthed" to fully disassembly-confirmed
  (@0x12c40-0x12ccb, byte-exact).

**`usb.cpp` - every real function checked, eleven more genuine bugs found and fixed**
(building on the 3 already fixed by the earlier same-day pass):

- **`WriteCallback`'s free-list push writes 4 bytes past each list head with no reserved
  slot for it.** Ground truth (@0x10098/0x1014f) performs a genuine, UNCONDITIONAL
  `hlist_add_head()` - no NULL/self-pointer check before `old_head->pprev = &new_node`.
  Since this project's own "empty" sentinel is the head pointer's own address, pushing
  onto an empty list needs a real 4-byte slot immediately after each head to absorb this
  write. Confirmed via `get_xrefs_to` on both head addresses: `sBulkFreeCommandURBList`
  (0x1b010) and the slot 4 bytes after it (0x1b014) both carry a genuine compiled-in
  self-referencing pointer value of 0x1b010 in the real binary (identically for the video
  list: 0x1b018/0x1b01c both self-reference 0x1b018) - i.e. each free-list head is a real
  8-byte urb_node-shaped pair in ground truth, not a bare pointer. Without this, the very
  first push after either pool was fully drained (all 16 command or all 256 video URBs
  simultaneously in flight) would have silently corrupted `sBulkFreeVideoURBList`
  (declared immediately after `sBulkFreeCommandURBList`) - a real, live memory-safety
  risk, not theoretical. Fixed with reserved `sBulkFreeCommandURBListPad`/
  `sBulkFreeVideoURBListPad` fields, explicitly reset (matching ground truth's own real
  reset sites) in `free_all_urbs()` and a newly-added unconditional pre-alloc reset in
  `OmapNKS4Probe` (see below).
- **`WriteCallback`'s `sDoingWait4Write`-guarded wake targets the wrong queue.** Ground
  truth (@0x100e2) uses queue address 0x1b668 there, not the 0x1b674 the
  `sCommandWatermarkWaiter`-guarded wake just above it correctly uses. 0x1b668 is
  submit.cpp's own `sReadEventWaitQueue` (`get_xrefs_to` now shows 9 real xrefs, one more
  than the "8 total, no others" an earlier submit.cpp-scoped pass found - this
  `WriteCallback` site is the one it hadn't looked at). Makes sense:
  `WaitForNKS4CommandWrite` (which `sDoingWait4Write` gates) sleeps on
  `sReadEventWaitQueue`, not a separate command-write queue - waking the wrong queue here
  meant a caller blocked in its final wait was never promptly woken by the last in-flight
  command URB completing, only recovered via its own 10-jiffy poll retry.
- **`InterruptCallback`'s `else` branch (unexpected `urb->status`) printk was missing its
  real "%s: line %d:" wrapper**, matching every other diagnostic in this file. Ground
  truth (@0x1026e-0x10289): `"<6>OmapNKS4:%s: line %d: InterruptCallback() urb->status
  %d\n"`, name="InterruptCallback", line=0x43e.
- **`WaitForFreeBulkWriteURB` was a `stg_msleep(20)` poll; ground truth genuinely blocks.**
  This file's own prior comment already flagged this as a known simplification, and a
  same-day submit.cpp wait-helper audit independently confirmed the real shape via a
  stray disassembly check but explicitly left the fix "to a later pass" since
  `WaitForFreeBulkWriteURB` lives outside that pass's own submit.cpp scope. Full
  disassembly trace (@0x10290-0x10449): the same `prepare_to_wait`/`schedule_timeout(10
  jiffies)`/`finish_wait` pattern already established for `WaitForNKS4CommandWrite`, with
  an outer 0x33=51-iteration retry loop per empty-list wait; giving up after 51 iterations
  doesn't fail (no error return) - it logs a bare, unprefixed `"waiting for Free Bulk
  <Video|Command> URB\n"` (confirmed via `read_memory` @0x19a80/0x19aa4 - no prefix or
  args, matching `WaitForNKS4CommandWrite`'s own already-documented bare-string style)
  and restarts from scratch. Reimplemented with a local `struct nks4_usb_wait_entry`
  mirroring submit.cpp's own 20-byte wait_queue_t shape and the same
  `%fs:per_cpu__current_task`/`autoremove_wake_function` idiom.
- **Both command/video and interrupt URB transfer buffers were kmalloc'd with hardcoded
  sizes (0x40, 0x220) instead of the real negotiated `wMaxPacketSize`.** This is the
  highest-severity finding of this pass. Full gapless instruction traces confirm ground
  truth computes the kmalloc size from the relevant endpoint's own `wMaxPacketSize` field
  in both cases: command/video URBs (@0x1152c-0x11534 and the video-loop twin
  @0x115ce-0x11673) use `outEp+4` (the SAME value already used for `urb->length` a few
  lines below - previously two independent constants tied to one real value in ground
  truth); the interrupt URB (@0x11373-0x11377) uses `intEp+4`. A hardcoded buffer smaller
  than the real negotiated packet size, combined with `urb->length` correctly set to the
  larger real value (already correct before this pass), would be a genuine heap-overflow
  risk the moment the USB core reads/writes up to the claimed length - not just an
  inefficiency. Fixed both allocation sites; `alloc_urb_pool`'s own `wMaxPacketSize` local
  (already computed for the pipe encoding) is now also used for the buffer size, and
  `OmapNKS4Probe` computes `intMaxPacketSize` from `intEp` for its own interrupt-buffer
  allocation.
- **The interrupt-URB allocation-failure path was one combined check where ground truth
  has two independent ones with different behaviour.** A `stg_usb_alloc_urb()` failure is
  SILENT in ground truth (straight to cleanup/-ENOMEM, no printk at all); only the
  kmalloc failure prints, and with different text than this reconstruction previously
  used ("cannot allocate buffer for transfers", not "cannot allocate interrupt
  URB/buffer") plus the usual missing wrapper (name="OmapNKS4Probe", line=0x4d3).
  Restructured into two separate checks to match.
- **`OmapNKS4Probe` was missing an unconditional pre-alloc reset of both free-list heads
  (+ pad slots), both in-use counters, and both pool arrays**, present in ground truth
  (@0x11405-0x114d7) right after the interrupt URB's buffer is confirmed allocated -
  BEFORE ever attempting a single command/video URB allocation, not just on a later
  failure. Previously relied entirely on whatever these globals already happened to be
  (BSS zero-init on a fresh load, or `free_all_urbs()`'s leftover state from a prior
  failed probe/disconnect cycle) - correct by coincidence for the basic empty-list check,
  but NULL is not ground truth's real self-pointing convention, and this newly mattered
  once `WriteCallback`'s free-list push (fixed above) started relying on it.
- **Six of `OmapNKS4Probe`'s own diagnostic printks were missing the real "%s: line %d:"
  wrapper**, and one ("wrong ID set") was also missing the word "and" from its real
  message text. All confirmed exact via `search_strings`/`read_memory` against the real
  `.rodata` (not decompile text alone): "wrong ID set: vendor %04x AND product %04x"
  (line 0x475), "DANGER! 2nd OmapNKS4 detected" (0x47c), "DANGER! found additional
  interrupt in endpoint!" (0x49e), "DANGER! found additional write out endpoint!"
  (0x4a9), "Unsupported endpoint found 0x%02x/0x%04x" (0x4b3), "fatal: found %d bulk
  write and %d interrupt endpoint/s" (0x4b9), "probe success" (0x4f9) - all name=
  "OmapNKS4Probe".
- **`OmapNKS4Disconnect`'s own "disconnect" printk was missing the same wrapper**
  (confirmed @0x10edd-0x10ef4: name="OmapNKS4Disconnect", line=0x50e).
- **`CleanupOmapNKS4Driver`'s emergency-stop block had its buffer-zero/length-set and
  printk in the wrong order, and both printks (plus a second pair bracketing the
  `/proc`-entry removal logic) were missing their real wrapper - with a genuinely
  surprising real name argument.** Ground truth (@0x10e6d-0x10ec1) zeroes the buffer and
  sets length=4 BEFORE the "about to emergency stop" printk, not after; both this printk
  and "done!" carry name="EmergencyStopScan" (confirmed via `read_memory` @0x190c1), not
  "CleanupOmapNKS4Driver" - ground truth's real source almost certainly has a separate
  `EmergencyStopScan()` helper the compiler inlined into its sole call site, with the
  inlined printks' compile-time name string unaffected by the later inlining. Separately,
  the `/proc`-entry-removal block (@0x10da0-0x10e2b) is bracketed by a real "enter"/"exit"
  printk pair using name="OmapNKS4ProcDone" (line 0x1e7/0x1f3) - **this directly
  contradicts this project's own prior "Full-coverage audit, main.cpp + procfs.cpp" pass**
  (same day, above), which found and REMOVED an "enter"/"exit" pair from procfs.cpp's own
  `OmapNKS4ProcDone()` as fake/non-existent. Both findings can be true at once:
  `OmapNKS4ProcDone()`'s real body most likely got compiler-inlined into
  `CleanupOmapNKS4Driver` (its sole real call site) - the printks genuinely exist in the
  shipped binary, just physically inside `CleanupOmapNKS4Driver`'s own disassembly.
  Fixed within this pass's own usb.cpp scope; **flagged here for a future session to
  reconcile against procfs.cpp**, which was out of this pass's scope.

**Independently re-confirmed correct, no change needed** (each individually
disassembly- or decompile-cross-checked, not assumed): `configure_interrupt_urb`'s full
body including the USB_SPEED_HIGH interval branch; `alloc_urb_pool`'s skip-first-on-
freelist logic and exact 16/256 loop bounds (re-confirmed independently via a fresh,
complete instruction trace of both loops, not just re-citing the earlier pass);
`OmapNKS4Probe`'s endpoint-discovery classification loop (bmAttributes&3 + direction-bit
logic); `CleanupOmapNKS4Driver`'s `sSTG2NKS4SrqNumber`-gated cleanup tail
(`stg_usb_deregister`/`rt_free_srq` sequence); `SetNumberOfAnalogInputs`'s exact bit
arithmetic (bonus verification, fell out of the same disassembly window as another
finding).

Rebuilt via the build server (`192.168.3.92`, `make ko KDIR=/home/build/linux-kronos`)
after all fixes: clean link, `OmapNKS4Module.ko` 65672 bytes, vermagic `2.6.32.11-korg
SMP preempt mod_unload ATOM`, no new errors beyond the pre-existing `-fpermissive`/
`-Wcomment` set already known-good from every prior session (this pass's own new
`command.cpp` comment accidentally introduced one more harmless `-Wcomment` nesting
warning, same class as the pre-existing one, not a real issue). `make verify` re-run
clean (0 failures, both `test_command`/`test_driver_receive_event_buffer`) - the new
`ReadPortConfiguration` printk fix is visible firing correctly in the test's own captured
output. Not independently re-verified this pass against a live VM boot (the buffer-size
fix in particular changes real allocation sizes and would benefit from a dedicated
live-boot smoke test in a future session, though `make verify`'s own known-answer
coverage of the affected wire-protocol behaviour is unaffected either way).



## Zero-blind-spots full-coverage audit, `submit.cpp` + `realtime.cpp` + `video.cpp`, 2026-07-18 (session 7)

Independent, from-scratch re-audit of exactly these three files against the correct
89849-byte `/home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko`, explicitly
re-verifying (not re-litigating) every claim the "Fresh independent adversarial pass"
entry above made, including its own blanket "already correct, no change needed"
findings. Every real function in all three files was individually decompiled and/or
disassembled fresh this session - none trusted from a prior report at face value. Full
itemized confirm/fix record (about 70 functions across the three files):

**`submit.cpp`** - all ~24 real functions individually confirmed
(`NumBytesToNumInts`/`ring_full_wait`-class helpers are compiler-inlined into their one
real call site, confirmed via `list_functions` showing no standalone symbol - not a gap).
Three genuine, previously-unreported bugs found and fixed, all in the URB-submit family:

- **`SubmitNKS4CommandMultipleWriteNONBLOCKING`**: both its own printks ("ran out of
  urbs", the `submit_urb_words`-inlined "fails %d") had completely wrong text - missing
  the real `%s: line %d: ERROR:` framing entirely. Real strings confirmed via
  `search_strings` against `.rodata` (@0x19b28 "...ran out of urbs", @0x19b88 "...fails
  %d", args `("SubmitNKS4CommandMultipleWriteNONBLOCKING", 785/810)`).
- **`SubmitOmapNKS4CmdBulkWrite`**/**`SubmitOmapNKS4BulkWrite`**: fresh decompile of both
  real functions (@0x12650/@0x127b0) turned up the identical 3-bug pattern in each - a
  wrong-text "too long" printk, a completely MISSING "no free urbs" printk, and a
  completely MISSING submit-failure printk (both previously just `return -1;` with no
  log line at all). All 6 real strings confirmed via `search_strings` (@0x1a018/0x1a0b4/
  0x1a198 for "no free urbs", @0x1a060/0x1a148 for "message too long", @0x19b88/0x1a100
  shared "fails %d" pair) - `SubmitOmapNKS4CmdBulkWrite`'s own real submit-failure string
  literally embeds "SubmitOmapNKS4VideoWrite()" even though its `%s` argument correctly
  says "SubmitOmapNKS4CmdBulkWrite" (a real Korg-side copy-paste, reproduced verbatim,
  same class as this file's own already-documented `SubmitNKS4CommandWrite`-in-
  `WaitForNKS4CommandWrite` precedent).
- **`pop_free_urb`'s emptiness check ran under the wrong lock discipline**: ground truth
  (all three real call sites, `SubmitNKS4CommandMultipleWriteNONBLOCKING`/`.clone.0`,
  `SubmitOmapNKS4CmdBulkWrite`, `SubmitOmapNKS4BulkWrite`) checks `*list == list`
  UNLOCKED, before ever calling `_spin_lock_irqsave()` - the locked section then pops
  UNCONDITIONALLY with no second check. The previous fix (re-verification pass,
  2026-07-17) had the check happen INSIDE the lock instead. Fixed by restructuring
  `pop_free_urb()` to check-then-lock, matching the real ordering exactly.
- Also removed two "TEMP diagnostic" printks in `submit_urb_words` (a wire-bytes/len/
  flags/pipe dump, an `rc=%d` dump) - confirmed absent from ground truth's own
  decompile (only 2 real printks exist in the whole function), same class of finding as
  main.cpp's own already-removed TEMP diagnostics.
- Everything else independently re-confirmed correct via fresh decompile/disassembly:
  `ReverseMessage` (full 4-byte reversal, algebraically re-verified against the
  decompiler's 16-bit-halves-swapped-then-recombined rendering), `OmapNKS4WriteQueueNotFull`,
  `OmapNKS4GetMaxWritePacketSize`/`SetShutdownDelay` (trivial getter/setter, confirmed
  exact addresses), `SendNKS4EventToLinuxReader`/`SignalAtmelReadComplete`/
  `SignalVideoMessageProcessor`/`SignalShutdownSSD` (`__wake_up` args - queue/mode/
  nr_exclusive/key - confirmed byte-exact via raw disassembly at every site),
  `WaitForNKS4ReadEvent`/`WaitForNKS4CommandWrite`/`WaitOnAtmelRead` (re-confirmed
  correct; `WaitForNKS4ReadEvent`'s apparent `iVar3 != 0` extra guard is a provably-
  always-true dead branch given the loop invariant, not a real behavioral difference
  from this file's simpler `if (sWaitReadPtr==0) return 0`), `SubmitNKS4CommandWrite`,
  `ApplyGenericCalibration`/`SetupNKS4Calibration`/`CleanupNKS4Calibration`/
  `ApplyNKS4Calibration` (including the chan 5/6/0x1d table offsets 0x5c/0x70/0x84, the
  aftertouch scale/offset float bits, the foot-pedal table index, and the FPU-enable
  dance - all re-confirmed via fresh disassembly at their exact cited addresses).

**`realtime.cpp`** - all ~22 real functions individually confirmed. No new functional
bugs found; two comment corrections where a prior pass's own claim didn't survive a
fresh from-scratch trace:

- **`OmapNKS4OutputFifo_WriteCommand`'s "return value differs from the class method"
  claim (re-verification pass, 2026-07-17) does not hold up.** A complete instruction-
  by-instruction trace of the real free function (@0x17830-0x178e0) shows every
  reachable exit path - including the "impractical 32-bit wraparound" edge case that
  comment itself hedged on - returns exactly the same value the already-correct
  delegating implementation already produces (0 if disabled, 0 if full, 1 otherwise).
  The one real (but immaterial) difference: the free function reads `dwEnabled` once
  and reuses the register, the class method re-reads it from memory - functionally
  identical since nothing writes `dwEnabled` mid-call.
- **`OmapNKS4_ActiveSenseThreadEntry` has no corresponding real symbol.** Ground truth's
  `CActiveSenseThread_Setup` passes the address of `ThreadRoutine`'s own regparm3 GCC
  clone (@0x18fa0, confirmed via disassembly of the `CreateRealTimeWithCPUAffinity`
  call site: `mov edx,0x18fa0`) directly as the RT task entry point - no separate
  trampoline exists in the binary (confirmed absent from a full "Thread"-filtered
  symbol listing, 38 entries). This reconstruction's own free function is a behavior-
  preserving indirection (a thiscall method and a plain regparm3 one-arg free function
  share the same calling convention here), not a fidelity bug - comment corrected to
  say so plainly rather than cite a fictitious real address.
- All four `CSTGThread` methods re-confirmed via fresh raw disassembly at their exact
  addresses (`CreateRealTimeWithCPUAffinity`@0x18b20 traced instruction-by-instruction:
  attr sizing/init/setrtpriority/setstacksize, the `this`/`fn`/`priority`/[`cpumask`,
  `arg`] argument marshalling into `rtwrap_pthread_create`, the bActive/debug-trap/
  rollback sequence including a real-but-provably-dead extra `bActive` guard on the
  rollback path; `Delete`@0x18bf0/`Wait`@0x18c20/`GetMaxRealTimePriority`@0x18c50 all
  byte-exact). `CSTGOmapNKS4Fifos::Initialize`/`::TriggerOutputInterrupt`/
  `CSTGOmapNKS4OutputFifo::WriteCommand` and their free-function siblings
  (`CSTGOmapNKS4Fifos_Initialize`, `OmapNKS4Fifos_TriggerOutputInterrupt`,
  `OmapNKS4InputFifo_ReadCommand`) all re-confirmed via fresh disassembly, including the
  already-fixed `rtwrap_pend_linux_srq(dwEnabled)` SRQ argument. `CActiveSenseThread`'s
  constructor (field offsets/math/the `1000000.0f` `.rodata` bit pattern @0x1af4c=
  `0x49742400`, byte-verified via `read_memory`), destructor, `ThreadRoutine`
  (confirmed instruction-for-instruction identical to `wait_until_deadline()` inlined +
  the tick-update/`bPending`/`TriggerOutputInterrupt` tail), `Sleep`, `Ping`,
  `CActiveSenseThread_Setup`/`_Cleanup`/`_Ping` (including the real
  `GetMaxRealTimePriority()-10`/cpumask=0/arg=`t` argument values, re-confirmed via
  disassembly at @0x17e10-0x17e2a independent of the prior pass's own citation of the
  same addresses) all check out exactly. `CNKS4EventFilter::FilterEvent` re-confirmed
  correct via a completely fresh decompile (@0x17eb0), field-offset-cross-checked
  against `omapnks4.h`'s struct layout.

**`video.cpp`** - all 26 real functions/clones individually confirmed (the ~24 the
prior pass's own count referenced, plus the 2 `ContinueProcessingEvent` clones and the
`ProcessEvents`/`OmapNKS4VideoAPIProcessEvents` pair counted separately). This pass's
findings are the most significant of the three files:

- **All five draw-builders (`InitLCDRegs`/`XAxisByteSize`/`SendPixelDataRegion`/
  `SendFillData`/`UpdateColorPal`) were missing a real concurrency-safety check
  (highest-severity finding this pass).** Each real compiled function is ~400+ bytes -
  far larger than this reconstruction's own few-line bodies, which is what prompted a
  closer look. Fresh decompile of all five (plus their five independently-compiled
  `OmapNKS4*` free-function duplicates, confirmed via near-identical sizes) shows every
  one is the fully-inlined combination of `GetNextFreeFifoEvent()`'s own ring-full-wait
  + pointer fetch, the field writes, and a genuine `AddFifoEvent()`-shaped re-fetch-and-
  compare check (`if (e != &pEvents[dwWriteIndex]) return 0xfffffffb;`) immediately
  before the commit tail - not a direct fetch-write-commit as this reconstruction had
  it. `GetNextFreeFifoEvent()`/`AddFifoEvent()` were independently confirmed byte-exact
  first (@0x15cd0/0x15e10), so the fix routes all five builders through them instead of
  duplicating their logic incompletely inline. This closes a real race: two producers on
  different CPUs could previously both fetch the same `&pEvents[dwWriteIndex]` slot,
  both write different event data into it, and both call `commit_event()` - advancing
  `dwWriteIndex` twice while only one caller's data survives, silently corrupting the
  ring. Ground truth's recheck catches exactly this and fails the losing caller with -5.
- **`ContinueProcessingEvent`'s row-wraparound branch used the wrong control-flow
  target.** Fresh decompile (@0x15840) shows the "row exhausted, columns still left"
  case is a direct `goto` into the byte-copy body (skipping both the outer `i < limit`
  retest and the `DAT_ed0c < 1` guard) - not a plain C `continue`, which would instead
  re-test both. Provably identical to the old `continue` in every normal case (a nonzero
  row width, which every real caller supplies), but a genuine divergence at the one
  pathological boundary a zero-width region would hit. Fixed via an explicit `goto`
  reproducing the exact jump target.
- **`COmapNKS4VideoAPI_Initialize` was calling the real constructor a second time.**
  The real function (@0x168b0) is a literal 1-byte `RET` - `get_xrefs_to` on the real
  constructor shows its own only reference is from "Entry Point" (the module's C++
  global-constructor table), confirming `g_video`'s constructor already runs
  automatically at insmod time and this exported symbol's real body does nothing
  further. The previous `new (&g_video) COmapNKS4VideoAPI()` here would have silently
  re-run the constructor's own screen-info printk a second time on real hardware (field
  values themselves are harmless to reassign twice). Not currently called from
  anywhere in this reconstruction either, so a pure fidelity fix with no other effect.
- **`OmapNKS4VideoAPIProcessEvents` only processed one step per call; ground truth
  drains the whole queue in a loop.** Fresh decompile of the real free function
  (@0x171a0, 703 bytes) shows an outer `do { ... } while(true)` with its only exit being
  the same "queue empty and not mid-stream" condition `ProcessEvents()` itself already
  implements - i.e. ground truth calls the equivalent of `ProcessEvents()` repeatedly
  until it would return 0, not once. Fixed to `while (g_video.ProcessEvents());`, an
  exact translation using the already-verified class method.
- **`OmapNKS4VideoAPI_SendFillData` has no corresponding real symbol** (same class of
  finding as realtime.cpp's `OmapNKS4_ActiveSenseThreadEntry`) - `get_xrefs_to` on the
  real `SendFillData` class method shows its only 8 non-entry-point callers (all inside
  driver.cpp's `SetProgressBarPercent`) call it directly, not through any bridge
  function. Comment corrected; left in place since driver.cpp is a different file and
  the bridge's own body is already behaviorally exact.
- Everything else re-confirmed correct via fresh decompile: the constructor (field
  offsets/values/printk), `ProcessEvents`'s full control flow (the `dwProcessingActive`/
  `sEventsToProcess` gates, the event-copy, the opcode dispatch structurally matching
  source's if/else-if chain including the implicit "unrecognized opcode still returns
  1" fallthrough) and, spot-checked byte-for-byte via raw disassembly, two of its five
  wire-packing formats (`0xc0`/`0x81` - the `data`/`reg`/`val` byte positions and the
  `pack_field19`-shaped 19-bit packing both matched exactly, corroborating the prior
  pass's already-detailed byte layouts for the other three opcodes without needing to
  re-derive them from scratch), `GetNextFreeFifoEvent`, `AddFifoEvent`,
  `GetNextEventToProcess`, `GetProgressBarPercent` (proxies to
  `COmapNKS4Driver_GetProgressBarPercent`, confirmed both read the same real
  `.rodata`-adjacent global), `UpdateScreenInfo` (field values confirmed past a
  decompiler `this`-mistyping artifact identical in class to the already-documented
  `SendPixelDataRegion` case), `COmapNKS4_SetMaxBulkOutMsgSize`, and all five
  `OmapNKS4*` free-function producer duplicates (confirmed via decompile to carry the
  identical field mappings and the same real race-check this pass added to their class
  method counterparts).

**Process note**: this session's worktree had only synced `submit.cpp`/`realtime.cpp`/
`video.cpp`/the two headers/this file at start, per its own instructions - `driver.cpp`/
`usb.cpp`/`main.cpp`/`command.cpp`/`procfs.cpp`/`rtwrap.cpp`/`softfloat.cpp`/`Makefile`/
`verify/` were stale leftovers from a much older snapshot and caused three genuine (but
already-fixed-in-canonical) compile errors on the first rebuild attempt
(`COmapNKS4Driver_NotifyTransmittedCommandComplete`'s arity, `CActiveSenseThread::Setup`/
`::Cleanup`'s stale static-method names). Re-synced all of those files fresh from the
canonical tree (confirming via diff that the shared headers' only change was an
unrelated `procfs.cpp`-scoped `make_proc_entry` signature addition) rather than
hand-patching against a stale baseline, then rebuilt clean.

Rebuilt via the build server (`192.168.3.92`, `make clean && make ko
KDIR=/home/build/linux-kronos`) after all fixes above: clean link, vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM` (matching the real target kernel exactly),
no new warnings/errors beyond the pre-existing `-fpermissive`/`-Wcomment` set. `make
verify` re-run clean, 0 failures on both `test_command`/`test_driver_receive_event_buffer`
(neither directly exercises these three files, but confirms nothing else regressed).
Not independently re-verified this pass against a live VM boot given the volume of
findings, particularly the video-ring race-check fix and the `ProcessEvents` drain-loop
fix - both worth a dedicated live-boot LCD-drawing/concurrent-producer test in a future
session.

## Independent Opus review checkpoint, 2026-07-18

Per the user's explicit "have Opus do a review" milestone requirement, an independent
review agent (Opus model) re-derived ground truth from scratch for a sample spanning six
of the eight files `make verify` doesn't cover, cross-referenced all 255 real functions
against the source, and re-ran the build/verify suite. Overall verdict: coverage is
complete (all 255 real functions have a corresponding implementation, confirmed against
a fresh function-list pull), and every function it independently re-derived - including
subtle ones like `WriteCallback`'s three distinct `__wake_up` queue targets, the
`rtwrap_pthread_mutex_init`/`rtwrap_pthread_barrier_destroy` polarity fixes, and
`ShutdownSSDRoutine`'s 4-host SCSI sequence - matched ground truth exactly, no logic
discrepancies found.

**One genuine new finding, fixed**: four unconditional "DIAG"/"TEMP diagnostic" printks
(dating to a 2026-07-16 debug session) had survived every prior "remove non-ground-truth
debug scaffolding" sweep this session already did elsewhere (main.cpp's 1000ms sleep,
submit.cpp's wire-byte dump) - `WriteCallback`'s unconditional "fired" printk (usb.cpp,
fires on every command/video URB completion), `InterruptCallback`'s unconditional "fired"
printk AND a per-byte raw-bytes dump loop (up to 64 individual `printk()` calls per real
interrupt-IN packet, from interrupt/completion context - the most severe of the four,
since it would perturb real-hardware timing on this module's most latency-sensitive
callback), and `OmapNKS4Init`'s "interrupt submit rc=0" printk (main.cpp). Ground truth
disassembly of `WriteCallback@0x10040`/`InterruptCallback@0x10220`/`OmapNKS4Init@0x18d06`
confirms none of these calls exist - all four removed. Rebuilt clean (`make ko
KDIR=/home/build/linux-kronos`, vermagic `2.6.32.11-korg SMP preempt mod_unload ATOM`,
65804 bytes) and `make verify` passes 0 failures on both suites after the fix.

**Risk assessment the review flagged, not yet acted on**: the verification methodology
itself (disassembly re-derivation + clean build + a 2-file host test suite + one
happy-path VM boot) leaves real residual risk in areas never behaviorally exercised -
most notably `video.cpp`'s `ProcessEvents` draw-packet formats (the single highest-
severity historical fix this session) and the concurrency fixes (`pop_free_urb`'s
hlist_del + dual locks, the video-ring race recheck, `rtwrap_global_save_flags_and_cli`'s
hand-rolled ticket-lock) - all verified only by re-reading the binary, never by observed
behavior under real load/concurrency. The review's recommendation: prioritize a live
VM/hardware LCD-drawing + concurrent-producer test before considering this module fully
validated, not just disassembly-complete. Consistent with this file's own earlier note
(directly above) flagging the same two fixes as needing a dedicated live-boot test.


## Live concurrency stress test, 2026-07-18: the video-ring race-check and `pop_free_urb`, exercised under real SMP load

Direct response to the Opus review checkpoint above - a live, `kronosvm` (192.168.3.87,
4 vCPUs) boot test that drives genuinely concurrent producers into the video draw-builder
ring, rather than re-reading the disassembly again. VM-only, gated behind a **new, second**
module parameter (`vm_video_stress`, `main.cpp`) on top of the existing `vm_virtual_probe`
- both must be `1` for anything below to run; either at `0` is a complete no-op, so every
prior `vm_virtual_probe=1` boot test this README documents is unaffected.

**What was built** (`video.cpp`, tail of file; a few one-line gated hooks in
`ProcessEvents()`; `main.cpp` gets the new module param + one call site):

- `vm_virtual_probe_stress_test_video()` - the orchestrator, called once from
  `OmapNKS4Init()` right after `vm_virtual_probe_test_setters()`, i.e. once the board is
  fully probed/configured/running and both real worker threads
  (`kOmapNKS4MsgRoutine`/`kShutdownSSDRoutine`) plus the active-sense RT thread are
  already alive - deliberately the same point the existing `vm_virtual_probe_inject_event`/
  `test_setters` VM-only hooks already use.
- **4 producer threads** (`vm_stress_producer_thread`, matching `kronosvm`'s 4 vCPUs),
  spawned via the real `kernel_thread()` primitive `main.cpp`'s own `create_thread()`
  already wraps (called directly here since these are throwaway test threads with no
  `OmapNKS4Init`-style startup handshake to honor). Each runs **2000 iterations**,
  round-robining through all 5 draw-builder free functions
  (`OmapNKS4InitLCDRegs`/`XAxisByteSize`/`SendPixelDataRegion`/`SendFillData`/
  `UpdateColorPal`) - 8000 calls total, no synchronization between producers at all,
  genuinely racing on `dwWriteIndex`/`sEventsToProcess`/`GetNextFreeFifoEvent`/
  `AddFifoEvent` across up to 4 real CPUs.
- **No extra consumer thread** - deliberately. Ground truth's own
  `OmapNKS4VideoAPIProcessEvents`/`ProcessEvents` design is single-consumer
  (`dwReadIndex`/`dwProcessingActive`/`currentEvent` are unprotected globals with no
  dequeue-side race-check, unlike the producer side) - running a second drainer would
  test a configuration ground truth was never built to survive, not a real fidelity gap.
  Instead the test reuses the REAL, already-running `kOmapNKS4MsgRoutine` thread as the
  one real consumer - it wakes via the real `SignalVideoMessageProcessor()` call every
  successful `commit_event()` already makes, so producers and that one real consumer
  genuinely race for the whole run, and every drained event flows through the real
  `SubmitOmapNKS4VideoWrite -> SubmitOmapNKS4BulkWrite -> pop_free_urb -> vm_usb_submit_urb
  -> WriteCallback` path repeatedly and sequentially under sustained real load.
- **Traceable, tamper-evident arguments**: each producer encodes `(thread id, iteration)`
  into its own call's arguments, chosen per-opcode to survive that opcode's specific wire
  truncation intact (a 19-bit `tid<<14 | iter` tag for the three `pack_field19`-encoded
  opcodes; a full 32-bit round-trip for `InitLCDRegs`'s `data` argument; independent
  redundant tid copies in `SendFillData`'s `color` byte, `InitLCDRegs`'s `reg` byte, and a
  fixed `0xAA` marker byte in `UpdateColorPal` - so a decoded packet's redundant fields
  must independently *agree* with its packed tag for a genuine cross-checked "OK", not
  merely decode to a plausible-looking value).
- **VM-gated packet capture**: a one-line hook (`vm_stress_capture_packet()`, gated on a
  new `sVmVideoStressCapture` flag, only ever `1` during the test's own run window) added
  at each of `ProcessEvents()`'s 5 wire-submit call sites, snapshotting the exact final
  wire buffer for up to 512 drained packets. A separate decoder
  (`vm_stress_decode_and_report()`) unpacks a 25-packet sample per the documented formats
  and prints per-packet OK/MISMATCH.
- **Counters**: per-producer and total attempts / successes / `-3` (ring full) / `-5`
  (lost the `AddFifoEvent` race) / other, plus a ring-drained confirmation
  (`sEventsToProcess==0 && dwProcessingActive==0`) polled with a bounded timeout after
  producers finish.
- Every diagnostic added is behind `sVmVirtualProbe && sVmVideoStress` (or the
  capture-specific sub-flag) - per this session's own explicit brief, given the Opus
  review's earlier finding that ungated debug printks had survived multiple cleanup
  sweeps in this exact file/module, this was written and re-checked to make sure nothing
  new joins that list.

**Two real, pre-existing bugs found and fixed just to get a boot far enough to test**
(neither is part of the concurrency fixes under test - both are separate, genuine
correctness gaps this pass happened to be the first to actually exercise live):

- **`usb.cpp`'s `nks4_get_current_task_usb()`** (added by the same-day "full-coverage
  sweep" `WaitForFreeBulkWriteURB` rewrite, immediately above in this file) used a
  **hardcoded** per-cpu offset (`mov %fs:0x20168,%eax`) lifted directly from the one
  shipped binary's own disassembly. First live boot attempt oopsed immediately
  (`BUG: unable to handle kernel paging request`, `CR2=0x0266c168`) at the very first real
  call site, `WaitForFreeBulkWriteURB` inside `COmapNKS4Driver_Configure`'s
  `CommunicationCheck` - `CR2` decoded exactly to `FS.base (0x0264c000 on that boot) +
  0x20168`, landing outside this build's real per-cpu area (the hardcoded literal is
  build/config-specific, not portable - exactly the pitfall `submit.cpp`'s own
  current-task accessor and `main.cpp`'s `stg_get_current_task_nks4()` already avoid, by
  referencing the real symbol name `per_cpu__current_task` and letting the
  assembler/linker resolve it for whatever kernel this actually links against). Fixed by
  making `nks4_get_current_task_usb()` use that same already-established, portable form -
  this one function had simply been missed when the pattern was set elsewhere in this
  same codebase.
- **A framebuffer bug in this test's OWN harness, not the code under test**: the first
  attempt at `SendPixelDataRegion`'s test arguments used the same 19-bit tag for
  width/offset/rowBytes verbatim. `g_video.dwScreenBase` is `0` until something calls
  `OmapNKS4UpdateScreenInfo()` (normally `OmapVideoModule.ko`, not loaded in this VM
  test), so `ContinueProcessingEvent`'s real byte-copy loop correctly (per ground truth)
  dereferenced `dwScreenBase+offset` - i.e. a small literal address - and oopsed
  (`CR2=0x2`, then `CR2=0xc02f` after a first attempted fix still left the offset
  encoding proportional to the tag). Also surfaced a real, separate, ground-truth-genuine
  characteristic along the way: the synthetic `vm_virtual_probe` board's negotiated max
  packet size is smaller than `ContinueProcessingEvent`'s fixed 0x200-byte chunk buffer,
  so a wide pixel region needs many `ContinueProcessingEvent` calls and hits
  `SubmitOmapNKS4BulkWrite`'s real "message too long" rejection on every chunk (harmless,
  just noisy - ~1600 rejected chunk submits in a full run) - not a bug, just heavier
  exercise of ground truth's real row-wrap-pointer-advance arithmetic than this
  concurrency test actually needs. Fixed by giving the test its own 256 KiB backing
  buffer (`new unsigned char[...]`, freed after the run) via a real
  `OmapNKS4UpdateScreenInfo()` call, and by constraining the test's own
  `SendPixelDataRegion` calls to a trivial 1-byte region (`width=rowBytes=1`, tag riding
  in the `offset` field instead) - still exercises the real wire header + a real streaming
  byte-copy + end-marker submit + `dwProcessingActive` transition, without depending on
  multi-call row-wrap correctness this test isn't targeting.

**One unexplained, NOT reproduced hang** - reported honestly rather than glossed over.
Between fixing the two bugs above, one boot attempt (reusing a disk image that had just
taken two prior oopses, not a fresh copy) made it through probe/configure successfully
but then produced no further console output at all for 15+ minutes with no oops/panic -
`info registers`/`info cpus` via the QEMU monitor showed 2 of 4 vCPUs genuinely executing
(not spinning at a fixed EIP - resampling minutes apart showed the EIP had moved), so this
did not look like a classic tight-spinlock livelock, but no further diagnosis was possible
without kernel symbols (no `vmlinux`/`System.map` available in this environment) and
`sysrq` did not appear to be wired to this kernel's console. Killed and retried from a
**fresh** `cp --sparse=always` copy of the canonical image (this project's own established
"never reuse another session's/attempt's disk state" convention) - two full,
back-to-back clean runs followed with no recurrence. Best-effort hypothesis, not
confirmed: post-oops ext3 journal/dirty-state on the reused disk image, not a genuine
kernel-level deadlock - but this is a hypothesis, not a proven root cause, and is flagged
here for anyone who hits it again with better tooling to investigate further.

**Live test transcript, full run** (`vm_virtual_probe=1 vm_video_stress=1`, after the two
fixes above, fresh disk copy):

```
OmapNKS4: vm_video_stress: starting - 4 producer threads x 2000 iterations each, draining
  via the real, already-running kOmapNKS4MsgRoutine consumer thread
COmapNKS4VideoAPI::UpdateScreenInfo() base = 0xf6580000, X= 800, Y=600
  ... (real 0xc0/0x81/0xc2/0xc4/0xc5 submits interleaved with vm_virtual_probe's synthetic
       query replies and ~1600 expected "message too long" rejections for the 0x200-byte
       pixel-region data chunks - see above for why these are real/expected, not new bugs)
OmapNKS4: vm_video_stress: producer 0: attempts=2000 success=1985 ring_full(-3)=15 race_lost(-5)=0 other_err=0 done=1
OmapNKS4: vm_video_stress: producer 1: attempts=2000 success=1972 ring_full(-3)=27 race_lost(-5)=1 other_err=0 done=1
OmapNKS4: vm_video_stress: producer 2: attempts=2000 success=1984 ring_full(-3)=15 race_lost(-5)=1 other_err=0 done=1
OmapNKS4: vm_video_stress: producer 3: attempts=2000 success=1975 ring_full(-3)=24 race_lost(-5)=1 other_err=0 done=1
OmapNKS4: vm_video_stress: TOTAL attempts=8000 success=7916 ring_full(-3)=81 race_lost(-5)=3 other_err=0 - ring drained=yes (sEventsToProcess=0 dwProcessingActive=0)
OmapNKS4: vm_video_stress: captured 512 packets (of 7916 submitted, cap 512) - decoding a sample:
OmapNKS4: vm_video_stress: [0] 0xc0 InitLCDRegs reg=1 data=0x00000000 -> tid=0 iter=0 MISMATCH
OmapNKS4: vm_video_stress: [1] 0x81 XAxisByteSize tag=0x0c001 -> tid=3 iter=1 OK
  ... (23 more, all OK)
OmapNKS4: vm_video_stress: sample decode complete - 1/25 shown mismatched (0 malformed opcode)
OmapNKS4: vm_video_stress: COMPLETE - no kernel oops/hang observed by this point (module still running).
```

A **second, back-to-back run** (fresh disk copy again) came back completely clean:
`TOTAL attempts=8000 success=7935 ring_full(-3)=63 race_lost(-5)=2 other_err=0 - ring
drained=yes`, `0/25 shown mismatched`, ending in the code's own explicit `PASS: all
decoded samples cross-checked OK, ring fully drained, no unexpected return codes` line.
No oops in either run; `/proc/modules` confirmed the module stayed `Live` throughout both.

**The one real, substantive finding, and what it means**: run 1's single mismatch (packet
`[0]`, `reg=1` but `data=0x00000000`) is not noise or a decoder bug - decoding it against
what each thread actually sent shows `reg=1` matches thread 1's `InitLCDRegs` call while
`data=0x00000000` matches thread 0's, i.e. **two producers' field writes tore across the
same ring slot** before either called `AddFifoEvent()`. This is a genuine, narrow gap the
disassembly-only pass could not have found: `GetNextFreeFifoEvent()` can hand the *same*
`&pEvents[dwWriteIndex]` pointer to two racing callers (nothing serializes the fetch), and
while `AddFifoEvent()`'s recheck-and-fail-with-`-5` correctly stops the **loser** from
double-advancing `dwWriteIndex` or double-counting `sEventsToProcess` (confirmed working -
3 and 2 real `-5` returns were observed in the two runs respectively, and `dwWriteIndex`
never got corrupted in either run: the ring stayed internally consistent and drained to
exactly 0 both times), it does **not** stop the two callers' raw field writes into that
shared slot from interleaving first. The **winner**'s committed event can end up carrying
a mix of both callers' data, not garbage and not silently dropped-and-recovered - a subtle
distinction from what the code comment set out to guarantee. This mirrors ground truth's
own real mechanism byte-for-byte (this reconstruction's `AddFifoEvent` is confirmed
byte-exact against the real binary), so this is not a reconstruction bug either - it is a
genuine characteristic of the real driver's own real design, only ever observable by
actually racing two callers against it, which is exactly what this test is for.

**Honest assessment - what this test does and doesn't prove**:

- **Proves**: the reconstructed `pop_free_urb`/`AddFifoEvent`/`GetNextFreeFifoEvent`
  code, run under genuine 4-CPU concurrent producer load against the real single-consumer
  path for 8000 real draw-builder calls across two independent full runs, produces no
  kernel oops, no hang, no ring corruption (`dwWriteIndex`/`sEventsToProcess` bookkeeping
  stayed exactly consistent - the ring drained to precisely 0 both times), and the 5
  real wire-packet formats documented in `ProcessEvents`' own comment are exactly what
  gets submitted (24/25 and 25/25 sampled packets decoded correctly across the two runs,
  cross-checked against independently-encoded redundant fields, not just "looked
  plausible"). The `-5` race-loss path is not just theoretically reachable - it fired 3
  and 2 times respectively under real contention, exactly as designed.
- **Also proves, as a genuine second-order finding**: the race-check's real guarantee is
  narrower than "producers never interfere" - it guarantees the ring's own bookkeeping
  (`dwWriteIndex`, `sEventsToProcess`, one committed event per successfully-won slot)
  stays consistent, not that a committed event's *payload* is always wholly one caller's
  data when two callers raced for the same slot. This is a real characteristic of ground
  truth's own design (confirmed byte-exact), not a reconstruction defect - but it's worth
  flagging for anyone relying on this ring for anything payload-integrity-sensitive under
  real multi-caller concurrency.
- **Does not prove**: multi-*popper* contention on `pop_free_urb`'s own lock specifically
  - in this VM's synchronous `vm_usb_submit_urb`-completes-inline model, and with ground
  truth's own single-consumer design for the video ring, `pop_free_urb` is only ever
  called from the one real consumer thread here, sequentially (just under heavy, sustained
  real call volume - thousands of pop/submit/complete cycles) - not from genuinely
  concurrent poppers. Exercising that specific scenario would need either real async URB
  completion (a different, harder VM gap - see `OmapNKS4VirtualBoard/README.md`) or
  multiple genuinely concurrent command-URB submitters, which was out of scope for this
  pass's video-ring focus.
- **Does not prove**: anything about `ContinueProcessingEvent`'s multi-chunk row-wrap
  arithmetic under a realistically-sized frame buffer and a realistic (>=512-byte) max
  packet size - this test deliberately used a trivial 1-byte region specifically to stay
  clear of that far more delicate, separate code path (already covered by the existing
  disassembly-verified pass, not this test's target).
- The one unreproduced hang (above) remains an open, honestly-flagged loose end - most
  likely tied to reusing a post-oops disk image rather than a genuine concurrency
  deadlock (two clean fresh-disk runs since), but not conclusively ruled out given the
  lack of kernel symbols to actually inspect a stuck task's real stack.

No changes were made to the canonical `kronos_local.img`; all testing used fresh
`cp --sparse=always` scratch copies in a dedicated, since-removed scratch directory on
`kronosvm`. `make verify`'s host suite was re-run clean after every source change in this
section (0 failures, both suites) - unaffected by these VM-only additions as expected.


## Fidelity notes

- The module is `-mregparm=3`: the first three int/pointer args are in EAX/EDX/ECX and
  C++ instance methods receive `this` in EAX. Reconstructed methods are plain C++; the
  ABI is reproduced by the build flags, not by hand.
- ~~The `stg_*` / `rtwrap_*` / `CSTGThread` layer is the **shared** STG framework (same
  across `OA.ko`, `loadmod.ko`, …), so it is imported, not re-implemented here.~~
  **WRONG, superseded by the "Continued RE, 2026-07-17 (session 2)" finding below and
  `rtwrap.cpp`'s own header comment**: Ghidra's function-size accounting shows every
  `rtwrap_*`/`CSTGThread` symbol at a real, substantial address *inside*
  `OmapNKS4Module.ko` itself (0x17f50-0x18c71), not a 1-byte extern-thunk stub like the
  genuine RTAI imports. It's a per-module statically-linked veneer, not something
  imported at insmod time from `STGEnabler.ko`/RTAI — each STG module almost certainly
  carries its own private compiled copy.
- Some panel-command payload bytes are assembled in the submit path; where the
  decompiler elides them they are reconstructed from the protocol behaviour and noted
  inline.
