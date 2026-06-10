# EX Authorization Storage & Verification Flow

How the stock Kronos firmware verifies, persists, and re-applies EX-expansion
authorization strings — the runtime path that the `IsUsingAnyUnauthorized*`
patches bypass, but which the original system uses to keep paid expansions
licensed across reboots.

Companion to [`auth_string_algorithm.md`](auth_string_algorithm.md), which
covers the *cryptographic* construction of an auth string. This document
covers the *data flow*: where the string comes from, where it's stored, and
when it's re-checked.

All function addresses below are from **3.2.2 OA.ko**. They are identical in
3.2.1 (both binaries place the auth subsystem below `.text+0x33DF0F`, the
first byte-level diff between the two versions).

---

## TL;DR

```
USER TYPES STRING                BOOT-TIME RESTORE                AUDIO RUNTIME CHECK
──────────────────                ─────────────────                ───────────────────
Front panel                      /korg/rw/Startup/                Per voice / per render
       │                         AuthorizationStrings (text)              │
       ▼                                  │                                ▼
ProcessOACmd                              ▼                       IsAuthorizedMultisampleBank
 (/proc/.oacmd)               CSTGInstalledEXProducts::Initialize    (FNV-1a hash check)
       │                                  │                                │
       ▼                                  ▼                                ▼
VerifyAndSaveAuthString          ParseAuths (tokenise file)          uses bank->+0x6D
       │                                  │                          (computed at auth time)
       ▼                                  ▼
VerifyAuthorizationString        ParseAuth (per entry)
   (dongle + MD5)                         │
       │                                  ▼
       ▼                       AuthorizeProductCallback
AuthorizeProduct                          │
   (write hash tags)                      ▼
       │                       (same path as user-typed)
       ▼                       sets bank->+0xA2 = 1
file: append string + '\n'     AuthorizeProduct (hash tags)
to AuthorizationStrings
```

Three layers — runtime entry, persistence, and re-application at boot — share
the same verification core (`ParseAuth`) but enter it differently.

---

## Layer 1 — file storage

### Path
`/korg/rw/Startup/AuthorizationStrings` — plain text, on the writable
partition (survives firmware updates). Read at boot, appended-to at runtime
when a new auth string is accepted.

### Format
Plain ASCII, one auth string per line (any whitespace separator works
because `ParseAuths` tokenises by anything outside `[0-9A-Za-z-]`):

```
DV4XK-A9P82-HFM3R-A8C2K-N9D7L-...    (24 chars per line, base32-ish + sep)
LB-Q2K-J4F8N-...
...
```

Each line is up to 256 chars (`local_15f[280]` in `ParseAuths`); in practice
each well-formed string is exactly 24 characters (5 + 5 + 5 + 5 + 4 = 24
alphanumerics, with `-` separators stripped before decoding).

The file is read at boot via
`CSTGFile_ReadFileIntoNewBuffer` and written incrementally — see Layer 2
for the append protocol.

### Per-bank checksum (separate from the file)

Each authorised multisample bank carries a 32-bit **FNV-1a-derived
checksum** at offset `+0x6D` of its struct. This is the runtime "is this
bank authorised" tag (see Layer 4). It is computed at auth time from the
bank UUID + a per-product counter, and verified on every render. The
checksum is recomputed at boot via the same `ParseAuth` → `AuthorizeProduct`
chain — it is not persisted on disk.

---

## Layer 2 — runtime entry (user types a code on the front panel)

### Entry point: `/proc/.oacmd`
Eva (the userspace app) sends OA-command messages to `OA.ko` via the
`/proc/.oacmd` interface. The dispatcher at `ProcessOACmd` (around
`.text+0xA190`) routes auth-string entries to:

### `CSTGInstalledEXProducts::VerifyAndSaveAuthString` — `.text+0x48290`

```c
bool VerifyAndSaveAuthString(char *auth_string) {
    // 1. Dongle + MD5 verification of the string itself
    if (VerifyAuthorizationString(auth_string) != 0) return false;

    // 2. Look up which installed product the string authorises
    //    (scan CSTGInstalledEXProducts list at +0x18)
    for (each product in installed_products) {
        if (memcmp(product->id, decoded_string_id, 4) == 0) {
            if (product->is_authorized) return true;   // already authorised
            break;
        }
    }

    // 3. Mark the product authorised and compute per-bank hash tags
    for (each product in installed_products) {
        if (memcmp(product->id, decoded_string_id, 4) == 0) {
            product->is_authorized = 1;                   // +0xA2 flag
            CSTGKLMManager::AuthorizeProduct(product);    // FNV-1a hash → bank->+0x6D
        }
    }

    // 4. Persist to AuthorizationStrings file (append, not overwrite)
    if (CSTGFile_Open(path, "a")) {
        CSTGFile_Seek(0, SEEK_END);
        CSTGFile_Write(auth_string, strlen(auth_string));
        CSTGFile_Write("\n", 1);
        CSTGFile_Close();
    }
    return true;
}
```

