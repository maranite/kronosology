# Exporting a Patched `.ko` from Ghidra

How to take byte-level edits you've made inside Ghidra to a kernel module and produce a
valid, deployable `.ko` file on disk — preserving the ELF structure (`.symtab`,
`.modinfo`, all relocation sections, etc.).

---

## The challenge

Ghidra **applies** the ELF relocations from `.rel.<section>` when it loads a relocatable
kernel module. So Ghidra's in-memory view differs from the on-disk bytes everywhere
there's a relocation target — typically `0xfffffffc` (the PC-relative `-4` addend) or
`0x00000000` (absolute) in the file becomes a real address in memory. A naive
byte-diff between Ghidra and the file shows hundreds of thousands of "differences"
that are all relocation noise, not real patches.

A second subtlety: `OA.ko` has thousands of **COMDAT sections** — every inline / virtual
method gets its own `.text.<mangled_symbol>` section, with its own file offset and its
own `.rel.text.<mangled_symbol>` relocation table. Patches in any of those would be
missed by a scanner that only looks at the main `.text`.

---

## The approach

Walk **every** PROGBITS executable section. For each section: mask out its relocation
targets, diff Ghidra's memory against the file bytes at the section's `sh_offset`,
collect surviving differences as user patches. Apply them to a copy of the original.

Steps:

1. **`readelf -S -W`** — enumerate every PROGBITS section with `X` (executable) flag.
   For each, capture `name`, `sh_offset`, `sh_size`.
2. **`readelf -r`** — enumerate every `.rel.<section>` relocation table. Build a per-
   section set of byte offsets (each relocation occupies 4 bytes on x86).
3. **`/list_segments`** from the Ghidra MCP bridge (paginated) — map each section name
   to its Ghidra-loaded address.
4. For each section, **read the section's bytes from Ghidra** in ≤ 64 KB chunks and
   **diff** them against the file bytes at the corresponding `sh_offset`, **excluding**
   any byte position in the relocation mask.
5. **Group surviving differences into runs** (consecutive bytes form one patch run).
6. **Apply** to a copy of the original `.ko` and MD5-verify.

This is exactly what **`/tmp/export_patched_oa3.py`** does. The v3 script covers all
sections including COMDATs; the previous v2 only scanned the main `.text`. A typical
run on `OA.ko` (5,374 PROGBITS executable sections, 1.1 million relocation bytes
masked) takes ~6 seconds end-to-end and lists every detected patch.

---

## Address mapping for OA.ko

For the main `.text`:

```
Ghidra .text starts at  Ghidra address  0x00000000
                        file offset     0xb390
size of main .text:                     0x592070 bytes
```

For COMDAT `.text.<mangled_symbol>` sections (every inline / virtual member function):

- Ghidra places them at sequential addresses starting at `0x00592080`
- Each has its own `sh_offset` in the file — get it from `readelf -S -W <file>`
- The mapping `Ghidra address → file offset` therefore varies per section

The script handles this automatically by intersecting the `readelf -S` section table
with Ghidra's `/list_segments` output.

---

## The current working script

`/tmp/export_patched_oa3.py` — self-contained, ~180 LOC. Usage:

```bash
python3 /tmp/export_patched_oa3.py
# default paths:
#   orig: kronos-fs/korg/Mod/OA.ko
#   dest: oa.patched.ko

# or override:
python3 /tmp/export_patched_oa3.py <orig.ko> <dest.ko>
```

Tunable constants at the top:

```python
ORIG       = '…/OA.ko'        # source file
DEST       = '…/oa.patched.ko'# output file
PROGRAM    = 'OA.ko'          # Ghidra program name
RELOC_SIZE = 4                # x86 R_386_32/PC32 — 4-byte mask per reloc
READ_CHUNK = 65536            # bytes per /read_memory call
```

Prerequisites:

- Ghidra running with `OA.ko` open
- `/tmp/ghidra_helper.py` pointing at the current Ghidra MCP UDS socket
- `readelf` and `c++filt` available on `$PATH`

