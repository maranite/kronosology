/*
 * test_stg_program_slot_getters.cpp  -  host-side known-answer test for
 * CSTGProgramSlot's round-57 27-method batch (solo, 2026-07-29). See
 * include/oa_global.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_global.h"

extern "C" unsigned char STGProgramSlotParams[4] = {0};
extern "C" unsigned char sMessageHandlers[4] = {0};
extern "C" unsigned char sValueGetters[4] = {0};
extern "C" int STGAPIOutToPhysBusId[16] = {0};
extern "C" int STGAPIOutToBusType[16] = {0};
extern "C" int STGAPIFXCtrlToWritePhysBusId[16] = {0};
extern "C" int STGAPIHDRPhysBusIds[16] = {0};
extern "C" int STGAPIHDRBusTypes[16] = {0};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	check("GetNumParams() == 0x52", CSTGProgramSlot::GetNumParams() == 0x52);
	check("GetParamDescriptors() == STGProgramSlotParams",
	      CSTGProgramSlot::GetParamDescriptors() == (const void *)STGProgramSlotParams);
	check("GetMessageHandlers() == sMessageHandlers",
	      CSTGProgramSlot::GetMessageHandlers() == (const void *)sMessageHandlers);
	check("GetValueGetters() == sValueGetters",
	      CSTGProgramSlot::GetValueGetters() == (const void *)sValueGetters);
	check("HasToneAdjust() == true", CSTGProgramSlot::HasToneAdjust() == true);
	check("ShouldUseSlotEQSettings() == true", CSTGProgramSlot::ShouldUseSlotEQSettings() == true);

	unsigned char buf[0x200];
	memset(buf, 0, sizeof(buf));
	CSTGProgramSlot *slot = reinterpret_cast<CSTGProgramSlot *>(buf);

	check("AccessToneAdjust() == this+0x7f", slot->AccessToneAdjust() == buf + 0x7f);

	*(float *)(buf + 0x48) = 1.5f;
	check("GetEQTrim() reads float at +0x48", slot->GetEQTrim() == 1.5f);
	*(float *)(buf + 0x4c) = -2.25f;
	check("GetEQLowGain() reads float at +0x4c", slot->GetEQLowGain() == -2.25f);
	*(float *)(buf + 0x50) = 3.0f;
	check("GetEQMidFreq() reads float at +0x50", slot->GetEQMidFreq() == 3.0f);
	*(float *)(buf + 0x54) = 4.0f;
	check("GetEQMidGain() reads float at +0x54", slot->GetEQMidGain() == 4.0f);
	*(float *)(buf + 0x58) = 5.0f;
	check("GetEQHighGain() reads float at +0x58", slot->GetEQHighGain() == 5.0f);
	*(float *)(buf + 0x1d) = -1.0f;
	check("GetDetune() reads float at +0x1d", slot->GetDetune() == -1.0f);

	buf[9] = 0x42;
	check("GetAliasBankSelect() reads +0x9", slot->GetAliasBankSelect() == 0x42);
	buf[0xb] = 0x17;
	check("GetAliasProgramId() reads +0xb", slot->GetAliasProgramId() == 0x17);
	buf[4] = 0x9;
	check("GetMeterIndex() reads +0x4", slot->GetMeterIndex() == 0x9);

	*(float *)(buf + 0x77 + 3 * 4) = 7.5f;
	check("GetSendLevel(3) reads +0x77+3*4", slot->GetSendLevel(3) == 7.5f);

	buf[0x5e + 2] = 0x55;
	check("GetInputChannelSelect(2) reads +0x5e+2", slot->GetInputChannelSelect(2) == 0x55);

	buf[0x44] = 0x1;
	check("GetUseDrumkitBusSettings() bit0 of +0x44 set", slot->GetUseDrumkitBusSettings() == true);
	buf[0x44] = 0x0;
	check("GetUseDrumkitBusSettings() bit0 of +0x44 clear", slot->GetUseDrumkitBusSettings() == false);

	buf[0x43] = 0x80;
	check("GetEQBypass() bit7 of +0x43 set", slot->GetEQBypass() == true);
	buf[0x43] = 0x7f;
	check("GetEQBypass() bit7 of +0x43 clear", slot->GetEQBypass() == false);

	buf[0x16] = 0x0;
	check("UsesProgramChordSource() +0x16==0 -> true", slot->UsesProgramChordSource() == true);
	buf[0x16] = 0x5;
	check("UsesProgramChordSource() +0x16!=0 -> false", slot->UsesProgramChordSource() == false);

	buf[4] = 10;
	check("GetMeterBus() == byte(+4)*2+0x52", slot->GetMeterBus() == (unsigned int)(10 * 2 + 0x52));

	STGAPIOutToPhysBusId[3] = 0x111;
	buf[0x60] = 3;
	check("GetOutputBus() indexes STGAPIOutToPhysBusId by +0x60",
	      slot->GetOutputBus() == 0x111u);

	STGAPIOutToBusType[3] = 0x222;
	check("GetOutputBusType() indexes STGAPIOutToBusType by +0x60",
	      slot->GetOutputBusType() == 0x222u);

	STGAPIFXCtrlToWritePhysBusId[5] = 0x333;
	buf[0x61] = 5;
	check("GetFXControlBus() indexes STGAPIFXCtrlToWritePhysBusId by +0x61",
	      slot->GetFXControlBus() == 0x333u);

	STGAPIHDRPhysBusIds[7] = 0x444;
	buf[0x62] = 7;
	check("GetHDRBus() indexes STGAPIHDRPhysBusIds by +0x62",
	      slot->GetHDRBus() == 0x444u);

	STGAPIHDRBusTypes[7] = 0x555;
	check("GetHDRBusType() indexes STGAPIHDRBusTypes by +0x62",
	      slot->GetHDRBusType() == 0x555u);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
