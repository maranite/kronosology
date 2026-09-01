# Hardware Evidence — re-save + SysEx dump (2026-08, Phase B)

Artifacts fetched from the Kronos (192.168.100.15) confirming two open questions.

## T8_RESAVE.PCG — the Kronos's own re-save of T8

- 48,037,230 bytes vs T8's 47,866,616 → **+170,614 bytes**
- Every object chunk (SLS1/PRG1/CMB1/DKT1/WSQ1/GLB1) is byte-identical to T8;
  the GLB1 category edit survived with a correctly recomputed checksum.
- **DPI1 grew** (330,324 → 500,938): DPN1 476→834, DPD1 7436→13164 (drum-pattern
  count 232 → 411, 32-byte patterns), DPS1 322,376→486,904.
- DIV1 byte 2 changed 0x00 → 0xB6; PCG1 size updated.

Conclusion: **the Kronos re-save is not byte-identical** — it expands DPI1 and
rewrites DIV1 DPI state, but preserves all object data exactly.

## HD-1_SysexDumpU-FF.txt — 128-program Object-Dump capture of bank U-FF

- Each message: `F0 42 30 68 73 [obj] [bank] [index] [ver=5] [8-to-7 body] F7`,
  7-bit payload from byte 10.
- Decodes (KSR `Decode8to7`: every 8 SysEx bytes → 7 binary, first byte = MSBs)
  to **exactly 3706 bytes** per HD-1 program.
- **127/128 match the first 3706 bytes of their PCG U-FF record byte-for-byte.**

Conclusion: **HD-1 wire body = the first 3706 bytes of the 4960-byte PCG record** —
KSR's `ProgramFormatConverter` (wire = PCG prefix) is hardware-confirmed.

## Reconstructing

```python
# decode a captured 0x73 object dump
def decode8to7(src, offset, sysExLen):
    binaryLen = (sysExLen // 8) * 7 + (sysExLen % 8) - (1 if sysExLen % 8 else 0)
    dst = bytearray(binaryLen); si = offset; di = 0
    while si < offset + sysExLen and di < binaryLen:
        msbs = src[si]; si += 1
        for bit in range(7):
            if si >= offset + sysExLen or di >= binaryLen: break
            dst[di] = src[si] | (((msbs >> bit) & 1) << 7); di += 1; si += 1
    return bytes(dst)
```
