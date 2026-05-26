# OA.ko — Authentication & Magic Value Analysis

Analysis performed via Ghidra MCP (GhidraMCP 5.10.0, project: kronos).  
Binary: `/korg/Mod/OA.ko` — 21,615 functions, x86 LE 32-bit kernel module.

---

## Part 1 — loadmod.ko Magic Value Integration

### `InitCdromSupport` (text +`0x0040`)

Runs two checks against loadmod.ko's kernel hooks:

**Check 1 — Korg kernel magic return value**
Calls `init_cdrom_command` (hooked by loadmod.ko) with a crafted cdrom struct (command code `0xA0F3`).  
The Korg-patched kernel returns **-42** (`0xFFFFFFD6`) as a magic return value.  
Any other return → `InitCdromSupport` returns -1 immediately.

**Check 2 — Magic dword at g_pCdromDrvInfo+5**
If check 1 passes, reads the pointer loadmod.ko stored as `sXCmd`/`sCdromCommand` and verifies:
```
*(int*)(ptr + 5) == 0x22FB39CC
```
Match → return 0 (success).  No match → return -1 (fail).

```asm
; text+0x85
cmp    $0xffffffd6, %eax     ; Korg kernel magic return?
je     0xa8                  ; yes → check magic dword
mov    $0xffffffff, %eax     ; no  → return -1
ret

; text+0xc6
cmpl   $0x22fb39cc, 0x5(%edx)   ; g_pCdromDrvInfo+5 == 0x22FB39CC ?
sete   %al                       ; al = 1 if match
sub    $0x1, %eax               ; 0 = success,  -1 = fail
```

---

### `CSTGEngine::Initialize` (text +`0x01B0`) — degradation block

Called after all subsystem constructors. Near the end:

```asm
; text+0x8AF
call   InitCdromSupport
test   %eax, %eax
je     0x912           ; EAX==0 (success) → skip degradation, return normally

; --- DEGRADATION BLOCK (entered only on failure) ---
; text+0x8B8 .. 0x90F
movl   $0x3f333333, allPlusOne[0]     ; 0.7f
movl   $0xbe4ccccd, allMinusOne[0]    ; -0.2f
movl   $0x3f333333, allPlusOne[1]
movl   $0xbe4ccccd, allMinusOne[1]
movl   $0x3f333333, allPlusOne[2]
movl   $0xbe4ccccd, allMinusOne[2]
movl   $0x3f333333, allPlusOne[3]
movl   $0xbe4ccccd, allMinusOne[3]
movl   $0x1f,       kAudXBZD          ; protection mode flag = 31
; --- END DEGRADATION BLOCK ---

; text+0x912 — normal return
```

**What these globals do:**  
`allPlusOne` / `allMinusOne` are IIR amplitude-smoother coefficients used in the DSP voice rendering inner loop (one pair per stereo channel group, 4 pairs total).  
Normally `allPlusOne = 1.0f`, `allMinusOne = -1.0f`.  
Setting them to `0.7f` / `-0.2f` creates an asymmetric IIR pair that drives the **cyclic volume fade** heard on all sample banks when loadmod.ko is absent or its magic value is not present.  
`kAudXBZD = 0x1f` enables an additional protection flag checked elsewhere in the audio pipeline.

---

### Patch 1 — Skip magic-value degradation entirely

**Recommended (2-byte patch in `CSTGEngine::Initialize`):**

| `.text` section offset | Current bytes | Patched bytes | Effect |
|---|---|---|---|
| `0x08B6` | `74 5A` (JE +90) | `EB 5A` (JMP +90) | Unconditionally skip degradation block |

The `allPlus/Minus` globals are never overwritten; `kAudXBZD` stays 0.  
OA.ko works correctly whether or not loadmod.ko is loaded.

**Alternative — stub out `InitCdromSupport` itself:**

| `.text` offset | Current | Patched | Effect |
|---|---|---|---|
| `0x0040` (function start) | `55 31 C0 B9 ...` | `31 C0 C3 90 ...` (XOR EAX,EAX; RET; NOP) | Always returns 0 (success) |

---

## Part 2 — `AuthorizationStrings` File and Per-Bank Auth

### File location and format

```
/korg/rw/Startup/AuthorizationStrings
```

Plain ASCII file; entries separated by whitespace.  
Each entry is a hex-ASCII-encoded binary blob (≥ 0x18 bytes decoded).  
Each decoded blob encodes a 15-byte bank UUID + an authorization index + an MD5 integrity check.

---

### Boot-time read chain

```
CSTGInstalledEXProducts::Initialize  (0x058620)
  ├─ CFileFolder::ProcessFiles()         scan EX product install folders
  └─ CSTGFile_ReadFileIntoNewBuffer()    read /korg/rw/Startup/AuthorizationStrings
  └─ ParseAuths(buffer, length)          (0x217C50)
       tokenise on whitespace (skip chars outside [0-9A-Za-z-])
       for each token ≥ 0x18 bytes:
         DecodeBytesFromAscii()           hex ASCII → binary
         ParseAuth(ECX=AuthorizeMultisampleBank callback)  (0x217890)
           moancjsd82() → parse UUID, must be exactly 15 bytes
           md5_init / md5_append          hash the decoded auth entry
           CSTGFile_ReadFileIntoNewBuffer  read PCM bank file for cross-check
           md5_append / md5_finish        compute MD5 of bank file
           if MD5 matches → call ECX callback: AuthorizeMultisampleBank(index, UUID)
           returns 0 on success
```

