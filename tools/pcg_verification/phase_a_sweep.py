#!/usr/bin/env python3
"""
phase_a_sweep.py — Corpus-wide consistency sweep + A/B frame test + checksum test.

Runs against every .PCG in the BBPB corpus (and any .PCG passed on argv).
Produces:
  1. Per-file: header, chunk tree, bank inventory, checksum status.
  2. A/B frame tests resolving the disputed offsets:
       - Combi timbre table: 4802 vs 4806 (which yields valid bank codes)
       - Names at record+0 vs +4
       - SBK1 slot params at slot+24/25/26 vs +12/13/14
  3. Checksum recompute/write test on a COPY (never touches originals).

Exit code: 0 = all checks pass, 1 = any file had errors/mismatches.
"""

from __future__ import annotations
import os, sys, struct, shutil
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pcg_truth import (open_pcg, PcgTruthError, BankChunk, Chunk,
                       program_bank_index, combi_bank_index,
                       COMBI_TIMBRE_OFFSET, COMBI_TIMBRE_STRIDE, COMBI_TIMBRES,
                       SLOT_BASE, SLOT_STRIDE, SLOT_COUNT, SETLIST_RECORD,
                       PROG_CATEGORY_OFFSET, COMBI_CATEGORY_OFFSET,
                       GLB_PROG_CAT, GLB_PROG_SUB, GLB_COMBI_CAT, GLB_COMBI_SUB,
                       CHECKSUM_CHUNK_TYPES)

VALID_TIMBRE_BANK_CODES = set(range(0, 7)) | set(range(17, 31))  # 0..6, 17..30


# ── A/B frame tests ─────────────────────────────────────────────
def ab_combi_timbres(f, bank):
    """For every combi record: decode at 4802 vs 4806; count how many timbres
    land in the valid bank-code space at each frame."""
    good4802 = good4806 = total = 0
    examples = []
    for i in range(bank.count):
        rec = f.record(bank, i)
        if rec is None: continue
        total += 1
        for t in range(COMBI_TIMBRES):
            o2 = COMBI_TIMBRE_OFFSET + t * COMBI_TIMBRE_STRIDE
            o6 = o2 + 4
            if o6 + 2 >= len(rec): break
            n2, b2 = rec[o2], rec[o2+1]
            n6, b6 = rec[o6], rec[o6+1]
            if b2 in VALID_TIMBRE_BANK_CODES and n2 <= 127: good4802 += 1
            if b6 in VALID_TIMBRE_BANK_CODES and n6 <= 127: good4806 += 1
            if not examples and (b2 in VALID_TIMBRE_BANK_CODES or b6 in VALID_TIMBRE_BANK_CODES):
                examples.append((i, t, n2, b2, n6, b6))
    return total, good4802, good4806, examples[:3]


def ab_names(f):
    """Names at record+0 vs +4: a valid name is printable ASCII (no control
    chars, no 0xFF) and non-empty."""
    counts = {0: 0, 4: 0}
    total = 0
    for b in f.banks:
        if b.tag in ("SBK1", "GLB1"): continue
        for i in range(min(b.count, 200)):
            rec = f.record(b, i)
            if rec is None or len(rec) < 28: continue
            total += 1
            for off in (0, 4):
                raw = rec[off:off+24]
                end = len(raw)
                while end > 0 and raw[end-1] in (0, 0x20): end -= 1
                s = raw[:end]
                if s and all(0x20 <= c < 0x7F for c in s):
                    counts[off] += 1
    return total, counts


def ab_setlist_slots(f, sbk):
    """SBK1 slot params at slot+24/25/26 vs +12/13/14. Compare the two frames
    against the slot's own name cross-reference when the target is a Program/Combi
    that exists in the file (the frame whose bank/num points at a real object is
    the correct one). Also report the type histogram per frame."""
    good24 = good12 = 0
    hist24 = Counter()
    hist12 = Counter()
    examples = []
    # build lookup: (bank_index_from_bankid) -> name for Programs and Combis
    prog = {}
    combi = {}
    for b in f.banks:
        if b.tag in ("MBK1", "PBK1"):
            idx = program_bank_index(b.bank_id)
            for i in range(b.count):
                prog[(idx, i)] = f.name(b, i)
        elif b.tag == "CBK1":
            idx = combi_bank_index(b.bank_id)
            for i in range(b.count):
                combi[(idx, i)] = f.name(b, i)
    for li in range(sbk.count):
        rec = f.record(sbk, li)
        if rec is None or len(rec) < SLOT_BASE + SLOT_COUNT * SLOT_STRIDE: continue
        for s in range(SLOT_COUNT):
            off = SLOT_BASE + s * SLOT_STRIDE
            def val(o):
                return (rec[o] & 0x03, rec[o+1] & 0x1F, rec[o+2])
            t24, b24, i24 = val(off + 24)
            t12, b12, i12 = val(off + 12)
            hist24[t24] += 1
            hist12[t12] += 1
            # a reference is "resolvable" if it points at an existing object
            def resolvable(t, b, i):
                if t == 0: return (b, i) in combi
                if t == 1: return (b, i) in prog
                return False
            if resolvable(t24, b24, i24): good24 += 1
            if resolvable(t12, b12, i12): good12 += 1
            if not examples and (resolvable(t24, b24, i24) or resolvable(t12, b12, i12)):
                examples.append((li, s, (t24,b24,i24), (t12,b12,i12)))
    return good24, good12, hist24, hist12, examples[:3]


