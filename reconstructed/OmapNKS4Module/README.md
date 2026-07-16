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
> field offsets computed from struct layout math rather than disassembly-verified)
> - see `KronosNKS4/docs/gaps.md` "Real Kbuild build attempt" for the complete,
> itemized confidence breakdown. Host `verify/` suite re-run clean throughout, with
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

## Fidelity notes

- The module is `-mregparm=3`: the first three int/pointer args are in EAX/EDX/ECX and
  C++ instance methods receive `this` in EAX. Reconstructed methods are plain C++; the
  ABI is reproduced by the build flags, not by hand.
- The `stg_*` / `rtwrap_*` / `CSTGThread` layer is the **shared** STG framework (same
  across `OA.ko`, `loadmod.ko`, …), so it is imported, not re-implemented here.
- Some panel-command payload bytes are assembled in the submit path; where the
  decompiler elides them they are reconstructed from the protocol behaviour and noted
  inline.
