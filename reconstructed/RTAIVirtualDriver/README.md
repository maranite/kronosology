# RTAIVirtualDriver.ko - software substitute for the real RTAI stack

Reconstructed-project-original source (not decompiled from a shipping
binary - this subsystem has no shipping-binary counterpart). Target:
Linux 2.6.32.11-korg + RTAI headers (for type/declaration compatibility
only - this module does not actually depend on RTAI being loaded), x86-32,
`gcc -mregparm=3`.

## What this replaces, and why

`OA.ko`'s own reconstructed `rtwrap_*` wrapper layer
(`reconstructed/OA/src/init/rtwrap.cpp`), daemon lifecycle
(`src/init/daemon_lifecycle.cpp`, `src/init/stg_daemons.cpp`) and RTAI-FIFO
layer (`src/init/rtfifo_init.cpp`, `src/engine/push_unsolicited_message.cpp`)
call 26 real RTAI symbols directly, resolved at `insmod` time against the
real `rtai_hal.ko`/`rtai_sched.ko`/`rtai_sem.ko`/`rtai_fifos.ko`/
`rtai_ndbg.ko`/`rtai_debug.ko` stack on real hardware.

Genuine RTAI depends on taking over the hardware timer and running an
I-pipe interrupt domain beneath the Linux kernel. That requirement makes it
impractical to bring up outside real hardware, independent of anything in
this project's own code - the relevant failures are inside stock RTAI's own
hardware-timer calibration and scheduler-init paths, not fixable at this
project's layer.

This module is a from-scratch substitute for the 26 real RTAI symbols
`OA.ko`'s own reconstructed callers actually invoke - architecturally the
same pattern as this project's other hardware virtual drivers:

| Virtual driver | Stands in for |
|---|---|
| `AT88VirtualChip.ko` | `OmapNKS4Module.ko`'s AT88SC/NV2AC chip access |
| `OmapNKS4VirtualDriver.ko` | `OmapNKS4Module.ko`'s 9 other exports |
| `KorgUsbAudioVirtualDriver.ko` | `KorgUsbAudioDriver.ko`'s USB audio/MIDI codec access |
| `RTAIVirtualDriver.ko` | the real RTAI stack itself (`rtai_hal.ko`/`rtai_sched.ko`/`rtai_sem.ko`/`rtai_fifos.ko`/`rtai_ndbg.ko`/`rtai_debug.ko`) |

Because this module deliberately never attempts genuine hard-real-time
scheduling - no I-pipe domain registration, no hardware-timer takeover, no
APIC reprogramming - it sidesteps the timing hazards that make real RTAI
unreliable to bring up off real hardware, rather than working around them.
Every exported symbol is implemented purely with ordinary Linux kernel
primitives that behave identically regardless of the host environment.

In addition to `OA.ko`'s 26 symbols, this module also exports the further
real RTAI symbols `OmapNKS4Module.ko`'s own `rtwrap.cpp` needs:
`rt_cond_wait`/`_timed`/`_until`, `rt_sem_broadcast`,
`rt_sem_wait_timed`/`_barrier`, `rt_sched_lock`/`unlock`,
`rt_get_priorities`/`_time_cpuid`, `nano2count`/`_cpuid`, `rt_sleep`,
`set_debug_traps_in_rt_task`, `rt_task_masked_unblock`, plus the
`_nano2count_cpuid`/`rtai_cpu_lock` per-CPU data symbols. These follow the
same discipline as the original 26: each substitute matches the caller's
currently-declared calling convention, and anything not independently
confirmed against ground-truth disassembly is flagged as such.

## What guarantee this deliberately does NOT provide

Genuine hard-real-time scheduling latency/determinism. Specifically:

- A "task" here is an ordinary preemptible Linux kthread, not a hard-RT
  thread running outside the Linux scheduler's normal jitter.
- `rt_task_suspend()`/`rt_task_resume()` can only meaningfully gate a task
  **before** it starts running its entry function - a plain kthread has no
  external-preemption primitive equivalent to real RTAI's, so this
  substitute cannot forcibly interrupt an entry() function already mid-run
  from the outside. See the function's own header comment in the source.
- `rt_typed_sem_init()`'s real RTAI "mutex mode" (recursive/errorcheck
  encoding, packed into the `value` argument for type==3 resource
  semaphores) is not reproduced - every type==3 semaphore simply starts
  available, matching real RTAI's own "freshly-initialized resource
  semaphore is unlocked" behavior, without the recursive/errorcheck
  behavioral nuance on lock.
