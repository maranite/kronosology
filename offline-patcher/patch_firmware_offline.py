#!/usr/bin/env python3
"""
patch_firmware_offline.py — Offline Kronos firmware patcher + USB package builder.

Decrypts Mod.img and Eva.img, patches OA.ko, loadmod.ko, loadoa, and Eva
entirely on the host, then produces a signed Korg UpdateOS package that delivers
pre-patched binaries via the tar payload.  All patching happens offline; the
Kronos never needs to mount a cryptoloop or run a patcher script during install.

The resulting package is self-contained and can be applied to a stock Kronos
with no prior rooting, SSH access, or manual module installation.

Patch strategy:
  OA.ko     — 11 section-relative patch sites.  Survives minor firmware updates
               that don't restructure these functions.  ELF parsing is pure Python.
  loadmod.ko — Symbol-relative: looks up 'init_module' and 'bbbbbbbba12' in
               .symtab to find the exact patch locations; falls back to a byte-
               pattern search if the obfuscated symbol name changes.
  loadoa    — String replacement: rewrites '/korg/Mod/OA.ko',
               '/korg/Mod/KorgUsbAudioDriver.ko', and '/korg/Eva/Eva' to null-
               padded /sbin/ equivalents so loadoa boots the patched copies.
  Eva        — Code-cave patch: intercepts the Authorise button to auto-generate
               auth strings via oa_authgen.ko instead of showing the keyboard
               dialog.
  /sbin/Eva  — Deployed as a wrapper shell script that insmod's oa_authgen.ko
               then exec's the patched Eva binary.  This is how oa_authgen.ko
               is loaded at every boot without touching init scripts.

oa_authgen.ko is included in the package and deployed to /sbin/oa_authgen.ko.
It is loaded at boot by the /sbin/Eva wrapper (which loadoa execs as the last
step of its startup sequence, after OmapNKS4Module.ko is already loaded).

Usage:
  python3 patch_firmware_offline.py <update_tree>
  python3 patch_firmware_offline.py <update_tree> --verify
  python3 patch_firmware_offline.py <update_tree> -o <output_dir>

<update_tree>: path to an unpacked Kronos firmware tree, e.g.:
  KRONOS_Update_3_2_2/mnt
Expected layout:
  <update_tree>/korg/ro/Mod.img     (encrypted ext2; OA.ko and KorgUsbAudio inside)
  <update_tree>/korg/ro/Eva.img     (encrypted ext2; Eva binary inside; optional)
  <update_tree>/sbin/loadmod.ko
  <update_tree>/sbin/loadoa

If Eva.img is absent, the Eva binary patch is skipped.  /sbin/Eva is still
deployed as a wrapper that loads oa_authgen.ko and exec's the stock Eva from
the cryptoloop mount, so InstallEXs auto-auth still works.

Output (default: <this_script_dir>/output/kronosology-offline-patched/):
  install.info          signed package metadata — UpdateOS reads this first
  kronosology.tar.gz    pre-patched binaries extracted to / on the Kronos
  pretar.sh             backup originals; rename InstallEXs -> InstallEXs.real
  posttar.sh            one-shot auth for already-installed EXs; write version marker
  README.txt

To install on a Kronos (no SSH needed):
  1. Build: python3 patch_firmware_offline.py <update_tree>
  2. Copy output contents to the root of a FAT-formatted USB stick
  3. Insert stick into Kronos, trigger Global -> OS Update from the front panel
  4. After completion, POWER-CYCLE (full off >= 60 s).  Do NOT soft-reboot.

Requires: cryptography  (pip install cryptography)
Tested on: Korg Kronos OS 3.2.1, 3.2.2, 3.2.3
"""

import argparse
import hashlib
import shutil
import stat
import struct
import sys
import tarfile
from io import BytesIO
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.backends import default_backend
except ImportError:
    sys.exit("pip install cryptography")

# ── constants ─────────────────────────────────────────────────────────────────

SECTOR     = 512
EXT2_MAGIC = 0xEF53

# Universal cryptoloop AES-256-CBC keys.
# 31 printable ASCII chars + trailing \x00 (Korg HexEncode quirk).
# Same across all Kronos units and all firmware versions checked (3.2.1, 3.2.2).
MOD_KEY = bytes.fromhex("6133333661313563643834316563383932366239396537633338383465616100")
EVA_KEY = bytes.fromhex("3334326565353964353439633764333239643833353533376265303534306400")

# UpdateOS package signing key (from UpdateOS .data section VMA 0x0813bac8)
UPDATER_KEY = bytes([0x13, 0xd0, 0xaf, 0xef, 0xe0, 0x3c, 0x9b, 0x92,
                     0x16, 0x2f, 0xae, 0xff, 0x77, 0x53, 0x55, 0xe1])

# Known stock MD5s — used for version identification and reporting only.
# The patcher does not refuse to proceed on unrecognised versions.
KNOWN_STOCK_MD5 = {
    "d1697c9b1c478c0dcdfaef71516fe5f2": ("loadmod.ko", "3.2.1"),
    "d9d56475be6ccb74e9001b84b64046f7": ("loadmod.ko", "3.2.2"),
    "978154267019b63cb57d78dd9686a58d": ("loadmod.ko", "3.2.3"),
    "8a3d61f3332d7bcf694e8c05845b4754": ("loadoa",     "3.2.x"),
    "955636c2b11a70a1dbecefaaa7bd4f80": ("OA.ko",      "3.2.1"),
    "39fec7465fd7886ed8099c9cb85e2cdc": ("OA.ko",      "3.2.2"),
    "499e04ad198ac88d81d0963ead7d0066": ("OA.ko",      "3.2.3"),
    "29fbd20cf729980e1cffd670391256b5": ("KorgUsbAudioDriver.ko", "all"),
}

# ── Eva patch constants (VMA-relative, Eva v3.2.1 / 3.2.2) ───────────────────
# Eva is a statically-linked i386 ET_EXEC with image base 0x08048000.
# file_offset = vma - EVA_IMAGE_BASE

EVA_IMAGE_BASE     = 0x08048000
EVA_CAVE_VMA       = 0x08eb3577    # 206 zero bytes in .rodata — safe to overwrite
EVA_CALL_PATCH_VMA = 0x086a492c    # E8 EF 3C 00 00  CALL CAuthKeyboard::CAuthKeyboard
EVA_NOP_START_VMA  = 0x086a4931    # start of dialog-open block to NOP
EVA_NOP_END_VMA    = 0x086a4958    # first byte after the block (XOR EAX,EAX)

EVA_FN_GET_OPTION  = 0x08e1d890    # USTGAPIKLM::GetProductOptionFileName(uint, char*)
EVA_FN_SEND_AUTH   = 0x08e48bc0    # SendCommandAuthorizeOption(const char*)
EVA_PLT_OPEN       = 0x0804bdbc    # open()
EVA_PLT_WRITE      = 0x0804c8cc    # write()
EVA_PLT_READ       = 0x0804c5cc    # read()
EVA_PLT_CLOSE      = 0x0804bcec    # close()

_EVA_EXPECTED_CALL = bytes([0xe8, 0xef, 0x3c, 0x00, 0x00])   # original CALL bytes

# ── OA.ko patch table (section-relative) ─────────────────────────────────────
# Identical to patch_oa_ko.py — patch runs addressed by ELF section name +
# section-relative offset.  Verified on 3.2.1, 3.2.2, 3.2.3.
OA_PATCHES = [
    # CSTGEngine::Initialize — primary magic-value degradation block
    (".text", 0x008b4,
        "85c0745ac705000000003333333fc70500000000cdcc4cbec705040000003333333f"
        "c70504000000cdcc4cbec705080000003333333fc70508000000cdcc4cbec7050c00"
        "00003333333fc7050c000000cdcc4cbec705000000001f000000",
        "eb5c909090900000000090909090909000000000909090909090040000009090909"
        "0909004000000909090909090080000009090909090900800000090909090909"
        "00c0000009090909090900c0000009090909090900000000090909090"),

    # Continuation of the degradation block (gap-grouped separately)
    (".text", 0x00ab3,
        "c705000000003333333fc70500000000cdcc4cbec705040000003333333fc70504"
        "000000cdcc4cbec705080000003333333fc70508000000cdcc4cbec7050c000000"
        "3333333fc7050c000000cdcc4cbec705000000001f000000",
        "9090000000009090909090900000000090909090909004000000909090909090"
        "04000000909090909090080000009090909090900800000090909090909"
        "00c0000009090909090900c0000009090909090900000000090909090"),

    # Three inlined copies of the magic-value check + degradation in .text
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

    # Four IsUsingAnyUnauthorizedMultisamples specializations in COMDAT sections.
    (".text._ZN17CSTGPCMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",    0x0,
        "8b92d0010000", "31c0c3909090"),
    (".text._ZN21CSTGPluckedModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b9278030000", "31c0c3909090"),
    (".text._ZN17CSTGVPMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",     0x0,
        "8b9225040000", "31c0c3909090"),
    (".text._ZN19CSTGPianoModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",   0x0,
        "8b92ac000000", "31c0c3909090"),
]

