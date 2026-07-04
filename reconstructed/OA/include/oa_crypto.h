// SPDX-License-Identifier: GPL-2.0
/*
 * oa_crypto.h  -  shared keyed-decode primitives used across the auth cluster.
 *
 * All three symbols below are REAL, confirmed via relocation entries in the
 * unstripped OA.ko 3.2.1 ELF (not obfuscated placeholders, not paraphrase --
 * see MASTER_REFERENCE.md sec 8/10.1 for how this was verified: disassembling
 * ParseAuth/ParseAuths/VerifyAuthorizationString directly and cross-referencing
 * .rel.text against the symbol table).
 *
 * fFfFfFfFfFfF13(zone, len, buf) -- AT88 zone read wrapper (regparm(3):
 * EAX=zone, EDX=len, ECX=buf). Confirmed call sites always read zones 0x10,
 * 0x18, 0x20 (8 bytes each) as one 24-byte block:
 *   zone 0x10 + 0x18  (16 bytes) -> Blowfish key
 *   zone 0x20         (8 bytes)  -> Blowfish IV
 * matching the AT88 Zone0 layout confirmed hardware-verified via
 * Tools/expansion_tools/kronos_auth.py (Zone0[0x10..0x1F] key,
 * Zone0[0x20..0x27] iv). Both ParseAuths() and VerifyAuthorizationString() read
 * all three zones themselves, inline, every call -- there is no cached copy.
 * IMPORTANT CORRECTION vs. earlier notes: this means the AT88 dongle IS
 * required for the boot-time ParseAuths path, not just the runtime
 * VerifyAuthorizationString path -- OA.ko_auth.md's "does not require the
 * hardware dongle" claim for ParseAuths does not hold up against the
 * disassembly and should be treated as superseded.
 *
 * DecodeBytesFromAscii(out, asciiIn) -- decodes a 24-character auth-string
 * token, encoded as **Crockford Base-32**, into exactly 15 raw ciphertext
 * bytes (24 chars x 5 bits = 120 bits = 15 bytes exactly). Confirmed via
 * relocation at both call sites (ParseAuths' per-token decode and
 * VerifyAuthorizationString's single-string decode), and independently
 * confirmed as the primary, fully-worked-out source of truth in
 * `docs/confirmed findings/EXs_Auth_Algorithm.md` (this project's own prior
 * research, hardware-verified against a real K2-73 unit's AT88 chip_data,
 * 2026-04-17):
 *   alphabet:     0123456789ACDEFGHJKLMNPQRTUVWXYZ
 *   equivalences: B=8, O=0, I=1, S=5
 * The `-` characters in a displayed auth string (e.g. "QH55-08W6-Y03J-...")
 * are cosmetic grouping only, not part of the alphabet -- ParseAuths'
 * tokenizer already treats `-` as an allowed-but-ignorable token character.
 * Returns 0 on success; nonzero if the input is malformed or not exactly 24
 * valid characters (this is also where the ">= 0x18 ASCII bytes" gate
 * OA.ko_auth.md vaguely describes actually lives).
 *
 * moancjsd82(chipKeyMaterial, ciphertext, p3, plainOut) -- Blowfish-CFB-64
 * decode. Confirmed from its own disassembly (.text+0x4f5f00): treats its
 * first argument as a pointer into a 16-byte circular KEY region (S-box XOR
 * key schedule) -- and independently confirmed by
 * `docs/confirmed findings/OA_Auth_Analysis_Report.md`, which disassembled
 * moancjsd82's F-function directly and found **standard, unmodified Blowfish**
 * (P/S-box pi-digit initializers: P[0]=0x243F6A88, S[0]=0xD1310BA6, F(x) =
 * ((S1[a]+S2[b])^S3[c])+S4[d]) in **CFB-64** mode, big-endian block halves:
 *   L = (IV[0]<<24)|(IV[1]<<16)|(IV[2]<<8)|IV[3]; R = same over IV[4..7]
 *   per byte i: enc=blowfish_encrypt(IV) every 8th byte, refilling IV;
 *               plaintext[i] = ciphertext[i] ^ IV[i%8]; IV[i%8] = ciphertext[i]
 * `p3` is the expected plaintext length; OA.ko's ParseAuth calls it with
 * p3=15 (EXs-style entries), loadmod.ko's bbbbbbbba12 calls its own separate
 * copy with p3=80 (.pairFact3/.reauth). HARDWARE-VERIFIED end to end
 * (2026-04-19, Korg-issued EXs18/19/42 + self-generated EXs10/EXs110 all
 * authorize on real hardware; roundtrip also verified against a real K2-73
 * unit's chip_data in EXs_Auth_Algorithm.md, 2026-04-17) --
 * Tools/expansion_tools/kronos_auth.py: plaintext for the p3=15 case is
 * [rand8(8)][s_file_name_ascii(4)][md5_bytes_3,7,11(3)], where
 * s_file_name is read as a little-endian dword and appended verbatim to
 * `kOptionsPath` to form `/korg/rw/Options/<name>` (e.g. "S017").
 *
 * IMPLEMENTED (2026-07-01, Stage 2): `moancjsd82` (src/crypto/moancjsd82.cpp,
 * on top of a standard Blowfish port in src/crypto/blowfish.cpp -- P/S-box
 * constants extracted programmatically from this exact kernel tree's own
 * `linux-2.6.32.11/crypto/blowfish.c`, not retyped) and `DecodeBytesFromAscii`
 * (src/crypto/cb32.cpp). `fFfFfFfFfFfF13` remains a call contract only (a
 * real AT88 hardware I/O primitive from `OmapNKS4Module.ko`, out of scope for
 * a software reconstruction). Both crypto ports are verified by
 * `verify/test_crypto.cpp` against vectors anchored to independent tool
 * chains, not just self-consistency: `moancjsd82` against a real
 * hardware-extracted key/iv + a ciphertext produced and checked by a separate
 * C#/BouncyCastle implementation (itself checked against a Python reference);
 * `DecodeBytesFromAscii` against an independent from-scratch Python
 * implementation written during this pass. `make verify` runs both.
 *
 * See also `KronosExtract/source/kronos_extract.c` (a live hardware hook
 * module, hooks moancjsd82 directly): its trampoline documents the real
 * calling convention identically to the above (EAX=24-byte key+iv buffer
 * pointer, ECX=p3) -- independent, hardware-level corroboration of every
 * finding in this header.
 */

