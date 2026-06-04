# loadmod.ko — RE Notes

## Module Purpose
Korg synthesizer kernel-level copy protection module. Runs on kernel 2.6.32 i386
(stext at 0x401010e8). Loaded as a `.ko` into an embedded Linux system.

## High-Level Flow (init_module)
1. **MD5 self-check** — `VerifyCodeIntegrityMd5` hashes the module's own code and
   compares against `maybe_a_key[]` to detect patching.
2. **CDROM driver registration** — `RegisterFakeCdromDriver` registers a fake
   `cdrom_device_info` with the kernel; the real `stgNV2AC_*` calls go to the
   hardware dongle.
3. **Anti-tamper probe** — `ReadPairFactAndVerify` (offset `0x4e90`) opens
   the regular file **`/.pairFact3`**, reads 0x50 bytes, and exercises the
   dongle with a known magic number. (The blog described this path as
   `/proc/iFactc3`, but binary analysis shows the path is assembled
   byte-by-byte on the stack as `/.pairFact3`, and `/proc/iFactc3` does
   not exist on a running 3.2.2 device.)
4. **Signal mask** — blocks all signals on current task (prevents debugger
   interrupts from working cleanly).
5. **VMA alloc** — `FindAndAllocVmaForCode` walks `mm->mmap` for a VMA at
   ~0x08xxxxxx and calls `do_brk()` to reserve space for injected code.
6. **RSA/GMP license check** — `VerifyDongleLicense`: full challenge-response
   using Blowfish stream cipher + GMP modular exponentiation.
7. **Kernel hooks** (six pointer patches):
   - `vmalloc` → `HookedVmalloc`
   - `call_usermodehelper_exec` → `HookedUsermodeHelperExec`
   - `call_usermodehelper_setup` → `UsermodeHelperSetupTrampoline`
   - loop-ctl fn → `LoopCtlTrampoline`
   - two scheduler/dispatch ptrs → `HookTrampolineA`, `HookTrampolineB`
8. **Kernel thread** — `KernelThreadMain` for async loop-device mounting.

## Crypto Subsystems
| Subsystem | Functions | Notes |
|---|---|---|
| NLFSR stream cipher | `StreamCipherStep`, `CipherKeySchedule`, `CipherStateReset` | 3 shift-registers: R(7,mod31), S(7,mod129), T(5,mod31); non-linear combiner → `gpa_byte` |
| Blowfish | `BlowfishEncryptBlock`, `BlowfishExpandKey`, `g_adwBlowfishInitTable` | Standard Blowfish, pi-seeded; used in challenge-response |
| MD5 | `Md5InitState`, `md5_init`, `md5_append`, `md5_finish` | Standard MD5; used for self-integrity |
| RSA/GMP | `RsaVerifyWithGmp`, `StoreRsaParams` | `__gmpz_powm`; params hardcoded as decimal strings |
| LCG | `DecryptObfuscatedString`, `DecryptAndFeedPathToMd5` | `lcg = lcg * 0x0BB38435 + 0x3619636B` |

## Intercepted Paths
`HookedVmalloc` and `HookedUsermodeHelperExec` intercept these app paths:
- `/korg/Eva` — Korg EVA sound engine
- `/korg/Mod` — Korg module loader  
- `/korg/rw/PCM/WaveMotion` — Korg WaveMotion PCM engine
- `/dev/loop` — suppressed (vmalloc returns 0)

## Key Globals Renamed
| Old name | New name | Description |
|---|---|---|
| `maybe_a_key` | `code_fingerprint_table` | MD5 byte lookup table |
| `some_global_ints` | `g_pRsaParamStrs` | RSA param string pointers |
| `aaaaaaaaa9` | `g_dwLcgState` | LCG stream cipher state |
| `aaaaaaaaa16` | `g_dwThreadWakeSignal` | kernel thread wake flag |
| `bzzzzzzzzzt18` | `g_abDeviceResponseBuf` | dongle response buffer |
| `orig_vmalloc` | `g_pnOrigVmalloc` | saved vmalloc pointer |
| `ref_counter` | `g_dwLockRefCount` | AcquireLock/ReleaseLock counter |