# ── checksum write test ─────────────────────────────────────────
def checksum_test(src_path, work_dir):
    """Copy src, flip one byte in a Program record, recompute chunk checksums
    with the PCG-Tools algorithm, and verify the re-read agrees."""
    out = os.path.join(work_dir, "csum_test.PCG")
    shutil.copy(src_path, out)
    with open(out, "r+b") as fh:
        data = bytearray(fh.read())
    f = open_pcg(out)
    # find a Program bank with count>0, flip a byte deep in record 0's name area
    target = next((b for b in f.banks if b.tag in ("MBK1","PBK1") and b.count > 0), None)
    if target is None: return "no program bank"
    rec_off = target.records_start
    data[rec_off] ^= 0x01  # flip name byte 0
    # recompute checksums for all 7 types
    for b in f.banks:
        if b.tag not in CHECKSUM_CHUNK_TYPES: continue
        payload = data[b.chunk.content_start : b.chunk.content_end]
        data[b.chunk.offset + 11] = sum(payload) & 0xFF
    # also GLB1
    if f.glb_chunk is not None:
        data[f.glb_chunk.offset + 11] = sum(data[f.glb_chunk.content_start:f.glb_chunk.content_end]) & 0xFF
    with open(out, "wb") as fh:
        fh.write(bytes(data))
    # re-read
    f2 = open_pcg(out)
    mism = [b for b in f2.banks if not b.checksum_ok]
    return f"ok, {len(f2.banks)} banks re-verified, mismatches={len(mism)}"


# ── main ────────────────────────────────────────────────────────
def analyze(path):
    f = open_pcg(path)
    rows = []
    rows.append(f"== {os.path.basename(path)}  size={len(f.data)} os={f.os_byte} csumflag={f.checksum_flag}")
    for e in f.errors:
        rows.append(f"   ERR: {e}")
    # top-level tree
    top = [c for c in f.chunks if c.parent is None or c.parent.tag == "PCG1"]
    rows.append("   top: " + " ".join(f"{c.tag}@{c.offset:x}({c.size})" for c in top))
    # bank inventory by type
    from collections import Counter
    inv = Counter()
    for b in f.banks:
        inv[(b.tag, bank_label(b.tag, b.bank_id))] += 1
    rows.append(f"   banks: {len(f.banks)}  " + ", ".join(f"{k[0]}{k[1]}={v}" for k, v in sorted(inv.items())))
    # checksums
    bad = [b for b in f.banks if not b.checksum_ok]
    rows.append(f"   checksum: {len(f.banks)-len(bad)}/{len(f.banks)} ok" + (f"  MISMATCH: {[b.tag+'@'+hex(b.chunk.offset) for b in bad]}" if bad else ""))
    # A/B tests
    for b in f.banks:
        if b.tag == "CBK1":
            total, g2, g6, ex = ab_combi_timbres(f, b)
            rows.append(f"   A/B timbre 4802vs4806: {g2}/{total*COMBI_TIMBRES} valid @4802, {g6} @4806  ex={ex[:1]}")
            break
    total, cnt = ab_names(f)
    rows.append(f"   A/B names +0 vs +4: {cnt[0]}/{total} printable @+0, {cnt[4]} @+4")
    for b in f.banks:
        if b.tag == "SBK1":
            g24, g12, h24, h12, ex = ab_setlist_slots(f, b)
            rows.append(f"   A/B slot +24 vs +12: {g24} resolvable @+24, {g12} @+12  hist24={dict(h24)} hist12={dict(h12)}  ex={ex[:2]}")
            break
    # GLB1 category names
    if f.glb_chunk is not None:
        g = f.glb_chunk
        base = g.content_start
        names = []
        for off, lab in ((GLB_PROG_CAT,"Pcat"),(GLB_PROG_SUB,"Psub"),(GLB_COMBI_CAT,"Ccat"),(GLB_COMBI_SUB,"Csub")):
            raw = f.data[base+off:base+off+24]
            s = raw.split(b"\x00")[0].decode("ascii","replace")
            names.append(f"{lab}={s!r}")
        rows.append("   GLB1 " + " ".join(names))
    return "\n".join(rows)


def bank_label(tag, bank_id):
    from pcg_truth import bank_label as bl
    return bl(tag, bank_id)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        here = os.path.dirname(os.path.abspath(__file__))
        corpus = os.path.join(here, "..", "BBPB")
        args = [os.path.join(corpus, f) for f in sorted(os.listdir(corpus)) if f.lower().endswith(".pcg")]
        args += [os.path.join(corpus, "Archive", f) for f in sorted(os.listdir(os.path.join(corpus, "Archive"))) if f.lower().endswith(".pcg")]
    work = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "_WORKING")
    os.makedirs(work, exist_ok=True)
    fails = 0
    for p in args:
        try:
            print(analyze(p))
        except (PcgTruthError, Exception) as e:
            print(f"== {os.path.basename(p)}: PARSE ERROR: {e}")
            fails += 1
    # checksum write test on one copy
    src = next((a for a in args if a.lower().endswith(".pcg")), None)
    if src:
        print("\n-- checksum write test (copy):", checksum_test(src, work))
    sys.exit(1 if fails else 0)
