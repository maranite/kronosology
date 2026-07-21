# OmapNKS4Module.ko - reconstructed source

Reverse-engineered source for the Korg Kronos **`OmapNKS4Module.ko`** kernel module
(firmware 3.2.2, 89849-byte target binary). Target: **Linux 2.6.32.11 + RTAI**, x86-32,
built with **`g++ -mregparm=3 -fno-exceptions -fno-rtti`** (it is a C++ kernel module).

This is a functionally faithful reconstruction recovered via Ghidra decompilation and
raw disassembly, not a byte-for-byte rebuild of the original source tree. Struct layouts
(field offsets/sizes) and wire-protocol encodings are reproduced exactly, cross-checked
against instruction-level disassembly of the shipped binary; method bodies are
transcribed from the decompiler and corrected wherever disassembly disagreed with it.
All 255 real functions in the target binary have a corresponding implementation
somewhere in this tree, and the module links and loads cleanly against a matching kernel
tree (see Building).

## What the module is

`OmapNKS4Module` is the **USB driver and real-time service task for the Kronos front
panel** - an OMAP-based "NKS4" board carrying the keybed, knobs/sliders/buttons, LEDs,
the colour LCD, the S/PDIF clock, and the **Atmel NV2AC security chip**. It:

- registers as a USB driver and binds the panel's bulk/interrupt endpoints;
- speaks a 32-bit-word **packet protocol** (`COmapNKS4Command`) to query versions,
  configure scanning/encoders/LEDs, drive the LCD, and read the security chip;
- decodes inbound event packets (`COmapNKS4Driver::ReceiveEventBuffer`), applies
  calibration and a sustain filter (`CNKS4EventFilter`), and forwards events to user
  space via `/proc` entries;
- runs an **RTAI real-time thread** (`CActiveSenseThread`) that paces output and fires
  the panel "active sense" tick;
- exposes a small **C-ABI** (`COmapNKS4Driver_*`, `stgNV2AC_*`) used by the rest of the
  Korg engine - the OA/GetPubId security path reads the NV2AC chip through this module's
  own Atmel command path, the same way `AT88VirtualChip.ko` (see that module's own
  README) stands in for the chip when this module's real hardware path isn't available.

## Architecture / subsystems

| Subsystem | Type(s) | File |
|---|---|---|
| Shared types / struct layouts | (all structs) | `omapnks4.h` |
| Framework externs (STG/RTAI/kernel), singletons | - | `omapnks4_internal.h` |
| Output FIFOs + active-sense RT thread + event filter | `CSTGOmapNKS4Fifos`, `CSTGOmapNKS4OutputFifo`, `CActiveSenseThread`, `CNKS4EventFilter` | `realtime.cpp` |
| Panel wire protocol (version/config/scan/LED/LCD/chip cmds) | `COmapNKS4Command` | `command.cpp` |
| Driver state machine, event decode, Atmel/NV2AC, C-ABI | `COmapNKS4Driver` | `driver.cpp` |
| Colour-LCD draw pipeline | `COmapNKS4VideoAPI` | `video.cpp` |
| USB probe/disconnect, URB write/interrupt callbacks | (free fns) | `usb.cpp` |
| `/proc` interface + event queue | (free fns) | `procfs.cpp` |
| URB submit/wait, signals, calibration | (free fns) | `submit.cpp` |
| RTAI/STG calling-convention veneer (`rtwrap_*`) | (free fns) | `rtwrap.cpp` |
| Soft-float helpers (present, currently dead code - see Known limitations) | (free fns) | `softfloat.cpp` |
| `init_module`/`cleanup_module` + the two service threads | (free fns) | `main.cpp` |

Everything is C++ (`.cpp`) - the `usb`/`procfs`/`submit`/`main` units use only
C-style code internally but call into `Class::method`/singletons elsewhere, so the whole
module compiles with one toolchain, matching the original `g++` build.

### The `stg_*`/`rtwrap_*`/`CSTGThread` layer is private to this module, not imported

Every `stg_*`/`rtwrap_*` symbol and every `CSTGThread` method has a real, substantial
address *inside* `OmapNKS4Module.ko` itself (in the 0x17f50-0x18c71 range of the target
binary) - not a 1-byte extern-thunk stub the way genuine kernel/RTAI imports
(`printk`, `rt_sem_wait`, `scsi_*`, etc.) are. This is a thin, per-module
statically-linked pthread-style veneer over RTAI's own C primitives, most likely
compiled from a shared Korg SDK source file linked into every STG-based module rather
than a symbol resolved at load time - each module carries its own private copy. It is
**not** the same shared framework layer that `STGEnabler.ko` exports (that module
implements a different subset: USB pass-through, scheduler/cpumask helpers, RTAI timer
bring-up, VFS helpers). `rtwrap.cpp` (~460 lines) reconstructs all ~65 of these
functions; its calling-convention findings are summarized below.

