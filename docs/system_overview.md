# Korg Kronos — System Architecture

Overall system architecture as understood from RE of kernel modules and supporting files.

---

## Platform

- **CPU**: OMAP (ARM-based) SoC, but kernel modules are x86 LE 32-bit — the Kronos uses an **x86 PC-style co-processor** for the synthesis engine alongside the OMAP main CPU
- **Kernel**: Linux 2.6.32 (x86), Korg-patched with modified `register_cdrom()` and `init_cdrom_command()` that return magic values
- **Linux user base**: `/`, mounted read-only; writable data under `/korg/rw/`

---

## Filesystem Layout

```
/
├── korg/
│   ├── Eva/              loop-device mounted (encrypted, keyed by pairFact)
│   ├── Mod/              loop-device mounted (encrypted) — contains OA.ko etc.
│   │   └── OA.ko
│   ├── rw/               writable area
│   │   ├── Startup/
│   │   │   └── AuthorizationStrings    EX bank auth keys (ASCII hex encoded)
│   │   ├── PCM/
│   │   │   ├── Bank00    EX expansion sample banks
│   │   │   ├── Bank01
│   │   │   ├── ...
│   │   │   ├── WaveMotion/             loop-device mounted (encrypted)
│   │   │   └── Precache/
│   │   │       └── PRECACHE.IMG
│   │   ├── HD/           HD recording
│   │   ├── PRELOAD/
│   │   │   └── PianoTypes
│   │   └── Options/
│   └── rw2/              secondary writable (some banks mirrored)
│       ├── PCM/
│       └── HD/
├── sbin/
│   ├── loadmod.ko        security/setup kernel module
│   └── loadoa            userspace loader that inserts modules
├── proc/
│   ├── .update           kernel thread proc entry (created by loadmod.ko)
│   └── iFactc3           proc entry created by Korg-patched kernel; provides pairFact data
└── tmp/
    ├── stgStatus         error code written by loadmod.ko for loadoa to display
    ├── UpdateOS          if present, run by /proc/.update kernel thread
    └── ...
/sbin/UpdateOS            primary update binary location
```

---

## Boot Sequence (Synthesis Engine)

```
1. Linux kernel boots (2.6.32, Korg-patched x86)

2. loadoa (userspace) runs:
   a. insmod loadmod.ko
      ├─ VerifyCodeIntegrityMd5    — MD5 check of filesystem  [error 1 on fail]
      ├─ RegisterFakeCdromDriver   — installs fake cdrom, stores 0x22FB39CC in kernel  [error 3 on fail]
      ├─ ReadPairFactAndVerify     — reads /proc/iFactc3, decrypts pairFact  [error 4 on fail]
      ├─ RetrieveSecurityICKey     — queries stgNV2AC IC via OmapNKS4Module.ko  [error 5 on fail]
      ├─ Hooks sys_mount / sys_oldumount (encrypted loop mounts for /korg/Eva, /korg/Mod, /korg/rw/PCM/WaveMotion)
      ├─ Creates /proc/.update kernel thread (polls for /sbin/UpdateOS or /tmp/UpdateOS)
      └─ Writes status to /tmp/stgStatus (0 = success)
   
   b. If loadmod.ko succeeds:
      insmod OmapNKS4Module.ko   — stgNV2AC dongle driver
      insmod OA.ko               — synthesis engine
         ├─ CSTGEngine::Initialize
         │   ├─ [construct all subsystem objects]
         │   ├─ CSTGKLMManager::Initialize
         │   ├─ CSTGInstalledEXProducts::Initialize
         │   │   └─ ParseAuths("/korg/rw/Startup/AuthorizationStrings")
         │   │       └─ AuthorizeMultisampleBank() for each valid entry
         │   ├─ CSTGKLMManager::AuthorizeBuiltins  — pre-authorise ROM banks
         │   └─ InitCdromSupport()                 — check loadmod magic value
         │       ├─ success: normal audio coefficients kept
         │       └─ fail: allPlusOne=0.7f, allMinusOne=-0.2f, kAudXBZD=0x1f (cyclic fade)
         └─ InitSharedMemProcInterface / InitPcmModProcInterface
   
   c. Launch Eva (main application binary)
      └─ Communicates with OA.ko via shared memory / proc interface
```

---

## Inter-Module Communication

### loadmod.ko → OA.ko (via kernel memory)

loadmod.ko stores the authentication token in kernel memory via the fake cdrom driver:
- `g_pCdromDrvInfo + 5` → dword `0x22FB39CC`
- OA.ko reads this in `InitCdromSupport` using `init_cdrom_command` 