#ifndef OA_CRYPTO_H
#define OA_CRYPTO_H

/* Read `len` bytes of AT88 zone `zone` into `buf`. Returns 0 on success. */
extern "C" int fFfFfFfFfFfF13(unsigned int zone, unsigned int len, unsigned char *buf);

/*
 * Decode a whitespace-delimited ASCII auth-string token at `asciiIn` (cb32
 * encoding) into raw ciphertext bytes at `out`. Returns 0 on success; nonzero
 * if the input is malformed or too short (this is the real ">= 0x18 bytes"
 * gate, not a separate caller-side length check).
 */
extern "C" int DecodeBytesFromAscii(unsigned char *out, const char *asciiIn);

/*
 * Blowfish-CFB-64 decode `ciphertext` into exactly `p3` bytes of plaintext at
 * `plainOut`, keyed by the 24-byte AT88 chip material at `chipKeyMaterial`
 * (bytes 0..15 = key, 16..23 = iv). Returns the number of bytes actually
 * decoded (compare against `p3` to detect failure), or a negative value on
 * hard failure.
 */
extern "C" int moancjsd82(const unsigned char *chipKeyMaterial, const unsigned char *ciphertext,
			  unsigned int p3, unsigned char *plainOut);

#endif /* OA_CRYPTO_H */
