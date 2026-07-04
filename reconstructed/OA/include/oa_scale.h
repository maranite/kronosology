// SPDX-License-Identifier: GPL-2.0
/*
 * oa_scale.h  -  the "Scale*" family of leaf linear-rescale/lerp helpers.
 * Stage 2 shared utility (PLAN.md's "leaf math/tables").
 *
 * Ground-truthed by fully disassembling ScaleLong (.text+0x584542, 49 bytes),
 * ScaleShort (.text+0x5845d1, 71 bytes), ScaleWord (.text+0x584618, 73
 * bytes), and ScaleByte (.text+0x5846c5, 83 bytes) -- all four share one
 * algorithm, differing only in operand width/signedness (confirmed via the
 * movzx-vs-movsx choice each uses when widening to the 32-bit registers the
 * arithmetic actually happens in). ScaleChar (.text+0x584661, 100 bytes) was
 * confirmed to follow the same normal-path formula; its degenerate-branch
 * bytes look odd (an unsigned `mul` where a signed `imul` might be expected)
 * but produce the identical low-byte result at that width regardless, so it
 * isn't a materially different algorithm.
 *
 * All five map `value` from the input range [inMin, inMax] to the output
 * range [outMin, outMax]:
 *
 *   normal case (inMin != inMax):
 *     result = outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin)
 *     (the multiply and divide both happen in the 32-bit registers the
 *     narrower types get zero/sign-extended into -- i.e. plain C++ integer
 *     promotion of the narrower types reproduces this exactly, no explicit
 *     widening needed)
 *
 *   degenerate case (inMin == inMax, avoids a divide-by-zero):
 *     result = outMin + (outMax - outMin) * (value - inMin)
 *     confirmed: the division is skipped entirely (not treated as /1) --
 *     this is a faithfully-reproduced quirk of the compiled code, not
 *     "fixed" into a more sensible degenerate behavior.
 *
 * ScaleLongDouble (.text+0x584573, 94 bytes) is a distinct sibling for the
 * `long`-width case: it does the identical formula but with the
 * multiply/divide computed in double precision (via the x87 FPU) instead of
 * 32-bit integer arithmetic -- almost certainly to avoid the int32 overflow
 * ScaleLong's `imul`/`idiv` would suffer for wide long ranges. Confirmed via
 * the FPU control word it installs before the final `fistp` (loads 0x0c
 * into the control word's high byte, i.e. RC=11/truncate-toward-zero,
 * PC=00/single) -- truncate-toward-zero is exactly what a plain C++
 * `(long)` cast from `double` does, so no special rounding call is needed.
 *
 * NOT reconstructed here (bigger, more specialized, not "leaf" primitives):
 * ScaleValToIndex, ScaleWhiteBlackCC, ScaleRTParmValue.
 */

#ifndef OA_SCALE_H
#define OA_SCALE_H

long          ScaleLong(long value, long inMin, long inMax, long outMin, long outMax);
long          ScaleLongDouble(long value, long inMin, long inMax, long outMin, long outMax);
short         ScaleShort(short value, short inMin, short inMax, short outMin, short outMax);
unsigned short ScaleWord(unsigned short value, unsigned short inMin, unsigned short inMax,
                          unsigned short outMin, unsigned short outMax);
unsigned char ScaleByte(unsigned char value, unsigned char inMin, unsigned char inMax,
                         unsigned char outMin, unsigned char outMax);
char          ScaleChar(char value, char inMin, char inMax, char outMin, char outMax);

#endif /* OA_SCALE_H */
