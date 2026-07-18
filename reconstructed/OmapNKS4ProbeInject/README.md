# OmapNKS4ProbeInject.ko — superseded, kept for the record

**Superseded 2026-07-17.** This module tried to invoke `OmapNKS4Module.ko`'s exported
`OmapNKS4Probe()` from a *separate* kernel module, loaded while `OmapNKS4Module.ko`'s
own `init_module()` was still blocked in its 10-second probe-wait.

It cannot work: the first live test hit a real kernel message,
`"OmapNKS4ProbeInject: gave up waiting for init of module OmapNKS4Module."` - the
kernel's own module loader (`kernel/module.c`) refuses to resolve a symbol exported by
a module still in `MODULE_STATE_COMING` (i.e. still inside its own `init_module()`,
which is exactly the function doing the waiting here). No amount of timing/retry logic
fixes that; it's a hard rule of the module loader, not a race to win.

The working replacement lives **inside `OmapNKS4Module.ko` itself**: a
`vm_virtual_probe` module parameter that calls the real `OmapNKS4Probe()` inline, from
`OmapNKS4Init()`, before its own wait even starts - no second module, no cross-module
symbol resolution, no timing dependency at all. See
`OmapNKS4Module/README.md`'s own "`vm_virtual_probe`: a working virtual NKS4 board"
section for the full story and a live boot-test transcript. `usb.cpp`'s own
`OMAPNKS4_EXPORT_SYMBOL`/`EXPORT_SYMBOL` plumbing this module depended on has been
removed from `OmapNKS4Module.ko` as dead weight now that nothing consumes it.

This directory (source + Makefile) is left in place purely as a record of the dead end
and why it's a dead end - not built or deployed by any current test path.
