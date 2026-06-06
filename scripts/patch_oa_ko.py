#!/usr/bin/env python3
"""
Apply the canonical "bypass everything" patch set to a stock Kronos OA.ko.

This produces a patched OA.ko that:
  - Skips the loadmod.ko magic-value check in CSTGEngine::Initialize (one
    primary branch flip plus four inlined copies of the same check) and
    NOPs the associated audio-degradation stores
  - Returns "no unauthorized samples" from all five
    CSTG*ModelPatch::IsUsingAnyUnauthorizedMultisamples specializations
    (two compiled into main .text, four as COMDAT sections)

11 patch runs total, 369 bytes. Patches are addressed by ELF section name +
section-relative offset, so the same patch table works on any firmware version
that hasn't reorganized these functions. Verified end-to-end on:

  3.2.1: 955636c2b11a70a1dbecefaaa7bd4f80 → 163550b60b7508b2c0ba1fd314b0b944
         (matches the canonical patched binary byte-for-byte)
  3.2.2: 39fec7465fd7886ed8099c9cb85e2cdc → e585d8ebb471a41ab6d9f77c88368bfe

The patch table was derived 2026-06-04 by diffing the canonical patched 3.2.1
OA.ko against stock 3.2.1, and verified to land at byte-identical original
content in stock 3.2.2.

Usage:
  python3 patch_oa_ko.py <stock_OA.ko> <output_patched_OA.ko>
  python3 patch_oa_ko.py --verify <patched_OA.ko>   # check whether already patched

Requires: readelf (binutils / e2fsprogs do NOT contain it; use binutils).
"""
import hashlib
import re
import subprocess
import sys


# ──────────────────────────────────────────────────────────────────────────
# Canonical patch table.
# Each entry: (section_name, section_relative_offset, original_hex, patched_hex)
# For .text patches, section_offset is just (file_off - 0xB390) for the
# 3.2.1/3.2.2 layout. For COMDAT patches the offset is into that COMDAT section.
# ──────────────────────────────────────────────────────────────────────────

PATCHES = [
    # CSTGEngine::Initialize — primary site
    (".text", 0x008b4,
        "85c0745ac705000000003333333fc70500000000cdcc4cbec705040000003333333f"
        "c70504000000cdcc4cbec705080000003333333fc70508000000cdcc4cbec7050c00"
        "00003333333fc7050c000000cdcc4cbec705000000001f000000",
        "eb5c909090900000000090909090909000000000909090909090040000009090909"
        "0909004000000909090909090080000009090909090900800000090909090909"
        "00c0000009090909090900c0000009090909090900000000090909090"),

    # Continuation of degradation block at the same site — gap-grouped separately
    (".text", 0x00ab3,
        "c705000000003333333fc70500000000cdcc4cbec705040000003333333fc70504"
        "000000cdcc4cbec705080000003333333fc70508000000cdcc4cbec7050c000000"
        "3333333fc7050c000000cdcc4cbec705000000001f000000",
        "9090000000009090909090900000000090909090909004000000909090909090"
        "04000000909090909090080000009090909090900800000090909090909"
        "00c0000009090909090900c0000009090909090900000000090909090"),

    # Three inlined copies of the same magic-value check + degradation block
    (".text", 0x04db3,
        "7409817805cc39fb22745ac705000000003333333fc70500000000cdcc4cbec705"
        "040000003333333fc70504000000cdcc4cbec705080000003333333fc70508000000"
        "cdcc4cbec7050c0000003333333fc7050c000000cdcc4cbec705000000001f000000",
        "eb63909090909090909090909000000000909090909090000000009090909090900"
        "4000000909090909090040000009090909090900800000090909090909008000000"
        "9090909090900c0000009090909090900c0000009090909090900000000090909090"),

    (".text", 0x05ebe,
        "7409817805cc39fb22745ac705000000003333333fc70500000000cdcc4cbec705"
        "040000003333333fc70504000000cdcc4cbec705080000003333333fc70508000000"
        "cdcc4cbec7050c0000003333333fc7050c000000cdcc4cbec705000000001f000000",
        "eb63909090909090909090909000000000909090909090000000009090909090900"
        "4000000909090909090040000009090909090900800000090909090909008000000"
        "9090909090900c0000009090909090900c0000009090909090900000000090909090"),

    (".text", 0x064a8,
        "7409817805cc39fb22745ac705000000003333333fc70500000000cdcc4cbec705"
        "040000003333333fc70504000000cdcc4cbec705080000003333333fc70508000000"
        "cdcc4cbec7050c0000003333333fc7050c000000cdcc4cbec705000000001f000000",
        "eb63909090909090909090909000000000909090909090000000009090909090900"
        "4000000909090909090040000009090909090900800000090909090909008000000"
        "9090909090900c0000009090909090900c0000009090909090900000000090909090"),

    # Two IsUsingAnyUnauthorizedMultisamples specializations compiled into main .text
    (".text", 0x13c7d0,
        "8b40088b52280fbf40088b44023485c00f95c0",
        "31c0c390909090909090909090909090909090"),

    (".text", 0x155d30,
        "8b40088b52280fbf40088b44020485c00f95c0",
        "31c0c390909090909090909090909090909090"),

    # Four IsUsingAnyUnauthorizedMultisamples specializations in COMDAT sections.
    # Section names + section-relative offset; works regardless of file-offset
    # shifts caused by firmware updates.
    (".text._ZN17CSTGPCMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b92d0010000", "31c0c3909090"),

    (".text._ZN21CSTGPluckedModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b9278030000", "31c0c3909090"),

    (".text._ZN17CSTGVPMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b9225040000", "31c0c3909090"),

    (".text._ZN19CSTGPianoModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b92ac000000", "31c0c3909090"),
]

