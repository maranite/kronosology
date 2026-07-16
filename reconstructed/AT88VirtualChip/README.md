# AT88VirtualChip — software AT88SC/NV2AC chip emulator (design)

A **new** component — this chip never had reconstructed source before; there is no
real `AT88VirtualChip.ko` on any Kronos. It's a from-scratch software stand-in for
the Atmel AT88SC "CryptoMemory" security IC (Korg calls it NV2AC), built so
**`OA.ko` and `loadmod.ko` can load and run unmodified** in a VM or on hardware with
no real chip present.

**Source lives here** (`kronosology/reconstructed/AT88VirtualChip/`), matching the
convention of `OmapNKS4Module/`, `STGGmp/`, `STGEnabler/` alongside it. **Build
outputs** (the compiled `.ko`) are copied/symlinked into `kronos_vm/AT88VirtualChip/`
for actual VM boot-testing, matching how `kronos_vm/`'s other stub drivers
(`8139cp/`, `8139too/`, `fakefb/`) are laid out as siblings of the VM runner scripts.

## Why this is smaller than it looks

Three findings make this tractable rather than "reverse-engineer tamper-resistant
silicon":

1. **The real hardware boundary is exactly two symbols.** OA.ko's own chip-protocol
   helpers (`cm_ReadUserZone`, `cm_SetUserZone`, `nv2ac_dispatch_cmd`,
   `nv2ac_enable_cipher`, `nv2ac_enable_encrypt`, ...) are already-compiled,
   internal OA.ko functions — confirmed by `oa_atmel.h` declaring them as externs
   never implemented anywhere in `reconstructed/OA/` (they live inside the real
   `OA.ko` binary and call downward on their own). Neither `OA.ko` nor `loadmod.ko`
   talks to hardware directly; both link against exactly two symbols that
   `OmapNKS4Module.ko` exports today: **`stgNV2AC_sync_cmd`** and
   **`stgNV2AC_sync_read_cmd`**. Providing a different, software-only
   implementation of just those two symbols is enough — nothing above that layer
   needs to know or care.

2. **The on-wire cipher ("GPA") is already fully reverse-engineered and has no
   per-device secret.** Per `docs/crypto/atmel_nv2ac.md`: "Self-contained — no
   per-device key needed; the algorithm uses a fixed Korg key... Used purely for
   on-wire confidentiality... a userspace re-implementation of the chip protocol
   is technically possible." This can be implemented for real, not faked — it's
   public knowledge at this point, not a secret we're missing.

3. **`loadmod.ko`'s hardest chip operation doesn't need real cryptography at all.**
   `loadmod.ko` sends the 80-byte `/.pairFact3` blob to the chip and gets back the
   three cryptoloop volume keys (`Mod.img`/`Eva.img`/`WaveMotion.img`). The chip's
   *internal* derivation (loadmod's own docs call it "RSA + NLFSR", not reversed,
   genuinely tamper-resistant) doesn't need to be understood: the **final output
   keys were already independently recovered via a `LOOP_GET_STATUS64` side-channel
   and confirmed universal across every unit tested** (`docs/crypto/
   cryptoloop_keys.md`). So for this one operation, the emulator can be a pure
   **response fixture**: recognize the known captured `pairFact3.bin` ciphertext
   and return the already-known-correct 3×16-byte key material. No chip-internal
   algorithm needs to be invented or guessed.

What's left to actually build is: a small AT88SC command/zone state machine, the
GPA wire cipher (real implementation), and a data-backing layer combining (a) real
captured per-device secret bytes where we have them and (b) the pairFact3 fixture
lookup for loadmod's specific need.

## Scope: what OA.ko and loadmod.ko each actually need

