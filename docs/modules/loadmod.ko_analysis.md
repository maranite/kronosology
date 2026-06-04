# loadmod.ko — Deep Analysis

Kernel module loaded early in the Korg Kronos boot sequence by `loadoa`.  
Architecture: x86 LE 32-bit, kernel 2.6.32.  
Runtime `.text` base: **`0x58EC9FD0`** (from `/proc/kallsyms`).  
Ghidra image base (to set): `0x58EC9FD0`.

---

## Purpose

loadmod.ko is a security and environment setup module. It:
1. Verifies the filesystem has not been tampered with (MD5 integrity check)
2. Installs a fake cdrom driver to store a magic authentication token in kernel memory
3. Reads and verifies the `pairFact` authorization file (encrypted loop-device keys)
4. Communicates with the hardware security IC (stgNV2AC dongle via OmapNKS4Module.ko)
5. Hooks several syscalls to intercept and control mount operations
6. Creates a kernel thread that can trigger OS updates
7. Writes a status code to `/tmp/stgStatus` for `loadoa` to display errors

---

## init_module Error Codes

The status code written to `/tmp/stgStatus`:

| Code | Meaning | Function |
|---|---|---|
| 0 | Success | — |
| 1 | MD5 filesystem integrity check failed | `VerifyCodeIntegrityMd5` |
| 3 | Kernel check failed (register_cdrom / init_cdrom_command magic values) | `RegisterFakeCdromDriver` |
| 4 | Cannot read `/.pairFact3` authorization blob | `ReadPairFactAndVerify` (at offset `0x4e90`) |
| 5 | Security IC (stgNV2AC via OmapNKS4Module.ko) communication failed | `VerifyDongleLicense` |

Error code 2 is never generated. Sequence is 1 → 3 → 4 → 5.

---

## Function Inventory

### Crypto / Encoding utilities

| Function | Address (Ghidra) | Notes |
|---|---|---|
| `GenerateCRCForAsciiPubId` | `0x00000030` | CRC over ASCII public ID |
| `GenerateAsciiPubIdWithCRC` | `0x00000180` | Generates ASCII pub ID + CRC |
| `ValidateCRC` | `0x00000460` | CRC validation |
| `EncodeBytesToAscii` | `0x00000770` | Binary → ASCII hex |
| `DecodeBytesFromAscii` | `0x000008e0` | ASCII hex → binary |
| `HexEncode` | `0x00000c10` | Hex encoding |
| `HexAsciiToBinary` | `0x00000cb0` | Hex ASCII → binary |
| `md5_process` | `0x00001e30` | MD5 block processing |
| `md5_init` | `0x00002900` | MD5 init |
| `md5_append` | `0x00002930` | MD5 update |
| `md5_finish` | `0x00002a50` | MD5 finalise |
| `Md5InitState` | `0x00002b40` | MD5 state init |
| `BlowfishEncryptBlock` | `0x00002c60` | Blowfish block encrypt |
| `BlowfishExpandKey` | `0x00003030` | Blowfish key expansion |
| `StoreRsaParams` | `0x000032c0` | Store RSA parameters |
| `RsaVerifyWithGmp` | `0x000032e0` | RSA signature verify (uses libgmp) |
| `GetRandomBytesWrapper` | `0x00003850` | Wraps kernel `get_random_bytes` |
| `RotatePermuteBits` | `0x00003860` | `RotatePermuteBits(0x3d9d7984)` → `0x22FB39CC` |

### Security IC / Dongle communication (stgNV2AC)

| Function | Address | Notes |
|---|---|---|
| `StreamCipherStep` | `0x00000dc0` | Step the stream cipher state |
| `SendCmdAndUpdateCipherState` | `0x00000f50` | Send command + update cipher |
| `DeviceReadAndCheckStatus` | `0x00001080` | Read and validate device status |
| `DeviceRouteCommand` | `0x000011a0` | Route a command to the IC |
| `CopyFromResponseBuffer` | `0x000011f0` | Copy data from IC response buffer |
| `CipherStateReset` | `0x00001230` | Reset cipher state |
| `CipherKeySchedule` | `0x000012d0` | Key schedule for cipher |
| `DeviceReadBlock` | `0x00001910` | Read block from IC |
| `DeviceSelectPage` | `0x00001ad0` | Select IC memory page |
| `ReadBlockWithXorDecrypt` | `0x00001b50` | Read IC block + XOR decrypt |
| `QueryCipherState` | `0x00001d20` | Query current cipher state |
| `DeviceReadOrSync` | `0x00001d50` | Read or synchronise device |
| `SetModeAuthenticate` | `0x00001dc0` | Set IC to authenticate mode |
| `SetModeDecrypt` | `0x00001df0` | Set IC to decrypt mode |
| `RetrieveSecurityICKey` | `0x00003e10` | Retrieve key from security IC |
| `CipherKeySchedule` | `0x000012d0` | Key schedule (Blowfish-based) |

