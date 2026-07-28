// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_hdr_track_valuegetters.cpp  -  KAT for CSTGHDRTrack's Get*
 * family -- all 15 real weak-symbol candidates, see
 * ../src/engine/stg_hdr_track_valuegetters.cpp.
 *
 * Expected values computed by a SEPARATE Python evaluator (see the
 * batch's own scratch oracle) over the same parsed offset/width/signed
 * facts the source file's own disassembly-derived translation used --
 * not by re-using the .cpp file's C output strings -- against the same
 * deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all
 * mod 0x100. GetValueSolo additionally exercises a second, independent
 * fixture object standing in for CSTGControllerRTData::sInstance (byte
 * pattern i times 0x51 plus 0x13) and a ctx fixture (byte pattern i
 * times 0x2b plus 0x9) for its own +0x18 shift-amount byte.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_hdr_track.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;
CSTGControllerRTData *CSTGControllerRTData::sInstance;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x200
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];
static unsigned char g_sinst[0x40];

int main(void)
{
	printf("CSTGHDRTrack value-getter family known-answer test (15 methods)\n");
	printf("=================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	for (unsigned int i = 0; i < 0x40; i++)
		g_ctxbuf[i] = (unsigned char)(i*0x2b + 0x9);
	for (unsigned int i = 0; i < 0x40; i++)
		g_sinst[i] = (unsigned char)(i*0x51 + 0x13);

	CSTGHDRTrack *s = (CSTGHDRTrack *)g_buf;
	CSTGHDRTrackMessageContext &ctx = *(CSTGHDRTrackMessageContext *)g_ctxbuf;
	CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_sinst;

	s->GetValueOutputBus(ctx);
	check_eq("CSTGHDRTrack::GetValueOutputBus value", CSTGParamsOwner::sValueGetterTemp.value, -77L);
	s->GetValueFXCtrlBus(ctx);
	check_eq("CSTGHDRTrack::GetValueFXCtrlBus value", CSTGParamsOwner::sValueGetterTemp.value, 82L);
	s->GetValueHDRBus(ctx);
	check_eq("CSTGHDRTrack::GetValueHDRBus value", CSTGParamsOwner::sValueGetterTemp.value, -15L);
	s->GetValueEQInputTrim(ctx);
	check_eq("CSTGHDRTrack::GetValueEQInputTrim value", CSTGParamsOwner::sValueGetterTemp.value, 1842229136L);
	check_eq("CSTGHDRTrack::GetValueEQInputTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1842229136L);
	s->GetValueEQLow(ctx);
	check_eq("CSTGHDRTrack::GetValueEQLow value", CSTGParamsOwner::sValueGetterTemp.value, -380982516L);
	check_eq("CSTGHDRTrack::GetValueEQLow displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -380982516L);
	s->GetValueEQMid(ctx);
	check_eq("CSTGHDRTrack::GetValueEQMid value", CSTGParamsOwner::sValueGetterTemp.value, 1707485064L);
	check_eq("CSTGHDRTrack::GetValueEQMid displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1707485064L);
	s->GetValueEQMidFreq(ctx);
	check_eq("CSTGHDRTrack::GetValueEQMidFreq value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGHDRTrack::GetValueEQMidFreq displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);
	s->GetValueEQHigh(ctx);
	check_eq("CSTGHDRTrack::GetValueEQHigh value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGHDRTrack::GetValueEQHigh displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetValuePan(ctx);
	check_eq("CSTGHDRTrack::GetValuePan value", CSTGParamsOwner::sValueGetterTemp.value, -650470404L);
	check_eq("CSTGHDRTrack::GetValuePan displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -650470404L);
	s->GetValueSend1Level(ctx);
	check_eq("CSTGHDRTrack::GetValueSend1Level value", CSTGParamsOwner::sValueGetterTemp.value, 1437996920L);
	check_eq("CSTGHDRTrack::GetValueSend1Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1437996920L);
	s->GetValueSend2Level(ctx);
	check_eq("CSTGHDRTrack::GetValueSend2Level value", CSTGParamsOwner::sValueGetterTemp.value, -785214476L);
	check_eq("CSTGHDRTrack::GetValueSend2Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -785214476L);
	s->GetValueLevel(ctx);
	check_eq("CSTGHDRTrack::GetValueLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1303252848L);
	check_eq("CSTGHDRTrack::GetValueLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1303252848L);
	s->GetValueEQBypass(ctx);
	check_eq("CSTGHDRTrack::GetValueEQBypass value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetValueMute(ctx);
	check_eq("CSTGHDRTrack::GetValueMute value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetValueSolo(ctx);
	check_eq("CSTGHDRTrack::GetValueSolo value", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
