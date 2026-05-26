# Korg Kronos — Reverse Engineering Knowledge Base

A comprehensive reference for the studied internals of the Korg Kronos OS,
binaries, kernel modules, security model, and file formats.

This index lives at `docs/`. Top-level docs in ``
remain authoritative for their topics and are linked from here.

---

## Project Goals

1. Understand the boot and integrity-check chain end-to-end so that any Kronos can be made
   to boot reliably (e.g. failed Atmel chip, missing license entries).
2. Be able to install and authorise any EX expansion legitimately, on any device, without
   patching `OA.ko` (so future Korg OS updates keep working).
3. Move toward compilable C/C++ source for a future port to 64-bit hardware with >4 GB RAM.

---

## Map of the Documentation

### Per-binary deep-dives — `modules/`

| File | Role | Doc |
|---|---|---|
| `OA.ko` | Main synthesis engine (22 195 functions) | [modules/OA.ko.md](modules/OA.ko.md) |
| `Eva` | GUI / front-panel application (37 828 functions) | [modules/Eva.md](modules/Eva.md) |
| `loadoa` | Userspace OS loader — verifies and loads `OA.ko` | [modules/loadoa.md](modules/loadoa.md) |
| `loadmod.ko` | Boot-integrity kernel module — hooks CD-ROM, syscalls | [modules/loadmod.ko.md](modules/loadmod.ko.md) |
| `GetPubIdMod.ko` | Atmel NV2AC chip interface — derives the Public ID | [modules/GetPubIdMod.ko.md](modules/GetPubIdMod.ko.md) |
| `OmapNKS4Module.ko` | OMAP NKS4 driver — exports the low-level `stgNV2AC_sync_*` primitives | [modules/OmapNKS4Module.ko.md](modules/OmapNKS4Module.ko.md) |
| `UpdateOS` | OS update installer — signature-checks scripts | [modules/UpdateOS.md](modules/UpdateOS.md) |
| `InstallEXs` | EX expansion installer — uses the same `UpdaterScriptsKey` | [modules/InstallEXs.md](modules/InstallEXs.md) |
| `STGEnabler.ko` | Tiny helper module | [modules/STGEnabler.ko.md](modules/STGEnabler.ko.md) |
| `STGGmp.ko` | GMP (multi-precision arithmetic) wrapper | [modules/STGGmp.ko.md](modules/STGGmp.ko.md) |
| `DisplayUpdaterMessage` | Front-panel status display during updates | [modules/DisplayUpdaterMessage.md](modules/DisplayUpdaterMessage.md) |

### Cryptography & security — `crypto/`

| Topic | Doc |
|---|---|
| EX authorisation-string algorithm (base32 + Blowfish-CFB-8 + MD5 + chip secret) | [crypto/auth_string_algorithm.md](crypto/auth_string_algorithm.md) |
| OS-update script signature (SHA-1 + 16-byte `UpdaterScriptsKey`) | [crypto/update_signature.md](crypto/update_signature.md) |
| Atmel NV2AC chip protocol & GPA stream cipher | [crypto/atmel_nv2ac.md](crypto/atmel_nv2ac.md) |

### Userspace ↔ kernel interfaces — `interfaces/`

| Topic | Doc |
|---|---|
| `/proc/.oacmd` command protocol (the `AU:`/`LM:`/`CL:` etc. dispatcher) | [interfaces/proc_oacmd.md](interfaces/proc_oacmd.md) |
| File formats — `AuthorizationStrings`, `EXsInstall.exsins`, option files, `install.info` | [interfaces/file_formats.md](interfaces/file_formats.md) |

### Committed-preset memory — `preload/`

The on-disk format of `/korg/rw/PRELOAD/` — programs, combis, drum kits, wave sequences,
set lists, globals, KARMA data, effect presets, drum-track patterns.

| Topic | Doc |
|---|---|
| Inventory + index | [preload/README.md](preload/README.md) |
| The universal 20-byte `P***` container format | [preload/container_format.md](preload/container_format.md) |
| Program banks (`PROG*.BIN`) | [preload/program_banks.md](preload/program_banks.md) |
| Combi banks (`COMB*.BIN`) | [preload/combi_banks.md](preload/combi_banks.md) |
| Drum-kit banks (`DKIT*.BIN`) | [preload/drum_kit_banks.md](preload/drum_kit_banks.md) |
| Wave-sequence banks (`WSEQ*.BIN`) | [preload/wave_sequence_banks.md](preload/wave_sequence_banks.md) |
| Set lists (`STLS.BIN`, `STMP*.BIN`) | [preload/set_list.md](preload/set_list.md) |
| Global settings (`GLBL.BIN`) | [preload/global_settings.md](preload/global_settings.md) |
| Drum-track patterns (`PPAT.BIN`, `DPAT.BIN`) | [preload/drum_track_patterns.md](preload/drum_track_patterns.md) |
| KARMA data (`GE.BIN`, `GE.KDF`, `KGEUA.BIN`) | [preload/karma_data.md](preload/karma_data.md) |
| Effect presets (`FXPR.BIN`) | [preload/effect_presets.md](preload/effect_presets.md) |
| Piano types (`PianoTypes/PianoType<NN>`) | [preload/piano_types.md](preload/piano_types.md) |
| Misc small files | [preload/misc_files.md](preload/misc_files.md) |
| **Extension points — adding more banks** | [preload/extension_points.md](preload/extension_points.md) |

### Workflow & tooling — `workflow/`