### Data model (struct layouts)

- **`COmapNKS4Driver`** (40-byte singleton) - hardware versions, key count, progress-bar
  state, flags (test mode, installer support, S/PDIF clock error, download/shutdown).
  Field map (offsets confirmed against every real C-ABI accessor):

  | Offset | Field |
  |---|---|
  | `+0x00`/`+0x01` | OmapVersion (major/minor) |
  | `+0x02`/`+0x03` | PSocVersion (major/minor) |
  | `+0x04`/`+0x05` | PanelLVersion (major/minor) |
  | `+0x06`/`+0x07` | PanelRVersion (major/minor) |
  | `+0x08`/`+0x09` | JackVersion (major/minor) |
  | `+0x0a` | Is88Key / number-of-keys byte |
  | `+0x0b` | HardwareVersion |
  | `+0x10` | TestMode |
  | `+0x1c` | SPDIFClockError |
  | `+0x1d` | StartScanning flag |
  | `+0x1e` | STGInDownload flag |
  | `+0x20` | EnableShutdownByDriver |
  | `+0x24` | NumberOfKeys (also mirrored into `+0x0a`) |

- **`COmapNKS4VideoAPI`** (12740-byte singleton) - a 384-entry ring of 33-byte LCD draw
  events plus screen geometry (`dwScreenBase`, X=800/Y=600); a worker pops events and
  emits USB video-bulk packets.
- **`CSTGOmapNKS4Fifos`** (1304-byte singleton) - 256-deep host<-panel event FIFO and
  64-deep host->panel command FIFO, drained by the kernel via an RTAI SRQ (label
  `"NKS4"`, handler `COmapNKS4Driver_HandleOutputSysReq`, probe-wait timeout 10000
  jiffies). `TriggerOutputInterrupt`/`WriteCommand` call `rtwrap_pend_linux_srq(dwEnabled)`
  where `dwEnabled` doubles as the real registered SRQ id, not a plain boolean.
- **`CActiveSenseThread`** (28-byte heap singleton) - TSC-paced 500 ms tick. Embeds a
  `CSTGThread` base at offset 0 (see below); its own first field, `qwNextTickCycles`,
  starts at `+0x08`.
- **`CSTGThread`** (8 bytes: `pTask` opaque RTAI task handle at `+0x00`, `bActive` byte
  flag at `+0x04`, 3 bytes padding) - has no vtable. `pTask` is written by
  `rtwrap_pthread_create()` through an out-parameter that aliases `this` directly (the
  object is its own out-param storage). `bActive` is set to 1 only after both task
  creation and debug-trap setup succeed; `Delete()`/`Wait()` are deliberate no-ops
  against a never-started or already-stopped thread. `CreateRealTimeWithCPUAffinity(fn,
  priority, cpumask, arg)` passes `this=EAX`, `fn=EDX` (entry point), `priority=ECX`,
  `cpumask`/`arg` on the stack; the RT task's stack size is hardcoded to `0x5000` bytes
  internally, not caller-supplied. `CActiveSenseThread_Setup()` calls it with
  `priority = GetMaxRealTimePriority() - 10`, `cpumask = 0`.
- **`OmapNKS4VideoAPIEvent`** / **`CNKS4EventFilter`** - see `omapnks4.h`.

## RTAI calling-convention notes (`rtwrap.cpp`)

The module is `-mregparm=3`: the first three int/pointer args normally go in
EAX/EDX/ECX, and C++ instance methods receive `this` in EAX. A number of real RTAI
primitives this module calls are stack-passed (cdecl) despite that ambient convention,
and are declared with `__attribute__((regparm(0)))` to match:
`rt_sem_wait_if`, `rt_sem_wait_timed`, `rt_sem_wait_barrier`, `rt_sem_signal`,
`rt_cond_wait`, `rt_cond_wait_timed`, `rt_cond_wait_until`, `rt_task_suspend`,
`rt_task_resume`, `rt_task_masked_unblock`, `rt_get_priorities`, `rt_get_time_cpuid`,
`start_rt_timer`, `rt_set_runnable_on_cpuid`. For `rt_sem_wait_timed`,
`rt_cond_wait_timed`, and `rt_cond_wait_until` even the leading pointer argument is
stack-passed, not just the trailing 64-bit one.

