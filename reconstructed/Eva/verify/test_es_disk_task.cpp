/*
 * test_es_disk_task.cpp  -  host-side known-answer test for CESDiskTask's
 * 39 real methods landed in round 45 (solo, 2026-07-29). See
 * include/es_disk_task.h for the full derivation.
 */

#include <cstdio>
#include <cstring>
#include "es_disk_task.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct DiskTaskTestHooks {
	static void SetBankProgFields(CESDiskTask &t, unsigned char bank, unsigned char prog,
				       unsigned char combiBank, unsigned char combi, unsigned char fullRangeBank)
	{
		unsigned char *p = reinterpret_cast<unsigned char *>(&t);
		p[0x84] = bank;
		p[0x85] = prog;
		p[0x86] = combiBank;
		p[0x87] = combi;
		p[0x88] = fullRangeBank;
	}
};

struct TestableDiskTask : public CESDiskTask {
	/* CTask's own protected Tier-B default ctor (task.h) -- established
	 * "CTask-derived class, real ctor not reconstructed yet" convention,
	 * same as es_disk_command_task.h's own TestableDiskCommandTask
	 * (round 44). None of this round's 39 methods touch CTask's own
	 * state or any vtable. */
	TestableDiskTask() : CESDiskTask() {}
};

int main()
{
	TestableDiskTask t;

	/* [1] plain static-global accessors */
	unsigned char buf;
	t.SetFilerMsg(0, (const unsigned char *)"\x05");
	t.GetFilerMsg(0, &buf);
	check("SetFilerMsg/GetFilerMsg round-trip", buf == 5);

	t.SetProgress(0, (const unsigned char *)"\x2a");
	t.GetProgress(0, &buf);
	check("SetProgress/GetProgress round-trip", buf == 0x2a);

	t.SetMultipleSelect(0, (const unsigned char *)"\x01");
	t.GetMultipleSelect(0, &buf);
	check("SetMultipleSelect/GetMultipleSelect round-trip", buf == 1);

	t.SetNotifyFileSelected(0, (const unsigned char *)"\x01");
	t.GetNotifyFileSelected(0, &buf);
	check("SetNotifyFileSelected/GetNotifyFileSelected round-trip", buf == 1);

	/* CDiskUtil::WriteByteToSharedBuffer's own real body is genuinely
	 * unrecovered -- es_disk_task.cpp provides an identity-pass-through
	 * stand-in (see its own header comment), so s_ucWriteRet ends up
	 * == the input byte unchanged. */
	unsigned char writeArg = 7;
	t.SetWriteExcl(0, &writeArg);
	t.GetResultWriteExcl(0, &buf);
	check("SetWriteExcl -> CDiskUtil::WriteByteToSharedBuffer(7) stand-in, GetResultWriteExcl reads it back", buf == 7);

	check("GetDefaultFileName returns a valid pointer", CESDiskTask::GetDefaultFileName() != 0);

	/* [2] this-offset accessors -- confirmed bank/prog packing formula */
	DiskTaskTestHooks::SetBankProgFields(t, 3, 0x10, 5, 0x20, 9);
	char shortBuf[2];
	t.GetBankProgToWrite(0, shortBuf);
	check("GetBankProgToWrite == bank*0x80+prog (3*0x80+0x10)",
	      *(unsigned short *)shortBuf == 3 * 0x80 + 0x10);
	t.GetBankProgToWriteFullRange(0, (unsigned char *)shortBuf);
	check("GetBankProgToWriteFullRange == fullRangeBank*0x80+prog (9*0x80+0x10, SAME prog field)",
	      *(unsigned short *)shortBuf == 9 * 0x80 + 0x10);
	t.GetBankCombiToWrite(0, shortBuf);
	check("GetBankCombiToWrite == combiBank*0x80+combi (5*0x80+0x20)",
	      *(unsigned short *)shortBuf == 5 * 0x80 + 0x20);

	/* Real ground truth: SetOscTypeToWrite reads only *param_2 (ONE byte,
	 * zero-extended to the real 4-byte field) -- NOT a 4-byte read,
	 * despite the field itself being 4 bytes wide. Preserved verbatim. */
	unsigned char oscSet[1] = {0x34};
	t.SetOscTypeToWrite(0, oscSet);
	unsigned char oscGet[2];
	t.GetOscTypeToWrite(0, oscGet);
	check("SetOscTypeToWrite reads only 1 byte (zero-extended); GetOscTypeToWrite reads back low 16 bits",
	      *(short *)oscGet == 0x34);

	/* [3] single-branch index-gated dialog family */
	unsigned char u16buf[2];
	*(unsigned short *)u16buf = 0x55aa;
	t.SetLdCombiBankDialog(0, u16buf);
	t.GetLdKarmaGEBankDialog(0, u16buf); /* shares s_usDestBank with LdCombiBankDialog */
	check("SetLdCombiBankDialog/GetLdKarmaGEBankDialog share s_usDestBank", *(unsigned short *)u16buf == 0x55aa);
	*(unsigned short *)u16buf = 0;
	t.GetLdCombiBankDialog(1, u16buf); /* param_1 != 0: real no-op */
	check("GetLdCombiBankDialog(param_1=1): real no-op, buffer untouched", *(unsigned short *)u16buf == 0);

	*(unsigned short *)u16buf = 0x99;
	t.SetLdDkitBankDialog(0, u16buf);
	*(unsigned short *)u16buf = 0;
	t.GetLdDkitBankDialog(0, u16buf);
	check("SetLdDkitBankDialog/GetLdDkitBankDialog round-trip (own sDestBankIndex, not shared)",
	      *(unsigned short *)u16buf == 0x99);
	/* confirm sDestBankIndex is genuinely independent of s_usDestBank */
	*(unsigned short *)u16buf = 0x55aa;
	t.SetLdCombiBankDialog(0, u16buf);
	t.GetLdDkitBankDialog(0, u16buf);
	check("sDestBankIndex independent of s_usDestBank", *(unsigned short *)u16buf == 0x99);

	unsigned char regionVal = 1;
	t.SetLoadRegionsDialog(0, &regionVal);
	buf = 0;
	t.GetLoadRegionsDialog(0, &buf);
	check("SetLoadRegionsDialog/GetLoadRegionsDialog round-trip", buf == 1);

	unsigned char cdrwVal = 2;
	t.SetEraseCDRWDialog(0, &cdrwVal);
	buf = 0;
	t.GetEraseCDRWDialog(0, &buf);
	check("SetEraseCDRWDialog/GetEraseCDRWDialog round-trip", buf == 2);

	/* [4] CopyBytes-backed dialog family (also exercises CopyBytes itself).
	 * GetDeleteDialog/GetNewDirDialog genuinely read/write the FULL
	 * 0xf0-byte buffer contract (real ground truth's own fixed dialog
	 * buffer size) -- test buffers must match, not just hold the string. */
	unsigned char nameIn[0xf0];
	memset(nameIn, 0, sizeof(nameIn));
	strcpy((char *)nameIn, "hello");
	t.SetDeleteDialog(0, nameIn);
	unsigned char nameOut[0xf0];
	memset(nameOut, 0xff, sizeof(nameOut));
	t.GetDeleteDialog(0, nameOut);
	check("SetDeleteDialog/GetDeleteDialog round-trip via CopyBytes", strcmp((char *)nameOut, "hello") == 0);

	unsigned char folderIn[0xf0];
	memset(folderIn, 0, sizeof(folderIn));
	strcpy((char *)folderIn, "MyFolder");
	t.SetNewDirDialog(0, folderIn);
	unsigned char folderOut[0xf0];
	t.GetNewDirDialog(0, folderOut);
	check("SetNewDirDialog/GetNewDirDialog round-trip via CopyBytes", strcmp((char *)folderOut, "MyFolder") == 0);

	unsigned char sampleIn[0xec];
	memset(sampleIn, 0, sizeof(sampleIn));
	strcpy((char *)sampleIn, "sample.wav");
	t.SetLoadSampleDialog(0, sampleIn);
	unsigned char sampleOut[0xec];
	t.GetLoadSampleDialog(0, sampleOut);
	check("SetLoadSampleDialog/GetLoadSampleDialog round-trip (0xec-byte buffer)",
	      strcmp((char *)sampleOut, "sample.wav") == 0);

	/* [5] CopyBytes itself: len==0 and len>0xf0 edge cases */
	{
		unsigned char dst[4] = {1, 1, 1, 1};
		unsigned char src[4] = {9, 9, 9, 9};
		t.CopyBytes(dst, src, 0);
		check("CopyBytes(len=0): no copy, dst[0] null-terminated", dst[0] == 0);
	}
	{
		unsigned char dst[260];
		unsigned char src[260];
		memset(src, 'A', sizeof(src));
		memset(dst, 0xcc, sizeof(dst));
		t.CopyBytes(dst, src, 0x100); /* > 0xf0: copy happens, no null-term */
		check("CopyBytes(len=0x100): copies data", dst[0] == 'A' && dst[0xff] == 'A');
		check("CopyBytes(len=0x100): NO null-term (len>0xf0, dst[0x100] left untouched)", dst[0x100] == 0xcc);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
