#!/usr/bin/env python3
"""
patch_eva_autoauth.py — Patch Eva to auto-generate EX auth strings on Authorise button press.

What this patch does:
  When the user presses "Authorise" on an installed EX library, instead of
  opening the CAuthKeyboard dialog for manual code entry, Eva automatically:
    1. Looks up the option filename (e.g. "S023") via GetProductOptionFileName
    2. Writes "GEN:S023" to /proc/.oaauth (provided by oa_authgen.ko)
    3. Reads back the 24-char auth string
    4. Submits it via SendCommandAuthorizeOption (writes "AU:<code>" to /proc/.oacmd)

Prerequisites:
  - oa_authgen.ko must be loaded (installed by the auto-auth USB package)
  - patched OA.ko must be active (verifies and accepts auth strings)

Usage:
    python3 patch_eva_autoauth.py <input_Eva> <output_Eva>

Patch locations in Eva v3.2.1:
    Cave:        VMA 0x08eb3577, file offset 0x008B3577  (206 zero bytes available)
    CALL patch:  VMA 0x086a492c, file offset 0x0066092C  (replace CALL CAuthKeyboard::ctor)
    NOP sled:    VMA 0x086a4931, file offset 0x00660931  (39 bytes NOP — skip dialog open)
"""

import struct
import sys

# ---------------------------------------------------------------------------
# Eva v3.2.1 addresses (VMA)
# ---------------------------------------------------------------------------
EVA_IMAGE_BASE   = 0x08048000

CAVE_VMA         = 0x08eb3577     # Start of 206-byte zero region in .rodata
CALL_PATCH_VMA   = 0x086a492c     # CALL CAuthKeyboard::CAuthKeyboard — replace with our cave
NOP_START_VMA    = 0x086a4931     # Start of dialog-open block to NOP out
NOP_END_VMA      = 0x086a4958     # First byte after NOP region (XOR EAX,EAX)

# Functions we call from cave
FN_GET_OPTION_FILENAME  = 0x08e1d890   # USTGAPIKLM::GetProductOptionFileName(uint, char*)
FN_SEND_AUTH            = 0x08e48bc0   # _Z26SendCommandAuthorizeOptionPKc(char*)

# PLT stubs
PLT_OPEN   = 0x0804bdbc
PLT_WRITE  = 0x0804c8cc
PLT_READ   = 0x0804c5cc
PLT_CLOSE  = 0x0804bcec

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def vma_to_file(vma):
    return vma - EVA_IMAGE_BASE

def rel32(from_addr, target):
    """Return 4-byte little-endian relative offset for a CALL/JMP instruction.
    from_addr is the address of the first byte of the 5-byte CALL/JMP instruction."""
    return struct.pack('<i', target - (from_addr + 5))

def jmp_short(from_addr, target):
    """2-byte short JMP/Jcc — returns the signed byte displacement."""
    disp = target - (from_addr + 2)
    if not (-128 <= disp <= 127):
        raise ValueError(f"Short jump out of range: from={from_addr:#x} target={target:#x} disp={disp}")
    return struct.pack('b', disp)[0]

# ---------------------------------------------------------------------------
# Assemble the cave
# ---------------------------------------------------------------------------
#
# Layout (starting at CAVE_VMA):
#   +0x00  "/proc/.oaauth\0"           — 14 bytes  (string for open())
#   +0x0e  cave_code_start             — code begins here
#
# On entry to cave code:
#   EDI = product_index (from PlugInDisplayData[display_idx]+4)
#   EBX, ESI, EDI are clobbered — caller (OnAuthorizePlugIn) restores
#   them from its own saved copies at [ESP+0x10/0x14/0x18]
#   ESP frame is still from OnAuthorizePlugIn's prologue.
#
# Stack frame inside cave (after SUB ESP, 0x50):
#   [ESP+0x00..0x0b]  arg scratch (3 cdecl args)
#   [ESP+0x10..0x17]  gen_cmd[8]  "GEN:S023" (no null needed for write)
#   [ESP+0x20..0x38]  auth_buf[25]
#   [ESP+0x40..0x47]  slot_buf[8]
#   [ESP+0x50]        (top of frame — original ESP before SUB)

