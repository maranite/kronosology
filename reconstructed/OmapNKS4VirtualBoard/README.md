# OmapNKS4VirtualBoard.ko - a genuine virtual NKS4 front-panel USB device

Reconstructed-project-original source (no shipping-binary counterpart - this is new
infrastructure, not a reverse-engineered driver). Target: Linux 2.6.32.11-korg,
x86-32, `gcc -mregparm=3`, plain C (matches every other virtual driver's own precedent
for avoiding C++ against this ancient kernel's headers).

## What this is, and why it differs from the other "virtual" modules in this project

Every other `VirtualX` module in this project (`AT88VirtualChip.ko`,
`KorgUsbAudioVirtualDriver.ko`, `OmapNKS4VirtualDriver.ko`) works by supplying the
`EXPORT_SYMBOL`s a real caller needs - standing in for a real module's own exports.
That pattern does not apply here. `OmapNKS4Module.ko` is not a library of symbols
something else calls into; it is itself a USB host driver
(`stg_usb_register_driver`, a thin `STGEnabler.ko` shim over the real kernel's own
`usb_register_driver()`). Its own `OmapNKS4Init()` blocks in
`wait_for_completion_timeout(sProbeComplete, 10000)` waiting for the Linux USB core to
call its registered `.probe` callback (`OmapNKS4Probe`) - which only happens when a
real USB device matching its ID table (vendor `0x0944`, product `0x1005`, read from
`usb.cpp`'s own `OmapNKS4Probe`) actually enumerates. No amount of providing extra
exported symbols substitutes for that - the missing piece was a genuine USB device for
the real module's own host-side driver to talk to.

This module is that device: a real Linux USB gadget driver presenting the exact
vendor/product ID and endpoint layout `OmapNKS4Probe`'s own logic checks for
(`usb.cpp`: `idVendor`/`idProduct` read from `dev+0xb8`/`+0xba`; endpoint classification
by `bmAttributes&3==3`+IN = interrupt, `bmAttributes&3==2`+OUT = bulk; interface class
`0xff`). Loaded together with `dummy_hcd.ko` - Linux's own loopback USB host+device
controller - in the same kernel instance, `dummy_hcd` loops this gadget's device-side
traffic back to its own virtual host controller. The real `OmapNKS4Module.ko`, bound to
that virtual host controller exactly as it would be to a real PC's own EHCI/OHCI/UHCI
controller, sees a real device enumerate and calls `OmapNKS4Probe` for real. No
wire-protocol guesswork stands between the two sides - this is genuine Linux USB core
code running the genuine enumeration/probe path on both sides.

## Kernel config requirement

The kernel source tree used to build kernel modules for this project needs
`CONFIG_USB_GADGET` enabled as loadable modules only (`CONFIG_USB_GADGET=m`,
`CONFIG_USB_DUMMY_HCD=m` via the `USB_GADGET_DUMMY_HCD` choice). This is a purely
additive change - it does not touch any ABI-relevant option (`SMP`/`PREEMPT`/etc. all
unchanged), so it is safe alongside every other module built against the same shared
tree. No kernel image (`bzImage`) rebuild is needed either - the gadget framework and
`dummy_hcd` build as ordinary out-of-tree-style loadable modules via a targeted build
of just `drivers/usb/gadget/`:

```sh
cd /path/to/kronos-kernel-tree
# Append CONFIG_USB_GADGET=m / CONFIG_USB_GADGET_DUMMY_HCD=y / CONFIG_USB_DUMMY_HCD=m /
# CONFIG_USB_GADGET_DUALSPEED=y to .config and fold in via
# `make ARCH=i386 oldconfig` (this kernel predates `oldconfig`'s modern
# `olddefconfig`/`oldnoconfig` non-interactive targets).
make ARCH=i386 modules_prepare
make ARCH=i386 M=drivers/usb/gadget modules
# -> drivers/usb/gadget/dummy_hcd.ko
```

`dummy_hcd.ko` itself exports `usb_gadget_register_driver`/`usb_gadget_unregister_driver`
directly - this kernel era does not have a separate shared "gadget core" module; each
UDC driver carries the registration entry points.

## Design goal

Get enumeration and `OmapNKS4Probe`'s own descriptor checks to succeed, so
`OmapNKS4Init`'s own `wait_for_completion_timeout` observes a real completion instead
of timing out after 10 seconds. Either outcome is a clean path through
`OmapNKS4Init`'s own real `if (sDriverState != 1) { ...; goto cleanup; }` handling -
timing out is a graceful failure, not a hang; a real completion is full success.

Deep wire-protocol fidelity is future work: the bulk-OUT/interrupt-IN completion
handlers in `OmapNKS4VirtualBoard.c` are deliberately minimal (log + ACK, re-queue),
not a full `COmapNKS4Command` protocol implementation. Once enumeration itself is
confirmed working end to end, the natural next step is teaching this gadget to respond
correctly to real wire commands (`CommunicationCheck`, version queries, `0x50`/`0x51`/
`0x52` register reads, ...) using the ground truth this project's own `command.cpp`/
`driver.cpp` reconstruction and the NKS4 panel firmware reconstruction already
establish.

## Build

```sh
make KDIR=/path/to/kronos-kernel-tree
```

Load order:

```
RTAIVirtualDriver.ko -> STGEnabler.ko -> STGGmp.ko -> dummy_hcd.ko ->
OmapNKS4VirtualBoard.ko -> OmapNKS4Module.ko (the real module, not
OmapNKS4VirtualDriver.ko's stand-in stub)
```

See `RTAIVirtualDriver/README.md` for why `RTAIVirtualDriver.ko` must load first.

## Files

| file | what |
|---|---|
| `OmapNKS4VirtualBoard.c` | the gadget driver: descriptors, `bind`/`unbind`/`setup`/`disconnect`, minimal transfer handlers |
| `epautoconf.c`, `gadget_chips.h` | copied verbatim from the kernel tree's `drivers/usb/gadget/` (see "Missing `usb_ep_autoconfig` symbol" below) |
| `Makefile` | out-of-tree Kbuild, matching every other module in this project |

## Missing `usb_ep_autoconfig` symbol

`usb_ep_autoconfig()` is not an exported kernel symbol on this kernel version - every
real Linux gadget driver of this vintage compiles it directly into its own module via a
source-level `#include "epautoconf.c"` (confirmed against this kernel tree's own
`drivers/usb/gadget/zero.c`, the real, unmodified reference gadget driver, which does
the same thing) rather than linking it as a shared symbol - a real, historical quirk of
this era's USB gadget subsystem, not a kernel bug.

Fix: `epautoconf.c` and its `gadget_chips.h` dependency are copied verbatim
(unmodified, GPL) from the kernel tree's `drivers/usb/gadget/` into this module's own
directory (keeps the module buildable standalone against `KDIR` without depending on
exact kernel-source-tree layout at include time), and `OmapNKS4VirtualBoard.c` includes
`epautoconf.c` directly. Only `usb_ep_autoconfig()` itself is used (no `usb_string()`/
config-buffer helpers), so `usbstring.c`/`config.c` are not needed.

With this fix in place, the gadget binds successfully and real endpoint
autoconfiguration succeeds (`OmapNKS4VirtualBoard: bound, ep_int=ep-a
ep_bulk=ep-b`).

## Known kernel-header ABI mismatches (open blockers)

`dummy_hcd.ko` is a loadable module built against this project's own reconstructed
kernel-header tree, but it loads into and interoperates with the real, opaque,
already-built `kronos.img` kernel binary, which was compiled against a different
(and not fully known) header state. Any struct shared between the two that has a
different field layout on each side silently corrupts pointer arithmetic on whichever
side reads a field the other side wrote at a different offset. Two such mismatches
have been found so far.

### 1. `struct usb_hcd` / `struct usb_bus`

Symptom: after `dummy_hcd.ko` loads and the kernel's generic hub driver begins
enumerating its virtual root hub ("configuration #1 chosen from 1 choice"), the guest
hangs, spinning forever inside a range of memory that disassembles as nothing but raw
zero bytes decoded as `add %al,(%eax)` (an effective 2-byte NOP) - a wild indirect call
through a bad function pointer, not a stalled wait.

Disassembling the real kernel's own compiled code around the crashing call site (in
its extracted, symbolicated `vmlinux`) identifies the two adjacent functions involved:

```
4034c100 usb_hcd_disable_endpoint
4034c120 usb_hcd_reset_endpoint
```

Both are ordinary, unmodified `drivers/usb/core/hcd.c` functions that do
`hcd = bus_to_hcd(udev->bus); if (hcd->driver->endpoint_X) hcd->driver->endpoint_X(hcd, ep);`
- reached generically for any USB device/endpoint, root hub included, as part of
applying the root hub's own configuration. `usb_hcd_reset_endpoint`'s real,
compiled code reads `hcd->driver` from offset `+0x94`.

Comparing that against `offsetof()` values computed from this project's own kernel
header tree (via a small out-of-tree probe module using `offsetof()`/`sizeof()`, whose
compile-time-constant immediates are visible directly in the compiled object's
disassembly) shows a real, measurable, growing delta:

| field | real kernel | this project's tree | delta |
|---|---|---|---|
| `usb_bus.devnum_next` | `0x14` | `0x10` | +4 |
| `usb_bus.root_hub`-ish field | `0x28` | `0x24` | +4 |
| `usb_hcd.kref` | `0x44` | `0x40` | +4 |
| `usb_hcd.rh_timer` | `0x68` | `0x60` | +8 |
| `usb_hcd.driver` | `0x94` | `0x8c` | +8 |
| `sizeof(struct usb_hcd)` | `0xdc` (from the real kernel's own `kmalloc` call size) | `0xc8` | +20 |

This fully explains the crash: `dummy_hcd.ko` populates `hcd->driver` at its own
tree's offset (`0x8c`); the real kernel's `usb_hcd_reset_endpoint` reads `hcd->driver`
from its own, differently-laid-out offset (`0x94`) - 8 bytes further into the object,
landing on whatever real (but wrongly-typed) field happens to live there and calling
through it as if it were a function-table pointer.

The delta is not caused by a `CONFIG_USB_GADGET` configuration mismatch: `struct
hc_driver` has zero `#ifdef`-conditional fields at all; `struct usb_hcd` has exactly
one (`wakeup_work`, under `CONFIG_PM`, `=y` on both sides); `struct usb_bus` has two
(`CONFIG_USB_DEVICEFS`, `CONFIG_USB_MON`, both matching between the two configs). None
of the three structs involved can be perturbed by that config option.

Leading explanation, unconfirmed: the real kernel reports `gcc version 4.5.0`, while
this project's kernel tree is built with a much newer host GCC. `struct usb_bus` mixes
plain `u8` members directly against 1-bit bitfields with no explicit padding -
bitfield/adjacent-narrow-member packing across mismatched base types is
implementation-defined territory where GCC's packing behavior can differ across major
versions, which would plausibly explain the first `+4` seen in `usb_bus`. It does not,
on its own, explain the further `+4` that opens up again between `kref` and
`rh_timer` (no bitfields at all in that region of either tree's `hcd.h`), which looks
more like a field genuinely missing or differently sized in this project's
reconstructed header (e.g. a backported struct member, or a buffer-size difference in
`irq_descr[]`). Which mechanism accounts for which portion of the total 20-byte delta
is not resolved.

**To validate/fix**: build a small standalone reproducer isolating just the `struct
usb_bus` bitfield region, compile it under both a GCC-4.5-era toolchain and the tree's
current host GCC, and diff the resulting `offsetof()` values to settle whether
compiler packing alone explains the first `+4`. Separately, disassemble more of the
real kernel's own `usb_create_shared_hcd`/`usb_add_hcd` for further explicit-offset
writes to pin down the remaining ~12 bytes of the `+20` total delta not yet accounted
for by named fields. Once the exact missing/mis-packed bytes are identified, patch the
kernel tree's `drivers/usb/core/hcd.h`/`include/linux/usb.h` so that
`offsetof(struct usb_hcd, driver)` (and the other fields above) match the real kernel
exactly, rebuild `dummy_hcd.ko`, and re-test.

### 2. `struct urb`

A second, independent layout mismatch affects `struct urb` (`include/linux/usb.h`):
the real kernel has `setup_packet` at offset `+0x58`, while this project's kernel tree
has it at `+0x54`, with every field from `transfer_buffer` onward consistently off by
`+4`. This was confirmed by disassembling the real kernel's own `usb_control_msg`/
inlined `usb_fill_control_urb` and cross-checking five independent fields
simultaneously.

This mismatch fully explains a separate, later crash inside `dummy_timer()` (real,
unmodified `drivers/usb/gadget/dummy_hcd.c`, `02 May 2005` vintage): `dummy_timer`'s
own SETUP-stage handling dereferences `urb->setup_packet`, which reads back `NULL` -
not because the pointer is genuinely null, but because `dummy_hcd.ko`'s own compiled
code (built against this project's tree, `setup_packet` at `+0x54`) reads 4 bytes short
of the real kernel's actual field, landing on an adjacent field that happens to be
zero. Live breakpoint tracing confirmed `urb->setup_packet` already reads as `0` at the
very first instruction of `dummy_urb_enqueue()`, before any `dummy_hcd`/`dummy_timer`
code has touched the urb at all - inconsistent with any reentrancy/race explanation,
and consistent instead with a simple wrong-offset read.

An earlier hypothesis, since superseded by the finding above, was that this crash was
caused by genuine reentrancy: on this kernel's I-pipe (Adeos/RTAI-patch) interrupt
model, a timer IRQ that arrives while "stalled" is replayed synchronously, on the
interrupted task's own stack, the instant the relevant spinlock is released - meaning
`dummy_timer()` genuinely can be invoked nested, before an outer `dummy_urb_enqueue()`
call has returned. This reentrancy is real and directly observed live (confirmed via
breakpoints on `dummy_urb_enqueue`, the `mod_timer` call site, `dummy_timer`'s entry,
and the crash site, firing in that exact order on the same stack), and a
non-reentrancy guard (`dum->timer_active`, checked and set under `dum->lock`) exists
for it in the standalone fork described below. But repeated live testing showed the
guard does not prevent the crash, because the crash happens on the very first
invocation of `dummy_timer()` for a given urb, not a nested one - there is no outer
invocation for a first call to race against. The `struct urb` offset mismatch above is
the finding that actually explains the observed NULL read.

A standalone fork, `dummy_hcd_fixed.c` (`reconstructed/OmapNKS4DummyHCDFix/`), adds a
defensive NULL check immediately before the line that dereferences `setup_packet`,
failing the request cleanly with `-EPROTO` (via the same graceful-failure path real,
unmodified `dummy_hcd.c` already uses when no gadget endpoint matches) instead of
crashing. This does not address the underlying offset mismatch, only converts its
symptom from a kernel panic into an ordinary USB transfer failure. Per this project's
scope rules, real, unmodified `dummy_hcd.c` itself is never patched; all fixes live in
this standalone fork.

**To validate/fix**: correct the `struct urb` field layout in the kernel tree's
`include/linux/usb.h` so `setup_packet` and the fields after it land at the real
kernel's offsets, rebuild `dummy_hcd_fixed.ko` (or `dummy_hcd.ko` once the fix is
folded back in), and re-run the same live-breakpoint trace to confirm
`urb->setup_packet` now reads a valid pointer at `dummy_urb_enqueue`'s entry. See
`reconstructed/OmapNKS4DummyHCDFix/README.md` for the full fix history and evidence
for this and related bugs in the same module.

## Alternative validation path (does not depend on the above being fixed)

`OmapNKS4Module.ko` has a `vm_virtual_probe` module parameter that synthesizes a
virtual NKS4 device in-process and feeds it directly to the real `OmapNKS4Probe()` -
no `dummy_hcd`, no gadget layer, no real USB bus at all. This reaches
`driver_state=1` and a real `CommunicationCheck` protocol round-trip without requiring
this module or `dummy_hcd.ko` to work. See `OmapNKS4Module/README.md`'s own
`vm_virtual_probe` section for detail, including two real bugs (a latent wait-queue
NULL-deref, a reply-delivery race) that path surfaced and fixed.

This module's own gadget-based design remains the architecturally more complete
approach (genuine USB core enumeration on both sides of a real device/host
relationship) and is the natural path forward once the kernel-header ABI mismatches
above are resolved; `vm_virtual_probe` is a working substitute in the meantime, not a
replacement for it.

## Known limitations

- The `struct usb_hcd`/`struct usb_bus` field-layout fix described above has been
  derived (the exact target offsets are known) but has not been applied to the kernel
  tree's headers and re-verified end to end. *To validate:* apply the header fix,
  rebuild `dummy_hcd.ko`, and confirm the root-hub enumeration no longer crashes.
- The `struct urb` field-layout fix has not been applied either. *To validate:* apply
  the header fix, rebuild, and confirm `dummy_timer()` no longer reads a NULL
  `setup_packet`.
- The exact mechanism behind each delta (compiler bitfield-packing difference vs. a
  genuinely missing/different-sized header field) is not fully isolated for either
  struct family. *To validate:* see the standalone-reproducer suggestion above.
- Wire-protocol fidelity is minimal (log + ACK, re-queue on both endpoints), not a
  real `COmapNKS4Command` implementation. *To validate:* once enumeration succeeds
  end to end, extend `nks4_setup()`/the bulk handler to answer real commands using the
  `command.cpp`/`driver.cpp` and NKS4 panel firmware reconstructions as ground truth.
