# Atmel NV2AC Chip & the GPA Stream Cipher

The hardware-based device-identity store at the centre of the Kronos's security model.

---

## The chip

An **Atmel NV2AC** — a small dedicated authentication IC with:

- Non-volatile memory partitioned into a *public* region (Public ID) and a *secret* region
- A small command/response protocol over serial/parallel lines
- A built-in stream cipher (Korg calls it "GPA") protecting the on-wire payloads

The chip is soldered onto every Kronos motherboard and is the device's unique identity.
Anything on the device that needs to *prove* it is the device-it-claims-to-be uses
this chip.

---

## Memory layout (what we know)

| Chip address | Size | Use |
|---|---|---|
| (low) | various | Public ID (readable without strong auth) |
| `0x10` | 8 bytes | Per-device secret block 1 |
| `0x18` | 8 bytes | Per-device secret block 2 |
| `0x20` | 8 bytes | Per-device secret block 3 |

Total 24 bytes of per-device secret. **This is the key input** to the EX authorisation
algorithm (see [`auth_string_algorithm.md`](auth_string_algorithm.md)).

---

## Wire protocol

Implemented inside `OmapNKS4Module.ko`, which exports two primitives that everything
else uses:

| Symbol | Purpose |
|---|---|
| `stgNV2AC_sync_cmd` | Send a command frame to the chip and read the synchronous response |
| `stgNV2AC_sync_read_cmd` | Read N bytes from a given chip address (wraps `stgNV2AC_sync_cmd` with the right read framing) |

Consumers:

- `GetPubIdMod.ko` — reads the Public ID via these symbols
- `loadmod.ko` — uses them as part of `pairFact` derivation
- `OA.ko` — wraps them in `nv2ac_read_data` (Ghidra `0x4f4840`) which the auth chain
  calls for the 24-byte secret

---

## The GPA stream cipher

A custom Korg stream cipher (not a standard one). Fully studied in earlier
sessions — algorithm and constants documented in the auto-memory under
`getpubidmod_analysis`.

Key properties:

- Self-contained — no per-device key needed; the algorithm uses a fixed Korg key
- Symmetric — same routine for encrypt and decrypt
- Used purely for **on-wire confidentiality** between the chip and the kernel module —
  it does not protect the values themselves, only their transit

This means: a userspace re-implementation of the chip protocol is technically possible
(GPA is RE'd, the chip address map is known, and the OMAP GPIO/CPLD registers are
documented). The blocker is just the engineering effort vs. the ~100 LOC of a helper
kernel module that calls the exported primitives.

---

## Atmel auth crypto primitives in OA.ko

`OA.ko` also has its own Atmel-specific helpers (used by the auth chain setup, not by
the per-line verification):

| Function | Address | Role |
|---|---|---|
| `atmel_auth_compute_c1` | `0x4f61c0` | Computes the challenge response (C1 in Atmel parlance) for a given seed |
| `atmel_auth_set_params` | `0x4f61a0` | Configures the chip's auth parameters before a session |
| `SetupAtmelForAuthorizations` | `0x207a50` | Top-level: arranges the chip into the right state before `ParseAuths` runs |

These are bookmarked with category `patch_for_auth` in the Ghidra project.

---

## Config-zone cross-check across 3 real units (2026-07-16)

Full `KronosExtract.bin`-format captures from three physically different
units — two Kronos (referred to here as `947e`, `6630`) and one Nautilus
(`2D68`) — became available, the first time this project has had more than
one unit's data to compare. (Device identifiers/public IDs intentionally
not reproduced here — private per-device info.) Diffed the full 128-byte
config zone across all three:

