#!/usr/bin/env python3
"""
phase_a_deep.py — Deep structural probes for the never-before-decoded object
types + reference-graph extraction, run against the BBPB corpus.

Covers (Phase A items A6/A7):
  * Drum Kit (DBK1): 24-byte name, then 128 notes x 300 bytes;
    8 zones x 34, 7 inter-zone crossfade blocks x3, 7-byte note trailer.
    Zone: +0 sample on/off, +1..+15 Sample Bank UUID, +18..19 Sample Id (LE),
    +20 level, etc.
  * Wave Sequence (WBK1): 24-byte name, 16-byte common, 64 steps x 34.
    Step: +0 type (0=MS,1=Rest,2=Tie), +1..+15 Bank Select UUID,
    +18..19 Multisample Select (LE).
  * Drum Track Patterns (DPI1): DPN1 / DPD1 / DPS1 (+DPV1) skeleton.
  * Reference graph: Combi timbre -> Program, SetList slot -> Program/Combi,
    with a "no dangling references" invariant check.
"""

from __future__ import annotations
import os, sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pcg_truth import (open_pcg, BankChunk,
                       program_bank_index, combi_bank_index,
                       COMBI_TIMBRE_OFFSET, COMBI_TIMBRE_STRIDE, COMBI_TIMBRES,
                       SLOT_BASE, SLOT_STRIDE, SLOT_COUNT, SETLIST_RECORD)

LEGACY_UUID_PREFIX = bytes.fromhex("4b4f5247" + "00"*8 + "4d5300")  # KORG + 8 zeros + MS + 0


# ── Drum Kit ───────────────────────────────────────────────────
def probe_drum_kit(f, bank: BankChunk):
    """Return (name, uuid_samples, sample_id_hist, note_count)."""
    rec = f.record(bank, 0)
    if rec is None or len(rec) < 24 + 128*300:
        return None
    name = rec[0:24].split(b"\x00")[0].decode("ascii", "replace")
    # note 0, zone 0..7
    uuids = []
    ids = []
    for z in range(8):
        zoff = 24 + z*34
        if zoff + 20 > len(rec): break
        on = rec[zoff]
        uuid = rec[zoff+1:zoff+16]
        sid = rec[zoff+18] | (rec[zoff+19] << 8)
        uuids.append(uuid)
        ids.append(sid)
    legacy = sum(1 for u in uuids if u.startswith(LEGACY_UUID_PREFIX))
    return name, legacy, ids


# ── Wave Sequence ──────────────────────────────────────────────
def probe_wave_seq(f, bank: BankChunk):
    rec = f.record(bank, 0)
    if rec is None or len(rec) < 24 + 16 + 64*34:
        return None
    name = rec[0:24].split(b"\x00")[0].decode("ascii", "replace")
    steps = []
    for s in range(64):
        soff = 24 + 16 + s*34
        stype = rec[soff] & 0x03
        uuid = rec[soff+1:soff+16]
        ms = rec[soff+18] | (rec[soff+19] << 8)
        steps.append((stype, ms, uuid.startswith(LEGACY_UUID_PREFIX)))
    type_hist = Counter(s[0] for s in steps)
    legacy = sum(1 for s in steps if s[2])
    return name, type_hist, legacy, steps[:3]


# ── DPI1 ───────────────────────────────────────────────────────
def probe_dpi(f):
    dpi = next((c for c in f.chunks if c.tag == "DPI1"), None)
    if dpi is None: return None
    out = []
    pos = dpi.content_start
    while pos + 12 <= dpi.content_end:
        tag = f.data[pos:pos+4].decode("ascii", "replace")
        size = int.from_bytes(f.data[pos+4:pos+8], "big")
        out.append((tag, size))
        pos += 12 + size
    return out


