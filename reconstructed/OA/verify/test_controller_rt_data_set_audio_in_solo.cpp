// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_set_audio_in_solo.cpp  -  host-side known-answer
 * test for CSTGControllerRTData::SetAudioInSolo(unsigned int, bool)
 * (batch 57), using the real mode-1 branch of ResolveCurrentPerformance()
 * (idx=0/bank=0 -> offset 0x1c77f10+6) since it's the SMALLEST of the
 * three confirmed real offset bases, keeping the required mmap32'd
 * "CSTGGlobal" buffer as small as this formula allows (~29.9MB -- still
 * large, but matches this project's own established precedent of a
 * multi-MB mmap32'd CSTGGlobal stand-in, test_global.cpp's own
 * `globalSize = 0x29cc920`).
 *
 * Standalone TU: only links controller_rt_data_set_audio_in_solo.cpp
 * itself -- NOT global.cpp (which would drag in that file's own large
 * transitive dependency/mock graph for a single, small, side-effect-free
 * helper). Instead supplies its OWN local copy of
 * `ResolveCurrentPerformance()`, verified byte-for-byte identical to
 * global.cpp's real (now externally-linked) definition -- matching this
 * project's established "local minimal stand-in, same real formula,
 * defined once for real elsewhere" convention (e.g. midi_port_manager.cpp's
 * local `CSTGHeapManager`/`CSTGCPUInfo` stand-ins).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

/* Real global.cpp needs these; provide standalone-TU-local storage,
 * matching this project's established convention. */
CSTGGlobal *CSTGGlobal::sInstance;

/* Local verified-identical copy of global.cpp's own (now externally
 * linked) ResolveCurrentPerformance() -- see this file's header comment
 * for why this is duplicated rather than linked. */
CSTGPerformance *ResolveCurrentPerformance(unsigned char *base)
{
	int mode = *(int *)(base + 0x684);
	if (mode == 1) {
		int idx = (*(int *)(base + 0x69c)) & 0x7f;
		int bank = *(int *)(base + 0x690);
		int offset = idx * 0x19e7 + bank * 0xcf381 + 0x1c77f10 + 6;
		return (CSTGPerformance *)(base + offset);
	}
	if (mode == 2) {
		int seqIdx = *(int *)(base + 0x6a0);
		int offset = seqIdx * 0x1cad + 0x27cd024;
		return (CSTGPerformance *)(base + offset);
	}
	int progIdx = *(int *)(base + 0x698);
	if (progIdx == 0xfffe)
		return (CSTGPerformance *)(base + 0x2976e33);
	int idx = progIdx & 0x7f;
	int bank = *(int *)(base + 0x68c);
	int offset = idx * 0xcec + bank * 0x67603 + 0x132e4d0 + 3;
	return (CSTGPerformance *)(base + offset);
}

static int g_dtorCalls;
static void *g_lastPerf;
static void MockPerformanceSlot27(void *perf)
{
	g_dtorCalls++;
	g_lastPerf = perf;
}

int main(void)
{
	printf("CSTGControllerRTData::SetAudioInSolo known-answer test\n");
	printf("========================================================\n");

	unsigned char *rtBuf = mmap32(0x40);
	memset(rtBuf, 0, 0x40);
	CSTGControllerRTData *crt = (CSTGControllerRTData *)rtBuf;

	/* Mode 1 (idx=0, bank=0): ResolveCurrentPerformance() lands on
	 * base + 0x1c77f10 + 6 -- the smallest of the three confirmed real
	 * bases, see this file's own header comment. */
	const unsigned int kMode1Offset = 0x1c77f10u + 6u;
	unsigned long globalSize = kMode1Offset + 0x10;
	unsigned char *g = mmap32(globalSize);
	memset(g, 0, globalSize);
	CSTGGlobal::sInstance = (CSTGGlobal *)g;
	*(unsigned int *)(g + 0x684) = 1;	/* mode = 1 */
	*(unsigned int *)(g + 0x69c) = 0;	/* idx = 0 */
	*(unsigned int *)(g + 0x690) = 0;	/* bank = 0 */

	/* Small separate vtable buffer -- the perf object's own vtable
	 * pointer (stored INSIDE the giant buffer at the resolved offset)
	 * points here, matching the real "vtable ptr is a member, the
	 * vtable array itself lives elsewhere" shape. */
	void **vtable = (void **)mmap32(64 * sizeof(void *));
	memset(vtable, 0, 64 * sizeof(void *));
	vtable[27] = (void *)&MockPerformanceSlot27;
	*(void **)(g + kMode1Offset) = vtable;	/* perf->vtable = vtable */

	printf("[1] solo=true, slot=2, single-solo mode clear -> OR into soloBits\n");
	{
		rtBuf[0x21] = 0;	/* single-solo mode bit clear */
		rtBuf[0x26] = 0x1;	/* slot 0 already soloed */
		g_dtorCalls = 0;
		g_lastPerf = 0;

		crt->SetAudioInSolo(2, true);

		check_eq("soloBits == 0x5 (bit0 | bit2)", rtBuf[0x26], 0x5);
		check_eq("perf slot-27 dispatched once", g_dtorCalls, 1);
		check_eq("perf pointer == resolved offset", (unsigned int)(unsigned long)g_lastPerf,
			 (unsigned int)(unsigned long)(g + kMode1Offset));
	}

	printf("[2] solo=false, slot=0 -> AND-clear only that bit\n");
	{
		rtBuf[0x26] = 0x5;	/* bits 0 and 2 set */
		g_dtorCalls = 0;

		crt->SetAudioInSolo(0, false);

		check_eq("soloBits == 0x4 (bit2 only)", rtBuf[0x26], 0x4);
		check_eq("perf slot-27 still dispatched (unconditional tail)", g_dtorCalls, 1);
	}

	printf("[3] solo=true, slot=1, single-solo mode SET -> overwrite + clear +0x22/+0x24\n");
	{
		rtBuf[0x21] = 0x2;	/* single-solo mode bit set */
		rtBuf[0x26] = 0x5;	/* bits 0 and 2 set beforehand */
		*(unsigned short *)(rtBuf + 0x22) = 0xbeef;
		*(unsigned short *)(rtBuf + 0x24) = 0xdead;

		crt->SetAudioInSolo(1, true);

		check_eq("soloBits == 0x2 (bit1 ONLY, overwritten not OR'd)", rtBuf[0x26], 0x2);
		check_eq("+0x22 cleared", *(unsigned short *)(rtBuf + 0x22), 0);
		check_eq("+0x24 cleared", *(unsigned short *)(rtBuf + 0x24), 0);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "All tests passed");
	return g_fail ? 1 : 0;
}
