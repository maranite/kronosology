# oa_authgen

Kernel module to communicate with the ATMEL security chip and calculate 
authorization strings for EXs options.

Exposes `/proc/.oaauth` so that userland programs can verify and generate authorization keys. 

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


## Building

Requires the Kronos kernel source tree (patched 2.6.32 from
[cgudrian/linux-kronos](https://github.com/cgudrian/linux-kronos)) and an
i386-capable compiler:

```bash
cd auto-auth/oa_authgen
make KDIR=/tmp/linux-kronos        # host must be i386/i686, or have 32-bit multilib
# Produces: oa_authgen.ko  (~24 KB)
```

> **Kernel tree prerequisite:** run `make ARCH=i386 oldconfig` and
> `make ARCH=i386 prepare scripts` in the kernel tree once before building
> external modules against it.

## Command reference

Interaction with this module is achieved by writing to then reading from `/proc/.oaauth` 

| Write | Read result | Effect |
|---|---|---|
| `GEN:Sxxx` | 24-char auth string | Generate auth string for option file `Sxxx` |
| `GENDBG:Sxxx` | `<chip_hex>:<auth_string>` | Same, but also returns the chip bytes used |
| `VERIFY:<auth>` | `rc=N opt=Sxxx` | Call `VerifyAuthorizationString` directly; rc=0 = valid |
| `DECODE:<auth>` | 30-char hex (15 bytes) | Call `DecodeBytesFromAscii` directly |
| `CHIP` | 48-char hex (24 bytes) | Read chip bytes via authenticated zone read |
| `DBG` | 48-char hex (24 bytes) | Call `SetupAtmelForAuthorizations`, then read chip bytes |

Errors are logged to the kernel ring buffer (`dmesg`).


Algorithm documented in
[`../docs/crypto/auth_string_algorithm.md`](../docs/crypto/auth_string_algorithm.md).