- `rt_request_srq`/`rt_pend_linux_srq` defer to a workqueue unconditionally
  (there is no genuine hard/soft context split left to preserve, since no
  hard-RT context ever exists in this substitute) rather than the real
  SRQ mechanism's own hard-RT-to-soft-IRQ signaling path.
- The IRQ-affinity family (`rt_assign_irq_to_cpu`/`rt_release_irq`/
  `rt_shutdown_irq`/`rt_startup_irq`) are pure no-ops - there is no real
  hardware IRQ routing to manage.

None of this matters for this module's actual purpose: real-time
audio/DSP fidelity is out of scope for this project's software-only
testing effort. This module's only job is to let `OA.ko`'s own structural
init/daemon-lifecycle logic run to completion (or fail on its own real
logic, not on missing RTAI symbols) in an environment where genuine hard-RT
was never achievable anyway.

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

Every regparm split above is taken directly from ground-truth call-site
disassembly recorded in this project's own
`reconstructed/OA/src/init/rtwrap.cpp` and
`reconstructed/OA/include/oa_rtfifo_init.h` header comments, not re-derived
or guessed here.

**Plus 3 bonus exports, outside the official 26**: `rt_linux_use_fpu`,
`rt_set_oneshot_mode`, `start_rt_timer` (all regparm(3), all no-ops) - not
needed by `OA.ko` itself, but needed by `STGEnabler.ko`'s own
`init_module()` in this configuration. See "Module load order" below.

## Implementation notes

**`set_debug_traps_in_rt_task()` returns `int`, always `0`.** Real callers
(`OmapNKS4Module.ko`'s own `rtwrap.cpp`, `CSTGThread::CreateRealTimeWithCPUAffinity`)
declare this exported symbol as `int set_debug_traps_in_rt_task(void)` and
branch on the returned value. Declaring it `void` with an empty body is a
latent ABI mismatch: on x86-32 a function that never writes `EAX` leaves
the caller reading whatever value happened to be left there by an earlier
call, so a caller that checks the return value can observe a
deterministic-but-meaningless nonzero result and treat it as failure. Since
this module never installs a hardware debug trap in the first place,
returning `0` (success) unconditionally is both correct and the only
meaningful answer, and matches what the real ground-truth caller-side
disassembly establishes this return value means. A caller-side failure
branch here is not safe to trigger unconditionally: if it tears down the
task via `rtwrap_pthread_cancel()` -> `rt_task_delete()` -> this module's
own `kthread_stop()`, that path is unsafe against a task whose entry point
is an intentional infinite RTAI-style service loop with no
`kthread_should_stop()` check (real RTAI tasks are torn down by forcible
deletion, not cooperative return), and whether `kthread_stop()` then blocks
forever depends on a scheduling race against the kthread having already
reached its `entry()` call.

**`rt_sleep()` uses `do_div()`, not plain 64-bit division.** Any RTAI
substitute that divides a runtime `long long` value must go through the
kernel's own `do_div()` macro rather than plain C `/`, since plain 64-bit
division on a 32-bit kernel target needs GCC's `__divdi3` helper, which is
unavailable in kernel space and produces an unresolved-symbol failure at
load time.

## Building and testing

Build via Kbuild against a properly configured Linux 2.6.32.11-korg kernel
source tree whose module ABI (struct layouts, the `-mregparm=3` calling
convention, RTAI header compatibility) matches the target kernel build - a
generic, unpatched 2.6.32.11 tree produces a module with the wrong
`vermagic`/struct layout and will not load.

Verification consists of:

- A clean Kbuild compile.
- `nm RTAIVirtualDriver.ko`: all 26 symbols present as real `T`/`D` symbols
  with genuine `__ksymtab_`/`__kstrtab_` pairs (not just references), and
  no unresolved externals beyond genuine kernel primitives
  (`kzalloc`/`kfree`/`kthread_create`/`wake_up_process`/`kthread_stop`/
  `printk`/`vprintk`/`set_cpus_allowed_ptr`/workqueue and semaphore
  primitives/`cpu_online`).
- A live module load exercising `OA.ko`'s own init/daemon-lifecycle path
  against these substitute symbols.

There is no host-side known-answer test for this module. Unlike
`AT88VirtualChip`/`KorgUsbAudioVirtualDriver`, this module's logic is
inseparable from real Linux kthread/semaphore/workqueue primitives that
only exist in a real kernel build - there is no freestanding,
hardware-boundary-mockable algorithmic core to test in isolation the way
chip-state/bignum arithmetic does. A clean Kbuild compile, `nm` export
verification, and a live boot test are this module's verification bar.

## Module load order (test-only load order; real hardware never uses it)

This module is a substitute for the entire RTAI load-order step, not an
addition to it. Real hardware must still load the genuine
`rtai_hal.ko`/`rtai_sched.ko` (or `rtai_smp.ko`)/`rtai_sem.ko`/
`rtai_ndbg.ko`/`rtai_fifos.ko` stack exactly as before -
`RTAIVirtualDriver.ko` must never be loaded alongside or in place of it
there (this module makes no attempt at real hard-RT scheduling and would
silently defeat the whole point of RTAI on hardware that can actually
achieve it).

Where genuine RTAI is not available, `RTAIVirtualDriver.ko` must load
first, before `STGEnabler.ko`:

```
RTAIVirtualDriver.ko -> STGEnabler.ko -> STGGmp.ko ->
AT88VirtualChip.ko -> OmapNKS4VirtualDriver.ko ->
KorgUsbAudioVirtualDriver.ko -> OA.ko
```

The "before `STGEnabler.ko`" ordering is load-bearing, not a style
preference. `STGEnabler.ko`'s own `init_module()` (`STGEnabler_init()`,
`reconstructed/STGEnabler/STGEnabler.c`) unconditionally calls
`stg_rtai_setup()`, which calls `rt_linux_use_fpu`/`rt_set_oneshot_mode`/
`start_rt_timer` directly at insmod time. Those three are not among
`OA.ko`'s own 26 target symbols (absent from `OA.ko`'s own `nm -u` -
`STGEnabler.ko` is the only module that calls them directly), so
they were not originally in this module's scope - but without them,
`STGEnabler.ko` itself fails outright at `insmod` when no real RTAI is
loaded, which cascades into every downstream companion module that depends
on `STGEnabler.ko`'s own `stg_*` exports (`stg_set_cpus_allowed`, etc.),
and ultimately into `OA.ko`. The bonus no-op substitutes for these three
symbols exist for exactly this reason - the only way to make
`STGEnabler.ko` itself insmod-clean without any real RTAI module loaded.
On real hardware this dependency is a non-issue, since `STGEnabler.ko`
always loads after the real, genuine RTAI stack there.

