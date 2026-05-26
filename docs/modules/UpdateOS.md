# UpdateOS — OS Update Installer

The standalone userspace binary that runs when the user inserts a Korg OS-update USB
stick and selects "OS Update" on the front panel. It verifies the package, runs the
pre-tar script, extracts the tarball over the filesystem, and runs the post-tar script.

| Property | Value |
|---|---|
| Path on device | `/sbin/UpdateOS` |
| Source path | `dump from kronos/sbin/UpdateOS` |
| Architecture | x86 LE 32-bit ELF executable (ET_EXEC) |
| Size | ~1 037 KB |
| Image base | `0x08048000` |
| Functions | 271 (Ghidra) / 63 nm-defined |
| C++ mangled symbols | 97 |
| Compiler | GCC 4.5.0 |
| MD5 | `bf5cc7f790e1fe3264801711ac47ce5b` |

**Authoritative document:** [`../../update-builder/README.md`](../../update-builder/README.md) — full
internals, signature algorithm, key location, and `update_builder.py` usage.

---

## Key facts

| Property | Value |
|---|---|
| Entry point | `0x8049770` |
| `main` | `0x0804cb60` |
| `ParseInfoLine` | `0x0804b7c0` |
| `VerifyScriptsSignature` | `0x0804c020` |
| `VerifySourceExists` | `0x0804c830` |
| Stripped? | **No** — debug symbols present |
| `UpdaterScriptsKey` | 16 bytes at `.data` VMA `0x0813bac8`, file offset `0x0f2ac8` |
| Key bytes | `13 d0 af ef e0 3c 9b 92 16 2f ae ff 77 53 55 e1` |

---

## install.info format

```
VERSION=3.2.1                            # display only — UpdateOS never checks the value
SOURCE=KronosOS.tar.gz                   # the payload tar.gz, must be on the USB stick
PRETARSCRIPT=pretar.sh                   # optional shell script, run before tar extract
POSTTARSCRIPT=posttar.sh                 # optional shell script, run after tar extract
SIGNATURE=<40-char SHA-1 hex>            # required if any script field is present
```

UpdateOS mounts the USB at `/mnt/updaterSource/` and looks for `install.info` and the
SOURCE file at that path.

---

## Signature algorithm (verified, two known-good test vectors)

```
SIGNATURE = lowercase_hex( SHA1( pretar_bytes || posttar_bytes || UPDATER_KEY ) )
```

- Scripts are concatenated in order: pretar first, posttar second
- If only one is present, only its bytes are hashed
- `UPDATER_KEY` is the 16 bytes above, always appended last

| Reference package | Pretar | Posttar | Expected SIGNATURE |
|---|---|---|---|
| `KRONOS_Updater_3_2_1` | `pretar.sh` | `posttar.sh` | `8844b641ee9ba99a35897dc961885c82850d08c0` |
| `kronos_rooting` | `pretar.sh` | `posttar.sh` | `0849999e1e85ba3208f571afbbfe683e95c4ccf7` |

Both reproduce correctly with [`update-builder/update_builder.py`](../../update-builder/update_builder.py).

---

## What `UpdateOS` does NOT check

| Thing | Why we know |
|---|---|
| The `VERSION` value | `strings` shows no comparison to running OS; only used for the LCD display |
| The MD5 of the tarball or filesystem | `strings UpdateOS | grep -i md5` returns nothing; MD5 validation is done by pretar/posttar scripts using a bundled-on-USB `md5sum` binary |
| Anything about the user — no public key, no per-device challenge | Pure SHA-1 with a fixed key |

This is the open-door: **anyone with the 16-byte key can produce a valid update package
that the stock UpdateOS will install on every Kronos in the world.** That key is now
public (and was already public via the `kronos_rooting` project on GitHub since 2014ish).

---

## Status

| Item | Status |
|---|---|
| Phase 1 prototypes | 50 applied, 1 error |
| Phase 2 struct layouts | 1 built |
| Phase 3a return types | 13 refined |
| Documented | Fully (also [`../../update-builder/README.md`](../../update-builder/README.md)) |
| Versioned in Ghidra | No |

---

## See also

- [`../../update-builder/README.md`](../../update-builder/README.md) — full internals + tool guide
- [`update-builder/update_builder.py`](../../update-builder/update_builder.py) — the package builder
- [`../crypto/update_signature.md`](../crypto/update_signature.md) — algorithm in one page
- [`InstallEXs.md`](InstallEXs.md) — sibling installer using the same key
