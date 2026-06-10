#!/usr/bin/env python3
"""
patch_firmware_offline.py — Offline Kronos firmware patcher + USB package builder.

Decrypts Mod.img, patches OA.ko, loadmod.ko, and loadoa entirely on the host,
then produces a signed Korg UpdateOS package that delivers pre-patched binaries
via the tar payload.  The package is functionally equivalent to the one built by
build_updater.py / kronos_patcher.sh, but all patching happens offline, so the
Kronos never needs to mount a cryptoloop or run a patcher script during install.

Patch strategy:
  OA.ko     — 11 section-relative patch sites (same table as patch_oa_ko.py).
               Survives minor firmware updates that don't restructure these
               functions.  No readelf dependency — ELF parsing is pure Python.
  loadmod.ko — Symbol-relative: looks up 'init_module' and 'bbbbbbbba12' in
               .symtab to find the exact patch locations; falls back to a byte-
               pattern search if the obfuscated symbol name changes.
  loadoa    — String replacement: searches for '/korg/Mod/OA.ko' and
               '/korg/Mod/KorgUsbAudioDriver.ko' and overwrites them with
               null-padded /sbin/ paths (same length, always safe).

Usage:
  python3 patch_firmware_offline.py <update_tree>
  python3 patch_firmware_offline.py <update_tree> --verify
  python3 patch_firmware_offline.py <update_tree> -o <output_dir>

<update_tree>: path to an unpacked Kronos firmware tree, e.g.:
  KRONOS_Update_3_2_2/mnt
Expected layout:
  <update_tree>/korg/ro/Mod.img     (encrypted ext2; OA.ko and KorgUsbAudioDriver.ko inside)
  <update_tree>/sbin/loadmod.ko
  <update_tree>/sbin/loadoa

Output (default: <this_script_dir>/output/kronosology-offline-patched/):
  install.info          signed package metadata — UpdateOS reads this first
  kronosology.tar.gz    pre-patched binaries extracted to / on the Kronos
  pretar.sh             stock-MD5 verify + backup only (no patching needed)
  README.txt

To install on a Kronos (no SSH needed):
  1. Build: python3 patch_firmware_offline.py <update_tree>
  2. Copy output contents to the root of a FAT-formatted USB stick
  3. Insert stick into Kronos, trigger Global → OS Update from the front panel
  4. After completion, POWER-CYCLE (full off ≥ 60 s).  Do NOT soft-reboot.

For --verify mode the script decrypts Mod.img, extracts OA.ko, and checks every
patch site for expected bytes.  No files are written.  Useful for confirming
compatibility with a new firmware version before committing to a build.

Requires: cryptography  (pip install cryptography)
Tested on: Korg Kronos OS 3.2.2
"""

import argparse
import hashlib
import os
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

# ── constants ────────────────────────────────────────────────────────────────

SECTOR   = 512
VERSION  = "kronosology-3.2.2"
EXT2_MAGIC = 0xEF53

# Universal cryptoloop AES-256-CBC key for Mod.img.
# 31 printable ASCII chars + trailing \x00 (Korg HexEncode quirk).
MOD_KEY = bytes.fromhex("6133333661313563643834316563383932366239396537633338383465616100")

# UpdateOS package signing key (from UpdateOS .data section VMA 0x0813bac8)
UPDATER_KEY = bytes([0x13, 0xd0, 0xaf, 0xef, 0xe0, 0x3c, 0x9b, 0x92,
                     0x16, 0x2f, 0xae, 0xff, 0x77, 0x53, 0x55, 0xe1])

# Stock MD5s indexed by firmware version where the file first appeared.
# The script accepts any known-stock binary; only the patch bytes differ per version.
KNOWN_STOCK_MD5 = {
    # loadmod.ko
    "d1697c9b1c478c0dcdfaef71516fe5f2": ("loadmod.ko", "3.2.1"),
    "d9d56475be6ccb74e9001b84b64046f7": ("loadmod.ko", "3.2.2"),
    # loadoa (unchanged across 3.2.1 → 3.2.2)
    "8a3d61f3332d7bcf694e8c05845b4754": ("loadoa",     "3.2.x"),
    # OA.ko (from Mod.img)
    "955636c2b11a70a1dbecefaaa7bd4f80": ("OA.ko",      "3.2.1"),
    "39fec7465fd7886ed8099c9cb85e2cdc": ("OA.ko",      "3.2.2"),
    # KorgUsbAudioDriver.ko (unchanged across versions)
    "29fbd20cf729980e1cffd670391256b5": ("KorgUsbAudioDriver.ko", "all"),
}