---

## Output verification

The script prints every patch run grouped by file offset:

```
   345 byte(s) in section .text
     6 byte(s) in section .text._ZN17CSTGPCMModelPatch34IsUsingAnyUnauthorizedMul...
     6 byte(s) in section .text._ZN21CSTGPluckedModelPatch34IsUsingAnyUnauthorize...
     6 byte(s) in section .text._ZN17CSTGVPMModelPatch34IsUsingAnyUnauthorizedMul...
     6 byte(s) in section .text._ZN19CSTGPianoModelPatch34IsUsingAnyUnauthorizedM...

Patch runs: 56
  file 0x00bc44   6B  85c0745ac705 -> eb5c90909090
  …
  file 0x147b60  19B  8b40088b52280fbf40088b44023485c00f95c0 -> 31c0c390909090909090909090909090909090
  …
  file 0x5af740   6B  8b92d0010000 -> 31c0c3909090
  …
```

…and a final MD5 check:

```
Original MD5: 955636c2b11a70a1dbecefaaa7bd4f80
Patched  MD5: 163550b60b7508b2c0ba1fd314b0b944
Size: 14285504 bytes  (same as original)
```

What to confirm:

- **Sections scanned** = section count (no missing entries)
- **File size** matches the original
- **MD5** differs from the original (you did patch something)
- **Patch runs** matches what you remember editing in Ghidra (no extras, no missing)

---

## Caveats and gotchas

### "Why does the export show fewer bytes patched than I edited in Ghidra?"

You probably only modified the function **prologue** in Ghidra. Ghidra leaves the
*trailing* bytes of the function unchanged — they become unreachable dead code after
your new `RET`. The export faithfully reproduces only what Ghidra has, not a "scrub
the whole function" assumption. This is the right behaviour — the trailing bytes never
execute so there's no point overwriting them.

If you *want* the trailing bytes also overwritten (for a cleaner disassembly or to
remove fingerprintable original code), do that in Ghidra by selecting and patching
those bytes to `NOP` (`90`), then re-export.

### "Why are exec-section patches in the export, but my patches to a vtable / `.rodata` aren't?"

The current script only walks **PROGBITS** sections with the **`X` (executable)** flag.
Data sections like `.rodata`, `.data`, `.data.rel.ro` (where vtables live) are skipped.
To include them, change this line in the script:

```python
if 'X' not in flg:                    # we care about executable sections
    continue
```

to e.g. `if stype != 'PROGBITS': continue` to include all PROGBITS sections, regardless
of flag. (Note: vtable patches additionally need the `.rel.rodata.<...>` mask handling,
which the script already does generically.)

### "Why doesn't Ghidra's built-in `File → Export → Binary` just work?"

It writes only the loaded memory image, without the ELF wrapping (`.symtab`, `.modinfo`,
relocations, `.shstrtab`, etc.). For a kernel module you must keep the original ELF
intact for `insmod` to accept it — hence the surgical diff-and-apply approach above.
The MCP bridge does not expose a programmatic equivalent of `File → Export → Original
File` (which would do this correctly), so we work around it.

The MCP scripting endpoints (`run_ghidra_script`, `run_script_inline`) are gated by
`GHIDRA_MCP_ALLOW_SCRIPTS=1` (off by default in this project). If they were enabled, the
neatest implementation would be a small Java GhidraScript that calls `ProgramExporter`
directly.

---

## Historical scripts

| Script | What it does | Status |
|---|---|---|
| `/tmp/export_patched_oa.py`   | Naïve diff — no relocation masking — produces 700k spurious diffs | **deprecated** |
| `/tmp/export_patched_oa2.py`  | Reloc-masked, **main `.text` only** — misses every COMDAT patch | **superseded** |
| `/tmp/export_patched_oa3.py`  | Reloc-masked, walks **every PROGBITS executable section** including COMDATs | **current** |
