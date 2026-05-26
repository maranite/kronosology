# OA.ko — Architecture & Analysis

The main synthesis engine kernel module for the Korg Kronos.  
Architecture: x86 LE 32-bit, kernel 2.6.32.  
Runtime `.text` base: **`0x59CE6000`** (from `/proc/kallsyms`).  
Function count: **22,041** (fully auto-analyzed from ELF symbol table; no unnamed functions).

**Ghidra address mapping:** OA.ko is an ELF relocatable (ET_REL) with all section VMAs=0.
Ghidra loads each section at its VMA (=0 for all sections in a .ko), so:
`Ghidra_addr = nm_symbol_value` (the ELF section-relative offset directly).
`file_offset  = 0x00b390 + nm_symbol_value` (main .text starts at file offset 0xb390).
COMDAT sections (`.text.ClassName::Method`) have independent file offsets and their own VMA=0.
Do NOT add 0xb390 to get Ghidra addresses — that gives the file byte position, not the address.

See also: `OA_auth.md` for detailed authentication/patching analysis.

---

## Module Role

OA.ko is the entire synthesis engine running as a kernel module. It contains:
- All synthesis voice models (PCM, VPM, Organ, Piano, MS-20, PolySix, Plucked, Analog Sync, EP)
- Effect processing, EQ, routing
- MIDI dispatcher and controller data processing
- PCM bank management and authorisation
- Audio driver interface (Korg USB audio)
- Front panel / UI interface
- Streaming sample engine
- HDR recording
- License key management (CSTGKLMManager / CSTGKLEG)
- Security integration with loadmod.ko

---

## Initialisation Sequence

`CSTGEngine::Initialize` (Ghidra `0x000001b0` / file `0x000cb40` / runtime `0x59CE61B0`) is the top-level init. Key sequence:

```
CSTGEngine::Initialize
  ├─ [allocate + construct all subsystem objects]
  ├─ CSTGKLMManager::Initialize          → CSTGKLEG::Initialize (audio rate params)
  ├─ CSTGInstalledEXProducts::Initialize → reads /korg/rw/Startup/AuthorizationStrings
  ├─ CSTGKLMManager::AuthorizeBuiltins   → pre-authorises factory ROM banks
  ├─ InitCdromSupport()                  → checks loadmod.ko magic value
  └─ [if check fails: set degraded audio globals allPlusOne/allMinusOne/kAudXBZD]
```

---

## Security Integration with loadmod.ko

### `InitCdromSupport` (Ghidra `0x00000040` / file `0x0000b3d0` / runtime `0x59CE6040`)

Checks that loadmod.ko is loaded and has set up the kernel magic correctly.

1. Calls the Korg-patched `init_cdrom_command` kernel function with command code `0xA0F3`
2. Expects return value **-42** (`0xFFFFFFD6`) — the Korg kernel magic return
3. If that passes: reads the pointer loadmod.ko stored as `sXCmd`/`sCdromCommand`
4. Checks `*(uint32*)(ptr + 5) == 0x22FB39CC` — the magic dword
5. Returns 0 (success) or -1 (fail)

### Degradation on failure (`CSTGEngine::Initialize` text `+0x8B6`)

When `InitCdromSupport` returns non-zero:
```c
allPlusOne[0..3]  = 0x3f333333  // 0.7f  — IIR amplitude smoother coefficient (×4 stereo pairs)
allMinusOne[0..3] = 0xbe4ccccd  // -0.2f — paired coefficient
kAudXBZD         = 0x1f         // = 31, protection mode flag
```
These float pairs are used in the DSP voice rendering inner loop as amplitude IIR coefficients. Normally `1.0f`/`-1.0f`. Setting them to `0.7f`/`-0.2f` creates the asymmetric feedback that drives the cyclic volume fade heard on all sample banks.

**Patch:** change `74 5A` → `EB 5A` at Ghidra `0x08b6` / file `0x0bc46` to unconditionally skip the degradation block.

---

## Synthesis Voice Models