MD5_LOADMOD_STOCK   = "d9d56475be6ccb74e9001b84b64046f7"
MD5_LOADOA_STOCK    = "8a3d61f3332d7bcf694e8c05845b4754"
MD5_OA_STOCK        = "39fec7465fd7886ed8099c9cb85e2cdc"
MD5_KORGUSB_STOCK   = "29fbd20cf729980e1cffd670391256b5"
MD5_USBMIDI_STOCK   = "fae9ff96711b86791a83272e5affb334"

# ── OA.ko patch table (section-relative) ─────────────────────────────────────
# Identical to patch_oa_ko.py — 11 patch runs, addressed by ELF section name +
# section-relative offset.  Verified on 3.2.1 and 3.2.2.
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

    # Two IsUsingAnyUnauthorizedMultisamples specializations in main .text
    (".text", 0x13c7d0,
        "8b40088b52280fbf40088b44023485c00f95c0",
        "31c0c390909090909090909090909090909090"),

    (".text", 0x155d30,
        "8b40088b52280fbf40088b44020485c00f95c0",
        "31c0c390909090909090909090909090909090"),

    # Four IsUsingAnyUnauthorizedMultisamples specializations in COMDAT sections.
    # If a future firmware adds a 5th COMDAT class, these four still get patched;
    # the new one is reported as a warning rather than a fatal error.
    (".text._ZN17CSTGPCMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",    0x0,
        "8b92d0010000", "31c0c3909090"),
    (".text._ZN21CSTGPluckedModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv", 0x0,
        "8b9278030000", "31c0c3909090"),
    (".text._ZN17CSTGVPMModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",     0x0,
        "8b9225040000", "31c0c3909090"),
    (".text._ZN19CSTGPianoModelPatch34IsUsingAnyUnauthorizedMultisamplesEPv",   0x0,
        "8b92ac000000", "31c0c3909090"),
]

# ── loadmod.ko patches (symbol-relative) ─────────────────────────────────────
# (symbol_name, within_symbol_offset, orig_hex, patched_hex)
# If the symbol is not found by name, falls back to a byte-pattern search in a
# ±16 KB window around the known-good 3.2.2 file offset.
LOADMOD_PATCHES = [
    # NOP test+jne after VerifyCodeIntegrityMd5 (init_module check 1)
    ("init_module", 0x39,  "85c00f85a3000000", "9090909090909090"),
    # NOP jne after RetrieveSecurityICKey dongle gate (init_module check 4)
    ("init_module", 0xBD,  "7547",             "9090"),
    # JMP past inner 16-byte MD5 check inside RetrieveSecurityICKey (bbbbbbbba12)
    ("bbbbbbbba12", 0x170, "0f85e7feffff",     "e91e01000090"),
]

# Fallback absolute file offsets for 3.2.2 — used when symbol lookup fails
_LOADMOD_FALLBACK = {
    ("init_module", 0x39):  22317,
    ("init_module", 0xBD):  22449,
    ("bbbbbbbba12", 0x170): 16304,
}

# ── loadoa patches (string replacement) ──────────────────────────────────────
# Searches for the original string anywhere in the binary and replaces it.
# Both strings are the same length before and after (null-padded), so the
# replacement is always safe regardless of file offset.
LOADOA_PATCHES = [
    (b"/korg/Mod/OA.ko\x00",
     b"/sbin/OA.ko\x00\x00\x00\x00\x00"),
    (b"/korg/Mod/KorgUsbAudioDriver.ko\x00",
     b"/sbin/KorgUsbAudioDriver.ko\x00\x00\x00\x00\x00"),
]

# ── cryptoloop decryption ─────────────────────────────────────────────────────

def _decrypt_sector(key: bytes, sector_num: int, ct: bytes) -> bytes:
    iv = struct.pack("<I", sector_num & 0xFFFFFFFF) + b"\x00" * 12
    ciph = Cipher(algorithms.AES(key), modes.CBC(iv), backend=default_backend())
    return ciph.decryptor().update(ct)


