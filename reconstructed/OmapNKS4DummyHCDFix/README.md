# OmapNKS4DummyHCDFix — standalone `dummy_hcd_fixed.ko`

## Status (updated 2026-07-20, eleventh pass): the tenth pass's config-
## descriptor bug is fixed and confirmed — 3/3 clean boots with the
## descriptor warnings completely gone. But this was NOT the last blocker:
## `OmapNKS4Probe()` still times out identically in all 3 runs, meaning
## something else, not yet identified, still prevents the real driver from
## being matched/attached even though the device now enumerates with a
## fully correct descriptor set. See "Eleventh pass" section (bottom).
## Everything below up through "Tenth pass" is prior history, kept verbatim.
## Everything below this point, up to "Seventh pass", is the sixth pass's own
## original writeup — kept verbatim for the historical trail; its own
## `timer_active` guard fix is superseded (real live evidence now shows the
## crash is the urb's *first-ever* `dummy_timer()` call, not a second,
## overlapping one, so that guard's own hazard was real but not this bug).

This directory contains a standalone, out-of-tree fork of real, unmodified
`drivers/usb/gadget/dummy_hcd.c` (from `/home/build/linux-kronos`, "02 May
2005" vintage, Alan Stern/David Brownell, GPL) with a targeted attempt at
fixing the `dummy_timer()` NULL-pointer crash that has blocked
`OmapNKS4VirtualBoard/` since its "third pass" (see that directory's own
README.md). **The fix implemented here does not resolve the crash** — this
was proven with real, repeated live-boot testing this session, not assumed.
This README documents exactly what was tried, what was found, and what is
now known that wasn't before, honestly, per this project's own established
convention of not claiming success without repeated-boot evidence.

Read `OmapNKS4VirtualBoard/README.md`'s "third pass" through "fifth pass"
sections first for full context — this picks up directly from the fifth
pass's own closing note ("What remains open... the fourth pass's own path
(a) — a live `dummy_timer` breakpoint... is still unresolved").

## What's in this directory

- `dummy_hcd_fixed.c` — the fork. Byte-identical to real `dummy_hcd.c`
  except for: (1) `#include "hcd.h"` instead of `#include "../core/hcd.h"`
  (local copy, see below), (2) driver/gadget name string renames
  (`"dummy_hcd"`→`"dummy_hcd_fixed"`, `"dummy_udc"`→`"dummy_udc_fixed"`,
  `MODULE_DESCRIPTION`/`MODULE_AUTHOR` updated) so this module is
  identifiable in dmesg/sysfs, and (3) the actual fix: a `dum->timer_active`
  non-reentrancy guard around `dummy_timer()`'s own body. All three kinds of
  change are marked inline with `KRONOS FORK` / `KRONOS I-PIPE REENTRANCY
  FIX` comments. See the file's own header comment for the full writeup.
- `hcd.h`, `hub.h` — verbatim, unmodified read-only copies of
  `/home/build/linux-kronos/drivers/usb/core/{hcd.h,hub.h}` (md5-confirmed
  identical to the tree's own copies as of this writing — see "scope
  discipline" below). Real `dummy_hcd.c` does `#include "../core/hcd.h"`,
  a same-source-tree relative include that only works building in-tree;
  out-of-tree here, that becomes a same-directory include of these copies.
  `hcd.h` itself pulls in `hub.h`; nothing else was needed (confirmed via
  `nm -u` on the built module — see "Build" below — the undefined-symbol
  list is exactly the ordinary kernel/USB-core exports a real `dummy_hcd.ko`
  needs, nothing unexpected).
- `Makefile` — same `KDIR=`/`obj-m` convention as every other module in this
  project.

## Scope discipline: `/home/build/linux-kronos` was never modified

Verified explicitly, not assumed:
```
$ md5sum /home/build/linux-kronos/drivers/usb/core/hcd.h /home/build/linux-kronos/drivers/usb/core/hub.h
9ea75dfad034553596df1dde4b7c1023  hcd.h
2daf13db17f3ad495da7aabea06cd014  hub.h
$ md5sum OmapNKS4DummyHCDFix/hcd.h OmapNKS4DummyHCDFix/hub.h
9ea75dfad034553596df1dde4b7c1023  hcd.h
2daf13db17f3ad495da7aabea06cd014  hub.h
```
Identical — these are read-only reference copies, never edited in place.
`drivers/usb/gadget/dummy_hcd.c` in the shared tree was also never touched
(only read, to seed this directory's own `dummy_hcd_fixed.c`); the shared
tree has no VCS (`git status` reports "not a git repository"), so this was
confirmed by direct `md5sum` comparison against a known-good reference
rather than `git diff`.

## Naming / coexistence note

`driver_name`/`gadget_name`/`driver_desc`/`MODULE_DESCRIPTION` were renamed
so this module is visibly distinct from a real `dummy_hcd.ko` in dmesg and
`/proc/modules`. This does **not** achieve full symbol-level coexistence:
like the real driver, this fork `EXPORT_SYMBOL()`s `usb_gadget_register_driver`/
`usb_gadget_unregister_driver` under those exact (unrenameable — callers
like `OmapNKS4VirtualBoard.ko` depend on the name) global symbol names, so
`insmod`ing both `dummy_hcd.ko` and `dummy_hcd_fixed.ko` at the same time
would still fail on a duplicate-symbol error. In practice only one is ever
loaded — this fork is meant to fully replace `dummy_hcd.ko` for this
project's own testing, not run alongside it.

## Build

```
cd OmapNKS4DummyHCDFix
make KDIR=/home/build/linux-kronos
```
Builds clean: zero new warnings beyond the same pre-existing
section-mismatch/`init_module`-`cold`-attribute/percpu-unused-variable noise
every other module in this project already has (confirmed by diffing this
build's warning output against a same-host build of unmodified
`OmapNKS4VirtualBoard.c`). `nm -u dummy_hcd_fixed.ko` shows only ordinary
kernel/USB-core symbols (`_spin_lock_irqsave`, `usb_hcd_giveback_urb`,
`usb_create_hcd`, `platform_driver_register`, etc.) plus
`__ipipe_restore_root` (pulled in transitively by this kernel's own
I-pipe-aware inline spinlock primitives — expected, and itself confirms
this kernel's `spin_unlock_irqrestore()` really does route through I-pipe's
own unstall path, consistent with the root-cause mechanism below).

## The fix implemented (real, builds clean, does NOT resolve the crash)

**Mechanism believed responsible going in** (established by
`OmapNKS4VirtualBoard/README.md`'s fourth/fifth passes): this kernel's
I-pipe (Adeos/RTAI-patch) interrupt model lets `spin_unlock_irqrestore()`
**synchronously** replay a queued interrupt (and everything it raises,
including the timer softirq that drives `dummy_timer()`) on the unlocking
function's own call stack, before that function returns to its own caller —
something vanilla Linux (which real `dummy_hcd.c` was written for in 2005)
never does. The fourth pass's own oops evidence caught this exact sequence:
`dummy_urb_enqueue()`'s closing `spin_unlock_irqrestore()` →
`__ipipe_unstall_root()` → `dummy_timer()`, nested, on `khubd`'s own stack.

**Fix applied**: `struct dummy` gained a `timer_active:1` bitfield, checked
and set under `dum->lock` at `dummy_timer()`'s own entry (atomically with
the lock, so a genuinely nested/reentrant call — whichever unstall point
permits it — sees the flag already set and safely bails out instead of
running its own `list_for_each_entry_safe()` over `dum->urbp_list`
concurrently with an outer, still-in-progress invocation), cleared on every
one of `dummy_timer()`'s three exit paths. This closes the "two overlapping
executions of `dummy_timer()`'s own list walk against the same `dum`"
hazard directly, without needing to enumerate every exact I-pipe unstall
point that could trigger it.

**Verified NOT to fix the crash.** Built, deployed, boot-tested five times
in a row (`omapnks4vb_dummyhcdfix_boottest_20260719/` on `kronosvm`, runs
1-5, `run_one_boot.sh` harness — fresh disk-image copy per run, real
`qemu-system-i386 -no-reboot`, waited for either `"fixtest chain done"` or
`"Kernel panic"` in the console log, not assumed): **5/5 runs crash**,
identical signature every time:
```
usb 1-1: new full speed USB device number 2 using dummy_hcd_fixed
usb 1-1: new full speed USB device number 3 using dummy_hcd_fixed
usb 1-1: new full speed USB device number 4 using dummy_hcd_fixed
BUG: unable to handle kernel NULL pointer dereference at (null)
IP: [<f819cd9f>] dummy_timer+0x58f/0xa9c [dummy_hcd_fixed]
```
Same instruction (`dummy_timer+0x58f`, the offset shifted from the
unfixed build's `+0x56f`/`+0x58f` purely because the added guard code
changed the function's own size — same relative source line, the
`setup = *(struct usb_ctrlrequest*) urb->setup_packet;` read), same
"device number 2/3/4" churn immediately before, same
`EDX: 00000000`/`CR2: 0000000000000000` NULL-setup_packet signature, in
every one of the 5 runs. **This empirically rules out "`dummy_timer()`
racing a nested invocation of itself" as the sole root cause** — the guard
correctly closes that specific hazard (confirmed: it builds, it's on the
right code path, `__ipipe_restore_root` in the symbol table confirms the
lock primitives really are I-pipe-aware) but the crash reproduces
identically regardless.

## Live GDB evidence gathered this pass (real, not assumed)

Per the fourth pass's own explicit open question ("path (a)... a live
`dummy_timer` breakpoint... to settle whether the NULL `setup_packet`
belongs to a fresh urb or a stale, already-freed one"), this pass did that
investigation, on **both** the unfixed real `dummy_hcd.ko` and this fork,
using the already-extracted symbolicated kernel
(`omapnks4_dummyhcd_abi_20260719/extracted/vmlinux_symbolicated.elf` on
`kronosvm`) and `gdb -q -batch -ex 'target remote localhost:PORT' -ex
'add-symbol-file .../dummy_hcd*.ko 0xf819b000' ...` against a genuinely
crashed (panicked, `-no-reboot`, memory still live) QEMU instance — not a
breakpoint-and-single-step, but direct post-mortem memory inspection of the
frozen guest, which is equally valid since `-no-reboot` halts the CPU
without unmapping anything.

**Finding 1 — the `urbp` (dummy_hcd's own internal `struct urbp { struct
urb *urb; struct list_head urbp_list; }`, reached via `urb->hcpriv`) is
self-consistent, not corrupted.** `urbp->urb` correctly points back to the
exact `urb` address being processed, and `urbp->urbp_list` holds
plausible-looking, non-self-referential list pointers (i.e. genuinely
linked into a real list, not an obviously-freed/poisoned node). This
**contradicts** a first-pass reading (recorded, then corrected, in this
session's own working notes) that mis-read 64 bytes starting at the
`urbp`'s own address — `struct urbp` is only 12 bytes, so words past the
first 3 belong to a **different, adjacent** kmalloc'd object entirely
(coincidentally a literal `"smsc7500"` module-name string, harmless
same-slab-page neighbor data, not `urbp`'s own content). Lesson for anyone
continuing this: bound every `struct` memory read to `sizeof()`, this
session initially didn't and drew a wrong conclusion from it before
catching the mistake via `struct urbp`'s own definition.

**Finding 2 — the `urb` struct's own fields are internally inconsistent in
a way real `usb_alloc_urb()`/`usb_fill_control_urb()` should never
produce**, confirmed identically across two independent boot instances
(different physical addresses, same relative content — expected under this
harness's own deterministic TCG execution, not itself diagnostic either
way):
```
kref=2 hcpriv=<valid urbp, see Finding 1> use_count=1 reject=0 unlinked=0
urb_list={linked}  anchor_list={self, i.e. genuinely empty}  anchor=NULL
dev=<plausible kernel ptr>  ep=<plausible kernel ptr>  pipe=0x80000080
  (decodes cleanly: PIPE_CONTROL, USB_DIR_IN, devnum=0, ep=0 -- exactly a
  device-descriptor GET_DESCRIPTOR-at-address-0 request, hub_port_init's
  very first enumeration step -- not garbage)
status=0
transfer_flags=0xffffff8d   <-- == -EINPROGRESS as a signed dword, and/or
                                 simply not the 0 usb_init_urb()'s own
                                 memset(urb,0,sizeof(*urb)) guarantees
transfer_buffer=0x00000200  <-- not a plausible kernel pointer
transfer_dma=<plausible-looking ptr, but dummy_hcd sets neither
              HCD_LOCAL_MEM nor uses_dma, so this should be untouched/0>
actual_length=8              <-- plausible (first GET_DESCRIPTOR read is 8B)
setup_packet=NULL             <-- the crash
setup_dma=<plausible-looking ptr, same "should be unused" concern>
context=NULL                  <-- NOT actually anomalous, see below
```
`usb_init_urb()` (`drivers/usb/core/urb.c`, called by every
`usb_alloc_urb()`) does `memset(urb, 0, sizeof(*urb))` before anything else
touches the urb, and `usb_fill_control_urb()` (called by
`usb_internal_control_msg()`, the real function behind `usb_control_msg()`,
used by `hub_port_init()`'s own descriptor reads) explicitly sets `dev`,
`pipe`, `setup_packet`, `transfer_buffer`, `transfer_buffer_length`,
`complete`, and `context` — meaning **`transfer_flags` should read exactly
0** (never explicitly set, so it should keep its post-memset value) and
**`setup_packet` should never be NULL** (always explicitly assigned before
submission) for a genuine, live, in-flight request from this call path. The
`context=NULL` reading, initially flagged as anomalous in this session's
own early notes, is **not** actually a red flag: `usb_internal_control_msg()`
calls `usb_fill_control_urb(..., usb_api_blocking_completion, NULL)` with a
literal `NULL` context argument — `usb_start_wait_urb()` (the actual caller
one level up) separately does `urb->context = &ctx` afterward, so `NULL`
briefly is a real, expected transient state, not corruption. Struck from
the evidence list after checking `drivers/usb/core/message.c` directly.

**Working conclusion, not fully proven this pass**: the `urb`'s own
backing memory is most consistent with having been genuinely freed and its
slab slot reused/overwritten for unrelated content at some point between
being correctly filled in and `dummy_timer()` reading it here — a real,
`urb`-level (not `urbp`-level — see Finding 1) use-after-free — while the
`urbp` referencing it via `dum->urbp_list`/`hcpriv` was never itself
invalidated or removed, i.e. `dummy_hcd`'s own bookkeeping never learned the
urb it's still tracking is gone. **The exact trigger was not pinned down
this pass** — see "Ruled out" and "Leading hypothesis for whoever continues"
below.

## Ruled out this pass (with real evidence, not assumption)

- **`dummy_timer()` racing a nested invocation of itself** (this pass's own
  leading hypothesis going in, per the fourth/fifth passes' own framing):
  disproven by direct fix-and-retest — see "Verified NOT to fix the crash"
  above. The guard is real and correctly placed; the crash is unaffected.
- **`usb_kill_urb()`'s timeout path freeing the urb before `dummy_timer()`
  processes it**: structurally unlikely on inspection of
  `drivers/usb/core/urb.c` — `usb_kill_urb()` calls
  `wait_event(usb_kill_urb_queue, atomic_read(&urb->use_count) == 0)`,
  which blocks (real scheduling, not I-pipe-synchronous) until
  `usb_hcd_giveback_urb()` (called from `dummy_timer()`'s own
  `return_urb:` path) has actually run and decremented `use_count` — this
  should hold even under the I-pipe reentrancy quirk, since `wait_event`
  doesn't depend on synchronous-vs-asynchronous interrupt delivery, only on
  eventually being woken. Not fully proven safe (not stress-tested
  directly), but no evidence found against it either; deprioritized as the
  likely mechanism.

## Leading hypothesis for whoever continues (not proven, real reasoning trail)

The "device number 2, 3, 4" rapid churn — present in every single boot, fix
or no fix, and independently flagged as suspicious by the third, fourth,
and fifth passes — is consistent with real `hub_port_init()`'s own
documented retry behavior (real Linux hub.c retries device-descriptor reads
with a fresh `struct usb_device`/device number on failure). Combined with
`dummy_urb_dequeue()`'s own design (it does **not** itself remove the
corresponding `urbp` from `dum->urbp_list` or call `usb_hcd_giveback_urb()`
— it only marks the urb via `usb_hcd_check_unlink_urb()` and re-arms
`dum->timer` for `dummy_timer()` to do the actual cleanup **later, on its
own next tick**): if device #2's own urb submission is where the I-pipe
reentrancy first corrupts something (not necessarily a crash yet — could be
subtler), `hub_port_init()` may time out and move on to device #3/#4 while
device #2's own `urbp` is still sitting, uncleaned, in `dum->urbp_list`.
Whether `usb_start_wait_urb()`'s own `usb_free_urb(urb)` can, under some
sequencing this pass didn't isolate, run before `dummy_timer()` actually
reaches and removes that specific `urbp` — genuinely leaving a dangling
`urbp`→freed-`urb` link for a **later**, unrelated `dummy_timer()` tick
(e.g. the ordinary idle-task periodic one, `Pid: 0, comm: swapper`, which
is what both this pass's and the fifth pass's own crash instances actually
show) to walk into — is the next thing to check. A live breakpoint at
`dummy_urb_dequeue()`'s own entry, correlated against each of the three
device numbers' own urb addresses and against exactly when `dummy_timer()`
finally frees each one, would settle this directly; not attempted this pass
given time already spent on the (disproven) reentrancy-guard hypothesis and
the corrected `urbp`-vs-`urb` memory analysis above.

## What's honestly true right now

- A standalone, out-of-tree `dummy_hcd_fixed.ko` exists, builds clean,
  never touches `/home/build/linux-kronos`, and is a real (if so far
  insufficient) attempt at the documented reentrancy hazard.
- Real, repeated (5/5) live-boot evidence shows the crash is unaffected by
  this fix.
- Real, live GDB evidence this pass adds two genuinely new facts beyond the
  fourth/fifth passes: the `urbp` link is *not* corrupted (contra a naive
  first read), and the `urb` struct's own `transfer_flags`/`transfer_buffer`
  fields hold values inconsistent with correct `usb_init_urb`/
  `usb_fill_control_urb` population, most consistent with `urb`-level (not
  `urbp`-level) use-after-free of unknown exact trigger.
- `OmapNKS4Module.ko`'s `OmapNKS4Probe()` was **not** reached this pass
  (same as every prior pass) — the crash still happens before that point.
- Per this project's own stated policy: not claiming success. Documenting
  precisely what was tried and found, for whoever picks this up next.

## Seventh pass (2026-07-20): live GDB breakpoints, a corrected root cause, a new fix, and a serious build-environment caveat

Full narrative and the real GDB transcript live in
`OmapNKS4VirtualBoard/README.md`'s own "seventh pass" section — not
duplicated in full here. Summary, honestly:

### What changed from every hypothesis above

This pass set **live** breakpoints (`hbreak`, armed before `dummy_hcd_fixed.ko`
was even inserted, so no post-load timing race) at `dummy_urb_enqueue`'s
entry, its `mod_timer`/unlock call site, `dummy_timer`'s entry, and the exact
faulting instruction — then just let the boot proceed and watched, live, in
real time, rather than reconstructing anything from a frozen post-mortem
core. Two independent boot runs both showed, unambiguously:

1. `dummy_timer()` really is invoked synchronously, nested, inside
   `dummy_urb_enqueue()`'s own closing `spin_unlock_irqrestore()`, before
   `dummy_urb_enqueue()` returns — the reentrancy mechanism itself is real
   and directly confirmed, not inferred.
2. It is the URB'S **FIRST-EVER** `dummy_timer()` call — `dum->timer_active`
   was still 0 at the nested entry. There is no *second, overlapping*
   invocation for this crash; the `timer_active` guard above could never
   have caught it, which is exactly why 5/5 boots still crashed with it in
   place. This closes the loop on *why* that fix didn't work, with real
   evidence instead of just "the crash reproduces so it must not have
   worked."
3. It is the **same urb** `dummy_urb_enqueue` just linked onto
   `dum->urbp_list` in this same call, not a stale one from an earlier
   device's abandoned attempt (the "leading hypothesis" a few sections up).
4. `urb->setup_packet` reads `0` at `dummy_urb_enqueue`'s own **first
   instruction**, before any `dummy_hcd` code has touched the urb at all.
   Real, unmodified `usb_fill_control_urb()` always sets it before this
   point in the real call chain, so this pass did not fully explain *why*
   it's already NULL — only *precisely when* and *on which urb*, live,
   reproduced twice.

The `urb`-level "use-after-free" read from this file's own earlier live GDB
section (the `transfer_flags=0xffffff8d`/`transfer_buffer=0x00000200`
oddities) is **not contradicted** by this pass, but also not confirmed as
the mechanism — this pass's own live captures show those same values already
present at `dummy_urb_enqueue`'s own entry, unchanged all the way through to
the crash, which is at least equally consistent with those simply being
stable, legitimate field values at offsets this file's earlier analysis
mis-attributed, as it is with genuine corruption. Only `setup_packet`'s own
offset (`0x54`) has ever been independently, rigorously confirmed (real
oops `EDX` register + an `offsetof()` probe, fourth pass), so this pass put
its own weight only on that field.

### The new fix

A defensive `NULL` check on `urb->setup_packet`, immediately before the line
that dereferences it, using this same file's own existing `-EPROTO`/
`goto return_urb` graceful-failure pattern (already used a few lines above
for "no ep configured for urb"). See `dummy_hcd_fixed.c`'s own inline
`KRONOS I-PIPE REENTRANCY FIX (3, seventh pass)` comment for the full
citation trail. This is a narrower, more honestly-scoped claim than the
`timer_active` guard: it does not explain the upstream anomaly, it just stops
that anomaly from crashing the kernel, using the file's own established
error-handling idiom.

### Why this fix has NOT been repeated-boot validated (read before trusting it)

Every fresh rebuild of `dummy_hcd_fixed.ko` done this pass — including a
rebuild of the **unmodified, pre-patch** source, before this pass's own fix
was even written — produces a binary that fails much earlier and
differently: `dummy_hub_control()`'s own root-hub descriptor read fails
(`hub 1-0:1.0: config failed, can't read hub descriptor (err -22)`) or, with
this directory's local `hcd.h`/`hub.h` updated to match the *current* live
`/home/build/linux-kronos` tree, hangs outright. Neither ever reaches real
device enumeration, so neither can exercise (or validate a fix for) the
actual `dummy_timer` crash at all. `objdump -d` confirms a genuine
struct-layout ABI difference (`struct dummy` field offsets inside
`dummy_hub_control()` shifted by a few bytes) between this pass's own
rebuilds and the sixth pass's own already-built, already-validated
`dummy_hcd_fixed.ko` (md5 `14615070a4223583dbd48b659e0f2654`) — despite
identical `.c` source. This pass spent real effort trying to root-cause the
drift (git history on `/home/build/linux-kronos` — clean, no relevant
commits; `.config` — identical to an earlier verified snapshot; `hcd.h`/
`hub.h`/`gadget.h` content — identical between the current live tree and an
independent full-tree snapshot from earlier the same day as the sixth pass)
and did **not** find the exact cause. This directory's `hcd.h`/`hub.h` were
left reverted to the originally-documented, git-tracked values (matching
what the sixth pass's own validated build used) — this is the safer,
deterministic (non-hanging) state, but it is **not** ABI-correct enough to
reach the bug.

**An earlier, invalid "10/10 repeated boots, no panic" result for this exact
fix was caught and discarded before being reported** — none of those 10 runs
ever got past the root-hub descriptor read, so none exercised the actual
crash path at all. A dedicated control test (same disk image, swapping only
the `.ko`: sixth pass's own known-good binary boots and crashes exactly as
documented every time; any of this pass's own fresh rebuilds fails the root
hub instead) isolated the problem to the rebuild itself, not the disk image,
not the source patch, and not general environment flakiness.

**What's honestly true after this pass**:
- Real, live (not post-mortem), twice-independently-reproduced GDB evidence
  directly disproves the `timer_active` guard's own premise (this was the
  first-ever `dummy_timer()` call, not an overlapping second one) and the
  "stale urb from an earlier device" hypothesis (it's the same, just-enqueued
  urb) — both with live breakpoint hits at the moment of the actual crash,
  not reconstructed afterward.
- A new, honestly-scoped, minimal fix exists, compiles clean, and is
  directly justified by this pass's own live evidence.
- That fix has **not** been confirmed to resolve the crash via repeated
  boot testing, because no rebuild produced this session could reach the
  crash's own code path at all — a separate, real, and currently unsolved
  build-environment problem, not a flaw in the fix's own logic.
- Whoever continues this next should treat getting a *provably* ABI-correct
  `dummy_hcd_fixed.ko` rebuild (matching the sixth pass's own known-good
  `14615070a4223583dbd48b659e0f2654` at the object-code level, not just the
  `.c` source level) as the actual next blocker — ahead of any further
  hypothesis work on the crash itself.

## Eighth pass (2026-07-20): the "build-environment ABI-drift" mystery
## root-caused (two hosts silently run different kernel trees) — and a
## second, independent, previously-undocumented `struct urb` layout
## mismatch found. Neither fixed or boot-tested yet; both real, both
## verified directly, not inferred.

Picked up the seventh pass's own explicit closing instruction: stop
hypothesizing about the crash itself, first get a provably ABI-correct
rebuild. Two real, separate findings came out of that, both confirmed by
direct inspection rather than assumed.

### Finding 1 — `kronosdev` and `kronosvm` have silently diverged `/home/build/linux-kronos` trees; every boot test has run against the *unfixed* one

This project runs across (at least) two hosts: `kronosdev` (ordinary
build/dev host, no real QEMU/`kronos.img` boot-test rig — confirmed, only
one stray leftover `kronos.img` exists there, in an unrelated OA-project
scratch dir) and `kronosvm` (the dedicated boot-test VM — every
`vmtest`/`*_boot_test_*`/`omapnks4vb_*` scratch dir with a `kronos.img` +
QEMU session lives there). Both hosts have their own independent copy of
`/home/build/linux-kronos`, and `project_linux_kronos_kernel_tree.md` — the
document that establishes this tree as the byte-accurate build environment —
never flags that there are two copies, only ever describing "the" tree.

Directly compared them:

```
kronosdev:/home/build/linux-kronos/drivers/usb/core/hcd.h   mtime 2026-07-19 17:14
kronosdev:/home/build/linux-kronos/include/linux/usb.h      mtime 2026-07-19 17:14
kronosvm:/home/build/linux-kronos/drivers/usb/core/hcd.h    mtime 2026-07-03 16:22
kronosvm:/home/build/linux-kronos/include/linux/usb.h       mtime 2026-07-03 16:22
kronosvm:/home/build/linux-kronos/.config                   mtime 2026-07-03 16:51
```

`kronosdev`'s copy carries real, explicitly-commented `KRONOS ABI FIX
(2026-07-19)` blocks in both `hcd.h` and `usb.h` — exactly the `usb_bus`/
`usb_hcd` struct-layout fix this same file's sibling,
`OmapNKS4VirtualBoard/README.md`'s "second pass" section, derived and the
"third pass" section claimed to have "confirmed working end-to-end."
Built a tiny `offsetof()` probe against each host's own tree to confirm the
practical effect, not just eyeball the comment:

| field | `kronosdev` (has the 2026-07-19 fix) | `kronosvm` (2026-07-03, untouched) |
|---|---|---|
| `usb_hcd.driver` | `0x94` (matches the real kernel) | `0x8c` (the historically "broken" value) |
| `sizeof(struct usb_hcd)` | `0xdc` (matches) | `0xc8` |

**`kronosvm`'s tree was never patched.** Its `hcd.h`/`usb.h`/`.config`
mtimes (2026-07-03, 16:xx) predate the *entire* second-through-seventh-pass
investigation (which starts 2026-07-19) — this is the tree's original,
untouched, `project_linux_kronos_kernel_tree.md`-setup state. Whoever
applied and validated the `usb_hcd`/`usb_bus` fix during the second/third
pass did so against a tree that either never got copied back to `kronosvm`,
or was later reset there — the exact mechanism wasn't chased further (not
needed to fix the actual, present-tense problem), but the practical,
directly-confirmed fact is: **every module ever boot-tested in this
project since the third pass — including the sixth pass's own "known-good"
`dummy_hcd_fixed.ko` (md5 `14615070a4223583dbd48b659e0f2654`) and every one
of the seventh pass's own confusing rebuild attempts — was built against
`kronosvm`'s tree, which has never had the `usb_hcd`/`usb_bus` fix applied,
despite that fix being fully derived, written up, and reported as
"confirmed working end-to-end" two passes ago.**

This reframes the seventh pass's own "build-environment ABI-drift" framing:
`kronosvm`'s tree itself has not drifted or changed at all (mtimes prove
it's been static since 2026-07-03) — the real problem is that it was never
brought up to the fixed state that exists, and was validated, only on a
different host. This doesn't fully explain every specific symptom the
seventh pass reported (e.g. why a fresh rebuild there behaves differently
from the sixth pass's own already-built binary, when both would be built
against the same never-changed tree — not re-investigated this pass, low
priority next to actually fixing the tree), but it is a concrete, verified,
actionable gap, and closing it is a precondition for any of this project's
own prior struct-layout analysis to actually apply to what gets boot-tested.

**Not yet done**: copying `kronosdev`'s fixed `hcd.h`/`usb.h` (or
re-deriving/reapplying the same `KRONOS ABI FIX` blocks) onto `kronosvm`'s
tree, rebuilding, and boot-testing. This is now the single highest-value,
cheapest next step available — the fix is already fully written and
verified elsewhere; it just needs to reach the host where it matters.

### Finding 2 — a second, independent, previously-undocumented `struct urb` layout mismatch, present on BOTH hosts, not yet fixed anywhere

Before finding the cross-host divergence above, this pass also re-verified
this file's own repeated claim that "`setup_packet`'s own offset (`0x54`)
has ever been independently, rigorously confirmed" — using a fresh
`offsetof()` probe (`probe3.c`, `omapnks4_build_stabilize_20260720_092500/
probe/` on `kronosvm`) built and disassembled twice from a clean rebuild
each time (byte-identical `.o`/disassembly both times — rules out
compiler/toolchain nondeterminism as a factor in anything below). Confirmed
`kronosvm`'s tree computes `setup_packet` at `+0x54` exactly as documented.

Then went further than any prior pass and disassembled the *real* kernel's
own `usb_control_msg` (in `omapnks4_dummyhcd_abi_20260719/extracted/
vmlinux_symbolicated.elf` — banner `Linux version 2.6.32.11-korg
(root@kronos) ... #31 SMP PREEMPT Thu Oct 10 08:41:36 JST 2024`, i.e.
genuinely Korg's own factory build, extracted from `kronos.img`'s own
`/boot/bzImage`, not a rebuild from either tree). `usb_internal_control_msg`
is inlined into it; matched every store directly against
`usb_fill_control_urb(urb, dev, pipe, setup_packet, transfer_buffer,
buffer_length, complete_fn, context)`'s own parameter order (source-verified
in `drivers/usb/core/message.c`), not guessed from position alone:

| field | real kernel (`kronos.img`'s own compiled code) | this project's tree (`kronosdev` AND `kronosvm` — both agree) | delta |
|---|---|---|---|
| `dev` | `0x28` | `0x28` | — (match) |
| `pipe` | `0x30` | `0x30` | — (match) |
| `transfer_buffer` | `0x40` | `0x3c` | **+4** |
| `transfer_buffer_length` | `0x50` | `0x4c` | **+4** |
| `setup_packet` | `0x58` | `0x54` | **+4** |
| `context` | `0x70` | `0x6c` | **+4** |
| `complete` | `0x74` | `0x70` | **+4** |

Every field up to and including `pipe` matches exactly; every field from
`transfer_buffer` onward is consistently `+4` in the real kernel. This
means a real, 4-byte field (or size difference) exists in the real kernel's
`struct urb` somewhere between `pipe` and `transfer_buffer` — i.e. inside
the `status`/`transfer_flags` region — that this project's `usb.h` (on
*both* hosts — confirmed identical there; this is not the same bug as
Finding 1) doesn't have.

**Ruled out**: `dma_addr_t` being 8 bytes (`CONFIG_X86_PAE`/
`CONFIG_HIGHMEM64G`) would explain a shift *starting at* `transfer_dma`, not
one that already exists at `transfer_buffer`, one field earlier — and both
this project's `.config`s (`kronosvm`'s live one and the
`linux-kronos-expcopy` snapshot) have `CONFIG_HIGHMEM4G=y` /
`CONFIG_HIGHMEM64G is not set` / `CONFIG_ARCH_PHYS_ADDR_T_64BIT is not set`
— `dma_addr_t` is 4 bytes in both, so this isn't the mechanism.

