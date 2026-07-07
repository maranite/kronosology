// SPDX-License-Identifier: GPL-2.0
/*
 * tick_count.cpp  -  GetSTGTickCount(), the STG engine's tick-counter
 * accessor (batch 35, sec 10.183).
 *
 * Ground truth (`objdump -dr` of /home/share/Decomp/OA.ko_Decomp/OA.ko,
 * symbol `GetSTGTickCount`, 16 bytes, extern "C"):
 *
 *     mov  CSTGGlobal::sInstance, %eax     ; R_386_32 _ZN10CSTGGlobal9sInstanceE
 *     push %ebp; mov %esp,%ebp
 *     mov  0x29c9fa8(%eax), %eax           ; load the u32 tick field
 *     pop  %ebp; ret
 *
 * A leaf accessor: reads a single 32-bit word at
 * `CSTGGlobal::sInstance + 0x29c9fa8` and returns it. The offset sits one
 * dword above the +0x29c9fa0 field `lfo_stepseq_quad.cpp` already reads,
 * inside CSTGGlobal's confirmed ~43.6MB-in field range -- so it is
 * accessed via this file's established raw `(unsigned char *)sInstance +
 * OFFSET` arithmetic, not a named struct field (CSTGGlobal's full layout
 * is nowhere near recovered).
 *
 * Read directly as a 32-bit value (`*(unsigned int *)`), which is
 * bit-identical on the real -m32 target (native 4-byte field there) and
 * host-safe (reads exactly 4 bytes regardless of host pointer width).
 *
 * Deliberately a standalone TU (NOT part of global.cpp) so the daemon-
 * watchdog host KAT (verify/test_stg_daemons.cpp) can link
 * signal_timed_out_daemons() against its OWN scripted GetSTGTickCount
 * mock, while this real body is exercised separately by
 * verify/test_tick_count.cpp -- the project's established "split a shared
 * dependency across TUs when mock footprints differ" pattern (sec 10.150).
 * CSTGGlobal::sInstance's storage lives in global.cpp (this file only
 * references it).
 */

#include "oa_global.h"

#ifndef STG_TICK_COUNT_OFFSET
#define STG_TICK_COUNT_OFFSET 0x29c9fa8
#endif

extern "C" unsigned int GetSTGTickCount(void)
{
	return *(unsigned int *)((unsigned char *)CSTGGlobal::sInstance
				 + STG_TICK_COUNT_OFFSET);
}
