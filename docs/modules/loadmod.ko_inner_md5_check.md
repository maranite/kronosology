# loadmod.ko — the second MD5 check inside RetrieveSecurityICKey

`loadmod.ko` has **TWO** places that verify the MD5 of the encrypted-path file list against baked-in constants `aaaaaaaaa5 / DAT_00006c81..DAT_00006c8f` (16 bytes at .data offset 0x6c80 in Ghidra):

1. **`VerifyCodeIntegrityMd5` @ ghidra 0x5350** (init_module check 1, error code 1) — easy to bypass: NOP the test+jne after the call at init.text+0x39.
2. **`RetrieveSecurityICKey` @ ghidra 0x3e10** (init_module check 4 dongle path) — does the SAME 16 comparisons internally at ghidra 0x3f72-0x40a2 before calling `BuildCdromCommandStruct`. If they fail, returns non-zero and `BuildCdromCommandStruct` is NEVER called.

**Why this matters:** `BuildCdromCommandStruct` populates the global state that loadmod's `HookedSysMount` → `MountLoopDevAndExec` uses for the cryptoloop key. If it isn't called, cryptoloop mounts fail with `VFS: Can't find an ext2 filesystem on dev loopN` (because the loop device gets bound with garbage key, decryption produces invalid superblock).

**How to apply:** Both bypasses are mandatory when patching any file in `loadmod-md5-check-files`. Without the inner one, `/tmp/stgStatus = 0` lies — init "succeeds" but cryptoloop mounts will fail later.

**The patches (file offsets in loadmod.ko, mapped via `file_offset = ghidra_addr + 0x30` for .text section):**

| # | File offset | Original | Replacement | Purpose |
|---|---|---|---|---|
| 1 | 0x572d (8 bytes) | `85 c0 0f 85 a3 00 00 00` | `90 × 8` | NOP test+jne after `VerifyCodeIntegrityMd5` (init check 1) |
| 2 | 0x57b1 (2 bytes) | `75 47` | `90 × 2` | NOP JNE after `RetrieveSecurityICKey` (init check 4) |
| 3 | 0x3fb0 (6 bytes) | `0f 85 e7 fe ff ff` | `e9 1e 01 00 00 90` | Replace first JNE inside `RetrieveSecurityICKey` with `JMP +0x11e` past all 16 byte-comparisons — lands at ghidra 0x40a3 = start of success path (`GetRandomBytesWrapper(pbVar9, 8)`) |

Bypassing #3 lets RetrieveSecurityICKey fall through to its Atmel-chip / RSA / SetModeAuthenticate / SetModeDecrypt / BlowfishExpandKey / Md5InitState / init_cdrom_command magic chain, which all succeed naturally when the chip is responsive. At the end it calls `BuildCdromCommandStruct()` which populates the cryptoloop key globals (`bbbbbbbba1555.25922`, `DAT_0001aba4`, etc.).

**Resulting patched md5:** `28d1cec16f1d893f1d78241b62a989d9` (file size unchanged: 52384 bytes).

Verified live on the Kronos 2026-05-26: rmmod loadmod → insmod patched → mount /korg/Mod and /korg/Eva both succeed → all USB modules load → patched OA installs → Eva runs as pocky user with patched OA active.
