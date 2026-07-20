# AT88VirtualChip - software AT88SC/NV2AC chip emulator

A software stand-in for the Atmel AT88SC "CryptoMemory" security IC that sits
on the NKS4 board of a Korg Kronos (Korg calls it NV2AC). `AT88VirtualChip.ko`
lets `OA.ko` and `loadmod.ko` load and run unmodified - real EXs
authorization and cryptoloop key retrieval, no source changes on either side -
on a system with no real chip present.

Source lives at `kronosology/reconstructed/AT88VirtualChip/`, alongside
`OmapNKS4Module/`, `STGGmp/`, `STGEnabler/`.

## Design rationale

Three properties of the real system make a software emulator sufficient,
rather than requiring the chip's own tamper-resistant silicon:

1. **The real hardware boundary is exactly two symbols.** OA.ko's own
   chip-protocol helpers (`cm_ReadUserZone`, `cm_SetUserZone`,
   `nv2ac_dispatch_cmd`, `nv2ac_enable_cipher`, `nv2ac_enable_encrypt`, ...)
   are internal, already-compiled OA.ko functions that call downward on
   their own - `oa_atmel.h` declares them as externs with no implementation
   anywhere in `reconstructed/OA/`. Neither `OA.ko` nor `loadmod.ko` talks to
   hardware directly; both link against exactly two symbols that
   `OmapNKS4Module.ko` exports: **`stgNV2AC_sync_cmd`** and
   **`stgNV2AC_sync_read_cmd`**. Implementing those two symbols in software
   is enough for both to load and run unmodified.
2. **The on-wire cipher ("GPA", internally called "DEAX" in this codebase)
   has no per-device secret.** It's a fixed algorithm keyed by a constant,
   used purely for on-wire confidentiality - see
   `docs/crypto/atmel_nv2ac.md`.
3. **`loadmod.ko`'s hardest chip operation doesn't require the chip's own
   cryptography.** `loadmod.ko` sends an 80-byte `/.pairFact3` blob to the
   chip and gets back the three cryptoloop volume keys (`Mod.img`/`Eva.img`/
   `WaveMotion.img`). `/.pairFact3` and `/.reauth` are the same format and
   decrypt with plain host-side Blowfish-CFB-64, keyed by Zone0 data this
   module already models - see `pairfact_decrypt.h`. The chip's own internal
   derivation never needs to be reimplemented.

## Scope

| Consumer | Operation | Chip addresses | Notes |
|---|---|---|---|
| `OA.ko` (`SetupAtmelForAuthorizations`, `reconstructed/OA/src/auth/atmel_setup.cpp`) | GPA two-round mutual-auth handshake | zone `0x19` (7 bytes, IdN/config), zone `0x50` (8 bytes, AAC/IV), zones `0x10`/`0x18`/`0x20` (24 bytes, per-device secret) | Fully modeled. Per-device secret and config-zone data load from a captured chip-data blob - see Data format below. |
| `loadmod.ko` (`RetrieveSecurityICKey`) | Send an 80-byte `/.pairFact3` blob, get back 3x16-byte volume keys | opaque chip-internal operation | Not modeled as chip protocol at all - see rationale point 3. Served by a fixture lookup (`pairfact_fixture.cpp`) and a general decrypt (`pairfact_decrypt.cpp`), both host-side logic outside the AT88 opcode dispatch. |
| `GetPubIdMod.ko` | Reads the device's Public ID | zone `0x19` (7 bytes, IdN) - the same address `OA.ko`'s IdN read uses | Public ID is `hex(IdN) + a CRC-8/0xD8 checksum` of that hex string - pure formatting over data this module already serves via its `$B6` config-zone read. No additional chip-side work needed for this consumer. |

## Repository layout