def assemble_cave(cave_base):
    """Return bytes for the full cave (string + code)."""

    code_start = cave_base + 0x0e   # After 14-byte string

    # All label addresses (absolute VMAs).  We fill in forward references last.
    # First pass: compute approximate offsets; second pass: exact with branch bytes.

    def build(code_start):
        b = bytearray()

        def at(): return code_start + len(b)   # current VMA

        def call(target):
            addr = at()
            b.extend(b'\xe8' + rel32(addr, target))

        def mov_esp_imm32(offset, val):
            """MOV dword [ESP+offset], imm32"""
            if offset == 0:
                b.extend(bytes([0xC7, 0x04, 0x24]) + struct.pack('<I', val))
            elif offset < 128:
                b.extend(bytes([0xC7, 0x44, 0x24, offset]) + struct.pack('<I', val))
            else:
                b.extend(bytes([0xC7, 0x84, 0x24]) + struct.pack('<I', offset) + struct.pack('<I', val))

        def mov_esp_reg(offset, reg):
            """MOV [ESP+offset], reg32.  reg: 0=EAX,1=ECX,2=EDX,3=EBX,4=ESP,5=EBP,6=ESI,7=EDI"""
            modrm_reg = reg << 3
            if offset == 0:
                b.extend(bytes([0x89, modrm_reg | 0x04, 0x24]))
            elif offset < 128:
                b.extend(bytes([0x89, 0x40 | modrm_reg | 0x04, 0x24, offset]))
            else:
                raise NotImplementedError

        def lea_eax_esp(offset):
            """LEA EAX, [ESP+offset]"""
            if offset < 128:
                b.extend(bytes([0x8D, 0x44, 0x24, offset]))
            else:
                b.extend(bytes([0x8D, 0x84, 0x24]) + struct.pack('<I', offset))

        # -------------------------------------------------
        # Prologue: allocate frame
        # -------------------------------------------------
        b.extend([0x83, 0xEC, 0x50])   # SUB ESP, 0x50

        # -------------------------------------------------
        # 1. GetProductOptionFileName(EDI, &slot_buf[ESP+0x40])
        # -------------------------------------------------
        lea_eax_esp(0x40)              # LEA EAX, [ESP+0x40]
        mov_esp_reg(0x04, 0)           # MOV [ESP+0x4], EAX  (arg1 = slot_buf)
        b.extend([0x89, 0x3C, 0x24])   # MOV [ESP], EDI      (arg0 = product_index)
        call(FN_GET_OPTION_FILENAME)

        # -------------------------------------------------
        # 2. Build gen_cmd at [ESP+0x10] = "GEN:" + slot[0..3]
        #    slot_buf is e.g. "S023\0" at [ESP+0x40]
        # -------------------------------------------------
        mov_esp_imm32(0x10, 0x3A4E4547)  # MOV [ESP+0x10], "GEN:" (LE)
        b.extend([0x8B, 0x44, 0x24, 0x40])  # MOV EAX, [ESP+0x40]  (slot first 4 bytes)
        mov_esp_reg(0x14, 0)             # MOV [ESP+0x14], EAX
        b.extend([0x8A, 0x44, 0x24, 0x44])  # MOV AL, [ESP+0x44]   (slot[4], usually null)
        b.extend([0x88, 0x44, 0x24, 0x18])  # MOV [ESP+0x18], AL   (gen_cmd null term)

        # -------------------------------------------------
        # 3. open("/proc/.oaauth", 2)
        # -------------------------------------------------
        mov_esp_imm32(0x04, 2)           # arg1 = O_RDWR
        mov_esp_imm32(0x00, cave_base)   # arg0 = "/proc/.oaauth"
        call(PLT_OPEN)
        b.extend([0x89, 0xC3])           # MOV EBX, EAX  (fd)
        b.extend([0x85, 0xDB])           # TEST EBX, EBX
        js_pos = len(b)
        b.extend([0x78, 0x00])           # JS .fail  (placeholder)

        # -------------------------------------------------
        # 4. write(fd, gen_cmd, 8)
        # -------------------------------------------------
        mov_esp_imm32(0x08, 8)           # arg2 = 8
        lea_eax_esp(0x10)                # LEA EAX, [ESP+0x10]
        mov_esp_reg(0x04, 0)             # MOV [ESP+0x4], EAX  (arg1 = gen_cmd)
        b.extend([0x89, 0x1C, 0x24])     # MOV [ESP], EBX  (arg0 = fd)
        call(PLT_WRITE)
        b.extend([0x83, 0xF8, 0x08])     # CMP EAX, 8
        jnz_pos = len(b)
        b.extend([0x75, 0x00])           # JNZ .closefail  (placeholder)

        # -------------------------------------------------
        # 5. read(fd, auth_buf, 25)
        # -------------------------------------------------
        mov_esp_imm32(0x08, 25)          # arg2 = 25
        lea_eax_esp(0x20)                # LEA EAX, [ESP+0x20]
        mov_esp_reg(0x04, 0)             # MOV [ESP+0x4], EAX  (arg1 = auth_buf)
        b.extend([0x89, 0x1C, 0x24])     # MOV [ESP], EBX  (arg0 = fd)
        call(PLT_READ)
        b.extend([0x85, 0xC0])           # TEST EAX, EAX
        jle_pos = len(b)
        b.extend([0x7E, 0x00])           # JLE .closefail  (placeholder)

        # Null-terminate auth_buf: byte [ECX+EAX] = 0 where ECX = &auth_buf
        b.extend([0x8D, 0x4C, 0x24, 0x20])  # LEA ECX, [ESP+0x20]
        b.extend([0xC6, 0x04, 0x01, 0x00])  # MOV byte [ECX+EAX], 0

        # close(fd) on success path
        b.extend([0x89, 0x1C, 0x24])     # MOV [ESP], EBX
        call(PLT_CLOSE)

        # -------------------------------------------------
        # 6. SendCommandAuthorizeOption(auth_buf)
        # -------------------------------------------------
        lea_eax_esp(0x20)                # LEA EAX, [ESP+0x20]
        b.extend([0x89, 0x04, 0x24])     # MOV [ESP], EAX
        call(FN_SEND_AUTH)
        jmp_pos = len(b)
        b.extend([0xEB, 0x00])           # JMP .done  (placeholder)

        # -------------------------------------------------
        # .closefail: close(fd) then fall to .done
        # -------------------------------------------------
        closefail_off = len(b)
        b.extend([0x89, 0x1C, 0x24])     # MOV [ESP], EBX
        call(PLT_CLOSE)

        # -------------------------------------------------
        # .fail / .done: epilogue
        # -------------------------------------------------
        fail_off = len(b)
        done_off  = len(b)
        b.extend([0x83, 0xC4, 0x50])     # ADD ESP, 0x50
        b.extend([0xC3])                 # RET

        # Patch forward-reference branches
        b[js_pos  + 1] = jmp_short(code_start + js_pos,   code_start + fail_off)
        b[jnz_pos + 1] = jmp_short(code_start + jnz_pos,  code_start + closefail_off)
        b[jle_pos + 1] = jmp_short(code_start + jle_pos,  code_start + closefail_off)
        b[jmp_pos + 1] = jmp_short(code_start + jmp_pos,  code_start + done_off)

        return bytes(b)

    code_bytes = build(code_start)
    print(f"  cave code size: {len(code_bytes)} bytes  (limit: {206 - 14} = 192 bytes)")
    if len(code_bytes) > 192:
        raise RuntimeError("Cave code overflows!")
    return code_bytes

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def patch_eva(input_path, output_path):
    with open(input_path, 'rb') as f:
        data = bytearray(f.read())

    # --- Sanity check original bytes at CALL patch site ---
    call_off = vma_to_file(CALL_PATCH_VMA)
    orig_call = data[call_off : call_off + 5]
    EXPECTED_CALL = bytes([0xE8, 0xEF, 0x3C, 0x00, 0x00])
    if orig_call != EXPECTED_CALL:
        print(f"WARNING: CALL at {CALL_PATCH_VMA:#x} = {orig_call.hex()!r}, expected {EXPECTED_CALL.hex()!r}")
        print("  This binary may not be v3.2.1 or may already be patched.")

    # --- Sanity check cave region is all zeros ---
    cave_off = vma_to_file(CAVE_VMA)
    cave_region = data[cave_off : cave_off + 206]
    nz = sum(1 for b in cave_region if b != 0)
    if nz > 0:
        print(f"WARNING: Cave region at {CAVE_VMA:#x} is not all zeros ({nz} non-zero bytes). May already be patched or wrong version.")

    # --- Assemble cave ---
    print(f"[eva-patch] Cave at VMA={CAVE_VMA:#010x}  file_offset={cave_off:#010x}")
    print(f"[eva-patch] Cave code starts at VMA={CAVE_VMA+0x0e:#010x}")
    cave_string = b'/proc/.oaauth\x00'   # 14 bytes
    cave_code   = assemble_cave(CAVE_VMA)
    cave_bytes  = cave_string + cave_code
    print(f"[eva-patch] Total cave content: {len(cave_bytes)} bytes")

    # --- Apply cave ---
    data[cave_off : cave_off + len(cave_bytes)] = cave_bytes

    # --- Patch CALL at 086a492c → call cave code ---
    cave_code_vma = CAVE_VMA + 0x0e
    new_call = b'\xe8' + rel32(CALL_PATCH_VMA, cave_code_vma)
    print(f"[eva-patch] CALL patch at VMA={CALL_PATCH_VMA:#010x}  file_offset={call_off:#010x}")
    print(f"  Old: {orig_call.hex()}  →  New: {new_call.hex()}")
    data[call_off : call_off + 5] = new_call

    # --- NOP out the dialog-open block 086a4931..086a4957 ---
    nop_off = vma_to_file(NOP_START_VMA)
    nop_len  = NOP_END_VMA - NOP_START_VMA      # 39 bytes
    print(f"[eva-patch] NOP sled at VMA={NOP_START_VMA:#010x}  file_offset={nop_off:#010x}  len={nop_len}")
    data[nop_off : nop_off + nop_len] = bytes([0x90] * nop_len)

    # --- Write output ---
    with open(output_path, 'wb') as f:
        f.write(data)
    print(f"[eva-patch] Written: {output_path}")
    print()
    print("Summary of patches:")
    print(f"  1. Cave:       VMA {CAVE_VMA:#010x}  ({len(cave_bytes)} bytes)")
    print(f"  2. CALL patch: VMA {CALL_PATCH_VMA:#010x}  (5 bytes)")
    print(f"  3. NOP sled:   VMA {NOP_START_VMA:#010x}  ({nop_len} bytes)")


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_Eva> <output_Eva>")
        sys.exit(1)
    patch_eva(sys.argv[1], sys.argv[2])