Confirmed register-passed / needing no override: `rt_task_init` (full 7-arg
regparm(3): task/fn/data in EAX/EDX/ECX, stacksize/priority/uses_fpu/signal on the
stack), `rtheap_alloc` (3-arg regparm(3): heap/size/flags), `rtheap_free`, `msleep`;
`rt_pend_linux_srq`, `rt_free_srq`, `rt_release_irq`, `rt_assign_irq_to_cpu`,
`rt_startup_irq`, `rt_shutdown_irq` (each a zero-instruction register tail-forward);
`nano2count`, `nano2count_cpuid`, `rt_sleep` (each takes a 64-bit argument, which GCC
never register-assigns under `regparm` on i386, so the ambient convention already
produces the right call shape); `rt_printk` (variadic, never regparm-eligible).

`rtwrap_pthread_mutex_init`'s `sem_type` mapping: `(*attr & 2) == 0` (RECURSIVE) maps
to `sem_type = 1`; `(*attr & 2) != 0` (ERRORCHECK) maps to `sem_type = -1`.
`rtwrap_pthread_barrier_destroy` treats `rt_sem_delete()` returning exactly `5` as the
failure case (returns `-EINVAL`); any other return value is success (returns `0`).

`rtwrap_pthread_join`'s Linux-context poll loop sleeps 10ms per iteration
(`msleep(10)`), not 1ms.

### The global-cli two-level lock

`rtwrap_global_save_flags_and_cli()`/`rtwrap_global_restore_flags()` implement a
two-level lock:

- `rtai_cpu_lock` is a genuine `unsigned long` bitmask, one bit per CPU, manipulated
  with atomic `LOCK BTS`/`LOCK BTR` - a per-CPU recursion guard: only the outermost
  save/restore call on a given CPU touches the second level below it; nested calls on
  the same CPU no-op through it.
- The 16-bit `_nano2count_cpuid` variable backs a cross-CPU ticket spinlock
  (`[serving_lo][ticket_hi]` byte pair): `LOCK XADD word ptr [_nano2count_cpuid], 0x100`
  hands out the next ticket, and the caller spins (`PAUSE` + re-read) until the serving
  byte matches its ticket - the classic ticket-lock idiom, serializing whichever CPUs
  are simultaneously the outermost holder on their own CPU into one critical section at
  a time. The release path bumps only the low (serving) byte with a plain, non-atomic
  increment - safe because the ticket lock's own mutual exclusion already guarantees at
  most one CPU is in that branch, plus a local `cli`.
- `per_cpu__cpu_number` is read via `%fs:`-relative per-CPU addressing, matching this
  kernel's per-CPU data model.
- The exact upstream-RTAI rationale for this precise scheme (ticket lock plus per-CPU
  recursion bit implementing "global cli") is inferred from the mechanism alone, not
  from any surviving RTAI source comment - see Known limitations.

## Wire protocol reference (`COmapNKS4Command`)

Panel commands are 32-bit words. Query commands are matched by their literal wire word;
setters that carry a variable byte are matched by opcode byte only.

| Command | Wire encoding | Reply tag | Notes |
|---|---|---|---|
| `CommunicationCheck` | `0x00ee0000` | `0x0066` | |
| `GetVersion` (4-out, no index) | `0x00f00000` | `0x0070` | Reply is one read decoded as four nibbles (OMAP/PSoC major/minor), not two reads decoded as whole bytes. |
| `GetVersion` (indexed, 1-arg) | reg byte `0xf0` | - | Used by the `hwVer==2/3` branches; reply byte layout for this form is not confirmed on real hardware - see Known limitations. |
| `ReadPortConfiguration` / `GetRawDipSwitches` | `0x01f10000` | `0x0171` | Same wire command as `GetRawDipSwitches`; the two differ only in how the response bytes (hwVer/is88 vs. raw switches) are decoded. |
| `SetNumberOfAnalogInputs`, `SetAllAnalogInputFilter`, `SetNumberOfLEDs`, `ConfigureRotaryEncoders` (3 words), `SetRotaryEncoderSampleSpeed`, `ConfigureScanning` | setter-specific | none | No reply expected; `COmapNKS4Driver_Configure()`'s boot sequence issues these in this order after `GetVersion`. |
| `SetLCDBrightness(level)` | `0xC7000000 \| level<<16` | none | |
| `ResetModule(mode)` | `0x06000000 \| mode<<16` | none | |

`COmapNKS4Driver_Configure()`'s `ConfigureScanning` call uses `bOmapRevision != 0`
unconditionally as its "arg5" boolean across every `hwVer` branch; the remaining two
boolean arguments come from `bPanelLVersion`/`bPanelLRevision` for `hwVer==2`, and from
`bPsocVersion`/`bPsocRevision` for `hwVer==3` and the default branch alike.

### Inbound event decode (`COmapNKS4Driver::ReceiveEventBuffer`)