| Class | Init function | Notes |
|---|---|---|
| `CSTGPCMModel` | `0x001BB560` | Main PCM multisampled model |
| `CSTGVPMModel` | — | VPM/FM synthesis |
| `CSTGOrganModel` | — | Organ |
| `CSTGPianoModel` | — | Piano |
| `CSTGMS20Model` | — | MS-20 filter model |
| `CSTGPolysixModel` | — | Polysix |
| `CSTGPluckedModel` | — | Physical modelling |
| `CSTGAnalogSyncModel` | — | Analog sync |
| `CSTGEPModel` | — | Electric piano |
| `CSTGOffModel` | — | No output / off |

All models are constructed and initialised in `CSTGEngine::Initialize` via virtual dispatch.

---

## PCM Audio Rate Processing

`CSTGPCMModel::ProcessAudioRate` (`0x001AB560` / runtime `0x5A1B1560`) dispatches on a global auth flag:

```asm
; text+0x1AB578
mov    0x0, %edx              ; load CSTGGlobal::sInstance (relocation)
cmpb   $0x0, 0x6db(%edx)     ; CSTGGlobal[+0x6db] == 0?
je     unauthorised_path      ; jump if flag clear (metering/auth disabled)
call   RunPCMModelAudioRateWithMeter   ; normal path
; ...
unauthorised_path:
call   RunPCMModelAudioRate            ; bare path (no metering)
```

`CSTGGlobal[+0x6db]` is set to 1 in the `CSTGGlobal` constructor (default: authorised).  
It is updated by `CSTGControlMsgHandler::EnableAudioMetering` via `setne`.  
Also checked in `CSTGAudioBusManager::MixPerformanceOutputs`.

---

## License Key Manager — `CSTGKLMManager`

### Class hierarchy
```
CSTGKLMManager
  └─ CSTGKLMManager::Initialize → CSTGKLEG::Initialize
```
`CSTGKLEG::Initialize` sets up audio-rate parameters (sample rate ratios, smoother coefficients) from `CSTGAudioBusManager::sInstance`.

### Authorization methods

| Method | Address | Notes |
|---|---|---|
| `AuthorizeProduct` | `0x0002de60` | Authorise an EX expansion product |
| `AuthorizeVoiceModel` | `0x0002e120` | Authorise a voice model type |
| `AuthorizeEffect` | `0x0002e190` | Authorise an effect type |
| `AuthorizeMultisampleBank` | `0x0002e200` | Authorise a PCM multisample bank by UUID |
| `AuthorizeBuiltins` | `0x0002e350` | Pre-authorise all factory ROM banks (called at boot) |
| `IsAuthorizedVoiceModel` | `0x0002e600` | Query voice model auth state |
| `IsAuthorizedMultisampleBank` | `0x0002e650` | Query bank auth state (checksum check) |
| `IsAuthorizedEffect` | `0x0002e740` | Query effect auth state |

### Bank authorization algorithm

`AuthorizeMultisampleBank(auth_index, UUID[16])`:
```c
hash = FNV1a_16(UUID);   // seed=0x050C5D1F, multiplier=0x1000193
bank[+0x71] = auth_index;
bank[+0x6d] = (hash + auth_index + 1) * global_key;
```

`IsAuthorizedMultisampleBank(bank)`:
```c
if (bank[+0x5c] & 0x08) return 1;   // factory ROM bit → always authorised
if (bank[+0x4] == 0)    return 1;   // empty UUID → always authorised
computed = (FNV1a_16(bank+0x5d) + bank[+0x71] + 1) * global_key;
return (computed == bank[+0x6d]);
```

`global_key` = `*(CSTGKLMManager*)` — a per-instrument constant baked into the instance.  
**Patch to authorise all banks:** replace `IsAuthorizedMultisampleBank` at Ghidra `0x2e650` / file `0x399e0` with `B8 01 00 00 00 C3` (MOV EAX,1; RET).

