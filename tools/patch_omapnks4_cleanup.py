#!/usr/bin/env python3
"""
Patch OmapNKS4Module.ko to add usb_reset_device(*sDeviceInstance) call to its
teardown path. This addresses the USB-wedge-after-reboot issue by forcing a
device-level reset before USB unregister, which causes the chip firmware to
re-initialise from a clean state and also cancels any in-flight URBs.

Strategy
--------
- Repurpose `COmapNKS4Driver_Cleanup` (ghidra 0x4980, file 0x49c0): a NOP function
  that is currently `ret` + 13 bytes of padding, called exactly once (from
  `CleanupOmapNKS4Driver` at ghidra 0x0e41) immediately before stg_usb_deregister.
- Overwrite its 13-byte body with:
      mov  [sDeviceInstance], %eax   ; a1 20 04 00 00   (R_386_32 vs .bss, +0x420)
      mov  [%eax], %eax              ; 8b 00           (deref to get udev*)
      call usb_reset_device          ; e8 fc ff ff ff  (R_386_PC32 vs new sym)
      ret                            ; c3
- Add `usb_reset_device` as a new SHN_UNDEF symbol at the end of .symtab.
- Add its name to the end of .strtab.
- Add two new R_386 relocations to .rel.text.

Section layout strategy
-----------------------
.strtab is already the last section in the file — we can just grow it by
appending bytes (sh_offset stays, sh_size += new bytes).

.symtab and .rel.text are not at the end, so we relocate them: append a full
new copy of each section at end-of-file (preserving original entries plus our
new ones). Update section header table sh_offset+sh_size for both. The original
bytes become dead space which the kernel ignores.

The section header table itself does not move; we just update three entries.
"""
import sys, struct, shutil, hashlib

SRC = '/mnt/source/Kronos/dump from kronos/sbin/OmapNKS4Module.ko'
DST = '/mnt/source/Kronos/patched/OmapNKS4Module.ko'

# ELF constants
SHT_NULL     = 0
SHT_PROGBITS = 1
SHT_SYMTAB   = 2
SHT_STRTAB   = 3
SHT_REL      = 9
R_386_NONE   = 0
R_386_32     = 1
R_386_PC32   = 2
STB_GLOBAL   = 1
STT_NOTYPE   = 0
SHN_UNDEF    = 0

# Layout of the original file (verified via readelf -S)
TEXT_SH_IDX        = 2          # .text
RELTEXT_SH_IDX     = 3          # .rel.text
BSS_SH_IDX         = 29         # .bss
SYMTAB_SH_IDX      = 33         # .symtab
STRTAB_SH_IDX      = 34         # .strtab

# Original sections (file offsets, sizes)
TEXT_OFF, TEXT_SIZE        = 0x40,    0x8c71
RELTEXT_OFF, RELTEXT_SIZE  = 0xba3c,  0x40d0
SYMTAB_OFF, SYMTAB_SIZE    = 0x10524, 0x2280   # 552 entries x 16 bytes
STRTAB_OFF, STRTAB_SIZE    = 0x127a4, 0x3755   # last section in file

# Ghidra/section-relative addresses
HELPER_GADDR = 0x4980                    # COmapNKS4Driver_Cleanup body
HELPER_FOFF  = TEXT_OFF + HELPER_GADDR   # = 0x49c0
SDEVICEINSTANCE_BSS_OFF = 0x460          # sDeviceInstance is .bss + 0x460
                                         # confirmed via readelf -s (st_value=0x460
                                         # for symbol sDeviceInstance in section .bss).
                                         # Ghidra displays it at addr 0xb660 with .bss
                                         # placed at 0xb200 — the file sh_offset 0xb240
                                         # is unrelated to the linker-assigned offset.

# Symbol info index for .bss (the section symbol). From existing relocs we saw
# r_info = 0x00001301 for .bss → sym index = 0x13.
BSS_SYM_IDX = 0x13


