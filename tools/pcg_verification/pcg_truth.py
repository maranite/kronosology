#!/usr/bin/env python3
"""
pcg_truth.py — Golden-truth .PCG model for Kronos OS 3.x files (Phase A).

Implements the container + record layout confirmed by the four-way comparison
(kronosology / DIY-KORG-KRONOS-EDITOR / KronosScreenRemote / PCG Tools) and by
the byte-level probe of real Kronos-written files in ../BBPB (OS 3.x, byte 7 == 2).

Confirmed facts this model encodes (see _WORKING/PHASE_A_REPORT.md):
  * 8-byte file header: "KORG" @0, model 0x68 @4, ftype @5 (0=PCG/1=SNG),
    byte6, OS/checksum-gen byte @7 (0=1.0/1.1, 1=2.x, 2=3.x), checksum-flag @8.
  * "PCG1" outer container @0x10 (size wraps everything after it); "DIV1" @0x1C.
  * Top-level chunks: DIV1, SLS1, PRG1, CMB1, DKT1, WSQ1, GLB1, DPI1 (in that
    order on real OS3 files); each chunk = [4-char tag][u32 BE size][4-byte dwX]
    + payload; walk advances pos += 12 + size.
  * SLS1 contains SLD1 then STL1; STL1 contains SBK1 (set-list params bank).
  * Bank sub-chunks MBK1/PBK1/CBK1/DBK1/WBK1/SBK1 have a 24-byte header:
    [tag][size][4][count BE][itemsize BE][bankid BE] then count records.
  * Checksum: for the 7 types MBK1/PBK1/CBK1/SBK1/GLB1/WBK1/DBK1,
    byte at header+11 = sum(payload bytes from +12 .. +12+size) & 0xFF.
  * Records: name @0 (24B, space/NUL padded) except GLB1 (no name@0).
    Program category @2568 (4 bits) / sub-category bits 7-5 of same byte.
    Combi timbre table @4802, 188-byte stride, 16 timbres: num@+0 bank@+1
    status@+2 (bits 7-5: 0=Off 1=Int 3=Ext 4=EX2).
    Set-list slot @+24 type (2 bits) @+25 bank (5 bits) @+26 index (8 bits),
    542-byte stride, 128 slots.
    Global category tables @12912/13344 (Program) /16800/17232 (Combi), 24B names.
  * Bank-id encodings: Program 0..4, 0x8000, 0x20000..0x2000D;
    Combi 0..6, 0x20000..0x20006; DrumKit/WaveSeq 0, 0x20000..0x2000D.

Read-only. Every function returns a plain result; nothing here writes files.
"""

from __future__ import annotations
import struct
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

MAGIC = b"KORG"
MODEL_KRONOS = 0x68
FILE_PCG = 0x00
FILE_SNG = 0x01

CHUNK_HDR = 12          # tag(4) + size(4) + dwX(4)
BANK_HDR = 24           # tag(4) + size(4) + 4 + count(4) + itemsize(4) + bankid(4)

BANK_CHUNK_TYPES = ("MBK1", "PBK1", "CBK1", "SBK1", "GLB1", "WBK1", "DBK1")
CHECKSUM_CHUNK_TYPES = ("MBK1", "PBK1", "CBK1", "SBK1", "GLB1", "WBK1", "DBK1")

PROG_RECORD = 4960
COMBI_RECORD = 7810
SETLIST_RECORD = 69416
DRUMKIT_RECORD = 38424
WAVESEQ_RECORD = 2216

COMBI_TIMBRE_OFFSET = 4802
COMBI_TIMBRE_STRIDE = 188
COMBI_TIMBRES = 16

SLOT_BASE = 24
SLOT_STRIDE = 542
SLOT_COUNT = 128

PROG_CATEGORY_OFFSET = 2568
COMBI_CATEGORY_OFFSET = 4790

GLB_PROG_CAT = 12912
GLB_PROG_SUB = 13344
GLB_COMBI_CAT = 16800
GLB_COMBI_SUB = 17232