So a successful entry **(a)** lights up `product->is_authorized = 1`,
**(b)** writes hash tags into every multisample bank the product owns, and
**(c)** appends the literal string to disk so it can be re-applied on next
boot. The file is the source of truth; the in-memory tags are computed
data.

---

## Layer 3 — boot-time restore

### Entry point: `CSTGInstalledEXProducts::Initialize` — `.text+0x48620`

Called from `CSTGEngine::Initialize` at boot, after the installed-products
list is populated from the option files on disk.

```c
bool Initialize(CSTGInstalledEXProducts *this) {
    // Build the installed-products list from /korg/rw/Options/*
    CFileFolder::ProcessFiles(options_dir, callback1);
    Alloc(...);
    CFileFolder::ProcessFiles(options_dir, callback2);

    // Read AuthorizationStrings and walk every entry
    CSTGFile_ReadFileIntoNewBuffer("AuthorizationStrings", &buf, &len);
    int rc = ParseAuths(buf, len);
    CSTGFile_FreeReadBuffer(buf);
    return rc == 0;
}
```

### `ParseAuths` — `.text+0x207C50`
Tokenises the file (skips whitespace, accumulates `[0-9A-Za-z-]` chars
into a 256-byte buffer), then for each token:

1. Calls `DecodeBytesFromAscii` — converts the 24-char base32-ish form
   into ~15 binary bytes. (See [`auth_string_algorithm.md`](auth_string_algorithm.md)
   for the exact encoding.)
2. Calls `ParseAuth` (no callback; just verifies + auths).

`ParseAuths` itself starts by calling `fFfFfFfFfFfF13` three times and summing
the results — a **dongle liveness check**. If the chip isn't responding
(those calls return non-zero), `ParseAuths` short-circuits to `return result`
without touching anything. This is why a Kronos with a broken Atmel chip
silently loses all EX authorisations even though the file is intact.

### `ParseAuth` — `.text+0x207890`
Verifies one decoded auth blob against an option file's MD5:

```c
uint ParseAuth(undefined4 *callback_arg) {
    /* receives in ECX: a callback function pointer
       receives binary auth bytes in some register state set up by caller */

    moancjsd82();                                // dongle interaction (read some state)
    if (local_28[0] != 0x0F) return -1;          // expects 15 bytes from chip

    md5_init();
    md5_append(/* some bytes derived from chip read */);
    strcpy(option_filename, /* product id from auth */);
    strcpy(option_filename + N, ".OPT");         // suffix; option file extension

    if (!CSTGFile_FileExists(option_filename)) return 0xFFFFFFFD;
    CSTGFile_ReadFileIntoNewBuffer(option_filename, &buf, &len);

    md5_append(buf, len);                        // hash the option file content
    md5_finish(&digest);

    // Compare the last 3 bytes of the MD5 against bytes in the auth string
    if (digest[13] == auth->md5_tag[0] &&
        digest[14] == auth->md5_tag[1] &&
        digest[15] == auth->md5_tag[2]) {

        if (callback_arg != NULL) {
            *callback_arg     = product_id;
            *(byte*)(callback_arg + 1) = 0;
        }

        if (in_ECX != cleanup_cpp_support) {     // callback supplied
            int rc = (*in_ECX)();                // calls e.g. AuthorizeProductCallback
            return (rc == 0) ? 0 : 0xFFFFFFFC;
        }
        return 0;                                 // verified OK
    }
    return 0xFFFFFFFE;                            // MD5 mismatch
}
```

Key facts:
- The MD5 is over **(some chip-derived bytes) ‖ (option-file content)**.
- Only the **last 3 bytes** of the MD5 are checked against the auth-string
  tail. So an auth string only commits to a 24-bit MD5 tag of the option
  file content. The remaining bits in the string encode the product ID and
  any per-bank info.
- The chip-derived bytes (from `moancjsd82` and friends) are device-specific
  — the same auth string fails on a different Kronos because the dongle
  produces different bytes. This is what makes auth strings non-transferable.

### `AuthorizeProductCallback` — `.text+0x47FA0`
The callback passed by `ParseAuths` into `ParseAuth`. When `ParseAuth`
returns success, this fires:

```c
uint AuthorizeProductCallback(char *product_id_4byte) {
    // scan installed products; for each whose id matches, mark + tag
    for (each product) {
        if (memcmp(product->id, product_id_4byte, 4) == 0) {
            product->is_authorized = 1;
            CSTGKLMManager::AuthorizeProduct(product);
        }
    }
}
```

Same effect as steps 3 of `VerifyAndSaveAuthString`. Net effect: at boot,
every auth string in the file → identical state to the user having typed
them all in via the front panel.

---

## Layer 4 — per-render bank check

### `CSTGKLMManager::IsAuthorizedMultisampleBank` — `.text+0x2E650`

Called for every multisample bank used by a voice during synthesis. The
patches in `patch_oa_ko.py` either make this function return 1 directly
(Patch 2 in the canonical script) or NOP out its callers (Patches 6-11).

The stock check:

```c
bool IsAuthorizedMultisampleBank(CSTGKLMManager *this, CSTGMultisampleBank *bank) {
    if ((bank->flags & 8) != 0) return 1;           // some "always authorised" flag
    if (bank->voice_count   == 0) return 1;         // empty bank, nothing to authorise

    // Recompute the FNV-1a hash over the bank UUID
    uint h = 0x050C5D1F;                            // FNV-1a seed
    for (int i = 0; i < 16; i++) {
        h = (h ^ bank->uuid_bytes[i]) * 0x1000193;  // FNV-1a multiplier
    }
    h = (h + 1 + bank->per_product_counter) * (*this);  // (*this) = magic constant

    return (h == bank->stored_hash_at_0x6D);
}
```

### `CSTGKLMManager::AuthorizeMultisampleBank` — `.text+0x2E200`
The write-side of the same check. Computes the same FNV-1a hash, stores it
at `bank->+0x6D` along with `+0x71 = per_product_counter`. After auth, the
bank reads its tag back during render and the check passes.

### `CSTGKLMManager::AuthorizeProduct` — `.text+0x2DE60`
Top-level loop: for each item the product authorises (voice models,
effects, multisample banks), looks up the corresponding struct and calls
the matching `Authorize*` to write its hash tag.

### `CSTGKLMManager::AuthorizeBuiltins` — `.text+0x2E350`
Called at boot before `Initialize` runs. Pre-authorises the **factory ROM
banks** so they're never gated on the AuthorizationStrings file. Iterates
over the static bank list at `CSTGEffectManager::sInstance + 0x800` and
seeds each with the FNV-1a hash. The legacy-bank-prefix UUID is at
`CSTGMultisampleBankUUIDBase::sLegacyBankPrefix`.

---

## Layer 5 — the dongle's role

The cryptographic anchor for all of this is the stgNV2AC chip, accessed
through `OmapNKS4Module.ko`'s `stgNV2AC_sync_read_cmd`. The auth flow uses
the chip in three places:

| Place | Function | What the chip provides |
|---|---|---|
| Liveness check | `ParseAuths` (`fFfFfFfFfFfF13` x 3) | Confirms the chip is reachable; sum of 3 reads must be 0 |
| MD5 prefix bytes | `ParseAuth` (`moancjsd82` then `md5_append`) | Device-unique bytes hashed before the option file content; binds the auth to this physical unit |
| Magic constant | `IsAuthorizedMultisampleBank` (`*this`) | `CSTGKLMManager::operator->()` reads a global the chip wrote during early boot; if missing the hash never matches |

The XOR-decrypt and stream-cipher work happens inside `fFfFfFfFfFfF13`:

```c
fFfFfFfFfFfF13(byte page, byte length, byte *out_buf) {
    // ... chip-mode setup ...
    stgNV2AC_sync_read_cmd(...);
    // ... pull `length` bytes into out_buf ...
    if (mode == 2) {                         // "decrypt" mode
        for each byte: *p ^= gpa_byte;       // GPA stream-cipher byte
    }
}
```

`gpa_byte` is a global maintained by the cipher's key schedule — see
[`atmel_nv2ac.md`](atmel_nv2ac.md) for the cipher itself. The point for
auth purposes is that the bytes feeding `md5_append` in `ParseAuth` are
encrypted-by-chip; the host CPU never sees the raw key, so reproducing
a valid auth string offline is computationally infeasible without a
chip dump.

---

## Function inventory (3.2.2 addresses; same in 3.2.1)

