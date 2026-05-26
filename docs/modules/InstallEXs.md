# InstallEXs — EX Expansion Installer

The userspace binary that installs a Korg EX (Expansion) into the Kronos: copies the
PCM bank files to `/korg/rw/PCM/Bank<NN>` and the per-bank "option file" to
`/korg/rw/Options/Sxxx`. The Kronos GUI launches it when an EX install USB is inserted.

| Property | Value |
|---|---|
| Path on device | `/sbin/InstallEXs` |
| Source path | `dump from kronos/sbin/InstallEXs` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |
| Size | ~138 KB |
| Functions | 210 (Ghidra) / 59 nm-defined |
| C++ mangled symbols | 52 |
| Compiler | GCC 4.5.0 |
| Source path leaked | `/home/pocky/Development/GitHub/KRONOS/Main/OPOS/Projects/x2100/Tools/InstallEXsApp` |
| `UpdaterScriptsKey` | **Same 16 bytes as UpdateOS**, located at file offset `0xa610` |

---

## Where the secret lives

InstallEXs hard-codes the same `UpdaterScriptsKey` as UpdateOS (shared `UpdaterScriptsKey.h`
in the original source tree under `STG/platformRT/CopyProtKeys`). This means anyone with
the key can produce a valid `.exsins` package for any EX, on any Kronos.

```
Key (16 bytes) — file offset 0xa610 in InstallEXs:
  13 d0 af ef e0 3c 9b 92 16 2f ae ff 77 53 55 e1
```

---

## Files involved at install time

| File on USB | Role |
|---|---|
| `EXsInstall.exsins` | Manifest. KEY=VALUE format with at minimum: `OPTIONFILE`, `EXSNUM`, `SIGNATURE` |
| `<OPTIONFILE>` | Plain-text EX metadata (4–5 lines: `EXs<num>\n<friendly name>\n<num>\n<type>,<id>,<full name>\n`). Gets MD5-fingerprinted into the auth string |
| PCM bank file(s) | Copied to `/korg/rw/PCM/Bank<NN>` (and `/korg/rw2/PCM/...` on dual-drive units) |

---

## Functions of interest

| Symbol | Address | Role |
|---|---|---|
| `main` | `0x0804ef80` | Entry |
| `MountSource` | `0x0804b3e0` | Mounts the EX USB stick to `/mnt/updaterSource/` |
| `ReadInstallInfo` | `0x0804c9a0` | Parses `EXsInstall.exsins` |
| `ParseInfoLine` | `0x0804c340` | Generic `KEY=VALUE` parser |
| `VerifyOptionFileSignature` | `0x0804db70` | SHA-1 verification of the option file using `UpdaterScriptsKey` |
| `CopyOptionFile` | `0x0804d9a0` | Writes the verified option file to `/korg/rw/Options/Sxxx` |
| `DoFullInstallation` | `0x0804e070` | Orchestrates everything |
| `VerifyInstallationChecksum` | `0x0804adb0` | Post-install verify |
| `_GLOBAL__I_UpdaterScriptsKey` | `0x0804f8d0` | C++ static initialiser for the key constant |
| `CUUID::Generate` / `ConvertToText` / `ConvertFromText` | `0x0804f930` / `0x0804f940` / `0x0804fb50` | UUID handling (for KApro-style 3rd-party EXs) |

Globals: `gOptionFileName`, `gOptionFileBaseName`, `gSignature`, `gDoVerifyOnly`,
`gSignatureOkay`, `gEXsNum`.

Crypto imports from libc/libcrypto: `EVP_DigestInit`, `EVP_DigestUpdate`, `EVP_DigestFinal`,
`EVP_sha1`, `MD5_Init`, `MD5_Update`, `MD5_Final`, `uuid_generate`.

---

## What InstallEXs does NOT do

| Missing capability | Consequence |
|---|---|
| Writes nothing to `/korg/rw/Startup/AuthorizationStrings` | Even after a successful install, the EX is only listed as "installed", not "authorised". OA.ko's per-bank auth check (`IsAuthorizedMultisampleBank`) still requires a matching line in AuthorizationStrings |
| No interaction with the Atmel chip | Cannot derive an auth string itself |

The historical workflow assumed Korg provided a separate auth string (printed on the EX
purchase confirmation, entered via the front panel's "Authorize" screen, which is in Eva
and writes via `/proc/.oacmd AU:`). That's why we have a per-device chip secret but no
per-EX-install auth string generation in InstallEXs.

---

## Planned patch: make InstallEXs produce the auth string

The full plan is in [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md).
Two artifacts:

1. **`oa_authgen.ko`** — small companion kernel module (~100 LOC) that calls
   `stgNV2AC_sync_read_cmd` (exported by `OmapNKS4Module.ko`) to read the 24-byte
   per-device chip secret. Exposes `/proc/.oaauth`: a write of `"GEN:Sxxx"` returns
   the freshly-generated 24-char auth string for that option file.
2. **Patched / rewritten `InstallEXs`** — after `CopyOptionFile`, opens `/proc/.oaauth`,
   writes `GEN:Sxxx`, reads back the auth string, then writes `"AU:<authstring>"` to
   `/proc/.oacmd`. Stock `OA.ko` validates and appends to `AuthorizationStrings`.

End state: any EX installs cleanly on any Kronos, producing a legitimate auth string,
with the stock `OA.ko` unchanged (so future Korg OS updates still work).

---

## Status

| Item | Status |
|---|---|
| Phase 1 prototypes | 26 applied, 4 errors |
| Phase 2 struct layouts | 0 (small class count) |
| Phase 3a return types | 3 refined |
| Documented | Yes |
| Versioned in Ghidra | No |
| Patch planned | Yes — see above |
