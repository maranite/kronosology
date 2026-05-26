# loadmod.ko MD5 integrity-check file list

`loadmod.ko`'s `VerifyCodeIntegrityMd5` (check 1, error code 1) hashes file *paths* and *contents* for 446 paths embedded as an LCG-encrypted list at .data offset `eeeeeeeea25 @ 0x70a0`.

**Why:** Determines which files cannot be modified without breaking the integrity check. Knowing the list lets you keep stock loadmod working while patching files NOT in the list.

**How to apply:** If you want to deploy patched versions of files in this list, you must also deploy `loadmod-patches` (the MD5+dongle bypass) — stock loadmod will reject them with `status=1`. Files NOT in the list can be freely modified.

## In the MD5 check (loadmod will reject if modified):
- `/sbin/loadmod.ko` — recursive; check hashes itself
- `/sbin/loadoa`
- `/sbin/USBMidiAccessory.ko` — must be the V1 stock `/sbin/` variant (md5 `fae9ff96…`, 102741 bytes), **not** the `/korg/Mod/` variant (md5 `e6b16f79…`, 102931 bytes)
- `/sbin/OmapNKS4Module.ko`
- `/sbin/STGEnabler.ko`, `/sbin/STGGmp.ko`, `/sbin/OmapVideoModule.ko`, `/sbin/GetPubIdMod.ko`, `/sbin/InstallEXs`, `/sbin/UpdateOS`, `/sbin/MIDID`
- many `/sbin/` busybox-style utilities (`losetup`, `ifup.lite`, `vsftpd`, `grub`, `init`, `insmod`, `rmmod`, `reboot`, etc.)
- all `/lib/lib*.so.*`, `/lib/security/pam_*.so`, `/lib/modules/.../kernel/...`
- all `/bin/*` utilities (`bash`, `mount`, `sh`, `ShowReauthScreen`, `fanctrld`, `md5sum`, etc.)
- many `/etc/pam.d/*`, `/etc/dbus-1/system.d/*`, `/etc/avahi/...`
- `/usr/realtime/modules/rtai_*.ko`
- directory entries (`/`, `/dev` *(skipped contents)*, `/var`, `/sbin`, `/lib`, `/bin`, `/proc`, `/sys`, `/boot`, `/mnt`, `/etc`, `/tmp`, `/usr`, `/korg`, `/korg/ftp`, `/korg/rw`, `/korg/Eva`, `/korg/ro`, `/korg/Mod`, `/korg/rw2`)

## NOT in the MD5 check (safe to modify with stock loadmod):
- `/sbin/OA.ko` ← key insight; patched OA can sit at `/sbin/OA.ko` without breaking stock loadmod
- `/sbin/KorgUsbAudioDriver.ko` ← not in `/sbin/` originally, only in `/korg/Mod/`
- `/etc/OA.rc`, `/etc/OA.clonos.rc`, `/etc/inittab.busybox`, `/etc/OA.si` — safe to edit for debug logging
- Anything in `/korg/rw/*` or `/korg/Eva/*` or `/korg/Mod/*` (the cryptoloop content is checked via a different mechanism)
- `/dev/*` (paths starting with `/dev` are explicitly skipped during MD5 traversal — see `DecryptAndFeedPathToMd5` at 0x4fe0)

## How to decode the path list yourself

```python
# read 10000 bytes from Ghidra at address 0x70a0 in loadmod.ko
LCG_MUL = 0x0BB38435
LCG_ADD = 0x3619636B
MASK = 0xFFFFFFFF
seed = struct.unpack('<I', data[0:4])[0]
state = (seed * LCG_MUL + LCG_ADD) & MASK  # first iteration
state = (state * LCG_MUL + LCG_ADD) & MASK  # second iteration
# count encoded as 2 bytes XOR'd with state low bytes from those 2 iterations
# then iterate per-character LCG to decrypt null-terminated strings
```
