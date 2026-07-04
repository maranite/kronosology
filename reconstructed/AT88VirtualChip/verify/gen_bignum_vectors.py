#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
gen_bignum_vectors.py - independent cross-check for bignum.cpp's
synth_sdflkjsvnd2g(), using Python's arbitrary-precision integers directly
(not a from-scratch bit-level reimplementation like gen_deax_vectors.py --
there's no need, Python ints make the modexp itself trivial to get right;
what this catches is a transcription bug in the FIXED CONSTANTS N1/N2/e0,
or in the IdN-packing/output-byte-selection steps, ported into bignum.cpp).

Uses the real captured IdN from KronosExtract.bin (cfg[0x19..0x1f] =
[REDACTED-PUBLIC-ID]) as its input, so the resulting p2 vector is grounded in
real hardware data, not an arbitrary made-up IdN.
"""

N1 = int("275870082984435801508285927170653268036")
N2 = int("275870082984435801541504784743492877417")


def synth_sdflkjsvnd2g(idn: bytes) -> bytes:
    assert len(idn) == 7
    m = int.from_bytes(idn[0:4], "little") | (int.from_bytes(idn[4:7], "little") << 32)
    m = (m * m)  # < 2^112 < N2, no reduction needed yet

    e = pow(2, 2176, N1)
    buf = bytearray(16)
    for outer in range(16):
        for inner in range(8):
            e = (e * 2) % N1
            result = pow(m, e, N2)
            if result & 1:
                buf[outer] |= (1 << inner)

    idxs = [1, 2, 3, 5, 7, 11, 13, 15]
    return bytes(buf[i] for i in idxs)


if __name__ == "__main__":
    # cfg[0x19..0x1f] from the real captured KronosExtract.bin
    idn = bytes.fromhex("[REDACTED-PUBLIC-ID]")
    p2 = synth_sdflkjsvnd2g(idn)
    print("idn:", idn.hex())
    print("p2: ", p2.hex())
