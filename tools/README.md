# `tools/` — assorted scripts from the analysis

Standalone scripts that were produced during the analysis but aren't part of the
main deployment flow. Kept here as references and starting points for future work.

## `patch_omapnks4_cleanup.py`

A complete worked example of **adding a new external-symbol import** to a Linux 2.6
relocatable kernel module (`OmapNKS4Module.ko`) by surgical ELF editing — no
recompilation, no source code.

What it does end-to-end:

1. Appends a new symbol name to `.strtab` (extending the section in place since
   `.strtab` is last in the file)
2. Appends a new `.symtab` (original entries + 1 new `SHN_UNDEF` entry for
   `usb_reset_device`) at end of file; updates the section header's `sh_offset`
   and `sh_size`
3. Appends a new `.rel.text` (original + 2 new relocations) at end of file; updates
   the section header
4. Splices a small handler body into a single-caller no-op function
   (`COmapNKS4Driver_Cleanup`) inside `.text`:
   ```
   mov  [sDeviceInstance], %eax    ; load the cached struct usb_device*
   test %eax, %eax                 ; NULL check
   je   .end
   call usb_reset_device           ; (R_386_PC32 vs the new SHN_UNDEF symbol)
.end: ret
   ```

The result is a `.ko` that `insmod` accepts and that calls `usb_reset_device(udev)`
during module unload — kernel API the original `.ko` did not import.

The technique generalises to any case where you want to call a kernel symbol that
the stock `.ko` doesn't already import. The non-obvious parts are:

- The order of operations matters: extending `.strtab` first, then `.symtab` (because
  `.symtab` entries point into `.strtab` by offset), then `.rel.text` (which points
  into `.symtab` by symbol index).
- For each section you grow, **move the section to end-of-file rather than trying
  to expand it in-place**; the original bytes become dead but the kernel module
  loader doesn't care, it consults the section header table.
- Section-relative addend convention: `st_value=0` for an `SHN_UNDEF` symbol; the
  resolved address is written by the kernel module loader at insmod time.
- The relocation's symbol index is `(len(orig_symtab) / sizeof(Elf32_Sym))` since
  we're appending at the end.

### Why isn't this deployed?

The patch was an investigation into recovering the chip wedge described in
[OmapNKS4Module chip wedge](../docs/modules/OmapNKS4Module.ko_chip_wedge.md).
It works mechanically and the kernel does call `usb_reset_device(udev)` on unload —
but the panel chip's wedge is a firmware-state issue that a USB-level reset doesn't
clear. Only a power-off-and-wait-for-capacitors-to-drain resolves it. So the patch
is preserved here as a reusable demo of ELF-surgery technique rather than a deployed
fix.

### Usage

```sh
python3 tools/patch_omapnks4_cleanup.py
# Reads:  kronos-fs/sbin/OmapNKS4Module.ko (stock)
# Writes: patched-OmapNKS4Module.ko
```

(Paths are hardcoded near the top of the script — edit if your source layout differs.)
