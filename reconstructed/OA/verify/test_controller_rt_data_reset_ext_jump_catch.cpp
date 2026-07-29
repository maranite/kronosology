// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_reset_ext_jump_catch.cpp  -  host-side
 * known-answer test for CSTGControllerRTData::OnEndDownload()/
 * ResetExtKnobJumpCatch(unsigned int)/ResetExtSliderJumpCatch(unsigned int)
 * (round 65, solo). See src/engine/controller_rt_data_reset_ext_jump_catch.cpp.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

CSTGGlobal *CSTGGlobal::sInstance;
unsigned char *STGAPIFrontPanelStatus::sInstance;

int main()
{
	unsigned char buf[0x200];
	CSTGControllerRTData *rt = (CSTGControllerRTData *)buf;

	printf("[1] OnEndDownload()\n");
	{
		memset(buf, 0xAB, sizeof(buf));
		rt->OnEndDownload();
		check("zeroes +0x30..+0x3c", *(unsigned int *)(buf + 0x30) == 0 &&
		      *(unsigned int *)(buf + 0x34) == 0 && *(unsigned int *)(buf + 0x38) == 0 &&
		      *(unsigned int *)(buf + 0x3c) == 0);
		check("clears low nibble of +0x2f, preserves high nibble",
		      buf[0x2f] == 0xA0);
	}

	/* [2] ResetExtKnobJumpCatch/ResetExtSliderJumpCatch */
	unsigned char *fp = mmap32(0x1000);
	STGAPIFrontPanelStatus::sInstance = fp;
	unsigned char *g = mmap32(0x2a00000); /* covers +0x29c9fc0 */
	CSTGGlobal::sInstance = (CSTGGlobal *)g;

	printf("[2] ResetExtKnobJumpCatch: front-panel byte copy-through\n");
	{
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 2] = 0x55; /* valid 7-bit value */
		g[0x29c9fc0] = 0;     /* gate byte clear -> just marks needs-catch */
		rt->ResetExtKnobJumpCatch(2);
		check("copies front-panel byte into +0x56+idx*3", buf[0x56 + 2 * 3] == 0x55);
		check("gate==0: marks +0x54+idx*3 = 1", buf[0x54 + 2 * 3] == 1);
	}
	{
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 1] = 0xFF; /* >= 0x80, must NOT be copied */
		rt->ResetExtKnobJumpCatch(1);
		check("out-of-range front-panel byte (>=0x80) not copied", buf[0x56 + 1 * 3] == 0);
	}
	{
		/* gate != 0 path, cur == -1 */
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 0] = 0x10;
		g[0x29c9fc0] = 1;
		buf[0x55] = (unsigned char)-1; /* cur */
		rt->ResetExtKnobJumpCatch(0);
		check("gate!=0, cur==-1: +0x54 = 0xff", buf[0x54] == 0xff);
	}
	{
		/* gate != 0, cur == target after copy-through (target IS +0x56, just written) */
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 0] = 0x20;
		g[0x29c9fc0] = 1;
		buf[0x55] = 0x20; /* cur == the value that will be copied to target */
		rt->ResetExtKnobJumpCatch(0);
		check("gate!=0, cur==target: +0x54 = 1", buf[0x54] == 1);
	}
	{
		/* gate != 0, cur != target, target <= cur -> 2 */
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 0] = 0x05; /* target after copy */
		g[0x29c9fc0] = 1;
		buf[0x55] = 0x10; /* cur, target(5) <= cur(0x10) */
		rt->ResetExtKnobJumpCatch(0);
		check("gate!=0, target<=cur: +0x54 = 2", buf[0x54] == 2);
	}
	{
		/* gate != 0, cur != target, target > cur -> 0 */
		memset(buf, 0, sizeof(buf));
		fp[0x90b + 0] = 0x30; /* target after copy */
		g[0x29c9fc0] = 1;
		buf[0x55] = 0x10; /* cur, target(0x30) > cur(0x10) */
		rt->ResetExtKnobJumpCatch(0);
		check("gate!=0, target>cur: +0x54 = 0", buf[0x54] == 0);
	}

	printf("[3] ResetExtSliderJumpCatch: same shape, different base offsets\n");
	{
		memset(buf, 0, sizeof(buf));
		fp[0x923 + 3] = 0x66;
		g[0x29c9fc0] = 0;
		rt->ResetExtSliderJumpCatch(3);
		check("copies front-panel byte into +0x6e+idx*3", buf[0x6e + 3 * 3] == 0x66);
		check("gate==0: marks +0x6c+idx*3 = 1", buf[0x6c + 3 * 3] == 1);
	}
	{
		memset(buf, 0, sizeof(buf));
		fp[0x923 + 0] = 0x05;
		g[0x29c9fc0] = 1;
		buf[0x6d] = 0x10; /* cur, target(5) <= cur(0x10) -> 2 */
		rt->ResetExtSliderJumpCatch(0);
		check("gate!=0, target<=cur: +0x6c = 2", buf[0x6c] == 2);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
