# Kronos File Formats — studied Schemas

The on-disk and on-USB files relevant to this project.

---

## `/korg/rw/Startup/AuthorizationStrings`

Plain ASCII, line-oriented. One 24-character authorisation string per line; trailing
`\n` after each. Encoding is the custom base32 documented in
[`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md).

Example (from `dump from kronos`):
```
XFGH9ZQD0RC65GZT7UNQAHTL
4WR0CYRHT858173P0E4NZ0JU
92XT04M28Y74YMPG3P5NK86V
44MPM4VXNT3TEX626T6FCG6R
2GZZVYGVGNCZQKJVJ17CFR1D
ZXP2Q4MMF1LQL0A15LT2TE5E
4PVCMTHE3XA7MFTWZM4MVA85
0313VC4Q0XUA6R9RLNA9GCLA
```

- Each line is **per-device** — these specific strings only authorise EXs on this
  specific Kronos
- Lines are processed by `OA.ko::ParseAuths` at boot and on `AU:` submissions
- Only mutator is `VerifyAndSaveAuthString` (via the `AU:` command on `/proc/.oacmd`)
- A `.backup` file (`AuthorizationStrings.backup`) exists in the same directory
  (24 bytes — likely a single legacy line; format isn't fully understood but not
  required for our work)

---

## `/korg/rw/Options/Sxxx` — Option files

Plain text, no signature embedded (signature lives in the `.exsins` manifest). 4 to 5
lines.

### Korg-internal format (e.g. EXs16):
```
EXs16
Funk and Soul Brass
16
2,17,EXs16 Funk and Soul Brass
```

### KApro / 3rd-party format (e.g. EXs285):
```
EXs285
KApro Premium Grands & Keys
285
2,uuid:a7f5dbaa-aaa2-425a-8519-954227f4b35e,EXs285 KApro Premium Grands & Keys
```

Field-by-field:

| Line | Field | Notes |
|---|---|---|
| 1 | Short identifier `EXs<num>` | 4-byte ASCII, matches the file basename's digits |
| 2 | Friendly name | Free-form, displayed in the UI |
| 3 | Bank number (decimal) | Used as the PCM bank index |
| 4 | `2,<id>,<long name>` | `2` = product type; `<id>` is either a decimal number (Korg-internal) or `uuid:<uuid>` (3rd-party). `<long name>` repeats the EX name |

The MD5 in the auth-string algorithm hashes (a 12-byte prefix from the auth string
plaintext) || (entire option-file bytes). Changing a single character of an option
file invalidates every auth string referencing it.

---

## `EXsInstall.exsins` — EX install manifest

Format: `KEY=VALUE` plain-text, same parser as `install.info` (`ParseInfoLine`). Found
at the root of an EX install USB stick.

```
EXSNUM=285
OPTIONFILE=S285
SIGNATURE=<40-char SHA-1 hex>
```

Optional/additional fields exist (the install also describes the tarball / PCM-bank
file layout) but the three above are mandatory for `InstallEXs` to accept the package.

`SIGNATURE` is computed identically to `UpdateOS`'s signature:

```
SIGNATURE = lowercase_hex(SHA1(option_file_bytes || UpdaterScriptsKey))
```

— i.e. the same `UpdaterScriptsKey` we know, applied to the option file's bytes.
**This signature is universal — anyone with the key (us) can produce a valid `.exsins`
for any EX.**

---

## `install.info` — OS update manifest

See [`../../update-builder/README.md`](../../update-builder/README.md) for the full schema. Summary:

```
VERSION=<any string>             # display only — UpdateOS never compares
SOURCE=<tarball filename>        # mandatory
PRETARSCRIPT=<script filename>   # optional
POSTTARSCRIPT=<script filename>  # optional
SIGNATURE=<40-char SHA-1 hex>    # required if any script field is present
```

Lives at the root of an OS update USB stick.

---

## PCM bank files (`/korg/rw/PCM/Bank<NN>` and `/korg/rw2/PCM/Bank<NN>`)

Binary. The actual sample data. Layout is the `CSTGMultisampleBank` on-disk format —
loaded by `OA.ko::CSTGMultisampleBank::LoadBankMetaData` and friends. Out of scope for
the auth-related work in this project, but documented for completeness:

- `/korg/rw/PCM/Bank<NN>` — primary storage
- `/korg/rw2/PCM/Bank<NN>` — secondary storage (some Kronoses have dual SSDs)
- `NN` is the zero-padded 2-digit bank number from the option file's third line

---

## Things in `/korg/` that are mounts, not real directories

| Path | Mount type |
|---|---|
| `/korg/Eva/` | Loop-mounted encrypted block image — keyed by `pairFact` |
| `/korg/Mod/` | Loop-mounted encrypted block image — also `pairFact`-keyed |
| `/korg/rw/`, `/korg/rw2/` | Normal writable filesystems |
| `/korg/ftp/SSD1/`, `/korg/ftp/HD/` | User-storage volumes exposed over FTP for backup tools |
| `/korg/Mod/OA.ko` | The kernel module file (visible only after the encrypted mount succeeds) |

Patching `OA.ko` requires deploying through the encrypted mount, which requires being
inside the live OS (e.g. via SSH after the `kronos_rooting` update). The patched copy
goes to `/korg/Mod/OA.ko` to override the encrypted-mount original.