# ── OA.ko symbol-relative patches ────────────────────────────────────────────
# Two more IsUsingAnyUnauthorizedMultisamples specializations that live directly
# in the main .text section rather than their own COMDAT section (unlike the
# four above), so a raw section-relative offset drifts whenever unrelated code
# earlier in .text changes size across firmware versions. Confirmed exactly
# this way going 3.2.2 -> 3.2.3: identical 19 bytes, identical symbols, just
# +0x90 further into .text after an unrelated recompile elsewhere. Resolved by
# symbol name instead, same mechanism as LOADMOD_PATCHES below.
OA_SYMBOL_PATCHES = [
    ("_ZN15CSTGTG92OscBase34IsUsingAnyUnauthorizedMultisamplesER23CSTGPatchMessageContext", 0x0,
        "8b40088b52280fbf40088b44023485c00f95c0",
        "31c0c390909090909090909090909090909090"),
    ("_ZN9CPianoOsc34IsUsingAnyUnauthorizedMultisamplesER23CSTGPatchMessageContext", 0x0,
        "8b40088b52280fbf40088b44020485c00f95c0",
        "31c0c390909090909090909090909090909090"),
]

# Last-resort fallback: section-relative .text offsets, valid on 3.2.1/3.2.2
# only. Only used if OA.ko is ever shipped stripped (no .symtab) -- if this
# path triggers on a firmware newer than 3.2.2, treat the offset as unverified.
_OA_SYMBOL_FALLBACK = {
    ("_ZN15CSTGTG92OscBase34IsUsingAnyUnauthorizedMultisamplesER23CSTGPatchMessageContext", 0x0): 0x13c7d0,
    ("_ZN9CPianoOsc34IsUsingAnyUnauthorizedMultisamplesER23CSTGPatchMessageContext", 0x0): 0x155d30,
}

# ── loadmod.ko patches (symbol-relative) ─────────────────────────────────────
# (symbol_name, within_symbol_offset, orig_hex, patched_hex)
LOADMOD_PATCHES = [
    ("init_module", 0x39,  "85c00f85a3000000", "9090909090909090"),
    ("init_module", 0xBD,  "7547",             "9090"),
    ("bbbbbbbba12", 0x170, "0f85e7feffff",     "e91e01000090"),
]

_LOADMOD_FALLBACK = {
    ("init_module", 0x39):  22317,
    ("init_module", 0xBD):  22449,
    ("bbbbbbbba12", 0x170): 16304,
}

# ── loadoa patches (string replacement) ──────────────────────────────────────
# All replacements are the same byte length as the originals (null-padded).
# The Eva path redirect causes loadoa to exec /sbin/Eva instead of the
# cryptoloop copy.  /sbin/Eva is always deployed (as a wrapper shell script)
# so this redirect is always safe even when Eva.img is absent.
LOADOA_PATCHES = [
    (b"/korg/Mod/OA.ko\x00",
     b"/sbin/OA.ko\x00\x00\x00\x00\x00"),
    (b"/korg/Mod/KorgUsbAudioDriver.ko\x00",
     b"/sbin/KorgUsbAudioDriver.ko\x00\x00\x00\x00\x00"),
    (b"/korg/Eva/Eva\x00",
     b"/sbin/Eva\x00\x00\x00\x00\x00"),
]


# ── cryptoloop decryption ─────────────────────────────────────────────────────

def _decrypt_sector(key: bytes, sector_num: int, ct: bytes) -> bytes:
    iv = struct.pack("<I", sector_num & 0xFFFFFFFF) + b"\x00" * 12
    ciph = Cipher(algorithms.AES(key), modes.CBC(iv), backend=default_backend())
    return ciph.decryptor().update(ct)


def _decrypt_img(key: bytes, img_path: Path) -> bytearray:
    """Decrypt a Kronos cryptoloop image into memory and return the ext2 plaintext."""
    size = img_path.stat().st_size
    total = size // SECTOR
    data = bytearray(size)
    print(f"  Decrypting {img_path.name} ({size // (1024*1024)} MB, {total} sectors)...")
    with open(img_path, "rb") as f:
        for i in range(total):
            if i and (i % 8192) == 0:
                print(f"    {i * 100 // total:3d}%", end="\r", flush=True)
            ct = f.read(SECTOR)
            pt = _decrypt_sector(key, i, ct)
            data[i * SECTOR:(i + 1) * SECTOR] = pt
        tail = f.read()
        if tail:
            data[total * SECTOR:] = tail
    print("    100%")
    magic, = struct.unpack_from("<H", data, 2 * SECTOR + 56)
    if magic != EXT2_MAGIC:
        raise RuntimeError(f"Decrypted {img_path.name} does not have ext2 magic (got 0x{magic:04X})")
    return data


def decrypt_mod_img(img_path: Path) -> bytearray:
    return _decrypt_img(MOD_KEY, img_path)


def decrypt_eva_img(img_path: Path) -> bytearray:
    return _decrypt_img(EVA_KEY, img_path)


# ── ext2 reader ───────────────────────────────────────────────────────────────

class Ext2Reader:
    """Minimal ext2 reader — direct, single-indirect, double-indirect, and triple-indirect blocks."""

    def __init__(self, data: bytes | bytearray):
        self._d = memoryview(data)
        sb = 1024  # superblock offset
        magic, = struct.unpack_from("<H", data, sb + 56)
        if magic != EXT2_MAGIC:
            raise ValueError(f"Not a valid ext2 image (magic=0x{magic:04X})")
        log_bs, = struct.unpack_from("<I", data, sb + 24)
        self.block_size = 1024 << log_bs
        self.inodes_per_group, = struct.unpack_from("<I", data, sb + 40)
        rev, = struct.unpack_from("<I", data, sb + 76)
        self.inode_size = struct.unpack_from("<H", data, sb + 88)[0] if rev >= 1 else 128
        self._bgdt_off = 2 * self.block_size if self.block_size == 1024 else self.block_size

    def _block(self, bn: int) -> bytes:
        off = bn * self.block_size
        return bytes(self._d[off:off + self.block_size])

    def _inode(self, num: int) -> bytes:
        grp = (num - 1) // self.inodes_per_group
        idx = (num - 1) % self.inodes_per_group
        inode_table_block, = struct.unpack_from("<I", self._d, self._bgdt_off + grp * 32 + 8)
        off = inode_table_block * self.block_size + idx * self.inode_size
        return bytes(self._d[off:off + self.inode_size])

    def _read_file_data(self, inode: bytes, file_size: int) -> bytes:
        ptrs = list(struct.unpack_from("<15I", inode, 40))
        bs = self.block_size
        pts_per_block = bs // 4
        out = bytearray()
        remaining = file_size

        def take(bn: int) -> None:
            nonlocal remaining
            if remaining <= 0 or bn == 0:
                return
            chunk = self._block(bn)
            n = min(bs, remaining)
            out.extend(chunk[:n])
            remaining -= n

        for i in range(12):
            if remaining <= 0:
                break
            take(ptrs[i])

        if remaining > 0 and ptrs[12]:
            ind = struct.unpack_from(f"<{pts_per_block}I", self._block(ptrs[12]))
            for bn in ind:
                if remaining <= 0:
                    break
                take(bn)

        if remaining > 0 and ptrs[13]:
            ind1 = struct.unpack_from(f"<{pts_per_block}I", self._block(ptrs[13]))
            for b1 in ind1:
                if remaining <= 0 or b1 == 0:
                    break
                ind2 = struct.unpack_from(f"<{pts_per_block}I", self._block(b1))
                for bn in ind2:
                    if remaining <= 0:
                        break
                    take(bn)

        if remaining > 0 and ptrs[14]:
            ind1 = struct.unpack_from(f"<{pts_per_block}I", self._block(ptrs[14]))
            for b1 in ind1:
                if remaining <= 0 or b1 == 0:
                    break
                ind2 = struct.unpack_from(f"<{pts_per_block}I", self._block(b1))
                for b2 in ind2:
                    if remaining <= 0 or b2 == 0:
                        break
                    ind3 = struct.unpack_from(f"<{pts_per_block}I", self._block(b2))
                    for bn in ind3:
                        if remaining <= 0:
                            break
                        take(bn)

        return bytes(out)

    def _lookup_inode(self, path: str) -> int:
        inode_num = 2
        for part in path.strip("/").split("/"):
            if not part:
                continue
            inode = self._inode(inode_num)
            fsize, = struct.unpack_from("<I", inode, 4)
            dir_data = self._read_file_data(inode, fsize)
            found = False
            pos = 0
            while pos < len(dir_data):
                ent_ino, rec_len, name_len = struct.unpack_from("<IHB", dir_data, pos)
                if ent_ino != 0:
                    name = dir_data[pos + 8:pos + 8 + name_len].decode("ascii", errors="replace")
                    if name == part:
                        inode_num = ent_ino
                        found = True
                        break
                if rec_len == 0:
                    break
                pos += rec_len
            if not found:
                raise FileNotFoundError(f"Not found in ext2: {path!r}")
        return inode_num

    def read_file(self, path: str) -> bytes:
        ino_num = self._lookup_inode(path)
        inode = self._inode(ino_num)
        fsize, = struct.unpack_from("<I", inode, 4)
        return self._read_file_data(inode, fsize)


