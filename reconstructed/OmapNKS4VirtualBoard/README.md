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