| Topic | Doc |
|---|---|
| Ghidra MCP setup, conventions, address mapping | [workflow/ghidra_setup.md](workflow/ghidra_setup.md) |
| How to export a patched `.ko` from Ghidra (reloc-aware diff) | [workflow/export_patched_ko.md](workflow/export_patched_ko.md) |
| **Deploying patched binaries (full recipe + script)** | [workflow/deploying_patches.md](workflow/deploying_patches.md) |
| Applying byte patches inside the Ghidra project | [workflow/ghidra_patch_application.md](workflow/ghidra_patch_application.md) |
| Phased deep-analysis campaign (prototypes → structs → returns → globals) | [workflow/analysis_campaign.md](workflow/analysis_campaign.md) |
| CPU affinity, thread model & multi-core scaling (4-core ready; 8+ requires patches) | [workflow/cpu_affinity_and_scaling.md](workflow/cpu_affinity_and_scaling.md) |

### Existing top-level documents (still authoritative)

| File | Topic |
|---|---|
| [`../kronos_system.md`](../kronos_system.md) | System-wide architecture (CPU, FS layout, boot sequence) |
| [`../OA_analysis.md`](../OA_analysis.md) | OA.ko initialisation sequence, classes, runtime layout |
| [`../OA_auth.md`](../OA_auth.md) | Magic-value chain, loadmod ↔ OA integration |
| [`../loadmod_analysis.md`](../loadmod_analysis.md) | Detailed loadmod.ko function inventory |
| [`../loadmod.MD`](../loadmod.MD) | loadmod ground truth from kronoshacker.blogspot.com |
| [`../patch_guide.md`](../patch_guide.md) | Step-by-step deploy procedure (SSH method A) |
| [`../update_builder.md`](../update_builder.md) | UpdateOS internals + `update_builder.py` guide |

---

## Quick-reference: where the secrets live

| Secret / artifact | Location | Notes |
|---|---|---|
| `UpdaterScriptsKey` (16 bytes) | Hard-coded in `UpdateOS` @ file offset `0x0f2ac8`; in `InstallEXs` @ file offset `0xa610`; in `UpdaterScriptsKey.h` source | Used to sign update scripts and EX install option files |
| Magic value `0x22FB39CC` | Written by `loadmod.ko` after boot integrity checks pass; read by `OA.ko::InitCdromSupport` and `CSTGEngine::Initialize` | Drives audio-degradation logic if missing |
| Per-device 24-byte chip secret | Atmel NV2AC at addresses `0x10, 0x18, 0x20` | Key for the EX authorisation algorithm; readable only via `stgNV2AC_sync_read_cmd` (kernel) |
| `AuthorizationStrings` | `/korg/rw/Startup/AuthorizationStrings` | 24-char lines; one per authorised EX; written by `VerifyAndSaveAuthString` |
| Option files | `/korg/rw/Options/Sxxx` | Plain-text EX metadata; MD5-fingerprinted into the auth string |
| `pairFact` keys | Generated by `loadmod.ko` from public ID + magic | Decrypt `/korg/Eva/` and `/korg/Mod/` loop devices |

---

## Quick-reference: what each patch unlocks

| Patch | What it does | Trade-off |
|---|---|---|
| `IsAuthorizedMultisampleBank` → `MOV EAX,1; RET` (6 bytes @ `0x399E0`) | Bypass per-bank auth — every PCM bank is authorised | Stays effective across OS updates only if the same function offset survives |
| Magic-value degradation `JE → JMP` (1 byte @ `0xBC46`) | Stops audio fade when `loadmod.ko` isn't writing the magic | Minimal — does not affect any signing or key state |
| Six-site degradation NOP-out (`oa.patched.ko` MD5 `835674…`) | Same as above but removes all six redundant degrade-store sites | More thorough; same effect |
| **EX-bank auth bypass — patched OA.ko (MD5 `163550b6…`) loaded via patched `loadoa` + 3-bypass `loadmod`** | Six `IsUsingAnyUnauthorizedMultisamples` paths skipped; any EX expansion installs without keys | Requires the matched loadmod/loadoa patches; see [workflow/deploying_patches.md](workflow/deploying_patches.md) |
| Auth-string-generator (planned) | Patched `InstallEXs` + helper `.ko` writes legitimate auth strings → stock `OA.ko` accepts | Cleanest long-term: no `OA.ko` patch needed |

---

## Reproducibility checklist

- All Ghidra work is in the `kronos.rep` project at ``
- `OA.ko` is **versioned** — must be checked out before saves (Project window → right-click → Check Out)
- The `.claude/memory/` directory holds session-persistent notes that complement these docs
- `kallsyms.txt` (live `/proc/kallsyms` from a running Kronos) gives runtime base addresses

---

## Status

| Module | Function prototypes | Struct layouts | Return types | Notes |
|---|---|---|---|---|
| `OA.ko` | 15 958 applied | 583 built (13 497 fields) | 8 160 refined | Full Phase 1-3 |
| `Eva` | 34 360 applied | not built (would cascade ~25 h) | 6 465 refined | Largest single binary |
| `UpdateOS` | 50 applied | 1 built | 13 refined | Small, complete |
| `OmapNKS4Module.ko` | 56 applied | (0 found in this small module) | 20 refined | Complete |
| `InstallEXs` | 26 applied | (0 found — tiny class count) | 3 refined | Complete |
| `GetPubIdMod.ko`, `loadmod.ko`, `loadoa`, `STGEnabler.ko`, `STGGmp.ko` | n/a (no C++ mangled symbols) | n/a | n/a | C-only; ceiling = Ghidra auto-analysis |
