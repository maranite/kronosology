# OmapNKS4ProbeInject - non-working cross-module probe injector, kept for reference

This module attempted to call `OmapNKS4Module.ko`'s exported `OmapNKS4Probe()`
from a *separate* kernel module, loaded while `OmapNKS4Module.ko`'s own
`init_module()` was still blocked in its own probe-wait. The intent was to
bring up a virtual NKS4 board without needing a real USB device, real
`dummy_hcd` gadget emulation, or any wire traffic at all - `OmapNKS4Probe()`
itself only allocates and configures URBs; it does not submit them.

## Why this approach cannot work

The Linux module loader (`kernel/module.c`) refuses to resolve a symbol
exported by a module that is still in `MODULE_STATE_COMING` - i.e. still
inside its own `init_module()`, which is exactly the state
`OmapNKS4Module.ko` is in while waiting on its probe-completion timeout. No
amount of timing or retry logic in a second module can work around this: it
is a hard rule enforced by the loader itself, not a race that can be won by
loading fast enough or waiting long enough. Attempting to insmod this module
against a still-initializing `OmapNKS4Module.ko` fails with a message from
the loader to the effect of "gave up waiting for init of module
OmapNKS4Module."

## The working replacement

The functioning version of this idea lives inside `OmapNKS4Module.ko`
itself, not as a second module: a `vm_virtual_probe` module parameter calls
the real `OmapNKS4Probe()` inline, from `OmapNKS4Init()`, before its own
probe-completion wait even starts. This avoids cross-module symbol
resolution and the `MODULE_STATE_COMING` restriction entirely, since the
call happens synchronously within the same module's own `init_module()`.
See `reconstructed/OmapNKS4Module/README.md`'s "`vm_virtual_probe`" section
for the current implementation and its state machine.

`usb.cpp`'s `OMAPNKS4_EXPORT_SYMBOL`/`EXPORT_SYMBOL` plumbing that this
module depended on to reach `OmapNKS4Probe()` from outside has since been
removed from `OmapNKS4Module.ko`, since nothing else consumes it.

## Technique: constructing a fake usb_interface

Independent of whether the cross-module approach works, the technique this
module used to synthesize a fake device is a real, reusable finding:
`OmapNKS4Probe()` (`usb.cpp` in `OmapNKS4Module.ko`) never dereferences a
real kernel `struct usb_interface`/`usb_device` through their normal field
accessors. It reinterprets the interface pointer as a raw int array and
walks fixed byte offsets recovered from the shipping binary's disassembly:

| Field | Offset | Contents |
|---|---|---|
| `intf[0]` | 0x00 | altsetting pointer |
| `intf[7]` | 0x1c | `usb_device *` ("dev") |
| `dev - 100` | - | this driver's own `sDeviceInstance` private struct; `[0]` = device address, `[7]` (0x1c) = speed (3 = `USB_SPEED_HIGH`) - read by the URB-configure helpers, not by `OmapNKS4Probe()` itself |
| `dev + 0xb8` | u16 | idVendor |
| `dev + 0xba` | u16 | idProduct |
| `altsetting + 4` | u8 | bNumEndpoints |
| `altsetting + 0xc` | ptr | endpoint descriptor array, 0x2c bytes/entry |
| `entry + 2` | u8 | bEndpointAddress |
| `entry + 3` | u8 | bmAttributes |
| `entry + 4` | u16 | wMaxPacketSize |
| `entry + 6` | u8 | bInterval |

Because none of this requires a live `usb_device`/`usb_bus`/`usb_hcd`,
correctly-shaped plain `kmalloc`'d memory satisfies `OmapNKS4Probe()` exactly
as well as a real enumerated device would. This module built that memory
with vendor `0x0944` / product `0x1005`, one interrupt-IN endpoint (`0x81`)
and one bulk-OUT endpoint (`0x02`), both `wMaxPacketSize` 64, matching the
descriptors `OmapNKS4VirtualBoard.c` uses for its own gadget-based emulation.

One detail worth preserving for anyone reusing this technique: on success,
`OmapNKS4Probe()` stashes pointers derived from the injected buffers
(`sDeviceInstance`, the URBs' own `+0x28` "dev" field) into
`OmapNKS4Module.ko`'s live state, so those buffers cannot be freed
immediately after the call without leaving dangling pointers a later
disconnect/cleanup path could touch.

## Status

This directory (source and Makefile) is kept only as a record of the dead
end and the reasoning above for why it is a dead end. It is not built or
loaded as part of any current workflow; `reconstructed/OmapNKS4Module/README.md`
documents the working in-module replacement.