Interrupt-IN packets are a sequence of 4-byte records `[dLo][dHi][idx][op]`, terminated
by a Sync record (`op == 0x87`). Dispatch is on `op` and, for buttons, on `idx & 0xf0`:

- `op == 1`, `idx & 0xf0 == 0x50` - button/key event. With installer support enabled,
  this also raises a `/proc` event: `v > 0` raises event `0xd`, `v < 0` raises event
  `0xe`, and `v == 0` raises no event at all (both proc-event call sites are skipped).
- `op == 3` - aftertouch. The calibration input is the 10-bit ADC reassembly
  `raw10 = (dLo << 2) | (dHi >> 6)`, not a naive 16-bit little-endian combine of
  `dLo`/`dHi`. This is the same `raw10` value the test-mode branch already computes
  independently for its own `0x61` diagnostic event.
- `idx == 0x71`, `op == 7`, `op == 0x1f`, `op == 8`, and the `op < 9`/`op >= 9` split
  cover the remaining dispatch paths (S/PDIF clock status, shutdown-request, and other
  event classes).

### Calibration (`submit.cpp`)

`ApplyNKS4Calibration(chan, raw)` dispatches per fixed channel number, not through a
uniform table lookup: channels 5, 6, and `0x1d` each use one of three independent,
non-uniformly-spaced calibration-table slots (`sCalibrationData + 0x5c/0x70/0x84`);
channels `0x10`-`0x18` use a direct scale/offset float formula; channel `0x1b` indexes
a 256-entry foot-pedal response-curve table baked into the binary's own read-only data;
every other channel passes through unchanged. The function is gated on both
`sCalibrationData` **and** `sCalibrationMsgCallbackFunc` being set - when both are set,
the callback is invoked unconditionally on every call with `(chan, raw)`. Because its
real callers run from USB interrupt-completion context, it performs a
CLTS/FXSAVE/FNINIT/FXRSTOR FPU-enable sequence around the float math, unlike this
module's other two floating-point use sites.

`ApplyGenericCalibration`'s low- and mid-threshold range comparisons are both
inclusive (`<=`), not exclusive (`<`).