# Bank-id -> index. Program: I-A..I-E = 0..4, I-F = 0x8000, U-A..U-GG = 0x20000..0x2000D.
def program_bank_index(bank_id: int) -> Optional[int]:
    if 0 <= bank_id <= 4: return bank_id
    if bank_id == 0x8000: return 5
    if 0x20000 <= bank_id <= 0x2000D: return 6 + (bank_id - 0x20000)
    return None

def combi_bank_index(bank_id: int) -> Optional[int]:
    if 0 <= bank_id <= 6: return bank_id
    if 0x20000 <= bank_id <= 0x20006: return 7 + (bank_id - 0x20000)
    return None

def drumkit_waveseq_bank_index(bank_id: int) -> Optional[int]:
    if bank_id == 0: return 0
    if 0x20000 <= bank_id <= 0x2000D: return 1 + (bank_id - 0x20000)
    return None

BANK_ID_LABELS = {
    # program
    0: "I-A", 1: "I-B", 2: "I-C", 3: "I-D", 4: "I-E", 0x8000: "I-F",
    0x20000: "U-A", 0x20001: "U-B", 0x20002: "U-C", 0x20003: "U-D", 0x20004: "U-E",
    0x20005: "U-F", 0x20006: "U-G", 0x20007: "U-AA", 0x20008: "U-BB", 0x20009: "U-CC",
    0x2000A: "U-DD", 0x2000B: "U-EE", 0x2000C: "U-FF", 0x2000D: "U-GG",
    # combi
    0x20000 | 0x0: "U-A", 0x20006: "U-G",
}

def bank_label(obj_type: str, bank_id: int) -> str:
    if obj_type in ("MBK1", "PBK1"):
        idx = program_bank_index(bank_id)
        if idx is None: return f"0x{bank_id:x}"
        return ["I-A","I-B","I-C","I-D","I-E","I-F","U-A","U-B","U-C","U-D","U-E","U-F","U-G",
                "U-AA","U-BB","U-CC","U-DD","U-EE","U-FF","U-GG"][idx]
    if obj_type == "CBK1":
        idx = combi_bank_index(bank_id)
        if idx is None: return f"0x{bank_id:x}"
        return ["I-A","I-B","I-C","I-D","I-E","I-F","I-G","U-A","U-B","U-C","U-D","U-E","U-F","U-G"][idx]
    idx = drumkit_waveseq_bank_index(bank_id)
    if idx is None: return f"0x{bank_id:x}"
    return "Int" if idx == 0 else ["U-A","U-B","U-C","U-D","U-E","U-F","U-G","U-AA","U-BB","U-CC","U-DD","U-EE","U-FF","U-GG"][idx-1]


@dataclass
class Chunk:
    tag: str
    offset: int
    size: int
    content_start: int   # offset + 12
    content_end: int     # offset + 12 + size
    parent: Optional["Chunk"] = None
    children: List["Chunk"] = field(default_factory=list)


@dataclass
class BankChunk:
    chunk: Chunk
    tag: str
    count: int
    itemsize: int
    bank_id: int
    records_start: int
    checksum_byte: int
    checksum_calc: int
    checksum_ok: bool


@dataclass
class Timbre:
    number: int
    bank: int
    status: int      # bits 7-5
    status_name: str


@dataclass
class SetListSlot:
    index: int
    slot_type: int
    type_name: str
    bank: int
    number: int
    name: str
    comment: str


class PcgTruthError(Exception):
    pass


def _u32(data: bytes, off: int) -> int:
    return struct.unpack(">I", data[off:off+4])[0]


def _tag_ok(tag: bytes) -> bool:
    return len(tag) == 4 and all(0x41 <= c <= 0x5A or 0x30 <= c <= 0x39 for c in tag)