- **`cfg[0x00:0x10]`, `cfg[0x20:0x28]` (`d508d549d58ad7cb`), and
  `cfg[0x40:0x50]` (`4142434445464748494a4b4c4d4e4f50` — literally the ASCII
  string `"ABCDEFGHIJKLMNOP"`) are byte-for-byte identical across all three
  units, including across the Kronos/Nautilus product-line boundary.** These
  are template/manufacturer constants baked into the config-zone layout
  itself, not per-device data — `"ABCDEFGHIJKLMNOP"` in particular has the
  unmistakable look of a factory read/write self-test pattern (the same
  flavor of thing found independently in the NKS4 panel firmware's own
  `CryptoAt88.cpp` self-test, which writes a sequential `0..15` pattern —
  see `docs/modules/KRONOS_V06R06.VSB.md`). Not secret, not per-device,
  useful mainly as a sanity check that config-zone addressing is aligned the
  same way across units and product lines.
- **`cfg[0x50]`, `cfg[0x60]`, `cfg[0x70]` each start a 16-byte-aligned
  record: `[0xff status byte][7 per-unit, non-repeating bytes][8 zero
  bytes]`.** `cfg[0x50..0x57]` is the already-documented AAC/IV zone (see
  `AT88VirtualChip/at88_chip.h`'s `p3` field, and `kNv2acStatusZone` in
  `reconstructed/OA/src/auth/nv2ac_handshake.cpp`, which lists exactly these
  three addresses plus `0x80` as a 4-entry table indexed by `sel`). What's
  new: **`0x60` and `0x70` are not blank or default-filled on any of the
  three real units** — they hold real, structurally-identical, per-device
  data, exactly like the slot at `0x50` that's actually used. But no
  currently-reconstructed code path (`OA.ko`, `loadmod.ko`) ever calls with
  `sel != 0` — every real call site hardcodes `sel=0`. So these two slots are
  **provisioned but functionally unconfirmed**: something writes real
  per-device secret-shaped material there, but nothing in this project's
  reconstructed call graph reads it back. Worth investigating whether
  `sel=1`/`sel=2` correspond to the other two auth systems `CLAUDE.md`
  tracks (boot auth's RSA-PRNG system, or the `.reauth`/`pairFact3` system —
  see `cryptoloop_keys.md`), which use the AT88 chip via entirely different
  call paths (`loadmod.ko`'s `RetrieveSecurityICKey`, or the boot_auth
  tooling) not yet cross-checked against this table.
- Checked whether any of the config-zone slot data or the freely-readable
  IdN (`cfg[0x19:0x20]`) has any derivable relationship to the Zone0 secret
  (`0x00`-`0x27`) — direct substring match, byte-wise XOR (pairwise across
  units and within a unit), and hashing (MD5/SHA1/SHA256 of IdN, salted and
  unsalted) all came up negative on all three units. No evidence of any
  derivation; see `auth_string_algorithm.md`'s existing "no known way to
  forge without the chip secret" conclusion, now checked empirically rather
  than only argued from the chip's designed security properties.

---

## Implications for our project

| Goal | What we need from the chip |
|---|---|
| EX auth-string generation | Read the 24 secret bytes at `0x10`/`0x18`/`0x20` |
| Boot on hardware with a failed chip | Spoof the chip's responses — substantial work (would need a small kernel helper that intercepts `stgNV2AC_sync_*`) |
| `pairFact` derivation for new hardware | Read Public ID + perform the RSA + NLFSR derivation that `loadmod.ko` does |

For the auth-string goal — by far our most immediate need — the recommended path is a
tiny `oa_authgen.ko` that exposes the secret (or a generated auth string) to userspace.
See [`auth_string_algorithm.md`](auth_string_algorithm.md#userspace-access-options).

---

## See also

- [`../modules/GetPubIdMod.ko.md`](../modules/GetPubIdMod.ko.md) — the public-ID consumer
- [`../modules/OmapNKS4Module.ko.md`](../modules/OmapNKS4Module.ko.md) — the primitives' owner
- [`../modules/loadmod.ko.md`](../modules/loadmod.ko.md) — heaviest consumer (pairFact derivation)
- [`auth_string_algorithm.md`](auth_string_algorithm.md) — what we do with the 24 secret bytes
