# `/proc/.oacmd` — Userspace ↔ OA.ko Command Channel

The single procfs file `OA.ko` creates for userspace control. Every userspace tool
(`Eva`, `InstallEXs`, the auth-string-generator we plan to add) talks to `OA.ko`
through it.

---

## Mechanics

| Aspect | Detail |
|---|---|
| Path | `/proc/.oacmd` (hidden — starts with `.`) |
| Created by | `OA.ko` via `create_proc_entry` |
| File operations table | `oa_cmd_fops` |
| Handlers | `oa_cmd_open`, `oa_cmd_read`, `oa_cmd_write`, `oa_cmd_close` |
| Write handler | `oa_cmd_write` @ Ghidra `0x9f20` |
| Dispatcher | `ProcessOACmd` @ Ghidra `0xa0c0` |
| State machine | `sOACmdStatus` — 0 = idle, 1 = ready, 2 = processing, 3 = result ready |
| Max command size | 127 bytes (`0x7f`) |

Userspace pattern: `write` command → `read` result.

---

## Command grammar

Commands are ASCII strings prefixed by a 2-letter (sometimes 3-character) code followed
by `:`. The dispatcher in `ProcessOACmd` matches the prefix and routes to the handler.
The current dispatch table (extracted from `ProcessOACmd`'s string comparisons):

| Cmd | Full form | Handler / effect |
|---|---|---|
| `LM:` | `LM:<uuid>:<flags>:<sample_idx>` | Load Multisample — `CSTGMultisampleBank::LoadMultisample` |
| `LD:` | `LD:<uuid>:<flags>:<drum_idx>` | Load Drum sample |
| `CM:` | `CM:<uuid>` | Close Multisample (release a loaded bank's files) |
| `CD:` | `CD:<uuid>` | Close Drum |
| **`AU:`** | **`AU:<24-char auth string>`** | **Verify + save authorisation string** — calls `VerifyAndSaveAuthString`. If valid, appends to `/korg/rw/Startup/AuthorizationStrings` AND calls `CSTGKLMManager::AuthorizeProduct` so the EX is live immediately, no reboot |
| `CL:` | `CL:<...>` | Close all bank files |
| `PT:` | `PT:<...>` | Rescan Piano Types |
| `SO:` | `SO:<...>` | Sound Options — `CSTGInstalledEXProducts::ReInitialize` |
| `PC:` | `PC:<flag1>:<flag2>:<flag3>` | PCM Precache — `CSTGPCMPrecacheManager::Reset` |
| `KI:` | `KI:<value>` | Set kill / interrupt flag |
| `LA:` | `LA:<long long>...` | Load Aux (load arbitrary by handle?) |
| `PR:` (PR* ?) | post-process / `AfterProcess` | `CSTGPCMPrecacheManager::AfterProcess` |

(The exact set is what `ProcessOACmd` recognises — the table above is from reading the
disassembly at `0xa0c0`. The 2-letter dispatch is followed by a `:` and a payload.)

---

## The `AU:` command in detail (the one we care about most)

```c
// pseudocode
case 'A','U':
    char *authstring = command + 3;   // skip "AU:"
    uint result = CSTGInstalledEXProducts::VerifyAndSaveAuthString(
                      g_InstalledEXProducts, authstring);
    *outparam = (result ^ 1) & 0xff;  // 0 = ok, 1 = fail
    return 0;
```

`VerifyAndSaveAuthString` (@ `0x48290`):

1. Calls `VerifyAuthorizationString` which:
   - Reads 24 bytes from chip addresses `0x10`/`0x18`/`0x20`
   - Base32-decodes the 24-char auth string → 15 ciphertext bytes
   - Blowfish-CFB-64 decrypts using `chip_secret[0:16]` as key, `chip_secret[16:24]` as IV
   - Verifies the MD5 fingerprint against the named option file
2. If valid: iterates `CSTGInstalledEXProducts` looking for an EX whose 4-byte option ID
   matches the decrypted plaintext bytes `[8..11]`
3. For a match: marks `bField_0xa2 = 1` (authorised), calls
   `CSTGKLMManager::AuthorizeProduct`
4. Opens `/korg/rw/Startup/AuthorizationStrings` for append, seeks to end, writes
   `authstring` + `\n` (the file is line-oriented)

---

## Usage from a shell

You can drive this from a root shell on the live device:

```sh
# Submit an auth string (requires you to have computed it correctly)
printf 'AU:XFGH9ZQD0RC65GZT7UNQAHTL' > /proc/.oacmd

# Then read the result (0x00 = ok, 0x01 = fail)
od -c /proc/.oacmd | head -1
```

For tools that need to do this repeatedly: open, write, read, close — note the state
machine resets between commands.

---

## What writes the file?

The only writer (in stock OA.ko) is `VerifyAndSaveAuthString`'s tail. So today,
`AuthorizationStrings` is only ever extended via the `AU:` command — there's no other
path. Tools that want to "install an auth string" must go through this channel.

---

## See also

- [`../modules/OA.ko.md`](../modules/OA.ko.md) — where the dispatcher lives
- [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md) — what's being checked
- [`file_formats.md`](file_formats.md) — file content schemas
