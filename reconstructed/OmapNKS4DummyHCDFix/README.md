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
independently re-verified against real hardware.

**Partial real-hardware validation (2026-07-21):** a passive inline hook
(kronos_extract.c-style CR0.WP trampoline, source at
`KronosExtract/source/nks4_sniff.c`) on the live `COmapNKS4Driver_ReceiveEventBuffer`
of an already-running `OmapNKS4Module.ko` (real Kronos 2 dev board, real
NKS4 panel) captured genuine RT Knob 1 wire traffic across three separate
manual sweeps (raw `dLo`/`dHi`/`idx`/`op` bytes, ring-buffered, no module
reload). Every captured event carried `idx=0x08` (confirming device_code 8
= RT Knob 1 on the wire) and `op=0x03` (confirming RT Knob 1 dispatches
through the same "aftertouch (calibrated)" opcode path as Aftertouch
itself - not previously stated explicitly). Decoding via this file's own
`raw10 = (dLo<<2)|(dHi>>6)` formula produced clean, monotonically
increasing/decreasing sequences as the knob was turned (three sweeps
spanning raw10 ranges of roughly 504-565, 566-621, and 163-223) - strong
confirmation the wire format and `raw10` reassembly are correct.
**Not confirmed:** the specific `raw10=0x100 -> displayed 16` claim itself.
None of the three sweeps' *captured* events (each limited to the last 16
of a much longer stream by the hook's 16-slot ring buffer) landed on
exactly `raw10=0x100`, and the hook observes wire bytes at function entry,
before `ApplyNKS4Calibration()` and the `out_hi`/display-value transform
run - so the post-calibration displayed value was never directly observed
either way. The general shape of the prediction (smooth, correctly-decoded
analog data on the same channel/opcode) is now real-hardware-backed; the
exact displayed number is still a derivation, not a measurement.