**Ruled out**: the GCC-4.5-vs-GCC-12 bitfield-packing mechanism that
`OmapNKS4VirtualBoard/README.md`'s second pass proposed (and partially
confirmed) for part of the `usb_bus` delta — `struct urb` has zero
bitfields anywhere in it, so there's nothing for that mechanism to act on.
This is most consistent with the *other* mechanism that second pass also
flagged but didn't pin down for its own remaining delta: a genuinely
different/extra field that Korg's real vendor kernel carries and the
`cgudrian/linux-kronos` community reconstruction doesn't — i.e. the same
general phenomenon as Finding 1's `usb_hcd`/`usb_bus` gap, independently
recurring in a completely different struct.

**Why this plausibly explains the entire multi-pass `dummy_timer` saga,
stated as a real hypothesis, not a confirmed fix**: `dummy_hcd`'s own code
(real, unmodified, in both `dummy_hcd.ko` and this directory's
`dummy_hcd_fixed.ko`) reads `urb->setup_packet` at its own compiled offset,
`+0x54`. If the real kernel's core code (`hub_port_init`'s descriptor reads,
via `usb_control_msg`/`usb_fill_control_urb`) actually writes the real
setup-packet pointer at `+0x58`, then `dummy_timer()`'s read of `+0x54` is
reading whatever *actually* lives at that offset in the true layout — not
the real `setup_packet` at all. This would cleanly explain the seventh
pass's own live GDB finding ("`urb->setup_packet` reads `0` at
`dummy_urb_enqueue`'s own first instruction, before any `dummy_hcd` code
touches it") without needing any race, reentrancy, or use-after-free
mechanism at all: if `+0x54` in the true layout is simply some other
legitimately-zero-at-that-point field (a strong candidate: it would fall
inside or adjacent to the still-unidentified `status`/`transfer_flags`
region this same finding already implicates), reading `0` there is exactly
what you'd see, on every boot, from the very first instruction, regardless
of race conditions — matching the observed evidence better than any of the
seven prior passes' own race-condition-based hypotheses, none of which ever
actually fixed the crash.