## Known limitations

- **`rt_task_suspend()`/`rt_task_resume()` cannot forcibly preempt a
  running task.** They only gate a kthread before it starts executing its
  entry function; there is no plain-kthread equivalent of RTAI's external
  preemption. No confirmed caller in this project needs mid-run
  suspend/resume today. *To validate:* if a future caller relies on
  suspending an already-running task, this substitute will not behave
  correctly for it and needs a different mechanism (e.g. a cooperative
  check inside the entry loop).
- **`rt_task_delete()` cannot forcibly cancel a kthread already executing
  an entry function with no stop-check**, unlike real RTAI's own forcible
  task deletion. This is a real, structural gap for any future caller that
  legitimately needs to cancel an already-running RT task; the one call
  site that previously reached this path did so only because of the
  `set_debug_traps_in_rt_task()` ABI bug described above, which is now
  fixed, so the path is not currently exercised by a live bug. *To
  validate:* if a new caller needs genuine forcible cancellation, this
  function's cancellation semantics need a rewrite (e.g. a cooperative
  stop flag checked inside every RTAI-style service loop this module's
  tasks run).
- **`rtwrap_pthread_create()`'s RT-task entry point is a hardcoded address
  this module cannot make valid.** `reconstructed/OA/src/init/rtwrap.cpp`
  passes `rt_task_init()` a hardcoded address constant
  (`RTWRAP_THREAD_TRAMPOLINE = (void *)0x118e80`) as the RT task's entry
  point - an intentionally-unreconstructed internal `OA.ko` trampoline
  function, modeled only as an opaque address because nothing in that file
  calls it directly (it is only ever passed through to `rt_task_init`).
  This module's `rt_task_init()` faithfully does what real RTAI would do
  with whatever entry pointer it is given - spawn a kthread that eventually
  calls it - but it cannot make that specific hardcoded address a valid
  function in a freshly-linked `OA.ko` build, since that build's own
  `.text` layout necessarily differs from the original binary the constant
  was extracted from. Any run that reaches the point where a daemon's
  kernel thread is resumed and jumps to that address is expected to fault
  there. This is a pre-existing `OA.ko`-side reconstruction gap, not
  something introduced by this module, and not this module's own scope to
  fix. *To validate:* reconstruct the real trampoline function's code from
  `OA.ko`'s own disassembly at that address, or patch the address at load
  time to point at an equivalent reconstructed function once one exists.