# ── ELF32 helpers ─────────────────────────────────────────────────────────────

def _elf32_shstrtab_off(data: bytes) -> tuple[int, int, int, int]:
    """Return (e_shoff, e_shentsize, e_shnum, shstrtab_file_off)."""
    e_shoff, = struct.unpack_from("<I", data, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 46)
    shstr_sh = e_shoff + e_shstrndx * e_shentsize
    shstr_foff, = struct.unpack_from("<I", data, shstr_sh + 16)
    return e_shoff, e_shentsize, e_shnum, shstr_foff


def elf32_progbits_sections(data: bytes) -> dict[str, tuple[int, int]]:
    """Return {section_name: (file_offset, size)} for all PROGBITS sections."""
    e_shoff, e_shentsize, e_shnum, shstr_foff = _elf32_shstrtab_off(data)
    SHT_PROGBITS = 1
    result = {}
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        nm, sh_type, _, _, sh_offset, sh_size = struct.unpack_from("<IIIIII", data, sh)
        if sh_type != SHT_PROGBITS:
            continue
        nul = data.index(b"\x00", shstr_foff + nm)
        name = data[shstr_foff + nm:nul].decode("ascii", errors="replace")
        result[name] = (sh_offset, sh_size)
    return result


def elf32_func_symbols_and_sections(data: bytes) -> tuple[dict, dict]:
    """
    Returns:
      symbols: {name: (section_index, value)}
      section_file_offsets: {section_index: file_offset}
    Only STT_FUNC symbols with a defined section.
    """
    e_shoff, e_shentsize, e_shnum, shstr_foff = _elf32_shstrtab_off(data)

    sec_foff = {}
    symtab_foff = symtab_size = symtab_entsize = 0
    strtab_foff = 0
    SHT_SYMTAB, SHT_STRTAB = 2, 3

    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        nm, sh_type, _, _, sh_offset, sh_size, sh_link, _, _, sh_entsize = struct.unpack_from("<IIIIIIIIII", data, sh)
        sec_foff[i] = sh_offset
        if sh_type == SHT_SYMTAB:
            symtab_foff, symtab_size, symtab_entsize = sh_offset, sh_size, sh_entsize
            strtab_idx = sh_link
    if not symtab_foff:
        return {}, sec_foff
    strtab_foff = sec_foff[strtab_idx]

    STT_FUNC = 2
    symbols = {}
    n_syms = symtab_size // symtab_entsize
    for i in range(n_syms):
        sym = symtab_foff + i * symtab_entsize
        st_name, st_value, _, st_info, _, st_shndx = struct.unpack_from("<IIIBBH", data, sym)
        if (st_info & 0xf) != STT_FUNC:
            continue
        if st_shndx == 0 or st_shndx >= 0xff00:
            continue
        nul = data.index(b"\x00", strtab_foff + st_name)
        name = data[strtab_foff + st_name:nul].decode("ascii", errors="replace")
        symbols[name] = (st_shndx, st_value)
    return symbols, sec_foff


# ── OA.ko patcher ─────────────────────────────────────────────────────────────

def patch_oa_ko(data: bytes, verify_only: bool = False) -> bytes:
    sections = elf32_progbits_sections(data)
    out = bytearray(data)
    applied = already = errors = 0

    for n, (sec_name, rel, orig_hex, pat_hex) in enumerate(OA_PATCHES, 1):
        orig = bytes.fromhex(orig_hex)
        pat  = bytes.fromhex(pat_hex)
        if len(orig) != len(pat):
            raise AssertionError(f"OA.ko patch {n}: orig/pat length mismatch in table")

        if sec_name not in sections:
            if ".text._ZN" in sec_name:
                print(f"  [OA.ko patch {n:2d}] WARNING: section {sec_name!r} not found"
                      " — skipping (may be a different firmware variant)")
                continue
            print(f"  [OA.ko patch {n:2d}] ERROR: required section {sec_name!r} not found")
            errors += 1
            continue

        sh_off, sh_sz = sections[sec_name]
        if rel + len(orig) > sh_sz:
            print(f"  [OA.ko patch {n:2d}] ERROR: patch extends past section end")
            errors += 1
            continue

        file_off = sh_off + rel
        cur = bytes(out[file_off:file_off + len(orig)])
        tag = (sec_name if len(sec_name) <= 55 else sec_name[:52] + "...")
        if cur == pat:
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{rel:06x}  already patched")
            already += 1
        elif cur == orig:
            if not verify_only:
                out[file_off:file_off + len(pat)] = pat
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{rel:06x}  {'would patch' if verify_only else 'patched'} ({len(pat)} bytes)")
            applied += 1
        else:
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{rel:06x}  ERROR: unexpected bytes")
            print(f"        expected: {orig[:16].hex()}{'...' if len(orig) > 16 else ''}")
            print(f"        found   : {cur[:16].hex()}{'...' if len(cur) > 16 else ''}")
            errors += 1

    # Symbol-relative patches (see OA_SYMBOL_PATCHES header comment above)
    symbols, sec_foff = elf32_func_symbols_and_sections(data)
    text_off, _ = sections.get(".text", (None, None))

    for i, (sym_name, sym_off, orig_hex, pat_hex) in enumerate(OA_SYMBOL_PATCHES):
        n = len(OA_PATCHES) + i + 1
        orig = bytes.fromhex(orig_hex)
        pat  = bytes.fromhex(pat_hex)
        if len(orig) != len(pat):
            raise AssertionError(f"OA.ko patch {n}: orig/pat length mismatch in table")

        file_off = None
        if sym_name in symbols:
            shndx, sym_val = symbols[sym_name]
            sec_base = sec_foff.get(shndx)
            if sec_base is not None:
                file_off = sec_base + sym_val + sym_off

        if file_off is None:
            fallback_rel = _OA_SYMBOL_FALLBACK.get((sym_name, sym_off))
            if fallback_rel is None or text_off is None:
                print(f"  [OA.ko patch {n:2d}] ERROR: symbol {sym_name!r} not found and no fallback offset known")
                errors += 1
                continue
            file_off = text_off + fallback_rel
            print(f"  [OA.ko patch {n:2d}] symbol {sym_name!r} not found — used fallback .text offset 0x{fallback_rel:x} (3.2.1/3.2.2 only)")

        cur = bytes(out[file_off:file_off + len(orig)])
        tag = sym_name if len(sym_name) <= 55 else sym_name[:52] + "..."
        if cur == pat:
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{sym_off:06x}  already patched")
            already += 1
        elif cur == orig:
            if not verify_only:
                out[file_off:file_off + len(pat)] = pat
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{sym_off:06x}  {'would patch' if verify_only else 'patched'} ({len(pat)} bytes)")
            applied += 1
        else:
            print(f"  [OA.ko patch {n:2d}] {tag:56s} +0x{sym_off:06x}  ERROR: unexpected bytes")
            print(f"        expected: {orig[:16].hex()}{'...' if len(orig) > 16 else ''}")
            print(f"        found   : {cur[:16].hex()}{'...' if len(cur) > 16 else ''}")
            errors += 1

    if errors:
        raise RuntimeError(f"OA.ko: {errors} patch site(s) had unexpected bytes — refusing to proceed")

    print(f"  OA.ko: {applied} patched, {already} already-patched, 0 errors")
    return bytes(out)