**Not yet determined**: which exact field is inserted, or whether it's a
field-size difference rather than an insertion — pinning this down needs
either real Korg kernel source (not just the `cgudrian` reconstruction) or
further live-kernel GDB triangulation of the `status`/`transfer_flags`
region during a real control transfer, the same way Finding 1's fields were
each pinned to a named, semantically-identified real field via direct
disassembly of the real kernel's own writes. **Not yet attempted this
pass.**

### What's honestly true after this pass

- Two real, independent, verified struct-layout mismatches now exist in
  this project's understanding: `usb_hcd`/`usb_bus` (documented since the
  second pass, fix derived and written, but never actually applied on the
  one host that matters for boot testing) and `struct urb` (new this pass,
  not yet fixed anywhere, exact missing field not yet identified).
- Neither has been fixed-and-reboot-tested. This pass found and evidenced
  both problems; it did not resolve either.
- **Recommended order for whoever continues, cheapest/most-certain first**:
  1. Sync `kronosdev`'s already-fixed `hcd.h`/`usb.h` onto `kronosvm`'s tree
     (or reapply the same `KRONOS ABI FIX` blocks there directly), rebuild
     `dummy_hcd_fixed.ko`, and boot-test — this fix is fully written and
     verified already; it has simply never been exercised on the host where
     boot tests happen. Do this in isolation first (without touching
     `struct urb`) so any change in crash behavior can be attributed
     correctly.
  2. Only then pursue the `struct urb` gap: triangulate the exact
     `status`/`transfer_flags`-region field via live GDB (per the method
     that worked for Finding 1), patch `usb.h` the same documented,
     commented-pad-field way `kronosdev`'s tree already does for
     `usb_hcd`/`usb_bus`, rebuild, and boot-test again, separately.
  3. Only after both are in place and boot-tested does re-attempting a
     `setup_packet`-NULL-check-style defensive fix (or re-evaluating whether
     one is even still needed) make sense — every previous fix attempt in
     this file was built and tested against a `struct urb`/`struct usb_hcd`
     understanding now known to disagree with the real kernel in two
     separate, real ways.