| Consumer | Operation | Chip addresses touched | Status |
|---|---|---|---|
| `OA.ko` (`SetupAtmelForAuthorizations`, `src/auth/atmel_setup.cpp`) | GPA two-round mutual-auth handshake | zone `0x19` (7 bytes, config), zone `0x50` (8 bytes, IV/nonce), zones `0x10`/`0x18`/`0x20` (24 bytes, per-device secret used by the EX auth-string algorithm) | **Fully captured.** Per-device secret (`0x10`/`0x18`/`0x20`) via `KronosExtract.bin`'s `exs_zone`/`pf3_zone` (`0x00-0x27`); zones `0x19`/`0x50` via the SAME file's `cfg[128]` field (`read_config_zone()` in `kronos_extract.c` covers the full 128-byte config zone) — see Open Items #1 for the exact correspondence. |
| `loadmod.ko` (`RetrieveSecurityICKey`, per `docs/crypto/cryptoloop_keys.md`) | Send 80-byte `/.pairFact3` blob, get back 3×16-byte volume keys | opaque chip-internal op, addresses not characterized | Not needed — see finding 3 above. Output keys already known and universal: `a336a15cd841ec8926b99e7c3884eaa\x00` (Mod), `342ee59d549c7d329d835537be0540d` (Eva, needs the same trailing-null-quirk check), `3e72c0e59fc017a9eb7d7e1168a4cdb` (WaveMotion) — see `cryptoloop_keys.md` for the exact 31-hex-char + `\x00` key format quirk. |
| `GetPubIdMod.ko` | Reads Public ID (low chip addresses) | Public ID region | Not yet characterized here; lower priority (not on OA.ko's or loadmod.ko's critical boot path). |

## Open items (confirm before/while implementing — certainty over speed)