KNOWN_STOCK_MD5 = {
    "955636c2b11a70a1dbecefaaa7bd4f80": "3.2.1 stock",
    "39fec7465fd7886ed8099c9cb85e2cdc": "3.2.2 stock",
}

KNOWN_PATCHED_MD5 = {
    "163550b60b7508b2c0ba1fd314b0b944": "3.2.1 canonical patched (matches reference exactly)",
    "e585d8ebb471a41ab6d9f77c88368bfe": "3.2.2 patched",
}


def list_progbits_sections(path: str) -> dict[str, tuple[int, int]]:
    out = subprocess.run(["readelf", "-S", "-W", path], capture_output=True, text=True).stdout
    sections = {}
    for line in out.splitlines():
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+PROGBITS\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)', line)
        if m:
            sections[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return sections


def file_md5(path: str) -> str:
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def apply_patches(target: str, output: str) -> int:
    target_md5 = file_md5(target)
    label = KNOWN_STOCK_MD5.get(target_md5) or KNOWN_PATCHED_MD5.get(target_md5) or "unrecognised"
    print(f"Target: {target}")
    print(f"  MD5  : {target_md5}  ({label})")
    print()

    data = bytearray(open(target, "rb").read())
    sections = list_progbits_sections(target)

    applied = already = 0
    for n, (name, rel, orig_hex, pat_hex) in enumerate(PATCHES, 1):
        orig = bytes.fromhex(orig_hex)
        pat = bytes.fromhex(pat_hex)
        if len(orig) != len(pat):
            print(f"  [{n:2d}] BUG: original/patched hex length mismatch in patch table")
            return 2
        if name not in sections:
            print(f"  [{n:2d}] section {name!r} not found in target — refusing to patch")
            return 2
        sh_off, sh_sz = sections[name]
        if rel + len(orig) > sh_sz:
            print(f"  [{n:2d}] patch extends past end of section {name!r}")
            return 2
        file_off = sh_off + rel
        cur = bytes(data[file_off:file_off + len(orig)])
        tag = name if len(name) < 60 else name[:57] + "..."
        if cur == pat:
            print(f"  [{n:2d}] {tag:60s} +0x{rel:05x}  ALREADY PATCHED")
            already += 1
            continue
        if cur != orig:
            print(f"  [{n:2d}] {tag:60s} +0x{rel:05x}  UNEXPECTED bytes — REFUSING")
            print(f"        expected: {orig.hex()}")
            print(f"        found   : {cur.hex()}")
            return 2
        data[file_off:file_off + len(pat)] = pat
        print(f"  [{n:2d}] {tag:60s} +0x{rel:05x}  patched ({len(pat)} bytes)")
        applied += 1

    with open(output, "wb") as f:
        f.write(bytes(data))
    out_md5 = hashlib.md5(bytes(data)).hexdigest()
    print()
    print(f"Wrote {output}")
    print(f"  MD5: {out_md5}")
    expected = KNOWN_PATCHED_MD5.get(out_md5)
    if expected:
        print(f"        ✓ matches known {expected}")
    elif target_md5 in KNOWN_STOCK_MD5:
        print(f"        (new patched MD5 — record this in KNOWN_PATCHED_MD5 if you'll reuse it)")
    print(f"  Applied {applied} new patch(es), {already} were already in place")
    return 0


def verify_only(target: str) -> int:
    md5 = file_md5(target)
    print(f"Target: {target}")
    print(f"  MD5  : {md5}")
    if md5 in KNOWN_STOCK_MD5:
        print(f"        UNPATCHED — recognised as {KNOWN_STOCK_MD5[md5]}")
        return 1
    if md5 in KNOWN_PATCHED_MD5:
        print(f"        PATCHED — recognised as {KNOWN_PATCHED_MD5[md5]}")
        return 0
    # Walk patches and check current state
    data = open(target, "rb").read()
    sections = list_progbits_sections(target)
    stock = patched = other = 0
    for name, rel, orig_hex, pat_hex in PATCHES:
        if name not in sections:
            other += 1; continue
        sh_off, _ = sections[name]
        cur = data[sh_off + rel:sh_off + rel + len(bytes.fromhex(orig_hex))]
        if cur == bytes.fromhex(pat_hex):
            patched += 1
        elif cur == bytes.fromhex(orig_hex):
            stock += 1
        else:
            other += 1
    print(f"        Mixed / unknown — {stock} stock, {patched} patched, {other} unexpected (of {len(PATCHES)})")
    return 0 if patched == len(PATCHES) else 1


def main() -> int:
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "--verify":
        return verify_only(args[1])
    if len(args) != 2:
        print(__doc__)
        return 1
    return apply_patches(args[0], args[1])


if __name__ == "__main__":
    sys.exit(main())