# ── Reference graph ────────────────────────────────────────────
def reference_graph(f):
    """Combi timbre -> (prog bank idx, num); SetList slot -> (type, bank, num).
    Returns (combi_refs, setlist_refs, dangling)."""
    prog_names = {}
    combi_names = {}
    for b in f.banks:
        if b.tag in ("MBK1", "PBK1"):
            idx = program_bank_index(b.bank_id)
            for i in range(b.count):
                prog_names[(idx, i)] = f.name(b, i)
        elif b.tag == "CBK1":
            idx = combi_bank_index(b.bank_id)
            for i in range(b.count):
                combi_names[(idx, i)] = f.name(b, i)

    combi_refs = []
    dangling_combi = 0
    for b in f.banks:
        if b.tag != "CBK1": continue
        cidx = combi_bank_index(b.bank_id)
        for i in range(b.count):
            for t in f.combi_timbres(b, i):
                if t.bank in range(0, 7):      # INT bank: raw code == file index
                    target = (t.bank, t.number)
                    exists = target in prog_names
                elif 17 <= t.bank <= 30:       # USER bank: raw code 17..30 -> file idx 6..19
                    target = (t.bank - 11, t.number)
                    exists = target in prog_names
                else:
                    exists = True  # GM/g codes are universal
                combi_refs.append((cidx, i, t.number, t.bank, t.status_name, exists))
                if not exists: dangling_combi += 1

    setlist_refs = []
    dangling_sl = 0
    for b in f.banks:
        if b.tag != "SBK1": continue
        for li in range(b.count):
            rec = f.record(b, li)
            if rec is None: continue
            for s in range(SLOT_COUNT):
                off = SLOT_BASE + s*SLOT_STRIDE
                st = rec[off+24] & 0x03
                bk = rec[off+25] & 0x1F
                num = rec[off+26]
                if st == 1:      # Program — slot bank byte is FUNC33 code:
                #   0..5 INT, 6=GM, 7..16 g(1..d), 17..30 USER (17->file 6..19)
                    if bk <= 5:
                        exists = (bk, num) in prog_names
                    elif 17 <= bk <= 30:
                        exists = (bk - 11, num) in prog_names
                    else:
                        exists = True  # GM/g universal
                elif st == 0:    # Combi
                    if bk <= 6:
                        exists = (bk, num) in combi_names
                    elif 17 <= bk <= 23:
                        exists = (bk - 10, num) in combi_names
                    else:
                        exists = True
                else:
                    exists = True  # Song slots point at a song, not a program
                setlist_refs.append((li, s, st, bk, num, exists))
                if not exists: dangling_sl += 1
    return combi_refs, setlist_refs, dangling_combi, dangling_sl


def analyze(path):
    f = open_pcg(path)
    rows = [f"== {os.path.basename(path)}"]
    # drum kit
    for b in f.banks:
        if b.tag == "DBK1":
            r = probe_drum_kit(f, b)
            if r:
                name, legacy, ids = r
                rows.append(f"   DK[{b.bank_id:x}] '{name}' zones: legacy_uuid={legacy}/8 sample_ids={ids[:4]}")
            break
    # wave seq
    for b in f.banks:
        if b.tag == "WBK1":
            r = probe_wave_seq(f, b)
            if r:
                name, th, legacy, st = r
                rows.append(f"   WSQ[{b.bank_id:x}] '{name}' step_types={dict(th)} legacy_uuid={legacy}/64 first_steps={st[:2]}")
            break
    # DPI
    dpi = probe_dpi(f)
    if dpi:
        rows.append("   DPI: " + " ".join(f"{t}@{s}" for t, s in dpi))
    # reference graph
    cr, sl, dc, ds = reference_graph(f)
    rows.append(f"   refs: combi_timbres={len(cr)} (dangling={dc})  setlist_slots={len(sl)} (dangling={ds})")
    return "\n".join(rows)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        here = os.path.dirname(os.path.abspath(__file__))
        corpus = os.path.join(here, "..", "BBPB")
        args = [os.path.join(corpus, f) for f in sorted(os.listdir(corpus)) if f.lower().endswith(".pcg")]
        args += [os.path.join(corpus, "Archive", f) for f in sorted(os.listdir(os.path.join(corpus, "Archive"))) if f.lower().endswith(".pcg")]
    for p in args:
        try:
            print(analyze(p))
        except Exception as e:
            print(f"== {os.path.basename(p)}: ERROR {e}")
