/*
 * test_stg_program_slot_updaters.cpp  -  host-side known-answer test for
 * CSTGProgramSlot's round-58 57-method batch (solo, 2026-07-29). See
 * include/oa_global.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_global.h"

extern "C" int STGAPIOutToPhysBusId[16] = {0};
extern "C" int STGAPIOutToBusType[16] = {0};
extern "C" unsigned char kKeyZoneSlopeTable[32];
extern "C" unsigned char kVelZoneSlopeTable[32];

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static STGConvertedParam MakeParam(int v)
{
	STGConvertedParam p;
	memset(&p, 0, sizeof(p));
	p.value = v;
	return p;
}
#define P(v) (*({ static STGConvertedParam _p; _p = MakeParam(v); &_p; }))

int main()
{
	unsigned char buf[0x200];
	memset(buf, 0, sizeof(buf));
	CSTGProgramSlot *slot = reinterpret_cast<CSTGProgramSlot *>(buf);
	CSTGProgramSlotMessageContext ctx;
	memset(&ctx, 0, sizeof(ctx));

	{
		unsigned char dtorBuf[0x90];
		memset(dtorBuf, 0xcc, sizeof(dtorBuf));
		CSTGProgramSlot *d = reinterpret_cast<CSTGProgramSlot *>(dtorBuf);
		d->~CSTGProgramSlot();
		check("dtor zeroes this+0x0..3", dtorBuf[0] == 0 && dtorBuf[3] == 0);
		check("dtor zeroes this+0x7f..0x82", dtorBuf[0x7f] == 0 && dtorBuf[0x82] == 0);
	}

	buf[0x43] = 0x00; // bit2 clear -> (0>>2&1)^1 = 1
	check("OverridesProgramScale: bit2 clear -> true", slot->OverridesProgramScale() == true);
	buf[0x43] = 0x04; // bit2 set -> (1)^1 = 0
	check("OverridesProgramScale: bit2 set -> false", slot->OverridesProgramScale() == false);
	buf[0x43] = 0;

	STGAPIOutToPhysBusId[5] = 0x1111;
	buf[99 + 2] = 5;
	check("GetDKitBus(2) indexes STGAPIOutToPhysBusId by this+99+idx", slot->GetDKitBus(2) == 0x1111u);
	STGAPIOutToBusType[5] = 0x2222;
	check("GetDKitBusType(2) indexes STGAPIOutToBusType by this+99+idx", slot->GetDKitBusType(2) == 0x2222u);

	slot->UpdateBankSelectEx2MSB(ctx, P(0x11));
	check("UpdateBankSelectEx2MSB writes +0xe", buf[0xe] == 0x11);
	slot->UpdateBankSelectEx2LSB(ctx, P(0x22));
	check("UpdateBankSelectEx2LSB writes +0xf", buf[0xf] == 0x22);
	slot->UpdateOscOnOffCtrl(ctx, P(0x33));
	check("UpdateOscOnOffCtrl writes +0x13", buf[0x13] == 0x33);
	slot->UpdateDelayBaseNote(ctx, P(0x44));
	check("UpdateDelayBaseNote writes +0x27", buf[0x27] == 0x44);
	slot->UpdateDelayTimes(ctx, P(0x55));
	check("UpdateDelayTimes writes +0x25", buf[0x25] == 0x55);
	slot->UpdateKeyZoneBottom(ctx, P(0x66));
	check("UpdateKeyZoneBottom writes +0x28", buf[0x28] == 0x66);
	slot->UpdateKeyZoneTop(ctx, P(0x77));
	check("UpdateKeyZoneTop writes +0x2a", buf[0x2a] == 0x77);
	slot->UpdateVelZoneBottom(ctx, P(0x88));
	check("UpdateVelZoneBottom writes +0x2e", buf[0x2e] == (unsigned char)0x88);
	slot->UpdateVelZoneTop(ctx, P(0x99));
	check("UpdateVelZoneTop writes +0x2c", buf[0x2c] == (unsigned char)0x99);
	slot->UpdateKeySync(ctx, P(0xa));
	check("UpdateKeySync writes +0x3d", buf[0x3d] == 0xa);
	slot->UpdateSwingPercent(ctx, P(0x11223344));
	check("UpdateSwingPercent writes 4-byte int at +0x3f",
	      *(int *)(buf + 0x3f) == 0x11223344);
	slot->UpdateQuantizeTrigger(ctx, P(0xb));
	check("UpdateQuantizeTrigger writes +0x3e", buf[0x3e] == 0xb);
	slot->UpdateVJSAssign(ctx, P(0xc));
	check("UpdateVJSAssign writes +0x30", buf[0x30] == 0xc);
	slot->UpdateChordSource(ctx, P(0xd));
	check("UpdateChordSource writes +0x16", buf[0x16] == 0xd);

	ctx.ifxSlotIndex = 2;
	slot->UpdateIFXDrumkitPatch(ctx, P(0xe));
	check("UpdateIFXDrumkitPatch writes this+99+ctx.ifxSlotIndex", buf[99 + 2] == 0xe);

	ctx.inputChannelIndex = 3;
	slot->UpdateInputSource(ctx, P(0xf));
	check("UpdateInputSource writes this+0x5c+ctx.inputChannelIndex", buf[0x5c + 3] == 0xf);
	slot->UpdateInputChannelSelect(ctx, P(0x10));
	check("UpdateInputChannelSelect writes this+0x5e+ctx.inputChannelIndex", buf[0x5e + 3] == 0x10);

	kKeyZoneSlopeTable[4] = 0x99;
	slot->UpdateKeyZoneBottomSlope(ctx, P(4));
	check("UpdateKeyZoneBottomSlope writes kKeyZoneSlopeTable[val] at +0x29",
	      buf[0x29] == (unsigned char)0x99);
	kKeyZoneSlopeTable[5] = 0xaa;
	slot->UpdateKeyZoneTopSlope(ctx, P(5));
	check("UpdateKeyZoneTopSlope writes kKeyZoneSlopeTable[val] at +0x2b",
	      buf[0x2b] == (unsigned char)0xaa);
	kVelZoneSlopeTable[6] = 0xbb;
	slot->UpdateVelZoneBottomSlope(ctx, P(6));
	check("UpdateVelZoneBottomSlope writes kVelZoneSlopeTable[val] at +0x2f",
	      buf[0x2f] == (unsigned char)0xbb);
	kVelZoneSlopeTable[7] = 0xcc;
	slot->UpdateVelZoneTopSlope(ctx, P(7));
	check("UpdateVelZoneTopSlope writes kVelZoneSlopeTable[val] at +0x2d",
	      buf[0x2d] == (unsigned char)0xcc);

	// GetValueXxx family: since ground truth reads the passed context at the
	// SAME offsets as CSTGProgramSlot's own layout, pass `ctx2` shaped as a
	// CSTGProgramSlot-sized buffer reinterpreted as the message context.
	unsigned char ctxBuf[0x100];
	memset(ctxBuf, 0, sizeof(ctxBuf));
	CSTGProgramSlotMessageContext *ctx2 = reinterpret_cast<CSTGProgramSlotMessageContext *>(ctxBuf);

	ctxBuf[0x60] = (unsigned char)-5;
	check("GetValueOutputBus reads signed +0x60", slot->GetValueOutputBus(*ctx2).value == -5);
	ctxBuf[0x61] = (unsigned char)-6;
	check("GetValueFXControlBus reads signed +0x61", slot->GetValueFXControlBus(*ctx2).value == -6);
	ctxBuf[0x62] = (unsigned char)-7;
	check("GetValueHDRBus reads signed +0x62", slot->GetValueHDRBus(*ctx2).value == -7);
	ctxBuf[9] = (unsigned char)-1;
	check("GetValueBankSelect reads signed +0x9", slot->GetValueBankSelect(*ctx2).value == -1);
	ctxBuf[0xb] = 0x42;
	check("GetValueProgramId reads unsigned +0xb", slot->GetValueProgramId(*ctx2).value == 0x42);
	ctxBuf[0x10] = 0x7;
	check("GetValueMIDIChannel reads unsigned +0x10", slot->GetValueMIDIChannel(*ctx2).value == 0x7);
	ctxBuf[0xd] = (unsigned char)-2;
	check("GetValueTrackStatus reads signed +0xd", slot->GetValueTrackStatus(*ctx2).value == -2);
	ctxBuf[0xe] = 0x33;
	check("GetValueBankSelectEx2MSB reads unsigned +0xe", slot->GetValueBankSelectEx2MSB(*ctx2).value == 0x33);
	ctxBuf[0xf] = 0x34;
	check("GetValueBankSelectEx2LSB reads unsigned +0xf", slot->GetValueBankSelectEx2LSB(*ctx2).value == 0x34);
	ctxBuf[0x11] = (unsigned char)-3;
	check("GetValueForceOscMode reads signed +0x11", slot->GetValueForceOscMode(*ctx2).value == -3);
	ctxBuf[0x12] = (unsigned char)-4;
	check("GetValueOscSelect reads signed +0x12", slot->GetValueOscSelect(*ctx2).value == -4);
	ctxBuf[0x13] = 0x9;
	check("GetValueOscOnOffCtrl reads signed +0x13", slot->GetValueOscOnOffCtrl(*ctx2).value == 0x9);
	ctxBuf[0x18] = 0xa;
	check("GetValuePortamentoTime reads signed +0x18", slot->GetValuePortamentoTime(*ctx2).value == 0xa);
	ctxBuf[0x17] = 0xb;
	check("GetValueTranspose reads signed +0x17", slot->GetValueTranspose(*ctx2).value == 0xb);
	ctxBuf[0x26] = 0xc;
	check("GetValueDelayType reads signed +0x26", slot->GetValueDelayType(*ctx2).value == 0xc);
	ctxBuf[0x27] = 0xd;
	check("GetValueDelayBaseNote reads signed +0x27", slot->GetValueDelayBaseNote(*ctx2).value == 0xd);
	ctxBuf[0x25] = 0xe;
	check("GetValueDelayTimes reads unsigned +0x25", slot->GetValueDelayTimes(*ctx2).value == 0xe);
	ctxBuf[0x28] = 0xf;
	check("GetValueKeyZoneBottom reads unsigned +0x28", slot->GetValueKeyZoneBottom(*ctx2).value == 0xf);
	ctxBuf[0x29] = 0x10;
	check("GetValueKeyZoneBottomSlope reads unsigned +0x29", slot->GetValueKeyZoneBottomSlope(*ctx2).value == 0x10);
	ctxBuf[0x2a] = 0x11;
	check("GetValueKeyZoneTop reads unsigned +0x2a", slot->GetValueKeyZoneTop(*ctx2).value == 0x11);
	ctxBuf[0x2b] = 0x12;
	check("GetValueKeyZoneTopSlope reads unsigned +0x2b", slot->GetValueKeyZoneTopSlope(*ctx2).value == 0x12);
	ctxBuf[0x2e] = 0x13;
	check("GetValueVelZoneBottom reads unsigned +0x2e", slot->GetValueVelZoneBottom(*ctx2).value == 0x13);
	ctxBuf[0x2f] = 0x14;
	check("GetValueVelZoneBottomSlope reads unsigned +0x2f", slot->GetValueVelZoneBottomSlope(*ctx2).value == 0x14);
	ctxBuf[0x2c] = 0x15;
	check("GetValueVelZoneTop reads unsigned +0x2c", slot->GetValueVelZoneTop(*ctx2).value == 0x15);
	ctxBuf[0x2d] = 0x16;
	check("GetValueVelZoneTopSlope reads unsigned +0x2d", slot->GetValueVelZoneTopSlope(*ctx2).value == 0x16);
	ctxBuf[0x3d] = 0x17;
	check("GetValueKeySync reads unsigned +0x3d", slot->GetValueKeySync(*ctx2).value == 0x17);
	ctxBuf[0x3e] = 0x18;
	check("GetValueQuantizeTrigger reads unsigned +0x3e", slot->GetValueQuantizeTrigger(*ctx2).value == 0x18);
	ctxBuf[0x30] = (unsigned char)-9;
	check("GetValueVJSAssign reads signed +0x30", slot->GetValueVJSAssign(*ctx2).value == -9);
	ctxBuf[0x14] = 0x19;
	check("GetValueMaxNumNotes reads unsigned +0x14", slot->GetValueMaxNumNotes(*ctx2).value == 0x19);
	ctxBuf[0x15] = 0x1a;
	check("GetChordModeValue reads unsigned +0x15", slot->GetChordModeValue(*ctx2).value == 0x1a);
	ctxBuf[0x16] = 0x1b;
	check("GetChordSourceValue reads unsigned +0x16", slot->GetChordSourceValue(*ctx2).value == 0x1b);
	check("GetChordSW() == const 1", CSTGProgramSlot::GetChordSW().value == 1);
	ctxBuf[0x44] = 0x3; // bit0 set
	check("GetValueUseDrumkitBusSettings reads bit0 of +0x44", slot->GetValueUseDrumkitBusSettings(*ctx2).value == 1);
	ctxBuf[0x44] = 0x2; // bit0 clear
	check("GetValueUseDrumkitBusSettings bit0 clear -> 0", slot->GetValueUseDrumkitBusSettings(*ctx2).value == 0);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