`COmapNKS4Driver_ApplyAftertouchTable` (the free-function C-ABI form) is not a
pass-through to the member `COmapNKS4Driver::ApplyAftertouchTable` despite the shared
name: it first calls `ApplyNKS4Calibration(chan=7, v)` (hardcoded channel 7, the same
aftertouch calibration channel `ReceiveEventBuffer`'s own `idx==7` case uses), then
applies the `sAfterTouch*ConvertTable` lookup to the *calibrated* result. The member
function is the plain table-only lookup with no calibration step - the two are
genuinely different, not duplicate implementations of the same logic.

### Atmel NV2AC chip access

`driver.cpp` implements the real hardware path OA.ko's security/EXs-authorization code
uses to read the AT88/NV2AC chip through this panel's own USB link. The read-payload
byte order is a true 32-bit reversal (`[b0,b1,b2,b3] -> [b3,b2,b1,b0]`), not a 16-bit
paired swap; the payload length byte must be sign-extended before being widened to a
wider integer type, not treated as a plain unsigned byte.

### Video draw pipeline (`video.cpp`)

`COmapNKS4VideoAPI::ProcessEvents()` dispatches five non-streaming draw opcodes -
`InitLCDRegs` (`0xc0`), `XAxisByteSize` (`0x81`), `SendPixelDataRegion` (`0xc2`),
`SendFillData` (`0xc4`), `UpdateColorPal` (`0xc5`) - each of which packs its own
parameters into a specific wire header before submitting; the exact per-opcode byte
layout is documented in `ProcessEvents()`'s own header comment in `video.cpp`.
`SendPixelDataRegion(width, offset, rowBytes)` is a genuine 3-argument function - its
third field corresponds to the row-width field of `OmapVideoModule.ko`'s
`omapfb_ioctl`/`OMAPFB_FLUSH` call, `struct omapfb_flush {count, offset, width}`.

Each of the five draw-builders routes through `GetNextFreeFifoEvent()` (ring-full wait
plus slot fetch) and `AddFifoEvent()` (a re-fetch-and-compare recheck,
`if (e != &pEvents[dwWriteIndex]) return -5;`, immediately before the commit) rather
than writing the ring slot directly. This recheck guarantees the ring's own bookkeeping
(`dwWriteIndex`, `sEventsToProcess`, exactly one committed event per won slot) stays
consistent when two producers race for the same slot - it does **not** guarantee a
committed event's payload is wholly one caller's data if two producers wrote into the
same slot before either called `AddFifoEvent()`; the winner's event can carry a mix of
both callers' field writes. This is a genuine characteristic of the real driver's
design, not a reconstruction defect - see Known limitations for what it means for
callers that need payload integrity under concurrent producers.

`ContinueProcessingEvent`'s streaming byte-copy uses a 0x80-dword (not 0x88-dword)
packet buffer; its end-of-region marker sends a fixed 4-byte header-only packet, not
the whole buffer; and its row-wraparound branch (row exhausted, columns still left)
jumps directly into the byte-copy body rather than looping back through the outer
bounds checks - behaviorally identical to a plain `continue` for any nonzero row width,
but a genuine divergence at a zero-width region.

`OmapNKS4VideoAPIProcessEvents` (the free-function C-ABI form) drains the whole event
queue in a loop, calling the equivalent of `ProcessEvents()` repeatedly until it would
return 0 - it is not a single-step call.

`HandleOutputSysReq`'s 0x0900-marker handling: on hitting a `0x0900` marker in FIFO
slot `n`, the loop does not discard the rest of the batch immediately. It decrements
`avail` (the same bound the outer loop tests), calls `SetShutdownDelay()` with the
marker word's own low 16 bits as its argument (not a constant `0`), sets
`fShutdownRequested`, and either re-fetches the next entry into the same slot `n` and
retests it (if `n` is still less than the shrunk `avail`), or stops once `avail`
catches up to `n`. The final `SubmitNKS4CommandMultipleWriteNONBLOCKING()` call always
passes the (possibly-shrunk) `avail`, not an index.

## USB / URB management (`usb.cpp`)

URB pools: **16 command URBs, 256 video URBs**. The allocator deliberately never links
the first allocated command URB onto its free list - that URB is reserved for the
emergency-stop path, so it can never be handed out while in flight.

Each free list's head is not a bare pointer: it is a real 8-byte `urb_node`-shaped
pair, with a self-referencing sentinel value plus a reserved 4-byte pad slot
immediately after it. `hlist_add_head()`-style pushes onto an empty list always write
into that pad slot; without a reserved slot there, the very first push after either
pool is fully drained would silently corrupt whatever data follows the free-list head
in memory. `free_all_urbs()` and `OmapNKS4Probe()` (before the first allocation
attempt) reset both free-list heads/pads, both in-use counters, and both pool arrays
unconditionally - correctness does not rely on BSS zero-init being valid past the very
first module load.

`pop_free_urb()` checks emptiness (`*list == list`) unlocked, then acquires the lock
and pops unconditionally (no second check inside the critical section) - this ordering
matters and must not be reversed. The pop itself performs a real `hlist_del()` (poison
values on the removed node, `pprev` fixup on the next node), uses two separate lock
objects (one per free list, held with `_spin_lock_irqsave`/`_irqrestore`), and
increments the in-use counter inside the same critical section as the pop.

Command/video URB transfer buffers and the interrupt URB's transfer buffer are all
sized from the endpoint's real negotiated `wMaxPacketSize`, not a hardcoded constant -
a buffer sized smaller than the value already used for `urb->length` would be a
heap-overflow risk once the USB core reads/writes up to the claimed length.

`WriteCallback` passes `urb->transfer_buffer` and `urb->actual_length >> 2` (byte
length converted to a word count) to `COmapNKS4Driver_NotifyTransmittedCommandComplete`.
Its `sDoingWait4Write`-guarded wake and `WaitForNKS4CommandWrite`'s own sleep both
target the same wait queue as read-event delivery (`sReadEventWaitQueue`) - there is no
separate "command write done" queue in the real binary.

`InterruptCallback` handles inbound interrupt-IN packets; on completion it hands the
buffer to `COmapNKS4Driver_ReceiveEventBuffer` unmodified.

## Wait/blocking primitives (`submit.cpp`)

Real blocking, not polling, backs every wait helper in this file:

- **`WaitForNKS4ReadEvent`** - `prepare_to_wait`/check-condition/`schedule_timeout`/
  `finish_wait`, budget 1000 jiffies.
- **`WaitOnAtmelRead`** - a single tail-call to the kernel's own
  `sleep_on_timeout(sAtmelReadWaitQueue, 0x7d0)` (2000 jiffies), not a loop.
- **`WaitForNKS4CommandWrite`** / **`WaitForFreeBulkWriteURB`** - the same
  `prepare_to_wait`/`schedule_timeout`/`finish_wait` shape, budget 10 jiffies,
  re-armed on each pass of an outer 51-iteration retry-and-log loop (~510 jiffies of
  real blocking before the first "waiting for..." log line). Giving up after 51
  iterations is not an error - it logs a bare, unprefixed "waiting for..." message and
  restarts the whole wait from scratch.

A local `struct omap_wait_entry` (`submit.cpp`) / `nks4_usb_wait_entry` (`usb.cpp`)
mirrors the real 20-byte i386 `wait_queue_t` (`flags`/`private`/`func`/
`task_list{next,prev}`), using `%fs:per_cpu__current_task` for the `private` field and
`autoremove_wake_function` as the wake callback.

`ProcessMsgRoutine`/`ShutdownSSDRoutine` (`main.cpp`) block the same way rather than
polling, with real per-thread timeouts of 3 jiffies and 10000 jiffies respectively.
`block_all_signals()` is not a real kernel primitive callable from this module (the
real 2.6.32 `block_all_signals()` is an unrelated NFS-lockd helper); both threads
instead inline the classic "block every signal" sequence directly against task-struct
fields (`sighand`@`+0x2a0`, `blocked.sig[0]/[1]`@`+0x2a4`/`+0x2a8`,
`sighand_struct.siglock`@`+0x504`).

`OmapNKS4Exit`'s two `wait_for_completion_timeout()` calls wait 2000 jiffies (2s)
each. SCSI SSD shutdown (`ShutdownSSDRoutine`) tries exactly four fixed host indices
(0-3) in order, stopping at the first with a device at channel/id/lun 0, and
transitions it through `SDEV_CANCEL` (3) then `SDEV_DEL` (4) before a final
`msleep(1000)`.

## `/proc` interface (`procfs.cpp`)

Four entries are created unconditionally at probe time:

| Path | Mode | Purpose |
|---|---|---|
| `/proc/OmapNKS4` | `0666` | Event queue read/write, command dispatch. |
| `/proc/OmapNKS4ProgressBar` | `0666` | Progress-bar control. |
| `/proc/OmapNKS4HardwareVersion` | `0444` | Read-only. |
| `/proc/OmapNKS4OmapVersion` | `0444` | Read-only. |

`OmapNKS4ProcWrite`'s keyword dispatch table recognizes: `clear`, `enable`, `disable`,
`unblock`, `block`, `async`, `sync`, `allow_shutdown_by_driver`, `61key`, `73key`,
`88key`. `OmapNKS4ProcWriteProgress` recognizes `inc`, `set`, `add`.
`OmapNKS4ProcReadEvent`'s poll loop is a `do { sleep 40ms; check queue; } while
(sBlockOnRead);`, not a bare `for(;;)` re-checking at the top of every iteration.

A fifth entry, `/proc/OmapNKS4InjectEvent` (mode `0222`, write-only), exists only when
the `vm_virtual_probe` module parameter (see Building/testing below) is set - it is
never created on real hardware. It accepts a raw interrupt-IN event buffer in the same
`[dLo][dHi][idx][op]` wire format described above and feeds it through the real,
unmodified `InterruptCallback()`/`ReceiveEventBuffer()` decode path, for driving
arbitrary key/knob/analog-input event testing without a physical or emulated USB
device attached. The buffer is silently clamped to the interrupt endpoint's own
negotiated `wMaxPacketSize` and rounded down to a whole number of 4-byte records.

## Building

This is one of the rare **C++ kernel modules**. It depends on the Korg "STG" support
layer (global ctor/dtor runner `init/cleanup_cpp_support`, `operator new/delete` over
`stg_kmalloc`, `__cxa_pure_virtual`) for genuine kernel/RTAI primitives resolved at
`insmod` time (the `stg_*`/`rtwrap_*` veneer itself, as noted above, is compiled into
this module directly, not imported). Build flags:

```make
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive
```

```sh
make ko KDIR=/path/to/kronos-kernel-tree
```

`KDIR` must point at a configured Linux 2.6.32.11-korg source tree whose module ABI
(struct layouts, the `-mregparm=3` calling convention, RTAI support) matches the
Kronos's own kernel build - a generic, unpatched 2.6.32.11 tree produces the wrong
`vermagic`/struct layout and will not load. A correct build reports vermagic
`2.6.32.11-korg SMP preempt mod_unload ATOM`.

```sh
make            # host-side sanity compile of each unit + make verify's known-answer tests
make verify      # same test suite directly; no kernel tree needed
```

The host-side `verify/` suite (`test_command`, `test_driver_receive_event_buffer`)
covers `command.cpp`'s wire-word encodings and `driver.cpp`'s `ReceiveEventBuffer`
decode path with known-answer tests. The remaining eight translation units require a
real kernel build to validate beyond compile-clean.

### `vm_virtual_probe`: testing without physical hardware

`OmapNKS4Module.ko` accepts a `vm_virtual_probe` module parameter (default 0,
completely inert on real hardware). When set, `OmapNKS4Init()` synthesizes a virtual
NKS4 USB device in-process (vendor `0x0944`/product `0x1005`, one interrupt-IN and one
bulk-OUT endpoint) and feeds it to the real, unmodified `OmapNKS4Probe()` - no second
module, no real USB bus, no dummy HCD required. This works because `OmapNKS4Probe()`'s
own logic never dereferences a real kernel `usb_interface`/`usb_device` through their
normal accessors; it reads fixed offsets out of the pointer directly, which a
correctly-shaped block of plain kernel memory satisfies equally well.

Under `vm_virtual_probe=1`, a `vm_usb_submit_urb()` wrapper stands in for
`stg_usb_submit_urb()` at every real call site, decoding and replying to whichever
wire command was sent (`CommunicationCheck`, `ReadPortConfiguration`/
`GetRawDipSwitches`, `GetVersion`, `SetLCDBrightness`, `ResetModule`, and every setter
in `COmapNKS4Driver_Configure()`'s boot sequence) so the real driver's init/probe/
configure/thread-startup sequence can run to completion without physical hardware.

A second module parameter, `vm_video_stress` (only meaningful when `vm_virtual_probe=1`
is also set), drives four producer threads through 2000 iterations each of the five
video draw-builder functions with no synchronization between them, exercising the
video ring's producer-side concurrency handling (see "Video draw pipeline" above) under
genuine multi-CPU contention, draining through the real, already-running worker thread.

## Fidelity notes

- The module is `-mregparm=3`: the first three int/pointer args are in EAX/EDX/ECX, and
  C++ instance methods receive `this` in EAX. Reconstructed methods are plain C++; the
  ABI is reproduced by the build flags, not by hand.
- Some panel-command payload bytes are assembled in the submit path; where the
  decompiler elides them, they are reconstructed from the protocol behavior and noted
  inline in the source.
- `init_omap_nks4_usb_driver()` (`main.cpp`) has no counterpart in the real binary -
  ground truth compile-time-initializes its `struct usb_driver` as static data, where
  this reconstruction populates the same fields at runtime. The two are behaviorally
  identical (every field value matches); this is a structural difference only.

## Known limitations

Each item below is either genuinely unconfirmed against real hardware, or a real
characteristic of the design that a future caller should be aware of. Where a
validation path exists, it's noted.

- **`GetVersion`'s indexed 1-arg form (the `hwVer==2`/`3` branches) has an unconfirmed
  reply-byte mapping.** The 4-out no-index form's decode is disassembly-confirmed; the
  indexed form's reply layout is inferred by analogy, not independently verified. *To
  validate:* capture real wire traffic from a Kronos 2/3 (or Nautilus) unit, whose
  `hwVer` differs from the Kronos 1/X unit this reconstruction was primarily verified
  against.
- **`softfloat.cpp`'s own arithmetic was never verified against ground truth, and no
  ground truth of that shape exists in the current target binary to verify it
  against.** All three of this module's real floating-point use sites
  (`ApplyGenericCalibration`, `CActiveSenseThread`'s tick pacing,
  `SetProgressBarPercent`) use real x87 hardware instructions directly in the shipped
  binary - none calls a soft-float helper. `Makefile` overrides `CFLAGS_submit.o`/
  `CFLAGS_realtime.o`/`CFLAGS_driver.o` to `-mhard-float` so these three files emit real
  FPU instructions instead of soft-float calls, matching this finding; `softfloat.cpp`
  is left in place as an inert fallback for any future translation unit that adds
  float/double arithmetic without its own `CFLAGS_*.o` override. *To validate:* if a new
  translation unit needs this fallback, verify its specific arithmetic independently -
  this file's own correctness has no known ground truth to check against on this binary.
- **`rt_request_irq`'s wrapper (`rtwrap_request_irq`) is dead code with an internally
  inconsistent argument setup** - it has zero callers anywhere in the target binary, and
  its own body only marshals one of its two outgoing arguments before calling the real
  primitive. Left declared under the ambient `regparm(3)` convention rather than
  guessed at, since there is no live call site to cross-check a fix against. *To
  validate:* if a real caller is ever identified (in this binary or a later firmware
  revision), disassemble that call site directly before trusting this wrapper.
- **The exact upstream-RTAI rationale for the `rtai_cpu_lock` + ticket-lock scheme**
  (`rtwrap_global_save_flags_and_cli`/`_restore_flags`) is inferred from the
  disassembled mechanism alone, not from any surviving RTAI source comment in this
  project. *To validate:* cross-check against real RTAI source for the target kernel
  version, if one becomes available.
- **`rtwrap_set_debug_traps_in_rt_task()`'s second argument** (a fixed buffer address
  loaded at its call site) is not reproduced - the real buffer's size/layout could not
  be recovered from disassembly alone. Nothing currently reachable in this module
  depends on the buffer's contents, only on the function's integer return value (which
  is reproduced correctly and is load-bearing - `CreateRealTimeWithCPUAffinity()`
  genuinely branches on it to decide whether to tear a just-created RT task back down).
  *To validate:* if a future caller needs this buffer's real contents, a dedicated
  disassembly pass around the real call site would be needed.
- **The video ring's producer-side race check guarantees bookkeeping consistency, not
  payload integrity, when two producers race for the same slot** (see "Video draw
  pipeline" above) - this is a genuine characteristic of the real driver's design, not
  a reconstruction defect, but it means any caller relying on this ring for
  payload-integrity-sensitive data under real multi-caller concurrency should
  synchronize its own producers rather than rely on the ring alone.
- **`pop_free_urb`'s locking has only been exercised against a single real consumer
  thread** draining the ring sequentially under heavy call volume - genuine multi-popper
  concurrency (multiple simultaneous command-URB submitters, or real asynchronous URB
  completion rather than the synchronous completion this module's own test scaffolding
  uses) has not been exercised. *To validate:* test with concurrent submitters, or with
  a real USB host controller driving genuinely asynchronous URB completion.
- **`SendPixelDataRegion`'s multi-chunk row-wrap streaming — CONFIRMED on real
  hardware (2026-07-21).** Independently reconstructed `OmapVideoModule.ko`
  (`reconstructed/OmapVideoModule/`) loaded cleanly against this module's genuine
  stock binary on a real Kronos 2 dev board (all 11 needed exports resolved via a
  real `__ksymtab` — see `docs/modules/OmapNKS4Module.ko.md`'s "Exported kernel
  symbols" section), then a deliberately narrow `OMAPFB_FLUSH` ioctl
  (`KronosFB/narrow_flush_test.c`: rect 137×60px at offset 100,50, i.e. `width=137`
  vs. the panel's real `line_length=800` — not a multiple of the 0x200-byte USB
  chunk size, forcing genuine multi-chunk row-wrap) rendered correctly on the
  physical panel: a static (non-corrupted, non-flickering) rectangle, user-measured
  at ~1.1"×0.5", consistent with the requested 137/800 × 60/600 proportions on a
  real panel. No dmesg errors. This closes the "not under a realistic non-full-size
  frame buffer" gap that motivated this item.
- **`ShutdownSSDRoutine`'s 10000-jiffy (~10s) wait loop — CONFIRMED (2026-07-21).**
  Root cause of why this had never been observed: `tools/run_vm_virtual_probe_test.sh`
  shuts the VM down the instant `loadoa`'s own script completes, which happens almost
  immediately after this thread prints its "alive, entering main loop" line - no
  amount of raising `--timeout` could ever help, since the poll loop exits (and
  proceeds straight to shutdown) the moment completion is detected. Added a
  `--linger SECONDS` option (keeps the VM up and still capturing console output for
  N extra seconds after completion, before shutdown) and re-ran with `--linger 45`.
  Captured 5 consecutive `DIAG SSD#N` cycles. Sanity-checked the timing itself, not
  just the print's presence: combining each line's `delta_hi`/`delta_lo` into the
  full 64-bit cycle count and dividing by the reported `khz` gives ≈10.0006s per
  cycle, every time — matching the intended 10000-jiffy timeout almost exactly, the
  same real confirmation `ProcessMsgRoutine`'s twin already had (cross-checked this
  same run's own `DIAG PMR#N` lines the same way: ~2.9-3.4ms per 3-jiffy cycle,
  i.e. this kernel runs at HZ=1000/1ms-per-jiffy, and 10000 jiffies vs. 3 jiffies
  scales to the observed ~10.0006s vs. ~3ms almost exactly). `--linger` is now
  available for any future background-thread diagnostic that fires after `loadoa`'s
  own completion.
- **`/proc` entry names changed from an earlier assumed `/proc/nks4*` convention to the
  real `/proc/OmapNKS4`, `/proc/OmapNKS4ProgressBar`, `/proc/OmapNKS4HardwareVersion`,
  `/proc/OmapNKS4OmapVersion` paths.** Any userspace tool that reads these paths needs
  to target the real names, not the earlier assumed ones.
- **A handful of leaf helpers belonging to the shared STG/kernel framework layer** -
  `kmalloc_buf`, `proc_set`, `wait_event*`, `dev_shutdown`, the generic calibration
  curve, and similar - are referenced as externs/thin shims rather than reimplemented
  here. Supply them from the real kernel/STG framework headers, or from stock kernel
  equivalents, to get a clean link against a specific target kernel tree.
