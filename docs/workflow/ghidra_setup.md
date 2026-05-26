# Ghidra Project Setup & Conventions

How the Ghidra side of this RE project is structured, so the work is reproducible.

---

## Project location

- **Project folder**: ``
- **Project name**: `kronos` (file is `kronos.gpr`, repo at `kronos.rep/`)
- **All binaries imported as separate programs** in the project:

| Program | Source | Versioned in Ghidra? |
|---|---|---|
| `OA.ko` | `dump from kronos/korg/Mod/OA.ko` | **Yes** — check out before saves |
| `Eva` | `dump from kronos/korg/Eva/Eva` | No |
| `UpdateOS` | `dump from kronos/sbin/UpdateOS` | No |
| `loadmod.ko` | `dump from kronos/sbin/loadmod.ko` | **Yes** |
| `loadoa` | `dump from kronos/sbin/loadoa` | **Yes** |
| `OmapNKS4Module.ko` | `dump from kronos/sbin/OmapNKS4Module.ko` | No |
| `STGEnabler.ko` | `dump from kronos/sbin/STGEnabler.ko` | No |
| `STGGmp.ko` | `dump from kronos/sbin/STGGmp.ko` | No |
| `InstallEXs` | `dump from kronos/sbin/InstallEXs` | No |
| `GetPubIdMod.ko` | `dump from kronos/sbin/GetPubIdMod.ko` | No |

For versioned programs: **Project window → right-click → Check Out** before any save
operation will succeed.

---

## Ghidra version & launch

- **Ghidra**: installed under `/home/maranite/ghidra/`
- **JDK**: **Eclipse Temurin 21 LTS** (`/usr/lib/jvm/temurin-21-jdk-amd64`), set in
  `support/launch.properties` via `JAVA_HOME_OVERRIDE`. We use Temurin specifically
  because earlier crashes/freezes traced back to an unstable `openjdk-21-jdk` EA build.
- **Heap**: `-Xmx12g` (set in `launch.properties`)
- **Display**: `GDK_BACKEND=x11` (Wayland was causing frozen-window issues)
- **HiDPI**: `Dsun.java2d.uiScale=2`

Launch with `~/ghidra/ghidraRun` (we also tested `nohup …/ghidraRun > /tmp/ghidra_run.log 2>&1 &`
for headless background launch).

---

## GhidraMCP bridge

The project uses a **GhidraMCP** plugin that runs inside Ghidra and exposes ~175 REST
endpoints over a Unix Domain Socket. This is the channel that automates everything.

- **Socket path**: `/run/user/1001/ghidra-mcp/ghidra-<PID>.sock` (recreated each Ghidra
  launch, with the new Java PID)
- **HTTP-over-UDS** — the bridge speaks HTTP/1.0 framing on the socket
- **Helper module** at `/tmp/ghidra_helper.py` provides a `g(path, params=..., method=..., body=...)`
  function — the SOCK constant must be updated to the current PID's socket whenever
  Ghidra restarts
- **Scripting endpoints (`run_ghidra_script`, `run_script_inline`) are gated** by the
  environment variable `GHIDRA_MCP_ALLOW_SCRIPTS=1`. In this project we have **not** set
  that variable, so all automation goes via the higher-level REST endpoints (not raw
  Java scripts)

Useful endpoint categories (use `g('/mcp/schema')` to enumerate all 175):

| Category | Highlights |
|---|---|
| `function/` | `set_function_prototype`, `decompile_function`, `batch_decompile`, `get_function_by_address`, `rename_function`, `disassemble_function` |
| `datatype/` | `create_struct`, `add_struct_field`, `remove_struct_field`, `apply_data_type`, `validate_data_type_exists`, `get_struct_layout`, `analyze_struct_field_usage` |
| `symbol/` | `create_label`, `rename_label`, `rename_global_variable` |
| `program/` | `open_program`, `save_program`, `save_all_programs`, `analysis_status`, `run_analysis`, `reanalyze` |
| `listing/` | `list_functions_enhanced`, `list_data_items`, `list_classes`, `list_namespaces` |
| `comment/` | `set_plate_comment`, `set_decompiler_comment` |
| `analysis/` | `find_dead_code`, `find_code_gaps`, `analyze_function_complete` |

