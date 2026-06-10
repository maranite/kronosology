# InstallEXs wrapper

A minimal i386 C binary that replaces `/sbin/InstallEXs` on the Kronos.  When
Eva installs an EX library from the front panel it calls `/sbin/InstallEXs`.
This wrapper:

1. Runs `/sbin/InstallEXs.real "$@"` — the stock binary, unchanged
2. After it exits, looks up the option file ID for the newly-installed EX
3. Writes `GEN:<id>` to `/proc/.oaauth` (exposed by `oa_authgen.ko`) to generate
   the device-specific auth string
4. Appends the auth string to `/korg/rw/Startup/AuthorizationStrings`

This happens automatically, with no user interaction, every time an EX is
installed.

---

## Building

Requires a 32-bit-capable C compiler (`gcc-multilib` on Debian/Ubuntu):

```bash
sudo apt install gcc-multilib    # one-time
cd offline-patcher/InstallEXs
make
# → InstallEXs  (i386 ELF, no libc dependency, stripped, ~10 KB)
```

The `patch_firmware_offline.py` build script will attempt `make` automatically
if the binary is absent when you run it.

---

## Deployment

`patch_firmware_offline.py` includes this binary in the USB update package at
`sbin/InstallEXs`.  The `pretar.sh` script renames the stock
`/sbin/InstallEXs` to `/sbin/InstallEXs.real` before the tar extracts, so the
wrapper lands in place without overwriting the original.

---

## Prerequisites

`oa_authgen.ko` must be loaded at boot for `/proc/.oaauth` to exist.  Install
the **auto-auth** USB package first (or alongside this one) to ensure the module
is loaded on every boot.