# ── loadmod.ko patcher ────────────────────────────────────────────────────────

def patch_loadmod_ko(data: bytes, verify_only: bool = False) -> bytes:
    symbols, sec_foff = elf32_func_symbols_and_sections(data)
    out = bytearray(data)
    applied = already = 0

    for sym_name, sym_off, orig_hex, pat_hex in LOADMOD_PATCHES:
        orig = bytes.fromhex(orig_hex)
        pat  = bytes.fromhex(pat_hex)

        file_off = None
        if sym_name in symbols:
            shndx, sym_val = symbols[sym_name]
            sec_base = sec_foff.get(shndx)
            if sec_base is not None:
                file_off = sec_base + sym_val + sym_off

        if file_off is None:
            fallback_base = _LOADMOD_FALLBACK.get((sym_name, sym_off))
            if fallback_base is None:
                raise RuntimeError(
                    f"loadmod.ko: symbol {sym_name!r} not found and no fallback offset known")
            window_start = max(0, fallback_base - 0x4000)
            window_end   = min(len(data), fallback_base + 0x4000)
            window = data[window_start:window_end]
            idx = window.find(orig)
            if idx == -1:
                idx = window.find(pat)
                if idx != -1:
                    file_off = window_start + idx
                else:
                    raise RuntimeError(
                        f"loadmod.ko: could not locate patch site for {sym_name!r}+0x{sym_off:x}")
            else:
                file_off = window_start + idx
            print(f"  [loadmod patch {sym_name}+0x{sym_off:x}] symbol not found — used pattern search, offset={file_off}")

        cur = bytes(out[file_off:file_off + len(orig)])
        label = f"{sym_name}+0x{sym_off:x}"
        if cur == pat:
            print(f"  [loadmod {label:25s}] file+0x{file_off:05x}  already patched")
            already += 1
        elif cur == orig:
            if not verify_only:
                out[file_off:file_off + len(pat)] = pat
            print(f"  [loadmod {label:25s}] file+0x{file_off:05x}  {'would patch' if verify_only else 'patched'} ({len(pat)} bytes)")
            applied += 1
        else:
            raise RuntimeError(
                f"loadmod.ko: unexpected bytes at {sym_name}+0x{sym_off:x} (file+0x{file_off:x})\n"
                f"  expected: {orig.hex()}\n  found:    {cur.hex()}")

    print(f"  loadmod.ko: {applied} patched, {already} already-patched")
    return bytes(out)


# ── loadoa patcher ────────────────────────────────────────────────────────────

def patch_loadoa(data: bytes, verify_only: bool = False) -> bytes:
    out = bytearray(data)
    applied = already = 0

    for orig_b, pat_b in LOADOA_PATCHES:
        if len(orig_b) != len(pat_b):
            raise AssertionError(f"loadoa patch: length mismatch {len(orig_b)} vs {len(pat_b)}")
        idx = out.find(orig_b)
        if idx == -1:
            idx_patched = out.find(pat_b)
            if idx_patched != -1:
                print(f"  [loadoa] {orig_b!r} already replaced at +0x{idx_patched:x}")
                already += 1
                continue
            raise RuntimeError(f"loadoa: string {orig_b!r} not found — wrong firmware version?")
        if not verify_only:
            out[idx:idx + len(pat_b)] = pat_b
        print(f"  [loadoa] {orig_b!r} → {pat_b!r}  at +0x{idx:x}  {'(would patch)' if verify_only else ''}")
        applied += 1

    print(f"  loadoa: {applied} patched, {already} already-patched")
    return bytes(out)


# ── Eva patcher ───────────────────────────────────────────────────────────────
#
# Code-cave patch to auto-generate EX auth strings when the Authorise button is
# pressed, instead of showing the manual-entry keyboard dialog.
#
# On entry to OnAuthorizePlugIn (at EVA_CALL_PATCH_VMA):
#   EDI = product_index (passed in from the caller)
#
# Cave layout (starting at EVA_CAVE_VMA):
#   +0x00  "/proc/.oaauth\0"           14 bytes (string for open())
#   +0x0e  cave_code                   code
#
# Stack frame inside cave (after SUB ESP, 0x50):
#   [ESP+0x10..0x18]  gen_cmd  "GEN:Sxxx\0"
#   [ESP+0x20..0x38]  auth_buf[25]
#   [ESP+0x40..0x48]  slot_buf[8]  (null-terminated option id, e.g. "S023\0")

def _eva_vma_to_file(vma: int) -> int:
    return vma - EVA_IMAGE_BASE


def _eva_rel32(from_vma: int, target_vma: int) -> bytes:
    """4-byte LE relative offset for a 5-byte CALL/JMP at from_vma."""
    return struct.pack("<i", target_vma - (from_vma + 5))


def _eva_jmp_short_byte(from_vma: int, target_vma: int) -> int:
    disp = target_vma - (from_vma + 2)
    if not (-128 <= disp <= 127):
        raise ValueError(f"Short jump out of range: from={from_vma:#x} target={target_vma:#x}")
    return struct.pack("b", disp)[0]