def decrypt_mod_img(img_path: Path) -> bytearray:
    """Decrypt Mod.img into memory and return the plaintext ext2 image."""
    size = img_path.stat().st_size
    total = size // SECTOR
    data = bytearray(size)
    print(f"  Decrypting {img_path.name} ({size // (1024*1024)} MB, {total} sectors)...")
    with open(img_path, "rb") as f:
        for i in range(total):
            if i and (i % 8192) == 0:
                print(f"    {i * 100 // total:3d}%", end="\r", flush=True)
            ct = f.read(SECTOR)
            pt = _decrypt_sector(MOD_KEY, i, ct)
            data[i * SECTOR:(i + 1) * SECTOR] = pt
        tail = f.read()
        if tail:
            data[total * SECTOR:] = tail
    print("    100%")

    magic, = struct.unpack_from("<H", data, 2 * SECTOR + 56)
    if magic != EXT2_MAGIC:
        raise RuntimeError(f"Decrypted Mod.img does not have ext2 magic (got 0x{magic:04X})")
    return data


# ── ext2 reader ───────────────────────────────────────────────────────────────

class Ext2Reader:
    """Minimal ext2 reader — direct, single-indirect, and double-indirect blocks."""

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
        # Block group descriptor table: block 2 for 1 KB blocks, block 1 otherwise
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

        for i in range(12):                    # direct blocks
            if remaining <= 0:
                break
            take(ptrs[i])

        if remaining > 0 and ptrs[12]:         # single-indirect
            ind = struct.unpack_from(f"<{pts_per_block}I", self._block(ptrs[12]))
            for bn in ind:
                if remaining <= 0:
                    break
                take(bn)

        if remaining > 0 and ptrs[13]:         # double-indirect
            ind1 = struct.unpack_from(f"<{pts_per_block}I", self._block(ptrs[13]))
            for b1 in ind1:
                if remaining <= 0 or b1 == 0:
                    break
                ind2 = struct.unpack_from(f"<{pts_per_block}I", self._block(b1))
                for bn in ind2:
                    if remaining <= 0:
                        break
                    take(bn)

        if remaining > 0 and ptrs[14]:         # triple-indirect (unlikely for these files)
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
        inode_num = 2  # root directory
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
      symbols: {name: (section_index, value, size)}
      section_file_offsets: {section_index: file_offset}
    Only includes STT_FUNC symbols with a defined section (shndx not SHN_UNDEF/ABS).
    """
    e_shoff, e_shentsize, e_shnum, shstr_foff = _elf32_shstrtab_off(data)

    # Build section file offset table and find .symtab / .strtab
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
        if st_shndx == 0 or st_shndx >= 0xff00:  # SHN_UNDEF, SHN_ABS, etc.
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
            # COMDAT sections can be legitimately absent in some firmware versions
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
            # Fallback: byte-pattern search in a ±16 KB window around the known offset
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
            # Check if already patched
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

PRETAR_SH = """\
#!/bin/sh
# Kronosology offline-patch updater — PRETAR (OS 3.2.2)
#
# Runs as root via UpdateOS BEFORE extracting the tar payload.
# The tar payload contains pre-patched binaries; this script only verifies
# stock MD5s and backs up the originals so --revert remains possible.
echo "============================================================"
echo "Kronosology offline patcher (3.2.2) — UpdateOS PRETAR"
echo "============================================================"
set -e
umask 022

MD5_LOADMOD_STOCK="d9d56475be6ccb74e9001b84b64046f7"
MD5_LOADMOD_PATCHED="fdbc4a1a4a094024b8f1f0e1443649c7"
MD5_LOADOA_STOCK="8a3d61f3332d7bcf694e8c05845b4754"
MD5_USBMIDI_STOCK="fae9ff96711b86791a83272e5affb334"

log() { echo "[pretar] $*"; }
die() { echo "[pretar] FATAL: $*" >&2; exit 1; }
md5_of() { [ -f "$1" ] && md5sum "$1" 2>/dev/null | cut -d' ' -f1 || echo "_missing_"; }

BACKUP_DIR="/korg/rw/kronos_patcher_backup"

# Idempotent: if loadmod is already patched, the tar will overwrite with the
# same patched content anyway — no backup needed for a re-install.
GOT_LOADMOD=$(md5_of /sbin/loadmod.ko)
if [ "$GOT_LOADMOD" = "$MD5_LOADMOD_PATCHED" ]; then
    log "Already patched — skipping backup (tar will re-apply pre-patched binaries)"
    exit 0
fi

# Refuse to run on an unrecognised firmware version.
[ "$GOT_LOADMOD" = "$MD5_LOADMOD_STOCK" ] || \\
    die "/sbin/loadmod.ko is not stock 3.2.2 (md5=$GOT_LOADMOD)"
