# EX Authorisation-String Algorithm

The full studied algorithm that `OA.ko` uses to verify the 24-character
authorisation strings stored in `/korg/rw/Startup/AuthorizationStrings`. Knowing this
algorithm is what makes it possible to install any EX, on any Kronos, with a stock
(unpatched) `OA.ko`.

**studied from** `OA.ko::VerifyAuthorizationString` (0x207de0), `ParseAuth`
(0x207890), `ParseAuths` (0x207c50), `moancjsd82` (0x4f5f00, the Blowfish-CFB-8 codec),
`DecodeBytesFromAscii` (0x4f39c0), `VerifyAndSaveAuthString` (0x48290),
`ProcessOACmd` (0xa0c0), `oa_cmd_write` (0x9f20).

**Verified end-to-end** by a Python reference implementation at `/tmp/auth_algo.py` —
synthetic auth string generates, decodes, and round-trip-verifies.

---

## What an auth string looks like

```
XFGH9ZQD0RC65GZT7UNQAHTL          # 24 chars, custom base32
4WR0CYRHT858173P0E4NZ0JU
92XT04M28Y74YMPG3P5NK86V
...
```

One line per authorised EX. Examples above are from the dump in
`dump from kronos/korg/rw/Startup/AuthorizationStrings` — they're
real, *per-device* strings that only validate against the specific Kronos they came from.

---

## Inputs to the algorithm

| Input | Source | Bytes |
|---|---|---|
| **Chip secret** | Atmel NV2AC chip addresses `0x10`, `0x18`, `0x20` (3 × 8 bytes) | 24 |
| **Option file** | `/korg/rw/Options/Sxxx` — plain-text EX metadata | variable, typically 60–120 bytes |
| **Option ID** | 4 ASCII chars, e.g. `b"S285"` (the option file basename) | 4 |
| **Salt** | Issuer-chosen; verifier just hashes whatever was used | 8 |

---

## The algorithm

```
plaintext_15  =  salt_8  ||  option_id_4  ||  fingerprint_3

  where fingerprint_3 = MD5(plaintext[0..11] || option_file_bytes)[3], [7], [11]

ciphertext_15 = Blowfish_CFB8_encrypt(
                    key  = chip_secret_24,
                    iv   = chip_secret_24[16:24],
                    data = plaintext_15)

auth_string_24 = custom_base32_encode(ciphertext_15)
```

### Step-by-step

1. **Build 12-byte hash input.** `plain_12 = salt_8 || option_id_4`. The salt can be
   any 8 bytes — both the writer and the verifier hash them, so any choice that the
   writer commits to works (`b"\x00"*8`, a timestamp, random bytes — issuer-chosen).
2. **Compute MD5 fingerprint.** Hash `plain_12 || file_bytes_of("/korg/rw/Options/<option_id>")`.
   Take bytes `[3]`, `[7]`, `[11]` of the 16-byte digest. This is the truncated
   fingerprint that ties the auth string to a specific option file's content.
3. **Assemble the 15-byte plaintext.** `plaintext_15 = plain_12 || fingerprint_3`.
4. **Blowfish-CFB-8 encrypt.** Key = the 24 chip-secret bytes (standard Blowfish key
   schedule with cycled key extension — OA.ko uses the bog-standard Blowfish P-array
   constants, digits of π, stored at Ghidra address `0x678d60`). IV = the last 8 bytes
   of the key (`chip_secret_24[16:24]`). Mode = byte-level CFB.
5. **Base32 encode.** Custom alphabet `0123456789ACDEFGHJKLMNPQRTUVWXYZ`. Encode the
   15 ciphertext bytes as 24 base32 chars (8 chars per 5-byte chunk × 3 chunks).
   When *decoding*, the chars `B/O/I/S` are accepted and remapped to `8/0/1/5` (visual
   ambiguity-tolerant) — but when *encoding*, the alphabet is used as-is (no `B/O/I/S`
   in produced output).

### Reference implementation

See `/tmp/auth_algo.py` (Python, depends on `pycryptodome` for Blowfish-ECB). The
implementation is ~150 lines and is verified by encoding-then-decoding a synthetic
input.

---

## How OA.ko verifies (the matching decode path)

`ParseAuths` runs at startup against the whole `AuthorizationStrings` file:

```
for each non-empty line:
    ct = custom_base32_decode(line)                    # 15 bytes
    chip_secret = nv2ac_read_data(0x10, 0x18, 0x20)    # 24 bytes from chip
    pt = Blowfish_CFB8_decrypt(chip_secret, ct,
                               iv = chip_secret[16:24])
    if len(pt) != 15: reject
    salt, option_id, fp_claimed = pt[0:8], pt[8:12], pt[12:15]
    path = "/korg/rw/Options/" + option_id
    if not file_exists(path): reject
    expected = MD5(pt[0:12] || open(path).read())
    if (expected[3], expected[7], expected[11]) == fp_claimed:
        mark this EX as authorised
```

