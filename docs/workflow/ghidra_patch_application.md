# Applying byte patches inside the Ghidra project

The Ghidra MCP server is configured with script execution **disabled** (sensible default
for an editor that gives shell access to the JVM). That means tools cannot directly write
patched bytes into the program. They can still set bookmarks and comments — which is how
this project records *where* the patches go — but actually applying the bytes is a manual
step you do once per program.

You only need to do this if you are working *inside the Ghidra project* (e.g. exporting a
new build of a patched binary via `export_patched.py`). The on-device deployment script
[`patched/kronos_patcher.sh`](../../patcher/kronos_patcher.sh) does not rely on Ghidra
state — it works directly against the binary files.

---

## Finding the patch sites in Ghidra

All patch sites are bookmarked with category `deploy_patch`:

- Open the program (`loadmod.ko`, `loadoa`, `OA.ko`)
- `Window → Bookmarks` (Ctrl-D), filter by category `deploy_patch`
- Each bookmark has a comment describing the exact byte change

Disassembly EOL comments at the same addresses also describe the patch so they're visible
in the listing view.

---

## Applying patches

### Method A — use the bundled GhidraScript (recommended)

GhidraScripts live in `~/ghidra_scripts/` under category **Kronos**:

| Script                        | Target        | Patches                                                  |
|-------------------------------|---------------|----------------------------------------------------------|
| `ApplyKronosLoadmodPatch.java`| `loadmod.ko`  | 3-site bypass (MD5 self-check + 2 inner verifications)   |
| `ApplyKronosOaPatch.java`     | `OA.ko`       | 11-run canonical bypass (magic-value skip + 6 IsUsingAny* specializations) |

To run one:

1. Open the target program (e.g. `OA.ko`).
2. `Window → Script Manager` (or the toolbar button).
3. Find the relevant `ApplyKronos*.java` under category **Kronos**.
4. Double-click to run.
5. The script verifies the original bytes match before writing; it skips any patch
   already applied; it refuses to touch anything that doesn't match either original
   *or* patched bytes (and warns).
6. `File → Save` to persist.

The OA.ko script addresses patches by ELF **section name + offset** (not Ghidra
linear address), so it works unchanged on any firmware version where these
specific functions haven't been reorganized. Verified on 3.2.1 and 3.2.2.

### Method B — Ghidra GUI

For each patch site listed in the bookmark:

1. Navigate to the bookmark address.
2. `Right-click → Patch Instruction…` (or `Ctrl-Shift-G`).
3. Type the new bytes (Ghidra accepts plain hex without spaces) or the new mnemonic.
4. Confirm.
5. Save.

This is what was originally done by the analyst for the OA.ko patches and the loadoa
string redirects. The bytes are present in the saved program and stay there.

---

## Verifying

After saving, run [`export_patched.py`](../../patcher/) (it walks every section, masks
relocs, and diffs Ghidra memory vs the original file) to get a new `.ko` / executable
file. The expected resulting MD5s are:

| File | Stock MD5 | Patched MD5 |
|---|---|---|
| `loadmod.ko` | `d1697c9b1c478c0dcdfaef71516fe5f2` | `28d1cec16f1d893f1d78241b62a989d9` |
| `loadoa`     | `8a3d61f3332d7bcf694e8c05845b4754` | `d17c26036fa0f51f5f4c0ef2f6f424bf` |
| `OA.ko`      | `955636c2b11a70a1dbecefaaa7bd4f80` | `163550b60b7508b2c0ba1fd314b0b944` |

---

## Current bookmark state (verified 2026-05-26)

| Program | Address | Status |
|---|---|---|
| `loadmod.ko` | `0x56fd` (8 B) | already patched in Ghidra |
| `loadmod.ko` | `0x5781` (2 B) | **stock — needs applying** |
| `loadmod.ko` | `0x3f80` (6 B) | **stock — needs applying** |
| `loadoa`     | `0x0804b696` (16 B) | already patched (your earlier work) |
| `loadoa`     | `0x0804b9c0` (32 B) | already patched (your earlier work) |
| `OA.ko` (3.2.1) | 11 patch runs / 369 B | already patched (canonical MD5 `163550b6…`) |
| `OA.ko` (3.2.2) | 11 patch runs / 369 B | apply via `ApplyKronosOaPatch.java` after import |

---

## Enabling automated patching for future runs

If you want MCP-driven patching to "just work" without using the Script Manager, restart
the Ghidra MCP plugin with:

```
GHIDRA_MCP_ALLOW_SCRIPTS=1 ghidraRun
```

(Optionally also set `GHIDRA_MCP_AUTH_TOKEN` if exposing the MCP bridge beyond loopback.)
With scripts enabled, the same `run_script_inline` tool calls that previously failed will
succeed.