`AuthorizeBuiltins` iterates over all legacy bank UUIDs (`sLegacyBankPrefix`) and authorises them with `auth_index = 0`. Factory banks also have `bank+0x5c & 0x08` set.

---

## AuthorizationStrings File

Path: `/korg/rw/Startup/AuthorizationStrings`

### Read chain (boot time — no dongle required)
```
CSTGInstalledEXProducts::Initialize  (Ghidra 0x048620 / file 0x0f3fb0)
  └─ CSTGFile_ReadFileIntoNewBuffer()    reads AuthorizationStrings
  └─ ParseAuths(buffer, length)          (Ghidra 0x207c50 / file 0x2b2fe0)
       for each whitespace-delimited token (≥ 0x18 bytes decoded):
         DecodeBytesFromAscii()
         ParseAuth(callback=AuthorizeMultisampleBank)  (Ghidra 0x207890 / file 0x2b2c20)
           moancjsd82() → extract 15-byte UUID
           MD5(auth_entry) verified against PCM bank file
           on match → AuthorizeMultisampleBank(index, UUID)
```

### Runtime path (requires dongle)
```
VerifyAuthorizationString  (Ghidra 0x207de0 / file 0x2b3170)
  nv2ac_read_data() × 3   dongle check (sum must == 0)
  DecodeBytesFromAscii()
  ParseAuth()
```

`SetupAtmelForAuthorizations` (Ghidra `0x207a50` / file `0x2b2de0`) initialises the Atmel security IC before auth operations.  
The `fFfFfFfFfFfF*` obfuscated names have been fully renamed — see table below.

### Atmel NV2AC / GPA functions in OA.ko

Same protocol as GetPubIdMod.ko (Atmel AT88SC CryptoMemory, GPA stream cipher).

Addresses below are Ghidra addresses (= nm value = ELF symbol offset; file_offset = addr + 0xb390).

### GPA stream cipher primitives

| Function       | Ghidra addr | Old name           | Role |
|----------------|-------------|--------------------|------|
| `gpa_reset`    | `0x4f4180`  | `bzzzzzzzzzzzt11`  | Zero all LFSR cells (RA–RG, SA–SG, TA–TE, gpa_byte); call before gpa_key_setup |
| `gpa_clock`    | `0x4f3d00`  | `bzzzzzzzzzzzt12`  | Advance all three GPA LFSRs one step; accumulates one nibble into gpa_byte; two calls = one keystream byte |
| `gpa_key_setup`| `0x4f4210`  | `fFfFfFfFfFfF11`   | Full key schedule: calls gpa_reset, then loads IV (8B) + key (7B) → 3×8B session keys |

### NV2AC AT88SC chip command layer

| Function                 | Ghidra addr | Old name                   | Role |
|--------------------------|-------------|----------------------------|------|
| `nv2ac_send_cmd`         | `0x4f40f0`  | `bzzzzzzzzzt16`            | Route cmd packet to sync_read_cmd (nibble==6/2) or sync_cmd; always msleep(20ms) |
| `nv2ac_copy_response`    | `0x4f4140`  | `bzzzzzzzzzt17`            | Copy N bytes from g_abNv2acResponseBuf to caller's buffer |
| `nv2ac_check_cmd_success`| `0x4f4c40`  | `bzzzzzzzzzzzt15`          | Read one status byte; return 1 if ≠0xFF (success), 0 if ==0xFF (busy/error) |
| `nv2ac_read_byte`        | `0x4f3ea0`  | `fFfFfFfFfFfF1C.clone.0`   | Cmd 0xB6: read 1 byte; GPA-decrypt if addr≥0xB0 and mode==2; Ghidra-generated clone |
| `nv2ac_read_block`       | `0x4f4a80`  | `fFfFfFfFfFfF1C`           | Cmd 0xB6: block read from response buf, GPA XOR for addr≥0xB0 |
| `nv2ac_read_data`        | `0x4f4840`  | `fFfFfFfFfFfF13`           | Cmd 0xB2: Read User Zone (≤64B), GPA cipher XOR; called ×3 in VerifyAuthorizationString |
| `nv2ac_verify_password`  | `0x4f4a00`  | `fFfFfFfFfFfF1A`           | Cmd 0xB4: Verify Password, zone 0-3 |
| `nv2ac_io_request`       | `0x4f4c70`  | `fFfFfFfFfFfF1F`           | Generic dispatcher via global cmd_3093, then calls nv2ac_read_byte |
| `nv2ac_enable_cipher`    | `0x4f4ce0`  | `fFfFfFfFfFfF1G`           | `__regparm3(unused, unused, byte *auth_session_key)` — loads session_q2 via nv2ac_send_cmd, sets mode=1 on success |
| `nv2ac_enable_encrypt`   | `0x4f4d10`  | `fFfFfFfFfFfF1H`           | `__regparm3(unused, unused, byte *auth_session_key)` — same with second gpa_key_setup round's session_q2, sets mode=2 |

