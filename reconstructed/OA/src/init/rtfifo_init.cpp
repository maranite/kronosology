// SPDX-License-Identifier: GPL-2.0
/*
 * rtfifo_init.cpp  -  stg_rtfifo_init(): init_module step 16 (hard-fail).
 * See oa_rtfifo_init.h for the ground-truthing details.
 *
 * Faithful reconstruction from a full objdump disassembly of the real
 * `our_fifo_setup` (`.init.text+0x34f`, 72 bytes) and `stg_rtfifo_init`
 * (`.init.text+0x397`, 163 bytes) in OA_real.ko.
 */

#include "oa_rtfifo_init.h"

/* Confirmed real .bss bitmask tracking which FIFO minors were
 * successfully created (bit N set = minor N's FIFO is up). Not
 * independently named beyond this project's own confirmed relocation
 * -- kept as a plain static here, matching this project's convention
 * for a confirmed-but-unnamed global. */
static unsigned int s_fifoCreatedMask;

/* Real .data-embedded file_operations blob for the "stg_direct" char
 * device (.data+0x3200, confirmed via relocation) -- its own field
 * contents (open/read/write/etc. handlers) are NOT independently
 * reconstructed in this pass; a minimal zeroed placeholder is used,
 * matching this project's established treatment for confirmed-real-
 * but-not-yet-reconstructed file_operations objects (e.g.
 * InitSharedMemProcInterface's own fops placeholder). */
static const void *s_stgDirectFops[16];

static int our_fifo_setup(unsigned int minor, int size)
{
	rtf_destroy(minor);
	if (rtf_create(minor, size) != 0)
		return -1;
	s_fifoCreatedMask |= (1u << minor);
	return 0;
}

int stg_rtfifo_init(void)
{
	static const struct { unsigned int minor; int size; } fifos[] = {
		{ 0, 0x400 },
		{ 1, 0x400 },
		{ 3, 0x400 },
		{ 4, 0x8000 },
		{ 5, 0x10000 },
		{ 7, 0x400 },
	};

	for (unsigned int i = 0; i < sizeof(fifos) / sizeof(fifos[0]); i++) {
		if (our_fifo_setup(fifos[i].minor, fifos[i].size) != 0) {
			stg_rtfifo_cleanup();
			return -1;
		}
	}

	/* Real confirmed literal name/args: major=0x98, baseminor=0,
	 * count=0x100, name="stg_direct" (extracted from .rodata, not
	 * guessed). */
	if (__register_chrdev(0x98, 0, 0x100, "stg_direct", s_stgDirectFops) != 0) {
		stg_rtfifo_cleanup();
		return -1;
	}

	return 0;
}

/* Diagnostic-only .bss counters read (never written) by
 * stg_rtfifo_cleanup()'s own rt_printk dumps -- real confirmed
 * relocations, exact semantics/names not independently decoded in this
 * pass (pure logging, no control-flow effect). Kept as plain statics
 * local to this TU, matching this project's convention for a
 * confirmed-but-unnamed global (same treatment as s_fifoCreatedMask
 * above). */
static unsigned int s_rtfifoStat0, s_rtfifoStat1, s_rtfifoStat2, s_rtfifoStat3;
static unsigned int s_rtfifoStat4, s_rtfifoStat5, s_rtfifoStat6, s_rtfifoStat7;

void stg_rtfifo_cleanup(void)
{
	static const unsigned int kMinors[] = { 0, 1, 3, 4, 5, 7 };

	for (unsigned int i = 0; i < sizeof(kMinors) / sizeof(kMinors[0]); i++) {
		unsigned int minor = kMinors[i];
		if (s_fifoCreatedMask & (1u << minor)) {
			s_fifoCreatedMask &= ~(1u << minor);
			rtf_destroy(minor);
		}
	}

	__unregister_chrdev(0x98, 0, 0x100, "stg_direct");

	/* CORRECTED (2026-07-04): these previously passed the raw .rodata
	 * offset as a literal integer standing in for the format string
	 * pointer -- see init_module.cpp's own printk/rt_printk fix comment
	 * for the full explanation (a genuine wild-pointer bug, not just a
	 * missing-text placeholder). Restored to valid string literals. */
	rt_printk("OA: rtfifo stats %u %u %u\n" /* real text not resolved, offset 0x64c */,
		  s_rtfifoStat0, s_rtfifoStat1, s_rtfifoStat2);
	rt_printk("OA: rtfifo stats %u %u\n" /* real text not resolved, offset 0x6cc */,
		  s_rtfifoStat3, s_rtfifoStat4);
	rt_printk("OA: rtfifo stats %u %u %u %u\n" /* real text not resolved, offset 0x700 */,
		  s_rtfifoStat5, s_rtfifoStat6, s_rtfifoStat7, s_rtfifoStat3);
}

/* Test/inspection accessor -- not present in the real binary, added
 * purely so a host KAT can reset the confirmed-real-but-otherwise-
 * process-lifetime bitmask between independent scenarios (matching
 * this project's established convention, e.g. oa_stgheap_init.h's own
 * stgheap_get_*() accessors). */
void stg_rtfifo_test_reset_mask(void)
{
	s_fifoCreatedMask = 0;
}