1. ~~Zones `0x19`/`0x50` are not in any existing capture.~~ **RESOLVED —
   they are.** `KronosExtract/source/kronos_extract.c`'s `read_config_zone()`
   reads the *entire* 128-byte AT88 config zone via `$B6` reads (addr
   `0,8,16,...,120`, 8 bytes each) into `s_cfg[128]`, which is exactly the
   `cfg[128]` field `KronosExtract.bin` stores. That range covers both:
   `0x19` (7 bytes, the chunk at `i=0x18`; `kronos_extract.c` itself reads
   this as `IdN = cfg[0x19..0x1f]`) and `0x50` (8 bytes, the chunk at
   `i=0x50` exactly; `kronos_extract.c` independently uses `p3 =
   cfg[0x50..0x57]` for its own unrelated purpose, corroborating that this
   is the real address, not a guess). `atmel_setup.cpp`'s own reconstructed
   comments call these the "config zone" and "IV zone" reads at these exact
   addresses, matching `kronos_extract.c`'s terminology and address range.
   **Fully confirmed, not just inferred from naming**: `cm_ReadUserZone` is
   the real OA.ko symbol `fFfFfFfFfFfF1C` (`.text+0x4f4a80`, resolved via
   `SetupAtmelForAuthorizations`'s own call-site relocation) — disassembling
   it shows `mov BYTE PTR [esp+0x2c], 0xb6` as its command-opcode byte, the
   exact same `$B6` opcode `kronos_extract.c`'s `read_config_zone()` uses.
   Same command, same address space, same data. Closed.
2. **Whether `0x19`/`0x50` are per-device secret or fixed/universal** is unknown.
   If they turn out to be fixed across units (plausible, given the GPA cipher
   itself uses no per-device key), a single captured sample would be sufficient
   for *any* unit, not just the one it came from — worth explicitly testing this
   by comparing captures across units if more than one becomes available.
3. **`GetPubIdMod.ko`'s needs** haven't been characterized against this design yet
   — worth a pass once the OA.ko/loadmod.ko path is solid, if `GetPubIdMod.ko` is
   also in the VM's boot-critical path.
4. **The `pairFact3.bin` fixture match must be exact-input-keyed** (or the
   emulator should recognize a fixed/known-good pairFact3 blob shape rather than
   pattern-match) — if a VM environment's `/.pairFact3` ever differs from the
   captured `pairFact3.bin` (e.g. a different image entirely), the fixture won't
   fire and `loadmod.ko` will get a wrong/undefined answer. Simplest mitigation:
   ship the VM's own `/.pairFact3` as the exact captured `pairFact3.bin`, so the
   fixture is guaranteed to match by construction.
5. ~~**`$B0` (write) is entirely unimplemented — falls into `stgNV2AC_sync_cmd`'s
   silent-no-op default case.**~~ **RESOLVED 2026-07-16.** Found the same day
   while tracing `KRONOS_V06R06.VSB` (the NKS4 panel board's own firmware —
   see `kronosology/docs/modules/KRONOS_V06R06.VSB.md`): that firmware's
   `CryptoAt88.cpp` runs a self-test that `$B4`-selects zone 0, `$B0`-writes a
   known 16-byte pattern to address 0, `$B2`-reads it back, and calls a fatal
   assert-and-hang handler (`do {} while(true)` after drawing an error screen —
   the same code path behind `"SYSTEM STARTUP FAILED"`) if the two don't match.

   **Fix**: added `at88_chip_write_zone0()` (`chip_state.cpp`) and wired `$B0`
   into `stgNV2AC_sync_cmd`'s dispatch (`nv2ac_exports.cpp`), inferring the
   wire packing by extending this file's existing $B8-style "one buffer
   pointer + one total length" convention: `{0xb0, 0x00, addr, len}` header
   followed by `len` payload bytes in the same buffer. Writes are always raw
   (no DEAX), matching the self-test's own visible behavior (it builds its
   pattern and writes it with no encrypt step in between) — deliberately NOT
   extended to a hypothetical authenticated-write path, since nothing in this
   project's scope ever issues a post-handshake `$B0` and there is no ground
   truth to build one from.

   This alone wasn't enough to make the self-test's round trip work, though:
   `at88_chip_read_zone0()`'s `$B2` dispatch was (still is, for zone routing —
   see below) unconditionally DEAX-encrypted, and a chip that always requires
   a live `$B8` session for zone 0 couldn't pass its own factory self-test
   before that session ever exists. Reconciled by gating `$B2` on
   `chip->b8RoundsAccepted`: `< 2` (no session yet, e.g. a freshly-loaded
   chip) reads raw, matching the self-test's own no-`$B8`-visible behavior;
   `>= 2` (post-handshake) keeps the exact prior DEAX-encrypted behavior,
   which is the only branch `OA.ko`'s real call sequence ever reaches (it
   always completes both rounds before its first `$B2`) — so this is
   behavior-preserving for every existing OA.ko-facing test, confirmed by the
   full suite staying green (one pre-existing test, `test_chip_state.cpp`'s
   zone0 self-consistency check, needed one line — explicitly setting
   `b8RoundsAccepted = 2` — to keep reaching the encrypted branch it was
   always meant to exercise). New KAT coverage reproduces the panel
   firmware's exact scenario end to end, both at the `chip_state.cpp` level
   and through the real exported `stgNV2AC_sync_cmd`/`nv2ac_read_cmd_impl`
   symbols, plus a check that post-auth reads of the same written pattern
   come back different (proving the gate has a real effect, not just two
   branches that happen to agree).

   **Deliberately still open, not touched by this fix**: `$B2`'s dispatch
   remains unconditional on zone 0 regardless of `g_chip.selectedZone` —
   consistent with today's stated scope ("only zone 0 is ever emulated"), but
   the panel firmware's zone-select call is explicit and structural on its
   side of the wire, not vestigial. Also open: whether a real chip's `$B0`
   is *always* raw regardless of auth state, or only pre-lock (this emulator
   currently implements the former, the simpler of the two, since nothing in
   scope distinguishes them) — see `at88_chip.h`'s `at88_chip_write_zone0()`
   doc comment.
6. **`useEightStepAac` (b8_handshake.cpp's real `$B8` lockout, added 2026-07-13)
   defaults to the 4-attempt decay sequence** (`$FF,$EE,$CC,$88,$00`) per the
   Atmel CryptoMemory datasheet's stated default ("ETA=1"). The real Kronos
   chip's actual ETA config bit has not been characterized from a live capture —
   if it turns out to be the 8-attempt config instead, set `useEightStepAac = 1`
   on the loaded `AT88ChipState` (or thread it through the `KronosExtract.bin`
   loader once that config bit's location in the captured config zone is
   identified). Doesn't affect the AT88 relay's correctness either way, only how
   many consecutive failed `$B8` attempts it takes to lock — see
   `verify/test_aac_lockout.cpp`.

## Planned layout

```
AT88VirtualChip/
  README.md            this file
  at88_chip.h           shared zone/state-machine types
  gpa_cipher.cpp/.h      the GPA wire cipher (real implementation, fixed key)
  chip_state.cpp/.h      zone storage, zone-select/read state machine, secret data
  pairfact_fixture.cpp   one known /.pairFact3 blob's key material (superseded
                         by pairfact_decrypt.cpp below for anything general)
  pairfact_decrypt.cpp   GENERAL .pairFact3/.reauth decrypt (host-test-only,
                         reuses reconstructed/OA's moancjsd82 + md5)
  nv2ac_exports.cpp      stgNV2AC_sync_cmd / stgNV2AC_sync_read_cmd (the two real
                         exported symbols; matches OmapNKS4Module.ko's ABI exactly
                         so OA.ko/loadmod.ko link against this module unmodified)
  Makefile
```

## Related tooling already in the repo (found while checking captured-data
coverage — worth using during implementation/validation, not just `KronosExtract/`)