[ "$(md5_of /sbin/loadoa)" = "$MD5_LOADOA_STOCK" ] || \\
    die "/sbin/loadoa is not stock 3.2.2"
[ "$(md5_of /sbin/USBMidiAccessory.ko)" = "$MD5_USBMIDI_STOCK" ] || \\
    die "/sbin/USBMidiAccessory.ko is not stock 3.2.2 V1 variant"

log "Stock binaries verified."
log "Backing up originals to $BACKUP_DIR ..."
mkdir -p "$BACKUP_DIR"
cp -p /sbin/loadmod.ko          "$BACKUP_DIR/loadmod.ko"
cp -p /sbin/loadoa              "$BACKUP_DIR/loadoa"
cp -p /sbin/USBMidiAccessory.ko "$BACKUP_DIR/USBMidiAccessory.ko"
printf '%s  loadmod.ko\\n%s  loadoa\\n%s  USBMidiAccessory.ko\\n' \\
    "$MD5_LOADMOD_STOCK" "$MD5_LOADOA_STOCK" "$MD5_USBMIDI_STOCK" \\
    > "$BACKUP_DIR/originals.md5"
sync
log "Backup done. UpdateOS will now extract pre-patched binaries to /sbin/."
exit 0
"""


def _tar_add_bytes(tf: tarfile.TarFile, arcname: str, data: bytes,
                   mode: int = 0o644) -> None:
    """Add raw bytes as a tar entry."""
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
              loadmod: bytes, loadoa: bytes) -> bytes:
    """Build kronosology.tar.gz containing pre-patched binaries at sbin/ paths."""
    buf = BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tf:
        _tar_add_dir(tf, "sbin")
        _tar_add_bytes(tf, "sbin/loadmod.ko",           loadmod, 0o644)
        _tar_add_bytes(tf, "sbin/loadoa",               loadoa,  0o755)
        _tar_add_bytes(tf, "sbin/OA.ko",                oa_ko,   0o644)
        _tar_add_bytes(tf, "sbin/KorgUsbAudioDriver.ko", korgusb, 0o644)
    return buf.getvalue()


def sign_install_info(staging: Path, version: str, source: str,
                      pretar: str) -> str:
    """Compute SHA1(pretar_content + UPDATER_KEY) and write install.info."""
    h = hashlib.sha1()
    h.update((staging / pretar).read_bytes())
    h.update(UPDATER_KEY)
    sig = h.hexdigest()
    info = f"VERSION={version}\nSOURCE={source}\nPRETARSCRIPT={pretar}\nSIGNATURE={sig}\n"
    (staging / "install.info").write_text(info)
    return sig


README_TXT = """\
Kronosology offline-patch installer (OS 3.2.2)
===============================================

Pre-built by patch_firmware_offline.py.  All patching was done on the host;
no cryptoloop mount or live patching occurs on the Kronos during install.

What this package installs
--------------------------
  /sbin/loadmod.ko           patched (3 integrity-check bypasses)
  /sbin/loadoa               patched (path redirects: /korg/Mod/ -> /sbin/)
  /sbin/OA.ko                patched (66 EX-auth bypass patches), copied from Mod.img
  /sbin/KorgUsbAudioDriver.ko stock, copied from Mod.img

Backups of the originals are saved to /korg/rw/kronos_patcher_backup/ by the
pretar.sh script before extraction.

To install
----------
1. Copy the contents of this folder to the root of a FAT-formatted USB stick.
2. Insert the stick into your Kronos.
3. Trigger an OS update: Global -> [Page Menu] -> Update System Software.
4. When the update completes: POWER-CYCLE the Kronos (full power-off, >= 60 s).
   DO NOT soft-reboot -- it may wedge the OmapNKS4 panel chip.

To revert (requires SSH / root access)
---------------------------------------
  ssh root@<kronos-ip>
  sh /korg/rw/kronos_patcher_backup/../kronos_patcher.sh --revert
  # then power-cycle

