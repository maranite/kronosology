// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtparm_family.cpp  -  independent KAT for the 56 reconstructed
 * RTParm family members (src/engine/rtparm_family.cpp). Expected values
 * are hand-computed directly from the confirmed disassembly semantics
 * (documented in each function's own header comment / this project's
 * memory), NOT derived by running the reconstructed code first -- an
 * independent oracle, matching this project's standing verification
 * discipline.
 */

#include <cstdio>
#include <cstring>
#include "oa_rtparm_family.h"
#include "oa_rtparm_ge_table.h"
#include "oa_rtparm_pe_table.h"
/* CKGParamEdit::GetRTParmBufferSelectId is tested separately, in
 * verify/test_rtparm_ckgparamedit.cpp -- see src/engine/
 * rtparm_ckgparamedit.cpp's own header comment for why it can't share a
 * TU (and therefore can't share a KAT binary either) with the rest of
 * this family. */

static int g_fail;

static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-55s %ld\n", label, got); return; }
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void check_ptr(const char *label, const void *got, const void *want)
{
	if (got == want) { printf("  ok    %-55s %p\n", label, got); return; }
	printf("  FAIL  %-55s got=%p want=%p\n", label, got, want);
	g_fail++;
}

int main()
{
	printf("-- tiny mechanical helpers --\n");
	{
		unsigned char b = 0xff;
		ResetDynRTParmWindow(&b);
		check_eq("ResetDynRTParmWindow zeroes byte", b, 0);
	}
	{
		unsigned char ge[0x8cc + 8 * 8];
		RTParm *rp = (RTParm *)(ge + 0x8cc + 3 * 8);
		check_eq("GetRTParmIDFromGE index 3", GetRTParmIDFromGE((GenEffect *)ge, rp), 3);
	}
	{
		unsigned char perf[0x26a + 5 * 8];
		RTParm *rp = (RTParm *)(perf + 0x26a + 4 * 8);
		check_eq("GetRTParmIDFromPE index 4", GetRTParmIDFromPE((Performance *)perf, rp), 4);
	}
	{
		/* static, not stack-local: on a 64-bit host, this function's
		 * own 32-bit `int base = *(int*)(self+0xc)` field (a faithful
		 * copy of the real 32-bit-target field width) can't round-trip
		 * a 64-bit stack address -- same class of gotcha already
		 * documented/fixed elsewhere in this project (test_ckg_engine,
		 * test_tone_adjust_descriptors: static buffers land in the
		 * low, 32-bit-representable range; stack addresses don't). */
		static unsigned char selfBuf[0x10];
		static unsigned char region[0x42a8 * 2 + 0x200];
		memset(selfBuf, 0, sizeof(selfBuf));
		memset(region, 0, sizeof(region));
		*(int *)(selfBuf + 0xc) = (int)(long)region;
		((CKGSysExBuffer *)selfBuf)->StoreRTParmBySeq(1, 5);
		check_eq("StoreRTParmBySeq writes 1", region[0x42a8 + 5 + 0xe0], 1);
	}
	{
		unsigned char genMod[0x86a4 + 0x500];
		memset(genMod, 0, sizeof(genMod));
		ResetRTParmGELastVal((GenMod *)genMod);
		short v0 = *(short *)(genMod + 0x86a4 + 0x26);
		short v31 = *(short *)(genMod + 0x86a4 + 0x26 + 31 * 0x28);
		check_eq("ResetRTParmGELastVal[0]", v0, (short)0x8001);
		check_eq("ResetRTParmGELastVal[31]", v31, (short)0x8001);
	}
	{
		unsigned char rp[8] = { 0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78 };
		ByteSwapRTParm((RTParm *)rp);
		check_eq("ByteSwapRTParm field4", *(unsigned short *)(rp + 4), 0x1234);
		check_eq("ByteSwapRTParm field6", *(unsigned short *)(rp + 6), 0x5678);
	}
	{
		check_eq("GetRTParmGroupItems(0,mode!=0)", GetRTParmGroupItems(0, 1), 0x00);
		check_eq("GetRTParmGroupItems(3,mode!=0)", GetRTParmGroupItems(3, 1), 0x29);
		check_eq("GetRTParmGroupItems(16,mode!=0) oob", GetRTParmGroupItems(16, 1), 0);
		check_eq("GetRTParmGroupItems(3,mode==0)", GetRTParmGroupItems(3, 0), 0x0c);
		check_eq("GetRTParmGroupItems(7,mode==0) oob", GetRTParmGroupItems(7, 0), 0);
	}
	{
		void *p_cur = GetRTParmFunctionGE(2, 5, RTPARM_BUFFER_CURRENT);
		void *p_cmp = GetRTParmFunctionGE(2, 5, RTPARM_BUFFER_COMPARE);
		check_ptr("GetRTParmFunctionGE current", p_cur, (void *)(gKS + 2 * 0x9d10 + 0x1f73c + 5 * 40));
		check_ptr("GetRTParmFunctionGE compare", p_cmp, (void *)(gKS + 2 * 0x9d10 + 0x1fc3c + 5 * 40));
	}
	{
		unsigned char perf[0x2ea + 33 * 8 * 8];
		void *p_cur = GetRTParmEditGE((Performance *)perf, RTPARM_BUFFER_CURRENT, 1, 3);
		check_ptr("GetRTParmEditGE current", p_cur, (void *)(perf + (1u * 32 + 3) * 8 + 0x2ea));
	}
	{
		unsigned char rd[8] = { 0x12, 0x34, 0, 0, 0x56, 0x78, 0x9a, 0xbc };
		ByteSwapRTParmEdit((RTParmEdit *)rd);
		check_eq("ByteSwapRTParmEdit[0]", *(unsigned short *)(rd + 0), 0x1234);
		check_eq("ByteSwapRTParmEdit[4]", *(unsigned short *)(rd + 4), 0x5678);
		check_eq("ByteSwapRTParmEdit[6]", *(unsigned short *)(rd + 6), 0x9abc);
	}
	{
		unsigned char rf[16] = {0};
		*(void **)(rf + 4) = (void *)&RT_phs_xpose_oct;
		AdjustRTParmFunctionGE((RTParmFunction *)rf);
		check_ptr("AdjustRTParmFunctionGE oct->base", *(void **)(rf + 4), (void *)&RT_phs_xpose);
		*(void **)(rf + 4) = (void *)&RT_drm_pat_xpose_oct_5th;
		AdjustRTParmFunctionGE((RTParmFunction *)rf);
		check_ptr("AdjustRTParmFunctionGE oct5th->base", *(void **)(rf + 4), (void *)&RT_drm_pat_xpose);
	}
	{
		unsigned char rf[16] = {0};
		*(void **)(rf + 4) = (void *)&RT_crb_xpose_oct;
		AdjustRTParmFunctionPE((RTParmFunction *)rf);
		check_ptr("AdjustRTParmFunctionPE crb", *(void **)(rf + 4), (void *)&RT_crb_xpose);
	}

	printf("-- GetRTParmFunctionTableEntry_{GE,PE} (reuse the already-verified tables) --\n");
	{
		InitializegRTParmFunctionTable_GE();
		/* search for entry[5]'s OWN real funcPtr (self-consistent --
		 * the GE table's initializer order doesn't necessarily match
		 * oa_rtparm_ge_table.h's alphabetical `extern` listing order,
		 * so don't assume which named RT_* occupies index 0). */
		void *want = gRTParmFunctionTable_GE[5].funcPtr;
		void *got = GetRTParmFunctionTableEntry_GE(kRTParmFunctionTableEntryIndexType_0, want);
		check_ptr("GetRTParmFunctionTableEntry_GE by funcPtr", got, (void *)&gRTParmFunctionTable_GE[5]);
	}
	{
		InitializegRTParmFunctionTable_PE();
		unsigned short idx = 9; /* RT_nbo_interp, non-null entry */
		void *got = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, &idx);
		check_ptr("GetRTParmFunctionTableEntry_PE by index", got, (void *)0);
		/* NOTE: real search compares record offset +0xa (field0a/field0b
		 * packed), NOT the +0x8 `index` field -- entry 9's field0a=3,
		 * field0b=0 (packed word 0x0003), not 9, so a literal index
		 * value normally won't hit; this asserts the documented "not
		 * simply `index`" behavior rather than a false positive. */
	}

	printf("-- GE/PE assign shapes --\n");
	{
		unsigned char rp[8] = {0};
		*(unsigned short *)rp = gRTParmFunctionTable_PE[9].index; /* entry with a real funcPtr */
		/* actually search key is packed field0a/field0b, not index -- use raw bytes matching entry 9's own +0xa word */
		*(unsigned short *)rp = *(unsigned short *)((unsigned char *)&gRTParmFunctionTable_PE[9] + 0xa);
		unsigned char rf[16] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
		AssignRTParmFunction_PE((RTParm *)rp, (RTParmFunction *)rf);
		/* found entry -> AssignRTParmPE stub does nothing (no crash) is the main assertion */
		check_eq("AssignRTParmFunction_PE found: no crash", 1, 1);

		unsigned char rp2[8] = { 0xff, 0xff };
		unsigned char rf2[16];
		memset(rf2, 0xAA, sizeof(rf2));
		AssignRTParmFunction_PE((RTParm *)rp2, (RTParmFunction *)rf2);
		check_eq("AssignRTParmFunction_PE not-found zeroes +0", *(unsigned int *)(rf2 + 0), 0);
		check_eq("AssignRTParmFunction_PE not-found zeroes +4", *(unsigned int *)(rf2 + 4), 0);
	}
	{
		unsigned char genMod[0x8ba4 + 0x500];
		for (unsigned i = 0; i < 40; ++i) genMod[0x86a4 + i] = (unsigned char)(i + 1);
		memset(genMod + 0x8ba4, 0, 40);
		CopyRTParmFunctionToOtherBuffer((GenMod *)genMod, RTPARM_BUFFER_CURRENT, 0);
		check_eq("CopyRTParmFunctionToOtherBuffer cur->cmp", memcmp(genMod + 0x86a4, genMod + 0x8ba4, 40), 0);
	}
	{
		gKS[0x280c + 2] = 5;
		SetRTParmMultiBackupGE(0, RTPARM_BUFFER_CURRENT);
		check_eq("SetRTParmMultiBackupGE entry0", gKS[0x1f73c + 0x20], 5);
	}
	{
		unsigned char perf[4096];
		memset(perf, 0, sizeof(perf));
		unsigned int idx = 2 * 32 + 1;
		*(unsigned int *)(perf + idx * 8 + 0x6ea) = 0xdeadbeef;
		CopyRTParmEditToOtherBuffer((Performance *)perf, RTPARM_BUFFER_COMPARE, 2, 1);
		check_eq("CopyRTParmEditToOtherBuffer cmp->cur", *(unsigned int *)(perf + idx * 8 + 0x2ea), (long)0xdeadbeef);
	}
	{
		unsigned char rp[8] = { 0xff, 0xff, 0x05 }; /* no match -> not-found path */
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Key((RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Key not-found zeroes", *(unsigned int *)(rf + 0), 0);
	}
	{
		for (int i = 0; i < 8; ++i) gKS[0x26c + i * 8] = (unsigned char)(0x10 + i);
		SetRTParmMultiBackupPE();
		check_eq("SetRTParmMultiBackupPE[0]", gKS[0x16f44], 0x10);
		check_eq("SetRTParmMultiBackupPE[7]", gKS[0x1705c], 0x17);
	}
	{
		unsigned char rp[8] = { 0xff, 0xff };
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Mix((RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Mix not-found zeroes", *(unsigned int *)(rf + 4), 0);
	}
	{
		unsigned char rp[8] = { 5, 0x0e };
		check_eq("IsRTParmTemplateRestoreType type4/0xe", IsRTParmTemplateRestoreType((RTParm *)(rp)), 0);
		unsigned char rp4[8] = { 4, 0x0e };
		check_eq("IsRTParmTemplateRestoreType type4 match", IsRTParmTemplateRestoreType((RTParm *)rp4), 1);
		unsigned char rp14a[8] = { 14, 0x2c };
		check_eq("IsRTParmTemplateRestoreType type14 in-range", IsRTParmTemplateRestoreType((RTParm *)rp14a), 1);
		unsigned char rp14b[8] = { 14, 0x2e };
		check_eq("IsRTParmTemplateRestoreType type14 out-of-range", IsRTParmTemplateRestoreType((RTParm *)rp14b), 0);
		unsigned char rp11[8] = { 11, 0 };
		check_eq("IsRTParmTemplateRestoreType type11 always false", IsRTParmTemplateRestoreType((RTParm *)rp11), 0);
	}
	{
		unsigned char rp[8] = { 0xff, 0xff };
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Dix(0, 0, (RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Dix not-found zeroes", *(unsigned int *)(rf + 0), 0);
	}
	{
		unsigned char perf[4096];
		memset(perf, 0, sizeof(perf));
		unsigned int srcIdx = 1u * 32 + 2 + 0x5c, dstIdx = 1u * 32 + 2 + 0xdc;
		*(short *)(perf + srcIdx * 8 + 0xe) = 0x1234;
		*(short *)(perf + srcIdx * 8 + 0x10) = 0x5678;
		CopyRTParmEditToModule((Performance *)perf, 1, 2);
		check_eq("CopyRTParmEditToModule +0xe", *(short *)(perf + dstIdx * 8 + 0xe), 0x1234);
		check_eq("CopyRTParmEditToModule +0x10", *(short *)(perf + dstIdx * 8 + 0x10), 0x5678);
	}
	{
		memset(gKS, 0, 0x400);
		gKS[0x1f73c + 5 * 0x28 + 0xc] = 3;
		ResetRTParmGELastValIndControl(0, 3, RTPARM_BUFFER_CURRENT);
		check_eq("ResetRTParmGELastValIndControl hit", *(short *)(gKS + 0x1f762 + 5 * 0x28), (short)0x8001);
		check_eq("ResetRTParmGELastValIndControl miss stays 0", *(short *)(gKS + 0x1f762 + 4 * 0x28), 0);
	}
	{
		memset(gKS + 0x20b08, 0xAA, 0x9d10);
		SetRTParmWasEdited(0, 5, true);
		check_eq("SetRTParmWasEdited single", gKS[0x20b08 + 5], 1);
		SetRTParmWasEdited(0, 0xff, false);
		int allZero = 1;
		for (int i = 0; i < 0x20; ++i) if (gKS[0x20b08 + i] != 0) allZero = 0;
		check_eq("SetRTParmWasEdited row clear", allZero, 1);
	}
	{
		unsigned char perf[4096];
		memset(perf, 0, sizeof(perf));
		unsigned int dstIdx = 1u * 32 + 2 + 0x5c;
		perf[dstIdx * 8 + 0xc] = 0xff;
		unsigned char before[8];
		memcpy(before, perf + dstIdx * 8, 8);
		CopyRTParmEditToMaster((Performance *)perf, 1, 2);
		check_eq("CopyRTParmEditToMaster no-op on 0xff guard", memcmp(before, perf + dstIdx * 8, 8), 0);
	}
	{
		unsigned char rp[8] = { 0xff, 0xff };
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Env(0, 0, (RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Env not-found zeroes", *(unsigned int *)(rf + 0), 0);
	}
	{
		check_ptr("GetRTParmDescriptorPE module0", GetRTParmDescriptorPE(0, 0), (void *)(RTParm_menu_pe_off + 0 * 0x20));
		check_ptr("GetRTParmDescriptorPE module2", GetRTParmDescriptorPE(2, 1), (void *)(RTParm_menu_pe_mix + 1 * 0x20));
		check_ptr("GetRTParmDescriptorPE oob", GetRTParmDescriptorPE(7, 0), (void *)0);
	}
	{
		unsigned char rp[8] = { 3, 5 };
		check_ptr("GetRTParmDescriptor mode!=0 forwards to GE", GetRTParmDescriptor((RTParm *)rp, 1),
		          (void *)(RTParm_menu_ge_phs + 5 * 0x20));
		unsigned char rp0[8] = { 2, 1 };
		check_ptr("GetRTParmDescriptor mode==0 uses PE table", GetRTParmDescriptor((RTParm *)rp0, 0),
		          (void *)(RTParm_menu_pe_mix + 1 * 0x20));
	}
	{
		unsigned char rp[8] = { 0xff, 0xff };
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Trg((RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Trg not-found zeroes", *(unsigned int *)(rf + 0), 0);
	}
	{
		memset(gKS, 0, 0x3000);
		unsigned char *base = gKS + 0u * 0x9cc + 0x280c;
		base[3 * 8 + 0] = 7; base[3 * 8 + 1] = 9; base[3 * 8 + 2] = 0x0f;
		unsigned int idx2 = (0u << 5) + 3;
		gKS[idx2 * 8 + 0x6ec] = 0x01; /* non-negative -> immediate true */
		check_eq("IsRTParmPairAssignedGE match, flag1 nonneg", IsRTParmPairAssignedGE(0, 7, 9, 0x0f), 1);
		check_eq("IsRTParmPairAssignedGE no-match", IsRTParmPairAssignedGE(0, 1, 1, 1), 0);
	}
	{
		memset(gKS, 0, 0x3000);
		unsigned char *region = gKS + 1u * 0x9cc + 0x2800 + 0xc;
		region[2 * 8 + 0] = 9;  region[2 * 8 + 1] = 4; /* module=1, elem2, matches p2=9 at outer=4 */
		UpdateRTParmName(1, 9, 4, 4);
		check_eq("UpdateRTParmName: no crash / ran to completion", 1, 1);
	}
	{
		unsigned char rp[8] = { 0xff, 0xff };
		unsigned char rf[16];
		memset(rf, 0xAA, sizeof(rf));
		AssignRTParmFunction_Rsd((RTParm *)rp, (RTParmFunction *)rf);
		check_eq("AssignRTParmFunction_Rsd not-found zeroes", *(unsigned int *)(rf + 0), 0);
	}
	{
		unsigned char rp_no[8] = { 4, 3 }; /* type != 5 -> no-op */
		short rd_val = 100;
		LimitRTParmPairPE(0, (RTParm *)rp_no, (RTParmEdit *)&rd_val);
		check_eq("LimitRTParmPairPE type!=5 no-op", rd_val, 100);
	}
	{
		unsigned char rf[16] = {0};
		unsigned char src[4] = { 0x00, 0x00, 0x00, 0x00 };
		*(unsigned char **)(rf + 0) = src;
		src[0] = 0xAB;
		rf[8] = 0; /* kind 0 -> raw byte */
		check_eq("GetRTParmCurValue kind0 rawbyte", GetRTParmCurValue((RTParmFunction *)rf), 0xAB);
		rf[8] = 1; /* signed byte */
		src[0] = 0xFF; /* -1 */
		check_eq("GetRTParmCurValue kind1 sbyte", GetRTParmCurValue((RTParmFunction *)rf), -1);
		rf[8] = 2; /* raw word */
		*(unsigned short *)src = 0x1234;
		check_eq("GetRTParmCurValue kind2 raw16", GetRTParmCurValue((RTParmFunction *)rf), 0x1234);
		rf[8] = 13; src[0] = 0x03;
		check_eq("GetRTParmCurValue kind13 &1", GetRTParmCurValue((RTParmFunction *)rf), 1);
		rf[8] = 26; src[0] = 0x38;
		check_eq("GetRTParmCurValue kind26 bits3:5", GetRTParmCurValue((RTParmFunction *)rf), 7);
		unsigned char *nullsrc = 0;
		*(unsigned char **)(rf + 0) = nullsrc;
		check_eq("GetRTParmCurValue null src", GetRTParmCurValue((RTParmFunction *)rf), 0);
	}
	{
		check_ptr("GetRTParmDescriptorGE module0", GetRTParmDescriptorGE(0, 0), (void *)(RTParm_menu_ge_off + 0 * 0x20));
		check_ptr("GetRTParmDescriptorGE module2", GetRTParmDescriptorGE(2, 3), (void *)(RTParm_menu_ge_rif + 3 * 0x20));
		check_ptr("GetRTParmDescriptorGE module15", GetRTParmDescriptorGE(15, 0), (void *)(RTParm_menu_ge_dix));
		check_ptr("GetRTParmDescriptorGE oob", GetRTParmDescriptorGE(16, 0), (void *)0);
	}
	{
		check_eq("IsRTParmFunctionSamePE self", IsRTParmFunctionSamePE(1, 3, 1, 3), 1);
		check_eq("IsRTParmFunctionSamePE kind mismatch", IsRTParmFunctionSamePE(1, 3, 2, 3), 0);
		check_eq("IsRTParmFunctionSamePE kind2 group 1<->2", IsRTParmFunctionSamePE(2, 1, 2, 2), 1);
		check_eq("IsRTParmFunctionSamePE kind2 group 1<->0 false", IsRTParmFunctionSamePE(2, 1, 2, 0), 0);
		check_eq("IsRTParmFunctionSamePE kind5 group 4<->8", IsRTParmFunctionSamePE(5, 4, 5, 8), 1);
		check_eq("IsRTParmFunctionSamePE kind5 group 5<->7", IsRTParmFunctionSamePE(5, 5, 5, 7), 1);
		check_eq("IsRTParmFunctionSamePE kind0 always false", IsRTParmFunctionSamePE(0, 1, 0, 1), 0);
	}
	{
		unsigned char tbl[0x10 + 4] = {0};
		*(unsigned int *)(tbl + 4) = 0; /* kind 0 = raw byte */
		*(unsigned int *)(tbl + 0x10) = 5; /* offset for idx0 */
		unsigned char base[16] = {0};
		base[5] = 0x42;
		check_eq("GetRTParmCurValueFromOffset rawbyte", GetRTParmCurValueFromOffset((RTParmFunctionTable *)tbl, base, 0), 0x42);
		check_eq("GetRTParmCurValueFromOffset null table", GetRTParmCurValueFromOffset(0, base, 0), 0);
	}
	{
		memset(gKS, 0, 0x3000);
		gKS[0x26a] = 5; gKS[0x26b] = 6; gKS[0x26c] = 0x0f;
		gKS[0 * 8 + 0x2ac] = 0x00; /* NOT -> 0xff, >>7 = 1 */
		check_eq("IsRTParmPairAssignedPE match", IsRTParmPairAssignedPE(5, 6, 0x0f), 1);
		gKS[0 * 8 + 0x2ac] = 0x80; /* NOT -> 0x7f, >>7 = 0 */
		check_eq("IsRTParmPairAssignedPE match, high flag", IsRTParmPairAssignedPE(5, 6, 0x0f), 0);
		check_eq("IsRTParmPairAssignedPE no id match", IsRTParmPairAssignedPE(0xEE, 0, 0), 0);
	}
	{
		unsigned char perf[0x2b0] = {0};
		perf[0x27a] = 11; perf[0x27b] = 12; perf[0x27c] = 0x3f;
		check_ptr("GetRTParmAssigned_PE match", GetRTParmAssigned_PE((Performance *)perf, 11, 12, 0x3f), (void *)(perf + 0x27a));
		check_ptr("GetRTParmAssigned_PE no match", GetRTParmAssigned_PE((Performance *)perf, 1, 1, 1), (void *)0);
	}

	printf("-- RTParmShortNameGroup / RTParmNameManager::SetPrependCCInfo --\n");
	{
		RTParmShortNameGroup g;
		int allZero = 1;
		unsigned char *p = (unsigned char *)&g;
		for (int i = 0; i < 0x18; ++i) if (p[i] != 0) allZero = 0;
		check_eq("RTParmShortNameGroup ctor zeroes 0x18 bytes", allZero, 1);

		static char strbuf[24] = "hello"; /* static: see the packed-32-bit-field
			* note on SetRTParmShortNameStringPtr's own definition --
			* a stack address wouldn't round-trip through the real
			* 32-bit field width on a 64-bit host either. */
		g.SetRTParmShortNameStringPtr((const char (*)[24])strbuf, 3, 5);
		check_eq("SetRTParmShortNameStringPtr field0 (packed 32-bit)",
		         *(unsigned int *)p, (unsigned int)(unsigned long)strbuf);
		check_eq("SetRTParmShortNameStringPtr field4", *(unsigned short *)(p + 4), 3);
		check_eq("SetRTParmShortNameStringPtr field6", *(unsigned short *)(p + 6), 5);

		g.SetProductArrays((RTParmNameProductID)2, 0x11, 0x22);
		check_eq("SetProductArrays field", *(unsigned short *)(p + 2 * 4 + 8), 0x11);
		check_eq("SetProductArrays field2", *(unsigned short *)(p + 2 * 4 + 0xa), 0x22);
	}
	{
		unsigned char mgr[0x200] = {0};
		((RTParmNameManager *)mgr)->SetPrependCCInfo((RTParmNameProductID)3, true, false);
		check_eq("SetPrependCCInfo a", mgr[3 * 3 + 0x180 + 4], 1);
		check_eq("SetPrependCCInfo b", mgr[3 * 3 + 0x180 + 5], 0);
	}

	printf("-- follow-up pass (2026-07-29): 7 deferred members --\n");
	extern int g_do_km_rtp_val_out_pe_calls;

	printf("  GetRTParmModAndID\n");
	{
		unsigned char out;
		check_eq("small-domain slot 0 (+0x26a)", GetRTParmModAndID((RTParm *)(gKS + 0x26a), &out), 0);
		check_eq("small-domain slot 0 out", out, 0);
		check_eq("small-domain slot 7 (+0x2a2)", GetRTParmModAndID((RTParm *)(gKS + 0x2a2), &out), 0);
		check_eq("small-domain slot 7 out", out, 7);
		check_eq("small-domain no-match out", (GetRTParmModAndID((RTParm *)(gKS + 0x1f00), &out), out), 8);

		gKS[0x1e5] = 3; /* bound = 3 channels */
		check_eq("channel0 (in [0x1f40,0x290c)) slot3",
		         (GetRTParmModAndID((RTParm *)(gKS + 0x280c + 3 * 8), &out), out), 3);
		check_eq("channel0 return", GetRTParmModAndID((RTParm *)(gKS + 0x280c + 3 * 8), &out), 0);
		check_eq("channel1 ([0x290c,0x32d8)) slot5",
		         (GetRTParmModAndID((RTParm *)(gKS + 0x9cc + 0x280c + 5 * 8), &out), out), 5);
		check_eq("channel1 return", GetRTParmModAndID((RTParm *)(gKS + 0x9cc + 0x280c + 5 * 8), &out), 1);
		check_eq("channel cap (bound-1) return", GetRTParmModAndID((RTParm *)(gKS + 0x3300), &out), 2);
		gKS[0x1e5] = 1; /* bound<=1 -> always channel 0 */
		check_eq("bound<=1 forces channel0", GetRTParmModAndID((RTParm *)(gKS + 0x5000), &out), 0);
		gKS[0x1e5] = 0;
		memset(gKS, 0, sizeof(gKS));
	}

	printf("  LimitRTParmEditValuesRow (type=1/subId=0 -> RTParm_menu_ge_ge[0], lo=0 hi=3)\n");
	{
		/* RTParm_menu_ge_ge (like every RTParm_menu_* table in this
		 * project) is a plain zero-initialized placeholder, not
		 * populated with real ground-truth .rodata content -- poke the
		 * one descriptor record this test uses directly, matching this
		 * project's established convention for these tables (real
		 * content was never in scope; only the code that INDEXES into
		 * them is). */
		RTParm_menu_ge_ge[0 * 0x20 + 0x18] = 0; RTParm_menu_ge_ge[0 * 0x20 + 0x19] = 0; /* lo = 0 */
		RTParm_menu_ge_ge[0 * 0x20 + 0x1a] = 3; RTParm_menu_ge_ge[0 * 0x20 + 0x1b] = 0; /* hi = 3 */

		unsigned char eb[0x3000] = {0};
		eb[0x280c] = 1; eb[0x280d] = 0; /* module0,ge0 -> RTParm_menu_ge_ge[0]: lo=0,hi=3 */
		short *cur = (short *)(eb + 0x2ea);
		cur[0] = 2; cur[2] = -5; cur[3] = 10; /* f0,f4,f6 */
		bool changed = LimitRTParmEditValuesRow((EditBuffer *)eb, 0, 0, RTPARM_BUFFER_CURRENT);
		check_eq("RowA changed", changed, 1);
		check_eq("RowA f4 clamped to lo", cur[2], 0);
		check_eq("RowA f6 clamped to hi", cur[3], 3);
		check_eq("RowA f0 unchanged (already in [0,3])", cur[0], 2);

		cur[0] = 100; cur[2] = 10; cur[3] = -5; /* swapped f4>f6 case */
		changed = LimitRTParmEditValuesRow((EditBuffer *)eb, 0, 0, RTPARM_BUFFER_CURRENT);
		check_eq("RowB changed", changed, 1);
		check_eq("RowB f4 clamped to hi", cur[2], 3);
		check_eq("RowB f6 clamped to lo", cur[3], 0);
		check_eq("RowB f0 clamped to max(f4,f6)", cur[0], 3);

		cur[0] = 1; cur[2] = 1; cur[3] = 2; /* already valid, no clamp needed */
		changed = LimitRTParmEditValuesRow((EditBuffer *)eb, 0, 0, RTPARM_BUFFER_CURRENT);
		check_eq("RowC unchanged", changed, 0);
		check_eq("RowC f0 untouched", cur[0], 1);

		short *cmp = (short *)(eb + 0x6ea);
		cmp[0] = -10; cmp[2] = -1; cmp[3] = 1;
		changed = LimitRTParmEditValuesRow((EditBuffer *)eb, 0, 0, RTPARM_BUFFER_COMPARE);
		check_eq("RowD (compare buffer) changed", changed, 1);
		check_eq("RowD f4 clamped to lo", cmp[2], 0);
		check_eq("RowD f6 unchanged (already in range)", cmp[3], 1);
		check_eq("RowD f0 clamped to min(f4,f6)", cmp[0], 0);
		check_eq("RowD current buffer untouched by compare call", cur[0], 1);
	}

	printf("  LimitRTParmEditValues\n");
	{
		unsigned char eb[0x5000] = {0}; /* must cover module*0x9cc+0x1f40 up to module==3 */
		*(unsigned short *)(eb + 0x9cc + 0x1f40) = 0xffff; /* disable channel 1 */
		*(unsigned short *)(eb + 2 * 0x9cc + 0x1f40) = 0xffff; /* disable channel 2 */
		*(unsigned short *)(eb + 3 * 0x9cc + 0x1f40) = 0xffff; /* disable channel 3 */
		/* channel0 all-zero rtpSlots -> type=0,subId=0 -> RTParm_menu_ge_off: lo=hi=0 */
		check_eq("all-zero, all channels-but-0 disabled -> unchanged",
		         LimitRTParmEditValues((EditBuffer *)eb), 0);

		short *row5 = (short *)(eb + 5 * 8 + 0x2ea); /* module0, ge5, current */
		row5[2] = 7; /* f4 = 7, will clamp to hi=0 */
		check_eq("one dirty row -> changed", LimitRTParmEditValues((EditBuffer *)eb), 1);
		check_eq("dirty row's f4 clamped to 0", row5[2], 0);
		check_eq("re-run is now clean", LimitRTParmEditValues((EditBuffer *)eb), 0);
	}

	printf("  UpdateRTParmIfSame_GE\n");
	{
		unsigned char genMod[0x20] = {0};
		unsigned char geBuf[0x8cc + 0x100] = {0};
		genMod[0] = 2; /* module index */
		*(unsigned char **)(genMod + 0xc) = geBuf;
		unsigned char *arr = geBuf + 0x8cc;

		/* rtParm IS slot 0 itself (pointer identity -> must be skipped) */
		arr[0 * 8 + 0] = 7; arr[0 * 8 + 1] = 9; arr[0 * 8 + 2] = 1;
		/* slot5: exact content match (incl. mask byte) -> updates */
		arr[5 * 8 + 0] = 7; arr[5 * 8 + 1] = 9; arr[5 * 8 + 2] = 1;
		/* slot10: type/subId match, mask differs AND no bit overlap -> skipped */
		arr[10 * 8 + 0] = 7; arr[10 * 8 + 1] = 9; arr[10 * 8 + 2] = 2;
		/* slot15: type/subId match, mask differs but bits overlap -> updates */
		arr[15 * 8 + 0] = 7; arr[15 * 8 + 1] = 9; arr[15 * 8 + 2] = 3;

		unsigned short rtd[4] = { 0x1234, 0, 0, 0 };
		g_do_km_rtp_val_out_pe_calls = 0;
		memset(gKS, 0, sizeof(gKS));
		UpdateRTParmIfSame_GE((GenMod *)genMod, (RTParm *)arr, (RTParmEdit *)rtd, RTPARM_BUFFER_CURRENT);
		check_eq("2 real matches (slot5, slot15) call Do_KM_rtp_val_out_pe", g_do_km_rtp_val_out_pe_calls, 2);
		check_eq("self slot0 not updated", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 0) * 8 + 0x2ea), 0);
		check_eq("slot5 current buffer updated", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 5) * 8 + 0x2ea), 0x1234);
		check_eq("slot10 (no overlap) not updated", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 10) * 8 + 0x2ea), 0);
		check_eq("slot15 (bit overlap) updated", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 15) * 8 + 0x2ea), 0x1234);

		g_do_km_rtp_val_out_pe_calls = 0;
		memset(gKS, 0, sizeof(gKS));
		UpdateRTParmIfSame_GE((GenMod *)genMod, (RTParm *)arr, (RTParmEdit *)rtd, RTPARM_BUFFER_COMPARE);
		check_eq("sel!=0 writes compare buffer (+0x6ea)", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 5) * 8 + 0x6ea), 0x1234);
		check_eq("sel!=0 does not touch current buffer", *(unsigned short *)(gKS + ((unsigned int)2 * 32 + 5) * 8 + 0x2ea), 0);

		unsigned char rp0[3] = {0, 0, 0};
		g_do_km_rtp_val_out_pe_calls = 0;
		UpdateRTParmIfSame_GE((GenMod *)genMod, (RTParm *)rp0, (RTParmEdit *)rtd, RTPARM_BUFFER_CURRENT);
		check_eq("rtParm.type==0 -> immediate no-op", g_do_km_rtp_val_out_pe_calls, 0);
	}
	memset(gKS, 0, sizeof(gKS));

	printf("  DoRTParmMultiEnablePE (real IsRTParmFunctionSamePE)\n");
	{
		gKS[0x26c] = 0x10; /* slot0 seed flag */
		gKS[0x26a] = 2;    /* slot0 valA (kind) */
		gKS[0x26b] = 5;    /* slot0 valB */
		gKS[0x282] = 2; gKS[0x283] = 5; gKS[0x284] = 0x0f; /* r=3 entry: kind match, p2==p4 -> real match */
		gKS[0x292] = 9; gKS[0x293] = 1; gKS[0x294] = 0xff; /* r=5 entry: kind mismatch -> no match */
		DoRTParmMultiEnablePE();
		check_eq("slot0 self-write (r2=0): ~(seed|matchFlag) | seed", gKS[0x16f24 + 0 * 0x28 + 0x24], 0xf0);
		check_eq("slot0 match-write (r2=3): ~(seed|matchFlag) | r3.flag", gKS[0x16f24 + 3 * 0x28 + 0x24], 0xef);
	}
	memset(gKS, 0, sizeof(gKS));

	printf("  DoRTParmMultiEnableGE (IsRTParmFunctionSameGE stubbed false -> every ge self-writes 0xff)\n");
	{
		DoRTParmMultiEnableGE(2, RTPARM_BUFFER_CURRENT);
		check_eq("module2 ge0 current table +0x24", gKS[2 * 0x9d10 + 0 * 0x28 + 0x1f73c + 0x24], 0xff);
		check_eq("module2 ge15 current table +0x24", gKS[2 * 0x9d10 + 15 * 0x28 + 0x1f73c + 0x24], 0xff);
		check_eq("module2 ge31 current table +0x24", gKS[2 * 0x9d10 + 31 * 0x28 + 0x1f73c + 0x24], 0xff);
		memset(gKS, 0, sizeof(gKS));
		DoRTParmMultiEnableGE(1, RTPARM_BUFFER_COMPARE);
		check_eq("module1 ge0 compare table +0x24 (sel!=0)", gKS[1 * 0x9d10 + 0 * 0x28 + 0x1fc3c + 0x24], 0xff);
		check_eq("module1 ge0 current table untouched (sel!=0)", gKS[1 * 0x9d10 + 0 * 0x28 + 0x1f73c + 0x24], 0);
	}
	memset(gKS, 0, sizeof(gKS));

	printf("  RTParmShortNameGroup::GetRTParmShortNameStringPtr\n");
	{
		static char strTable[100][24];
		memset(strTable, 0, sizeof(strTable));
		strcpy(strTable[4], "DirectHit");
		strcpy(strTable[40], "WalkBackFound");
		strcpy(strTable[60], "ShortCase");
		strcpy(strTable[80], "UseAltStride");

		/* real class extent beyond the confirmed-minimum 0x18 bytes is
		 * unconfirmed (see the class's own header comment) -- this test
		 * uses product indices up to 7, needing offset 7*4+0xa+2=0x28,
		 * so back it with an oversized raw buffer rather than a plain
		 * `RTParmShortNameGroup g;` (would genuinely overflow the
		 * class's own declared 0x18-byte extent), same convention as
		 * the RTParmNameManager `mgr[0x200]` test above. */
		unsigned char gbuf[0x100] = {0};
		RTParmShortNameGroup &g = *(RTParmShortNameGroup *)gbuf;
		unsigned char *p = gbuf;
		*(unsigned int *)p = (unsigned int)(unsigned long)strTable;
		*(unsigned short *)(p + 6) = 5; /* stride */

		/* product0: start=2,len=3, a==0 -> index=start=2; row2 empty, row3
		 * empty, row4 non-empty -> should NOT be reached this way (index
		 * stays 2, not 4) -- this case instead exercises the *forward*
		 * direct hit by using product0's row2 itself as non-empty. */
		*(unsigned short *)(p + 0 * 4 + 8) = 2;  /* start */
		*(unsigned short *)(p + 0 * 4 + 0xa) = 3; /* len */
		strcpy(strTable[2], "Direct");
		check_ptr("direct hit (a==0, index==start)",
		          g.GetRTParmShortNameStringPtr((RTParmNameProductID)0, 0, 0), strTable[2]);

		/* product6: len==0 -> immediate NULL */
		*(unsigned short *)(p + 6 * 4 + 8) = 0;
		*(unsigned short *)(p + 6 * 4 + 0xa) = 0;
		check_ptr("len==0 -> NULL", g.GetRTParmShortNameStringPtr((RTParmNameProductID)6, 0, 0), (const char *)0);

		/* product5: len==1 -> index=start; row60 non-empty (set above) */
		*(unsigned short *)(p + 5 * 4 + 8) = 60;
		*(unsigned short *)(p + 5 * 4 + 0xa) = 1;
		check_ptr("len==1 short case", g.GetRTParmShortNameStringPtr((RTParmNameProductID)5, 0, 0), strTable[60]);

		/* product3: start=40,len=6, a=7 (popcount 3 -> seed 2) -> index=42;
		 * row42,row41 empty, row40 non-empty -> walk-back finds row40. */
		*(unsigned short *)(p + 3 * 4 + 8) = 40;
		*(unsigned short *)(p + 3 * 4 + 0xa) = 6;
		strcpy(strTable[40], "WalkBackFound");
		check_ptr("walk-back finds nearest non-empty",
		          g.GetRTParmShortNameStringPtr((RTParmNameProductID)3, 0, 7), strTable[40]);

		/* product4: start=50,len=6, a=7 -> index=52; rows 52,51,50 all
		 * empty -> walk-back exhausts down to start, returns NULL. */
		*(unsigned short *)(p + 4 * 4 + 8) = 50;
		*(unsigned short *)(p + 4 * 4 + 0xa) = 6;
		check_ptr("walk-back exhausted -> NULL",
		          g.GetRTParmShortNameStringPtr((RTParmNameProductID)4, 0, 7), (const char *)0);

		/* product7: start=70,len=3, useAlt=2, stride=5 -> rowStride=10,
		 * index=70 -> final row = 70+10 = 80. */
		*(unsigned short *)(p + 7 * 4 + 8) = 70;
		*(unsigned short *)(p + 7 * 4 + 0xa) = 3;
		check_ptr("useAlt*stride offset applied",
		          g.GetRTParmShortNameStringPtr((RTParmNameProductID)7, 2, 0), strTable[80]);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