For every OTHER
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
  ends): **Joystick X** feeds its 10-bit raw value (left-shifted 4 bits to
  14-bit MIDI Pitch Bend range, center `0x2000`) through
  `CSTGControllerRTData::CPitchBendFilter::Filter` and emits a real 14-bit
  MIDI Pitch Bend message (`low7 = v&0x7f`, `high7 = (v>>7)&0x7f`) - a
  genuine, different scaling model from the knob/slider group.
  **`CPitchBendFilter::Filter`'s own smoothing arithmetic - RESOLVED
  2026-07-20** via direct disassembly of the real, small (84-byte)
  function (`_ZN20CSTGControllerRTData16CPitchBendFilter6FilterEt`,
  `Decomp/OA.ko_Decomp/OA.ko` offset `0xd5f0`): it is not a smoothing/decay
  filter in the low-pass sense - it is a **center-crossing guarantor**.
  Given the new raw value and the filter's own stored previous value (a
  16-bit field in the filter object), it forces the stored value to snap to
  exactly the true center (`0x2000`) whenever the new value and the
  previous stored value fall on opposite sides of the `0x1fff`/`0x2000`
  boundary (new `<=0x1fff` while previous `>0x2000`, or new `>0x2000` while
  previous `<=0x1fff`) - guaranteeing a genuine bipolar axis like pitch
  bend always emits a true-center message when crossing over, rather than
  jumping straight from e.g. a low negative-bend value to a positive one in
  a single step. When the stored value lands exactly on `0x2000` (having
  just snapped there, or already there) AND the incoming value is
  `<=0x2000`, it additionally writes one extra byte, sourced from a global
  lookup table (`CSTGGlobal::sInstance+0x6b9`), into a second field of the
  filter object - not yet connected to a named downstream consumer, but a
  real, ground-truthed side effect, not a guess. Otherwise, it simply
  tracks the new value directly. Returns whether the stored value actually
  changed. **Ribbon X** shows real touch/release semantics: `param_1!=0`
  means finger-down (stores a displayed value of `0x3ff-param_1` - another
  instance of the same center-inversion style seen in
  `ShortInvertNkS4AnalogValue`, but applied again at this OA.ko layer, not
  a duplicate of it) and sends a 7-bit CC via `SendCCToKG`; `param_1==0`
  (release) resets to a fixed midpoint `0x40`. **FootPedal** is a thin
  wrapper forwarding straight to `CSTGControllerRTData::HandleFootPedalChange`,
  which itself forwards to a generic `HandleControllerChange` CC dispatcher
  with no extra scaling visible at this layer (structurally similar to
  Aftertouch's direct pass-through). **The remaining device codes were
  traced too, closing every one of them structurally**: **Joystick Y**
  (`ProcessJoystickY`) is a genuine, concrete, different model from
  Joystick X - a split bipolar CC pair, not pitch bend: below center,
  `((0x1ff-raw)>>2)&0x7f` drives one CC; above center, `(raw>>2)&0x7f`
  drives a *different* CC; exact center (`0x200`) sends neither - the same
  "raw10 >> 2" reduction pattern as the RT-Knob/Slider group's `byte0`
  math, just split across two unipolar CCs instead of one bipolar value.
  **Ribbon Z is a confirmed, deliberate no-op** - a real, 1-byte function
  (`ret` and nothing else) is wired into the dispatch table for it; not an
  unconfirmed formula, but a settled fact that this axis currently does
  nothing. **Vector Joystick X/Y** both route through the same
  `SendCCToKG` generic CC dispatcher Ribbon X uses, gated on whether a CC
  is actually assigned to that axis (`CSTGGlobal::sInstance+0x6c0`/`0x6c1`
  checked `>=0`, a signed-byte per-axis CC-number assignment table) - a
  real, concrete routing model. **The 16-bit-to-byte reduction -
  RESOLVED 2026-07-20**: disassembly of the real
  `AnalogVectorXHandler`/`AnalogVectorYHandler`
  (`_ZN18CSTGControllerInfo20AnalogVectorXHandlerEtt`/`...YHandlerEtt`,
  `Decomp/OA.ko_Decomp/OA.ko` offsets `0x97900`/`0x97870`) shows
  `SendCCToKG` itself does no reduction at all - it is a thin 5-byte
  MIDI-message packer (status/data1/data2 + two fixed trailer bytes) that
  just forwards to a further helper; the actual reduction happens in the
  caller, and it is the simplest possible one: **`ccValue = raw16 & 0xff`**
  (the incoming ushort's low byte, `%dl`, taken directly with no shift or
  scale). Both handlers have a second, previously undocumented branch,
  gated on a mode flag (`CSTGGlobal::sInstance+0x2f` bit `0x2`): when set,
  BOTH X and Y CCs (if assigned) are sent a fixed value `0x40` (MIDI CC
  center) regardless of the live raw value - a "force to rest position"
  behavior (e.g. plausibly on release/disable), not yet connected to a
  named trigger condition. **Footswitch** (`HandleFootSwitchChange`) is confirmed as the expected
  trivial boolean on/off, plus one extra real detail: a configurable
  polarity-invert (normally-open vs normally-closed jack wiring) and
  change-detection gating before dispatch. **Value Slider**
  (`AnalogValueSliderHandler`) structurally confirms the hardware-tested
  `byte0=value*2` formula's own value flows straight into
  `SendUnsolControl2MessageToUI`, gated by a separate mode flag for
  whether it ALSO sends a CC via `SendCCToKG`. Tempo/Damper remain
  documented only via `nks4_inject.c`'s own hardware-tested non-linear
  curves (not independently re-traced in this pass). *To validate:* trace
  `HandleAnalogController`'s own further use of
  `param2`/`param3` for a specific `device_code`, or compare synthetic
  values against live
  real-hardware readings for that control.
- ~~Real-hardware verification of this exact wire-level event byte
  sequence, as consumed by the real driver's `ReceiveEventBuffer()`, has
  not been performed~~ **RESOLVED (2026-07-20) - genuine wire-level
  real-hardware verification achieved, with a real bug in this project's
  OWN test tooling found and fixed along the way.** Earlier,
  semantic-layer-only checks (via `nks4_inject`'s `/proc/.nks4inject`, an
  unrelated additive-call injection path operating on `OA.ko`'s
  already-translated `CSTGFrontPanel::Handle*` layer, not the raw wire
  bytes) had shown a framebuffer diff before/after an EXIT button
  press/release with the expected large pixel change (333,532/480,021
  bytes), then a full revert to baseline - real, but one layer removed
  from the actual wire protocol this module's generator speaks.
  A dedicated additive-call module, `nks4_wire_verify.ko` (same safety
  posture as `nks4_inject.c`: no hook, no trampoline, no patching of any
  existing code), was built to close that gap by calling the real, live
  `ReceiveEventBuffer()` directly with genuine wire bytes. Its FIRST
  version resolved `COmapNKS4Driver::sInstance`'s address by reading an
  already-relocated pointer out of another function's own machine code
  (`SetProgressBarPercentEh.clone.3`) - a technique already proven
  elsewhere in this project - but got the offset's ADDEND wrong: the
  relocation slot used belongs to a `MOV moffs8,AL` instruction
  (`SetProgressBarPercent`'s own body, storing into `sInstance.bProgress`
  at `+0x1b`), so the raw relocated value in that slot is
  `&sInstance + 0x1b`, not `&sInstance`. Every read AND every
  `ReceiveEventBuffer(this=...)` call this module made before the fix used
  a `this` pointer 0x1b bytes past the real object - past the end of the
  documented 40-byte struct - which is why a live diagnostic read showed
  `bEnabled=0`/implausible `num_keys`/implausible `hw_version`: none of
  those reads, nor the event-buffer calls themselves, were ever touching
  the real struct. (The "`bEnabled=0` gates the event filter" root cause
  this document previously reported was consequently WRONG - a real,
  disassembly-confirmed fact about the struct layout, misapplied to a
  wrongly-computed base address; `CNKS4EventFilter::FilterEvent()` and its
  `sInstance+0x1d`/`+0x1e` offsets remain correctly documented, just not
  the actual explanation here.) After subtracting the `0x1b` addend, a
  fresh baseline read showed `sinstance` shift exactly by `0x1b` as
  predicted, `filter_enabled=1` and `hw_version=1` (both now plausible,
  unlike before) - and a real `BTN 9` press+release, sent as genuine wire
  bytes (`[00 09 30 00]`/`[00 09 40 00]`) directly into the real, live
  `ReceiveEventBuffer()`, produced a **real, large `/dev/fb1` diff:
  334,229 of 480,021 bytes changed** (closely matching the earlier
  `nks4_inject`-layer EXIT test's 333,532 bytes - the same real UI
  transition, now confirmed reachable via the actual wire protocol). A
  wire-level `BTN 8` (EXIT) press/release afterward restored the display
  to **exactly** the original baseline (0 bytes differ), a clean,
  zero-net-disruption round trip. This is now genuine, complete,
  wire-level real-hardware verification, not a semantic-layer proxy for
  it.
- **An intermittent failure where the first configuration-time command
  reply (`CommunicationCheck`) is not delivered or received correctly**,
  causing the driver load to hang rather than fail cleanly, occurs on a
  minority of otherwise-identical boot attempts and remains unresolved.
  It is suspected to be a timing artifact of this kernel's RTAI/I-pipe
  substitution rather than a logic bug in this module's own code, but
  this is not proven. **PARTIAL DATA (2026-07-20):** ran 8 repeated
  boots on `kronosvm` using a bounded, minimal 6-module chain
  (`RTAIVirtualDriver.ko` -> `STGEnabler.ko` -> `STGGmp.ko` ->
  `dummy_hcd_fixed.ko` -> `OmapNKS4VirtualBoard.ko` ->
  `OmapNKS4Module.ko`, no `AT88VirtualChip.ko`/`OmapNKS4VirtualDriver.ko`,
  no module parameters) with a 150s per-boot bound and stall detection
  (45s with no new console output after `OmapNKS4`/`dummy_hcd` mentioned).
  **8/8 runs stalled - 0 completed.** This is a materially different
  result from the `run_vm_virtual_probe_test.sh` hang-rate study
  referenced elsewhere in this project (9/20, 45%, for the *unrelated*
  `vm_virtual_probe=1` standalone path that bypasses this dummy_hcd/gadget
  chain entirely) and should not be read as the same measurement at a
  different rate - different chain, different failure point. Also new: in
  every run examined, the actual stall point was EARLIER than
  `CommunicationCheck` - the virtual root hub's own descriptor read fails
  first (`hub 1-0:1.0: config failed, can't read hub descriptor (err
  -22)`), `OmapNKS4Probe()` is consequently never called
  (`OmapNKS4: Waited for OmapNKS4Probe(). driver state is 0` /
  `OmapNKS4: probe failed`), and - despite `OmapNKS4Init`'s own
  documented graceful-timeout design (see `OmapNKS4VirtualBoard/README.md`
  "Design goal") - no further console output, including this project's own
  `loadoa_test.sh`'s follow-up `kmsg` lines, ever appears; something after
  the printed "probe failed" message stops producing output entirely, a
  genuine hang rather than the designed clean-failure-and-continue path.
  **Caveat:** this minimal harness differs from whatever configuration
  produced the "loads and configures successfully end to end" result
  documented above (bugs #1-6) - missing `AT88VirtualChip.ko`/
  `OmapNKS4VirtualDriver.ko` and module parameters could plausibly affect
  root-hub enumeration timing, so this 8/8 result should be read as "a
  real, reproducible failure mode exists and was newly characterized down
  to the root-hub descriptor read," not as a revised, definitive failure
  rate for the previously-validated configuration. *To validate:* rerun
  with the full module chain (including AT88VirtualChip/
  OmapNKS4VirtualDriver) and any real module parameters, and if the root
  hub `err -22` persists, GDB-attach across a stalled boot to find what,
  after "probe failed" prints, is actually blocking further execution.
- **UPDATED 2026-07-20 - most of this padding now has real, confirmed field
  names; a few smaller gaps remain genuinely unidentified despite
  exhaustive effort, not for lack of trying.** Re-checked against the
  current kernel tree (`/home/build/linux-kronos`'s own `hcd.h`/`usb.h`,
  dated 2026-07-19/20 fix comments): the largest single gap, `struct
  usb_hcd`'s 12-byte shared-roothub cluster, is now fully named and
  confirmed via direct disassembly of the real kernel's own
  `usb_create_shared_hcd()`/`usb_hc_died()` (recovered from `kronos.img`'s
  own symbolicated vmlinux) - `bandwidth_mutex@0xbc`, `shared_hcd@0xc0`,
  `primary_hcd@0xc4`, matching real upstream Linux's own shared-USB2/3-
  root-hub fields, evidently backported into Korg's vendor kernel while
  keeping the same `2.6.32.11-korg` version string. Three smaller 4-byte
  gaps remain genuinely unnamed after real effort, each with a specific,
  reasoned (not confirmed) hypothesis: `struct usb_hcd`'s kref-to-
  `rh_timer` gap (likely `irq_descr[]` grew to 28 bytes, or an inserted
  field); `struct usb_bus`'s bitfield-region gap (likely a GCC-version
  difference in i386 bitfield storage-unit alignment between the real
  kernel's GCC 4.5 and this tree's host GCC 12, empirically reproduced
  with a standalone `offsetof()` probe but not independently confirmed as
  THE cause); and `struct urb`'s `pipe`-to-`status` gap, which was checked
  against the **live production Kronos itself** (not just a factory
  image - `/boot/bzImage` pulled directly from 192.168.100.15, extracted
  to vmlinux, and disassembled) across every generic urb-lifecycle
  function that touches nearby fields, all of which independently confirm
  the surrounding offsets without ever touching this specific one. This
  *is* the "further live-kernel triangulation" this item's own next step
  asked for, already performed as thoroughly as static/disassembly
  analysis allows - what remains open would need either real Korg kernel
  source (unavailable) or a code path this triangulation hasn't reached
  yet.

## Related documentation

- `reconstructed/OmapNKS4VirtualBoard/README.md` - the emulated NKS4 panel
  gadget device this fork exists to test against.
- `reconstructed/OmapNKS4Module/README.md` - the real NKS4-panel USB
  driver this whole chain exists to load and exercise.
