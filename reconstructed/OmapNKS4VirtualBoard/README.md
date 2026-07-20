# OmapNKS4VirtualBoard.ko — a genuine virtual NKS4 front-panel USB device

Reconstructed-project-original source (no shipping-binary counterpart — this is new
infrastructure, not a reverse-engineered driver). Target: **Linux 2.6.32.11-korg**,
x86-32, `gcc -mregparm=3`, plain C (matches every other virtual driver's own precedent
for avoiding C++ against this ancient kernel's headers).

## What this is, and why it's different from every other "virtual" module here

Every other `VirtualX` module in this project (`AT88VirtualChip.ko`,
`KorgUsbAudioVirtualDriver.ko`, `OmapNKS4VirtualDriver.ko`) works by supplying the
`EXPORT_SYMBOL`s a real caller needs — standing in for a real module's own exports.
That pattern doesn't apply here. `OmapNKS4Module.ko` is not a library of symbols
something else calls into; it is **itself a USB host driver**
(`stg_usb_register_driver`, a thin `STGEnabler.ko` shim over the real kernel's own
`usb_register_driver()`). Its own `OmapNKS4Init()` blocks in
`wait_for_completion_timeout(sProbeComplete, 10000)` waiting for the Linux USB core to
call its registered `.probe` callback (`OmapNKS4Probe`) — which only happens when a
**real USB device** matching its ID table (vendor `0x0944`, product `0x1005`,
confirmed real values, see `OmapNKS4Module/usb.cpp`'s own `OmapNKS4Probe`) actually
enumerates. No amount of providing extra exported symbols substitutes for that — the
missing piece was a genuine USB device for the real module's own host-side driver to
talk to.

This module **is** that device: a real Linux USB gadget driver presenting the exact
vendor/product ID and endpoint layout `OmapNKS4Probe`'s own confirmed logic checks for
(`usb.cpp`: `idVendor`/`idProduct` read from `dev+0xb8`/`+0xba`; endpoint classification
by `bmAttributes&3==3`+IN = interrupt, `bmAttributes&3==2`+OUT = bulk; interface class
`0xff`, confirmed real value cross-referenced in `main.cpp`'s own `struct usb_driver`
reconstruction and `CLAUDE.md`'s own documented ground truth). Loaded together with
`dummy_hcd.ko` — Linux's own loopback USB host+device controller — in the **same**
kernel instance, `dummy_hcd` loops this gadget's device-side traffic back to its own
virtual USB host controller. The real `OmapNKS4Module.ko`, bound to that virtual host
controller exactly as it would be to a real PC's own EHCI/OHCI/UHCI controller, sees a
real device enumerate and calls `OmapNKS4Probe` for real. No wire-protocol guesswork
stands between the two — this is genuine Linux USB core code running the genuine
enumeration/probe path on both sides.

## Kernel config change required (already applied to this project's own tree)

The target kernel tree (`/home/build/linux-kronos`) had `CONFIG_USB_GADGET` disabled.
Enabled as **loadable modules only** (`CONFIG_USB_GADGET=m`, `CONFIG_USB_DUMMY_HCD=m`
via the `USB_GADGET_DUMMY_HCD` choice), a purely additive change — it doesn't touch any
ABI-relevant option (`SMP`/`PREEMPT`/etc. all unchanged), so it's safe for every other
module already built against this same shared tree. No kernel image (`bzImage`) rebuild
is needed either — the gadget framework and `dummy_hcd` build as ordinary out-of-tree-
style loadable modules via a **targeted** build of just `drivers/usb/gadget/`:

```sh
cd /home/build/linux-kronos
# .config already has CONFIG_USB_GADGET=m / CONFIG_USB_GADGET_DUMMY_HCD=y /
# CONFIG_USB_DUMMY_HCD=m / CONFIG_USB_GADGET_DUALSPEED=y appended and folded in via
# `make ARCH=i386 oldconfig` (this kernel predates `oldconfig`'s modern
# `olddefconfig`/`oldnoconfig` non-interactive targets).
make ARCH=i386 modules_prepare
make ARCH=i386 M=drivers/usb/gadget modules
# -> drivers/usb/gadget/dummy_hcd.ko
```

`dummy_hcd.ko` itself exports `usb_gadget_register_driver`/`usb_gadget_unregister_driver`
directly (this kernel era doesn't have a separate shared "gadget core" module — each
UDC driver carries the registration entry points).

## Status: `dummy_hcd.ko` found TCG-incompatible on this VM, same class of finding as RTAI

**Live VM boot test, 2026-07-17**: `dummy_hcd.ko` loads cleanly (its own init messages
print correctly - "USB Host+Gadget Emulator", "new USB bus registered") and the kernel's
generic hub driver begins enumerating dummy_hcd's own virtual root hub ("usb usb1:
configuration #1 chosen from 1 choice") - then the whole VM **hangs**, confirmed via two
independent tests:

1. The full chain (`RTAIVirtualDriver` → `STGEnabler` → `STGGmp` → `dummy_hcd` →
   `OmapNKS4VirtualBoard` → `OmapNKS4Module`) stalled at that exact point - 60+ seconds
   with zero new console output, QEMU process pinned at 150%+ CPU (actively spinning,
   not blocked/sleeping - ruled out a lock wait).
2. An **isolation test** loading only `RTAIVirtualDriver` → `STGEnabler` → `STGGmp` →
   `dummy_hcd` (this module and `OmapNKS4Module.ko` both excluded) hit the **identical**
   stall at the **identical** log line. This conclusively rules out
   `OmapNKS4VirtualBoard.c`'s own code (bind/setup/completion handlers never even ran)
   and `OmapNKS4Module.ko` as the cause - the hang is inside `dummy_hcd.ko`'s own
   root-hub enumeration path, under this specific QEMU-TCG environment.

This is architecturally the **same class of finding** that motivated
`RTAIVirtualDriver.ko`'s own existence (see that module's README, `MASTER_REFERENCE.md`
sec 10.211-10.214): a stock Linux subsystem that depends on kernel timer/polling behavior
which QEMU-TCG's own timing model doesn't service reliably. `dummy_hcd`'s root hub uses a
periodic kernel timer to poll for "port status changed" events (the same general
category of mechanism, though not necessarily the identical code path, as RTAI's own
hardware-timer takeover) - plausible root cause, not independently confirmed via GDB
this pass given time spent isolating the symptom instead.

**Not yet attempted**: a GDB-based root-cause trace into `dummy_hcd.c`'s own root-hub
timer/workqueue code (the same investigative technique that diagnosed RTAI's own hang,
see `MASTER_REFERENCE.md` sec 10.212), or a from-scratch VM-appropriate substitute for
`dummy_hcd` itself (the same "sidestep the stock subsystem" pattern `RTAIVirtualDriver.ko`
already established) - the natural next step if genuine live USB enumeration in this VM
is still wanted. Until then, `OmapNKS4Module.ko`'s own boot-test coverage is validated
via the core chain **without** this module (`RTAIVirtualDriver` → `STGEnabler` →
`STGGmp` → `OmapNKS4Module`, no `dummy_hcd`/gadget layer) - see
`OmapNKS4Module/README.md`'s own boot-test section for that result. This module's own
source is complete, builds clean, and is ready to use the moment the `dummy_hcd`
blocker is resolved - the design itself is not in question, only this one VM/TCG
compatibility gap.

## Update, 2026-07-19: root-caused live via GDB — NOT a timer/polling hazard, a wild
## indirect call through a bad function pointer, deterministic across independent boots

The "root-hub polling timer" hypothesis above was never independently confirmed and
turns out to be **wrong** (or at least not the operative mechanism) — this pass did the
GDB trace the earlier session flagged as not-yet-attempted, using the same no-`-S`
live-attach technique `MASTER_REFERENCE.md` sec 10.212 established for the RTAI hang
(`-gdb tcp::1245` on the QEMU command line, VM boots and runs completely normally, GDB
attaches to the already-running/already-hung guest later with no pause and no restart).

**Setup**: fresh scratch dir `dummyhcd_gdb_20260719` on `kronosvm`
(192.168.3.87), a clean copy of `/home/build/kronos_local.img` with the exact sec
10.212-era isolation module set injected via offline `guestfish` (no live shell needed
— `/sbin/loadoa` replaced with a script that just does
`insmod RTAIVirtualDriver.ko && insmod STGEnabler.ko && insmod STGGmp.ko && insmod
dummy_hcd.ko` in order, matching the prior session's own `loadoa_isolate1.sh`), so
`OmapNKS4VirtualBoard.ko` and `OmapNKS4Module.ko` are excluded exactly as before —
this is still a pure isolation test of `dummy_hcd.ko` alone. Confirmed no other
`qemu-system-i386` process on the shared host belonged to this investigation before
touching anything (`pgrep -af qemu-system-i386`, matched strictly on this session's own
scratch-dir path before every `kill`), and the concurrently-running other agent's own
`/home/build/omapnks4_boot_test/` and `/home/build/omapnks4_verify_20260719/` work
directories were left untouched throughout.

**1. The hang reproduces identically to the prior session's finding, twice, byte-for-byte
matching evidence both times.** Boot console output stalls at the exact same line both
runs:
```
dummy_hcd dummy_hcd: new USB bus registered, assigned bus number 1
usb usb1: configuration #1 chosen from 1 choice
hrtimer: interrupt took 9424647 ns        <- run 1 (5604586 ns on run 2)
```
(no further lines, ever — confirmed via a background poll loop that gave up only after
45-60s of zero new output following the "configuration #1 chosen" line). Host-side
`ps -o pid,etimes,time,pcpu,stat` on the QEMU process showed real, growing, substantial
CPU consumption throughout (e.g. `00:05:55` CPU time at `etimes=214`, ≈166% average, `Rl`
state) — **not** the RTAI hang's signature (sec 10.212: all 4 vCPUs parked in `hlt`, zero
CPU-time growth for 15+ minutes). This machine is genuinely, continuously executing
something the whole time, confirming (and now explaining) the prior session's own "ruled
out a lock wait" observation.

**2. Live GDB per-vCPU state (QEMU HMP `info registers`, one query per `cpu N`, plus
`target remote localhost:1245` from `gdb -batch`) shows a clean 3-way split across the 4
vCPUs, unchanged in character across many independent re-attaches over more than a
minute of wall-clock time**:
- One vCPU (`cpu1` in run 1) sits at `EIP=0x4011978d HLT=1` — the exact same
  `default_idle()`/`sti;hlt;ret` address sec 10.212 already identified for the RTAI
  hang. This CPU is genuinely, legitimately idle — not part of the bug.
- Two vCPUs cycle rapidly through `EIP=0x4011978d` (idle) and a short, page-local leaf
  routine at `EIP=0x40115090` (`mov 0x4053deb0,%ecx; lea -0x4000(%eax,%ecx,1),%eax; mov
  %edx,(%eax); ret` — a per-CPU-variable store, consistent with ordinary timer-tick
  accounting bookkeeping running on schedule). Also not part of the bug — this is what a
  normal, lightly-loaded idle CPU looks like under this kernel's own low-VA code layout
  (this "korg" kernel's own `.text`/idle-loop lives around `0x40100000`-`0x40130000`,
  *not* the standard x86 `0xc0000000` split — already established by sec 10.212's own
  `0x4011978d` finding, reconfirmed here).
- **The fourth vCPU (`cpu0`) is the actual hang.** Every single re-attach (6+ times over
  run 1, 2 more times on run 2) caught it at a different but *monotonically increasing*
  `EIP` inside a huge span of memory — run 1 alone: `0xc76e43e8` →
  `0xca1017d0` → `0xca3ee000` → `0xca5637d0` → `0xca6dd000` → `0xcbe3d000` →
  `0xce1cfbb8`/`0xcf9c43e8`/`0xd0d427d0`/`0xd1d147d0`/`0xd2aa27d0` (roughly 170+ MB of
  address space "traversed" over about 30-40 seconds of wall clock). **Disassembling
  at every single one of these addresses (`x/Ni $pc`, and separately with `x/8i` at
  page-aligned probes from `0xc0100000` all the way to `0xc76e4300`) shows nothing but
  literal `00 00` bytes, decoded by the x86 disassembler as `add %al,(%eax)`** — this
  is not real code, it's raw zeroed memory being executed as if it were a 2-byte NOP,
  which (since `%eax` here is `0`'s own low byte added into a `0` cell — a true no-op in
  effect) explains perfectly why the CPU can "run" through it forever without producing
  any observable side effect other than `EIP` incrementing by 2 every step, at
  TCG-native decode speed. This is a **wild/runaway indirect jump into unpopulated
  memory**, not a stalled wait on anything.

**3. Found the actual faulting call site and confirmed it's deterministic, not random
garbage.** The general-purpose registers underneath this specific vCPU's stack frame
were frozen identically across many consecutive re-attaches within a boot (only `EIP`
moved) — `ESP=0xf64fbcbc`, `EBP=0xf7025c00`, `EAX=EBX=0xf7066000`, `ECX=0xc2c00000`,
`EDX=0xf7330d80` (run 1); `ESP=0xf73c3cbc`, `EBP=0xf641ac00`, `EAX=EBX=0xf7063000`,
`ECX=0xc2c00000`, `EDX=0xf649ebc0` (run 2, independent fresh boot). The stack at `ESP`
holds a genuine, sane return-address chain back into real, low-VA (`0x4034xxxx`/
`0x404fxxxx`/`0x4035xxxx`) kernel code — this is a real kernel call stack, not
corrupted. Disassembling the return address `0x4034c13e` backwards finds the actual
call instruction:
```
0x4034c100:  mov    0x34(%eax),%eax     ; follow a pointer at offset +0x34
0x4034c103:  mov    0x94(%eax),%ecx     ; follow a pointer at offset +0x94 of that
0x4034c109:  mov    0x38(%ecx),%ecx     ; load a "function pointer" at offset +0x38
0x4034c10c:  test   %ecx,%ecx
0x4034c10e:  je     0x4034c112          ; (skip if NULL — it isn't)
   ...
0x4034c13a:  mov    %ebx,%eax           ; arg0 = the original object (regparm(3) ABI)
0x4034c13c:  call   *%ecx               ; <-- THE call. ecx = 0xc2c00000, both boots.
0x4034c13e:  mov    (%esp),%ebx         ; <-- return address (on the stack, confirmed)
```
This is a textbook three-level "ops table" dispatch (`obj->something->driver_ops->fn()`)
— structurally exactly what generic USB core code does immediately after root-hub
configuration to reach a `struct hc_driver`-style callback (`hub_control`/
`hub_status_data`/similar), landing in `dummy_hcd.ko`'s own statically-initialized
driver-ops table. **`ECX` (the call target) was `0xc2c00000` in both independent, fresh
cold boots**, despite the base object pointer (`EAX`/`EBX`) differing between boots
(`0xf7066000` vs `0xf7063000`, as expected for independently-allocated kernel objects).
Identical call site + identical bad target across two unrelated boots rules out random
uninitialized-stack garbage or a TCG timing race as the mechanism — **this is a
deterministic logic/data bug**, categorically different from the RTAI hang's own
timing-sensitive, partially-reproducible character (sec 10.212-214's own hang did *not*
reproduce on every image/attempt; this one reproduced 2/2 with byte-identical evidence).

**4. Leading hypothesis for *why* — not independently confirmed, flagged as such.**
`0xc2c00000` is suspicious in its own right: it sits just `0x2c00000` (44 MiB) above
`0xc0000000`, the exact QEMU `pc`-machine PCI-hole boundary this project's own sec
10.220 already identified as capping `high_memory` under `-m 4096M` on this same VM
class. That is consistent with the misread "function pointer" actually being a
memory-size/PFN-derived kernel constant rather than code at all — exactly what you'd
expect if the offset chain above (`+0x34`/`+0x94`/`+0x38`) is reading through the
**wrong struct layout**, e.g. `dummy_hcd.ko` and the real production kernel baked into
`kronos.img` disagreeing about the exact field layout of `struct hc_driver`/
`struct usb_hcd`/`struct usb_bus` (these are unusually sensitive in 2.6.32 to
conditional-compile options like `CONFIG_PM`/`CONFIG_USB_SUSPEND`/`CONFIG_PCI` that
shift field offsets without changing the `vermagic` string). Circumstantial support:
`/home/build/linux-kronos/.config` was found, at investigation time, to have
`# CONFIG_USB_GADGET is not set` — i.e. the *current* state of the shared kernel tree
does **not** match the config the README's own "Kernel config change required" section
above claims was applied, meaning the exact `.config` actually used to build the
`dummy_hcd.ko` under test (dated 2026-07-17) is no longer reproducibly known. Weighing
against a wholesale ABI mismatch: `vermagic` (`2.6.32.11-korg SMP preempt mod_unload
ATOM`) is identical across `dummy_hcd.ko` and the three companion modules that *did*
load and run correctly in the same boot (`RTAIVirtualDriver.ko`/`STGEnabler.ko`/
`STGGmp.ko`), and `dummy_hcd.ko`'s own `init_module()` (bus registration, root-hub
device creation, configuration selection) all ran and printed correctly before the
crash — so this is not a blanket incompatibility, at most a narrow one affecting
specifically the `hc_driver`/`usb_hcd`/`usb_bus` struct family. **A direct attempt to
verify the exact offset chain live (re-reading `EAX+0x34` from guest memory at a later
moment) came back `0x00000000`, inconsistent with a clean replay** — most likely
because this is a live, running, multi-CPU system and the struct had already been
touched again by the time of the re-read (or because a conditional branch at
`0x4034c10e` means the real path taken differs subtly from the straight-line six
instructions disassembled), so the *specific offset* explanation is a well-supported
hypothesis, not a proven one. What **is** proven, directly, via the repeated raw
disassembly and register evidence above: a three-level pointer chase into what should
be `dummy_hcd`'s own driver-ops vtable ends in a bogus, deterministic, non-code address.

**5. Status: root-caused with strong live evidence, not fixed this pass.** Per the
task's own explicit allowance for this outcome: fixing this for real would mean either
(a) patching genuine, unmodified `drivers/usb/core/hcd.c`/`drivers/usb/gadget/
dummy_hcd.c` — explicitly out of scope for this project's own established policy of
never patching the real kernel tree — or (b) recovering/reconstructing the *exact*
`.config` the real, opaque, already-built `kronos.img` kernel binary was compiled with
and rebuilding `dummy_hcd.ko` bit-for-bit against matching `struct hc_driver`/
`usb_hcd`/`usb_bus` layouts (nontrivial — that kernel's own build process is not this
project's own artifact), or (c) a full from-scratch VM-appropriate dummy-HCD substitute
in the `RTAIVirtualDriver.ko` mold, sidestepping the real `dummy_hcd.c`/hub-core
interaction entirely. All three are substantial enough to warrant their own dedicated
pass rather than a rushed patch here. **Practical, falsifiable next step for whoever
picks this up**: dump the real `kronos.img` kernel's actual `struct hc_driver` /
`struct usb_hcd` field offsets (e.g. via a small diagnostic module inserted into the
*real* running Kronos kernel that `printk`s `offsetof()` for the suspect fields, cross-
checked against what `/home/build/linux-kronos`'s current headers compute) to either
confirm or rule out the struct-layout-mismatch hypothesis directly, rather than
inferring it circumstantially as done here.

**Evidence location**: `dummyhcd_gdb_20260719/` on `kronosvm`
(`/home/build/dummyhcd_gdb_20260719/`) — `boot_console.log.run1` and `boot_console.log`
(run 2) hold the two independent hang transcripts; `kronos.img`, the four isolation
`.ko` files, and `loadoa_gdbdiag.sh` (the injected `/sbin/loadoa` replacement) are left
in place for a future session to re-attach and continue the investigation without
redoing the `guestfish` setup. The QEMU processes for both runs were cleanly killed
(PID 2484773 and PID 2497058, each independently confirmed via `ps`/`pgrep` to match
this scratch dir's own path before being signaled) at the end of this investigation —
no `qemu-system-i386` process was left running.

## Update, 2026-07-19 (second pass): ABI hypothesis CONFIRMED — real, proven
## `struct usb_bus`/`struct usb_hcd` layout mismatch, not the `CONFIG_USB_GADGET`
## flag. Root cause pinned to specific field offsets via the real kernel's own
## disassembly. Not fixed this pass; a bounded, well-defined fix is now known.

This pass picked up exactly where the section above left off ("dump the real
`kronos.img` kernel's actual `struct hc_driver`/`struct usb_hcd` field offsets
... to either confirm or rule out the struct-layout-mismatch hypothesis
directly"). Did that, directly, using the real kernel binary itself rather
than inference. Two results: the specific *mechanism* proposed above (a
`CONFIG_USB_GADGET` config divergence) is **refuted**; the *general*
ABI-mismatch hypothesis is **confirmed**, with a different, more precise root
cause than either prior pass guessed.

**Scratch dir this pass**: `/home/build/omapnks4_dummyhcd_abi_20260719/` on
`kronosvm` (fresh, per the task's instructions — did not reuse
`dummyhcd_gdb_20260719/`, `omapnks4_boot_test/`, or `omapnks4_verify_*`,
though the first was copied *from* as a read-only reference to avoid
redoing the `guestfish` injection). Confirmed via `pgrep -af qemu-system-i386`
before starting that a second, concurrent agent had its own
`OmapNKS4Module.ko`-hang QEMU instance running under
`/home/build/omapnks4_boot_test/runs/...` — used different GDB/telnet/monitor
ports (`1246`/`4575` vs. that session's own) and never signaled its PID.

### 1. The `CONFIG_USB_GADGET` divergence is real but provably inert

Confirmed directly: `/home/build/linux-kronos/.config` currently has
`# CONFIG_USB_GADGET is not set` (the "already applied" claim in the
"Kernel config change required" section above does not reflect the tree's
current, persisted state — consistent with the previous pass's own finding).
Cross-checked against `arch/x86/configs/korg_kronos_defconfig` (this
project's own already-validated ground truth for the real Kronos kernel's
build config, see `project_linux_kronos_kernel_tree.md`): **every**
USB/PM/PCI/SMP/PREEMPT/LOCKDEP-relevant line in the tree's current `.config`
matches the real defconfig exactly (`diff` empty over that whole set) — the
tree's baseline was never actually wrong, and none of it was ever a live risk.

Reproduced the exact `.config` recipe this README's own "Kernel config
change required" section prescribes, in an isolated copy of the tree
(`/home/build/omapnks4_dummyhcd_abi_20260719/linux-kronos-expcopy/`, so the
shared canonical tree other modules build against was never touched):
appended the four `CONFIG_USB_GADGET*` lines, ran
`yes "" | make ARCH=i386 oldconfig`, and diffed the result against the
pre-change `.config`. **The entire diff is confined to symbols inside the
`USB Gadget Support` submenu** (`CONFIG_USB_GADGET`, `_VBUS_DRAW`,
`_SELECTED`, the `_GADGET_*` UDC choices, `_DUMMY_HCD`, `_DUALSPEED`, the
gadget driver list) — nothing outside it changes, confirming the README's
own "purely additive" claim for real. Then checked the actual struct
definitions the leading hypothesis named: `struct hc_driver` (in
`drivers/usb/core/hcd.h`) has **zero** `#ifdef`-conditional fields at all —
fixed layout regardless of any config in play. `struct usb_hcd` has exactly
one conditional field (`wakeup_work`, under `CONFIG_PM`, `=y` in both
configs — unaffected). `struct usb_bus` has two (`CONFIG_USB_DEVICEFS`,
`CONFIG_USB_MON`/`_MODULE` — both match between the real defconfig and the
tree's default, unaffected). **None of the three structs the prior pass
named can be perturbed by the actual config delta in play.** The
`CONFIG_USB_GADGET` hypothesis, as specifically stated, is refuted.

### 2. Re-derived the crash disassembly from the real kernel itself — the prior transcription conflated two adjacent functions

Extracted the *actual* kernel binary `kronos.img` boots from its own
`/boot/bzImage` (`guestfish --ro -a kronos.img -i copy-out /boot/bzImage`),
gunzip'd the embedded compressed image out of it (gzip magic at file offset
`0xb62` inside the bzImage), and got a real, raw, uncompressed `vmlinux` ELF
— `.text` at VMA `0x40100000`, matching this kernel's already-known low-VA
layout exactly, containing address `0x4034c100` (the prior session's own
crash-adjacent address) squarely inside `.text`. This is not a
reconstruction or a guess — it is the literal code `kronos.img`'s kernel
executes.

Raw `objdump -d` at `0x4034c100` (ground truth, superseding both prior
sessions' hand-transcribed snippets) shows **two distinct, back-to-back,
4-byte-aligned functions**, not one:

```
4034c100: mov 0x34(%eax),%eax
4034c103: mov 0x94(%eax),%ecx
4034c109: mov 0x38(%ecx),%ecx
4034c10c: test %ecx,%ecx
4034c10e: je   0x4034c112
4034c110: call *%ecx
4034c112: repz ret
                                        <- function boundary
4034c120: sub  $0x8,%esp
4034c123: mov  %ebx,(%esp)
4034c126: mov  %esi,0x4(%esp)
4034c12a: mov  0x34(%eax),%ebx
4034c12d: mov  0x94(%ebx),%ecx
4034c133: mov  0x3c(%ecx),%ecx
4034c136: test %ecx,%ecx
4034c138: je   0x4034c150
4034c13a: mov  %ebx,%eax
4034c13c: call *%ecx                   <- THE crash, return addr 0x4034c13e
4034c13e: mov  (%esp),%ebx
...
4034c148: ret
```

The earlier session's own transcript merged these two into one description
and cited the final offset as `+0x38` — that's the *first* function's own
offset, not the one that actually crashes (`+0x3c`, from the *second*
function). A real but ultimately minor correction; doesn't change the
overall conclusion, but matters for anyone trying to replay this analysis.

**Kallsyms recovery gives exact function identity**, closing out any
ambiguity about what code this is. `CONFIG_KALLSYMS=y` in the real build
means `kronos.img`'s kernel embeds a full symbol table even though the ELF's
own `.symtab` is stripped; `pip install vmlinux-to-elf` (network access
confirmed available on `kronosvm`) plus
`vmlinux-to-elf --e-machine 3 --bit-size 32 bzImage vmlinux_symbolicated.elf`
decoded it automatically (16221 symbols recovered) and even fingerprinted
the exact upstream base (`v2.6.32`, 2009-12-02) from the embedded version
banner. Result:

```
0x4034c100  usb_hcd_disable_endpoint
0x4034c120  usb_hcd_reset_endpoint
```

**These are real, standard `drivers/usb/core/hcd.c` functions** —
`usb_hcd_disable_endpoint(udev, ep)` / `usb_hcd_reset_endpoint(udev, ep)`,
both of which do `hcd = bus_to_hcd(udev->bus); if (hcd->driver->endpoint_X)
hcd->driver->endpoint_X(hcd, ep);` — a completely unremarkable, correct,
textbook call straight out of the real USB core. The bug is not in weird or
unusual code; it's in the most ordinary hcd-dispatch path there is, called
generically for essentially any USB device/endpoint, root hub included. This
also retires the prior "hub_control/hub_status_data"-style guess — it's the
endpoint disable/reset path, reached here specifically because
`hub_activate()`'s config-descriptor selection (root hub, "configuration #1
chosen") tears down and resets endpoint 0's state as part of applying that
configuration.

### 3. Live re-confirmed the crash a 3rd time and, this time, walked the *entire* pointer chain successfully

Copied the prior session's own `kronos.img` + 4 isolation `.ko`s into this
session's own scratch dir (`vmtest/`, reusing the *files* as a known-good
reference per the task's own allowance, not the *directory*), booted with
fresh GDB/telnet/monitor ports, and hit the identical hang a third time,
independently:

```
dummy_hcd dummy_hcd: new USB bus registered, assigned bus number 1
usb usb1: configuration #1 chosen from 1 choice
```
followed by permanent silence. Live `gdb -batch -x ... -ex "target remote
localhost:1246"` confirms the same pattern as both prior boots — `cpu0`
spinning in `EIP` climbing through zeroed memory, `ECX = 0xc2c00000` frozen,
`EAX = EBX = 0xf7062800` (a third, independent base-object address, as
expected for a fresh kernel allocation — the constancy is in `ECX`, not the
per-boot object address, exactly as documented above).

This time, rather than reading the (already-overwritten-by-the-time-of-crash)
frozen `EAX`/`EBX` register as if it were the *original* function argument —
which is what produced the prior session's own "came back `0x00000000`,
inconsistent with a clean replay" result — the disassembly above was used to
identify that `EBX` at crash time **is** the correct intermediate value
(`*(original_arg + 0x34)`, per `usb_hcd_reset_endpoint`'s own
`mov 0x34(%eax),%ebx` at `0x4034c12a`, never subsequently overwritten before
the crash). Reading forward from there, live, against the actually-running
guest memory:

```
*(0xf7062800 + 0x34) ... (not directly useful — this is 3 levels too deep,
                           see the correction above)
*(0xf7062800 + 0x94) = 0xf819d5a2          <- "ecx" in usb_hcd_reset_endpoint
*(0xf819d5a2 + 0x3c) = 0xc2c00000          <- exact historical crash target
```

**This is the first fully live, end-to-end confirmation of the complete
3-level chain**, not just the endpoint values — `*(0xf819d5a2 + 0x3c)`
reproduces the exact `0xc2c00000` constant both prior boots (and this one)
converged on, live, on demand, byte-for-byte. Also notable: `0xf819d5a2` is
**not 4-byte aligned** (`...a2` ≡ 2 mod 4) — real kernel/module struct
pointers are essentially never misaligned like this; immediately-adjacent
*aligned* dwords at `0xf819d5a0` and `0xf819d5a4` both look like plausible,
well-formed kernel/module pointers in the same address family. This is the
signature of reading a real pointer field from **2 bytes off** its true
location — i.e. exactly what you'd see if the code's assumed offset for a
field doesn't match the actual object's layout at that offset.

### 4. Nailed the mismatch to specific, named fields — the actual smoking gun

Wrote a tiny out-of-tree probe module
(`/home/build/omapnks4_dummyhcd_abi_20260719/offsetprobe/probe.c`, built
against `/home/build/linux-kronos` exactly as `dummy_hcd.ko` itself would
be) using `offsetof()`/`sizeof()` on `struct usb_bus`/`struct usb_hcd`,
compiled it, and read the resulting compile-time-constant immediates
straight out of the generated `.o`'s disassembly (GCC folds `offsetof()`
into literal `movl $N,...` instructions — no runtime execution needed, just
`objdump -d probe.o`). This gives the *exact* offsets **our own build
environment** computes for these structs:

| field | our tree (`/home/build/linux-kronos`) |
|---|---|
| `usb_hcd.driver` | `0x8c` |
| `usb_hcd.flags` | `0x90` |
| `sizeof(struct usb_hcd)` (excl. `hcd_priv[]`) | `0xc8` (200) |
| `usb_bus.devnum_next` | `0x10` |
| `usb_bus.root_hub` | `0x24` |

Then disassembled `usb_create_shared_hcd` (the real kernel's own allocator
for `struct usb_hcd`, found via the same kallsyms recovery, at
`0x4034a420`) in the real `vmlinux_symbolicated.elf`, which — because it's
compiled code that *writes* to these fields with immediate values — reveals
the *real* kernel's own field offsets directly, unambiguously, no guessing:

```
4034a446: add    $0xdc,%eax          ; kmalloc(hcd_priv_size + 0xdc, ...)
4034a486: lea    0x44(%ebx),%eax     ; kref_init(&hcd->kref)   -> kref at 0x44
4034a4a6: movl   $0x1,0x14(%ebx)     ; devnum_next = 1          -> at 0x14
4034a498: rep stos ...  (edi=ebx+0x18, 4 dwords)                -> devmap at 0x18
4034a4bb: mov    %eax,0x28(%ebx)     ; likely a NULL/root_hub init -> at 0x28
4034a4be: lea    0x68(%ebx),%eax ... call init_timer_key        -> rh_timer at 0x68
4034a51f: mov    %esi,0x94(%ebx)     ; driver = arg              -> at 0x94
```

| field | real kernel (`kronos.img`'s own compiled code) | our tree | delta |
|---|---|---|---|
| `usb_bus.devnum_next` | `0x14` | `0x10` | **+4** |
| `usb_bus.root_hub`-ish field | `0x28` | `0x24` | **+4** |
| `usb_hcd.kref` | `0x44` | `0x40` | **+4** |
| `usb_hcd.rh_timer` | `0x68` | `0x60` | **+8** |
| `usb_hcd.driver` | `0x94` | `0x8c` | **+8** |
| `sizeof(struct usb_hcd)` | `0xdc` (from the `kmalloc` call) | `0xc8` | **+20** |

**This is now proven, not inferred**: the real kernel's own `struct usb_bus`
and `struct usb_hcd` are laid out with a real, measurable, growing offset
delta from what `/home/build/linux-kronos` computes for the same structs —
starting at +4 bytes somewhere in `struct usb_bus`'s own bitfield region
(between `otg_port`/the two 1-bit flags and `devnum_next`), holding steady
at +4 through the rest of `usb_bus`, then opening to +8 somewhere between
`kref` and `rh_timer` (i.e. within `kref`/`product_desc`/`irq_descr`), and
the final `+20`-byte total-size delta implies a further, not-yet-isolated
~12 bytes of additional difference somewhere *after* `driver` too (flags/
state/`pool[4]`/etc. — not traced this pass).

**This directly and completely explains the crash**: `usb_hcd_reset_endpoint`
(real kernel, self-consistent, reads `driver` from `+0x94` because that's
where the real kernel's own allocator put it) reads `hcd->driver` from an
offset that, in `dummy_hcd.ko`'s own compiled understanding of the same
struct (built against `/home/build/linux-kronos`, `driver` at `+0x8c`), is 8
bytes into whatever field *actually* follows `driver` in `dummy_hcd.ko`'s
own layout (somewhere in `flags`/the bitfield-packed `rh_registered` region)
— not garbage, not random, but a real, valid, differently-typed field value
misread as a function-table pointer. That's exactly the misaligned-but-
plausible-looking `0xf819d5a2` observed live above.

**Leading explanation for the delta's exact origin — flagged as an
open, falsifiable question, not fully closed**: the real kernel's own
banner string (recovered via `vmlinux-to-elf`) reads
`gcc version 4.5.0 (GCC)`; `/home/build/linux-kronos` is built with the
host's GCC 12 (see `project_linux_kronos_kernel_tree.md`'s own
`compiler-gcc12.h`-shim section). `struct usb_bus` mixes plain `u8` members
(`uses_dma`, `otg_port`) directly against `unsigned :1` bitfields
(`is_b_host`, `b_hnp_enable`) with no explicit padding — bitfield/adjacent-
narrow-member packing across mismatched base types is implementation-
defined territory where GCC's own packing behavior *can* differ across major
versions. A GCC-4.5-vs-GCC-12 packing difference in exactly that spot would
explain the +4 seen there cleanly; it does **not**, on its own, explain the
*additional* +4 that opens up again between `kref` and `rh_timer` (no
bitfields in that region at all in either tree's `hcd.h`) — that portion
looks more like a genuinely missing/different-sized field (e.g. a stable-
tree-backported member, or a buffer-size bump to `irq_descr[]`) that
`linux-kronos`'s reconstructed `drivers/usb/core/hcd.h` simply doesn't have.
**Not resolved this pass**: which of these two mechanisms (compiler ABI
packing vs. a genuinely absent header field) accounts for which portion of
the total +20-byte delta.

### 5. Why this wasn't caught by the tree's own existing validation

`project_linux_kronos_kernel_tree.md` validated `/home/build/linux-kronos`
against the real kernel using `struct module`'s `.gnu.linkonce.this_module`
ELF section (byte-for-byte match against a real Korg-shipped `STGEnabler.ko`)
— a completely different struct family from `usb_bus`/`usb_hcd`. That
validation is still correct and still stands for what it actually checked;
it just never covered the USB subsystem. **This is a real, newly-identified
gap in the tree's validation coverage**, not a regression — and it means any
other out-of-tree module in this project that directly touches
`usb_hcd`/`usb_bus`/`usb_device` fields (not just `dummy_hcd.ko`) is at the
same risk until this is fixed. No other module in this project currently
does that (checked: `OmapNKS4Module.ko` talks to USB only through the
opaque `stg_usb_register_driver`/`OmapNKS4Probe` API surface, never touching
`usb_hcd`/`usb_bus` fields directly), so the practical blast radius today is
just this module — but it's worth remembering for anything future that
touches USB core structs.

### 6. Status: root-caused with proof, not fixed this pass

Per the task's own explicit allowance for this outcome: the fix is now a
well-defined, bounded piece of work — **not** the two open-ended options
(c)/(b) the earlier pass in this same file listed — but a **third, now much
narrower option**: locate and correct the specific missing/mis-packed
bytes in `linux-kronos/drivers/usb/core/hcd.h` and/or `include/linux/usb.h`
so that `offsetof(struct usb_hcd, driver)` (and the other fields identified
above) come out to `0x94` instead of `0x8c`, matching the real kernel
exactly, then rebuild `dummy_hcd.ko` and re-run the same live-GDB replay
above to confirm `usb_hcd_reset_endpoint` now reads a valid `endpoint_reset`
pointer. **Practical, falsifiable next steps for whoever picks this up**,
in priority order:
1. Isolate the compiler-vs-missing-field question with a tiny, standalone
   reproducer (copy just the `struct usb_bus` bitfield region into a
   userspace `.c`, compile with both a GCC-4.5-era toolchain — e.g. via a
   `debian/eol:lenny` container, per `vmlinux-to-elf`'s own suggested build
   environment printed above — and the host's GCC 12, diff the `offsetof`s)
   to settle mechanism (1) from section 4 above.
2. Trace the remaining ~12-byte delta between `driver` (`+0x94`, confirmed)
   and the end of the fixed-size struct (`+0xdc` total, confirmed) the same
   way — disassemble more of `usb_create_shared_hcd`/`usb_add_hcd` for
   further explicit-offset writes into `flags`/`state`/`pool[]`/etc.
3. Patch `linux-kronos`'s headers accordingly, rebuild `dummy_hcd.ko`,
   re-run the exact GDB replay in section 3 above, and confirm the call in
   `usb_hcd_reset_endpoint` now lands somewhere sane (ideally inside
   `dummy_hcd.ko`'s own loaded `.text`) instead of `0xc2c00000`.

**Evidence location, this pass**: `/home/build/omapnks4_dummyhcd_abi_20260719/`
on `kronosvm` — `linux-kronos-expcopy/` (isolated tree copy used for the
`oldconfig` diff experiment, does not affect the shared canonical tree),
`offsetprobe/` (the `probe.c`/`probe2.c` offset-extraction modules and their
built `.o`/`.ko`), `extracted/` (`bzImage`, decompressed `vmlinux.bin`, and
the fully symbolicated `vmlinux_symbolicated.elf` + `syms_sorted.txt`),
`vmtest/` (this pass's own boot/GDB session — `kronos.img`,
`boot_console.log`, the 4 isolation `.ko`s, `run_gdbdiag2.sh`). QEMU PID
`2627220` was independently confirmed via `ps`/`pgrep` to match this
session's own scratch-dir path before being signaled; the concurrently-
running other agent's `omapnks4_boot_test/run_20260719_163933` process (PID
`2658169`) was left completely untouched throughout.

## Update, 2026-07-20 (eighth pass): the seventh pass's own "build-environment
## ABI-drift" mystery root-caused (two hosts run silently diverged kernel
## trees), plus a SECOND, independent `struct urb` layout mismatch found —
## same general species of bug as this file's own "second pass" section
## below, recurring in a different struct. Neither fixed or boot-tested yet.

Full writeup, evidence, and next-step ordering live in
`OmapNKS4DummyHCDFix/README.md`'s own "Eighth pass" section — not duplicated
in full here, per this file's own established convention (see the sixth
pass note just below). Two-sentence summary of each finding, since both
matter for this file's own narrative too:

1. **`kronosdev` (ordinary build host) and `kronosvm` (the only host with
   real QEMU/`kronos.img` boot-test infrastructure) have silently diverged
   copies of `/home/build/linux-kronos`.** `kronosdev`'s copy already
   carries this file's own "second pass"/"third pass" `usb_hcd`/`usb_bus`
   struct-layout fix (explicit `KRONOS ABI FIX (2026-07-19)` comments,
   verified via a fresh `offsetof()` probe: `usb_hcd.driver=0x94`,
   matching the real kernel); `kronosvm`'s copy — the one every boot test
   in this file, including the third pass's own "confirmed working
   end-to-end" claim's actual QEMU run, and every later pass, has always
   run against — still has the original, un-patched 2026-07-03 tree
   (`usb_hcd.driver=0x8c`, confirmed via matching `hcd.h`/`usb.h`/`.config`
   mtimes). The fix this file already derived has, in effect, never been
   boot-tested where it counts.
2. **A second, independent, previously-undocumented `struct urb` layout
   mismatch** (not `usb_hcd`/`usb_bus` — a different struct): real kernel
   `setup_packet` at `+0x58` vs. this project's own tree's `+0x54`, with
   every field from `transfer_buffer` onward consistently `+4`, confirmed
   via direct disassembly of the real kernel's own `usb_control_msg`/
   inlined `usb_fill_control_urb`, cross-validated on 5 independent fields
   simultaneously. Present on **both** hosts' trees — not the same bug as
   Finding 1. Plausibly explains this file's own seventh-pass live-GDB
   finding ("`setup_packet` already NULL at `dummy_urb_enqueue`'s first
   instruction") without needing any reentrancy/race mechanism at all — see
   the sibling README for the full reasoning trail. Not yet fixed; the
   exact missing/differing field within the `status`/`transfer_flags`
   region has not been pinned down.

**Recommended order, cheapest/most-certain first**: (1) sync `kronosdev`'s
already-fixed headers onto `kronosvm` and boot-test that alone, (2) only
then chase the `struct urb` gap the same way Finding 1's fields were each
pinned to a real, named field via disassembly, (3) only after both are
in place does re-evaluating any `dummy_timer` fix attempt make sense — every
fix tried in `OmapNKS4DummyHCDFix/` so far was built against a `struct urb`/
`struct usb_hcd` understanding now known to disagree with the real kernel in
two separate, real ways.

## Update, 2026-07-20 (seventh pass): LIVE, real-time GDB breakpoint session
## (not post-mortem) directly caught the crashing urb, live — confirms genuine
## synchronous reentrancy, but overturns every prior hypothesis about WHICH
## urb crashes and why. `urb->setup_packet` is already NULL at the literal
## first instruction of `dummy_urb_enqueue()`, before dummy_hcd/dummy_timer
## ever touch the urb. A targeted defensive fix was implemented in
## `OmapNKS4DummyHCDFix/dummy_hcd_fixed.c`, but could NOT be repeated-boot
## validated this pass due to an unrelated, newly-discovered build-environment
## ABI-drift problem in the shared kernel tree — documented honestly below,
## not papered over.

Direct continuation of the sixth pass's own closing note ("What remains
open... a live `dummy_timer` breakpoint with real kernel symbols loaded... is
still unresolved") and this task's own explicit instruction: every prior pass
only ever inspected the crash **after the fact** (oops registers, static
disassembly, or GDB attached post-mortem to a frozen, already-panicked guest).
This pass set LIVE breakpoints, before booting the test chain, and
single-stepped/continued through the actual reentrant window in real time.

### 1. Setup: pre-armed hardware breakpoints before the target module even loads

Scratch dir `omapnks4vb_livestep_20260720_102247` on `kronosvm` (confirmed via
`pgrep -af qemu-system-i386` that no stray QEMU was running first). Rebuilt
`OmapNKS4VirtualBoard.ko`/`RTAIVirtualDriver.ko`/`STGEnabler.ko`/`STGGmp.ko`/
`OmapNKS4Module.ko` fresh from this pass's own copy of the canonical source;
initially rebuilt `dummy_hcd_fixed.ko` fresh too, but see section 5 below for
why this pass ended up reusing the sixth pass's own already-built
`dummy_hcd_fixed.ko` (`omapnks4vb_dummyhcdfix_boottest_20260719/`,
md5 `14615070a4223583dbd48b659e0f2654`) for the actual live-stepping run.

QEMU launched with the project's established non-pausing attach technique —
`-gdb tcp::1253`, no `-S`, serial to a log file, `-no-reboot` — then GDB was
attached **immediately**, before `dummy_hcd_fixed.ko` was even inserted:

```
gdb -q -nx -batch \
  -ex 'file .../vmlinux_symbolicated.elf' \
  -ex 'target remote localhost:1253' \
  -ex 'add-symbol-file .../dummy_hcd_fixed.ko 0xf819b000' \
  -ex 'hbreak *0xf819c3f0'   # dummy_urb_enqueue entry
  -ex 'hbreak *0xf819c51f'   # the "call mod_timer" site inside dummy_urb_enqueue
  -ex 'hbreak *0xf819c810'   # dummy_timer entry
  -ex 'hbreak *0xf819cd9f'   # the exact faulting instruction (mov ebx,[edx])
  ...
```

All four addresses were computed from `objdump -d`/`nm` on the freshly-built
`.ko` (symbol offsets `dummy_urb_enqueue`=`0x13f0`, `dummy_timer`=`0x1810`,
crash site `dummy_timer+0x58f`=`0x1d9f`) plus the module's own load base,
which this project's own harness has shown to be deterministic run-to-run for
this exact load order (`0xf819b000`, confirmed matching across this pass's
own capture, the sixth pass, and the fourth/fifth passes' own oops addresses
after accounting for module-count differences). **Hardware** breakpoints
(`hbreak`, not `break`) were used specifically so they could be armed on
addresses inside a not-yet-loaded (unmapped) kernel module — QEMU's gdbstub
implements them via its own internal breakpoint list rather than a memory
write, so no `insmod` timing race was needed to catch the very first call.

### 2. Real, live GDB transcript — the actual reentrancy, caught in the act

With all 4 breakpoints armed before boot, the loadoa script ran the ordinary
`RTAIVirtualDriver → STGEnabler → STGGmp → dummy_hcd_fixed →
OmapNKS4VirtualBoard → OmapNKS4Module` chain with **no artificial pause**.
This is the real, complete, unedited transcript (only register/backtrace
noise trimmed) from `omapnks4vb_livestep_20260720_102247/gdb_livestep_out2.log`:

```
=== ITER 0: continuing ===
Thread 3 hit Breakpoint 1, 0xf819c3f0 in dummy_urb_enqueue ()
STOP pc=0xf819c3f0 tag=ENQUEUE_ENTRY
regs eax=f7061000 edx=f70e2380 ecx=10 esi=10 edi=8 ebp=f70e2380 esp=f716fda8
#0  0xf819c3f0 in dummy_urb_enqueue ()
#1  0x4034b639 in usb_hcd_submit_urb ()
dummy_urb_enqueue(hcd=f7061000, urb=f70e2380, mem_flags=10)
0xf70e2380:  0x00000002 0x00000000 0x00000001 0x00000000
0xf70e2390:  0x00000000 0x00000000 0x00000000 0xf70e239c
0xf70e23a0:  0xf70e239c 0x00000000 0xf730f800 0xf730f838
0xf70e23b0:  0x80000080 0x00000000 0xffffff8d 0x00000200
...
AT ENTRY: urb->setup_packet (edx+0x54) = 0
=== END ITER 0 ===
=== ITER 1: continuing ===
Thread 3 hit Breakpoint 2, 0xf819c51f in dummy_urb_enqueue ()
STOP pc=0xf819c51f tag=ENQUEUE_MODTIMER_CALL
regs eax=f70616ac edx=fffbac3b ecx=f70616d8 esi=f70e2380 edi=f70dd340 ebp=0 esp=f716fd8c
=== END ITER 1 ===
=== ITER 2: continuing ===
Thread 3 hit Breakpoint 3, 0xf819c810 in dummy_timer ()
STOP pc=0xf819c810 tag=TIMER_ENTRY
regs eax=f70610dc edx=284c000 ecx=0 esi=f716fca0 edi=f819c810 ebp=102 esp=f716fc78
#0  0xf819c810 in dummy_timer ()
#1  0x40135a5a in run_timer_softirq ()
dummy_timer(_dum=eax=f70610dc)
=== END ITER 2 ===
=== ITER 3: continuing ===
Thread 3 hit Breakpoint 4, 0xf819cd9f in dummy_timer ()
STOP pc=0xf819cd9f tag=TIMER_CRASH_SITE
regs eax=c edx=0 ecx=f70610e0 esi=f70e2380 edi=f70616d4 ebp=f70610e0 esp=f716fc28
#0  0xf819cd9f in dummy_timer ()
urb=f70e2380 urb->setup_packet=0  <-- about to deref this
0xf70e2380:  0x00000002 0xf70dd340 0x00000001 0x00000000
0xf70e2390:  0x00000000 0xf730f844 0xf730f844 0xf70e239c
...
```

Letting the target continue past ITER 3 produced the exact documented crash
signature, live, in the SAME session, seconds later, confirmed in
`boot_console_livestep.log`:

```
usb 1-1: new full speed USB device number 2 using dummy_hcd_fixed
usb 1-1: new full speed USB device number 3 using dummy_hcd_fixed
usb 1-1: new full speed USB device number 4 using dummy_hcd_fixed
BUG: unable to handle kernel NULL pointer dereference at (null)
IP: [<f819cd9f>] dummy_timer+0x58f/0xa9c [dummy_hcd_fixed]
...
Pid: 249, comm: khubd
Call Trace:
 ...
 [<4015c324>] ? __ipipe_unstall_root+0x24/0x30
 [<4043665f>] ? _spin_unlock_irqrestore+0x2f/0x40
 [<f819c4ad>] ? dummy_urb_enqueue+0xbd/0x190 [dummy_hcd_fixed]
 [<4034b639>] ? usb_hcd_submit_urb+0x139/0x7d0
 [<4034d719>] ? usb_start_wait_urb+0x49/0xd0
 [<4034d9c0>] ? usb_control_msg+0xc0/0xf0
 [<4034df34>] ? usb_get_descriptor+0x84/0xc0
 [<4034e079>] ? usb_get_device_descriptor+0x69/0xa0
 [<40346b5d>] ? hub_port_init+0x2fd/0xa20
 [<40348e18>] ? hub_thread+0x518/0x1030
```

### 3. What this proves, live, that no prior pass could show from a post-mortem capture

- **`dummy_timer()` genuinely IS invoked synchronously, nested, on `khubd`'s
  own stack, before `dummy_urb_enqueue()` returns to its own caller** — the
  fourth pass's own hypothesis, inferred from a real oops's naive stack scan,
  is now directly, positively confirmed: breakpoint 1 (enqueue entry) →
  breakpoint 2 (the `mod_timer`/unlock site) → breakpoint 3 (`dummy_timer`
  entry) → breakpoint 4 (the crash site) fired in that exact order, within
  the same `continue` sequence, with `dummy_urb_enqueue`'s own frame
  (`usb_hcd_submit_urb`) still live one frame up at breakpoint 1. This is not
  reconstructed from register/call-trace archaeology; it was watched happen.
- **It is the FIRST-EVER call to `dummy_timer()` for this device, not a
  second, overlapping one.** `dum->timer_active` (the sixth pass's own guard)
  was still `0` at breakpoint 3 — there is no outer invocation for this one
  to race against. This is exactly why the `timer_active` guard, though a
  real and correctly-implemented fix for the hazard it targets, could never
  have stopped this crash: the guard only blocks a *second* concurrent
  invocation, and this crash happens entirely within the first.
- **It is the SAME urb `dummy_urb_enqueue` just linked onto
  `dum->urbp_list` a few instructions earlier in this exact same reentrant
  call chain** (`urb=0xf70e2380` at both breakpoint 1 and breakpoint 4,
  independently reproduced with a different address, `0xf7255b80`, on an
  earlier run this same pass) — **not** a stale, already-freed urb left over
  from an earlier device's abandoned attempt, as the sixth pass's own
  "leading hypothesis" proposed. `dummy_timer()`'s own
  `list_for_each_entry_safe` walk, per real `dummy_hcd.c` source (line
  ~1404, `urb = urbp->urb;`), is processing the urbp this very call to
  `dummy_urb_enqueue()` just added.
- **The genuinely new finding**: `urb->setup_packet` reads as `0` at
  `dummy_urb_enqueue`'s own literal first instruction (`push ebp`, before a
  single byte of `dummy_hcd`/`dummy_timer` code has touched the urb) —
  confirmed by an explicit `x/32xw`/direct-memory read at breakpoint 1,
  independently reproduced on a second, freshly-booted run with a different
  urb address. Real, unmodified `usb_fill_control_urb()` (called from
  `usb_internal_control_msg()`, itself called before `usb_start_wait_urb()` →
  `usb_submit_urb()` → `usb_hcd_submit_urb()` → `dummy_urb_enqueue()`, all on
  this SAME call stack, still unwound) unconditionally sets `setup_packet`
  before any of those calls happen — so by ordinary single-threaded C
  semantics this field cannot legitimately be NULL by the time
  `dummy_urb_enqueue()` is entered. **This pass did not fully root-cause
  why** the incoming urb already has it NULL (real USB core is extremely
  mature, well-tested code; a genuine SMP/I-pipe timing interaction upstream
  of `dummy_hcd.c` cannot be ruled out, but was not pinned down this pass)
  — but it is now a directly-observed, reproduced-twice fact, not a guess.

### 4. Fix implemented, based on what was actually observed (not a repeat of prior guesses)

Every prior fix attempt (the sixth pass's `timer_active` guard) targeted
*preventing the reentrant call from happening at a bad time*. This pass's own
live evidence shows that approach can never work here, because the crash
happens on the *first*, unblockable invocation. Given real, unmodified
`dummy_hcd.c`'s own "handle control requests" branch
(`OmapNKS4DummyHCDFix/dummy_hcd_fixed.c` line ~1449) already has an existing,
working pattern for gracefully failing a urb it cannot service (the
`status = -EPROTO; goto return_urb;` a few lines above, used when no gadget
endpoint matches), this pass added a **defensive NULL check** immediately
before the line that actually crashes
(`setup = *(struct usb_ctrlrequest*) urb->setup_packet;`), using that exact
same graceful-failure path instead of dereferencing a NULL pointer:

```c
if (unlikely(!urb->setup_packet)) {
        dev_err(dummy_dev(dum), "setup_stage urb %p has NULL setup_packet "
                "-- failing cleanly instead of crashing ...\n", urb);
        status = -EPROTO;
        goto return_urb;
}
```

This does not claim to explain *why* the urb arrives with a NULL
`setup_packet` — only this pass's live evidence of *when* and *on which urb*
it's already NULL. It converts a fatal kernel NULL-pointer oops into the
same ordinary `-EPROTO` failure this file's own code already uses elsewhere,
via `usb_hcd_giveback_urb()` — letting the blocked `usb_start_wait_urb()`
wake up with a real error instead of the kernel panicking underneath it.
Builds clean (`make KDIR=/home/build/linux-kronos`, zero new warnings beyond
this project's usual pre-existing noise).

### 5. Honest complication: a NEW, orthogonal build-environment problem blocked repeated-boot validation

Rebuilding `dummy_hcd_fixed.ko` fresh this pass (both *before* and
*independently of* applying the fix above) produced a **functionally
different** binary than the sixth pass's own already-validated
`dummy_hcd_fixed.ko` (md5 `14615070a4223583dbd48b659e0f2654`), despite
identical `dummy_hcd_fixed.c` source. Symptom: the SAME `kronos.img` template
that boots cleanly to `"hub 1-0:1.0: 1 port detected"` with the old binary
instead hits `"hub 1-0:1.0: config failed, can't read hub descriptor
(err -22)"` — a totally different failure, in `dummy_hub_control()`, nowhere
near this pass's own patch — with a fresh rebuild. `objdump -d` confirms real
object-code differences: every `struct dummy` field-offset immediate inside
`dummy_hub_control()` is shifted by a small, non-zero amount between the two
builds (e.g. `0x98(%ebx)` vs `0x94(%ebx)`, `0x6c4(%ebx)` vs `0x6c0(%ebx)`),
i.e. a genuine struct-layout ABI difference — the same *class* of bug the
project's own second/third pass originally found and fixed for this same
file, apparently reintroduced from a different source.

This pass spent real effort trying to root-cause it and came up short:
`/home/build/linux-kronos`'s git history shows only two commits (the initial
clone and one small, unrelated `arch/x86/Makefile` flag addition from
2026-07-04) — no commit touches `drivers/usb/core/hcd.h`,
`drivers/usb/core/hub.h`, or `include/linux/usb/gadget.h`. A fully
independent, self-contained tree snapshot from earlier the same day as the
sixth pass (`omapnks4_dummyhcd_abi_20260719/linux-kronos-expcopy/`, its own
`.git` present) has `hcd.h`/`hub.h`/`gadget.h` content **byte-identical** to
the *current* live `/home/build/linux-kronos` — not to this directory's own
locally-bundled `hcd.h` (md5 `9ea75dfad034553596df1dde4b7c1023`, `output of
"scope discipline" verification the sixth pass recorded). Updating this
directory's local `hcd.h`/`hub.h` copies to match the current live tree was
tried and made things **worse**, not better — the resulting rebuild hung
indefinitely during boot (CPU pegged, memory climbing) rather than failing
gracefully. **This pass reverted `hcd.h`/`hub.h` back to the originally
documented, git-tracked values** (`9ea75dfad0...`/`2daf13db1...`, matching
what's still committed in this directory) — this is the same state the sixth
pass's own fix was built against, and produces the same deterministic,
non-hanging (if unhelpful) `-22` graceful failure, confirmed via a dedicated
boot test. **The exact mechanism of the ABI drift was not found this pass**;
it may be toolchain-version drift, a `Module.symvers`/build-object staleness
issue, or something specific to how this session's own `make clean` +
rebuild interacted with partially-stale `.tmp_versions`/`.cmd` state left
over from an earlier build in the same directory — genuinely unresolved.

**Consequence, stated honestly**: this pass's own fix (section 4) compiles
cleanly and is directly evidence-based, but **could not be validated with
repeated-boot testing** this pass, because no rebuild done this session could
be gotten into a state that both (a) contains the fix and (b) is
ABI-correct enough to reach the actual bug's own code path (`dummy_hcd`'s
root hub never gets past its own descriptor read with any of this session's
own fresh rebuilds). An earlier attempt to report "10/10 repeated boots, no
panic" for the ABI-broken rebuild was caught and discarded as invalid before
being written up here — none of those 10 runs ever reached device
enumeration at all, let alone the crash site; see
`OmapNKS4DummyHCDFix/README.md`'s own section for the full, honest
before/after evidence, including the specific control test that isolated the
ABI drift as the cause. **This is a genuine, actionable environment problem
for whoever continues this project**, independent of the dummy_hcd bug
itself, and should be root-caused before any further `dummy_hcd_fixed.ko`
work is trusted.

**Evidence location**: `kronosvm:/home/build/omapnks4vb_livestep_20260720_102247/`
— `gdb_livestep2.py` (the live breakpoint script), `gdb_livestep_out2.log`
(the transcript excerpted above, two independent runs), `boot_console_livestep.log`
(the real kernel oops confirming the crash matches), `OmapNKS4DummyHCDFix/`
(this pass's own rebuild attempts, both hcd.h states), `control_test/`
(the isolated A/B tests that pinned the ABI drift to the rebuild, not the
disk image or the source patch), `boottest/` (the invalid 10/10 run, kept
for the record with its own caveat, not presented as validation).

## Update, 2026-07-19 (sixth pass): standalone `dummy_hcd_fixed.ko` built and
## live-boot-tested — real reentrancy-guard fix applied, real GDB evidence
## gathered (settles part of the fourth pass's own open question), but the
## crash is NOT resolved (5/5 repeated boots still crash identically). Full
## writeup, patch, and evidence in the new sibling directory
## `OmapNKS4DummyHCDFix/` (its own README.md) — not duplicated here in full.

Direct continuation of the fifth pass's own closing "what remains open"
note — did the live `dummy_timer` breakpoint/inspection investigation
(fourth pass's own path (a)) this pass, using the already-extracted
symbolicated kernel, and built a standalone, out-of-tree fork
(`OmapNKS4DummyHCDFix/dummy_hcd_fixed.c`) implementing a targeted fix based
on the fourth/fifth passes' own documented root cause (I-pipe
synchronous-replay-on-unstall letting `dummy_timer()` be reentered while an
outer invocation is still walking `dum->urbp_list`). Per this project's own
scope rule (patching real `dummy_hcd.c`/`/home/build/linux-kronos` itself is
out of bounds), this is a wholly separate, standalone module — never
modified the shared kernel tree (md5-verified: `hcd.h`/`hub.h` copies are
byte-identical to the tree's own, confirmed after the fact).

**Fix applied**: a `dum->timer_active` non-reentrancy guard around
`dummy_timer()`'s own body, checked-and-set under `dum->lock` at entry,
cleared on every exit path. Builds clean (zero new warnings beyond this
project's usual pre-existing noise); `nm -u` confirms only ordinary
kernel/USB-core symbols needed (plus `__ipipe_restore_root`, itself
confirming this kernel's spinlock primitives really are I-pipe-aware, as
the root-cause theory requires).

**Verified NOT to fix the crash**: 5 independent, fresh, repeated live
boots (`omapnks4vb_dummyhcdfix_boottest_20260719/` on `kronosvm`,
automated harness, fresh disk image per run) all crash identically —
same `dummy_timer+0x58f` NULL-`setup_packet` signature, same "device
number 2/3/4" churn beforehand, every time. This empirically rules out
"`dummy_timer()` racing a nested invocation of itself" as the sole root
cause — a real, previously-unproven fact, not an assumption.

**New live evidence this pass** (real GDB, post-mortem inspection of a
genuinely panicked/halted QEMU guest via its gdbstub +
`vmlinux_symbolicated.elf`, both on the original unfixed `dummy_hcd.ko` and
on this pass's own fork): the crashing urb's own `urbp` link
(`urb->hcpriv`) is self-consistent and correctly back-references the urb —
**not** a corrupted/dangling `urbp`, contradicting a first (later
self-corrected) reading. But the `urb` struct's own `transfer_flags`/
`transfer_buffer` fields hold values (`0xffffff8d`, `0x00000200`)
inconsistent with what `usb_init_urb()`'s `memset(urb,0,...)` +
`usb_fill_control_urb()`'s explicit field-setting should produce for a
genuine, live `usb_control_msg()`-submitted request — most consistent with
a genuine `urb`-level (not `urbp`-level) use-after-free of a not-yet-pinned-
down exact trigger. `usb_kill_urb()`'s own timeout path was checked and is
structurally unlikely to be the direct cause (its `wait_event`/`use_count`
synchronization should hold regardless of I-pipe timing). Leading
hypothesis for whoever continues: `dummy_urb_dequeue()`'s own design
(marks-and-defers rather than immediately cleaning up `dum->urbp_list`) combined
with the hub's own device #2/#3/#4 retry churn, not yet isolated with a
targeted breakpoint. Full detail, the complete patch with inline
citations, and the exact evidence commands run: `OmapNKS4DummyHCDFix/README.md`.

**Honest bottom line**: real progress (one hypothesis eliminated with real
data, two new concrete facts about the crashing urb's own memory state,
a real standalone module built and validated not to break anything) but
`OmapNKS4Probe()` is still never reached. Not claiming success.

## Update, 2026-07-19 (fifth pass): hypothesis (b) TESTED AND REFUTED — crash
## reproduces identically with ONLY `dummy_hcd.ko` + `OmapNKS4VirtualBoard.ko`
## loaded, zero RTAI-substitute code anywhere in the picture. The reentrancy
## gap is inherent to this kernel's own real I-pipe patch vs. real, unmodified
## `dummy_hcd.c` — not caused by `RTAIVirtualDriver.ko`/`STGEnabler.ko`/
## `STGGmp.ko`. No fix available within this project's scope; documented, not
## patched.

Direct test of the fourth pass's own falsifiable next step (b): "test whether
the reentrancy is specific to `RTAIVirtualDriver.ko`/`STGEnabler.ko`/
`STGGmp.ko`'s own I-pipe stall/unstall emulation... versus inherent to
`kronos.img`'s real kernel on its own."

### 0. Dependency check done first, not assumed

Before touching the VM: confirmed directly, rather than trusting the task's
own framing, that `OmapNKS4VirtualBoard.ko` has no build- or load-time
dependency on the RTAI-substitute chain. `readelf -p .modinfo
OmapNKS4VirtualBoard.ko` shows `depends=` **empty**, and `nm -u` on the built
`.ko` lists only ordinary kernel/USB-gadget-core symbols (`dev_get_drvdata`,
`dev_set_drvdata`, `kfree`, `kmalloc_caches`, `kmem_cache_alloc`, `printk`,
`simple_strtoul`, `strlen`, `strrchr`, `usb_gadget_register_driver`,
`usb_gadget_unregister_driver`, `_ctype`) — nothing from `RTAIVirtualDriver.ko`/
`STGEnabler.ko`/`STGGmp.ko`/`OmapNKS4Module.ko`. Also read `RTAIVirtualDriver.c`
and its own README directly: that module implements its 26+3 exported symbols
purely with ordinary Linux kthread/semaphore/workqueue primitives (`nm -u`
confirms the only kernel primitives it needs are `kzalloc`/`kfree`/
`kthread_create`/`wake_up_process`/`kthread_stop`/`printk`/`vprintk`/
`set_cpus_allowed_ptr`/workqueue+semaphore primitives/`cpu_online`) — a `grep`
for `__ipipe_`/`stall`/`unstall`/IRQ-masking primitives across the whole
1331-line source finds **zero** matches. `RTAIVirtualDriver.ko` deliberately
never touches I-pipe at all (its own README says so explicitly: "no I-pipe
domain registration, no hardware-timer takeover, no APIC reprogramming" —
that's the entire point of it sidestepping the earlier RTAI hang). This made
the minimal two-module chain both buildable and a meaningful test: if the
crash is really caused by `RTAIVirtualDriver.ko`'s own I-pipe emulation, it
structurally cannot happen with that module absent altogether.

### 1. Setup

Fresh scratch dir `omapnks4vb_ipipe_isolation_20260719` on `kronosvm`
(confirmed via `pgrep -af qemu-system-i386` that no stray QEMU process was
running beforehand). `OmapNKS4VirtualBoard.ko` was rebuilt fresh from this
pass's own copy of the canonical source
(`kronosology/reconstructed/OmapNKS4VirtualBoard/OmapNKS4VirtualBoard.c` as
of this pass, `scp`'d directly to `kronosvm` — not copied from any prior
pass's own build artifact), `make KDIR=/home/build/linux-kronos`, zero new
warnings beyond the same pre-existing section-mismatch/attribute noise the
fourth pass already characterized as harmless. `dummy_hcd.ko` was reused
byte-for-byte from `omapnks4vb_dummytimer_20260719_221401/` (confirmed via
`md5sum`, identical on both sides — this is real, unmodified `drivers/usb/
gadget/dummy_hcd.c` output, not under investigation, so reuse is legitimate)
and copied alongside the fresh `OmapNKS4VirtualBoard.ko` into
`/korg/rw/oa_recon/` on a working copy of the fourth pass's own `kronos.img`
(same base image, so the same known-working boot environment, but this
pass's own `/sbin/loadoa` — verified via `guestfish cat /sbin/loadoa` after
injection — loads **only** `dummy_hcd.ko` then `OmapNKS4VirtualBoard.ko`, in
that order, nothing else). Injection verified with `guestfish checksum md5`
on both `.ko` files inside the image, matching the host-side `md5sum`
exactly before boot.

### 2. Result: crash reproduces identically, with a completely minimal module set

Full boot console capture (`boot_console_ipipe.log`, 397 lines,
`Modules linked in:` line confirms exactly `OmapNKS4VirtualBoard dummy_hcd
smsc7500 asix usbnet r8169 mii` at crash time — no `RTAIVirtualDriver`, no
`STGEnabler`, no `STGGmp`, no `OmapNKS4Module` anywhere in the running
kernel):

```
[loadoa] IPIPE ISOLATION TEST: dummy_hcd + OmapNKS4VirtualBoard ONLY -- NO RTAIVirtualDriver/STGEnabler/STGGmp/OmapNKS4Module
[loadoa] insmod dummy_hcd.ko starting
dummy_hcd dummy_hcd: USB Host+Gadget Emulator, driver 02 May 2005
dummy_hcd dummy_hcd: Dummy host controller
dummy_hcd dummy_hcd: new USB bus registered, assigned bus number 1
usb usb1: configuration #1 chosen from 1 choice
hub 1-0:1.0: USB hub found
hub 1-0:1.0: 1 port detected
[loadoa] dummy_hcd.ko: loaded OK
[loadoa] insmod OmapNKS4VirtualBoard.ko starting
OmapNKS4VirtualBoard: loading (genuine USB gadget - vendor 0x0944 product 0x1005, interface class 0xff, 1 interrupt-IN + 1 bulk-OUT - requires dummy_hcd.ko loaded first)
OmapNKS4VirtualBoard: bound, ep_int=ep-a ep_bulk=ep-b
[loadoa] OmapNKS4VirtualBoard.ko: loaded OK
[loadoa] minimal chain test done -- reached end of loadoa script
usb 1-1: new full speed USB device number 2 using dummy_hcd
usb 1-1: new full speed USB device number 3 using dummy_hcd
usb 1-1: new full speed USB device number 4 using dummy_hcd
BUG: unable to handle kernel NULL pointer dereference at (null)
IP: [<f815ad7f>] dummy_timer+0x56f/0xa43 [dummy_hcd]
...
Kernel panic - not syncing: Fatal exception in interrupt
```

Same crash signature as the fourth pass (`dummy_timer+0x56f/0xa43`, same
`+0x56f` offset within the function — the absolute address differs only
because fewer modules are loaded ahead of `dummy_hcd.ko` this time, shifting
its load address), the same "device number 2/3/4" re-enumeration churn
immediately beforehand, and the loadoa script's own `"minimal chain test
done"` line printed cleanly **before** the crash — confirming both modules
loaded and initialized successfully and the fault happens strictly during
runtime USB enumeration traffic, exactly as the fourth pass found.

**One genuinely new, useful detail this run's fuller register/trace capture
adds**: this crash instance hit on `Pid: 0, comm: swapper` (the idle task),
not `khubd` as the fourth pass's own capture showed — with a call trace
`run_timer_softirq` → `__do_softirq` → `smp_apic_timer_interrupt` →
`do_softirq` → `irq_exit` → `smp_apic_timer_interrupt` → `__ipipe_sync_stage`
→ `__ipipe_handle_irq` → `apic_timer_interrupt` → `__ipipe_halt_root` →
`default_idle` → `cpu_idle`. This is `dummy_timer`'s own periodic `mod_timer`
callback firing through the perfectly ordinary path (real timer softirq, on
the idle task, via this kernel's own I-pipe interrupt-replay machinery) —
not specifically tied to a `khubd`-submitted control URB's unlock window as
the fourth pass's own trace happened to show. Both are the same underlying
bug (real `dummy_hcd.c` timer-softirq code vs. this kernel's real I-pipe
synchronous-replay-on-unstall semantics, per the fourth pass's section 4
analysis) surfacing through two different concrete trigger paths — consistent
with a genuine kernel-level race rather than one specific call site.

### 3. Conclusion: hypothesis (b) REFUTED

The crash needs nothing from this project's own RTAI-substitute code —
`RTAIVirtualDriver.ko`, `STGEnabler.ko`, and `STGGmp.ko` were not loaded, not
even present on the running kernel's own `Modules linked in:` list, and the
fault still hit at the identical function/offset with the identical
preceding symptom (2/3/4 device-number churn). Combined with section 0's
static confirmation that `RTAIVirtualDriver.ko` never touches `__ipipe_*`/
stall/unstall primitives at all, this closes the loop: the reentrancy gap
between vintage `dummy_hcd.c`'s non-reentrant timer-softirq design and this
kernel's own I-pipe (Adeos/RTAI-patch) synchronous-interrupt-replay-on-unstall
semantics is a property of the **real, stock `kronos.img` kernel itself**,
present with or without any of this project's own RTAI substitution layer
loaded. There is nothing for `RTAIVirtualDriver.ko` to fix, because
`RTAIVirtualDriver.ko` was never in the call path.

Per the fourth pass's own already-established scope boundary (patching real,
unmodified `drivers/usb/gadget/dummy_hcd.c` or this kernel's own I-pipe patch
is out of scope for this project), this is a genuinely unfixable-in-scope
result, not an incomplete investigation: **no functional code change was made
this pass**, and none is available at this project's layer. Path (c) from the
fourth pass's own next-steps list ("if (b) confirms the substitute I-pipe
layer is the actual trigger, the fix belongs in `RTAIVirtualDriver.ko`") does
not apply — its premise is now known false.

**What remains open, for whoever picks this up next**: the fourth pass's own
path (a) — a live `dummy_timer` breakpoint with real kernel symbols loaded,
to settle whether the NULL `setup_packet` belongs to a fresh urb or a stale,
already-freed one racing `dummy_timer`'s own list iterator — is still
unresolved and is now the only genuinely open technical question left in
this thread; it does not change the actionable scope conclusion above either
way.

**Evidence location**: `omapnks4vb_ipipe_isolation_20260719/` on `kronosvm`
(`/home/build/omapnks4vb_ipipe_isolation_20260719/`) — freshly-built
`OmapNKS4VirtualBoard.{c,ko}` (md5 `b6870c517060226484d7f485e895d4e6`),
`dummy_hcd.ko` (md5 `2e1c03653ad611a7fd220bb01125b086`, matching the fourth
pass's own copy exactly), `kronos.img` (own copy, `guestfish`-injected
`/sbin/loadoa` + two-module set only), `boot_console_ipipe.log` (the full,
397-line transcript excerpted above), `loadoa_ipipe_isolation.sh` (the
injected `/sbin/loadoa` source), `run_ipipe_isolation.sh` (the QEMU launch
script, VM name `Kronos-IpipeIsolation-20260719`, ports 4589/1259/
`qmon_ipipe.sock` to avoid colliding with any other concurrent session).
QEMU (PID `2959352`) was confirmed via `pgrep -af qemu-system-i386` to be
this pass's own process before being killed after the panic; a
post-kill `ps aux | grep qemu-system-i386` confirmed no real
`qemu-system-i386` process remained (a same-named `pgrep`/`bash -c` self-match
artifact in one intermediate check was noticed and independently ruled out
via `ps`, not trusted at face value).

## Update, 2026-07-19 (fourth pass): `dummy_timer` NULL deref root-caused with hard
## live evidence — crash is entirely inside real, unmodified USB core/`dummy_hcd.c`,
## our own gadget's `setup()` is PROVEN never even called. Not fixable within
## `OmapNKS4VirtualBoard.c`'s own scope; documenting honestly rather than patching
## around it.

Direct continuation of the third pass immediately below. Picked up its exact stated
blocker ("**Not yet investigated further this pass**" — the `dummy_timer+0x56f` NULL
deref after the "device number 2/3/4" re-enumeration churn) and did the GDB trace +
source cross-reference this task called for.

**Setup**: fresh scratch dir `omapnks4vb_dummytimer_20260719_221401` on `kronosvm`
(confirmed via `pgrep -af qemu-system-i386` that the only running instance beforehand
was a stale, already-panicked process from the third pass's own prior session,
`Kronos-DummyHCD-FullChain-20260719` — confirmed genuinely wedged, not "idle-and-
progressing", via its console log's `mtime` being 4+ hours stale with zero growth, and
via a QEMU HMP `info status` showing `VM status: running` but the guest's own dmesg
having produced nothing new; killed it cleanly). `OmapNKS4VirtualBoard.ko` was rebuilt
fresh from this directory's current source (not copied from any prior pass's own
build); `dummy_hcd.ko`/`RTAIVirtualDriver.ko`/`STGEnabler.ko`/`STGGmp.ko`/
`OmapNKS4Module.ko` were reused as-is from `omapnks4vb_epautoconf_verify/` (read-only
reference, not this pass's own artifact) since none of those are under investigation
here. Since `kronosvm`'s own `/home/share` CIFS mount (still pointed at the old,
being-decommissioned `.8` host per `kronos_share_migration` — see memory) was down for
this whole session, every file handed to `kronosvm` was `scp`'d there directly from a
host with working `/home/share` access, rather than fetched by `kronosvm` itself.
Also fixed, in this pass's own generated `/sbin/loadoa` only (not the shared
`kronos.img` template, and not any other pass's own script): the third pass's own
noted `if insmod ... | tee` footgun (checks `tee`'s exit status, not `insmod`'s) — now
uses `${PIPESTATUS[0]}` instead.

### 1. Reproduced identically, twice, from a completely fresh rebuild+reboot

Both runs this pass (one before, one after adding the diagnostic `printk`s described
below) hit the byte-identical crash signature the third pass found:
```
usb 1-1: new full speed USB device number 2 using dummy_hcd
usb 1-1: new full speed USB device number 3 using dummy_hcd
usb 1-1: new full speed USB device number 4 using dummy_hcd
BUG: unable to handle kernel NULL pointer dereference at (null)
IP: [<f819cd7f>] dummy_timer+0x56f/0xa43 [dummy_hcd]
```
`f819cd7f` (`dummy_timer+0x56f`) is the exact same address across this pass's two runs
*and* the third pass's own earlier session — strong evidence this is deterministic,
not TCG-timing garbage (module load addresses/sizes are apparently very stable in this
harness run-to-run).

### 2. Live GDB + real disassembly: identified the exact faulting C statement

Objdumped this pass's own built `dummy_hcd.ko` (`objdump -d -M intel`), found
`dummy_timer` at file-relative `0x1810` (matching runtime `0xf819c810` from the oops's
own `Call Trace` — confirms the module's `.text` loads at `0xf819b000`), and located
offset `0x1810+0x56f = 0x1d7f`:
```
1d79: 8b 34 24    mov esi,[esp]
1d7c: 8b 56 54    mov edx,[esi+0x54]
1d7f: 8b 1a       mov ebx,[edx]        <-- faults here
```
This **byte-for-byte matches** the oops's own `Code:` hex dump
(`... 8b 34 24 8b 56 54 <8b> 1a 8b 72 04 ...`, `<>` marking the faulting byte) — an
exact, independent confirmation this is the real crash site, not a guess.

Cross-checked the field offset against `struct urb` (`include/linux/usb.h`) — wrote a
tiny standalone `offsetof()` probe (`/tmp/urb_offsets.c`, plain userspace, no kernel
headers needed since every field type was copied verbatim) rather than hand-counting,
confirming `dma_addr_t` is 4 bytes on this build (`CONFIG_HIGHMEM64G`/`CONFIG_X86_64`
both unset in `/home/build/linux-kronos/.config`) and **`setup_packet` sits at exactly
offset `0x54`** — matching `EDX: 00000000` from the oops's own register dump exactly.
This is not a coincidental offset: the instructions immediately after (`mov ebx,[edx]`
/ `mov esi,[edx+4]`, copying 8 bytes into a local `struct usb_ctrlrequest`) match
`dummy_timer`'s own real source (`drivers/usb/gadget/dummy_hcd.c`) line ~1306:
```c
if (ep == &dum->ep [0] && ep->setup_stage) {
    struct usb_ctrlrequest setup;
    ...
    setup = *(struct usb_ctrlrequest*) urb->setup_packet;   /* <-- THIS LINE */
```
**Root cause, precisely**: `dummy_timer()` — real, unmodified `dummy_hcd.c`, `02 May
2005` vintage code, not this project's own — enters ep0's "new SETUP stage" handling
for some `urb`, and that urb's own `setup_packet` pointer is `NULL` at the moment
`dummy_timer` reads it.

### 3. Confirmed, empirically, that this project's own gadget code is NEVER REACHED

Added temporary (kept — cheap, genuinely useful) diagnostic `printk`s to
`nks4_setup()` in `OmapNKS4VirtualBoard.c`, logging every control request's
`bRequestType`/`bRequest`/`wValue`/`wIndex`/`wLength` on entry and `len`/queue-result
(or STALL) on exit. Rebuilt, re-injected into a fresh copy of the test image via
`guestfish`, reran end-to-end. **Zero `"OmapNKS4VirtualBoard: setup ..."` lines
appear anywhere in the boot log, ever** — confirmed by `grep`ping the complete,
421-line console capture. `nks4_bind()`'s own `"bound, ep_int=... ep_bulk=..."` line
prints exactly once, normally, well before the crash — our gadget stays bound
throughout; it's specifically the `setup()` callback (`dum->driver->setup(...)`,
called a few lines *after* the line that crashes) that is never invoked. **This
proves the crash happens strictly upstream of anything `OmapNKS4VirtualBoard.c`
controls** — before dummy_hcd ever gets far enough into processing a control
transfer's SETUP stage to hand it to our driver at all.

### 4. The real kernel's own call trace pins this squarely inside genuine USB core + `dummy_hcd`, both real/unmodified

This pass's fuller oops capture (the third pass's own log was truncated before the
full `Call Trace:`) shows:
```
Pid: 249, comm: khubd ...
Call Trace:
 ...
 [<f819c4ad>] ? dummy_urb_enqueue+0xbd/0x190 [dummy_hcd]
 [<4034b639>] ? usb_hcd_submit_urb+0x139/0x7d0
 [<4034d719>] ? usb_start_wait_urb+0x49/0xd0
 [<4034d9c0>] ? usb_control_msg+0xc0/0xf0
 [<4034df34>] ? usb_get_descriptor+0x84/0xc0
 [<4034e079>] ? usb_get_device_descriptor+0x69/0xa0
 [<40346b5d>] ? hub_port_init+0x2fd/0xa20
 [<40344700>] ? usb_alloc_dev+0x1b0/0x320
 [<40348e18>] ? hub_thread+0x518/0x1030
```
(the `?` prefixes are the kernel's own naive-stack-scan markers — not a fully verified
unwind, standard for x86-32 frame-pointer-less traces — but the shape is unambiguous:
this is `khubd`'s own kernel thread, live inside its own real enumeration call chain,
`hub_port_init` → `usb_get_device_descriptor` → `usb_control_msg` →
`usb_start_wait_urb` → `usb_hcd_submit_urb` → `dummy_urb_enqueue`.) Checked
`dummy_urb_enqueue`'s own real source (`dummy_hcd.c` line 965-1009): it ends in
exactly `spin_unlock_irqrestore(&dum->lock, flags); return rc;` — matching the trace's
own `_spin_unlock_irqrestore` → `__ipipe_unstall_root` → `smp_apic_timer_interrupt` →
`__ipipe_sync_stage` → `dummy_timer` sequence precisely. On this kernel's own I-pipe
(Adeos/RTAI-patch) interrupt model, a timer IRQ that arrives while "stalled" gets
replayed **synchronously, on the interrupted task's own stack**, the instant
`__ipipe_unstall_root` runs — here, that's the exact moment `dummy_urb_enqueue`
releases `dum->lock` after `mod_timer(&dum->timer, jiffies + 1)` (`dummy_hcd.c` line
1004, `/* kick the scheduler, it'll do the rest */`) for `khubd`'s own freshly-
submitted control urb. `dummy_timer` (real, unmodified) was plainly never written
expecting its own driving timer callback to potentially fire this close, on this same
stack, to the exact `mod_timer()`/unlock call that arms it — a genuine assumption
gap between vanilla `dummy_hcd.c`'s single-context timer-softirq design and this
project's own I-pipe-patched "korg" kernel's synchronous-replay-on-unstall semantics.
**Not fully nailed down this pass**: whether the specific `urb` `dummy_timer` finds
with a `NULL setup_packet` in this scenario is this same brand-new urb (whose
`setup_packet` *should* still be valid at this exact point — `usb_control_msg`
doesn't `kfree()` it until its own synchronous wait fully returns) or a **stale,
already-`usb_kill_urb()`'d-and-freed urb left over from device #2 or #3's own earlier,
abandoned attempt** that never got fully unlinked from `dum->urbp_list` because this
same reentrant-timing window corrupted `dummy_timer`'s own
`list_for_each_entry_safe(urbp, tmp, &dum->urbp_list, urbp_list)` iterator (a real,
plausible mechanism: if a nested `dummy_timer` invocation mutates/frees list entries
while an outer invocation's cached `tmp` pointer is still in flight, the outer
invocation dereferences freed memory next). Distinguishing these two would need a
live breakpoint on `dummy_timer` itself with kernel symbols loaded, not attempted this
pass given time already spent — but it doesn't change the actionable conclusion
below.

### 5. Honest conclusion: NOT fixable within `OmapNKS4VirtualBoard.c`'s own scope

Section 3 above is the decisive finding: our own gadget driver's `setup()` callback is
**proven, via live instrumentation, never to run** before this crash. There is
nothing in `OmapNKS4VirtualBoard.c` — no response content, no timing, no missing
descriptor field — that can cause, prevent, or influence a NULL-pointer read inside
`dummy_timer()`'s own SETUP-stage handling of a `khubd`-submitted urb, because our
code is never in the call path at all. The real mechanism (section 4) is a timing/
reentrancy interaction between genuine, unmodified `drivers/usb/gadget/dummy_hcd.c`
(`02 May 2005` vintage, written for vanilla Linux's non-reentrant softirq model) and
this specific kernel's own I-pipe-patched interrupt delivery — a kernel-level
property, present regardless of which of this project's own modules are loaded
alongside it. Per this project's own established, repeated convention (this exact
file's "second pass" section above; the top-level `CLAUDE.md`'s RTAI/kernel-tree
notes): patching `dummy_hcd.c` itself is out of scope, and no safely-scoped fix exists
in `OmapNKS4VirtualBoard.c` because the bug does not involve `OmapNKS4VirtualBoard.c`.
**No functional code change was made this pass** — only the diagnostic `printk`s in
`nks4_setup()` (section 3), which are cheap, harmless, and kept for future gadget-side
debugging, but do not touch `bind()`/`setup()`'s actual behavior otherwise. Rebuilt
clean (`make KDIR=/home/build/linux-kronos`, zero new warnings beyond the pre-existing
section-mismatch/attribute noise already present before this pass), MD5-verified
identical between the built artifact and what was injected into the test image before
each of this pass's two live boots.

**Practical, falsifiable next steps for whoever picks this up**: (a) settle section
4's open question with a live `dummy_timer` breakpoint (needs a `vmlinux`/symbol table
for this exact running kernel — the third-pass "second pass" section above already
extracted one via `guestfish`+`gunzip` from `kronos.img`'s own `/boot/bzImage`, reuse
that rather than redoing it); (b) test whether the reentrancy is specific to
`RTAIVirtualDriver.ko`/`STGEnabler.ko`/`STGGmp.ko`'s own I-pipe stall/unstall
emulation (this pass's own leading hypothesis, untested) versus inherent to
`kronos.img`'s real kernel on its own — if `OmapNKS4Module.ko`'s dependency chain
truly requires those three substitute modules (undetermined this pass), this needs a
minimal standalone repro rather than the full chain; (c) if (b) confirms the
substitute I-pipe layer is the actual trigger, the fix (if any) belongs in
`RTAIVirtualDriver.ko`'s own stall/unstall emulation, not here — a substantial,
separate effort, out of scope for a "gadget driver" pass.

**Evidence location**: `omapnks4vb_dummytimer_20260719_221401/` on `kronosvm`
(`/home/build/omapnks4vb_dummytimer_20260719_221401/`) — freshly-built
`OmapNKS4VirtualBoard.{c,ko}` (both the pre- and post-instrumentation builds, latter
is what's currently there), `kronos.img` (own copy, `guestfish`-injected `/sbin/loadoa`
+ module set), `boot_console_dt.log` (this pass's second/instrumented run — the first,
pre-instrumentation run's log was overwritten per the second run's own `rm -f` before
rebooting; its crash signature was byte-identical to the kept one before being
discarded), `/tmp/urb_offsets.c`'s own output captured inline above. Both this pass's
own QEMU processes (`Kronos-DummyTimer-Investigation-20260719`, PIDs `2931872` then
`2939131` after the rebuild) were confirmed via `pgrep -af qemu-system-i386` to match
this session's own scratch-dir path before being killed; no stray `qemu-system-i386`
process was left running on `kronosvm` at the end of this pass (confirmed).

## Update, 2026-07-19 (third pass): struct-layout fix confirmed working end-to-end;
## `usb_ep_autoconfig` missing-symbol gap found and fixed; real USB enumeration now
## starts but hits a NEW, distinct crash further in (`dummy_timer` NULL deref)

Direct continuation of the second pass above, picked up while a concurrent agent
session was independently working the same struct-layout thread. Verified via my own
fresh, independently-launched boot test (isolated scratch dir, own disk-image copy,
own QEMU instance — did not touch the concurrent agent's process) that the struct-
layout fix genuinely clears the original wild-`call *%ecx` crash: the boot reaches
`"usb usb1: configuration #1 chosen from 1 choice"` → `"hub 1-0:1.0: USB hub found"` →
`"hub 1-0:1.0: 1 port detected"` with no oops — real progress past every previous
session's stopping point.

**New blocker found and fixed**: `OmapNKS4VirtualBoard.ko` failed to insmod with
`Unknown symbol usb_ep_autoconfig`. Root cause: `usb_ep_autoconfig()` is not an
exported kernel symbol on this kernel version — every real Linux gadget driver of this
vintage compiles it directly into its own module via a source-level `#include
"epautoconf.c"` (confirmed against this exact kernel tree's own
`drivers/usb/gadget/zero.c`, the real, unmodified reference gadget driver: `#include
"usbstring.c"` / `"epautoconf.c"` right after its own headers) rather than linking it
as a shared symbol — a real, historical quirk of this era's USB gadget subsystem, not
a bug in the kernel tree. This project's own `OmapNKS4VirtualBoard.c` reconstruction
was missing that include (previous testing only ever exercised `OmapNKS4Module.ko`'s
`vm_virtual_probe` in-process shortcut, which never needed a real gadget bind).

**Fix applied**: copied `epautoconf.c` and its own `gadget_chips.h` dependency
verbatim (unmodified, GPL, same license as this project's other kernel-tree-derived
files) from `/home/build/linux-kronos/drivers/usb/gadget/` into
`OmapNKS4VirtualBoard/` itself (keeps the module buildable standalone against `KDIR`
without depending on exact kernel-source-tree layout at include time), and added
`#include "epautoconf.c"` to `OmapNKS4VirtualBoard.c` with a full evidence-citing
comment. Only `usb_ep_autoconfig()` itself is used (no `usb_string()`/config-buf
helpers), so `usbstring.c`/`config.c` were not needed. Verified via `nm`: the built
`.o` now defines `usb_ep_autoconfig`/`usb_ep_autoconfig_reset` as local symbols (`T`,
not `U` undefined) — confirmed no unresolved externs remain (`nm -u ... | grep
autoconfig` → empty).

**Live-tested, real success**: with the fix, `OmapNKS4VirtualBoard.ko` insmod'd
cleanly and printed `"OmapNKS4VirtualBoard: bound, ep_int=ep-a ep_bulk=ep-b"` — a
genuine, successful USB gadget bind with real endpoint autoconfiguration, the first
time this has ever happened in this project. (Note: this test's `loadoa` script
piped each `insmod` through `tee -a /dev/kmsg` inside an `if` check, which reports
`tee`'s exit status, not `insmod`'s — every module in this run logged a misleading
`"WARNING: insmod ... failed"` regardless of actual outcome; trust the real printk
content above the pipeline's own exit-code check, and fix that `if insmod | tee`
pipeline shape in any future test script — it's a real footgun, not a finding about
these modules.)

**New crash found, further in than any previous session reached**: immediately after
`OmapNKS4Module.ko` starts (`OmapNKS4Init: enter`), real enumeration begins —
`"usb 1-1: new full speed USB device number 2 using dummy_hcd"`, then **again** as
`"device number 3"` and `"device number 4"` in rapid succession (suspicious — a
single real enumeration should assign one device number once, not churn through
three in a row; this smells like the gadget side is triggering repeated
disconnect/reconnect rather than one clean enumeration) — then a fatal kernel oops:

```
BUG: unable to handle kernel NULL pointer dereference at (null)
IP: [<f819cd7f>] dummy_timer+0x56f/0xa43 [dummy_hcd]
...
Kernel panic - not syncing: Fatal exception in interrupt
```

`dummy_timer` is `dummy_hcd.ko`'s own real, unmodified periodic transfer-processing
callback (not this project's code) — it walks some internal request/URB list and
dereferenced a NULL entry, most plausibly because the gadget side
(`OmapNKS4VirtualBoard.c`) freed or never properly initialized something `dummy_hcd`'s
timer later expects to find (e.g. an incomplete endpoint enable/request-queue
lifecycle), or because the rapid re-enumeration churn above raced `dummy_hcd`'s own
internal bookkeeping. **Not yet investigated further this pass** — this is a
genuinely new, distinct bug from the struct-layout crash (different subsystem,
different failure mode: a real NULL deref inside real dummy_hcd code, not a wild
jump through garbage), and is the concrete next blocker for real USB-core
enumeration to fully succeed.

**Honest status**: two real bugs down (struct-layout mismatch, missing
`usb_ep_autoconfig` symbol), real USB gadget bind now genuinely works, but full
enumeration still does not complete — a third, different bug remains. This is
significant, verified forward progress, not full completion.

Test artifacts: `kronosvm:/home/build/omapnks4vb_epautoconf_verify/` (isolated scratch
dir, own `kronos.img` copy, own QEMU instance, own `boot_console.log` — cleanly killed
after the panic, did not touch the concurrent agent's own `omapnks4_dummyhcd_fix_20260719`
work).

## Update, 2026-07-17: a working virtual board exists now, via a different path

The `dummy_hcd` blocker documented above is still unresolved (not reattempted this
pass). Rather than keep debugging it blind, `OmapNKS4Module.ko` itself gained a
`vm_virtual_probe` module parameter that synthesizes a virtual NKS4 device **in-process**
and feeds it directly to the real `OmapNKS4Probe()` - no `dummy_hcd`, no gadget layer,
no real USB bus at all. Live-tested: reaches `driver_state=1` and a real
`CommunicationCheck` protocol round-trip. See `OmapNKS4Module/README.md`'s own
"`vm_virtual_probe`: a working virtual NKS4 board" section for the full story,
including two real bugs (a latent wait-queue NULL-deref, a reply-delivery race) this
path surfaced and fixed along the way.

This module's own gadget-based design is still the architecturally "more real" approach
(genuine USB core enumeration on both sides) and remains valid future work once
`dummy_hcd`'s TCG incompatibility is actually root-caused - it just isn't the path that
got a live, working virtual board first.

## Scope, this pass

Get enumeration + `OmapNKS4Probe`'s own descriptor checks to succeed, so
`OmapNKS4Init`'s own `wait_for_completion_timeout` observes a real completion instead
of timing out after 10 seconds and failing cleanly (`OmapNKS4Init`'s own real
`if (sDriverState != 1) { ...; goto cleanup; }` path — a real, graceful failure, not a
hang, confirmed via this project's own reconstruction of that function).

**Not yet attempted**: deep wire-protocol fidelity. The bulk-OUT/interrupt-IN
completion handlers in `OmapNKS4VirtualBoard.c` are deliberately minimal (log + ACK,
re-queue), not a full `COmapNKS4Command` protocol implementation. Once enumeration
itself is confirmed working, the natural next step is teaching this gadget to respond
correctly to real wire commands (`CommunicationCheck`, version queries, `0x50`/`0x51`/
`0x52` register reads, ...) using the ground truth this project's own `command.cpp`/
`driver.cpp` reconstruction and the extensive `NKS4PanelFirmware/` ARM firmware
reconstruction (the real board's own confirmed behavior) already establish.

## Files

| file | what |
|---|---|
| `OmapNKS4VirtualBoard.c` | the gadget driver: descriptors, `bind`/`unbind`/`setup`/`disconnect`, minimal transfer handlers |
| `Makefile` | out-of-tree Kbuild, matching every other module in this project |

## Build & test

```sh
make KDIR=/home/build/linux-kronos
```

Boot-test load order (see `RTAIVirtualDriver/README.md` for why `RTAIVirtualDriver.ko`
must load first):

```
RTAIVirtualDriver.ko -> STGEnabler.ko -> STGGmp.ko -> dummy_hcd.ko ->
OmapNKS4VirtualBoard.ko -> OmapNKS4Module.ko (the REAL module, not
OmapNKS4VirtualDriver.ko's stand-in stub)
```

See `MASTER_REFERENCE.md`'s own dated section for this batch's live VM boot-test
transcript and result.
