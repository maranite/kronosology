# OmapVideoModule.ko — Kronos `/dev/fb1` front-panel framebuffer driver

Reconstructed 2026-07-21 from `3.2.1 update contents/sbin/OmapVideoModule.ko`
(ELF32 LSB relocatable, i386, **not stripped** — full symtab, BuildID
`b167779090bcdf61f1c48d035d2d8afb23e4267d`). Four byte-identical copies exist
elsewhere in the repo (md5 `bcce18c4128eee8596057d790000225d`) — irrelevant,
same binary.

| Property | Value |
|---|---|
| Path on device | `/sbin/OmapVideoModule.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | ~9.8 KB (.text) |
| Own functions | 19 (14 driver funcs + init/cleanup + 4 static cfb bitfill helpers) |
| modinfo | `author=Korg R&D`, `description=Korg OMAP Video interface Support`, `license=GPL`, `version=1.0.0`, `depends=` (empty), `vermagic=2.6.32.11-korg SMP preempt mod_unload ATOM ` |
| Load order | `STGEnabler.ko → STGGmp.ko → OmapNKS4Module.ko → **OmapVideoModule.ko** → GetPubIdMod.ko → loadmod.ko → KorgUsbAudioDriver.ko → USBMidiAccessory.ko → OA.ko` (see `MASTER_REFERENCE.md` §6) |
| Reconstructed source | `kronosology/reconstructed/OmapVideoModule/` |

**Note:** this doc's prose was written directly by the RE agent (cloud), not
delegated through the `qwen-docs`/Ollama pipeline — that delegation instruction
was only added to `re-decompiler.md` after this doc was already produced.
Facts were independently re-verified against the built `.ko` regardless.

## Architecture summary

There is no real OMAP display controller behind this driver — it is a plain
`vmalloc()`-backed software framebuffer (`videomemory`, sized by the
`videomemorysize` module param, default 800×600 = 480000 bytes). Actual pixel
delivery to the physical NKS4 panel happens entirely through
**`OmapNKS4Module.ko`**'s USB/serial protocol, driven by three Korg-private
ioctls (`OMAPFB_COLORPAL`/`OMAPFB_FILLDATA`/`OMAPFB_FLUSH` in this repo's
existing terminology — `kronosology/docs/.../KronosFB` README already
documented these independently from the userspace side; this reconstruction
supplies the kernel-module internals that produce that documented behavior,
and the two sides cross-validate exactly — same three ioctl numbers, same
argument shapes, arrived at from opposite directions).

In short: userspace `mmap()`s `/dev/fb1`, draws into the plain malloc'd
buffer, then must call the flush ioctl (`OMAPFB_FLUSH`, cmd `0x400c7207`,
which this module's `omapfb_ioctl` routes to `OmapNKS4SendPixelDataRegion()`
in `OmapNKS4Module.ko`) — writing to `videomemory` alone does **not** appear
on the physical panel, matching this project's own prior finding
("mmap does not auto-refresh") and an independent third-party writeup cited
in `MASTER_REFERENCE.md`.

### File layout (mirrors the original binary's embedded `.symtab` FILE markers)

The original's symtab literally records six source files
(`OmapVideoMod.c`, `OmapVideo.c`, `omapfb_fillrect.c`, `omapfb_copyarea.c`,
`omapfb_imgblt.c`, plus the auto-generated `OmapVideoModule.mod.c`) —
reproduced 1:1 here:

| File | Contents |
|---|---|
| `omapvideo_internal.h` | Shared declarations: globals, fb_ops prototypes, ioctl command table, OmapNKS4Module.ko externs |
| `OmapVideoMod.c` | `MODULE_AUTHOR/DESCRIPTION/LICENSE/VERSION`, `omapfb_init`/`omapfb_exit` (module_init/module_exit targets) |
| `OmapVideo.c` | All `fb_ops` callbacks, `omapfb_probe`/`omapfb_remove`, `/proc` glue, the two exported dimension accessors, and all module-global data (`omapfb_default`, `omapfb_fix`, `omapfb_ops`, `omapfb_driver`) |
| `omapfb_fillrect.c` | `bitfill_aligned[_rev]`, `bitfill_unaligned[_rev]`, `omapfb_fillrect` |
| `omapfb_copyarea.c` | `bitcpy`, `bitcpy_rev`, `omapfb_copyarea` |
| `omapfb_imgblt.c` | `cfb_tab8/16/32`, `color_imageblit`, `slow_imageblit`, `fast_imageblit`, `omapfb_imageblit` |
| `omapfb_draw.h` | Local, bswapmask-free bit-twiddling helpers shared by the three blit files |
| `Makefile` | Kbuild module Makefile, mirrors `OmapNKS4Module/Makefile`'s dual-mode pattern |

### The blit routines are an *older* cfb generic-fb snapshot, not the current kernel's

`bitfill_aligned`/`bitfill_unaligned`/`_rev` variants and `bitcpy`/`bitcpy_rev`
decompile with **no `bswapmask` parameter at all** (6/8 args instead of the
current kernel's 7/9), and `fast_imageblit()`'s color-expansion table is a
**single** `cfb_tab8`/`cfb_tab16`/`cfb_tab32` per bit depth (confirmed via
`nm` — one symbol each, no `_be`/`_le` pair), not the current kernel's
runtime `fb_be_math()`-selected pair. Both facts point the same direction:
this module statically links an early, pre-byte-swap-aware, x86-only build
of `drivers/video/cfbfillrect.c`/`cfbcopyarea.c`/`cfbimgblt.c` — from before
`CONFIG_FB_CFB_REV_PIXELS_IN_BYTE` existed upstream — rather than the
2.6.32.11 tree's own current copy of those files (confirmed present at
`/home/build/linux-kronos/drivers/video/cfb*.c`, and diffed against: those
*do* have bswapmask/be-le tables, so they were **not** what Korg compiled
in here). `omapfb_draw.h` documents this in detail and hardcodes the
little-endian-only `FB_SHIFT_HIGH`/`FB_SHIFT_LOW`/`FB_LEFT_POS` forms rather
than pulling the real, bswap-aware macros from `<linux/fb.h>`.

The rest of the algorithm (aligned/unaligned pattern fill, forward/reverse
bitwise copy, mono color-expansion fast path) is otherwise a straight,
line-for-line match to the upstream algorithm minus that one parameter —
confirmed by decompiling `bitfill_aligned` and comparing it instruction-
by-instruction against the current kernel source before writing the
translation, not assumed from the function name alone.

## Function-by-function summary (19 own functions)

| Function | File | Behavior |
|---|---|---|
| `GetScreenXDimension` | OmapVideo.c | `EXPORT_SYMBOL`'d. Returns literal `800`. |
| `GetScreenYDimension` | OmapVideo.c | `EXPORT_SYMBOL`'d. Returns literal `600`. |
| `omapfb_init` | OmapVideoMod.c | `module_init` target: registers `omapfb_driver`, allocates+adds a `"omapfb"` platform_device, then `OmapVideoProcInitialize()`. |
| `omapfb_exit` | OmapVideoMod.c | `module_exit` target: tears down `/proc` entry (if up), unregisters platform_device + platform_driver. |
| `omapfb_probe` | OmapVideo.c | `platform_driver.probe`. `vmalloc()`s `videomemory`, zeroes it, `framebuffer_alloc(1024, &pdev->dev)`, populates `var`/`fix`/`fbops`/`screen_base`, repurposes `framebuffer_alloc`'s extra 1024-byte `par` block as the 256-entry `pseudo_palette` (confirmed via raw disasm field-offset walk, not decompiler guess), `fb_alloc_cmap`, `register_framebuffer`, calls `OmapNKS4UpdateScreenInfo(videomemory, 800, 600)`. |
| `omapfb_remove` | OmapVideo.c | `platform_driver.remove`. Unregisters fb, frees `videomemory`, releases the `fb_info`. |
| `omapfb_check_var` | OmapVideo.c | `fb_ops.fb_check_var`. Clamps xres/yres/bpp to supported values (1/8/16/24/32), computes RGB bitfield layout per bpp (special-cases 16bpp 565 vs 1555 based on caller's `transp.length` hint), rejects modes that exceed `videomemorysize`. |
| `omapfb_set_par` | OmapVideo.c | `fb_ops.fb_set_par`. Recomputes `fix.line_length` from `var.xres_virtual`/`bits_per_pixel`, 32-bit-row-aligned. |
| `omapfb_setcolreg` | OmapVideo.c | `fb_ops.fb_setcolreg`. Packs r/g/b/transp into the `pseudo_palette` entry for TRUECOLOR visual only. |
| `omapfb_pan_display` | OmapVideo.c | `fb_ops.fb_pan_display`. Standard bounds-checked xoffset/yoffset update, toggles `FB_VMODE_YWRAP` in `info->var.vmode`. |
| `omapfb_mmap` | OmapVideo.c | `fb_ops.fb_mmap`. Page-by-page `vmalloc_to_pfn` + `remap_pfn_range`. **Uses a hardcoded pgprot value `0x27`, not `vma->vm_page_prot`** — confirmed via raw disassembly (immediate operand, not a load from the vma); reproduced verbatim. |
| `omapfb_sys_read` / `omapfb_sys_write` | OmapVideo.c | Generic vmalloc-backed `fb_read`/`fb_write` (the standard `fb_sys_read`/`fb_sys_write` idiom), bounds-checked against `screen_size`/`fix.smem_len`. |
| `omapfb_ioctl` | OmapVideo.c | `fb_ops.fb_ioctl`. 14-command dispatch table, magic `'r'` — see below. |
| `omapfb_fillrect` | omapfb_fillrect.c | `fb_ops.fb_fillrect` — see cfb section above. |
| `omapfb_copyarea` | omapfb_copyarea.c | `fb_ops.fb_copyarea` — see cfb section above. |
| `omapfb_imageblit` | omapfb_imgblt.c | `fb_ops.fb_imageblit` — see cfb section above. |
| `bitfill_aligned`/`_rev`, `bitfill_unaligned`/`_rev` | omapfb_fillrect.c | `static`. Word-aligned/unaligned pattern fill (ROP_COPY) and invert (ROP_XOR) primitives used by `omapfb_fillrect`. |
| `OmapVideoProcInitialize`/`Done`/`Initialized` | OmapVideo.c | `create_proc_entry("omapfb", ...)` / `remove_proc_entry` / `gProc != NULL` accessor. |
| `OmapVideoProcRead`/`Write` | OmapVideo.c | `/proc/omapfb` handlers: read reports `"OMAP Video fb%d:\n"`; write just logs the string via `printk` (no actual command parsing). |

`omapfb_fillrect`/`omapfb_copyarea`/`omapfb_imageblit` are `GLOBAL` text
symbols with **no `__ksymtab` entry** — confirmed via `nm`: they were left
non-`static` (unusual — normally either `static` or `EXPORT_SYMBOL`'d) but
never exported. Reproduced as-is (external linkage, no `EXPORT_SYMBOL`) —
the only two genuinely `EXPORT_SYMBOL`'d functions in the whole module are
`GetScreenXDimension`/`GetScreenYDimension`.

## `omapfb_ioctl` command table

Every command below was decoded from the raw x86 disassembly (not just the
Ghidra decompilation, which mangles this function's compare-tree badly
enough to be actively misleading in a few spots — e.g. it initially looked
like cmd `0x80047202` called `COmapNKS4_GetProgressBarPercent()`, but the
actual jump target for that cmd is a bare `mov [x], 0x37` stub; the real
`GetProgressBarPercent()` call is on cmd `0x4004720a` instead). Magic `'r'`
(`0x72`), standard `_IOW`/`_IOR` encoding.

| cmd | Macro | Direction (encoded) | OmapNKS4Module call | Notes |
|---|---|---|---|---|
| `0x40047201` | `OMAPFB_IOCTL_PING` | W, 4B | *(none)* | Reads+discards 4 bytes; pointer-validation stub only |
| `0x80047202` | `OMAPFB_IOCTL_GETPROGRESSPCT2` | R, 4B | *(none)* | **Hardcoded stub**: always returns `0x37` (55) |
| `0x40047203` | `OMAPFB_IOCTL_DUMPPALETTE` | W, 4B | — | Reads a start index, `printk`s `pseudo_palette[idx..255]` |
| `0x40047204` | `OMAPFB_IOCTL_DUMPVIDMEM` | W, 4B | — | `printk`s 8 bytes of `videomemory` at a user-given offset |
| `0x40087205` | `OMAPFB_IOCTL_INITLCDREGS` | W, 8B | `OmapNKS4InitLCDRegs(s8,s8,s32)` | Return value passed straight through, no copy_to_user |
| `0x40047206` | `OMAPFB_IOCTL_XAXISBYTESIZE` | W, 4B | `OmapNKS4XAxisByteSize(int)` | Return value passed straight through |
| `0x400c7207` | `OMAPFB_IOCTL_SENDPIXELDATA` (= repo's `OMAPFB_FLUSH`) | W, 12B | `OmapNKS4SendPixelDataRegion(int,int,int)` | The panel-flush ioctl — matches `KronosFB/README.md`'s independently-documented `OMAPFB_FLUSH {count,offset,width}` exactly |
| `0x40107208` | `OMAPFB_IOCTL_SENDFILLDATA` (= repo's `OMAPFB_FILLDATA`) | W, 16B | `OmapNKS4SendFillData(s8,s32,s32,s32)` | Matches `KronosFB`'s `OMAPFB_FILLDATA {fill,count,off,width}` |
| `0x40047209` | `OMAPFB_IOCTL_UPDATECOLORPAL` (= repo's `OMAPFB_COLORPAL`) | W, 4B | `OmapNKS4UpdateColorPal(s8,s8,s8,s8)` | 4 packed signed bytes (index + R/G/B), matches `KronosFB`'s documented `u8[4]{index,R,G,B}` |
| `0x4004720a` | `OMAPFB_IOCTL_GETPROGRESSPCT` | W (but only copy_to_user'd) | `COmapNKS4_GetProgressBarPercent()` | Zero-extended byte result copied back |
| `0x4004720b` | `OMAPFB_IOCTL_SETPROGRESSPCT` | W, 4B | `COmapNKS4_SetProgressBarPercent(u8)` | **Quirk**: copies back 4 bytes read from `*info` (i.e. `info->node`), not the percent value — reproduced verbatim, almost certainly an unintentional register-reuse bug in the original rather than deliberate |
| `0x4004720c` | `OMAPFB_IOCTL_INCPROGRESSBAR` | W (but no input read) | `COmapNKS4_IncProgressBar()` | Bare trigger + copy_to_user of the return value |
| `0x4004720d` | `OMAPFB_IOCTL_ADDTOPROGRESSBAR` | W, 4B | `COmapNKS4_AddToProgressBar(int)` | Return value copied back to user |
| `0x4004720e` | `OMAPFB_IOCTL_GETTITLESCRVER` | W (but no input read) | `COmapNKS4_GetTitleScreenVersion()` | Bare trigger + copy_to_user of the return value |

Several `_IOW`-encoded commands only ever `copy_to_user` (never
`copy_from_user`) in the real disassembly — the encoded direction bit does
not always match the actual data flow. This is reproduced faithfully rather
than "corrected", since the wire protocol (the cmd number itself) is what
matters for compatibility, not the encoding's internal consistency.

## `/dev/fb1` video memory layout

800×600, 8bpp (default mode; up to 32bpp supported by `omapfb_check_var`),
`FB_VISUAL_TRUECOLOR`, `line_length` = 800 (8bpp × 800px, 32-bit row-aligned
— already a multiple of 32 bits at 8bpp so no padding). Backing store is a
single `vmalloc()` block (`videomemory`, default size 800×600 = 480000
bytes, overridable via the `videomemorysize` module param). This matches
`KronosFB/README.md`'s independently-derived "800×600, 8bpp indexed
(256-color)" model exactly, arrived at from the opposite (userspace/
hardware-behavior) direction — see that doc for the full `mmap` + `OMAPFB_COLORPAL`
+ `OMAPFB_FLUSH` draw sequence used by `fbshow`.

## Reconstruction verification

Built via `make` (dual-mode Makefile, `KDIR=/home/build/linux-kronos`,
mirroring `OmapNKS4Module/Makefile`'s established pattern for this
CIFS-mount-can't-hold-symlinks constraint). **Build: clean, rc=0.** Only
warning is the pre-existing `include/linux/log2.h:22: warning: ignoring
attribute 'noreturn' because it conflicts with attribute 'const'` — a
modern-GCC-vs-2.6.32-headers artifact already seen (and accepted) building
`OmapNKS4Module.ko` against the same tree, unrelated to this module's code.

| Check | Original | Rebuilt | Result |
|---|---|---|---|
| `__ksymtab` exports | `GetScreenXDimension`, `GetScreenYDimension` | same, same offsets/sizes | ✅ match |
| Kernel imports (23) + OmapNKS4Module imports (11) = 34 `U` symbols | full list in task brief | identical set, `diff` empty | ✅ **identical** |
| Own GLOBAL/local text symbols | 19 functions + `init_module`/`cleanup_module` aliases | all 19 present under the same names | ✅ match |
| `modinfo` fields | version/author/description/license/vermagic/depends | identical (byte-for-byte on every field except `srcversion`, which is a source-hash and expected to differ) | ✅ match |
| `__mod_author33`/`__mod_license31`/etc. | present with `__LINE__`-derived numeric suffixes | present, different numeric suffixes (`__mod_author21` etc.) | expected — suffix is literally the source line number, and our file isn't a byte-for-byte line match |
| `CSWTCH.26` (compiler-synthesized switch-value table, imgblt.c) | present | **absent** | GCC 12 didn't need one for our 3-case `switch(bpp)` — pure codegen difference, not a semantic one |
| `__func__.NNNNN` (3x, in the `/proc` write/init/done printks) | present, sizes 19/24/18 bytes | present (`__func__.0/.1/.2`), same 3 functions use `__func__` the same way | ✅ match (numeric suffix differs, same as `__mod_*`) |
| `*.cold` split symbols (`omapfb_ioctl.cold`, `omapfb_fillrect.cold`, `OmapVideoProc{Write,Initialize}.cold`) | absent | present | GCC 12's `-freorder-blocks-and-partition` (default at `-O2`) splits unlikely paths into `.text.unlikely`; the original 2.6.32-era GCC didn't do this by default — compiler-version artifact only |

No genuine symbol is missing and no genuine symbol is spurious; every
divergence above is either a compiler-version codegen artifact (cold-split,
absent switch table) or an inherent, un-reproducible internal counter
(`__LINE__`-derived static names) — both explicitly anticipated by the task
brief's "sizes/names may differ slightly from compiler differences" note.

## Judgment calls / open uncertainties

- **`OMAPFB_IOCTL_GETPROGRESSPCT2` (cmd `0x80047202`) really is a dead
  stub** in the original — always returns `0x37` regardless of actual
  progress state, never calls into `OmapNKS4Module.ko`. Verified twice (once
  via decompiler misreading, once via raw disassembly re-check) before
  accepting this as intentional-in-the-binary rather than a decompiler
  artifact.
- **`omapfb_mmap`'s hardcoded `pgprot` value `0x27`** is reproduced as a raw
  `__pgprot(0x27)` rather than the conventional `vma->vm_page_prot` — this
  looks like a genuine upstream shortcut/bug (it discards whatever
  protection flags the VFS/mmap path set up), not a transcription error on
  this side.
- **`OMAPFB_IOCTL_SETPROGRESSPCT`'s copy-back-the-wrong-field quirk** (see
  ioctl table above) is kept byte-for-byte faithful rather than "fixed",
  per this project's translation philosophy of matching binary behavior
  exactly, warts included.
- **`omapfb_ops.owner` and `omapfb_driver.driver.bus` are left at their
  observed zero/auto values** rather than hand-set — `.driver.owner` is set
  to `THIS_MODULE` (standard convention; the raw `.data` bytes show 0 only
  because that field is populated by a `.rel.data` relocation at module-load
  time, not visible as a static value in the file).
- Ioctl command names use this reconstruction's own naming
  (`OMAPFB_IOCTL_*`) rather than retrofitting the shorter names
  `KronosFB/README.md` already uses (`OMAPFB_COLORPAL`/`OMAPFB_FLUSH`/
  `OMAPFB_FILLDATA`) — the numeric values and argument shapes are identical
  either way; the mapping is called out explicitly in the ioctl table above
  so the two docs cross-reference cleanly without requiring a rename in
  either.

## Real-hardware load readiness (2026-07-21)

An initial ad-hoc test on a real Kronos 2 dev board hit
`OmapVideoModule: Unknown symbol COmapNKS4_GetTitleScreenVersion` (and 10
similar errors) in dmesg, raising a real concern: the genuine stock
binary's own `depends=` field is empty (see `modinfo` row above), so it
wasn't obvious the standard kernel module-loading mechanism would ever
resolve these 11 `OmapNKS4Module.ko` symbols at all.

Root-caused and closed out — see
[`OmapNKS4Module.ko.md`](OmapNKS4Module.ko.md)'s "Exported kernel symbols"
section for the full writeup. Short version: the genuine stock
`OmapNKS4Module.ko` (confirmed via `readelf`/`nm` on a copy pulled live from
the dev board) really does export all 11 needed symbols via a normal
`__ksymtab`; the empty `depends=` is just a build-time metadata artifact
with no effect on runtime resolution; and real `loadoa.c` already inserts
`OmapNKS4Module.ko` before `OmapVideoModule.ko` (`loadoa/loadoa.c:442-447`).
The earlier dmesg failure was purely this session's own manual test
inserting things out of order.

**Practical implication:** this reconstruction's `OmapVideoModule.ko`
should load cleanly against a board where the genuine stock
`OmapNKS4Module.ko` is already resident (its real exports are already in
kernel memory) — no dependency on this project's own `OmapNKS4Module`
reconstruction being fixed up first (that reconstruction is separately
confirmed to still be missing all 11 `EXPORT_SYMBOL()` calls).

**CONFIRMED on real hardware (2026-07-21):** `insmod` returned `rc=0` on
the dev board with the genuine stock `OmapNKS4Module.ko` already loaded —
zero "Unknown symbol" errors, all 11 imports resolved. Own init log:
`COmapNKS4VideoAPI::UpdateScreenInfo() base = 0x58d7f000, X=800, Y=600`
and `fb1: Virtual OMAP frame buffer device, using 468K of video memory`.
`lsmod` showed `OmapNKS4` refcount incremented to 1 with `OmapVideoModule`
as dependent, confirming real kernel-tracked symbol resolution, not
coincidental success. `/dev/fb1` needed a manual `mknod c 29 1` (this
rescue image has no devtmpfs), then a basic read worked cleanly.

Follow-up test (`KronosFB/narrow_flush_test.c`) drove a deliberately
narrow `OMAPFB_FLUSH` (137×60px, `width=137` vs. the panel's real
`line_length=800`) through `SendPixelDataRegion` — see this project's
`reconstructed/OmapNKS4Module/README.md`'s "Known limitations" entry for
the full result: rendered correctly on the physical panel (static,
uncorrupted, user-measured proportions matching the requested rect). Both
this module and the specific untested multi-chunk row-wrap gap in
`OmapNKS4Module` are now real-hardware-confirmed.

## Paths

- Source: `kronosology/reconstructed/OmapVideoModule/{omapvideo_internal.h,omapfb_draw.h,OmapVideoMod.c,OmapVideo.c,omapfb_fillrect.c,omapfb_copyarea.c,omapfb_imgblt.c,Makefile}`
- Built artifact: `kronosology/reconstructed/OmapVideoModule/OmapVideoModule.ko`
- This doc: `kronosology/docs/modules/OmapVideoModule.ko.md`