### Kernel hooks / syscall trampolines

| Function | Address | Notes |
|---|---|---|
| `ReturnEInval` | `0x000038f0` | Stub: just returns EINVAL |
| `SysCreateModTrampoline` | `0x00003900` | Hook for `sys_create_module` (pass-through only) |
| `SysInitModTrampoline` | `0x00003910` | Hook for `sys_init_module` (pass-through only) |
| `SysIoctlTrampoline` | `0x00003920` | Hook for `sys_ioctl` (pass-through only) |
| `SysUmountTrampoline` | `0x00003930` | Hook for `sys_umount` (pass-through only) |
| `HookedSysOldUmount` | `0x000047a0` | **Active hook** — intercepts `sys_oldumount` |
| `HookedSysMount` | `0x00004ac0` | **Active hook** — intercepts `sys_mount` |

The four trampolines (SysCreateMod, SysInitMod, SysIoctl, SysUmount) are noted as "not finished" — they do not actually block or modify any operations.

### Mount interception

`HookedSysMount` and `HookedSysOldUmount` intercept mount/umount calls involving:
- `/korg/Eva`
- `/korg/Mod`
- `/korg/rw/PCM/WaveMotion` (blog summarises as `/korg/WaveMotion`)

When a mount targeting these paths is detected, the hook sets up an encrypted loop device (using keys from `pairFact`) and executes a helper. Calls into:

| Function | Address | Notes |
|---|---|---|
| `PrepareLoopDevMount` | `0x000046e0` | Set up loop device with encryption keys |
| `MountLoopDevAndExec` | `0x00004830` | Mount the loop device and exec helper |
| `MmWriteLockAndUnmap` | `0x000044d0` | Lock and unmap memory pages |
| `OpenAndInstallFd` | `0x00004520` | Open file and install file descriptor |
| `CloseFileAndReleaseFd` | `0x000045a0` | Close fd and release |
| `FindAndAllocVmaForCode` | `0x00003b70` | Find/allocate VMA for code injection |

### Fake cdrom driver / magic value

| Function | Address | Notes |
|---|---|---|
| `RegisterFakeCdromDriver` | `0x00003ab0` | Registers fake cdrom with modified kernel hooks |
| `BuildCdromCommandStruct` | `0x00003c00` | Builds a cdrom command structure |
| `RotatePermuteBits` | `0x00003860` | Computes `0x22FB39CC` from `0x3d9d7984` |

`RegisterFakeCdromDriver` calls the Korg-patched `register_cdrom()` and `init_cdrom_command()` kernel functions, which return magic values specific to the Korg kernel. The result is stored at `g_pCdromDrvInfo + 5`. The magic value **`0x22FB39CC`** is what OA.ko checks on startup.

### pairFact / authorisation

| Function | Address | Notes |
|---|---|---|
| `ReadPairFactAndVerify` | `0x00004e90` | Read and verify the `pairFact` file |
| `DecryptAndFeedPathToMd5` | `0x00004fe0` | Decrypt a path string and feed to MD5 |