## Ninth pass (2026-07-20): step 1 of the eighth pass's own recommended
## order done and boot-tested in isolation — the panic is gone, real USB
## enumeration is still blocked by the separate, not-yet-touched `struct urb`
## bug

Did exactly what the eighth pass's own closing recommendation said to do
first, and only that:

1. Copied `kronosdev`'s already-fixed `drivers/usb/core/hcd.h` and
   `include/linux/usb.h` onto `kronosvm`'s `/home/build/linux-kronos`
   (`kronosvm`'s originals backed up first to
   `/home/build/linux-kronos_prefix_backup/`). Verified byte-identical via
   md5 after the copy, and confirmed both hosts' `.config`s agree on the one
   `#ifdef`-conditional field inside `struct usb_hcd` (`CONFIG_PM`, both
   `y`) — ruling out a `.config`-driven layout confound before trusting the
   sync.
2. Rebuilt a fresh `offsetof()` probe against the now-synced live tree
   (explicitly pointing `KDIR=/home/build/linux-kronos`, not the separate
   `linux-kronos-expcopy` snapshot the eighth pass's own probe used) and
   confirmed the practical effect: `offsetof(struct usb_hcd, driver)` on
   `kronosvm` is now `0x94` (matching the real kernel), up from the old
   broken `0x8c`.
3. Rebuilt `dummy_hcd_fixed.ko` on `kronosvm` from the canonical
   `dummy_hcd_fixed.c`/`hcd.h`/`hub.h`/`Makefile` (fresh copies staged from
   this directory, not reused stale scratch-dir artifacts), clean build, only
   the same pre-existing warning noise every other module in this project
   already has.
4. Injected that new `.ko` into a fresh copy of
   `omapnks4vb_dummyhcdfix_boottest_20260719/kronos.img` via `guestfish`
   (md5-verified match between the built file and the file inside the image
   before booting), and boot-tested it **3 times independently** (3 separate
   fresh image copies, not the same image rebooted), each a real
   `qemu-system-i386 -no-reboot` run watched for either `"fixtest chain
   done"` or `"Kernel panic"` in the console log, exactly like the sixth
   pass's own harness.

**Result, 3/3, no panic:**
```
[loadoa] WARNING: insmod OmapNKS4Module.ko failed (rc=141)
[loadoa] insmod OmapNKS4Module.ko returned rc=141
[loadoa] fixtest chain done -- reached end of loadoa script
```
This is the first time in this file's entire multi-pass history that a boot
test of this module has reached the end of the loadoa chain without a
kernel panic. All 3 runs also show the identical `setup_packet`-NULL
defensive-failure signature from the seventh pass's own fix (12 hits each,
different urb addresses across the 3 independent boots as expected, same
message):
```
dummy_hcd_fixed dummy_hcd_fixed: setup_stage urb <addr> has NULL setup_packet -- failing cleanly instead of crashing (see dummy_hcd_fixed.c KRONOS I-PIPE REENTRANCY FIX (3) comment)
```
followed by `usb 1-1: device descriptor read/8, error -71`, `hub 1-0:1.0:
unable to enumerate USB device on port 1`, and `OmapNKS4Probe()` timing out
— i.e. **the panic is gone, but real enumeration still doesn't work**,
exactly as predicted: this pass deliberately did not touch the eighth pass's
second finding (the `struct urb` `+4` offset bug), specifically so the two
fixes' effects could be attributed separately. `setup_packet` reading NULL
here is now understood to be a direct, expected consequence of that
still-open bug, not a mystery.

