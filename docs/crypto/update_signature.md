# OS-Update Script Signature

`UpdateOS` (and by close family resemblance, `InstallEXs`) protects its scripts with a
SHA-1 keyed hash using a fixed 16-byte secret embedded in the binary. The algorithm is
trivial and the key is public.

**Authoritative document:** [`../../update-builder/README.md`](../../update-builder/README.md) — full
internals, control-flow trace, and `update_builder.py` user guide.

---

## Algorithm

```
SIGNATURE = lowercase_hex( SHA1( pretar_bytes || posttar_bytes || UPDATER_KEY ) )
```

| Parameter | Notes |
|---|---|
| `pretar_bytes` | The raw file content of `PRETARSCRIPT` (if present in `install.info`) |
| `posttar_bytes` | The raw file content of `POSTTARSCRIPT` (if present) |
| Order | Pretar first, posttar second — empirically verified against two reference packages |
| `UPDATER_KEY` | 16 bytes, appended last (after both scripts) |
| Output | 40 lowercase hex chars; stored as `SIGNATURE=<hex>` in `install.info` |

If only one script is present, only that script's bytes are hashed; the key is still
appended at the end. If neither script is present, the SIGNATURE field is not required.

---

## The key

| Property | Value |
|---|---|
| Symbol | `UpdaterScriptsKey` |
| Bytes | `13 d0 af ef e0 3c 9b 92 16 2f ae ff 77 53 55 e1` |
| Location in `UpdateOS` | `.data` VMA `0x0813bac8`, file offset `0x0f2ac8` |
| Location in `InstallEXs` | File offset `0xa610` (same 16 bytes) |
| Source-tree header (leaked) | `STG/platformRT/CopyProtKeys/UpdaterScriptsKey.h` |

The key is identical between `UpdateOS` and `InstallEXs` — Korg's `UpdaterScriptsKey.h`
is shared across both tools.

---

## Verified test vectors

| Package | PRETARSCRIPT | POSTTARSCRIPT | Expected SIGNATURE |
|---|---|---|---|
| `KRONOS_Updater_3_2_1` | `pretar.sh` | `posttar.sh` | `8844b641ee9ba99a35897dc961885c82850d08c0` |
| `kronos_rooting` | `pretar.sh` | `posttar.sh` | `0849999e1e85ba3208f571afbbfe683e95c4ccf7` |

Both reproduce with `update_builder.py` (located at `update_builder.py`).

---

## Threat model

The key was already public via the `kronos_rooting` project on GitHub (since ~2014). Our
analysis simply re-derives it from the binary. Anyone can sign update packages and EX
install packages.

What this signature does *not* protect against:

- **Per-device licensing** — the per-EX `AuthorizationStrings` use a *different*
  algorithm tied to the per-device chip secret (see [`auth_string_algorithm.md`](auth_string_algorithm.md))
- **Kernel module signatures** — `loadmod.ko` does RSA-based verification of its own
  loaded modules (see `loadmod_analysis.md`) — a different key system entirely

So a forged update package can install any OS files, including a modified `OA.ko`, but
not (without further work) a modified `loadmod.ko`.

---

## See also

- [`../../update-builder/README.md`](../../update-builder/README.md) — full RE + tool guide
- [`../modules/UpdateOS.md`](../modules/UpdateOS.md) — the binary that uses the signature
- [`../modules/InstallEXs.md`](../modules/InstallEXs.md) — sibling installer with the same key
- [`update-builder/update_builder.py`](../../update-builder/update_builder.py) — package builder