### Global: NV2AC response buffer

| Symbol                  | nm addr    | Old name          | Role |
|-------------------------|------------|-------------------|------|
| `g_abNv2acResponseBuf`  | `0x5c90e0` | `bzzzzzzzzzt18`   | BSS global; holds the raw byte(s) returned by the chip after each command. Rename this label in Ghidra manually (right-click → Rename Label). |

**Calling convention confirmed via disassembly of `SetupAtmelForAuthorizations`:**  
Both functions receive `(EAX=0, EDX=&key_buf, ECX=&session_q2)` — only ECX (param_3 = `auth_session_key`) is used.  
The `auth_session_key` is the 8-byte `session_q2` buffer produced by each `gpa_key_setup` round.  
Both return the raw result of `bzzzzzzzzzzzt16` (0 = chip accepted the key, mode enabled).

---

## PCM Decryption

`CSTGPCMDecrypter::Decrypt` (Ghidra `0x0002f4b0` / file `0x000ba240`):
- Uses AES with a key table initialised by LCG: `key = key * 0xBB38435 + 0x3619636B`, seed `0x6C87385F`
- Key selection: `sKeyTable[(sample_id * block_id * 0x7E136DF5) & 0x7FFF]`
- Decrypts PCM data blocks for EX expansion banks
- Separate from the authorization checksum — decryption uses a fixed global key table

---

## Per-Bank Fade (unauthorized samples)

```
IsAuthorizedMultisampleBank() → false
  ↓
CSTGTG92OscBase::IsUsingAnyUnauthorizedMultisamples() (0x014C7D0)
  reads osc_context bank authorization field
  ↓
CSTGPCMModelPatch::IsUsingAnyUnauthorizedMultisamples() (0x05B43B0)
  returns *(int*)(patch + 0x1D0) != 0
  ↓
CSTGPCMModel::ProcessAudioRate
  voices on unauthorized banks receive attenuated/fading treatment
```

`CSTGPatch::IsUsingAnyUnauthorizedMultisamples` (base class, `0x05B4010`) returns 0 unconditionally — only the derived patch classes have real checks.

**Patch to suppress fade flag:** replace `CSTGPCMModelPatch::IsUsingAnyUnauthorizedMultisamples` at `0x5B43B0` with `31 C0 C3` (XOR EAX,EAX; RET).

---

## Multisample Bank Manager

| Function | Address | Notes |
|---|---|---|
| `StartupInitializeRAMBank` | `0x0004CFF0` | Init a RAM-resident bank |
| `StartupInitializeROMBank` | `0x0004D0A0` | Init a ROM bank (copies UUID from `kROMBankUUID`, calls LoadBank) |
| `StartupInitializeEXs` | `0x0004E200` | Init EX expansion banks |
| `AccessBank` | — | Look up a bank by UUID |
| `AccessBankWithLegacyRAMAlias` | — | Look up bank including legacy RAM alias |

