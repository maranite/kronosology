# OA.ko — Main Synthesis Engine Kernel Module

The heart of the Kronos. Everything that makes a sound, every front-panel control,
every patch/combi/program management routine, every MIDI route, every PCM bank, every
effect, the entire HDR recorder, **all** lives in this single ELF relocatable kernel module.

| Property | Value |
|---|---|
| Path on device | `/korg/Mod/OA.ko` (the `/korg/Mod/` mount is itself encrypted, see [`../crypto/atmel_nv2ac.md`](../crypto/atmel_nv2ac.md)) |
| Source path | `dump from kronos/korg/Mod/OA.ko` |
| Architecture | x86 LE 32-bit, Linux 2.6.32 kernel module (ET_REL) |
| Size | 14 285 504 bytes |
| Functions | 22 195 (Ghidra after analysis) / 17 685 nm `T/t/W/w` defined symbols |
| C++ symbols | 21 693 mangled `_Z…` |
| Distinct classes | 873 |
| Compiler | GCC 4.5.0 |
| MD5 (original) | `955636c2b11a70a1dbecefaaa7bd4f80` |
| MD5 (degrade-stripped variant) | `835674443b7d646bec66dcb171b0d740` (`oa.patched.ko`) |

See also: [`../modules/OA.ko_analysis.md`](../modules/OA.ko_analysis.md) for the initialisation sequence,
class roles, and runtime layout; [`../modules/OA.ko_auth.md`](../modules/OA.ko_auth.md) for the magic-value
chain; [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md) for the
EX authorisation algorithm; [`../interfaces/proc_oacmd.md`](../interfaces/proc_oacmd.md)
for the userspace command interface.

---

## Address mapping (critical reference)

OA.ko is an ELF relocatable — every section header has `sh_addr = 0`. Ghidra loads each
section at its declared base (also 0 for `.text`) and stacks the per-function COMDAT
sections (`.text.ClassName::Method`) at higher addresses. The result:

```
Ghidra_address  =  nm_symbol_value                         (for functions/data in .text)
file_offset     =  0xb390 + nm_symbol_value                (main .text starts at 0xb390)
runtime_address =  0x59CE6000 + nm_symbol_value            (live kallsyms base)
```

**Caveat:** the formula `Ghidra = nm` is exact only for the main `.text` and `.rodata`
sections. Some labels (e.g. `abnkodkis0` which `nm` reports at `0x000b0920`) are at
different Ghidra addresses because the inline-function COMDAT sections shift them. Read
them via Ghidra's `read_memory` against the address Ghidra reports, not nm's value.

---

## What lives in OA.ko

| Subsystem | Representative classes / functions |
|---|---|
| Top-level engine | `CSTGEngine::Initialize` (`0x000001b0`), `CSTGEngine::sInstance` (`0x00000008`) |
| Synthesis voice models | `CSTGPCMModel`, `CSTGMS20Model`, `CSTGOrganModel`, `CSTGPianoModel`, `CSTGPolysixModel`, `CSTGPluckedModel`, `CSTGEPModel`, `CSTGAnalogSyncModel`, `CSTGOffModel`, `CSTGSimpleTG92Osc` |
| Filters | `CSTGAnalog4PoleBase`, `CSTGMultiModeFilter`, `CSTGMultiFilter2Pole` |
| Envelopes / LFO | `CSTGADSRBase`, `CSTGEGBase`, `CSTGKLEG`, `CSTGCommonLFO`, `CSTGPolysixMG`, `CSTGLFOTables` |
| Effects | `CSTGEffectManager`, `CSTGEffectAlgorithm`, `CSTGEffectSlot`, `CIFXEffectSlot`, plus ~200 individual effect classes (`CSTGNewStereoCompressor2`, `CSTGRotarySpeaker`, `CSTGOverb2`, `CSTGStereoBPMDelay`, every `CSTGParallel*`, every `CSTGCompressor*`, every reverb) |
| KARMA | `CKarmaPerf1Module`, `CKarmaPerfCommon`, `CKarmaPerfModule`, `CKarmaGlobal` |
| Voice / note management | `CSTGSlotState`, `CSTGSlotVoiceData`, `CSTGVoice`, `CSTGVoiceAllocator`, `CEmergencyStealer`, `CDeferredNoteOnInfo` |
| Sequencer (CSPR\*) | `CSPREngine`, `CSPRPlayer`, `CSPRRecorder`, all `CSPRRec*EventManager`, `CSPRClockHandler`, `CSPRMetronome`, ~120 sequencer classes |
| KARMA Generation (CKG\*) | `CKGEngine`, `CKGRTCHandler`, `CKGMIDIMsgProcessor`, all the switch/knob/pad handlers |
| MIDI / IO | `CSTGMidiInPort`, `CSTGMidiOutPort`, `CSTGMidiDispatcher`, `CSTGFrontPanel` |
| PCM / multisample / bank | `CSTGMultisampleBank`, `CSTGMultisampleBankManager`, `CSTGKLMManager`, `CSTGSampler`, `CSTGSamplingDaemon`, `CSTGSamplingInterface`, `CSTGPCMBlock`, `CSTGPCMPrecacheManager` |
| Patches / Combis / Programs | `CSTGProgram`, `CSTGProgramSlot`, `CSTGProgramMode*`, `CSTGCombi`, `CSTGPatch`, `CSTGGlobal` (very large — 440 fields) |
| HDR (hard-disk recording) | `CSTGHDRManager`, `CSTGHDRCircularBuffer`, `CSTGHDRFileReader/Writer`, `CSTGHDRMiniModel`, `CSTGHDRTrackMsgHandler` |
| Audio driver / clock | `CSTGAudioManager`, `CSTGAudioDriverInterface`, `CSTGAudioDriverInterfaceKorgUsb`, `CSTGMIDIClockSync` |
| Authentication / licensing | `CSTGKLMManager`, `CSTGInstalledEXProducts`, `CSTGEXProductInfo`, `VerifyAuthorizationString`, `VerifyAndSaveAuthString`, `SetupAtmelForAuthorizations` |
| Boot integrity hooks | `InitCdromSupport` (text +0x40), `cleanup_cpp_support` (text +0x0) |