---

## Address mapping cheatsheet

Critical because each binary type has its own convention:

| Binary class | Formula |
|---|---|
| Kernel module (`.ko`, ET_REL) | `Ghidra_addr = nm_symbol_value` for sections at `sh_addr=0`. **Caveat**: COMDAT sections (`.text.ClassName::Method`) place at higher addresses than nm reports; use Ghidra's `read_memory` against Ghidra's address, not nm's |
| Userspace executable (`Eva`, `UpdateOS`, `InstallEXs`, `loadoa`) | `Ghidra_addr = full VMA` (image base + offset) — typically `0x08048000`+ |
| File offset (in the `.ko` ELF on disk) | `file_offset = nm_value + 0xb390` for OA.ko's main `.text` |
| Runtime address on a live Kronos | `runtime_addr = 0x59CE6000 + nm_value` for OA.ko (from `/proc/kallsyms`) |

---

## Naming conventions

- **Functions**: `PascalCase` (Korg's convention — e.g. `IsAuthorizedMultisampleBank`)
- **Globals**: `g_` + Hungarian prefix (`g_iCount`, `g_pBank`) — use `set_global` endpoint
  for proper Hungarian-style application
- **Class statics**: `Class::sInstance`, `Class::sMessageHandlers`, `Class::sValueGetters`
  (Korg pattern — `s` for static)
- **Field names**: from accessor analysis (e.g. method `GetCCNumber` → field
  `m_CCNumber` at the offset it accesses)
- **Plate comments**: brief class purpose for constructors; the C++ demangled signature
  is automatically populated by Ghidra's demangler so don't overwrite it without copying
  it back

---

## Bookmarks

Used to mark structurally interesting locations:

| Category | Use |
|---|---|
| `patch_for_auth` | Every function or branch involved in authorisation (~127 in OA.ko) — see [`../modules/OA.ko.md`](../modules/OA.ko.md#auth-bookmarks) |

---

## Stability notes (from experience)

- Avoid 100s of rapid `set_function_prototype` calls without a 1–2s pause between
  save batches — Ghidra throws `Unable to lock due to active transaction` until the
  pending write transaction settles
- For bulk operations on >1000 things, expect `add_struct_field`/`remove_struct_field`
  to slow down as cross-reference count grows (the famous "Phase 2 took 6 hours" cascade);
  do them as one background pass with regular saves, and don't expect the API to feel
  fast at scale
- Ghidra has been known to crash mid-run; save every 1500–2000 operations to limit
  potential lost work
- `delete_data_type` on a referenced struct **breaks every function signature** that
  uses it — never delete-and-recreate when modifying placeholder types; use
  `add_struct_field` / `remove_struct_field` in place

---

## Files supporting the analysis (in `/tmp/`)

| File | Purpose |
|---|---|
| `oa_syms_demangled.txt` | All 23 447 `T/t/W/w` defined symbols, demangled |
| `oa_nm_all_dem.txt` | All 65 395 symbols (any type), demangled — for vtables and globals |
| `oa_prototypes.json` | 15 958 reconstructed function prototypes |
| `oa_class_inventory.json` | 824 classes with method lists |
| `oa_class_layouts.json` | 584 inferred struct layouts |
| `oa_field_names.json` | 457 accessor-derived field names |
| `oa_decompiled.jsonl` | All 22 041 functions decompiled (snapshot at end of Phase 1) |
| `auth_algo.py` | Verified Python reference for the EX auth-string algorithm |
| `export_patched_oa2.py` | Reloc-aware Ghidra → `.ko` reconstruction tool |
| `phase{1,2,3,5}_*.py` | The deep-analysis campaign scripts |

These are *working files* — not source-of-truth — but they're useful for re-running any
phase or as starting points for future work. See [`analysis_campaign.md`](analysis_campaign.md).