ROM banks: `bank[+0x5d..0x6c]` = `kROMBankUUID` at load, `bank[+0x5c] |= 0x08` (factory flag).  
RAM / EX banks: UUID set from the bank metadata; no factory flag; must be authorised via AuthorizationStrings or `AuthorizeMultisampleBank`.

---

## CSTGEngine Struct Layout

`CSTGEngine` is the top-level synthesis engine coordinator. Size: **8 bytes** (confirmed: `AllocAligned(0x8, 0x10)` in `setup_global_resources`). No vtable (no virtual functions, no base class).

```c
struct CSTGEngine {           // size = 8
    uint32_t  dw_reserved0;            // +0x00  not set by constructor; zero-filled by allocator
    bool      fEmergency_steal_active; // +0x04  SETNZ from CEmergencyStealer::sInstance[+0x18];
                                       //         true when a voice steal is occurring this tick
    bool      fSuppress_audio_tick;    // +0x05  if non-zero, PreAudioTick skips all processing
                                       //         and returns 0 (only MIDI keep-alive runs)
    uint16_t  w_pad6;                  // +0x06  padding
};
```

`CSTGEngine::sInstance` is the global singleton pointer (stored at `0x006e5888`).

All subsystems (audio manager, voice allocator, effect manager, KLM, etc.) are owned as their **own static `sInstance` globals**, not as member pointers inside CSTGEngine. The engine itself is a thin coordinator and is only 8 bytes.

---

## Key Global Structures

| Symbol | Notes |
|---|---|
| `CSTGEngine::sInstance` | Engine singleton; `+0x04` = `fEmergency_steal_active`, `+0x05` = `fSuppress_audio_tick` |
| `CSTGGlobal::sInstance` | Global state singleton; `+0x6db` = PCM auth/metering flag |
| `CSTGKLMManager` instance | `*[0]` = `global_key` for auth checksum |
| `CSTGVoiceModelManager::sInstance` | Voice model registry |
| `CSTGEffectManager::sInstance` | Effect registry |
| `CSTGAudioBusManager::sInstance` | Audio bus / sample rate info |
| `allPlusOne` / `allMinusOne` | DSP IIR coefficients — set to degraded values when loadmod check fails |
| `kAudXBZD` | Protection mode flag — set to `0x1f` when loadmod check fails |

---

## EX Product System

| Function | Address | Notes |
|---|---|---|
| `CSTGInstalledEXProducts::Initialize` | `0x058620` | Boot-time: reads AuthorizationStrings |
| `CSTGInstalledEXProducts::ReInitialize` | `0x0584D0` | Re-run auth after install |
| `CSTGInstalledEXProducts::IsOptionInstalled` | `0x058A0` | Check if EX option is installed |
| `CSTGInstalledEXProducts::IsOptionAuthorized` | `0x058130` | Check if installed option is authorised |
| `CSTGInstalledEXProducts::AuthorizeProductByFilename` | `0x0581D0` | Authorise by product filename |
| `CSTGInstalledEXProducts::VerifyAndSaveAuthString` | `0x058290` | Verify an auth string and persist it |
| `CSTGInstalledEXProducts::LoadProductFile_private` | `0x059170` | Load an EX product descriptor file |
| `CSTGInstalledEXProducts::InstallProductFile` | `0x059310` | Install an EX product |
| `CSTGKLMManager::AuthorizeProduct` | `0x0003DE60` | Final authorize step for a product |

---

## Relationships with Other Modules

| Module | Relationship |
|---|---|
| `loadmod.ko` | OA.ko checks the magic value loadmod stores in kernel memory via `init_cdrom_command`. If absent → audio degradation. |
| `OmapNKS4Module.ko` | Provides stgNV2AC hardware dongle comms. OA.ko calls `stgNV2AC_sync_cmd` / `stgNV2AC_sync_read_cmd` (imported) for runtime auth string verification. |
| `GetPubIdMod.ko` | Public ID / serial number generation. |
| `Eva` | Front-end application binary. Communicates with OA.ko via shared memory / proc interface (`InitSharedMemProcInterface`, `InitPcmModProcInterface`). |