### OA.ko → OmapNKS4Module.ko (imported symbols)

OA.ko imports:
- `stgNV2AC_sync_cmd` — send a synchronous command to the stgNV2AC IC
- `stgNV2AC_sync_read_cmd` — send command and read response

Used in `SetupAtmelForAuthorizations` and `VerifyAuthorizationString` for runtime auth key verification.

### OA.ko ↔ Eva (shared memory / proc)

- `InitSharedMemProcInterface` — shared memory interface (performance/program data)
- `InitPcmModProcInterface` — PCM module proc interface
- `CleanupPcmModProcInterface` / `CleanupSharedMemProcInterface`

### loadmod.ko → kernel thread → userspace

`/proc/.update` kernel thread polls for:
1. `/sbin/UpdateOS` (primary)
2. `/tmp/UpdateOS` (override / temporary)

Runs whichever it finds as a usermode helper (`call_usermodehelper_setup`).

---

## Security Hardware

### stgNV2AC (Atmel security IC, via OmapNKS4Module.ko)

- Stores encryption keys used to decrypt the `pairFact` file
- Must be present and responsive for loadmod.ko error code 5 to be avoided
- Also queried at runtime by OA.ko's `SetupAtmelForAuthorizations` when verifying new auth strings via front panel
- Boot-time `AuthorizationStrings` read does **not** require the dongle

### Encrypted loop filesystems

Three filesystem paths are encrypted loop devices:
- `/korg/Eva` — Eva application code
- `/korg/Mod` — Kernel modules (OA.ko etc.)
- `/korg/rw/PCM/WaveMotion` — WaveMotion PCM bank

Keys come from `pairFact`. loadmod.ko intercepts mount calls to these paths and supplies the loop device encryption key before allowing the mount to proceed.

---

## Known Kernel Modules

| Module | Purpose | Runtime .text base |
|---|---|---|
| `loadmod.ko` | Security, syscall hooks, pairFact, magic value | `0x58EC9FD0` |
| `OmapNKS4Module.ko` | stgNV2AC hardware security IC driver | unknown |
| `OA.ko` | Entire synthesis engine | `0x59CE6000` |
| `OmapVideoModule.ko` | Video output module | unknown |
| `GetPubIdMod.ko` | Public ID / serial number | unknown |

---

## Known Userspace Binaries

| Binary | Purpose |
|---|---|
| `loadoa` | Module loader; inserts loadmod.ko, OmapNKS4Module.ko, OA.ko; displays /tmp/stgStatus |
| `Eva` | Main Kronos application (touch UI, sequencer, etc.) |
| `UpdateOS` | OS update binary (run by loadmod.ko's kernel thread) |
| `InstallEXs` | EX expansion installer |

---

## Kronos Kernel Modifications (GPL violations noted in blog)

The Korg 2.6.32 kernel has at minimum these non-standard modifications:

1. **`register_cdrom()`** — returns a magic value instead of 0 on success
2. **`init_cdrom_command()`** — returns **-42** (`0xFFFFFFD6`) instead of 0
3. **`/proc/iFactc3`** — proc entry added to the kernel (not created by any module); provides pairFact data to loadmod.ko

---

## Key Constants

| Constant | Value | Where used |
|---|---|---|
| `init_cdrom_command` magic return | `0xFFFFFFD6` (-42) | loadmod.ko `RegisterFakeCdromDriver`, OA.ko `InitCdromSupport` |
| `g_pCdromDrvInfo+5` magic dword | `0x22FB39CC` | OA.ko `InitCdromSupport` |
| `RotatePermuteBits(0x3d9d7984)` | `0x22FB39CC` | How loadmod.ko computes the magic value |
| OA.ko degraded `allPlusOne` | `0x3f333333` (0.7f) | DSP amplitude IIR coefficient on magic check fail |
| OA.ko degraded `allMinusOne` | `0xbe4ccccd` (-0.2f) | DSP amplitude IIR coefficient on magic check fail |
| OA.ko protection mode flag | `0x1f` (31) | `kAudXBZD`, set when magic check fails |
| FNV-1a seed | `0x050C5D1F` | Bank UUID hash in `AuthorizeMultisampleBank` |
| FNV-1a multiplier | `0x1000193` | Bank UUID hash |
| PCM key table LCG seed | `0x6C87385F` | `CSTGPCMDecrypter` key table init |
| PCM key table LCG multiplier | `0xBB38435` | LCG: `key = key * 0xBB38435 + 0x3619636B` |