```
AT88VirtualChip/
  README.md            this file
  at88_chip.h           shared zone/state-machine types
  chip_state.cpp         zone storage, $B0/$B6/$B2 dispatch, secret data, DEAX cipher
  bignum.cpp/.h          130-bit modexp primitive for the $B8 challenge seed
  b8_handshake.cpp        $B8 mutual-auth handshake + AAC lockout
  pairfact_fixture.cpp    one known /.pairFact3 blob's key material (host-test only)
  pairfact_decrypt.cpp    general .pairFact3/.reauth decrypt (host-test only)
  nv2ac_exports.cpp       stgNV2AC_sync_cmd / stgNV2AC_sync_read_cmd - the two
                          real exported symbols OA.ko/loadmod.ko link against
  module_main.c           kernel module_init/exit, deferred blob load
  Makefile
  verify/                 host-side known-answer tests (KATs)
```

`chip_state.cpp`, `bignum.cpp`, `b8_handshake.cpp`, `nv2ac_exports.cpp`, and
`pairfact_fixture.cpp` are freestanding C++ (no Linux headers) and build both
as host-testable objects and as part of the real `.ko`. `module_main.c` is
kernel-only, and is deliberately plain C rather than C++ - this kernel's
headers use inline-asm string-literal-suffix syntax a modern `g++` cannot
parse, so this file compiles as C via `gcc` instead. `pairfact_decrypt.cpp`
is host-test-only - it's a verification/analysis tool, not chip emulation,
so it is not part of the `.ko` build.

## Protocol reference

The AT88 command set this module implements, as issued through
`stgNV2AC_sync_cmd`/`stgNV2AC_sync_read_cmd`:

| Opcode | Direction | Wire format | Behavior |
|---|---|---|---|
| `$B4` (zone select) | write | `{0xb4, 0x03, zone, 0x00}` | Stores `zone` as the currently selected zone. Consulted by `$B2`'s read dispatch (see below); **not** consulted by `$B0` (write always targets zone 0 regardless of selection - see Known limitations). |
| `$B0` (zone 0 write) | write | `{0xb0, 0x00, addr, len, data[len]}` | Raw copy into the 40-byte zone-0 buffer at `addr..addr+len`. Always unencrypted, regardless of session state. |
| `$B8` (verify crypto) | write | `{0xb8, zone, 0x00, 0x10, Nc[8], Q[8]}`, 20 bytes | One round of the mutual-auth handshake. `zone == 0x00` is round 1 ("cipher"); `zone == 0x10` is round 2 ("encrypt"), chained on round 1's own output. Accept/reject is not signaled through this call's return value - it's observed via a subsequent `$B6` read of the AAC byte (config-zone address `0x50`) increasing (accepted, reset to `0xFF`) or decreasing (rejected, next step of the decay sequence below). |
| `$B6` (config zone read) | read | `{0xb6, 0x00, addr, len}` | Unencrypted read directly out of the 128-byte config zone. |
| `$B2` (zone read) | read | `{0xb2, 0x00, addr, len}` | Reads from whichever zone the most recent `$B4` selected. Zone 0 (the only zone with real per-device data) is served raw before a session is established, and DEAX-encrypted once one is - see the session-gating note below. |