def _assemble_eva_cave() -> bytes:
    """
    Assemble the cave string + code for EVA_CAVE_VMA.
    Returns bytes that start at EVA_CAVE_VMA (string first, then code at +0x0e).
    """
    code_start_vma = EVA_CAVE_VMA + 0x0e

    def build(cs: int) -> bytes:
        """Build code bytes; cs = VMA of first code byte."""
        b = bytearray()

        def at() -> int:
            return cs + len(b)

        def call(target: int) -> None:
            addr = at()
            b.extend(b'\xe8' + _eva_rel32(addr, target))

        def mov_esp_imm32(offset: int, val: int) -> None:
            if offset == 0:
                b.extend(bytes([0xC7, 0x04, 0x24]) + struct.pack('<I', val))
            elif offset < 128:
                b.extend(bytes([0xC7, 0x44, 0x24, offset]) + struct.pack('<I', val))
            else:
                b.extend(bytes([0xC7, 0x84, 0x24]) + struct.pack('<I', offset) + struct.pack('<I', val))

        def mov_esp_reg(offset: int, reg: int) -> None:
            modrm_reg = reg << 3
            if offset == 0:
                b.extend(bytes([0x89, modrm_reg | 0x04, 0x24]))
            elif offset < 128:
                b.extend(bytes([0x89, 0x40 | modrm_reg | 0x04, 0x24, offset]))
            else:
                raise NotImplementedError

        def lea_eax_esp(offset: int) -> None:
            if offset < 128:
                b.extend(bytes([0x8D, 0x44, 0x24, offset]))
            else:
                b.extend(bytes([0x8D, 0x84, 0x24]) + struct.pack('<I', offset))

        # Prologue
        b.extend([0x83, 0xEC, 0x50])   # SUB ESP, 0x50

        # 1. GetProductOptionFileName(EDI, &slot_buf)
        lea_eax_esp(0x40)
        mov_esp_reg(0x04, 0)           # arg1 = slot_buf
        b.extend([0x89, 0x3C, 0x24])   # arg0 = product_index (EDI)
        call(EVA_FN_GET_OPTION)

        # 2. Build gen_cmd = "GEN:" + slot[0..4] at [ESP+0x10]
        mov_esp_imm32(0x10, 0x3A4E4547)               # "GEN:" little-endian
        b.extend([0x8B, 0x44, 0x24, 0x40])            # MOV EAX, [ESP+0x40]
        mov_esp_reg(0x14, 0)                           # [ESP+0x14] = slot bytes 0-3
        b.extend([0x8A, 0x44, 0x24, 0x44])            # MOV AL, [ESP+0x44]
        b.extend([0x88, 0x44, 0x24, 0x18])            # [ESP+0x18] = slot[4] (null term)

        # 3. open("/proc/.oaauth", O_RDWR)
        mov_esp_imm32(0x04, 2)
        mov_esp_imm32(0x00, EVA_CAVE_VMA)
        call(EVA_PLT_OPEN)
        b.extend([0x89, 0xC3])           # MOV EBX, EAX (fd)
        b.extend([0x85, 0xDB])           # TEST EBX, EBX
        js_pos = len(b)
        b.extend([0x78, 0x00])           # JS .fail  (patched later)

        # 4. write(fd, gen_cmd, 8)
        mov_esp_imm32(0x08, 8)
        lea_eax_esp(0x10)
        mov_esp_reg(0x04, 0)
        b.extend([0x89, 0x1C, 0x24])
        call(EVA_PLT_WRITE)
        b.extend([0x83, 0xF8, 0x08])     # CMP EAX, 8
        jnz_pos = len(b)
        b.extend([0x75, 0x00])           # JNZ .closefail  (patched later)

        # 5. read(fd, auth_buf, 25)
        mov_esp_imm32(0x08, 25)
        lea_eax_esp(0x20)
        mov_esp_reg(0x04, 0)
        b.extend([0x89, 0x1C, 0x24])
        call(EVA_PLT_READ)
        b.extend([0x85, 0xC0])           # TEST EAX, EAX
        jle_pos = len(b)
        b.extend([0x7E, 0x00])           # JLE .closefail  (patched later)

        # Null-terminate auth_buf
        b.extend([0x8D, 0x4C, 0x24, 0x20])  # LEA ECX, [ESP+0x20]
        b.extend([0xC6, 0x04, 0x01, 0x00])  # MOV byte [ECX+EAX], 0

        # close(fd) on success path
        b.extend([0x89, 0x1C, 0x24])
        call(EVA_PLT_CLOSE)

        # 6. SendCommandAuthorizeOption(auth_buf)
        lea_eax_esp(0x20)
        b.extend([0x89, 0x04, 0x24])
        call(EVA_FN_SEND_AUTH)
        jmp_pos = len(b)
        b.extend([0xEB, 0x00])           # JMP .done  (patched later)

        # .closefail
        closefail_off = len(b)
        b.extend([0x89, 0x1C, 0x24])
        call(EVA_PLT_CLOSE)

        # .fail / .done — epilogue
        fail_off = done_off = len(b)
        b.extend([0x83, 0xC4, 0x50])     # ADD ESP, 0x50
        b.extend([0xC3])                 # RET

        # Patch forward-reference branches
        b[js_pos  + 1] = _eva_jmp_short_byte(cs + js_pos,   cs + fail_off)
        b[jnz_pos + 1] = _eva_jmp_short_byte(cs + jnz_pos,  cs + closefail_off)
        b[jle_pos + 1] = _eva_jmp_short_byte(cs + jle_pos,  cs + closefail_off)
        b[jmp_pos + 1] = _eva_jmp_short_byte(cs + jmp_pos,  cs + done_off)

        return bytes(b)

    code_bytes = build(code_start_vma)
    limit = 206 - 14
    if len(code_bytes) > limit:
        raise RuntimeError(f"Eva cave code overflow: {len(code_bytes)} bytes > {limit} limit")
    return b"/proc/.oaauth\x00" + code_bytes


def _scan_absolute_refs(data: bytes, lo_vma: int, hi_vma: int,
                        exclude_off: int = None, exclude_len: int = 0):
    """Find every place in `data` holding a 4-byte little-endian absolute
    pointer into [lo_vma, hi_vma). Eva is a fixed-base, non-PIE ET_EXEC, so any
    live reference to bytes in that range (a string table entry, a vtable
    slot, ...) has to show up this way -- a linked binary keeps no relocation
    table to consult instead. Returns [(file_offset, target_vma), ...]."""
    hits = []
    for target in range(lo_vma, hi_vma):
        needle = struct.pack("<I", target)
        start = 0
        while True:
            idx = data.find(needle, start)
            if idx == -1:
                break
            if exclude_off is None or not (exclude_off <= idx < exclude_off + exclude_len):
                hits.append((idx, target))
            start = idx + 1
    return hits


def patch_eva(data: bytes, verify_only: bool = False) -> bytes:
    out = bytearray(data)

    call_foff = _eva_vma_to_file(EVA_CALL_PATCH_VMA)
    cave_foff = _eva_vma_to_file(EVA_CAVE_VMA)
    nop_foff  = _eva_vma_to_file(EVA_NOP_START_VMA)
    nop_len   = EVA_NOP_END_VMA - EVA_NOP_START_VMA
    cave_len  = 206

    # Sanity checks
    cur_call = bytes(out[call_foff:call_foff + 5])
    cave_region = bytes(out[cave_foff:cave_foff + cave_len])
    patched_call = b"\xe8" + _eva_rel32(EVA_CALL_PATCH_VMA, EVA_CAVE_VMA + 0x0e)

    if cur_call == patched_call and bytes(out[nop_foff:nop_foff + nop_len]) == bytes([0x90] * nop_len):
        print(f"  Eva: already patched")
        return bytes(out)

    if cur_call != _EVA_EXPECTED_CALL:
        raise RuntimeError(
            f"Eva: unexpected bytes at CALL site (VMA {EVA_CALL_PATCH_VMA:#010x})\n"
            f"  expected: {_EVA_EXPECTED_CALL.hex()}  found: {cur_call.hex()}\n"
            f"  Binary may not be v3.2.1/3.2.2 or may already be differently patched.")

    nz = sum(1 for x in cave_region if x != 0)
    if nz > 0:
        # Non-zero bytes alone don't make the cave unsafe. .rodata string-merge
        # (SHF_MERGE|STRINGS) sections can leave orphaned tail fragments behind
        # once a recompile changes which strings get deduplicated (confirmed on
        # 3.2.3: a staircase of stray '\r' bytes here, not code or a live
        # string). What *would* make it unsafe is some other still-live string
        # tail-sharing into this exact range. Confirm by scanning the whole
        # binary for an absolute pointer landing inside it.
        refs = _scan_absolute_refs(data, EVA_CAVE_VMA, EVA_CAVE_VMA + cave_len,
                                    exclude_off=cave_foff, exclude_len=cave_len)
        if refs:
            detail = ", ".join(f"file+{off:#x}->{vma:#010x}" for off, vma in refs[:8])
            raise RuntimeError(
                f"Eva: cave region at VMA {EVA_CAVE_VMA:#010x} is not all zeros "
                f"({nz} non-zero bytes) AND is still referenced elsewhere in the "
                f"binary ({detail}) — unsafe to overwrite, check firmware version.")
        print(f"  Eva: cave region has {nz} non-zero byte(s) ({cave_region.hex()}) "
              f"but nothing else in the binary holds a pointer into this range "
              f"(scanned) — safe to overwrite as dead .rodata merge leftover.")

    cave_bytes = _assemble_eva_cave()
    code_start_vma = EVA_CAVE_VMA + 0x0e
    print(f"  Eva: cave at VMA {EVA_CAVE_VMA:#010x} file+{cave_foff:#010x}  "
          f"({len(cave_bytes)} bytes, code starts at {code_start_vma:#010x})")
    print(f"  Eva: CALL redirect at VMA {EVA_CALL_PATCH_VMA:#010x}  "
          f"{cur_call.hex()} → {patched_call.hex()}")
    print(f"  Eva: NOP sled {nop_len} bytes at VMA {EVA_NOP_START_VMA:#010x}")

    if not verify_only:
        out[cave_foff:cave_foff + len(cave_bytes)] = cave_bytes
        out[call_foff:call_foff + 5] = patched_call
        out[nop_foff:nop_foff + nop_len] = bytes([0x90] * nop_len)
        print(f"  Eva: patched (3 sites)")
    else:
        print(f"  Eva: verified (3 sites)")

    return bytes(out)


# ── MD5 helpers ───────────────────────────────────────────────────────────────