def section_header_entry(d, idx):
    """Read one Elf32_Shdr from the SHT."""
    shoff = struct.unpack_from('<I', d, 0x20)[0]   # e_shoff
    shentsize = struct.unpack_from('<H', d, 0x2e)[0]  # e_shentsize, should be 40
    assert shentsize == 40
    base = shoff + idx * shentsize
    return list(struct.unpack_from('<10I', d, base)), base


def write_sh_fields(buf, sh_base, sh_offset=None, sh_size=None):
    if sh_offset is not None:
        struct.pack_into('<I', buf, sh_base + 0x10, sh_offset)
    if sh_size is not None:
        struct.pack_into('<I', buf, sh_base + 0x14, sh_size)


def main():
    print(f'reading  {SRC}')
    with open(SRC, 'rb') as f:
        orig = f.read()
    buf = bytearray(orig)

    # ---- sanity checks -------------------------------------------------
    assert buf[0:4] == b'\x7fELF', 'not an ELF'
    text_sh, text_sh_base       = section_header_entry(buf, TEXT_SH_IDX)
    reltext_sh, reltext_sh_base = section_header_entry(buf, RELTEXT_SH_IDX)
    symtab_sh, symtab_sh_base   = section_header_entry(buf, SYMTAB_SH_IDX)
    strtab_sh, strtab_sh_base   = section_header_entry(buf, STRTAB_SH_IDX)

    assert text_sh[4] == TEXT_OFF,       f'text off mismatch {text_sh[4]:x}'
    assert reltext_sh[4] == RELTEXT_OFF, f'reltext off mismatch {reltext_sh[4]:x}'
    assert symtab_sh[4] == SYMTAB_OFF,   f'symtab off mismatch {symtab_sh[4]:x}'
    assert strtab_sh[4] == STRTAB_OFF,   f'strtab off mismatch {strtab_sh[4]:x}'
    assert strtab_sh[5] == STRTAB_SIZE,  f'strtab size mismatch'

    # Verify slack at COmapNKS4Driver_Cleanup
    expected_slack = bytes.fromhex('c3eb0d90909090909090909090909090')
    actual_slack = bytes(buf[HELPER_FOFF:HELPER_FOFF + 16])
    assert actual_slack == expected_slack, \
        f'unexpected bytes at COmapNKS4Driver_Cleanup: {actual_slack.hex()}'

    print(f'orig file size:  {len(orig):#x}')
    print(f'original .strtab ends at {STRTAB_OFF + STRTAB_SIZE:#x}')

    # ---- 1. Patch the helper body in .text ------------------------------
    # sDeviceInstance already holds the struct usb_device* pointer. usb_device's
    # first field is `devnum`, so we must NOT dereference once more.
    #
    # mov  [sDeviceInstance], %eax    a1 20 04 00 00   (eax = udev pointer)
    # test %eax, %eax                 85 c0            (NULL check)
    # je   .end                       74 06
    # call usb_reset_device           e8 fc ff ff ff   (regparm3: arg in %eax)
    # .end: ret                       c3
    helper_bytes = bytes([
        0xa1, 0x60, 0x04, 0x00, 0x00,        # a1 + addend for R_386_32 (=0x460)
        0x85, 0xc0,                          # test eax, eax
        0x74, 0x05,                          # je +5 (skip the 5-byte call → land on ret)
        0xe8, 0xfc, 0xff, 0xff, 0xff,        # e8 + addend for R_386_PC32 (=-4)
        0xc3,                                 # ret
    ])
    # Tail-pad remaining slack with int3 so any stray jump traps.
    helper_bytes += b'\xcc' * (16 - len(helper_bytes))
    assert len(helper_bytes) == 16

    buf[HELPER_FOFF:HELPER_FOFF + 16] = helper_bytes
    print(f'patched .text at file 0x{HELPER_FOFF:x} '
          f'(ghidra 0x{HELPER_GADDR:x})')

    # ---- 2. Build the new .strtab tail with the new symbol name ----------
    new_string = b'usb_reset_device\x00'
    new_strtab_size = STRTAB_SIZE + len(new_string)
    new_symbol_name_offset = STRTAB_SIZE   # at end of original .strtab
    # Append to file end (already at strtab end).
    assert len(buf) == STRTAB_OFF + STRTAB_SIZE, \
        f'unexpected EOF position {len(buf):x}'
    buf.extend(new_string)
    print(f'appended {len(new_string)} bytes of strtab string '
          f'(sym name offset {new_symbol_name_offset:#x})')

    # ---- 3. Build the new .symtab (copy original + 1 new entry) ----------
    # Elf32_Sym layout: st_name(4) st_value(4) st_size(4) st_info(1) st_other(1) st_shndx(2)
    new_sym = struct.pack('<IIIBBH',
                          new_symbol_name_offset,
                          0,             # st_value (UNDEF)
                          0,             # st_size
                          (STB_GLOBAL << 4) | STT_NOTYPE,  # st_info
                          0,             # st_other
                          SHN_UNDEF)
    assert len(new_sym) == 16

    new_symtab_offset = len(buf)
    buf.extend(orig[SYMTAB_OFF:SYMTAB_OFF + SYMTAB_SIZE])
    buf.extend(new_sym)
    new_symtab_size = SYMTAB_SIZE + 16
    new_symbol_index = SYMTAB_SIZE // 16   # = 552
    print(f'wrote new .symtab at file 0x{new_symtab_offset:x} '
          f'size {new_symtab_size:#x}; new sym index = {new_symbol_index}')

    # ---- 4. Build the new .rel.text (copy original + 2 new entries) ------
    # Elf32_Rel layout: r_offset(4) r_info(4)
    # Reloc 1: at .text+0x4981 (the operand of the `a1` mov), R_386_32 against .bss
    #          addend stored in slot = SDEVICEINSTANCE_BSS_OFF (0x420)
    reloc1 = struct.pack('<II',
                         0x4981,
                         (BSS_SYM_IDX << 8) | R_386_32)
    # Reloc 2: at .text+0x498a (the operand of the `e8` call), R_386_PC32 against
    #          our new usb_reset_device symbol; addend in slot = -4 (0xfffffffc)
    reloc2 = struct.pack('<II',
                         0x498a,
                         (new_symbol_index << 8) | R_386_PC32)

    new_reltext_offset = len(buf)
    buf.extend(orig[RELTEXT_OFF:RELTEXT_OFF + RELTEXT_SIZE])
    buf.extend(reloc1)
    buf.extend(reloc2)
    new_reltext_size = RELTEXT_SIZE + 16
    print(f'wrote new .rel.text at file 0x{new_reltext_offset:x} '
          f'size {new_reltext_size:#x}')

    # ---- 5. Update the three section headers in-place --------------------
    write_sh_fields(buf, strtab_sh_base,
                    sh_size=new_strtab_size)
    write_sh_fields(buf, symtab_sh_base,
                    sh_offset=new_symtab_offset,
                    sh_size=new_symtab_size)
    write_sh_fields(buf, reltext_sh_base,
                    sh_offset=new_reltext_offset,
                    sh_size=new_reltext_size)

    # ---- 6. Note: .strtab section header sh_offset stays at STRTAB_OFF.
    # We appended to file end which equals STRTAB_OFF + STRTAB_SIZE, so the
    # appended bytes are contiguous with .strtab. The original layout had
    # strtab as the last in-file section; we've extended it cleanly.
    # But .symtab is no longer at its original sh_offset — that's fine
    # because we updated the section header.

    # ---- write output ----
    print(f'final file size: {len(buf):#x}')
    with open(DST, 'wb') as f:
        f.write(bytes(buf))
    print(f'wrote {DST}')
    print(f'orig md5: {hashlib.md5(orig).hexdigest()}')
    print(f'new  md5: {hashlib.md5(bytes(buf)).hexdigest()}')


if __name__ == '__main__':
    main()