- `KronosExtract/source/at88_sniffer.c` — hooks `stgNV2AC_sync_cmd` (the
  *command* side only, not responses) and logs every AT88 opcode/zone/length
  OA.ko issues to `dmesg`. Useful for confirming the exact live command
  sequence (and catching anything this design missed) against real hardware,
  though it doesn't capture response *data*.
- `ARCHIVE/chip_sniff_working/chip_sniff.c` (+ a captured
  `chip_sniff_ring.bin`) — a different, earlier tool: inline-hooks OA.ko's
  `moancjsd82` directly (not the raw chip commands) and ring-buffers every
  24-byte key+iv material passed through it. Useful cross-check for the GPA
  cipher/session-key material once the emulator is far enough along to
  compare against, but operates one layer up from the chip protocol itself.

## Status

**`chip_state.cpp` done and KAT-verified** (`at88_chip.h` / `chip_state.cpp` /
`verify/test_chip_state.cpp`, 6/6 checks passing). Covers:

- The DEAX/GPA stream cipher (`deax_init`/`deax_step`/`deax_compute_challenges`)
  — ported directly from `KronosExtract/source/kronos_extract.c`'s own
  already-hardware-validated implementation (not re-derived), with the bare
  file-scope globals it uses turned into an explicit `DeaxState` struct so
  more than one chip session can exist at once.
- Zone storage (`AT88ChipState`: 128-byte config zone + 40-byte Zone0 secret)
  loaded and validated straight from the real captured
  `KronosExtract/build/KronosExtract.bin` (magic + CRC-32 + the
  pf3_zone/exs_zone overlap cross-check, not just trusted blindly).
- `at88_chip_read_config()` — the unencrypted `$B6` config-zone read.
- `at88_chip_read_zone0()` — the DEAX-encrypted `$B2` Zone0 read, ported from
  `kronos_extract.c`'s `synth_zone0_read()` with the chip/host roles
  reversed (this side encrypts real plaintext; `kronos_extract.c` decrypts
  what a real chip sends — same cipher, same step order, opposite direction).

KAT approach used two independent kinds of ground truth on purpose (so a bug
in one can't hide behind the other): the *real* captured file (magic/CRC/zone
bytes checked against the actual 188-byte file on disk, not assumed), and a
from-scratch independent Python re-implementation of `deax_step`
(`verify/gen_deax_vectors.py`, written straight from `kronos_extract.c`, not
from this C++ port) for a 12-step vector. Also directly exercises encrypt/
decrypt round-trip symmetry: `at88_chip_read_zone0()` encrypts the real
captured Zone0 secret, and a second, independently-seeded `DeaxState` running
the mirrored host-side decrypt (matching `synth_zone0_read()`'s own step
order) recovers the exact original plaintext.

**`$B8` handshake done and KAT-verified** (`bignum.h`/`bignum.cpp`,
`b8_handshake.cpp`, `verify/test_b8_handshake.cpp`, 12/12 checks passing).
Covers:

- **`bignum.cpp`**: the 130-bit modular-exponentiation primitive
  (`synth_sdflkjsvnd2g`, deriving the GPA challenge seed "p2" from the
  chip's IdN) — ported directly from `kronos_extract.c`'s own
  `ke_bn5_*`/`ke_synth_sdflkjsvnd2g` (itself a from-scratch, GMP-free
  reimplementation of loadmod.ko's `sdflkjsvnd2g`, already confirmed
  against real hardware). No GMP dependency, deliberately — `STGGmp.ko` is
  a separate already-reconstructed module this chip emulator doesn't need.
- **`b8_handshake.cpp`**: `at88_chip_handle_b8()`, processing one `$B8`
  command by independently recomputing the expected Q (via
  `synth_sdflkjsvnd2g` + `deax_compute_challenges`) and comparing against
  the received Q. Confirmed real per-round zone numbers (round 1 = `0x00`
  "cipher", round 2 = `0x10` "encrypt", chained via round 1's `p5` output
  becoming round 2's `p2` input) match `SetupAtmelForAuthorizations`'s own
  comments exactly (`reconstructed/OA/src/auth/atmel_setup.cpp`). On
  acceptance, advances the persistent session cipher state by the
  confirmed 18-step post-`$B8` continuation
  (`at88_chip_post_b8_steps()`, ported from `kronos_extract.c`'s
  `synth_post_b8_steps()` — "critical for subsequent zone0 reads to
  decrypt correctly" per that file's own comment) and resets the AAC byte
  (`configZone[0x50]`) to `$FF` on accept, or steps it through the real
  datasheet-confirmed decay sequence (`$FF,$EE,$CC,$88,$00`, see
  `at88_chip.h`'s `useEightStepAac`) on reject. Once the AAC reaches `$00`
  the key set is locked for the rest of that `AT88ChipState`'s lifetime —
  every further `$B8` (even with a cryptographically correct Q) is
  rejected without evaluation, matching real hardware's own documented
  anti-brute-force lockout (Good Info's Atmel CryptoMemory datasheet
  §6.3.18) rather than the plain saturating ±1 counter this emulator used
  before 2026-07-13. See `verify/test_aac_lockout.cpp`.

**Honesty note on what's actually verified**: the bignum/p2 derivation IS
checked against an independent oracle (`verify/gen_bignum_vectors.py`, a
Python cross-check using the real captured IdN). The `$B8` **handshake
round-trip itself has no independent oracle** — there's no captured real
Nc/Q exchange to compare against — so its tests verify internal
consistency (the verifier's math matches its own challenge-generation
math, exactly mirroring what `kronos_extract.c`'s `synth_try()` does
against a real chip) rather than independent ground truth. The strongest
evidence available is the end-to-end test: after a self-consistent
handshake, the resulting session cipher state must still correctly
decrypt the **real captured Zone0 secret** — tying the (self-consistency-
only) handshake to the (real-data) zone reads. That test passes.

**`nv2ac_exports.cpp` done and KAT-verified** (`verify/test_nv2ac_exports.cpp`,
7/7 checks passing). The two real exported symbols, matching
`OmapNKS4Module/driver.cpp`'s own confirmed real signatures exactly:

```c
void stgNV2AC_sync_cmd(unsigned char *address, unsigned int data);
int  stgNV2AC_sync_read_cmd(int cmd4, int dest);
```

`stgNV2AC_sync_cmd` dispatches `$B4` (stores the selected zone; unused —
see below) and `$B8` (`at88_chip_handle_b8()`) by opcode byte.
`stgNV2AC_sync_read_cmd` dispatches `$B6` (`at88_chip_read_config()`) and
`$B2` (`at88_chip_read_zone0()`, using the chip's persistent session
cipher state). A single module-level `AT88ChipState` singleton holds all
state; `at88_chip_module_init()` loads it from a `KronosExtract.bin`-format
blob (the real kernel-integration question of *where that blob comes
from* — a module parameter path, an embedded data section — is out of
scope here, left for whatever wires this into a real `init_module`).

**A real ABI bug caught and fixed while writing the first test for this
file, not a design flaw in the exported signature itself**:
`stgNV2AC_sync_read_cmd`'s real signature packs pointers into `int`
parameters — lossless on the real 32-bit target (where `int` and a
pointer are both 32 bits), but the first test wrote for it truncated real
64-bit host pointers into 32-bit ints and back, corrupting them, and
segfaulted immediately. Fixed by splitting the exported function into a
thin ABI-matching wrapper plus a pointer-typed `nv2ac_read_cmd_impl()`
core that tests call directly — the same fix pattern this project has
used before for 32-bit-target/64-bit-host mismatches (see `at88_chip.h`'s
own struct-layout notes and the earlier `oa_engine.h` precedent). The real
kernel-module build is unaffected; only host testing needed the split.

`verify/test_nv2ac_exports.cpp` drives the module *only* through these two
symbols — the same interface OA.ko/loadmod.ko actually link against — and
exercises the complete real wire sequence end to end: `$B4` select, both
`$B8` rounds, a `$B6` IdN read, and a post-handshake `$B2` Zone0 read that
correctly decrypts to the real captured secret. Same honesty note as the
handshake tests applies to the `$B8` rounds here (self-consistency, no
independent Nc/Q oracle).

**`loadmod.ko` pairFact3 fixture done and KAT-verified**
(`pairfact_fixture.h`/`pairfact_fixture.cpp`,
`verify/test_pairfact_fixture.cpp`, 10/10 checks passing — 3 more added
2026-07-16 validating the full 16-byte keys, see below). Recognizes the one
known captured `/.pairFact3` blob (80 bytes, MD5
`817956d550647905828e115f9eae7a0e`) by exact byte match and returns raw
16-byte-per-volume key material for Mod/Eva/WaveMotion, reconstructed from
the already-confirmed-universal 31-char ASCII keys in
`docs/crypto/cryptoloop_keys.md` (recovered independently via
`LOOP_GET_STATUS64` on live hardware — nothing to do with reversing the
chip's actual tamper-resistant internal crypto, which remains and will
likely always remain opaque).

`hexencode_31char()` is a faithful port of that doc's documented
`HexEncode` quirk: loadmod.ko's real function only ever writes 31 of the
32 hex characters a full 16-byte encoding would produce, into a pre-zeroed
32-byte buffer, so the 16th raw byte's low nibble is provably never
emitted regardless of its true value (confirmed by that doc's own
side-by-side comparison of a public blog's wrong 32-char guess against
the real device's observed key). This let the raw key material be
reconstructed exactly from the known ASCII strings, with that one
never-emitted nibble set arbitrarily (0) since it's provably irrelevant —
and the KAT closes the loop by hex-encoding the reconstructed raw bytes
back through the same quirk and confirming the *exact* real
independently-recovered key comes out the other side, for all three
volumes.

**A real transcription bug caught by this KAT, not a logic bug**: the
first version had `0xcb` instead of `0xcd` for the WaveMotion key's 15th
byte (a manual hex-transcription slip while hand-splitting the 31-char
string into byte pairs) — the round-trip test failed immediately with the
mismatch, exactly the kind of error this test methodology exists to catch.

**Scoping note, corrected 2026-07-16**: this fixture is deliberately NOT
wired into `nv2ac_exports.cpp`'s opcode dispatch — but not because of an
unconfirmed wire format, as this note used to claim. There is no such wire
format to confirm: `/.pairFact3`/`.reauth` decryption is plain host-side
Blowfish-CFB-64 (`moancjsd82`, `p3=80` — see `oa_crypto.h`'s own header
comment, already documenting this before this fixture was written) keyed
by Zone0 data this project already reads via the ordinary authenticated
protocol. No special chip opcode moves an 80-byte blob anywhere. Confirmed
by direct reproduction against two real `.reauth` files (see
`pairfact_decrypt.h`/`pairfact_decrypt.cpp`, and
`docs/crypto/cryptoloop_keys.md`'s "`.reauth` IS `.pairFact3`" section).
It's *correctly* not wired into `nv2ac_exports.cpp` because it isn't chip
protocol at all — it's host-side logic (`loadmod.ko`'s job, not the AT88
chip's), so it doesn't belong in a chip-opcode dispatch table by
definition, not because of a missing piece of reverse engineering.

**Kernel-module scaffolding done** (`module_main.cpp`, plus a Kbuild
section added to the `Makefile`). Covers:

- **RTAI-safe deferred blob loading**: `AT88VirtualChipInit()` (the real
  `module_init`) does NOT call `filp_open` directly — this project's own
  documented RTAI constraint (`CLAUDE.md`'s "AT88 Chip Data Extraction"
  section, re-derived the hard way while building `kronos_extract.ko`)
  is that `filp_open` from `init_module` context fails outright on this
  kernel (2.6.32.11-korg + RTAI blocks the `GFP_KERNEL` allocation it
  needs), `create_proc_entry` fails silently the same way, and
  `kthread_run` starts but gets starved by RTAI. The only approach
  confirmed to actually work is a workqueue (`schedule_work`/
  `create_singlethread_workqueue`, full `GFP_KERNEL` available in worker
  context) — ported that exact pattern from `kronos_extract.c`'s own
  proven `INIT_WORK`/`queue_work` usage rather than re-discovering it.
  `AT88VirtualChipInit()` only sets up the workqueue and queues the real
  work; `load_chip_blob_work()` (running in workqueue context) does the
  actual `filp_open`/`vfs_read`/`at88_chip_module_init()`.
- **`blob_path` module parameter** (default `/korg/rw/KronosExtract.bin`,
  matching `CLAUDE.md`'s documented capture output location) rather than
  a hardcoded path — deliberately configurable since the whole point of
  this sub-project is VM/foreign-hardware boot testing (see
  `MASTER_REFERENCE.md` sec 10.17/10.18), where the blob may need to live
  somewhere other than the real device's path.
- **`EXPORT_SYMBOL(stgNV2AC_sync_cmd)` / `EXPORT_SYMBOL(stgNV2AC_sync_read_cmd)`**
  — the actual mechanism that lets OA.ko/loadmod.ko link against this
  module's symbols at `insmod` time, same as they do against the real
  `OmapNKS4Module.ko`.
- **A Kbuild section added to the existing `Makefile`**, guarded by the
  standard `ifneq ($(KERNELRELEASE),)` idiom so one Makefile serves both
  purposes: `make` (unset `KERNELRELEASE`) still does the host-testable
  object compiles + KAT suite as before; `make ko KDIR=...` invokes real
  Kbuild (`$(MAKE) -C $(KDIR) M=$(PWD) modules`), matching
  `OmapNKS4Module/Makefile`'s own confirmed build-flag convention exactly
  (`-mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics
  -fno-use-cxa-atexit -fpermissive`, plus the same custom `.cpp` compile
  rule that Kbuild convention needs).

**Honest scope boundary, not glossed over**: the real `.ko` build has NOT
been verified in this session. `KDIR` needs to point at the actual Kronos
2.6.32.11-korg kernel source tree with the RTAI patches applied and a
32-bit cross-toolchain. Note (2026-07-01): the dev environment moved off
Windows/WSL to native Linux; `gcc-multilib`/`libc6-dev-i386` are installed
on this host, and a *configured* (has `.config`) extracted kernel tree
already exists at `/home/share/linux-2.6.32.11` (also mirrored under
`KronosExtract/linux-2.6.32.11`) — so a real build may now be directly
possible here, unlike when this section was first written (which assumed
the old WSL-only workflow and only checked `/home/share/Korg_Kernel_src`'s
compressed tarballs). Not yet attempted or verified — flagged as the
natural next step rather than assumed to work.
`module_main.cpp`'s kernel API usage (`filp_open`/`vfs_read`/
`get_fs`/`set_fs`, `INIT_WORK`/`queue_work`, `module_param`) is copied
directly from `kronos_extract.c`'s own already-hardware-proven idioms
line-for-line rather than guessed, which is the strongest confidence
available short of an actual compile.

**Still open**: actually verifying the `.ko` build + a real/VM boot test
once a proper `KDIR` is available. The pairFact3/`.reauth` wiring item
that used to be listed here is resolved, not open — see `pairfact_decrypt.h`
(2026-07-16): there was no chip wire format to confirm, so there's nothing
to wire into `nv2ac_exports.cpp`. Every piece of chip-side protocol logic
itself — zone storage, `$B0`/`$B6`/`$B2` reads and writes, the full `$B8`
handshake, the two real exported symbols, the pairFact3 fixture, and the
general `.pairFact3`/`.reauth` decrypt — is complete and KAT-verified
(97/97 checks across all seven host test binaries, after Open Item #5's
`$B0` write support and the `pairfact_decrypt` correction, both
2026-07-16).