### Storage path (write-side)
| Function | Address | Class | Notes |
|---|---|---|---|
| `VerifyAndSaveAuthString` | `0x48290` | `CSTGInstalledEXProducts` | Front-panel entry point; appends to file |
| `Authorize` | `0x48480` | `CSTGEXProductInfo` | Just sets `+0xA2=1` then calls `AuthorizeProduct` |
| `AuthorizeProductByFilename` | `0x481D0` | `CSTGInstalledEXProducts` | Used during install — match by `.OPT` file name |
| `IsOptionAuthorized` | `0x48130` | `CSTGInstalledEXProducts` | Returns bank `+0xA2` flag; UI display |

### Verification core
| Function | Address | Notes |
|---|---|---|
| `VerifyAuthorizationString` | `0x207DE0` | Dongle liveness + decode + ParseAuth |
| `ParseAuth` | `0x207890` | Per-entry MD5 verification |
| `ParseAuths` | `0x207C50` | Tokenise file + per-token ParseAuth |
| `SetupAtmelForAuthorizations` | `0x207A50` | One-time chip session init for auth ops |

### Boot/file path
| Function | Address | Class | Notes |
|---|---|---|---|
| `Initialize` | `0x48620` | `CSTGInstalledEXProducts` | Reads AuthorizationStrings, calls ParseAuths |
| `AuthorizeProductCallback` | `0x47FA0` | (free fn) | Called by ParseAuth on success |

### Per-bank tagging
| Function | Address | Class | Notes |
|---|---|---|---|
| `AuthorizeProduct` | `0x2DE60` | `CSTGKLMManager` | Iterates banks/voicemodels/effects, calls per-type Authorize* |
| `AuthorizeMultisampleBank` | `0x2E200` | `CSTGKLMManager` | Writes FNV-1a hash to bank `+0x6D` |
| `AuthorizeBuiltins` | `0x2E350` | `CSTGKLMManager` | Pre-tags ROM banks at boot |
| `AuthorizeEffect` | `0x2E190` | `CSTGKLMManager` | Same idea for effects |
| `AuthorizeVoiceModel` | `0x2E120` | `CSTGKLMManager` | Same idea for voice models |

### Render-time check (the gate the patches bypass)
| Function | Address | Notes |
|---|---|---|
| `IsAuthorizedMultisampleBank` | `0x2E650` | Per-render bank check |
| `IsAuthorizedEffect` | `0x2E740` | Per-render effect check |
| `IsAuthorizedVoiceModel` | `0x2E600` | Per-render voice-model check |
| `IsUsingAnyUnauthorizedMultisamples` (6 specs) | `0x13C7D0`, `0x155D30`, `0x5A4020`, `0x5A43C0`, `0x5A6340`, `0x5A8BD0`, `0x5A9F70` | Per-class roll-up; patched to return 0 in all 6 specializations |

### Chip primitives
| Function | Address | Notes |
|---|---|---|
| `fFfFfFfFfFfF13` | `0x4F4850` | Chip read with optional XOR decrypt (`gpa_byte`) |
| `stgNV2AC_sync_read_cmd` | (imported from OmapNKS4Module.ko) | Actual chip I/O |
| `moancjsd82` | (obfuscated wrapper) | Chip-state reader used by ParseAuth |

---

## What this means for patching

The `IsUsingAny*` and `IsAuthorized*` patches break the **render-time
gate** (Layer 4). They don't touch the file or the chip protocol.

A non-rooted Kronos with all the auth strings entered through the front
panel produces the same bank state as a rooted Kronos with the patches —
every bank's `+0x6D` field holds a matching FNV-1a hash, every product's
`+0xA2` flag is set. The difference: the legitimate path requires the
dongle to produce specific bytes for each string, whereas the patched path
makes the render-time check skip the comparison entirely.

If anyone ever wanted to **avoid the OA.ko patch** and instead generate
synthetic auth strings, the gap is `ParseAuth`'s MD5 comparison — it needs
the chip's MD5-prefix bytes to match. There is no known way to forge those
without the chip secret.

---

## Related

- [`auth_string_algorithm.md`](auth_string_algorithm.md) — the encoding/decoding of an auth string (base32 + Blowfish-CFB-64 + MD5)
- [`atmel_nv2ac.md`](atmel_nv2ac.md) — chip protocol + GPA stream cipher
- [`../modules/OA.ko_auth.md`](../modules/OA.ko_auth.md) — patch deployment reference (which sites to break to bypass auth)
- [`../interfaces/proc_oacmd.md`](../interfaces/proc_oacmd.md) — the `/proc/.oacmd` dispatcher that ProcessOACmd implements
