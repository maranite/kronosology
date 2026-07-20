# RTAIVirtualDriver.ko — software substitute for the real RTAI stack

Reconstructed-project-original source (**not** decompiled from a shipping
binary — this subsystem has no shipping-binary counterpart). Target:
**Linux 2.6.32.11-korg + RTAI headers** (for type/declaration compatibility
only — this module does not actually depend on RTAI being loaded), x86-32,
`gcc -mregparm=3`.

## Extended 2026-07-17 for OmapNKS4Module.ko's own live boot test

Originally built for `OA.ko`'s own 26+3 symbols (see below). `OmapNKS4Module.ko`'s own
`rtwrap.cpp` needed ~17 more real RTAI symbols this module didn't yet export
(`rt_cond_wait`/`_timed`/`_until`, `rt_sem_broadcast`, `rt_sem_wait_timed`/`_barrier`,
`rt_sched_lock`/`unlock`, `rt_get_priorities`/`_time_cpuid`, `nano2count`/`_cpuid`,
`rt_sleep`, `set_debug_traps_in_rt_task`, `rt_task_masked_unblock`, plus the
`_nano2count_cpuid`/`rtai_cpu_lock` per-CPU data symbols) — added as a new numbered
section (see the source's own "9. Additional RTAI symbols..." comment), using the same
"match the caller's currently-declared convention, flag what wasn't independently
re-confirmed" discipline as every symbol below. **Real bug found and fixed in this
pass**: `rt_sleep`'s own new implementation used plain 64-bit `/` on a runtime `long
long` value, which needs GCC's `__divdi3` helper — unavailable in 32-bit kernel space,
a genuine "Unknown symbol `__divdi3`" insmod failure on the first live attempt. Fixed
with the kernel's own `do_div()` macro. Confirmed via a real, successful live boot test
(`OmapNKS4Module.ko`'s own `init_module()` running to completion) — see that module's
own README.md.

## What this replaces, and why

`OA.ko`'s own reconstructed `rtwrap_*` wrapper layer
(`reconstructed/OA/src/init/rtwrap.cpp`), daemon lifecycle
(`src/init/daemon_lifecycle.cpp`, `src/init/stg_daemons.cpp`) and RTAI-FIFO
layer (`src/init/rtfifo_init.cpp`, `src/engine/push_unsolicited_message.cpp`)
call 26 real RTAI symbols directly, resolved at `insmod` time against the
real `rtai_hal.ko`/`rtai_sched.ko`/`rtai_sem.ko`/`rtai_fifos.ko`/
`rtai_ndbg.ko`/`rtai_debug.ko` stack on real hardware.

`MASTER_REFERENCE.md` sec 10.211–10.214 (read those sections for the full
live-diagnosis trail) established, through repeated live GDB/QEMU-monitor
investigation on the dedicated `kronosvm` sandbox, that genuine RTAI cannot
be reliably brought up under QEMU-TCG on this target kernel:

- `rtai_hal.ko`'s own I-pipe/APIC hardware-timer takeover hangs
  deterministically right after printing `PIPELINE layers:` — a genuine,
  unrecoverable timer-interrupt deadlock caused by a TCG-timing race
  between a delayed native tick and RTAI's own next-interrupt-deadline
  calibration (sec 10.212, confirmed via the CPU-time-pinned-over-time
  diagnostic — the QEMU process's own cumulative CPU time stops climbing
  entirely, not just "looks idle"). This is inside stock, unbuilt-by-this-
  project `rtai_hal.ko` internals, **not** a bug in this project's own
  code and **not** fixable at this project's layer.
- Even when that first hang doesn't occur, a second, deeper stall was
  found one dependency level further in, inside the three silent stock
  RTAI modules (`rtai_sem.ko`/`rtai_ndbg.ko`/`rtai_fifos.ko`) (sec 10.214).
- QEMU's `-icount` deterministic-timing mode gets past the first hang fast
  but then causes a real, fatal kernel panic in RTAI's own scheduler init
  (`Kernel BUG at resched_task`, "Attempted to kill the idle task!") — sec
  10.214 confirms this is **harmful**, not merely untested, and should not
  be used as a mitigation.

Per the standing project policy
(`MASTER_REFERENCE.md` sec 10.185, *"RTAI substitution... a first-class,
encouraged technique"*), this module is a from-scratch, VM-appropriate
**substitute** for the 26 real RTAI symbols `OA.ko`'s own reconstructed
callers actually invoke — architecturally the exact same pattern as this
project's three existing hardware virtual drivers:

| Virtual driver | Stands in for |
|---|---|
| `AT88VirtualChip.ko` | `OmapNKS4Module.ko`'s AT88SC/NV2AC chip access |
| `OmapNKS4VirtualDriver.ko` | `OmapNKS4Module.ko`'s 9 other exports |
| `KorgUsbAudioVirtualDriver.ko` | `KorgUsbAudioDriver.ko`'s USB audio/MIDI codec access |
| **`RTAIVirtualDriver.ko`** | **the real RTAI stack itself** (`rtai_hal.ko`/`rtai_sched.ko`/`rtai_sem.ko`/`rtai_fifos.ko`/`rtai_ndbg.ko`/`rtai_debug.ko`) |

Because this module deliberately **never** attempts genuine hard-real-time
scheduling — no I-pipe domain registration, no hardware-timer takeover, no
APIC reprogramming — it entirely **sidesteps** the exact QEMU-TCG timing
hazard sec 10.212–10.214 diagnosed, rather than working around it. Every
one of the 26 symbols below is implemented purely with ordinary Linux
kernel primitives that behave identically whether the guest is real
hardware or QEMU-TCG.

## What guarantee this deliberately does NOT provide

Genuine hard-real-time scheduling latency/determinism. Specifically:

- A "task" here is an ordinary preemptible Linux kthread, not a hard-RT
  thread running outside the Linux scheduler's normal jitter.
- `rt_task_suspend()`/`rt_task_resume()` can only meaningfully gate a task
  **before** it starts running its entry function — a plain kthread has no
  external-preemption primitive equivalent to real RTAI's, so this
  substitute cannot forcibly interrupt an entry() function already mid-run
  from the outside. See the function's own header comment in the source.
- `rt_typed_sem_init()`'s real RTAI "mutex mode" (recursive/errorcheck
  encoding, packed into the `value` argument for type==3 resource
  semaphores) is not reproduced — every type==3 semaphore simply starts
  **available**, matching real RTAI's own "freshly-initialized resource
  semaphore is unlocked" behavior, without the recursive/errorcheck
  behavioral nuance on lock.
- `rt_request_srq`/`rt_pend_linux_srq` defer to a workqueue unconditionally
  (there is no genuine hard/soft context split left to preserve, since no
  hard-RT context ever exists in this substitute) rather than the real
  SRQ mechanism's own hard-RT-to-soft-IRQ signaling path.
- The IRQ-affinity family (`rt_assign_irq_to_cpu`/`rt_release_irq`/
  `rt_shutdown_irq`/`rt_startup_irq`) are pure no-ops — there is no real
  hardware IRQ routing to manage in a VM.

None of this matters for this module's actual purpose. Per sec 10.185,
real-time audio/DSP fidelity is **already** explicitly out of scope for
this project's VM-testing effort — this module's only job is to let
`OA.ko`'s own structural init/daemon-lifecycle logic run to completion (or
fail on its own real logic, not on missing RTAI symbols) in an environment
where genuine hard-RT was never achievable anyway.

## A genuine, pre-existing limitation this module does not and cannot fix

`rtwrap_pthread_create()` (`reconstructed/OA/src/init/rtwrap.cpp`) passes
`rt_task_init()` a hardcoded ground-truth address constant
(`RTWRAP_THREAD_TRAMPOLINE = (void *)0x118e80`) as the RT task's own entry
point — that file's own header comment already documents this as an
intentionally-unreconstructed internal `OA.ko` trampoline function, modeled
only as an opaque address because nothing in that file calls it directly
(it only ever gets *passed through* to `rt_task_init`). This module's
`rt_task_init()` faithfully does what real RTAI would do with whatever
entry pointer it is given — spawn a kthread that eventually calls it — but
it cannot make that specific hardcoded address be a real, valid function in
this project's freshly-linked `OA.ko` (its own `.text` layout necessarily
differs from the original ground-truth binary that constant was extracted
from). **Any live boot that reaches the point where a daemon's kernel
thread is actually resumed and jumps to that address is expected to fault
there** — a pre-existing `OA.ko`-side reconstruction gap, not something
introduced by this module, and not in this module's own scope to fix.

## Exported symbols (26, exactly the set `OA.ko`'s own `nm -u` needs)

| Symbol | Real ABI | Substitute |
|---|---|---|
| `rt_task_init` | regparm(3) | `kthread_create()`, parked until resumed |
| `rt_task_delete` | regparm(3) | `kthread_stop()` + free private state |
| `rt_task_resume` | regparm(0) | clear suspend flag + `wake_up_process()` |
| `rt_task_suspend` | regparm(0) | set suspend flag (best-effort, see above) |
| `rt_whoami` | regparm(3) | static "Linux context" sentinel (+0x1c = `RT_SCHED_LINUX_PRIORITY`) |
| `rt_set_runnable_on_cpuid` | regparm(0) | `set_cpus_allowed_ptr()` on the kthread |
| `clear_debug_traps_in_rt_task` | regparm(3) | no-op (no debug-trap facility modeled) |
| `rtheap_alloc` | regparm(3) | `kzalloc(size, GFP_KERNEL)` |
| `rtheap_free` | regparm(3) | `kfree()` |
| `rtai_global_heap` | data symbol | 4-byte opaque placeholder, address-only use |
| `rt_typed_sem_init` | regparm(0) | placement `sema_init()` (see mutex-mode note above) |
| `rt_sem_wait` | regparm(0) | `down()` |
| `rt_sem_wait_if` | regparm(0) | `down_trylock()`, polarity-translated |
| `rt_sem_signal` | regparm(0) | `up()` |
| `rt_sem_delete` | regparm(0) | no-op (nothing dynamically allocated) |
| `rt_request_srq` | regparm(3) | allocate a workqueue-backed slot, 1-based |
| `rt_free_srq` | regparm(3) | `cancel_work_sync()` + free the slot |
| `rt_pend_linux_srq` | regparm(3) | `queue_work()` on a dedicated workqueue |
| `rtf_create` | regparm(0) | `kzalloc()`'d ring buffer per minor |
| `rtf_destroy` | regparm(0) | free the ring buffer, safe on an unknown minor |
| `rtf_put_if` | regparm(0) | non-blocking, all-or-nothing ring-buffer write |
| `rt_assign_irq_to_cpu` | regparm(3) | no-op, returns 0 |
| `rt_release_irq` | regparm(3) | no-op, returns 0 |
| `rt_shutdown_irq` | regparm(3) | no-op, returns 0 |
| `rt_startup_irq` | regparm(3) | no-op, returns 0 |
| `rt_printk` | regparm(0) | forwards to `vprintk()` |

Every regparm split above is taken directly from ground-truth-confirmed
call-site disassembly already recorded in this project's own
`reconstructed/OA/src/init/rtwrap.cpp` and
`reconstructed/OA/include/oa_rtfifo_init.h` header comments — not
re-derived or guessed here.

**Plus 3 bonus exports, outside the official 26**: `rt_linux_use_fpu`,
`rt_set_oneshot_mode`, `start_rt_timer` (all regparm(3), all no-ops) --
not needed by `OA.ko` itself, but needed by `STGEnabler.ko`'s own
`init_module()` in this configuration. See "Module load order" below.

## Verification

- Clean Kbuild compile against `/home/build/linux-kronos`.
- `nm RTAIVirtualDriver.ko`: all 26 symbols confirmed as real `T`/`D`
  symbols with genuine `__ksymtab_`/`__kstrtab_` pairs (not just
  references), and no unresolved externals beyond genuine kernel
  primitives (`kzalloc`/`kfree`/`kthread_create`/`wake_up_process`/
  `kthread_stop`/`printk`/`vprintk`/`set_cpus_allowed_ptr`/workqueue and
  semaphore primitives/`cpu_online`).
- **No host KAT.** Unlike `AT88VirtualChip`/`KorgUsbAudioVirtualDriver`,
  this module's own logic is inseparable from real Linux kthread/
  semaphore/workqueue primitives that only exist in a real kernel build —
  there is no freestanding, hardware-boundary-mockable algorithmic core to
  test in isolation the way chip-state/bignum arithmetic does. A clean
  Kbuild compile + `nm` export verification + the live VM boot test below
  are this module's own verification bar, matching the task's own explicit
  allowance ("a host KAT may not be practical for kernel-thread-based
  logic; use your judgment").
- **Live VM boot test**: see `MASTER_REFERENCE.md`'s own dated section for
  this batch's actual boot-test transcript and result.

## BUG FIX, 2026-07-19: `set_debug_traps_in_rt_task()` was `void` with an empty body — a real ABI mismatch that caused an intermittent (~40-45%) `OmapNKS4Module.ko` `vm_virtual_probe` boot hang

Root-caused and fixed from the `OmapNKS4Module.ko` side (see that module's own
README.md, "Hang ROOT-CAUSED AND FIXED, 2026-07-19" section, for the full
live-evidence trail — this note is the short version, from this module's own
perspective).

`set_debug_traps_in_rt_task()` was declared `void` with a completely empty
body — a function that never writes `EAX` at all. `OmapNKS4Module.ko`'s own
`rtwrap.cpp`, however, declares this exact exported symbol as `int
set_debug_traps_in_rt_task(void)` and genuinely branches on the returned
value (`CSTGThread::CreateRealTimeWithCPUAffinity`, confirmed load-bearing
against real ground-truth disassembly) — a real, silent signature mismatch
across this module's own symbol-export boundary, not merely a documentation
gap. Every call returned whatever stale value happened to be in `EAX` at the
call site (a deterministic-but-meaningless **71** on every observed boot,
confirmed live), which meant the caller's "debug-trap setup failed, roll the
task back" branch fired on **100% of calls**, not just the fraction that
went on to actually hang. The rollback path (`rtwrap_pthread_cancel()` ->
`rt_task_delete()` -> this module's own `kthread_stop()`) is not safe to
take unconditionally on a task whose entry point is an intentional infinite
RTAI-style service loop with no `kthread_should_stop()` check (real RTAI
tasks are torn down by forcible deletion, not cooperative return) — whether
`kthread_stop()` then blocks forever depends entirely on a scheduling race
against the kthread having already reached its `entry()` call, which is
exactly the ~40-45% intermittency that was observed.

**Fix**: `set_debug_traps_in_rt_task()` is now `int`, explicitly `return 0;`
(success) — this module never installed any hardware debug trap in the
first place, so "success" is the only meaningful answer, and it matches
what the real ground-truth caller-side disassembly already established this
return value means. Confirmed via 19 consecutive clean `vm_virtual_probe`
boot passes on `kronosvm` post-fix (0 hangs), against a measured ~48%
combined pre-fix hang rate across 23 runs spanning two sessions — see
`OmapNKS4Module/README.md` for the full run-by-run numbers.

**What this does NOT fix**: `rt_task_delete()`'s reliance on `kthread_stop()`
genuinely cannot forcibly cancel a kthread already executing an entry
function with no stop-check, unlike real RTAI's own forcible task deletion —
this remains a real, structural gap in this substitute (already partially
documented by `rt_task_suspend()`'s own "best-effort only" comment above) for
any future caller that legitimately needs to cancel an already-running RT
task. No confirmed caller in this project currently does (the one call site
that reached it was this exact bug, now unreachable since the spurious
rollback trigger is gone) — flagged here rather than spending this pass's
scope on a speculative rewrite of `rt_task_delete()`'s own cancellation
semantics.

## Module load order (VM boot test only — do NOT use this order on real hardware)

This module is a **substitute for the entire RTAI load-order step**, not an
addition to it. Real hardware must still load the genuine
`rtai_hal.ko`/`rtai_sched.ko` (or `rtai_smp.ko`)/`rtai_sem.ko`/
`rtai_ndbg.ko`/`rtai_fifos.ko` stack exactly as before —
`RTAIVirtualDriver.ko` must never be loaded alongside or in place of it
there (this module makes no attempt at real hard-RT scheduling and would
silently defeat the whole point of RTAI on hardware that can actually
achieve it).

For a **VM/QEMU-TCG boot test only**, `RTAIVirtualDriver.ko` must load
**FIRST**, before `STGEnabler.ko`:

```
RTAIVirtualDriver.ko  ->  STGEnabler.ko  ->  STGGmp.ko  ->
AT88VirtualChip.ko  ->  OmapNKS4VirtualDriver.ko  ->
KorgUsbAudioVirtualDriver.ko  ->  OA.ko
```

Stating the "before STGEnabler.ko" part explicitly matters: it is
load-bearing, not just a style preference. `STGEnabler.ko`'s own
`init_module()` (`STGEnabler_init()`,
`reconstructed/STGEnabler/STGEnabler.c`) unconditionally calls
`stg_rtai_setup()`, which calls `rt_linux_use_fpu`/`rt_set_oneshot_mode`/
`start_rt_timer` directly at insmod time. Those three are not among
`OA.ko`'s own 26 target symbols (confirmed absent from `OA.ko`'s own
`nm -u` -- `STGEnabler.ko` is the only module that calls them directly),
so they were not originally in this module's scope -- but without them,
`STGEnabler.ko` itself fails outright at `insmod` in a configuration with
no real RTAI loaded, which cascades into every downstream companion
module that depends on `STGEnabler.ko`'s own `stg_*` exports
(`stg_set_cpus_allowed`, etc.), and ultimately into `OA.ko`. Bonus no-op
substitutes for these three are therefore included here too (see
`RTAIVirtualDriver.c` section 8) -- the only way to make `STGEnabler.ko`
itself insmod-clean without any real RTAI module loaded. On real hardware
this dependency is a non-issue (`STGEnabler.ko` always loads after the
real, genuine RTAI stack there).