`pairFact` is the 80-byte blob at **`/.pairFact3`** — a regular file at the root of the EXT2 root partition. It contains the (chip-encrypted) keys for the three encrypted loop filesystems. `ReadPairFactAndVerify` assembles the path `/.pairFact3` byte-by-byte on the stack and calls `filp_open`, then passes the contents to the stgNV2AC chip via `OmapNKS4Module.ko` to recover the three AES-256 keys. (The kronoshacker blog's claim of a `/proc/iFactc3` proc entry was wrong — verified by direct `ls /proc/iFactc3` on a live 3.2.2 device.) See [docs/crypto/cryptoloop_keys.md](../crypto/cryptoloop_keys.md) for the full chain and the recovered final keys.

### Filesystem integrity

| Function | Address | Notes |
|---|---|---|
| `VerifyCodeIntegrityMd5` | `0x00005350` | MD5 integrity check of mounted filesystems |
| `DecryptAndFeedPathToMd5` | `0x00004fe0` | Decrypt one path string, open the file, feed its contents to MD5 |
| `DecryptObfuscatedString` | `0x000054c0` | Decrypts an obfuscated string (path/key) |
| `FilterDirEntry` | `0x00005630` | Filters directory entries (hides files) |

#### `VerifyCodeIntegrityMd5` — checked file list (fully decrypted)

The path list is stored at ELF data address `0x70C0` as:
- `[0..3]` — seed `0x2D5D8428` (plaintext)
- `[4..5]` — encrypted path count (LE16), decrypted by subtracting `LCG(seed) & 0xFF` and `LCG(LCG(seed)) & 0xFF`
- `[6..]` — null-separated path strings, each byte decrypted by `plaintext = encrypted - (LCG_state & 0xFF) mod 256`

LCG: `state = state * 0xBB38435 + 0x3619636B` (same as OA.ko PCM key derivation).

Decrypted count: **32 entries**. Complete path list:

```
/           /dev        /var        /sbin
/sbin/losetup               /sbin/USBMidiAccessory.ko
/sbin/ifup.lite             /sbin/nologin
/sbin/vsftpd                /sbin/ifplugd.lite
/sbin/grub                  /sbin/avahi-autoipd
/sbin/avahi-daemon          /sbin/reboot
/sbin/arping                /sbin/dhclient-script
/sbin/hdparm                /sbin/UpdateRandomSeed.sh
/sbin/STGGmp.ko             /sbin/sysctl
/sbin/OmapNKS4Module.ko     /sbin/rmmod
/sbin/e2fsck                /sbin/ifplugd
/sbin/mkswap                /sbin/MIDID
/sbin/pidof                 /sbin/route
/sbin/swapon                /sbin/consoletype
/sbin/halt                  /sbin/
```

**`/korg/Mod/OA.ko` is absent.** OA.ko can be freely modified without failing this check.

### /proc update thread

| Function | Address | Notes |
|---|---|---|
| `KernelThreadMain` | `0x00004c40` | Main kernel thread loop |
| `CreateUpdateOsProcEntry` | `0x00005520` | Creates `/proc/.update` entry |
| `RemoveUpdateOsProcEntry` | `0x00005510` | Removes `/proc/.update` entry |
| `IncrementCounterAndWake` | `0x000039c0` | Increments counter and wakes the thread |
| `SecureWipeKeyBuffer` | `0x00003940` | Zeroes key material on wipe |
| `PageReadCallback` | `0x00003a30` | Callback for page read operations |

The kernel thread at `/proc/.update` polls for `/sbin/UpdateOS` or `/tmp/UpdateOS` and runs it as a usermode helper when found. This is the OS update mechanism.

### Synchronisation

| Function | Address | Notes |
|---|---|---|
| `AcquireLock` | `0x000038a0` | Spin lock acquire |
| `ReleaseLock` | `0x000038d0` | Spin lock release |

### Module entry points

| Function | Address | Notes |
|---|---|---|
| `init_module` | `0x000056c4` | Module init — runs all checks, installs hooks |
| `cleanup_module` | `0x00005560` | Module cleanup — removes hooks and proc entries |

---

## Key Global Variables / Kernel Symbols

| Symbol | Notes |
|---|---|
| `g_pCdromDrvInfo` | Pointer to cdrom driver info struct; magic value at `+5` |
| `sXCmd` / `sCdromCommand` | Pointers set after cdrom setup; both point to cdrom driver struct |
| `g_pSctSysMount` | Saved original `sys_mount` pointer |
| `g_pSctSysOldUmount` | Saved original `sys_oldumount` pointer |
| `sys_call_table` | Kernel syscall table (imported symbol, used for hook installation) |

---

## Modified Kernel Functions (GPL violation)

The Korg 2.6.32 kernel has two standard functions patched to return magic values:

| Function | Normal return | Korg magic return | Purpose |
|---|---|---|---|
| `register_cdrom()` | 0 on success | magic value | Detected by `RegisterFakeCdromDriver` |
| `init_cdrom_command()` | 0 | **-42** (`0xFFFFFFD6`) | Detected by OA.ko `InitCdromSupport` |

These are the "kernel check" — if the kernel does not return these magic values, loadmod.ko reports error code 3.

---

## Waitqueues in init_module

Three `__init_waitqueue_head()` calls in `init_module` initialise:
1. The `/proc/.update` kernel thread — sleeps between update polls, woken on unload
2. The stgNV2AC / dongle communication — blocks caller while waiting for hardware IC response
3. The mount hook serialisation — queues callers during the `/korg/*` mount interception checks

---

## Relationships with Other Modules

- **OmapNKS4Module.ko**: Provides `stgNV2AC_sync_cmd` and `stgNV2AC_sync_read_cmd` — the actual I²C/SPI communication with the Atmel security IC. loadmod.ko imports these.
- **OA.ko**: Reads the magic value loadmod.ko stored at `g_pCdromDrvInfo+5`. If absent, OA.ko degrades audio quality.
- **Korg kernel**: Provides patched `register_cdrom` and `init_cdrom_command` that return magic values.