Or reinstall the official Korg OS update from www.korg.com/support/.
"""


def build_package(staging: Path,
                  oa_ko: bytes, korgusb: bytes,
                  loadmod: bytes, loadoa: bytes) -> None:
    if staging.exists():
        import shutil as _sh
        _sh.rmtree(staging)
    staging.mkdir(parents=True)

    # pretar.sh
    pretar_path = staging / "pretar.sh"
    pretar_path.write_text(PRETAR_SH)
    pretar_path.chmod(pretar_path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    # tar payload
    tar_data = build_tar(oa_ko, korgusb, loadmod, loadoa)
    (staging / "kronosology.tar.gz").write_bytes(tar_data)

    # signed install.info
    sig = sign_install_info(staging, VERSION, "kronosology.tar.gz", "pretar.sh")

    # README
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
    print(f"To install:")
    print(f"  1. Format a USB stick as FAT (or ext2)")
    print(f"  2. Copy contents of {staging}/ to the USB root")
    print(f"  3. Insert into Kronos and trigger Global -> OS Update")
    print(f"  4. POWER-CYCLE after completion (full off, >= 60 s)")


# ── verify mode ───────────────────────────────────────────────────────────────

def run_verify(tree: Path) -> int:
    """Check all patch sites without writing anything.  Returns 0 if all good."""
    mod_img  = tree / "korg" / "ro" / "Mod.img"
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

    print()
    print("=== Patch site verification (verify_only — no writes) ===")
    try:
        patch_oa_ko(oa_data, verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

    try:
        lm_data = loadmod.read_bytes()
        patch_loadmod_ko(lm_data, verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

    try:
        la_data = loadoa.read_bytes()
        patch_loadoa(la_data, verify_only=True)
    except RuntimeError as e:
        print(f"  ERROR: {e}")
        ok = False

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
    loadmod  = tree / "sbin" / "loadmod.ko"
    loadoa_p = tree / "sbin" / "loadoa"

    for path in (mod_img, loadmod, loadoa_p):
        if not path.exists():
            p.error(f"Required file not found: {path}")

    if args.verify:
        return run_verify(tree)

    staging = Path(args.output).resolve() if args.output else DEFAULT_OUT

    # ── Verify stock MD5s ────────────────────────────────────────────────────
    print("Verifying source file MD5s...")
    lm_md5 = md5_file(loadmod)
    la_md5 = md5_file(loadoa_p)

    lm_entry = KNOWN_STOCK_MD5.get(lm_md5)
    if lm_entry:
        print(f"  loadmod.ko: stock {lm_entry[1]} ✓")
    else:
        print(f"  WARNING: loadmod.ko md5={lm_md5} — unrecognised stock version")
        print("  Proceeding, but patches may fail if firmware layout has changed")

    la_entry = KNOWN_STOCK_MD5.get(la_md5)
    if la_entry:
        print(f"  loadoa:     stock {la_entry[1]} ✓")
    else:
        print(f"  WARNING: loadoa md5={la_md5} — unrecognised stock version")

    # ── Decrypt Mod.img and extract OA.ko / KorgUsbAudioDriver.ko ───────────
    print()
    mod_data = decrypt_mod_img(mod_img)
    ext2 = Ext2Reader(mod_data)
    print("  Extracting OA.ko from Mod.img ...")
    oa_data = ext2.read_file("OA.ko")
    oa_md5 = md5(oa_data)
    oa_entry = KNOWN_STOCK_MD5.get(oa_md5)
    if oa_entry:
        print(f"  OA.ko: {len(oa_data):,d} bytes, stock {oa_entry[1]} ✓")
    else:
        print(f"  WARNING: OA.ko md5={oa_md5} — unrecognised stock version")

    print("  Extracting KorgUsbAudioDriver.ko from Mod.img ...")
    korgusb_data = ext2.read_file("KorgUsbAudioDriver.ko")
    kusb_md5 = md5(korgusb_data)
    if kusb_md5 != MD5_KORGUSB_STOCK:
        print(f"  WARNING: KorgUsbAudioDriver.ko md5={kusb_md5} (expected {MD5_KORGUSB_STOCK})")
    else:
        print(f"  KorgUsbAudioDriver.ko: {len(korgusb_data):,d} bytes, stock ✓")

    # Free the large Mod.img decryption buffer (OA.ko + KorgUsbAudio extracted)
    del mod_data, ext2

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

    # ── Build package ────────────────────────────────────────────────────────
    print()
    print(f"Building package in {staging} ...")
    build_package(staging, oa_patched, korgusb_data, lm_patched, la_patched)

    print()
    print("Done.  Install instructions:")
    print("  1. Format a USB stick as FAT")
    print(f"  2. cp -r {staging}/* /media/your-usb-stick/")
    print("  3. Insert into Kronos and trigger Global -> OS Update")
    print("  4. POWER-CYCLE after completion (full off >= 60 s, NOT soft-reboot)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