class PcgFile:
    def __init__(self, data: bytes, path: str = "<mem>"):
        self.data = data
        self.path = path
        self.errors: List[str] = []
        self.chunks: List[Chunk] = []
        self.banks: List[BankChunk] = []
        self.glb_chunk: Optional[Chunk] = None
        self._validate_header()
        self._walk()
        self._collect_banks()

    # ── header ────────────────────────────────────────────────
    def _validate_header(self):
        d = self.data
        if len(d) < 16:
            raise PcgTruthError("too short")
        if d[0:4] != MAGIC:
            raise PcgTruthError(f"bad magic {d[0:4]!r}")
        if d[4] != MODEL_KRONOS:
            self.errors.append(f"model byte 0x{d[4]:02x} != 0x68 (Kronos)")
        if d[5] not in (FILE_PCG, FILE_SNG):
            self.errors.append(f"filetype byte 0x{d[5]:02x} not 0=PCG/1=SNG")
        self.os_byte = d[7]
        self.checksum_flag = d[8]
        if self.os_byte not in (0, 1, 2):
            self.errors.append(f"OS byte 0x{d[7]:02x} not in {{0,1,2}}")

    # ── chunk walk ────────────────────────────────────────────
    def _walk(self):
        # PCG1 wrapper at 0x10 (documented in hand-notes; present in all real files probed)
        if self.data[0x10:0x14] == b"PCG1":
            size = _u32(self.data, 0x14)
            pcg1 = Chunk("PCG1", 0x10, size, 0x1C, 0x1C + size)
            self.chunks.append(pcg1)
            self._walk_children(pcg1, pcg1.content_start, pcg1.content_end, depth=0)
        else:
            self.errors.append("no PCG1 wrapper at 0x10 (may be OS1.x or trimmed file)")
            self._walk_children(None, 0x10, len(self.data), depth=0)

    def _walk_children(self, parent, start, end, depth):
        if depth > 64:
            self.errors.append("walk depth > 64 (possible corrupt file)")
            return
        pos = start
        while pos + CHUNK_HDR <= end:
            tag = self.data[pos:pos+4]
            if not _tag_ok(tag):
                self.errors.append(f"@0x{pos:x}: bad tag {tag!r} — stopping walk")
                return
            size = _u32(self.data, pos + 4)
            content_start = pos + CHUNK_HDR
            content_end = content_start + size
            if content_end > len(self.data) or content_end < content_start:
                self.errors.append(f"@0x{pos:x}: chunk {tag!r} size 0x{size:x} runs past EOF")
                return
            c = Chunk(tag.decode("ascii"), pos, size, content_start, content_end, parent)
            self.chunks.append(c)
            if parent is not None:
                parent.children.append(c)
            # descend into known CONTAINER chunks only; bank chunks (MBK1/PBK1/
            # CBK1/DBK1/WBK1/SBK1/GLB1) are LEAVES whose payload is raw record
            # data, never further nested chunks. (A naive walker that recursed
            # into MBK1 content collided with record bytes — fixed.)
            if c.tag in ("PCG1", "SLS1", "STL1", "PRG1", "CMB1", "DKT1", "WSQ1", "DPI1"):
                self._walk_children(c, content_start, content_end, depth + 1)
            pos += CHUNK_HDR + size
            if size == 0:
                return

    # ── bank chunks ───────────────────────────────────────────
    def _collect_banks(self):
        for c in self.chunks:
            if c.tag not in BANK_CHUNK_TYPES:
                continue
            if c.tag == "GLB1":
                self.glb_chunk = c
                continue
            if c.content_start + 20 > c.content_end:
                self.errors.append(f"@0x{c.offset:x} {c.tag} header truncated")
                continue
            # bank header (24 bytes at c.offset): tag(4) size(4) dwX(4)
            # count(4) itemsize(4) bankid(4) — records at c.offset+24.
            count = _u32(self.data, c.offset + 12)
            itemsize = _u32(self.data, c.offset + 16)
            bank_id = _u32(self.data, c.offset + 20)
            records_start = c.offset + 24
            # SBK1 is a bank too (128 set lists x 69416) — keep it in banks
            # so checksum/record access works uniformly; its records are
            # set-list objects, decoded via set_list_slots().
            stored = self.data[c.offset + 11]
            calc = sum(self.data[c.content_start : c.content_end]) & 0xFF
            self.banks.append(BankChunk(
                chunk=c, tag=c.tag, count=count, itemsize=itemsize, bank_id=bank_id,
                records_start=records_start, checksum_byte=stored, checksum_calc=calc,
                checksum_ok=(stored == calc)))

    # ── record accessors ──────────────────────────────────────
    def record(self, bank: BankChunk, index: int) -> Optional[bytes]:
        if index < 0 or index >= bank.count:
            return None
        off = bank.records_start + index * bank.itemsize
        if off + bank.itemsize > len(self.data):
            return None
        return self.data[off : off + bank.itemsize]

    def name(self, bank: BankChunk, index: int) -> str:
        rec = self.record(bank, index)
        if rec is None or len(rec) < 24:
            return ""
        raw = rec[0:24]
        end = len(raw)
        while end > 0 and raw[end-1] in (0, 0x20):
            end -= 1
        return raw[:end].decode("ascii", "replace")

    def program_category(self, bank: BankChunk, index: int):
        rec = self.record(bank, index)
        if rec is None or len(rec) < PROG_CATEGORY_OFFSET + 1:
            return None
        b = rec[PROG_CATEGORY_OFFSET]
        return b & 0x0F, (b >> 5) & 0x07

    def combi_timbres(self, bank: BankChunk, index: int) -> List[Timbre]:
        rec = self.record(bank, index)
        out: List[Timbre] = []
        if rec is None:
            return out
        for t in range(COMBI_TIMBRES):
            off = COMBI_TIMBRE_OFFSET + t * COMBI_TIMBRE_STRIDE
            if off + 2 >= len(rec):
                break
            num = rec[off]
            b = rec[off + 1]
            status = (rec[off + 2] >> 5) & 0x07
            names = {0: "Off", 1: "Int", 3: "Ext", 4: "EX2"}
            out.append(Timbre(num, b, status, names.get(status, f"?{status}")))
        return out

    def set_list_slots(self, sbk: BankChunk) -> List[SetListSlot]:
        """Decode 128 slots from one SBK1 (set-list params) bank.

        SBK1 payload = 128 set lists, each 69416 bytes:
          +0  24-byte set-list name
          +24 128 slots x 542 bytes; each slot: +0 24-byte slot name,
              +24 type(2b)/+25 bank(5b)/+26 index(8b), +30.. comment (512B)
        """
        slots: List[SetListSlot] = []
        if sbk.count != 128 or sbk.itemsize != SETLIST_RECORD:
            return slots
        # first set list only (caller iterates set_list_index separately)
        setlist_rec = self.record(sbk, 0)
        if setlist_rec is None or len(setlist_rec) < SLOT_BASE + SLOT_COUNT * SLOT_STRIDE:
            return slots
        slots_base = SLOT_BASE  # slot records start 24 bytes into the set-list record
        for i in range(SLOT_COUNT):
            off = slots_base + i * SLOT_STRIDE
            stype = setlist_rec[off + 24] & 0x03
            bank = setlist_rec[off + 25] & 0x1F
            number = setlist_rec[off + 26]
            name = setlist_rec[off:off+24].decode("ascii", "replace").rstrip("\x00 ")
            comment = setlist_rec[off+30:off+30+64].decode("ascii", "replace").rstrip("\x00 ")[:48]
            slots.append(SetListSlot(i, stype, {0:"Combi",1:"Program",2:"Song"}.get(stype, f"?{stype}"),
                                     bank, number, name, comment))
        return slots


def open_pcg(path: str) -> PcgFile:
    with open(path, "rb") as f:
        return PcgFile(f.read(), path)


if __name__ == "__main__":
    import sys
    for p in sys.argv[1:]:
        try:
            f = open_pcg(p)
        except PcgTruthError as e:
            print(f"{p}: PARSE ERROR: {e}")
            continue
        print(f"== {p}")
        print(f"   os={f.os_byte} checksum_flag={f.checksum_flag} chunks={len(f.chunks)} banks={len(f.banks)}")
        for b in f.banks:
            mark = "" if b.checksum_ok else "  <-- CHECKSUM MISMATCH"
            print(f"   {b.tag} {bank_label(b.tag, b.bank_id):>4} id=0x{b.bank_id:x} "
                  f"count={b.count:3d} itemsz={b.itemsize} csum=0x{b.checksum_byte:02x}/{b.checksum_calc:02x}{mark}")
        for e in f.errors:
            print(f"   ERR: {e}")