### Key constants in `OA.ko`

| Symbol | Ghidra address | Notes |
|---|---|---|
| Blowfish initial P/S array | `0x678d60` | Standard pi-derived values (`0x243f6a88, 0x85a308d3, ...`) |
| Working Blowfish state | `0x00cae980` | Where the keyed P/S array lives after schedule |
| `sEncodeTable` | `0x00678cc0` | `"0123456789ACDEFGHJKLMNPQRTUVWXYZ"` |
| `sRemapEncodeTable` | `0x00678cb8` | `B→8, O→0, I→1, S→5` pairs |
| `k5BitShifts` | `0x00678ca0` | `[27, 22, 17, 12, 7, 2]` (per-char bit positions in 5-byte chunk) |
| `kOptionsPath` | `0x0000a52c` (file-offset addressing) | `"/korg/rw/Options/"` |

---

## Why this is reproducible without Korg

Every input except the **per-device chip secret** is on the device, in the open. The
secret is in the chip, but the chip is on the board. So:

- Each device legitimately has its own chip secret
- Each EX install can compute its own correct auth string
- Stock `OA.ko` accepts the result because the chip secret matches

No Korg server, no per-customer key, no online activation.

---

## Userspace access options <a name="userspace-access-options"></a>

`OA.ko::nv2ac_read_data` is kernel-only. No stock `/proc` file exposes the 24 secret
bytes to userspace. To compute an auth string in userspace, one of:

### (A) Tiny helper `.ko` module (recommended)

A new ~100-LOC kernel module — call it `oa_authgen.ko` — that:

- Imports the already-exported `stgNV2AC_sync_read_cmd` (from `OmapNKS4Module.ko`)
- Reads the 24 secret bytes
- Exposes `/proc/.oaauth`: writing `"GEN:Sxxx"` returns the freshly-generated auth string
  for option file `Sxxx`; OR writing nothing and reading returns the raw 24-byte secret

```
/* sketch of oa_authgen.ko */
static ssize_t oaauth_write(struct file *f, const char __user *buf,
                            size_t n, loff_t *o) {
    char cmd[32]; copy_from_user(cmd, buf, min(n, 32));
    if (strncmp(cmd, "GEN:", 4) == 0) {
        u8 secret[24];
        stgNV2AC_sync_read_cmd(0x10, 8, secret+0);
        stgNV2AC_sync_read_cmd(0x18, 8, secret+8);
        stgNV2AC_sync_read_cmd(0x20, 8, secret+16);
        /* read option file, compute MD5, Blowfish-CFB-8, base32 encode */
        compute_auth_string(secret, &cmd[4], result_buf);
        /* stash result_buf for next read() */
    }
    return n;
}
```

Patched `InstallEXs` then does:

```c
fd = open("/proc/.oaauth", O_RDWR);
write(fd, "GEN:S285", 8);
read(fd, authstring, 24);
close(fd);

fd = open("/proc/.oacmd", O_WRONLY);
char cmd[32]; snprintf(cmd, 32, "AU:%s", authstring);
write(fd, cmd, 3+24);
close(fd);
/* stock OA.ko's VerifyAndSaveAuthString does the rest */
```

### (B) Userspace re-implementation of the NV2AC + GPA cipher

Possible (GPA is fully RE'd — see [`atmel_nv2ac.md`](atmel_nv2ac.md)), but requires
root + `iopl()` and is substantial code. Not recommended when option (A) is so much
simpler.

### (C) Patch `OA.ko` to add a `GS:` (get-secret) command

Defeats the goal of leaving `OA.ko` stock so future OS updates keep working. Mentioned
only for completeness.

---

## Threat model considerations

Anyone with the chip secret of a given Kronos can forge auth strings for that Kronos
only. They cannot:

- Forge auth strings for other Kronoses (each has its own chip secret)
- Forge an `.exsins` install package (that requires the `UpdaterScriptsKey`, which is
  separately known and embedded in `InstallEXs`/`UpdateOS`)

In other words: a leaked chip secret authorises an attacker to install any EX on **that
one device only**. This matches Korg's apparent design — the device, not the user, is the
locked unit.

---

## See also

- [`../modules/OA.ko.md`](../modules/OA.ko.md) — where the verifier lives
- [`../modules/InstallEXs.md`](../modules/InstallEXs.md) — the planned consumer
- [`../interfaces/proc_oacmd.md`](../interfaces/proc_oacmd.md) — the `AU:` submission interface
- [`atmel_nv2ac.md`](atmel_nv2ac.md) — the chip protocol
- [`../interfaces/file_formats.md`](../interfaces/file_formats.md) — `AuthorizationStrings` and option files