> **Note:** The *boot-time* `ParseAuths` path does **not** require the hardware dongle.  
> The runtime `VerifyAuthorizationString` (0x217DE0) path *does* check the Atmel/stgNV2AC  
> dongle (three `fFfFfFfFfFfF13()` calls, sum must equal 0) — this only matters when  
> installing new auth strings via the front-panel UI at runtime.

---

### Authorization storage — `CSTGKLMManager`

#### `AuthorizeMultisampleBank(auth_index, UUID[16])` — 0x03E200

```c
// FNV-1a hash of the 16-byte UUID
hash = ((uuid[0] ^ 0x050c5d1f) * 0x1000193)
         ^ uuid[1]) * 0x1000193 ^ uuid[2]) * ... ^ uuid[15];

bank[+0x71] = auth_index;                              // store index
bank[+0x6d] = (hash + auth_index + 1) * global_key;   // store checksum
```

`global_key` is `*(CSTGKLMManager*)` — a per-instrument constant baked into the KLM manager instance, binding authorizations to this specific hardware unit.

#### `IsAuthorizedMultisampleBank(bank)` — 0x03E650

```c
if (bank[+0x5c] & 0x08) return 1;    // factory ROM bit → always authorized
if (bank[+0x4]  == 0)   return 1;    // empty UUID     → always authorized

// re-derive checksum from stored UUID bytes (bank+0x5d..0x6c) and index (bank+0x71)
computed = (FNV1a_16(bank+0x5d) + bank[+0x71] + 1) * global_key;
return (computed == bank[+0x6d]);     // 1 = authorized, 0 = not
```

#### `AuthorizeBuiltins()` — 0x03E350

Called at boot. Pre-authorizes all factory ROM banks using the `sLegacyBankPrefix` UUID pattern with `auth_index = 0`.  Factory banks also have `bank+0x5c & 0x08` set, so they pass the fast-path check regardless.

---

### Per-bank fade — call chain

When a voice references an unauthorized bank, the fade is propagated as follows:

```
CSTGKLMManager::IsAuthorizedMultisampleBank()        (0x03E650)
  ↓ result feeds into oscillator bank state
CSTGTG92OscBase::IsUsingAnyUnauthorizedMultisamples() (0x014C7D0)
  reads: *(int*)(osc_context + bank_index_offset)
  ↓
CSTGPCMModelPatch::IsUsingAnyUnauthorizedMultisamples() (0x05B43B0)
  returns: *(int*)(patch + 0x1D0) != 0    ← count of unauthorized banks in patch
  ↓
CSTGPCMModel::ProcessAudioRate()           (0x1AB560)
  if (CSTGGlobal[+0x6db] == 0)
      call RunPCMModelAudioRate            ← bare path
  else
      call RunPCMModelAudioRateWithMeter   ← normal path with metering
  (both paths apply per-voice fade for voices on unauthorized banks)
```

`CSTGGlobal[+0x6db]` is set to 1 in the `CSTGGlobal` constructor (default = authorized).  
It is updated by `CSTGControlMsgHandler::EnableAudioMetering` based on current auth state.  
It is also checked in `CSTGAudioBusManager::MixPerformanceOutputs` (0x024120).

---

### Patch 2 — Authorize all banks unconditionally

**Option A — Stub `IsAuthorizedMultisampleBank` to always return 1 (recommended):**

| `.text` offset | Current | Patched bytes | Effect |
|---|---|---|---|
| `0x03E650` | function prologue | `B8 01 00 00 00 C3` (MOV EAX,1; RET) | All banks treated as authorized everywhere |

**Option B — Suppress the unauthorized flag at patch level only:**

| `.text` offset | Current | Patched bytes | Effect |
|---|---|---|---|
| `0x05B43B0` | `0F B6 80 D0 01 00 00` ... | `31 C0 C3 90 ...` (XOR EAX,EAX; RET) | `IsUsingAnyUnauthorizedMultisamples` always returns false |

Option A is more thorough — it fixes the authorization state everywhere it is queried.  
Option B only suppresses the fade trigger in `CSTGPCMModelPatch`; other code that calls  
`IsAuthorizedMultisampleBank` directly still sees unauthorized for un-keyed banks.

---

## Summary of Recommended Patches

| What | `.text` section offset | Old bytes | New bytes |
|---|---|---|---|
| Skip cdrom/magic degradation | `0x0008B6` | `74 5A` | `EB 5A` |
| Authorize all multisample banks | `0x03E650` | (function prologue) | `B8 01 00 00 00 C3` |

Both patches are independent and can be applied separately.  
The `.text` offsets are relative to the start of the `.text` ELF section (i.e. these are the  
values shown in `objdump -d` output for the relocatable `.ko` file, before kernel load-time  
relocation). To find the correct file byte offset, add the `.text` section's `sh_offset` from  
the ELF section headers (`readelf -S OA.ko`).
