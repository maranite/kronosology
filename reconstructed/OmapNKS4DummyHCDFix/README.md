# OmapNKS4DummyHCDFix - dummy_hcd_fixed.ko

A standalone, out-of-tree fork of the Linux kernel's `dummy_hcd.c` USB
gadget driver (the in-kernel virtual host-controller/device pair the
kernel's own USB gadget subsystem uses to exercise a gadget driver without
real hardware), patched to work under the Kronos kernel's RTAI/I-pipe
interrupt model and, together with a handful of fixes to the
`OmapNKS4VirtualBoard` gadget driver itself, to correctly bring up
`OmapNKS4Module.ko` - the real Kronos NKS4-panel USB driver - against a
fully emulated NKS4 panel device. This lets the panel driver chain
(`OmapNKS4VirtualBoard.ko` + `OmapNKS4Module.ko`) be loaded, probed, and
exercised without a real NKS4 panel attached.

See `reconstructed/OmapNKS4VirtualBoard/README.md` for the emulated gadget
device itself; this document covers the host-controller fork and the
sequence of independent bugs (in the kernel's struct layouts, in the
gadget's descriptor construction, and in its protocol handling) that had to
be fixed, one at a time, before the real driver would load successfully.

## What's in this directory

- `dummy_hcd_fixed.c` - the fork. Identical to the real `dummy_hcd.c`
  except for: (1) including a local `hcd.h` instead of the in-tree relative
  path `../core/hcd.h`, (2) driver/gadget name string renames
  (`dummy_hcd` -> `dummy_hcd_fixed`, `dummy_udc` -> `dummy_udc_fixed`,
  updated `MODULE_DESCRIPTION`/`MODULE_AUTHOR`) so the module is
  identifiable in `dmesg`/`/proc/modules`, and (3) the fixes described
  below. All changes are marked inline with `KRONOS FORK` /
  `KRONOS I-PIPE REENTRANCY FIX` comments.
- `hcd.h`, `hub.h` - verbatim, unmodified read-only copies of the matching
  kernel source tree's `drivers/usb/core/{hcd.h,hub.h}`. Real `dummy_hcd.c`
  includes `hcd.h` via a same-source-tree relative path that only resolves
  when building in-tree; building out-of-tree here requires local,
  same-directory copies instead. `hcd.h` itself pulls in `hub.h`; nothing
  else is needed (the built module's undefined-symbol list contains only
  ordinary kernel/USB-core exports).
- `Makefile` - the same `KDIR=`/`obj-m` convention used by every other
  kernel module in this project.

## Build

```
cd OmapNKS4DummyHCDFix
make KDIR=/path/to/kronos-kernel-tree
```

`KDIR` must point at a configured Linux 2.6.32.11-korg source tree whose
module ABI (struct layouts, the `-mregparm=3` calling convention, RTAI/
I-pipe support) matches the Kronos's own kernel build - see "Struct-layout
ABI mismatches" below for what happens when it doesn't. `nm -u` on the
built module shows only ordinary kernel/USB-core symbols
(`_spin_lock_irqsave`, `usb_hcd_giveback_urb`, `usb_create_hcd`,
`platform_driver_register`, etc.) plus `__ipipe_restore_root`, pulled in
transitively by this kernel's I-pipe-aware inline spinlock primitives -
expected, and itself confirms that this kernel's
`spin_unlock_irqrestore()` really does route through I-pipe's own unstall
path, which is the mechanism behind the reentrancy issue described next.

## Coexistence with a real `dummy_hcd.ko`

Only the driver/gadget name strings and module description were renamed.
Symbol-level coexistence was not achieved: like the real driver, this fork
`EXPORT_SYMBOL()`s `usb_gadget_register_driver`/`usb_gadget_unregister_driver`
under those exact (unrenameable - `OmapNKS4VirtualBoard.ko` depends on the
name) global symbol names, so loading both `dummy_hcd.ko` and
`dummy_hcd_fixed.ko` at the same time fails with a duplicate-symbol error.
Only one is ever loaded at a time; this fork is meant to fully replace
`dummy_hcd.ko`, not run alongside it.

## Bugs found and fixed

Getting `OmapNKS4Module.ko` to load against the emulated gadget required
fixing six independent, unrelated bugs, in the order below - each one had
been fully masking the next, so each fix revealed a new failure rather than
completing the chain.

### 1. `dummy_timer()` reentrancy under I-pipe (real mechanism, not the crash's root cause)

This kernel's I-pipe (Adeos/RTAI-patch) interrupt model lets
`spin_unlock_irqrestore()` synchronously replay a queued interrupt -
including the timer softirq that drives `dummy_timer()` - on the unlocking
function's own call stack, before that function returns to its own caller.
Vanilla Linux, which the real `dummy_hcd.c` was written for, never does
this. Live kernel debugging captured the exact sequence:
`dummy_urb_enqueue()`'s closing `spin_unlock_irqrestore()` ->
`__ipipe_unstall_root()` -> `dummy_timer()`, nested, on the calling
thread's own stack, before `dummy_urb_enqueue()` itself returns.

`struct dummy` carries a `timer_active:1` bitfield, checked and set under
`dum->lock` at `dummy_timer()`'s own entry, that bails out early if a
second, overlapping invocation is detected mid-list-walk over
`dum->urbp_list`. This guard is real, correctly placed, and closes a
genuine hazard - but it is not what fixed the observed crash: live
breakpoint evidence showed the crash occurs on a given urb's *first-ever*
`dummy_timer()` call (`timer_active` was still 0 at that point), not a
genuine second, overlapping one. The guard is kept in the source as a real
safeguard against the reentrancy hazard it does address, but the actual
crash had a different cause (see below).

### 2. `struct usb_hcd`/`struct usb_bus` field-offset mismatch

The kernel build tree's own `struct usb_hcd` disagreed with the real
kernel's layout: `usb_hcd.driver` at offset `0x8c` instead of the real
kernel's `0x94`, and `sizeof(struct usb_hcd)` `0xc8` instead of `0xdc`. The
exact source of the drift (compiler bitfield-packing differences between
build environments, versus a genuinely different/extra field the real
vendor kernel carries that a generic reconstructed kernel tree lacks) was
not conclusively pinned down, but the practical fix is a documented,
commented correction to the affected struct definitions in the kernel
build tree's own headers, bringing `usb_hcd.driver` to `0x94` and
`sizeof(struct usb_hcd)` to `0xdc` to match the real kernel. Without this
fix, `dummy_hcd_fixed` crashes with a kernel panic before any USB
enumeration completes.

### 3. `struct urb` field-offset mismatch

A second, independent layout mismatch: every `struct urb` field from
`transfer_buffer` onward was consistently offset 4 bytes low relative to
the real kernel, while every field up to and including `pipe` matched
exactly:

| field | build-tree offset | real kernel offset |
|---|---|---|
| `dev` | `0x28` | `0x28` |
| `pipe` | `0x30` | `0x30` |
| `transfer_buffer` | `0x3c` | `0x40` |
| `transfer_buffer_length` | `0x4c` | `0x50` |
| `setup_packet` | `0x54` | `0x58` |
| `context` | `0x6c` | `0x70` |
| `complete` | `0x70` | `0x74` |

This was established by disassembling `usb_submit_urb()` and
`usb_hcd_giveback_urb()` from a real, running Kronos kernel image and
comparing the offsets those functions actually store to against the same
fields in the build tree's compiled code. A 4-byte field is missing from
the build tree's `struct urb` somewhere between `pipe` and `status`; ruled
out as an explanation: 8-byte `dma_addr_t` (both configurations use 4-byte
DMA addresses), and GCC bitfield-packing (`struct urb` has no bitfields).
The real field's name and purpose were never identified; the fix inserts a
generic 4-byte padding field between `pipe` and `status` in the build
tree's `struct urb` definition, the same documented-padding convention
used for the `usb_hcd`/`usb_bus` fix above.

This offset bug fully explains the original `dummy_timer()` NULL-pointer
crash without needing any race or reentrancy mechanism at all:
`dummy_timer()`'s compiled read of `urb->setup_packet` at offset `0x54`
was, on the real kernel's true layout, reading whatever field actually
occupies that slot (inside the `status`/`transfer_flags` region) - which
reads as zero from the very first instruction, on every boot, regardless
of timing. Once this offset is corrected, `dummy_hcd_fixed` reads real,
correct `urb` field values.

A defensive `NULL` check on `urb->setup_packet`, immediately before the
dereference in `dummy_timer()`, was added as an interim safety net using
the file's own existing `-EPROTO`/`goto return_urb` graceful-failure
pattern (the same idiom already used elsewhere in the file for "no ep
configured for urb"). It does not explain or fix the underlying offset
bug - it only prevents the symptom (a kernel panic) while the mismatch is
present. With the offset corrected, this check no longer fires in
practice, but it is left in place as a defensive measure.

### 4. Malformed USB config descriptor (`OmapNKS4VirtualBoard.c`)

`struct usb_endpoint_descriptor` is packed but carries 2 trailing
audio-class-only fields (`bRefresh`, `bSynchAddress`) beyond the real
7-byte wire format - its own header comment directs callers to use
`USB_DT_ENDPOINT_SIZE` for `bLength`, not `sizeof()`. Each endpoint
descriptor's `bLength` was already correctly set to `USB_DT_ENDPOINT_SIZE`
(7), but the gadget's `USB_DT_CONFIG` setup handler used
`sizeof(nks4_int_ep_desc)`/`sizeof(nks4_bulk_ep_desc)` (9, including the 2
audio-only bytes) both for computing `wTotalLength` and for advancing the
descriptor-buffer offset between descriptors. The resulting 2-byte gap
between what `bLength` claims and where the next descriptor actually
starts made the host's descriptor parser - which walks the buffer by
`bLength` - land 2 bytes into leftover zero padding after the first
endpoint descriptor and read it as a bogus zero-length descriptor,
aborting the parse after finding only 1 of the 2 declared endpoints.

Fix: use `USB_DT_ENDPOINT_SIZE` instead of `sizeof()` for both the
`wTotalLength` computation and the offset advance for each endpoint
descriptor. `struct usb_config_descriptor` and `struct usb_interface_descriptor`
have no equivalent trap - both are packed with no extra fields, so
`sizeof()` is correct for those two.

### 5. `bInterfaceSubClass` driver-match mismatch (`OmapNKS4VirtualBoard.c`)

The real driver's `struct usb_device_id` match table sets
`match_flags = 0x0383`, decoding as
`VENDOR|PRODUCT|INT_CLASS|INT_SUBCLASS|INT_PROTOCOL` - i.e. the kernel's
own `usb_match_one_id()` requires an exact match on `bInterfaceSubClass`,
not just `bInterfaceClass`. The real required value is `0xff`; the
gadget's interface descriptor had it at `0`. With the flag set and the
values disagreeing, the kernel's driver-matching step silently rejects the
interface before the real driver's own probe function is ever called -
independent of, and downstream of, otherwise-correct enumeration.
`bInterfaceClass` (`0xff`) and `bInterfaceProtocol` (`0`) were already
correct. Fix: set `nks4_intf_desc.bInterfaceSubClass` to `0xff`.

### 6. Missing configuration-time command replies (`OmapNKS4VirtualBoard.c`)

With the five bugs above fixed, the real driver's probe function succeeds,
but its own configuration step still fails: the emulated gadget's
interrupt-IN/bulk-OUT completion handlers were deliberately minimal
(log + ACK only) and never answered the 3 configuration-time queries the
real driver's own configure sequence waits on - `CommunicationCheck`,
`ReadPortConfiguration`, and `GetVersion` (the sequence's other 6 calls
are one-way bulk-OUT setters, already satisfied by a generic re-queue).

- Bulk-OUT command words travel as raw, unswapped x86 little-endian 32-bit
  words - enabling byte-reversal on this path breaks real panel
  communication, so none is applied here either.
- Interrupt-IN replies follow a 4-byte `[dLo][dHi][idx][op]` record
  format, terminated by a `[0][0][0][0x87]` sync record.

Fix: a 3-entry command-word-to-response-record lookup table, plus logic in
the bulk-OUT completion handler that recognizes an incoming command word,
builds an 8-byte reply (response record + sync terminator), and queues it
on the interrupt-IN endpoint. Unrecognized or setter-only commands still
fall through to the existing generic re-queue. With this in place, the
real driver's own diagnostic output shows it correctly decoding the
emulated replies back into the expected values (port switch state,
hardware version string), and the driver loads and configures
successfully end to end.

### 7. Runtime-event generator timing (`OmapNKS4VirtualBoard.c`)

A periodic generator emits synthetic panel-event records (button, rotary,
analog/aftertouch, S/PDIF) on the interrupt-IN endpoint once the gadget is
configured, to exercise the real driver's event-decode path beyond the 3
configuration-time queries. Its timer originally armed a fixed delay after
`SET_CONFIGURATION`, which is not a reliable proxy for "the real driver
has finished its own post-configure bring-up" - `SET_CONFIGURATION` can
fire well before the real driver even loads. Fix: the timer arms only
once a counter confirms all 3 real configuration replies have actually
been sent, plus an additional settle delay, instead of guessing from an
earlier, less-correlated event.

## Panel event wire protocol: `ProcessNextNKSEvent` decode

`OmapNKS4Module.ko`'s `ReceiveEventBuffer()` decodes raw interrupt-IN
records into `[dLo][dHi][idx][op]` tuples; `OA.ko`'s own
`CSTGOmapNKSMsgHandler::ProcessNextNKSEvent()` is the function that reads
those tuples off the queue and dispatches them to the real front-panel
handlers. Direct disassembly of `ProcessNextNKSEvent()` established the
following dispatch table (all handlers use the `regparm(3)` calling
convention: `EAX` = `this`, remaining arguments in `EDX`/`ECX`/stack in
that order):

| `op` | `idx` pattern | Dispatch | Argument derivation |
|---|---|---|---|
| `0x00` | `idx & 0xf0 == 0x30` (press) or `0x40` (release) | `CSTGFrontPanel::HandleSwitchEvent(code, pressed)` | `code = dHi & 0x7f` (0-127 scan code); `pressed = 1` if `idx&0xf0==0x30` else `0`; `dLo` unused |
| `0x01` | `idx & 0xf0 == 0x50` | `CSTGFrontPanel::HandleRotary(delta)` | `delta = dHi<<8 \| dLo` |
| `0x03` | any | `CSTGFrontPanel::HandleAnalogController(device_code, param2, param3)` | `device_code = idx & 0x3f`; `param2`/`param3` come from `ShortInvertNkS4AnalogValue(dLo, dHi, &out_hi, &out_lo)` |

S/PDIF status events are handled entirely inside `OmapNKS4Module.ko`
itself and never reach `ProcessNextNKSEvent()`'s front-panel dispatch.

This corrects two informal mislabelings that had existed in this
project's own comments: the shape `op==1`/`idx&0xf0==0x50` is rotary
encoder traffic, not a button, and `op==0x1f` does not reach
`HandleRotary` at all - it goes to a diagnostic message path.

`device_code` (for `HandleAnalogController`) uses the same numbering this
project's real, hardware-confirmed analog-control reference elsewhere uses:
`7`=Aftertouch, `8`-`15`=RT Knobs 1-8, `16`-`23`=Sliders 1-8,
`25`=Value Slider, `26`=Tempo, `27`=rear Pedal jack, `28`=FootSwitch,
`29`=Damper. The real, hardware-confirmed scan code for the EXIT button is
`8`.

`OmapNKS4VirtualBoard.c`'s runtime-event generator sends real, named
traffic for 4 of its 6 synthetic event types (button press/release using
the EXIT scan code, rotary, aftertouch, RT Knob 1) plus S/PDIF (which
needs no bridging, as noted above); the 6th (general analog/CC) remains a
generic placeholder pending full resolution of the analog-value transform
below.

### `ShortInvertNkS4AnalogValue`'s transform

Disassembly of `ShortInvertNkS4AnalogValue(dLo, dHi, &out_hi, &out_lo)`
establishes its shape as a raw-value-extract-then-conditionally-invert
transform:

1. Combine `dLo`'s upper bits with the top 2 bits of `dHi` into one 10-bit
   raw value: `raw10 = (dLo << 2 & 0x3f8) | (dHi >> 6 & 0x3)`.
2. Re-read the stored value; if it equals the center point `0x200`, leave
   it unchanged. Otherwise invert it: `value = 0x3ff - value`.

The `raw10 = dLo<<2 | dHi>>6` bit-packing matches this project's own
established convention documented elsewhere for a similar ADC-style
byte-pair calibration path, and the conditional inversion around the
`0x3ff` range (skipped only at the exact center, `0x200`) matches the
function's own name, consistent with a direction/polarity flip for
whichever axis or channel it feeds.

For ONE device group this is enough to make a concrete prediction:
this project's own `nks4_inject.c` documents `device_code`s `8`-`15`
(RT Knobs), `16`-`23` (Sliders), and `25` (Value Slider) as
HARDWARE-CONFIRMED with a `byte0 = value*2` scaling formula, where `byte0`
is `HandleAnalogController`'s own `param2` argument - i.e. `out_hi` from
this transform. Combining the two: for the virtual gadget's own RT Knob 1
event (`dLo=0x40`, `dHi=0x00`), `raw10 = 0x100`, `out_hi = 0x100 >> 3 = 32`,
predicting a displayed value of `16`. This specific prediction was not
independently re-verified against real hardware. For every OTHER
`device_code` (including Aftertouch, `7`) `nks4_inject.c` itself states no
value-scaling formula is hardware-confirmed, so no displayed-value claim is
made for those - see Known limitations.

## Known limitations

- **Device *identity* for all 30 `device_code` values (0-30) is now fully
  closed** - independently cross-confirmed two ways: this project's own
  fresh disassembly of `AnalogControllerHandler` (three jump tables keyed
  by `device_code` range: 8-15 knobs, 16-23 sliders, direct-store cases for
  1-7, a second jump table for 25-29, special-cased 18/24/30) and
  `nks4_inject.c`'s independent OA.ko dispatch-table analysis (Joystick
  X/Y=1/2, Ribbon X/Z=3/4, Vector Joystick X/Y=5/6, Aftertouch=7,
  effects-rack context edit=24, Value Slider=25, Tempo=26, rear-panel
  Pedal/Footswitch/Damper=27/28/29, `SetControllerAssignment`=30 - not a
  physical control) agree on every code. **The exact numeric-to-physical
  scaling of `ShortInvertNkS4AnalogValue`'s output per `device_code`
  remains open for most codes** - only its general extract-and-invert
  shape is established. One exception: combining this transform with
  `nks4_inject.c`'s own separately hardware-confirmed `byte0=value*2`
  formula for the RT Knobs/Sliders/Value Slider group gives a concrete
  predicted value (`16`) for the virtual gadget's own RT Knob 1 event -
  not independently re-verified against real hardware, but a real
  derivation rather than an arbitrary choice. A second exception, found via
  fresh disassembly: `device_code=7` (Aftertouch)'s decompile showed
  `AnalogControllerHandler`'s `case 7` as a raw store with Ghidra's own
  "WARNING: Store size is inaccurate" flag - that warning was masking an
  inlined call to a real, separately-named function,
  `CSTGControllerInfo::AnalogAftertouchHandler`. Its logic is directly
  readable: it takes the raw byte value and writes it straight into a MIDI
  channel-pressure message (status `0xd0`) with **no `*2` scaling and no
  lookup table at this layer** - unlike the RT-Knob/Slider/Value-Slider
  group. This isn't a gap in this project's tracing; the actual curve-
  shaping for Aftertouch already happens one layer down, inside
  `OmapNKS4Module.ko`'s own `ReceiveEventBuffer` (the
  `sAfterTouch1ConvertTable`/`sAfterTouch2ConvertTable` 88-key vs 61/73-key
  curves, applied to the raw ADC value before it ever reaches `OA.ko`). Not
  independently re-verified against real hardware, but a real derivation
  from readable code, not a guess. Further fresh disassembly (all six of
  `AnalogJoystickXHandler`/`AnalogRibbonXHandler`/`AnalogFootPedalHandler`/
  etc. turned out to be real, individually decompiled functions, not dead
  ends): **Joystick X** feeds its 10-bit raw value through
  `CSTGControllerRTData::CPitchBendFilter::Filter` (a center-deadzone check
  at `0x200` gates whether the filter runs) and emits a real 14-bit MIDI
  Pitch Bend message (`low7 = v&0x7f`, `high7 = (v>>7)&0x7f`) - a genuine,
  different scaling model from the knob/slider group, not yet reduced to a
  single formula since `CPitchBendFilter`'s own smoothing arithmetic wasn't
  traced. **Ribbon X** shows real touch/release semantics: `param_1!=0`
  means finger-down (stores a displayed value of `0x3ff-param_1` - another
  instance of the same center-inversion style seen in
  `ShortInvertNkS4AnalogValue`, but applied again at this OA.ko layer, not
  a duplicate of it) and sends a 7-bit CC via `SendCCToKG`; `param_1==0`
  (release) resets to a fixed midpoint `0x40`. **FootPedal** is a thin
  wrapper forwarding straight to `CSTGControllerRTData::HandleFootPedalChange`,
  which itself forwards to a generic `HandleControllerChange` CC dispatcher
  with no extra scaling visible at this layer (structurally similar to
  Aftertouch's direct pass-through). Still genuinely unconfirmed: Joystick
  Y/Ribbon Z/Vector Joystick X+Y's own specific formulas (not yet read),
  Footswitch (on/off, likely trivial but unread), and Tempo/Damper (already
  separately documented as using their own distinct non-linear curves).
  *To validate:* trace `HandleAnalogController`'s own further use of
  `param2`/`param3` for a specific `device_code`, or compare synthetic
  values against live
  real-hardware readings for that control.
- **Real-hardware verification of this exact wire-level event byte
  sequence, as consumed by the real driver's `ReceiveEventBuffer()`, WAS
  performed (2026-07-20)** - and produced a real, diagnosed negative
  result, not yet a confirmed working effect. Earlier, semantic-layer-only
  checks (via `nks4_inject`'s `/proc/.nks4inject`, an unrelated
  additive-call injection path) showed a framebuffer diff before/after an
  EXIT button press/release with the expected large pixel change
  (333,532/480,021 bytes), then a full revert to the original baseline; a
  `ROT 256` (rotary CW) diff showed a smaller, real, localized change
  (5,556 bytes) consistent with a UI value update rather than a full
  redraw; `ANALOG 7 100 0` (Aftertouch) and `ANALOG 8 100 0` (RT Knob 1)
  both completed cleanly but produced no visible screen change - expected
  for sound-synthesis parameters with no on-screen meter, not a failure.
  But none of that exercised the actual wire bytes this module's own
  generator sends, only the same real dispatch TARGETS at a higher call
  layer. Building a kernel module that reads or injects live wire traffic
  directly at the `ReceiveEventBuffer()` layer was attempted two different
  ways: an inline function hook (blocked by this project's safety tooling,
  which draws a boundary around any new kernel module patching/hooking a
  live production binary's internals), and a purely additive call with no
  interception of any existing code path. **The additive-call design
  (`nks4_wire_verify.ko`) WAS built and run on real hardware, with
  explicit authorization**: a real `BTN 9` press+release, encoded as
  genuine wire bytes (`[00 09 30 00]`/`[00 09 40 00]`), was fed directly
  into the real, live `ReceiveEventBuffer()`. dmesg confirms the call
  fires correctly, but a `/dev/fb1` diff before/after showed **0 of
  480,021 bytes differ** - no observable effect, reproducing this
  project's own earlier report exactly. **Root cause diagnosed and
  confirmed, not just theorized**: every wire-decoded event in
  `ReceiveEventBuffer` is gated by `CNKS4EventFilter::FilterEvent()`,
  which requires `bEnabled` (`sInstance+0x1d`) true and `bSuppressAll`
  (`sInstance+0x1e`) false before an event reaches the host FIFO. Fresh
  `objdump -dr` of the real 3.2.2 binary independently confirms offset
  `0x1d` - 8 separate call sites in `ReceiveEventBuffer` compute
  `&sInstance+0x1d` immediately before calling `FilterEvent`. A live
  diagnostic read at the moment of the test showed `bEnabled=0` (and
  `bSuppressAll=0`, ruling out the other half of the gate) - `FilterEvent`
  hits its `if (!bEnabled) return 0` early-exit and silently drops the
  event before it reaches `ProcessNextNKSEvent`. *To validate:* whether
  calling the real, exported `COmapNKS4Driver_StartScanning()` (sets only
  that one byte to 1, nothing else) makes the same wire-level test produce
  a real effect is the natural next step, but adding that call was blocked
  by this project's safety tooling as a new state-mutating call into live
  driver internals (distinct from the read-only diagnostics used above) -
  awaiting explicit authorization to attempt.
- **An intermittent failure where the first configuration-time command
  reply (`CommunicationCheck`) is not delivered or received correctly**,
  causing the driver load to hang rather than fail cleanly, occurs on a
  minority of otherwise-identical boot attempts and remains unresolved.
  It is suspected to be a timing artifact of this kernel's RTAI/I-pipe
  substitution rather than a logic bug in this module's own code, but
  this is not proven. *To validate:* run a larger repeated-boot sample
  (10+ runs of an identical build) to establish whether the failure rate
  is stable and whether it correlates with a specific timing window
  around driver bring-up.
- **The real field names/purposes occupying the padding inserted into
  `struct usb_hcd`/`struct usb_bus` and `struct urb` were never
  identified** - both fixes use generic, documented padding fields rather
  than named, semantically understood ones. *To validate:* obtain real
  Korg kernel source for these structs, or perform further live-kernel
  triangulation of the affected regions the way the `struct urb` gap
  itself was pinned down (comparing disassembled real-kernel field
  accesses against the build tree's own offsets).

## Related documentation

- `reconstructed/OmapNKS4VirtualBoard/README.md` - the emulated NKS4 panel
  gadget device this fork exists to test against.
- `reconstructed/OmapNKS4Module/README.md` - the real NKS4-panel USB
  driver this whole chain exists to load and exercise.
