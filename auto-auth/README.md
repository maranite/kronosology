# auto-auth — Automatic EX Authorisation for Korg Kronos

Generates device-specific EX auth strings accepted by **stock, unmodified OA.ko**,
so future Korg OS updates keep working.  Works on any stock Kronos — no prior
rooting or SSH access required.

Algorithm documented in
[`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md).

---

## Two deployment modes

### Mode 1 — Zero-footprint USB authoriser (`build_auto_auth.py`)

Authorises all installed EX libraries **without writing anything to the Kronos
internal disk**.  The kernel module is loaded from the USB stick, generates the
auth strings, writes them to `/korg/rw/Startup/AuthorizationStrings`, then
unloads itself.  Nothing else changes.

Best for: a stock Kronos that has EX libraries already installed (or whose EX
files were copied via SSH) and just needs the auth strings populated.

```bash
python3 build_auto_auth.py
# → output/auto-auth/   copy contents to USB root, trigger OS update
```

Output directory:

```
output/auto-auth/
├── install.info           ← signed package metadata
├── auth.tar.gz            ← stamp payload (just drops /tmp/auth.stamp)
├── pretar.sh              ← pre-flight check
├── posttar.sh             ← loads ko, writes auth strings, rmmod
└── DisplayUpdaterMessage  ← on-screen progress binary (if built)
```

Copy to a FAT USB stick, insert into the Kronos, trigger **Global → OS Update**:

```sh
cp -r output/auto-auth/* /media/your-usb-stick/
sync
```

### Mode 2 — Direct install via SSH (`install.sh`)

If you already have root SSH access to the Kronos:

```bash
scp -r auto-auth/ root@192.168.0.3:/tmp/
ssh root@192.168.0.3 'sh /tmp/auto-auth/install.sh'
```

`install.sh` will:
1. `insmod oa_authgen.ko` → exposes `/proc/.oaauth`
2. `mv /sbin/InstallEXs /sbin/InstallEXs.real`
3. Copy the C wrapper to `/sbin/InstallEXs`

After this, every EX installation from the front panel automatically generates
and writes the correct auth string for that EX.

---

## How it works

When Eva installs an EX from the front panel it calls `/sbin/InstallEXs` via
`execve()`.  The stock binary copies the PCM bank and writes the option file
`/korg/rw/Options/Sxxx`, but does **not** generate an auth string.

The wrapper intercepts that call:

```
Eva
 └─ execve("/sbin/InstallEXs", ...)       ← replaced by our C wrapper
      └─ /sbin/InstallEXs.real "$@"       ← original binary, unchanged
      └─ write "GEN:Sxxx" → /proc/.oaauth   (oa_authgen.ko)
      └─ read 24-char auth string
      └─ write "AU:<authstring>" → /proc/.oacmd  (stock OA.ko)
           └─ VerifyAndSaveAuthString()   → validates, marks EX authorized,
                                            appends to AuthorizationStrings
```

The zero-footprint USB mode bypasses OA.ko entirely and writes directly to
`/korg/rw/Startup/AuthorizationStrings`, which OA.ko reads at the next normal boot.

### Why a kernel module?

The **chip secret** (24 bytes from Atmel NV2AC at addresses 0x10/0x18/0x20) is
the device-specific key.  `oa_authgen.ko` reads it from kernel space via
`stgNV2AC_sync_cmd` / `stgNV2AC_sync_read_cmd` (exported by `OmapNKS4Module.ko`)
— those functions are inaccessible from userspace.

### Native GPA — no OA.ko dependency

`oa_authgen.ko` implements the full **Group Authentication Protocol** (GPA)
natively in C, reverse-engineered from `OA_322.ko`.  It requires only
`stgNV2AC_sync_cmd` and `stgNV2AC_sync_read_cmd` from `OmapNKS4Module.ko`,
which are available during UpdateOS (where OA.ko is absent).

The generated auth string is:
- Device-specific (tied to this Kronos's Atmel chip secret)
- Option-file-specific (MD5 fingerprint of the option file is baked in)
- Identical in format to what Korg's own activation server produces

---

## Contents

| File / Directory | Purpose |
|---|---|
| `build_auto_auth.py` | Build script — produces the zero-footprint USB authoriser package |
| `install.sh` | Direct deployment script — run on the Kronos over SSH |
| `oa_authgen/oa_authgen.c` | Kernel module source — native GPA, chip read, `/proc/.oaauth` |
| `oa_authgen/Makefile` | Cross-compiles against Kronos 2.6.32 i386 kernel |
| `oa_authgen/oa_authgen.ko` | Pre-built module (Kronos 3.2.2 kernel) |
| `wrapper_c/installexs.c` | Source for the C `InstallEXs` wrapper |
| `wrapper_c/InstallEXs` | Pre-built wrapper binary (i386, no libc) |
| `wrapper_c/Makefile` | Build the wrapper (needs 32-bit gcc) |
| `InstallEXs` | Shell wrapper fallback (used by `install.sh` if C binary absent) |
| `updater_msg/kronosology_updater_msg.c` | Source for our `DisplayUpdaterMessage` replacement |
| `updater_msg/DisplayUpdaterMessage` | Pre-built display binary (i386, GLIBC 2.0) |
| `updater_msg/Makefile` | Build the display binary (needs `gcc-multilib`, `libfreetype6-dev:i386`) |

---

## Building

### `oa_authgen.ko`

Requires the Kronos kernel source tree (patched 2.6.32 from
[cgudrian/linux-kronos](https://github.com/cgudrian/linux-kronos)) and an
i386-capable compiler:

```bash
cd oa_authgen
make KDIR=/tmp/linux-kronos
# → oa_authgen.ko  (~24 KB)
```

> **Kernel tree prerequisite:** run `make ARCH=i386 oldconfig` and
> `make ARCH=i386 prepare scripts` in the kernel tree before building external
> modules against it.

### `wrapper_c/InstallEXs`

Requires a 32-bit-capable gcc (e.g. `gcc-multilib` on Debian/Ubuntu):

```bash
cd wrapper_c
make
# → InstallEXs  (i386 ELF, no libc dependency)
```

### `updater_msg/DisplayUpdaterMessage`

Requires `gcc-multilib` and `libfreetype6-dev:i386` (Debian/Ubuntu):

```bash
cd updater_msg
make
# → DisplayUpdaterMessage  (i386 ELF, GLIBC 2.0 only)
```

`build_auto_auth.py` invokes `make` automatically for any binary not already
present.  If `DisplayUpdaterMessage` is absent it logs a note but continues —
the USB package will simply show no on-screen progress messages.

---

## `/proc/.oaauth` command reference

| Write | Read result | Effect |
|---|---|---|
| `GEN:Sxxx` | 24-char auth string | Generate auth string for option file `Sxxx` |
| `GENDBG:Sxxx` | `<chip_hex>:<auth_string>` | Same, but also returns the chip bytes used |
| `VERIFY:<auth>` | `rc=N opt=Sxxx` | Call `VerifyAuthorizationString` directly; rc=0 = valid |
| `DECODE:<auth>` | 30-char hex (15 bytes) | Call `DecodeBytesFromAscii` directly |
| `CHIP` | 48-char hex (24 bytes) | Read chip bytes via authenticated zone read |
| `DBG` | 48-char hex (24 bytes) | Call `SetupAtmelForAuthorizations`, then read chip bytes |

Errors are logged to the kernel ring buffer (`dmesg`).

---

## Power-cycle warning

After using the USB authoriser, **power-cycle** the Kronos (full power-off ≥ 60 s,
then on).  Do not use the front-panel soft-reboot — it can wedge the
OmapNKS4 panel chip.  See
[`../docs/modules/OmapNKS4Module.ko_chip_wedge.md`](../docs/modules/OmapNKS4Module.ko_chip_wedge.md).
