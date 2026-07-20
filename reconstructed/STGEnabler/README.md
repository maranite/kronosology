# STGEnabler.ko - reconstructed source

Reverse-engineered, compilable source for the Korg Kronos `STGEnabler.ko` kernel
module (firmware 3.2.2). Target: Linux 2.6.32.11 + RTAI, x86-32, built with
`gcc -mregparm=3`.

This is a functionally faithful reconstruction recovered from the shipping binary
in Ghidra, not a byte-for-byte rebuild. The logic, exported symbols and call
sequences match the original.

## What this module is

`STGEnabler` is a GPL symbol shim (a "GPL condom"). The Korg sound-engine modules
(`OA.ko`, `OmapNKS4Module.ko`, `loadmod.ko`, `GetPubIdMod.ko`, ...) are proprietary
and therefore may not link the kernel's `EXPORT_SYMBOL_GPL` symbols directly.
`STGEnabler` is licensed GPL, calls those symbols itself, and re-exports them
under neutral `stg_*` names via plain `EXPORT_SYMBOL()`. The proprietary modules
link only the `stg_*` shims.

It also provides a few real helpers and brings up the RTAI timer at load.

## Exported symbols

| Symbol | Kind | Wraps / does |
|---|---|---|
| `stg_usb_alloc_urb` | shim | `usb_alloc_urb` |
| `stg_usb_free_urb` | shim | `usb_free_urb` |
| `stg_usb_submit_urb` | shim | `usb_submit_urb` |
| `stg_usb_register_driver` | shim | `usb_register_driver` |
| `stg_usb_deregister` | shim | `usb_deregister` |
| `stg_usb_driver_claim_interface` | shim | `usb_driver_claim_interface` |
| `stg_sched_setscheduler` | shim | `sched_setscheduler` |
| `stg_set_cpus_allowed` | helper | wraps a raw cpumask word -> `set_cpus_allowed_ptr` |
| `stg_cpumask_of_cpu` | helper | first word of `get_cpu_mask(cpu)` |
| `stg_rtai_setup` | helper | `rt_linux_use_fpu(1)` + `rt_set_oneshot_mode()` + `start_rt_timer(0)` |
| `stg_mkdir` | helper | in-kernel `mkdir(2)` (path_lookup -> lookup_create -> vfs_mkdir) |
| `stg_get_free_diskspace` | helper | sums free blocks from Korg-FS `s_fs_info`, x blocksize |

`init_module` calls `stg_rtai_setup()`; `cleanup_module` is empty.

## Load order

`STGEnabler` must be loaded before any proprietary STG module that imports
`stg_*` symbols - it sits at the bottom of the dependency stack.

## Notes on fidelity

- `stg_get_free_diskspace` walks Korg's private superblock structure
  (`sb->s_fs_info`) directly rather than using `vfs_statfs()`. The kernel headers
  don't contain that struct, so the recovered field offsets (`struct
  korgfs_sb_info`, `sb->s_fs_info` at `+0x180`, `sb->s_blocksize` at `+0x0c`) are
  reproduced verbatim from the binary and are kernel-/build-specific. Like the
  original, it does not `path_put()` the looked-up path.
- A handful of imported symbols have no public prototype (`path_lookup`,
  `lookup_create`, the RTAI `rt_*`/`start_rt_timer`); they are `extern`-declared
  in the source. The Kronos kernel exports them; a stock 2.6.32 tree may need
  `lookup_create`/`path_lookup` un-static'd, or these call sites adjusted to use
  `kern_path` instead.
- Struct offsets such as `super_block` `+0x180`/`+0x0c` assume this exact kernel
  build. Rebuild against a kernel tree matching the Kronos's own build - a
  generic, unpatched 2.6.32.11 tree will not have the same struct layout.

## Building

```sh
make KDIR=/path/to/kronos-kernel-tree
# or, against the running kernel:
make
```

Produces `STGEnabler.ko`. `make clean` removes build artifacts.

## Known limitations

- The `struct korgfs_sb_info` offsets used by `stg_get_free_diskspace` are
  reproduced from one specific binary/kernel build and are not derived from a
  public struct definition. If they turn out to be wrong for a given build, disk
  free-space reporting would misread arbitrary superblock fields rather than
  failing loudly. To validate: cross-check `+0x180`/`+0x0c` against a symbol
  table or debug build of the matching kernel, if one becomes available.
- The missing `path_put()` after the `stg_mkdir` path lookup is reproduced
  faithfully from the original binary; whether this is an intentional resource
  trade-off or a genuine leak in the original code is unconfirmed. To validate:
  trace `stg_mkdir` call sites to see whether the leaked reference is bounded
  (e.g. by module lifetime) or can accumulate under repeated calls.