**What's honestly true after this pass:**
- The `usb_hcd`/`usb_bus` fix is now actually live on the one host that
  matters for boot testing, confirmed both structurally (`offsetof` probe)
  and behaviorally (3/3 clean boot chain completions, no panic — a real
  change from every single prior pass's 100% panic rate).
- Real USB device enumeration through `dummy_hcd_fixed` still does not work.
  The blocker is now narrowed to exactly one known, already-diagnosed cause
  (`struct urb` `setup_packet` etc. at `+0x54` instead of the real kernel's
  `+0x58`) rather than an open mystery.
- Per the eighth pass's own recommended order, the next step is triangulating
  and patching the `struct urb` gap (item 2 in that pass's list), then
  boot-testing again — only after that should the `setup_packet`-NULL defensive
  check be re-evaluated, since it may no longer be needed once the real
  offset is fixed.
- Scratch boot-test disk image copies from this pass were deleted after their
  console logs were captured; only the logs and this writeup persist.

## Tenth pass (2026-07-20): the `struct urb` gap triangulated against the
## LIVE production Kronos, patched, and boot-tested — `setup_packet` bug
## fully resolved, enumeration progresses further than ever, one new
## separate bug exposed

Per the eighth/ninth passes' own recommended order (item 2): triangulated
the exact `struct urb` insertion point using the **live production Kronos
itself** (192.168.100.15) as source of truth, not just the `kronos.img`
factory image used previously.

### Triangulation

With live SSH access to the powered-on unit, pulled `/boot/bzImage` directly
off it (confirmed genuinely running, not stale, via `/proc/uptime`),
extracted `vmlinux`, and disassembled real `usb_submit_urb` (`0x4034c5c0`)
and `usb_hcd_giveback_urb` (`0x4034aa40`) using addresses read from that same
device's own `/proc/kallsyms`:

- `usb_submit_urb`: `mov DWORD PTR [eax+0x38],0xffffff8d` — `urb->status =
  -EINPROGRESS` at real offset **0x38**, not this tree's `0x34`. Also zeroes
  `actual_length` at `[eax+0x54]` (not `0x50`), and does a transfer_flags
  direction-bit read-modify-write at `[eax+0x3c]` (not `0x38`).
- `usb_hcd_giveback_urb`: `mov DWORD PTR [ebx+0x38],esi` — `urb->status =
  status` again at **0x38**, immediately before `call DWORD PTR [ebx+0x74]`
  — `urb->complete(urb)`, independently reconfirming `complete@0x74` a
  second way, via a completely different function than the one that
  originally established it.
- Both agree, and both agree with every previously-confirmed field
  (`dev@0x28`, `pipe@0x30`, `transfer_buffer@0x40`,
  `transfer_buffer_length@0x50`, `setup_packet@0x58`, `context@0x70`,
  `complete@0x74`): a single 4-byte gap sits right after `pipe`, before
  `status`. Also confirmed `hcpriv@0x4`, `use_count@0x8`, `reject@0xc`,
  `unlinked@0x10` all unshifted (read directly in `usb_hcd_giveback_urb`),
  bounding the gap precisely to just those 4 bytes.

**A false lead caught and ruled out before being written down anywhere**:
initially misread a `call DWORD PTR [ebx+0x34]` inside what looked like the
same function as pointing at a mystery field in `struct urb`. Rechecking the
real function's boundaries against `/proc/kallsyms` (sorted, adjacent
symbols) showed that instruction actually belongs to `unlink1`, a separate
static helper starting at `0x4034aac0` — `ebx` there is `hcd->driver` (offset
`0x94`, the already-known real `usb_hcd.driver` offset), and the call is
`hcd->driver->urb_dequeue`, a `struct hc_driver` hook with nothing to do
with `struct urb`. Caught by checking symbol boundaries before trusting the
read, not after.

**What the field actually is: not identified.** Deliberately checked every
generic urb-lifecycle function that touches nearby fields —
`usb_control_msg`, `usb_submit_urb`, `usb_hcd_giveback_urb`, `unlink1`,
`usb_unlink_urb`, `usb_kill_urb`, `usb_hcd_check_unlink_urb` — full
disassembly of each, not just a narrow grep. None of them ever read or write
the literal gap offset. Whatever real Korg/vendor field occupies it isn't
touched by any of usbcore's own generic urb-handling code; only
transfer-type-specific or HCD-internal code this pass didn't reach could
name it. Same situation as the sibling `usb_hcd`/`usb_bus` fix's own
unidentified `irq_descr`-to-`rh_timer` gap — plain padding used, same
convention, documented the same honest way in `usb.h`'s own inline comment.

### Patch, sync, rebuild

Inserted `u32 __kronos_abi_pad_pipe_to_status;` between `pipe` and `status`
in `/home/build/linux-kronos/include/linux/usb.h` (this host, `kronosdev`).
Verified via a fresh `offsetof()` probe module: `status=0x38`,
`transfer_flags=0x3c`, `transfer_buffer=0x40`, `transfer_buffer_length=0x50`,
`actual_length=0x54`, `setup_packet=0x58`, `context=0x70`, `complete=0x74` —
every one now matching the real kernel exactly. Synced the patched `usb.h`
onto `kronosvm` (md5-confirmed identical after copy), rebuilt
`dummy_hcd_fixed.ko` there (clean build, same pre-existing warning noise as
every other module), and injected the new binary into 3 fresh disk-image
copies via `guestfish` (md5-verified before booting each).

### Result, 3/3: NULL `setup_packet` failure completely gone, enumeration progresses further than ever, one new bug exposed

```
usb 1-1: new full speed USB device number 2 using dummy_hcd_fixed
OmapNKS4:OmapNKS4Init: line 1393: OmapNKS4Init: enter
usb 1-1: new full speed USB device number 3 using dummy_hcd_fixed
usb 1-1: new full speed USB device number 4 using dummy_hcd_fixed
usb 1-1: config 1 has an invalid descriptor of length 0, skipping remainder of the config
usb 1-1: config 1 interface 0 altsetting 0 has 1 endpoint descriptor, different from the interface descriptor's value: 2
usb 1-1: configuration #1 chosen from 1 choice
OmapNKS4:OmapNKS4Init: line 1418: Waited ... cycles for OmapNKS4Probe(). driver state is 0
OmapNKS4:OmapNKS4Init: line 1423: OmapNKS4Init: probe failed
[loadoa] WARNING: insmod OmapNKS4Module.ko failed (rc=141)
[loadoa] fixtest chain done -- reached end of loadoa script
```
Identical across all 3 independent runs. `grep -c "has NULL setup_packet"`
is **0** in every log (was 12 in every ninth-pass log) — the seventh pass's
defensive NULL check never even fires now, meaning `dummy_hcd_fixed` is
genuinely reading a real, non-NULL `setup_packet` for the first time in this
entire multi-pass investigation. Enumeration also gets substantially
further: it now successfully reads a device descriptor, walks the
configuration, and reaches `configuration #1 chosen from 1 choice` — real
forward progress no prior pass reached.

**New, separate, previously-hidden bug**: `usb 1-1: config 1 has an invalid
descriptor of length 0, skipping remainder of the config` and `...has 1
endpoint descriptor, different from the interface descriptor's value: 2`.
`OmapNKS4VirtualBoard.c`'s own `nks4_intf_desc.bNumEndpoints = 2` and it
does define two endpoint descriptors (`nks4_int_ep_desc`,
`nks4_bulk_ep_desc`), so the interface descriptor's claim of 2 is correct —
but the real host-side config-descriptor walk only finds 1 before hitting a
zero-length descriptor and aborting early. This points at a bug in this
project's own bind-time descriptor-buffer assembly (`wTotalLength` /
concatenation logic around line ~267-271 of `OmapNKS4VirtualBoard.c`), not
at struct-layout ABI drift — a different class of bug than everything this
file has chased through nine prior passes. **Not yet investigated or
fixed.**

