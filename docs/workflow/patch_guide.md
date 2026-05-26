# OA.ko Patch Guide

Practical patching reference.

**Address conventions for OA.ko (ET_REL, all section VMAs = 0):**
- **Ghidra address** = ELF symbol value = nm value = `.text` section-relative offset  
- **File byte offset** = Ghidra address + 0xb390 (the `.text` section's file position)  
- `readelf -S OA.ko | grep " .text "` → confirms `sh_offset = 0xb390`

All `file off` values in this guide are ready to pass to `dd seek=` or `xxd -s`.

---

## Patch 1 — Skip loadmod.ko Magic Value Check

**Problem:** `CSTGEngine::Initialize` calls `InitCdromSupport()`. If it returns non-zero (loadmod.ko not loaded, or magic value not present), a degradation block sets `allPlusOne=0.7f`, `allMinusOne=-0.2f`, and `kAudXBZD=0x1f` — causing cyclic volume fade across all sample banks.

**Location:** `CSTGEngine::Initialize`, Ghidra `0x08B6` (file off = `0x08B6 + 0xb390 = 0xBC46`)

```
Function:   _ZN10CSTGEngine10InitializeEv  (Ghidra 0x1b0)
Ghidra addr:    0x08B6
File offset:    0x0BC46
Runtime addr:   0x59CE6000 + 0x8B6 = 0x59CE68B6
```

| File offset | Original bytes | Patched bytes | Instruction change |
|---|---|---|---|
| `0x0BC46` | `74 5A` | `EB 5A` | `JE +90` → `JMP +90` |

This unconditionally jumps over the entire degradation block. The `allPlus/Minus` globals and `kAudXBZD` are never written; audio initialises with correct coefficients.

**Alternative — stub InitCdromSupport itself:**

| File offset | Original bytes | Patched bytes | Effect |
|---|---|---|---|
| Ghidra `0x0040` (file `0x0B3D0`) | `55 31 C0 B9 15 ...` | `31 C0 C3 90 90 ...` | `XOR EAX,EAX; RET` — always returns 0 |

---

## Patch 2 — Authorise All Multisample Banks

**Problem:** `IsAuthorizedMultisampleBank` returns 0 for any bank whose UUID+index checksum does not match the stored value. Banks not listed in `/korg/rw/Startup/AuthorizationStrings` are treated as unauthorised. Affected voices receive a cyclic fade/attenuation instead of normal audio output.

**Location:** `IsAuthorizedMultisampleBank`, Ghidra `0x2E650` (file off = `0x2E650 + 0xb390 = 0x399E0`)

```
Function:   _ZNK14CSTGKLMManager27IsAuthorizedMultisampleBankEPK19CSTGMultisampleBank
Ghidra addr:    0x02E650
File offset:    0x0399E0
Runtime addr:   0x59CE6000 + 0x2E650 = 0x5A014650
```

| File offset | Original bytes | Patched bytes | Effect |
|---|---|---|---|
| `0x0399E0` | `F6 42 5C 08 B9 01` | `B8 01 00 00 00 C3` | `MOV EAX, 1; RET` — always returns 1 (authorised) |

This is the most thorough option — all callers of `IsAuthorizedMultisampleBank` see authorised for every bank, including the oscillator authorization queries and any UI display of bank auth state.

**Alternative — suppress per-patch unauthorized flag only:**

`CSTGPCMModelPatch::IsUsingAnyUnauthorizedMultisamples` lives in a separate COMDAT ELF section (`.text._ZN17CSTGPCMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv`). To find its file offset:

```bash
readelf -S OA.ko | grep CSTGPCMModelPatch34
# Note the Offset column; that is the file byte offset directly (VMA=0 in each COMDAT section)
```

| Original bytes | Patched bytes | Effect |
|---|---|---|
| `8B 92 D0 01 00 00 85 D2 0F 95 C0` | `31 C0 C3 90 90 90 90 90 90 90 90` | `XOR EAX,EAX; RET` — always returns false (no unauthorized samples) |

Patch 2 (IsAuthorizedMultisampleBank) is preferred — it fixes all callers.

---

## Applying Both Patches

Both patches are independent and safe to apply together. File offsets are fixed constants for OA.ko v3.2.1 (MD5 `955636c2b11a70a1dbecefaaa7bd4f80`). Always work on a copy first.

```bash
cp OA.ko OA.ko.orig

# Patch 1: JE → JMP at file offset 0xBC46 (skips InitCdromSupport degradation)
printf '\xEB' | dd of=OA.ko bs=1 seek=$((0xBC46)) conv=notrunc

# Patch 2: IsAuthorizedMultisampleBank → MOV EAX,1; RET at file offset 0x399E0
printf '\xB8\x01\x00\x00\x00\xC3' | dd of=OA.ko bs=1 seek=$((0x399E0)) conv=notrunc
```

**Verify with:**
```bash
# Patch 1 — expect: eb 5a
xxd -s $((0xBC46)) -l 2 OA.ko

# Patch 2 — expect: b8 01 00 00 00 c3
xxd -s $((0x399E0)) -l 6 OA.ko
```

**Patched binary MD5:** `a8751b31df68b580435d3fb25bfbe74c`

The patched binary lives at `/tmp/OA.ko.patched` on this machine.

---

## Deploying to the Kronos

The Kronos must be rooted (dropbear running). `/korg/Mod` is an encrypted loop filesystem mounted by loadmod.ko's kernel hook. The hook re-triggers when you call `mount` on that path, so you can mount it from an SSH session.

### Method A — Direct SCP via live SSH session

```bash
# 1. On this machine: SCP patched binary to Kronos /tmp
scp /tmp/OA.ko.patched root@<KRONOS_IP>:/tmp/OA.ko.patched

# 2. On the Kronos (SSH in):
# Trigger loadmod.ko's mount hook to mount /korg/Mod read-write
mount /korg/Mod   # the hook intercepts this and sets up the loop device

# 3. Backup original
cp /korg/Mod/OA.ko /korg/Mod/OA.ko.orig
md5sum /korg/Mod/OA.ko   # should be 955636c2b11a70a1dbecefaaa7bd4f80

# 4. Install patch
cp /tmp/OA.ko.patched /korg/Mod/OA.ko
md5sum /korg/Mod/OA.ko   # should be a8751b31df68b580435d3fb25bfbe74c

# 5. Unmount and reboot
umount /korg/Mod
reboot
```

### Fallback procedure

If the Kronos fails to boot (OA.ko insmod fails → loadoa exits → Eva never starts but dropbear should still be up):

```bash
# SSH in immediately after powerup (dropbear starts before loadoa)
ssh root@<KRONOS_IP>

# Remount /korg/Mod and restore original
mount /korg/Mod
cp /korg/Mod/OA.ko.orig /korg/Mod/OA.ko
umount /korg/Mod
reboot
```

**Boot sequence confirmed from `/etc/inittab.busybox`** (rooting kit):
```
::sysinit:/etc/OA.clonos.si           (hardware setup, loads loadmod.ko etc.)
:3:wait:/etc/OA.clonos.rc start       (runs loadoa, insmod OA.ko — waits for exit)
:3:respawn:/bin/dropbear -F -R        (starts AFTER rc start exits)
```

If OA.ko fails to insmod, `loadoa` calls `Fail()` and exits → `OA.clonos.rc start` exits → dropbear starts → SSH is available for recovery. The patches (a branch flip and a stub function) are too conservative to cause a kernel panic — worst case is OA.ko module init fails with an error.

### Method B — Custom OS Update Package

If `/korg/Mod` cannot be mounted from userspace (mount hook only triggers during loadoa's boot sequence), create a minimal update package using the structure from `kronos_rooting/`:

```
update/
├── install.info         (VERSION, SOURCE, PRETARSCRIPT, POSTTARSCRIPT, SIGNATURE)
├── pretar.sh            (verify package integrity)
├── posttar.sh           (cp OA.ko.patched /korg/Mod/OA.ko)
├── update.tar.gz        (contains OA.ko.patched)
└── update.tar.gz.csum   (MD5 of the files in the tar)
```

The SIGNATURE in `install.info` is the SHA1 of the tar.gz. `posttar.sh` does NOT need to do the standard `.csum` check — it can simply copy the file and verify. Since OA.ko is not in `KRONOS_Updater_3_2_1/KRONOS_Update_3_2_1.csum`, a custom posttar.sh that skips that verification is fine.

---

## Patch Verification

After applying, confirm in Ghidra or objdump:

**Patch 1** — `CSTGEngine::Initialize` tail should show:
```asm
call   InitCdromSupport
test   %eax, %eax
jmp    <return>          ← was: je <return>
; degradation block should be unreachable
```

**Patch 2** — `IsAuthorizedMultisampleBank` should show:
```asm
mov    $0x1, %eax
ret
```

---

## What is NOT patched

These are explicitly left alone:

| Thing | Reason |
|---|---|
| `InitCdromSupport` itself (if using Patch 1A) | Not needed — the branch in `CSTGEngine::Initialize` is sufficient |
| `CSTGPCMDecrypter::Decrypt` | AES decryption of PCM data — separate from auth; must remain for sample data to decode correctly |
| `ParseAuths` / `ParseAuth` | File reading still works fine; authorising via file still works alongside the patch |
| `AuthorizeBuiltins` | Correctly authorises factory ROM banks; no reason to touch |
| `SetupAtmelForAuthorizations` | Only needed for runtime front-panel auth string entry; not called at boot |
| `VerifyAuthorizationString` | Only needed for runtime auth; boot-time auth path is unaffected |
| loadmod.ko syscall hooks | Mount interception is independent of the audio degradation; patching OA.ko is sufficient |

---

## Ghidra Addresses (for navigation)

With Ghidra image base set to `0x59CE6000`. **Note:** ELF VMA (from objdump) + base = Ghidra address.

| Patch | Function | ELF VMA | File offset | Ghidra addr (base+VMA) | Runtime addr |
|---|---|---|---|---|---|
| 1 | `CSTGEngine::Initialize` degradation branch | `0x008B6` | `0x0BC46` | `0x59CE68B6` | `0x59CE68B6` |
| 1 alt | `InitCdromSupport` | `0x00040` | `0x0BBD0` | `0x59CE6040` | `0x59CE6040` |
| 2 | `IsAuthorizedMultisampleBank` | `0x02E650` | `0x399E0` | `0x59D14650` | `0x59D14650` |
| 2 alt | `CSTGPCMModelPatch::IsUsingAnyUnauthorizedMultisamples` | COMDAT (see readelf) | — | — | — |

---

## Related Functions (for further analysis)

| Function | .text offset | Notes |
|---|---|---|
| `CSTGEngine::Initialize` | `0x000101B0` | Top-level init; contains Patch 1 branch |
| `InitCdromSupport` | `0x00010040` | Magic value check |
| `CSTGKLMManager::AuthorizeMultisampleBank` | `0x0003E200` | Stores auth checksum in bank struct |
| `CSTGKLMManager::AuthorizeBuiltins` | `0x0003E350` | Pre-authorises factory banks at boot |
| `CSTGInstalledEXProducts::Initialize` | `0x00058620` | Reads AuthorizationStrings file |
| `ParseAuths` | `0x00217C50` | Tokenises and processes auth file |
| `ParseAuth` | `0x00217890` | Processes one auth entry; verifies MD5 |
| `VerifyAuthorizationString` | `0x00217DE0` | Runtime auth (requires dongle) |
| `SetupAtmelForAuthorizations` | `0x00217A50` | Atmel IC init for auth ops |
| `CSTGPCMModel::ProcessAudioRate` | `0x001AB560` | PCM voice audio rate processing |
| `CSTGPCMDecrypter::Decrypt` | `0x0003F4B0` | AES decryption of PCM sample data |

---

## Permanently Removing the Cryptoloop (Optional)

By default, `/korg/Mod` is an AES-encrypted loop filesystem. `loadmod.ko` hooks `sys_mount` and uses the key from `pairFact` (decrypted by the stgNV2AC hardware IC) to set up the loop encryption every boot. This is inconvenient if you need to modify `/korg/Mod/OA.ko` repeatedly.

This procedure replaces the encrypted image with a plain ext2 image and patches `loadmod.ko` to skip the `LOOP_SET_STATUS` ioctl, making `/korg/Mod` a normal unencrypted loop mount thereafter.

**Prerequisites:** Kronos booted, SSH (dropbear) running, rooted.

### Step 1 — Extract the decrypted image (live on Kronos)

```bash
# SSH into running Kronos
ssh root@<KRONOS_IP>

# Trigger the hook to mount /korg/Mod
mount /korg/Mod

# Find the loop device (e.g. /dev/loop2)
cat /proc/mounts | grep Mod
# or: losetup -a

# dd the decrypted block device to the rw partition (17 MB, ~30 sec)
dd if=/dev/loop2 of=/korg/rw/Mod_plain.img bs=4096 conv=sync status=progress

# Unmount
umount /korg/Mod
```

### Step 2 — Patch OA.ko into the plain image (on Kronos)

```bash
# Mount the plain image as a plain loop
losetup /dev/loop3 /korg/rw/Mod_plain.img
mount -t ext2 /dev/loop3 /mnt

# Copy the patched OA.ko (transfer via SCP first if not already on device)
cp /tmp/OA.ko.patched /mnt/OA.ko
md5sum /mnt/OA.ko   # should be a8751b31df68b580435d3fb25bfbe74c

umount /mnt
losetup -d /dev/loop3
```

### Step 3 — Patch loadmod.ko to skip AES setup

`loadmod.ko` lives at `/sbin/loadmod.ko` on the plain rootfs — it is NOT inside the encrypted image. The `VerifyCodeIntegrityMd5` check in loadmod.ko does NOT check `/sbin/loadmod.ko`, so it can be patched freely.

**What to patch:** In `MountLoopDevAndExec` at `.text` offset `0x4942`, the `LOOP_SET_STATUS` ioctl call (`call *0x18(%esp)`) must be replaced with `XOR EAX,EAX; NOP; NOP` so the loop device is attached to the image file without AES being configured.

```bash
# Verify original bytes at file offset 0x49a2 (= 0x60 + 0x4942)
xxd -s $((0x49a2)) -l 4 /sbin/loadmod.ko
# Expected: ff 54 24 18

# Apply patch (skips LOOP_SET_STATUS, fakes success)
cp /sbin/loadmod.ko /sbin/loadmod.ko.orig
printf '\x31\xc0\x90\x90' | dd of=/sbin/loadmod.ko bs=1 seek=$((0x49a2)) conv=notrunc

# Verify
xxd -s $((0x49a2)) -l 4 /sbin/loadmod.ko
# Expected: 31 c0 90 90
```

### Step 4 — Replace the encrypted image with the plain one

```bash
# Move the plain image to replace the encrypted one
cp /korg/rw/Mod_plain.img /korg/ro/Mod.img
```

**Note:** `/korg/ro` is mounted read-only (`mount -t ext2 -o ro /dev/sda5 /korg/ro`).
To write to it: `mount -o remount,rw /korg/ro`, then copy, then `mount -o remount,ro /korg/ro`.

```bash
mount -o remount,rw /korg/ro
cp /korg/rw/Mod_plain.img /korg/ro/Mod.img
mount -o remount,ro /korg/ro
rm /korg/rw/Mod_plain.img    # free space on rw partition
```

### Step 5 — Reboot and verify

```bash
reboot
# After reboot, SSH in:
ssh root@<KRONOS_IP>
mount /korg/Mod
ls /korg/Mod/OA.ko   # should exist
md5sum /korg/Mod/OA.ko   # should be a8751b31df68b580435d3fb25bfbe74c
```

**Fallback:** If boot fails, the original loadmod.ko is at `/sbin/loadmod.ko.orig` and can be restored via the same update package mechanism. The encrypted `Mod.img` backup is NOT kept by default — take a backup before Step 4 if you want one.

### Alternative: Use an update package for Steps 3–4

If you prefer not to operate directly on the live filesystem, create an update package (see Method B in the Deploying section above) with a `posttar.sh` that:

```bash
# posttar.sh
cp /tmp/Mod_plain.img /korg/ro/Mod.img
printf '\x31\xc0\x90\x90' | dd of=/sbin/loadmod.ko bs=1 seek=$((0x49a2)) conv=notrunc
```

Pack `Mod_plain.img` into `update.tar.gz` and set the SIGNATURE to the SHA1 of that tar.gz.

### Technical details

- Encrypted images: `/korg/ro/{Mod,Eva,WaveMotion}.img` (17 MB, 125 MB, 236 MB)
- This procedure only removes encryption for `/korg/Mod`. `Eva.img` and `WaveMotion.img` remain encrypted (they are not needed for patching).
- The `LOOP_SET_STATUS` ioctl (`0x4C04`) at `.text:0x4942` / `file:0x49A2` in loadmod.ko is the sole source of AES encryption setup for the loop device.
- After the patch, `MountLoopDevAndExec` calls `LOOP_SET_FD` (attaches image) then immediately calls the original `sys_mount` without configuring any encryption. The plain ext2 image mounts correctly.
- `VerifyCodeIntegrityMd5` in loadmod.ko does not check `/sbin/loadmod.ko` itself, nor `/korg/ro/Mod.img`, so both patches evade the integrity check.
