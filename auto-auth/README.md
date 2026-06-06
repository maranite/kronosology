# auto-auth — Automatic EX Authorisation for Korg Kronos

Installs any EX expansion on any Kronos with a legitimate auth string,
accepted by **stock, unmodified OA.ko**, so future Korg OS updates keep working.

Full algorithm documented in
[`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md).

---

## How it works

When Eva installs an EX from the front panel it calls `/sbin/InstallEXs` via
`execve()`.  The stock binary copies the PCM bank and writes the option file
`/korg/rw/Options/Sxxx`, but it does **not** generate an auth string — that was
historically done server-side by Korg.

This package intercepts that call at the OS level:

```
Eva
 └─ execve("/sbin/InstallEXs", ...)      ← replaced by wrapper shell script
      └─ /sbin/InstallEXs.real "$@"      ← original binary, unchanged
      └─ write "GEN:Sxxx" → /proc/.oaauth  (oa_authgen.ko)
      └─ read 24-char auth string
      └─ write "AU:<authstring>" → /proc/.oacmd  (stock OA.ko)
           └─ VerifyAndSaveAuthString()  → validates, marks EX authorized,
                                           appends to AuthorizationStrings
```

The **chip secret** (24 bytes from Atmel NV2AC at addresses 0x10/0x18/0x20)
is the device-specific key.  `oa_authgen.ko` reads it from kernel space via
`stgNV2AC_sync_read_cmd` (exported by `OmapNKS4Module.ko`) — that function is
never accessible from userspace, which is why a small kernel module is needed.

The generated auth string is:
- Device-specific (tied to this Kronos's chip secret)
- Option-file-specific (MD5 fingerprint of the option file content is baked in)
- Identical in format to what Korg's own activation server would produce

---

## Contents

| File | Purpose |
|---|---|
| `oa_authgen/oa_authgen.c` | Kernel module source — reads chip secret, computes auth string, exposes `/proc/.oaauth` |
| `oa_authgen/Makefile` | Cross-compiles against the Kronos 2.6.32 i386 kernel source |
| `InstallEXs` | Wrapper shell script (replaces `/sbin/InstallEXs` on device) |
| `install.sh` | Deployment script — run on the Kronos to activate everything |
| `auth_gen.py` | Python reference implementation for host-side testing / verification |

---

## Building `oa_authgen.ko`

You need the Kronos kernel source tree (the patched 2.6.32 from
[cgudrian/linux-kronos](https://github.com/cgudrian/linux-kronos), branch per
the notes at issue #1) and an i386-capable compiler.

```bash
# On a Linux host with an i686-linux-gnu toolchain:
cd auto-auth/oa_authgen
make KDIR=/mnt/source/Kronos/linux-kronos CROSS_COMPILE=i686-linux-gnu-
# Produces: oa_authgen.ko
```

If your host is already i386/i686 (or you have a 32-bit multilib), omit
`CROSS_COMPILE`.

> **Note on kernel build prerequisites:**  The linux-kronos tree needs to be
> configured and have its header dependencies generated before external modules
> can be built against it.  Run `make ARCH=i386 oldconfig` and
> `make ARCH=i386 prepare scripts` in the kernel tree once.

---

## Deployment (on the Kronos)

Copy the entire `auto-auth` folder to the Kronos (USB stick or `scp`), then:

```bash
# On the Kronos, as root:
cd /path/to/auto-auth
sh install.sh
```

`install.sh` will:
1. `insmod oa_authgen.ko` → creates `/proc/.oaauth`
2. `mv /sbin/InstallEXs /sbin/InstallEXs.real`
3. Copy the wrapper script to `/sbin/InstallEXs`

To survive reboots, add to `/etc/rc.local` (or the relevant init script):
```bash
insmod /sbin/oa_authgen.ko
```

---

## Testing

### Host-side (Python, no Kronos needed)

```bash
pip install pycryptodome
python3 auth_gen.py selftest
```

To generate an auth string from a captured chip secret:
```bash
python3 auth_gen.py gen S285 /path/to/S285 --chip-secret <48-hex-chars>
```

To verify an existing auth string from `AuthorizationStrings`:
```bash
python3 auth_gen.py verify <24-char-string> \
    --chip-secret <48-hex-chars> \
    --option-id S285 \
    --option-file /path/to/S285
```

### On the Kronos (manual smoke test)

After running `install.sh`:

```bash
# Confirm the proc file exists
ls -la /proc/.oaauth

# Test generation for an already-installed option file, e.g. S23 (factory piano)
printf 'GEN:S023' > /proc/.oaauth
cat /proc/.oaauth       # should print 24 chars

# Submit it manually
auth=$(cat /proc/.oaauth)
printf 'AU:%s' "$auth" > /proc/.oacmd

# Check that the auth string appeared in the file
grep "$auth" /korg/rw/Startup/AuthorizationStrings
```

---

## `/proc/.oaauth` protocol

| Write | Read | Effect |
|---|---|---|
| `GEN:Sxxx` | — | Compute auth string for option file `Sxxx`; store result internally |
| — | 24 bytes | Return last successfully computed auth string |

Errors are logged to the kernel ring buffer (`dmesg`).  If the NV2AC chip is
not responding (panel-chip wedge symptom), the write will fail with an IO
error — power-cycle the Kronos (60 s unplugged) to reset the chip.

---

## Bundling into a Korg update package

Once tested, these files can be bundled into a Korg-format update package so
that `UpdateOS` installs everything automatically.  The update package format
is documented in
[`../docs/modules/UpdateOS.md`](../docs/modules/UpdateOS.md) and
[`../docs/interfaces/file_formats.md`](../docs/interfaces/file_formats.md).
The key constraint is that all files added to the update must appear in the
`install.info` manifest with correct MD5 hashes, signed with the
`UpdaterScriptsKey`.

The rough plan:
- Add `oa_authgen.ko` and the `InstallEXs` wrapper to the update's file list
- Add an `install.sh`-equivalent to the post-install script block
- Sign the manifest and package as a standard OS update

See [`../update-builder/`](../update-builder/) for tooling that already
handles the signing step.

---

## Threat model

The auth string is device-specific: it is only valid on the Kronos whose chip
secret was used to generate it.  Anyone with access to a device's chip secret
can install any EX on that device, but **cannot forge auth strings for other
devices**.  This matches Korg's apparent design intent (the device is the
locked unit, not the user account).

See [`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md)
for the full threat model discussion.