---

## Initialisation chain

`CSTGEngine::Initialize` orchestrates everything. The fully-typed signature after our
Phase 1 work:

```
void __regparm3 CSTGEngine::Initialize(CSTGEngine *this)
```

Key calls in order (see [`../modules/OA.ko_analysis.md`](../modules/OA.ko_analysis.md) for the full chain):

1. `InitCdromSupport()` — verifies loadmod magic chain → returns 0 (good) or -1 (bad)
2. If bad **and** the JE at `+0x8B6` is NOT patched: audio-degradation cascade
   (six sites NOP-able, see [Patches](#patches))
3. `CSTGGlobal::sInstance` construction → loads `/korg/rw/Startup/AuthorizationStrings`
   via `CSTGInstalledEXProducts` → `ParseAuths` → per-line `VerifyAuthorizationString`
4. `CSTGKLMManager::Initialize` → marks each `CSTGEXProductInfo` as authorised based
   on (a) the option file existing in `/korg/rw/Options/Sxxx` and (b) a valid line in
   AuthorizationStrings whose decrypted plaintext option-ID matches
5. Per-bank `IsAuthorizedMultisampleBank` checks during `LoadMultisample` calls

---

## Userspace interface

OA.ko creates **`/proc/.oacmd`** (a single procfs file) via `create_proc_entry`. All
commands from `Eva`, `InstallEXs`, or anyone else flow through this file with
`copy_from_user`. See [`../interfaces/proc_oacmd.md`](../interfaces/proc_oacmd.md) for
the full command table.

---

## Patches <a name="patches"></a>

See [`../workflow/patch_guide.md`](../workflow/patch_guide.md) for deployment steps. The patch points:

### Patch 1 — Magic-value degradation bypass

| Variant | Location | Original | Patched | Effect |
|---|---|---|---|---|
| Minimal (1 byte) | file `0xBC46` (`CSTGEngine::Initialize` +`0x8B6`) | `74 5A` | `EB 5A` | JE → JMP, skip degradation entirely |
| Thorough (50 runs, 307 bytes) | six call sites in init code | various `C7 05 …` MOV-imm stores of degraded float values | `90 …` NOP fill | Removes every redundant degrade-store |

The current `oa.patched.ko` (MD5 `835674…`) contains the thorough
variant. See [`../workflow/export_patched_ko.md`](../workflow/export_patched_ko.md) for
how it was reconstructed from a Ghidra in-memory state.

### Patch 2 — Bank authorisation bypass (the kronos_rooting approach)

| Location | Original | Patched | Effect |
|---|---|---|---|
| `IsAuthorizedMultisampleBank` @ Ghidra `0x02E650`, file `0x399E0` | `F6 42 5C 08 B9 01` | `B8 01 00 00 00 C3` | `MOV EAX,1 ; RET` — every bank is "authorised" |

**Either Patch 1 *or* the auth-string-generator approach (`crypto/auth_string_algorithm.md`)
can stand alone**; you don't need both. Patch 2 is only needed if you skip the auth chain
entirely. The combination of Patch 1 + the generator approach gives the cleanest result:
stock auth chain works, audio plays at full quality.

### Patch 3 — EX-bank authorisation bypass (current default)

The variant deployed by [`../workflow/deploying_patches.md`](../workflow/deploying_patches.md).
Six functions named `IsUsingAnyUnauthorizedMultisamples` (the per-engine helpers plus the
four COMDAT vtable overrides for `CSTGPCMModelPatch`, `CSTGPluckedModelPatch`,
`CSTGVPMModelPatch`, `CSTGPianoModelPatch`) all patched to `xor eax,eax; ret`. Surrounding
auth-failure degradation MOV-imms NOP'd out.

- **56 byte runs**, 369 bytes total
- Patched MD5: `163550b60b7508b2c0ba1fd314b0b944`
- Deployed at `/sbin/OA.ko` (note: `/sbin/` not `/korg/Mod/`), loaded by patched `loadoa`
- Not detected by `loadmod`'s MD5 check (`/sbin/OA.ko` is not in the file list)

### Hot-swap gotcha — `/proc/.shm` leak

Stock `OA.ko`'s `cleanup_module` calls `remove_proc_entry(".shm", NULL)` correctly, but
Eva holds an fd on `/proc/.shm`, so the proc entry's refcount stays > 0 after `rmmod`.
The kernel keeps `proc_dir_entry` around but frees the module text — including the
`fops` table the entry points to. Next process exit that closes its fds (often an SSH
`sh` exiting) calls `fops->release` on freed memory ⇒ kernel oops with `Fixing recursive
fault but reboot is needed!`. **Do not `rmmod OA` while Eva is running.** See
[`../modules/OA.ko_hot_swap_bug.md`](../modules/OA.ko_hot_swap_bug.md).

---

## Crypto primitives present

OA.ko contains its own copies of (so that loadable-module independence is preserved):

| Primitive | Symbol | Notes |
|---|---|---|
| MD5 | `md5_init`/`md5_append`/`md5_finish` @ `0x4f57d0`/`0x4f5800`/`0x4f5900` | Used by `ParseAuth` for option-file fingerprinting |
| Blowfish ECB | `BlowfishEncryptBlock` @ `0x4f5b30` | Standard, P/S tables at `0x678d60` |
| Blowfish CFB-8 | `moancjsd82` @ `0x4f5f00` | Used for auth-string ciphertext |
| Custom base32 | `DecodeBytesFromAscii` @ `0x4f39c0` | Alphabet `0123456789ACDEFGHJKLMNPQRTUVWXYZ`, remap `B/O/I/S → 8/0/1/5` |
| NV2AC chip I/O | `nv2ac_read_data` @ `0x4f4840` | Wraps `stgNV2AC_sync_read_cmd` exported from `OmapNKS4Module.ko` |
| Atmel auth crypto | `atmel_auth_compute_c1` @ `0x4f61c0`, `atmel_auth_set_params` @ `0x4f61a0` | Per-device challenge-response |

---

## Auth bookmarks

The Ghidra project has all of these (and ~120 more) bookmarked with category
`patch_for_auth`:

| Bookmark | Address | Role |
|---|---|---|
| `CSTGEngine::Initialize` (JE) | `0x0008B6` | Patch 1 location |
| `IsAuthorizedMultisampleBank` | `0x02E650` | Patch 2 location |
| `VerifyAuthorizationString` | `0x207DE0` | Per-line auth verifier |
| `VerifyAndSaveAuthString` | `0x048290` | Verify-and-append handler called by `AU:` command |
| `ParseAuth` / `ParseAuths` | `0x207890` / `0x207C50` | Inner parser; reads chip secret, calls `moancjsd82` |
| `SetupAtmelForAuthorizations` | `0x207A50` | Chip init for auth reads |
| `IsAuthorizedEffect`, `IsAuthorizedVoiceModel`, `IsOptionAuthorized` | `0x02E740` / `0x02E600` / `0x048130` | Per-resource auth checks |
| `IsUsingAnyUnauthorizedMultisamples` (×7) | various | Reverse-direction check |
| `atmel_auth_compute_c1`, `atmel_auth_set_params` | `0x4F61C0` / `0x4F61A0` | NV2AC challenge-response primitives |

---

## How the analysis was performed

See [`../workflow/analysis_campaign.md`](../workflow/analysis_campaign.md) for the full
phased pipeline (prototypes → struct layouts → return types → globals) and the timings.
Net result: decompilation went from raw `*(byte *)(in_EDX + 0x5c)` arithmetic to typed
`CSTGMultisampleBank * pBank` parameters with `pBank->field_0x5c` struct access — a
foundation suitable for translation toward compilable C/C++ source.
