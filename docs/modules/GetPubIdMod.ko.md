# GetPubIdMod.ko — Atmel NV2AC Chip Interface

The kernel module that derives the per-device **Public ID** from the Atmel NV2AC
authentication chip soldered to every Kronos motherboard. This Public ID is the
cryptographic identity of the device — `loadmod.ko` uses it to derive `pairFact`,
and `OA.ko` indirectly relies on the same chip's secret state for EX authorisation.

| Property | Value |
|---|---|
| Path on device | `/sbin/GetPubIdMod.ko` |
| Source path | `dump from kronos/sbin/GetPubIdMod.ko` |
| Architecture | x86 LE 32-bit kernel module (ET_REL) |
| Size | ~12 KB (tiny) |
| Functions | 23 (per `nm`) |
| C++ symbols | none — pure C, source files named `GetPubId2.c`, `GetPubIdMod.mod.c` |
| Status | **Fully studied in prior sessions** — see auto-memory `getpubidmod_analysis.md` |

---

## The Atmel NV2AC chip

The Kronos uses an **Atmel NV2AC** secure authentication chip (a tiny crypto-IC family
with non-volatile memory and a challenge-response interface). It:

- Is read over a custom serial/parallel protocol via OMAP GPIO/CPLD (the OmapNKS4 hardware)
- Has a public, readable region holding the Public ID
- Has a secret region (addresses `0x10`, `0x18`, `0x20` — 24 bytes total) that is read
  through an authenticated/encrypted protocol
- Communicates via a custom stream cipher — the **GPA cipher** — that `GetPubIdMod.ko`
  implements

The chip is one of three pillars of Kronos device identity:

1. Public ID (readable by anyone via this module)
2. Secret data at `0x10`/`0x18`/`0x20` (readable only by authenticated commands — used
   by `OA.ko` for the EX authorisation algorithm, see
   [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md))
3. The fact that the chip exists and responds at all (boot integrity)

---

## Exported symbols

`GetPubIdMod.ko` uses `stgNV2AC_sync_cmd` and `stgNV2AC_sync_read_cmd` — these are
**exported by `OmapNKS4Module.ko`** (not by GetPubIdMod itself; see
[`OmapNKS4Module.ko.md`](OmapNKS4Module.ko.md)). GetPubIdMod is the consumer of the
low-level primitives.

`loadmod.ko` and `OA.ko` are also consumers of `stgNV2AC_sync_*` for their own purposes.

---

## The GPA stream cipher

Custom Korg stream cipher (not a standard one). Used to encrypt the chip-protocol
payloads in both directions. Fully studied — the algorithm and the constants
are documented in `getpubidmod_analysis.md` (in `.claude/memory/`). The cipher is
self-contained — no per-device key needed, just a known constant key.

This means: in principle, a userspace tool **could** reimplement the GPA cipher and talk
to the chip directly via port I/O (with root + `iopl`). The chip is on the OMAP side
of the system, accessible via the NKS4 hardware. This is one of the three options
considered for [auth-string generation in userspace](../crypto/auth_string_algorithm.md#userspace-access-options).

---

## Role for our project

Most relevant to:

1. **EX authorisation** — generating valid auth strings requires the 24-byte chip secret,
   which is read via `stgNV2AC_sync_read_cmd`. A small helper kernel module
   (`oa_authgen.ko`) could expose that secret to userspace (see
   [`../crypto/auth_string_algorithm.md`](../crypto/auth_string_algorithm.md)).
2. **Understanding Public ID** — if you're trying to make a Kronos boot on hardware
   with a *failed* chip, you'd need to spoof the GPA-cipher responses.

---

## Status

Fully studied in earlier sessions (see auto-memory). No work was done on this
module during the current campaign; it's complete from prior analysis.