**Exported symbol ABI.** `stgNV2AC_sync_cmd` must return `int`, not `void` -
every real caller checks the return value (`cm_SetUserZone`,
`Nv2acVerifyRound`). Returning `0` for every recognized opcode is correct;
the real accept/reject signal for `$B8` comes from the subsequent `$B6` AAC
read described above, not from this call's own return value.
`stgNV2AC_sync_read_cmd`'s real signature is `int
stgNV2AC_sync_read_cmd(int cmd4, int dest)` - it packs pointers into `int`
parameters, which is lossless on the real 32-bit target where `int` and a
pointer are the same width.

**AAC lockout.** The Authentication Attempt Counter (config-zone byte
`0x50`) steps through a decay sequence on each rejected `$B8` round:
`$FF -> $EE -> $CC -> $88 -> $00`. Once it reaches `$00` the current key set is
permanently locked for the rest of that session - every further `$B8`,
even with a cryptographically correct `Q`, is rejected without evaluation.
A correct `$B8` round resets the byte to `$FF`. There is no in-protocol
recovery from a lockout; only reloading the chip state (see Data format)
clears it.

**Session gating on `$B2`.** Zone 0 access is gated on whether the `$B8`
handshake has completed: before both rounds are accepted, `$B2` reads zone 0
raw (no DEAX stepping); after, it's DEAX-encrypted using the session cipher
state built up over the handshake. This models the chip's crypto-auth gate
as a configuration rather than a permanently-wired property. **This gating
model itself is not independently confirmed** - see Known limitations.

## Zone map

**Config zone (128 bytes, read via `$B6`):**

| Address | Size | Contents |
|---|---|---|
| `0x00-0x0F`, `0x20-0x27`, `0x40-0x4F` | - | Fixed, manufacturer-constant bytes, identical across every unit checked so far. Not secret, not per-device - useful only as a sanity check that config-zone addressing lines up correctly. |
| `0x19-0x1F` | 7 bytes | IdN - the device's per-device Public ID. |
| `0x50` | 1 byte | AAC status byte (see AAC lockout above). |
| `0x51-0x57` | 7 bytes | Per-device bytes, part of the `$B8` challenge seed (`p3`). |
| `0x60-0x67`, `0x70-0x77` | 16 bytes | Structurally identical per-device-shaped records to the `0x50` slot, present on every unit checked, but not read by any consumer this module currently models - see Known limitations. |
| `0x80` | - | A fourth slot address that appears in `OA.ko`'s own status-zone table alongside `0x50`/`0x60`/`0x70`, but falls past the end of this module's 128-byte config zone. Not used by any call path this module supports. |

**Zone 0 (40 bytes, the only genuinely secret region, read via `$B2`):**

| Address | Size | Contents |
|---|---|---|
| `0x00-0x17` | 24 bytes | `/.pairFact3`/`/.reauth` Blowfish key (16) + IV (8). |
| `0x10-0x27` | 24 bytes | EXs auth-string Blowfish key (16) + IV (8). Overlaps the row above in `0x10-0x17` - both halves must agree there; a captured blob whose two halves disagree over that overlap is invalid. |

## Data format

`at88_chip_load_from_extract()` parses a 188-byte captured chip-data blob:

```
off   0  "KREX"   magic
off   4  version  1
off   5  flags    bit0=pf3_ok  bit1=exs_ok (both set = complete)
off   8  cfg[128] the full config zone described above
off 136  pf3_zone[24]  Zone0[0x00..0x17]
off 160  exs_zone[24]  Zone0[0x10..0x27]
off 184  crc32(4) crc32_le over bytes 0..183
```

The loader rejects the blob on bad magic, bad length, a CRC mismatch, or if
`pf3_zone`/`exs_zone` disagree on their overlapping 8 bytes. On the real
device, this blob is expected at `/korg/rw/KronosExtract.bin` by default
(configurable via the `blob_path` module parameter).

If no valid blob is available, `at88_chip_load_synthetic()` populates the
chip with an all-zero config zone and zone-0 secret - a legitimate,
self-consistent choice, since the DEAX wire cipher itself has no per-device
key: both sides of the `$B8` handshake derive the same expected challenge
from the same (here, zero) config-zone bytes. The one exception is the AAC
byte at config-zone address `0x50`, which is pre-set to `$FF` rather than
left at `0`: `$00` means permanently locked (see AAC lockout above), and
`OA.ko`'s own verification requires reading back exactly `0xFF` to consider
a round accepted, so a naively zero-initialized synthetic chip would refuse
its very first authentication attempt. A synthetic chip's own handshake
therefore succeeds, but it carries no real per-device Zone0 secret, so
EXs auth-string validation against it is not meaningful.

## Building and testing

```bash
# Host-side: compiles each unit as a freestanding x86-32 object (sanity
# check only, not linked) and builds + runs the full known-answer test
# suite. No kernel source tree needed.
make