### What's honestly true after this pass

- Both struct-layout bugs identified across the eighth/ninth/tenth passes
  (`usb_hcd`/`usb_bus`, and now `struct urb`) are fixed, synced to
  `kronosvm`, and boot-tested — 3/3 clean each time, with the exact
  predicted effect (panic gone, then NULL-setup_packet gone) confirmed both
  times, not assumed.
- `dummy_hcd_fixed` genuinely works correctly now as far as struct-layout
  ABI is concerned — it reads real, correct field values from real urbs.
- Real device enumeration is still blocked, but by a newly-visible, different,
  narrower bug (a malformed gadget config descriptor) that was always there
  but was previously masked by the two struct-layout bugs crashing/failing
  the chain before ever reaching this point.
- Next step for whoever continues: fix `OmapNKS4VirtualBoard.c`'s config
  descriptor assembly so the second endpoint descriptor is actually included
  and no zero-length descriptor is emitted, then boot-test again.

## Eleventh pass (2026-07-20): config descriptor bug fixed and confirmed —
## but `OmapNKS4Probe()` still doesn't fire, a further, distinct blocker

### Root cause and fix

`struct usb_endpoint_descriptor` (`ch9.h`) is `__attribute__((packed))` but
carries 2 trailing audio-only fields (`bRefresh`, `bSynchAddress`) beyond the
real 7-byte wire format — the header's own comment says so explicitly
("use USB_DT_ENDPOINT*_SIZE in bLength, not sizeof"). Each endpoint
descriptor's own `bLength` was already correctly `USB_DT_ENDPOINT_SIZE` (7),
but the `USB_DT_CONFIG` handler in `nks4_setup()`
(`OmapNKS4VirtualBoard.c`) used `sizeof(nks4_int_ep_desc)`/
`sizeof(nks4_bulk_ep_desc)` (9, including the 2 audio-only bytes) both for
`wTotalLength` and for advancing the buffer offset between descriptors. That
2-byte-per-endpoint gap between what `bLength` claims and where the next
descriptor actually starts is exactly what the real host's config parser
tripped on: it walks the buffer by `bLength` (7), lands 2 bytes into the
leftover zero-padding after the first endpoint descriptor, and reads those
zero bytes as a bogus zero-length descriptor header — matching the observed
`"invalid descriptor of length 0, skipping remainder of the config"`
exactly, and explaining why only 1 of 2 endpoints was ever found (parsing
aborted right there, before reaching the real second one that the code did
write, just 2 bytes further down than the host expected).

