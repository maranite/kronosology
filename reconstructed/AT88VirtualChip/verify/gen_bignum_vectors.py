#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
gen_bignum_vectors.py - independent cross-check for bignum.cpp's
synth_sdflkjsvnd2g(), using Python's arbitrary-precision integers directly
(not a from-scratch bit-level reimplementation like gen_deax_vectors.py --
there's no need, Python ints make the modexp itself trivial to get right;
what this catches is a transcription bug in the FIXED CONSTANTS N1/N2/e0,
or in the IdN-packing/output-byte-selection steps, ported into bignum.cpp).

Originally used a real captured IdN as input; switched 2026-07-16 to a
synthetic IdN (private per-device info, not committed to this repo). The
oracle-cross-check property this script exists for -- does an independent
Python implementation of the modexp agree with bignum.cpp's C++ port --
doesn't depend on the IdN being real hardware data, only on both
implementations being fed the SAME input.
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
    # Synthetic IdN (not real device data) -- see module docstring.
    idn = bytes.fromhex("de00ad00be00ef")
    p2 = synth_sdflkjsvnd2g(idn)
    print("idn:", idn.hex())
    print("p2: ", p2.hex())