# The real kernel module, via Kbuild.
make ko KDIR=/path/to/kronos-kernel-tree
```

`KDIR` must point at a configured Linux 2.6.32.11-korg source tree whose
module ABI (struct layouts, the `-mregparm=3` calling convention, RTAI
support) matches the Kronos's own kernel build - a generic, unpatched
2.6.32.11 tree produces a module with the wrong `vermagic`/struct layout and
will not load. Kbuild flags match `OmapNKS4Module`'s own build
(`-mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics
-fno-use-cxa-atexit -fpermissive`), since this module links into the same
kernel, alongside it, and must use the same ABI.

**Blob loading happens off the `init_module` path.** `filp_open` from
`init_module` context fails outright on this kernel - RTAI blocks the
`GFP_KERNEL` allocation `filp_open` needs. `create_proc_entry` in
`init_module` fails silently the same way, and `kthread_run`'s resulting
kthread gets starved by RTAI. A workqueue worker has full `GFP_KERNEL`
available, and is the only approach that works: `AT88VirtualChipInit()` only
sets up a workqueue and queues the real work; the worker function does the
actual `filp_open`/`vfs_read`/blob parsing, falling back to the synthetic
chip on any failure (missing file, short read, or a blob that fails
validation).

## Known limitations

Each item below is something this module does not model with real captured
data, or a design choice not independently confirmed against a real chip.
Where a validation path exists, it's noted.

- **Config-zone slots `0x60`/`0x70` (and the notional `0x80`) are
  unmodeled beyond raw storage.** Real, per-device-shaped data is
  provisioned there on every unit checked, but no consumer this module
  models ever reads it back - every real call site hardcodes the zone-0
  slot (`0x50`). *To validate:* determine whether any other module or auth
  subsystem issues `$B8`/`$B2` selecting one of these slots; if none does,
  these remain provisioned-but-unread by design.
- **`$B0` (write) does not consult the selected zone** - it always targets
  zone 0, unlike `$B2` (read), which routes on the most recently selected
  zone. No consumer this module models ever issues a `$B0` to a non-zero
  zone. *To validate:* capture a real `$B0` write targeting a non-zero zone,
  if one exists, before extending write support to match `$B2`'s dispatch.
- **Non-zero-zone `$B2` reads return an all-zero placeholder**, not real
  chip data - there is no captured data for any zone but zone 0. This
  proves the dispatch plumbing itself is correct (different zones really do
  route differently); it is not a claim about real non-zero-zone contents.
  *To validate:* capture zone 1 (or whichever zone a real consumer selects)
  directly from a chip.
- **Whether zone 0's raw-vs-encrypted gating is genuinely tied to session
  state, as implemented, is unconfirmed.** The evidence motivating the
  raw/pre-auth branch is a firmware self-test routine
  (`reconstructed/K1_V06R06/crypto_at88.c`) that a full cross-reference
  sweep of that firmware image found has no callers anywhere - it may be
  dead code that never executes on a real boot, not a routine confirmed to
  run before `$B8`. The command framing itself is still solid (the same
  header-byte mapping applies to every opcode that firmware's runtime AT88
  queue relay handles, and that relay does run), but the specific claim that
  a real chip serves raw zone-0 access before any `$B8` session exists rests
  on this uncertain evidence. *To validate:* find a confirmed-executing real
  consumer that performs a zone-0 access before completing `$B8`, or
  determine that no such access ever legitimately occurs and the gating can
  be simplified to always-encrypted.
- **The AAC decay sequence length (four steps vs. eight - the chip's "ETA"
  configuration) has not been read back from a live chip's config zone.**
  This module defaults to the four-step sequence
  (`$FF,$EE,$CC,$88,$00`). *To validate:* identify the ETA bit's location in
  the config zone from a captured blob, or observe directly how many
  consecutive rejected `$B8` attempts a real chip tolerates before locking,
  and set `useEightStepAac` on the loaded `AT88ChipState` accordingly if it
  turns out to be the eight-step variant.

## Related documentation

- `docs/crypto/atmel_nv2ac.md` - the GPA/DEAX wire cipher and chip memory
  layout, including the three-unit config-zone comparison referenced above.
- `docs/modules/GetPubIdMod.ko.md` - the Public ID consumer.
- `reconstructed/OA/src/auth/atmel_setup.cpp` - `OA.ko`'s own real
  `SetupAtmelForAuthorizations()` call chain, which this module's chip logic
  is designed to satisfy unmodified.
- `reconstructed/K1_V06R06/crypto_at88.c` - the NKS4 panel firmware's own,
  independent path to the same chip; the source of the `$B0`/zone-select
  wire-framing evidence discussed above.