Fix: use `USB_DT_ENDPOINT_SIZE` instead of `sizeof(...)` for both the
`wTotalLength` computation and the `memcpy`/offset advance for each endpoint
descriptor (`struct usb_config_descriptor`/`struct usb_interface_descriptor`
have no such trap — both are packed with no extra fields, so `sizeof()` was
already correct for those two).

### Verification, 3/3 clean

Rebuilt `OmapNKS4VirtualBoard.ko` on `kronosvm` against the patched source
(synced fresh from this canonical directory, not a stale scratch copy),
injected it alongside the already-fixed `dummy_hcd_fixed.ko` into 3 fresh
disk-image copies (md5-verified both files in each image before booting),
and boot-tested 3 independent times. **All 3 runs: no panic, and the
descriptor warnings are completely gone** — no `"invalid descriptor of
length 0"`, no `"different from the interface descriptor's value"`,
anywhere in any log. The device cleanly reaches `configuration #1 chosen
from 1 choice` with a fully correct descriptor set for the first time in
this entire investigation.

### But: `OmapNKS4Probe()` still doesn't fire — a further, distinct blocker

All 3 runs still show the identical failure as every prior pass:
```
OmapNKS4:OmapNKS4Init: line 1418: Waited ... cycles for OmapNKS4Probe(). driver state is 0
OmapNKS4:OmapNKS4Init: line 1423: OmapNKS4Init: probe failed
[loadoa] WARNING: insmod OmapNKS4Module.ko failed (rc=141)
```
The config descriptor bug was real and is genuinely fixed — this is not a
"the fix didn't work" result — but it was not the last thing standing
between this project and a working `OmapNKS4Probe()` call. With enumeration
now completing cleanly (correct descriptors, both endpoints recognized,
configuration selected), whatever's still preventing `OmapNKS4Module.ko`'s
own `usb_register_driver()`-registered driver from being matched/attached to
this now-correctly-enumerated device is a **different, not-yet-identified**
class of bug — not struct-layout ABI drift (both known instances of that are
fixed and confirmed), not descriptor malformation (also just fixed and
confirmed). Candidates not yet checked: driver `usb_device_id` match-table
criteria (vendor/product ID match vs. interface-class match flags),
load-order/timing between `OmapNKS4VirtualBoard.ko`'s bind and
`OmapNKS4Module.ko`'s own `usb_register_driver()` call, or something in
`OmapNKS4Module.ko`'s own probe-eligibility checks not yet reached by this
investigation. **Not yet investigated.**

### What's honestly true after this pass

- The specific bug this pass was asked to fix (config descriptor
  malformation) is fixed and confirmed via repeated, independent boot
  testing — a real, verified result, not an assumption.
- All three struct/descriptor-layout bugs found across this file's eighth
  through eleventh passes are now fixed, synced to `kronosvm`, and
  boot-tested clean.
- Full `OmapNKS4Probe()` success — the file's original stated goal — is
  still not achieved. The remaining blocker is real but not yet
  characterized; this pass narrows the search space (it's neither of the
  two known layout bugs) without yet identifying the actual cause.