def md5(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()


def md5_file(path: Path) -> str:
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ── UpdateOS package builder ──────────────────────────────────────────────────

# pretar.sh: backs up originals; renames stock InstallEXs -> InstallEXs.real.
# No hard-coded firmware version checks — the host script already verified
# patch-site compatibility.  Reinstall is detected via the version marker
# written by a previous posttar.sh run.
PRETAR_SH = r"""#!/bin/sh
# Kronosology offline-patch updater — PRETAR
# Backs up original binaries before the patched tar is extracted.

umask 022

log() { echo "[pretar] $*"; }
die() { echo "[pretar] FATAL: $*" >&2; exit 1; }

BACKUP_DIR="/korg/rw/kronos_patcher_backup"
MARKER="$BACKUP_DIR/version"

# Reinstall: version marker present means we've run before; skip backup
if [ -f "$MARKER" ]; then
    log "Reinstall detected ($(cat "$MARKER" 2>/dev/null))"
    log "Skipping backup — tar will overwrite with pre-patched binaries"
    if [ -f /sbin/InstallEXs ] && [ ! -f /sbin/InstallEXs.real ]; then
        mv /sbin/InstallEXs /sbin/InstallEXs.real
        log "Renamed /sbin/InstallEXs -> /sbin/InstallEXs.real"
    fi
    exit 0
fi

# First install: back up everything we will overwrite
log "First-time install — backing up originals to $BACKUP_DIR"
mkdir -p "$BACKUP_DIR" || die "Cannot create $BACKUP_DIR"

for f in /sbin/loadmod.ko /sbin/loadoa; do
    if [ ! -f "$f" ]; then
        die "$f not found"
    fi
    cp -p "$f" "$BACKUP_DIR/" && log "  backed up $f"
done

for f in /sbin/Eva /sbin/InstallEXs; do
    if [ -f "$f" ]; then
        cp -p "$f" "$BACKUP_DIR/$(basename "$f")" && log "  backed up $f"
    fi
done

# Record stock checksums
{
    for f in loadmod.ko loadoa; do
        h=$(md5sum "/sbin/$f" 2>/dev/null | cut -d' ' -f1)
        [ -n "$h" ] && printf '%s  %s\n' "$h" "$f"
    done
} > "$BACKUP_DIR/originals.md5"

sync
log "Backup done."

# Rename stock InstallEXs so the tar places our wrapper at /sbin/InstallEXs
if [ -f /sbin/InstallEXs ] && [ ! -f /sbin/InstallEXs.real ]; then
    mv /sbin/InstallEXs /sbin/InstallEXs.real
    log "Renamed /sbin/InstallEXs -> /sbin/InstallEXs.real"
fi

log "Ready for tar extraction."
exit 0
"""

# posttar.sh template — @@VERSION@@ is substituted at build time.
# Runs after tar extraction; writes the version marker and does a one-shot
# authorisation of any already-installed EX libraries.
_POSTTAR_SH_TEMPLATE = r"""#!/bin/sh
# Kronosology offline-patch updater — POSTTAR

exec >> /korg/rw/kronos_patcher.log 2>&1
echo "--- posttar start $(date) ---"

log() { echo "[posttar] $*"; }

BACKUP_DIR="/korg/rw/kronos_patcher_backup"
mkdir -p "$BACKUP_DIR"

# Write version marker so pretar detects reinstalls
printf '%s\n' "@@VERSION@@" > "$BACKUP_DIR/version"
log "Version: @@VERSION@@"

# One-shot authorisation of already-installed EX libraries.
# oa_authgen.ko was just deployed to /sbin/ by the tar; load it temporarily.
OAAUTH=/proc/.oaauth
AUTHFILE=/korg/rw/Startup/AuthorizationStrings
OPTIONS=/korg/rw/Options

WE_LOADED=0
if [ ! -e "$OAAUTH" ] && [ -f /sbin/oa_authgen.ko ]; then
    /sbin/insmod /sbin/oa_authgen.ko
    log "insmod oa_authgen exit=$?"
    sleep 1
    WE_LOADED=1
fi

authorised=0
if [ -e "$OAAUTH" ] && [ -d "$OPTIONS" ]; then
    mkdir -p "$(dirname "$AUTHFILE")"
    AUTHTMP="${AUTHFILE}.tmp$$"
    > "$AUTHTMP"
    for opt_file in "$OPTIONS"/S*; do
        [ -f "$opt_file" ] || continue
        opt_id="${opt_file##*/}"
        uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
              "$opt_file" 2>/dev/null)
        [ -z "$uid" ] && continue
        [ "$uid" = "0" ] && continue
        if printf 'GEN:%s' "$opt_id" > "$OAAUTH" 2>/dev/null; then
            auth=$(cat "$OAAUTH" 2>/dev/null)
            if [ "${#auth}" = "24" ]; then
                printf '%s\n' "$auth" >> "$AUTHTMP"
                log "Authorised $opt_id"
                authorised=$((authorised + 1))
            fi
        fi
    done
    mv "$AUTHTMP" "$AUTHFILE"
    chown pocky:pocky "$AUTHFILE" 2>/dev/null || true
    log "Authorised $authorised existing EX libraries"
fi

[ "$WE_LOADED" = "1" ] && /sbin/rmmod oa_authgen 2>/dev/null || true
sync
log "Done."
echo "--- posttar end $(date) ---"
"""


def _make_posttar_sh(version: str) -> str:
    return _POSTTAR_SH_TEMPLATE.replace("@@VERSION@@", version)


def _make_eva_wrapper(exec_target: str) -> bytes:
    """
    Shell script deployed as /sbin/Eva.
    loadoa execs /sbin/Eva as its last startup step, after OmapNKS4Module.ko
    is loaded.  This wrapper insmod's oa_authgen.ko (which depends on
    OmapNKS4Module.ko exports), then exec's the real Eva binary.
    Errors from insmod are silently ignored — if it can't load, Eva still runs.
    """
    script = (
        "#!/bin/sh\n"
        "insmod /sbin/oa_authgen.ko 2>/dev/null || true\n"
        f"exec {exec_target} \"$@\"\n"
    )
    return script.encode()


def _tar_add_bytes(tf: tarfile.TarFile, arcname: str, data: bytes,
                   mode: int = 0o644) -> None:
    ti = tarfile.TarInfo(name=arcname)
    ti.size  = len(data)
    ti.mode  = mode
    ti.uid   = ti.gid = 0
    ti.uname = ti.gname = "root"
    tf.addfile(ti, BytesIO(data))


def _tar_add_dir(tf: tarfile.TarFile, arcname: str) -> None:
    ti = tarfile.TarInfo(name=arcname)
    ti.type  = tarfile.DIRTYPE
    ti.mode  = 0o755
    ti.uid   = ti.gid = 0
    ti.uname = ti.gname = "root"
    tf.addfile(ti)


def build_tar(oa_ko: bytes, korgusb: bytes,
              loadmod: bytes, loadoa: bytes,
              eva: bytes | None = None,
              install_exs: bytes | None = None,
              oa_authgen: bytes | None = None) -> bytes:
    """
    Build kronosology.tar.gz containing pre-patched binaries at sbin/ paths.

    /sbin/Eva is always deployed as a shell wrapper that insmod's oa_authgen.ko
    then exec's the real Eva binary.  When eva (patched binary bytes) is
    provided, it is deployed as /sbin/Eva.elf and the wrapper exec's that.
    When eva is None, the wrapper exec's the stock /korg/Eva/Eva from the
    cryptoloop mount — so oa_authgen.ko still loads at boot even without a
    patched Eva binary.
    """
    buf = BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tf:
        _tar_add_dir(tf, "sbin")
        _tar_add_bytes(tf, "sbin/loadmod.ko",            loadmod, 0o644)
        _tar_add_bytes(tf, "sbin/loadoa",                loadoa,  0o755)
        _tar_add_bytes(tf, "sbin/OA.ko",                 oa_ko,   0o644)
        _tar_add_bytes(tf, "sbin/KorgUsbAudioDriver.ko", korgusb, 0o644)

        if eva is not None:
            # Patched Eva binary + wrapper that exec's it
            _tar_add_bytes(tf, "sbin/Eva.elf",           eva,                               0o755)
            _tar_add_bytes(tf, "sbin/Eva",               _make_eva_wrapper("/sbin/Eva.elf"), 0o755)
        else:
            # No patched Eva — wrapper redirects to stock cryptoloop Eva
            # (loadoa is already redirected to /sbin/Eva, so this must exist)
            _tar_add_bytes(tf, "sbin/Eva",               _make_eva_wrapper("/korg/Eva/Eva"), 0o755)

        if oa_authgen is not None:
            _tar_add_bytes(tf, "sbin/oa_authgen.ko",     oa_authgen,   0o644)
        if install_exs is not None:
            _tar_add_bytes(tf, "sbin/InstallEXs",        install_exs,  0o755)
    return buf.getvalue()


def sign_package(staging: Path, version: str, source: str,
                 pretar: str, posttar: str) -> str:
    """Write install.info with SHA1(pretar + posttar + UPDATER_KEY) signature."""
    h = hashlib.sha1()
    h.update((staging / pretar).read_bytes())
    h.update((staging / posttar).read_bytes())
    h.update(UPDATER_KEY)
    sig = h.hexdigest()
    info = (
        f"VERSION={version}\n"
        f"SOURCE={source}\n"
        f"PRETARSCRIPT={pretar}\n"
        f"POSTTARSCRIPT={posttar}\n"
        f"SIGNATURE={sig}\n"
    )
    (staging / "install.info").write_text(info)
    return sig


README_TXT = """\
Kronosology offline-patch installer
=====================================

Pre-built by patch_firmware_offline.py.  All patching was done on the host;
no cryptoloop mount or live patching occurs on the Kronos during install.

What this package installs
--------------------------
  /sbin/loadmod.ko           patched (3 integrity-check bypasses)
  /sbin/loadoa               patched (3 path redirects to /sbin/)
  /sbin/OA.ko                patched (EX-auth bypass), extracted from Mod.img
  /sbin/KorgUsbAudioDriver.ko stock, extracted from Mod.img
  /sbin/Eva                  wrapper: insmod oa_authgen.ko then exec Eva binary
  /sbin/Eva.elf              patched Eva binary (absent if Eva.img not available)
  /sbin/oa_authgen.ko        kernel module: exposes /proc/.oaauth for auth generation
  /sbin/InstallEXs           our wrapper (stock binary moved to /sbin/InstallEXs.real)
                             (absent if InstallEXs/InstallEXs was not built)

loadoa is redirected to exec /sbin/Eva at startup.  /sbin/Eva is a shell
wrapper that insmod's oa_authgen.ko (making /proc/.oaauth available) then
exec's the real Eva binary.  This is how oa_authgen.ko is loaded at every
boot — no init script changes required.

Backups of the originals are saved to /korg/rw/kronos_patcher_backup/ by
pretar.sh before extraction.

To install
----------
1. Copy the contents of this folder to the root of a FAT-formatted USB stick.
2. Insert the stick into your Kronos.
3. Trigger an OS update: Global -> [Page Menu] -> Update System Software.
4. When the update completes: POWER-CYCLE the Kronos (full power-off, >= 60 s).
   DO NOT soft-reboot -- it may wedge the OmapNKS4 panel chip.

To revert (requires SSH / root access)
---------------------------------------
Restore the backups from /korg/rw/kronos_patcher_backup/:
  ssh root@<kronos-ip>
  cp /korg/rw/kronos_patcher_backup/loadmod.ko /sbin/loadmod.ko
  cp /korg/rw/kronos_patcher_backup/loadoa     /sbin/loadoa
  sync && reboot

Or reinstall the official Korg OS update from www.korg.com/support/.
"""


def build_package(staging: Path,
                  oa_ko: bytes, korgusb: bytes,
                  loadmod: bytes, loadoa: bytes,
                  eva: bytes | None = None,
                  install_exs: bytes | None = None,
                  oa_authgen: bytes | None = None,
                  version: str = "kronosology") -> None:
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    pretar_path = staging / "pretar.sh"
    pretar_path.write_text(PRETAR_SH)
    pretar_path.chmod(pretar_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    posttar_path = staging / "posttar.sh"
    posttar_path.write_text(_make_posttar_sh(version))
    posttar_path.chmod(posttar_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    tar_data = build_tar(oa_ko, korgusb, loadmod, loadoa, eva, install_exs, oa_authgen)
    (staging / "kronosology.tar.gz").write_bytes(tar_data)

    sig = sign_package(staging, version, "kronosology.tar.gz", "pretar.sh", "posttar.sh")
    (staging / "README.txt").write_text(README_TXT)

    print()
    print("================================================================")
    print("Build complete:")
    print("================================================================")
    for f in sorted(staging.iterdir()):
        size = f.stat().st_size if f.is_file() else 0
        print(f"  {f.name:40s}  {size:>12,d} bytes" if f.is_file() else f"  {f.name}/")
    print(f"  SIGNATURE: {sig}")
    print()
    if eva is None:
        print("  NOTE: Eva.img was not found — Eva binary was NOT patched.")
        print("        /sbin/Eva wrapper still loads oa_authgen.ko and exec's stock Eva.")
        print("        The front-panel Authorise button auto-auth will not work,")
        print("        but InstallEXs auto-auth will.")
        print()
    if oa_authgen is None:
        print("  WARNING: oa_authgen.ko not found — NOT included in package.")
        print("           Build it: cd ../auto-auth/oa_authgen && make KDIR=/path/to/linux-kronos")
        print("           Without it, Eva and InstallEXs auto-auth will not work.")
        print()
    if install_exs is None:
        print("  NOTE: InstallEXs/InstallEXs binary not found — wrapper NOT included.")
        print("        Build it first: cd InstallEXs && make")
        print()
    print(f"To install:")
    print(f"  1. Format a USB stick as FAT (or ext2)")
    print(f"  2. Copy contents of {staging}/ to the USB root")
    print(f"  3. Insert into Kronos and trigger Global -> OS Update")
    print(f"  4. POWER-CYCLE after completion (full off, >= 60 s)")


# ── verify mode ───────────────────────────────────────────────────────────────

def run_verify(tree: Path) -> int:
    mod_img  = tree / "korg" / "ro" / "Mod.img"
    eva_img  = tree / "korg" / "ro" / "Eva.img"
    loadmod  = tree / "sbin" / "loadmod.ko"
    loadoa   = tree / "sbin" / "loadoa"

    def _stock_label(m: str) -> str:
        entry = KNOWN_STOCK_MD5.get(m)
        return f"stock {entry[1]}" if entry else "UNKNOWN"

    ok = True

    print("=== loadmod.ko ===")
    lm_md5 = md5_file(loadmod)
    label = _stock_label(lm_md5)
    if label == "UNKNOWN" and lm_md5 == "fdbc4a1a4a094024b8f1f0e1443649c7":
        label = "already patched"
    print(f"  {loadmod}  md5={lm_md5}  ({label})")
    if "UNKNOWN" in label:
        print("  WARNING: unrecognised MD5 — patch offsets may not apply correctly")

    print()
    print("=== loadoa ===")
    la_md5 = md5_file(loadoa)
    label = _stock_label(la_md5)
    if label == "UNKNOWN" and la_md5 == "d17c26036fa0f51f5f4c0ef2f6f424bf":
        label = "already patched"
    print(f"  {loadoa}  md5={la_md5}  ({label})")

    print()
    print("=== OA.ko (from Mod.img) ===")
    print(f"  Decrypting {mod_img} ...")
    mod_data = decrypt_mod_img(mod_img)
    ext2 = Ext2Reader(mod_data)
    oa_data = ext2.read_file("OA.ko")
    oa_md5 = md5(oa_data)
    label = _stock_label(oa_md5)
    if label == "UNKNOWN" and oa_md5 == "e585d8ebb471a41ab6d9f77c88368bfe":
        label = "already patched (3.2.2)"
    elif label == "UNKNOWN" and oa_md5 == "163550b60b7508b2c0ba1fd314b0b944":
        label = "already patched (3.2.1)"
    print(f"  OA.ko: {len(oa_data):,d} bytes  md5={oa_md5}  ({label})")
    del mod_data, ext2

    print()
    print("=== Patch site verification (verify_only — no writes) ===")
    try:
        patch_oa_ko(oa_data, verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

    try:
        patch_loadmod_ko(loadmod.read_bytes(), verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

    try:
        patch_loadoa(loadoa.read_bytes(), verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

    if eva_img.exists():
        print()
        print("=== Eva (from Eva.img) ===")
        print(f"  Decrypting {eva_img} ...")
        try:
            eva_data_raw = decrypt_eva_img(eva_img)
            eva_ext2 = Ext2Reader(eva_data_raw)
            eva_data = eva_ext2.read_file("Eva")
            print(f"  Eva: {len(eva_data):,d} bytes  md5={md5(eva_data)}")
            del eva_data_raw, eva_ext2
            patch_eva(eva_data, verify_only=True)
        except (RuntimeError, FileNotFoundError) as e:
            print(f"  ERROR: {e}")
            ok = False
    else:
        print()
        print("=== Eva (from Eva.img) ===")
        print(f"  SKIPPED — {eva_img} not found")
        print("  /sbin/Eva wrapper will exec stock /korg/Eva/Eva instead.")

    print()
    if ok:
        print("All patch sites verified — safe to build with this firmware version.")
        return 0
    else:
        print("One or more patch sites failed verification.")
        return 1


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    HERE = Path(__file__).resolve().parent
    DEFAULT_OUT = HERE / "output" / "kronosology-offline-patched"

    p = argparse.ArgumentParser(
        description="Build an offline-patched Kronos USB update package.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("update_tree", metavar="<update_tree>",
                   help="Path to unpacked firmware tree (e.g. KRONOS_Update_3_2_2/mnt)")
    p.add_argument("--verify", action="store_true",
                   help="Check patch sites without writing anything")
    p.add_argument("-o", "--output", metavar="<dir>",
                   help=f"Output directory (default: {DEFAULT_OUT})")
    args = p.parse_args()

    tree = Path(args.update_tree).resolve()
    if not tree.is_dir():
        p.error(f"update_tree {tree!r} is not a directory")

    mod_img  = tree / "korg" / "ro" / "Mod.img"
    eva_img  = tree / "korg" / "ro" / "Eva.img"
    loadmod  = tree / "sbin" / "loadmod.ko"
    loadoa_p = tree / "sbin" / "loadoa"

    for path in (mod_img, loadmod, loadoa_p):
        if not path.exists():
            p.error(f"Required file not found: {path}")

    if not eva_img.exists():
        print(f"NOTE: {eva_img} not found — Eva binary patch will be skipped.")
        print("      /sbin/Eva wrapper will still load oa_authgen.ko at boot.")
        print("      The front-panel Authorise button will not auto-auth.")
        print()

    # ── Find oa_authgen.ko ────────────────────────────────────────────────────
    # Look in the sibling auto-auth directory (the canonical location).
    oa_authgen_bin = HERE.parent / "auto-auth" / "oa_authgen" / "oa_authgen.ko"
    oa_authgen_data = None
    if oa_authgen_bin.exists():
        oa_authgen_data = oa_authgen_bin.read_bytes()
        print(f"  oa_authgen.ko: {len(oa_authgen_data):,d} bytes ✓  ({oa_authgen_bin})")
    else:
        print(f"WARNING: oa_authgen.ko not found at {oa_authgen_bin}")
        print("         Eva and InstallEXs auto-auth will not work without it.")
        print("         Build: cd auto-auth/oa_authgen && make KDIR=/path/to/linux-kronos")
        print()

    # ── Find InstallEXs wrapper ───────────────────────────────────────────────
    install_exs_bin = HERE / "InstallEXs" / "InstallEXs"
    if not install_exs_bin.exists():
        print(f"[build] InstallEXs binary not found — attempting make ...")
        import subprocess
        subprocess.run(["make", "-C", str(HERE / "InstallEXs"), "-s"], check=False)
    if not install_exs_bin.exists():
        print(f"NOTE: {install_exs_bin} not found — InstallEXs wrapper will NOT be included.")
        print("      Build it first: cd InstallEXs && make")
        print()

    if args.verify:
        return run_verify(tree)

    staging = Path(args.output).resolve() if args.output else DEFAULT_OUT

    # ── Identify firmware version ─────────────────────────────────────────────
    print("Identifying firmware version...")
    lm_md5 = md5_file(loadmod)
    la_md5 = md5_file(loadoa_p)

    lm_entry = KNOWN_STOCK_MD5.get(lm_md5)
    fw_version = lm_entry[1] if lm_entry else "unknown"
    if lm_entry:
        print(f"  loadmod.ko: stock {lm_entry[1]} ✓")
    else:
        print(f"  loadmod.ko: md5={lm_md5} — unrecognised version (patching will proceed)")

    la_entry = KNOWN_STOCK_MD5.get(la_md5)
    if la_entry:
        print(f"  loadoa:     stock {la_entry[1]} ✓")
    else:
        print(f"  loadoa:     md5={la_md5} — unrecognised version (patching will proceed)")

    version_str = f"kronosology-{fw_version}"

    # ── Decrypt Mod.img → extract OA.ko and KorgUsbAudioDriver.ko ───────────
    print()
    mod_data = decrypt_mod_img(mod_img)
    ext2 = Ext2Reader(mod_data)
    print("  Extracting OA.ko from Mod.img ...")
    oa_data = ext2.read_file("OA.ko")
    oa_entry = KNOWN_STOCK_MD5.get(md5(oa_data))
    if oa_entry:
        print(f"  OA.ko: {len(oa_data):,d} bytes, stock {oa_entry[1]} ✓")
    else:
        print(f"  OA.ko: {len(oa_data):,d} bytes, md5={md5(oa_data)} — unrecognised version")

    print("  Extracting KorgUsbAudioDriver.ko from Mod.img ...")
    korgusb_data = ext2.read_file("KorgUsbAudioDriver.ko")
    print(f"  KorgUsbAudioDriver.ko: {len(korgusb_data):,d} bytes")
    del mod_data, ext2

    # ── Decrypt Eva.img → extract Eva (optional) ─────────────────────────────
    eva_data = None
    if eva_img.exists():
        print()
        print(f"Decrypting Eva.img ...")
        eva_raw = decrypt_eva_img(eva_img)
        eva_ext2 = Ext2Reader(eva_raw)
        print("  Extracting Eva from Eva.img ...")
        eva_data = eva_ext2.read_file("Eva")
        print(f"  Eva: {len(eva_data):,d} bytes  md5={md5(eva_data)}")
        del eva_raw, eva_ext2

    # ── Patch OA.ko ──────────────────────────────────────────────────────────
    print()
    print("Patching OA.ko ...")
    oa_patched = patch_oa_ko(oa_data)
    print(f"  OA.ko patched MD5: {md5(oa_patched)}")

    # ── Patch loadmod.ko ─────────────────────────────────────────────────────
    print()
    print("Patching loadmod.ko ...")
    lm_patched = patch_loadmod_ko(loadmod.read_bytes())
    print(f"  loadmod.ko patched MD5: {md5(lm_patched)}")

    # ── Patch loadoa ─────────────────────────────────────────────────────────
    print()
    print("Patching loadoa ...")
    la_patched = patch_loadoa(loadoa_p.read_bytes())
    print(f"  loadoa patched MD5: {md5(la_patched)}")

    # ── Patch Eva ────────────────────────────────────────────────────────────
    eva_patched = None
    if eva_data is not None:
        print()
        print("Patching Eva ...")
        eva_patched = patch_eva(eva_data)
        print(f"  Eva patched MD5: {md5(eva_patched)}")

    # ── Load InstallEXs wrapper ──────────────────────────────────────────────
    install_exs_data = install_exs_bin.read_bytes() if install_exs_bin.exists() else None
    if install_exs_data:
        print(f"  InstallEXs: {len(install_exs_data):,d} bytes ✓")

    # ── Build package ────────────────────────────────────────────────────────
    print()
    print(f"Building package in {staging} ...")
    build_package(staging, oa_patched, korgusb_data, lm_patched, la_patched,
                  eva_patched, install_exs_data, oa_authgen_data, version_str)

    print()
    print("Done.  Install instructions:")
    print("  1. Format a USB stick as FAT")
    print(f"  2. cp -r {staging}/* /media/your-usb-stick/")
    print("  3. Insert into Kronos and trigger Global -> OS Update")
    print("  4. POWER-CYCLE after completion (full off >= 60 s, NOT soft-reboot)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
