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
| `0x18` | 1 byte | DCR (Device Configuration Register, datasheet sec 6.3.8) - bit 4 is the ETA (Eight Trials Allowed) fuse bit driving the AAC decay-sequence length above. Real value on every unit checked: `0xFB` (ETA not asserted -> 4-step sequence). Not currently modeled as a distinct field (falls in a byte this module doesn't specially interpret), but the AAC-length default it confirms is already applied. |
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

- **RESOLVED (2026-07-20) — config-zone slots `0x60`/`0x70`/`0x80` are
  provisioned-but-unread by design, confirmed rather than assumed.** Real,
  per-device-shaped data sits there on every unit checked, but tracing the
  actual consumer settles why it's never read: `Nv2acVerifyRound()`
  (`reconstructed/OA/src/auth/nv2ac_handshake.cpp`) indexes a real,
  ground-truth `.rodata` lookup table `kNv2acStatusZone[4] = {0x50, 0x60,
  0x70, 0x80}` by a `sel` (0-3) parameter its two real callers,
  `nv2ac_enable_cipher`/`nv2ac_enable_encrypt`, pass straight through from
  `SetupAtmelForAuthorizations()` (`reconstructed/OA/src/auth/atmel_setup.cpp`,
  the sole confirmed-executing caller of both) - and both of
  `atmel_setup.cpp`'s own call sites hardcode `sel=0`, i.e. only
  `kNv2acStatusZone[0] == 0x50` is ever actually selected. The table's
  other three entries exist in the real binary (so the reconstruction is
  faithful to reproduce them) but are structurally unreachable from any
  call path this project has found - `crypto_at88_self_test`
  (`reconstructed/K1_V06R06/crypto_at88.c`, the panel firmware's
  independent AT88 consumer) also only ever touches zone 0. No further
  validation step is open unless a not-yet-reconstructed OA.ko call site
  (Stage 4/5, not started) turns out to pass a nonzero `sel`.
- **RESOLVED (2026-07-20) — `$B0` (write) does not consult the selected
  zone, and no real consumer anywhere in this project's evidence base
  issues one to a non-zero zone.** Searched every reconstructed real AT88
  consumer for a `0xb0` opcode call: `OA.ko` never issues `$B0` at all (its
  only zone-select-adjacent call is `cm_SetUserZone`'s `$B4`); the only
  `$B0` caller found anywhere is `crypto_at88_self_test()`
  (`reconstructed/K1_V06R06/crypto_at88.c` and its K2 counterpart,
  identical), and it always writes zone 0 (`crypto_at88_write(chip, 0xb0,
  0, 0, 16, buf)`). That function itself has zero located callers in a
  full cross-reference sweep - possibly dead factory-test code - so even
  its own zone-0-only usage may never execute on a real boot. Since no
  confirmed-executing consumer, nor any consumer at all in the traced
  evidence, ever selects a non-zero zone for `$B0`, the current
  always-zone-0 behavior remains correct by the absence of any
  contradicting evidence. Only a not-yet-reconstructed OA.ko call path
  (Stage 4/5, not started) could change this.
- **Non-zero-zone `$B2` reads return an all-zero placeholder**, not real
  chip data - there is no captured data for any zone but zone 0. This
  proves the dispatch plumbing itself is correct (different zones really do
  route differently); it is not a claim about real non-zero-zone contents.
  *To validate:* capture zone 1 (or whichever zone a real consumer selects)
  directly from a chip.
- **PARTIALLY RESOLVED (2026-07-20) — the post-`$B8`/encrypted half of the
  gating is now confirmed against a real, confirmed-executing consumer;
  the pre-`$B8`/raw half remains unconfirmed but is inert.** `OA.ko`'s own
  `fFfFfFfFfFfF13` (`reconstructed/OA/src/auth/atmel_zone_io.cpp`, real
  `$B2` opcode reader for the auth-key-material zones `0x10`/`0x18`/`0x20` -
  i.e. this module's "Zone 0") is called by `ParseAuths`/
  `VerifyAuthorizationString`, the real, confirmed-executing EXs
  authorization path - not dead code. Its decode behavior is gated on the
  persistent `mode` global, which only reaches `2` (decode-enabled) after
  `SetupAtmelForAuthorizations()`'s both `$B8` rounds succeed
  (`nv2ac_enable_cipher`/`nv2ac_enable_encrypt`,
  `reconstructed/OA/src/auth/nv2ac_handshake.cpp`) - and
  `SetupAtmelForAuthorizations()` always runs, once, before `ParseAuths` in
  the real boot sequence. This confirms the encrypted/post-auth half of
  the model against real code: every confirmed-executing Zone 0 access
  happens with `mode==2` (decoded), never `mode==0` (raw). The
  raw/pre-auth half remains unconfirmed by any executing consumer - the
  only candidate for one, the firmware self-test
  (`reconstructed/K1_V06R06/crypto_at88.c`'s `crypto_at88_self_test`), has
  zero located callers anywhere (a separate, already-resolved finding, see
  that file's own header comment) - but this is harmless rather than an
  open risk: nothing in this project's evidence base ever exercises a
  pre-auth zone-0 read, so the raw branch, even if the real chip's actual
  behavior there turns out to differ from this model's assumption, has no
  confirmed real call path that would expose the difference. *Still open
  only if* a not-yet-reconstructed OA.ko call path (Stage 4/5, not
  started) turns out to read Zone 0 before `SetupAtmelForAuthorizations()`
  completes.
- **RESOLVED (2026-07-20) — the AAC decay sequence length (four steps vs.
  eight) is now confirmed four-step from real chip data, not just assumed
  from the documented default.** The Atmel datasheet (`Good Info/Atmel-
  8664-CryptoMem-Low-Density-Full-Specification-Datasheet.pdf`, sec 5.3
  Table 5-1) places the Device Configuration Register (DCR) at config-zone
  address `$18`, byte 0 - immediately before the already-known
  Identification Number field this project already reads at `cfg[0x19:0x20]`
  (Table 5-1's own "`$18` bytes 1-7"), confirming the address alignment.
  DCR bit 4 (mask `0x10`) is the ETA bit (sec 6.3.8.4: active-low, asserted
  = 8-trial mode). Checked `cfg[0x18]` across all three real captured
  units this project has (`real_data/947e/`, `real_data/6630/`, and a
  Nautilus capture in `NautilusTest/`, all `KronosExtract.bin` format):
  **every unit reads `DCR = 0xFB`**, bit 4 set (`0xFB & 0x10 != 0`), i.e.
  ETA not asserted - the 4-step sequence
  (`$FF,$EE,$CC,$88,$00`). `useEightStepAac` staying `0` by default is
  therefore confirmed correct on real hardware, not merely the documented
  factory default carried over unverified.

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
