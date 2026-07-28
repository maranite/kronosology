---
name: ckg-seq-backup-technique
description: Scripted instruction-pattern decoder for dense "tiny accessor" clusters (Set*/Get* one-liners) in OA.ko, used for CKGSeqBackupCommonParam/ModuleParam
type: user
---

For a dense cluster of many (100-300) tiny near-identical accessor methods
(read one bitfield/byte/word from a struct pointer at a fixed or
index-scaled offset, optionally shift+mask, store into one output field),
do NOT hand-transcribe each one. Instead:

1. `objdump -dr -M intel --start-address=X --stop-address=Y` the real
   `OA_real.ko` (or equivalent ground-truth `.ko`) over the full
   contiguous address range of the cluster in one shot.
2. Write a small Python parser that splits objdump output into per-symbol
   instruction blocks (regex on `ADDR <sym>:` headers and
   `addr:\tbytes\tmnemonic` lines).
3. Write a tiny symbolic evaluator over JUST the instruction vocabulary
   these accessors use (typically: `mov reg,[this+K]` for K in {0,4,8},
   `lea reg,[reg+reg*N]` for index*stride scaling, `add reg,[this+K]` or
   `add reg,reg` to combine index-scaled-offset with a base pointer,
   `movzx`/`movsx reg,BYTE/WORD [mem]`, `sar`/`shr reg,N` or `reg,cl`,
   `and reg,mask`, final `mov [this+K],reg` or `,imm`). Track a tiny
   register-state dict mapping x86 regs to symbolic tuples
   (`('ptr','src')`, `('int','idx')`, `('load', base, index, scale, disp,
   width, signed)`, `('shift', ...)`, `('mask', ...)`) and render each to
   a C expression at the end. This handled 195/197 real methods for the
   CKGSeqBackup{Common,Module}Param cluster (2 outliers -- a nibble-pack
   parity branch, a `idx+4` dynamic shift amount -- fell outside the
   vocabulary and were written by hand, cheaply, since the decoder's
   `raise Exception` on an unhandled instruction told me exactly which 2
   needed hand treatment and why).
4. Generate the KAT test the SAME way, but with an INDEPENDENT second
   evaluator (plain Python, not reusing the C-string renderer) computing
   expected values from the same parsed (base, index*stride, disp, width,
   signed, shift, mask) facts against a deterministic non-trivial byte
   pattern (e.g. `buf[i] = (i*0x9f+0x37)&0xff` -- NOT all-same-byte, so
   every bit position is independently distinguishable). Two independent
   renderers (C codegen vs. Python KAT oracle) sharing only the parsed
   metadata, not each other's output, is a real regression check, not
   just self-consistency theater.

Gotcha hit twice already in this project: a literal `*/` inside a plain-
English derivation comment (e.g. "SetChordMemNote*/SetChordMemNote*Vel")
silently terminates the C block comment early and cascades into wildly
unrelated parse errors dozens of lines later (looked like an ODR/missing-
type conflict in an unrelated header at first). Grep any generated
header-comment prose for a literal `*/` substring before compiling.

See [[ckg_bankmanager_class_facts]] for the specific classes this
produced (CKGSeqBackupCommonParam/ModuleParam, CKGBankManager, CSPREngine).
