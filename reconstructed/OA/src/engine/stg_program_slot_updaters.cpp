// SPDX-License-Identifier: GPL-2.0
/*
 * stg_program_slot_updaters.cpp  -  CSTGProgramSlot's round-58 57-method
 * batch (solo). See include/oa_global.h's own header comment above this
 * batch's declarations for the full derivation -- every offset here is a
 * cross-check against round 57's own plain accessors or this round's own
 * UpdateXxx siblings, not a fresh derivation.
 */
#include "oa_global.h"

extern "C" int STGAPIOutToPhysBusId[16];
extern "C" int STGAPIOutToBusType[16];

// kKeyZoneSlopeTable/kVelZoneSlopeTable: real .rodata conversion tables
// indexed by a raw STGConvertedParam::value; content and true size not
// independently confirmed, sized to a safe, generous upper bound.
extern "C" unsigned char kKeyZoneSlopeTable[32] = {0};
extern "C" unsigned char kVelZoneSlopeTable[32] = {0};

CSTGProgramSlot::~CSTGProgramSlot()
{
	unsigned char *base = (unsigned char *)this;
	base[0] = base[1] = base[2] = base[3] = 0;
	base[0x7f] = base[0x80] = base[0x81] = base[0x82] = 0;
}

bool CSTGProgramSlot::OverridesProgramScale() const
{
	const unsigned char *base = (const unsigned char *)this;
	return (((base[0x43] >> 2) & 1) ^ 1) != 0;
}

unsigned int CSTGProgramSlot::GetDKitBus(unsigned int index) const
{
	const unsigned char *base = (const unsigned char *)this;
	return STGAPIOutToPhysBusId[(signed char)base[99 + index]];
}

unsigned int CSTGProgramSlot::GetDKitBusType(unsigned int index) const
{
	const unsigned char *base = (const unsigned char *)this;
	return STGAPIOutToBusType[(signed char)base[99 + index]];
}

void CSTGProgramSlot::UpdateBankSelectEx2MSB(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0xe] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateBankSelectEx2LSB(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0xf] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateOscOnOffCtrl(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x13] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateDelayBaseNote(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x27] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateDelayTimes(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x25] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateKeyZoneBottom(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x28] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateKeyZoneTop(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2a] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateVelZoneBottom(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2e] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateVelZoneTop(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2c] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateKeySync(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x3d] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateSwingPercent(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	*(int *)(((unsigned char *)this) + 0x3f) = val.value;
}

void CSTGProgramSlot::UpdateQuantizeTrigger(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x3e] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateVJSAssign(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x30] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateChordSource(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x16] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateIFXDrumkitPatch(CSTGProgramSlotMessageContext &ctx, STGConvertedParam &val)
{
	((unsigned char *)this)[99 + ctx.ifxSlotIndex] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateInputSource(CSTGProgramSlotMessageContext &ctx, STGConvertedParam &val)
{
	((unsigned char *)this)[0x5c + ctx.inputChannelIndex] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateInputChannelSelect(CSTGProgramSlotMessageContext &ctx, STGConvertedParam &val)
{
	((unsigned char *)this)[0x5e + ctx.inputChannelIndex] = (unsigned char)val.value;
}

void CSTGProgramSlot::UpdateKeyZoneBottomSlope(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x29] = kKeyZoneSlopeTable[val.value];
}

void CSTGProgramSlot::UpdateKeyZoneTopSlope(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2b] = kKeyZoneSlopeTable[val.value];
}

void CSTGProgramSlot::UpdateVelZoneBottomSlope(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2f] = kVelZoneSlopeTable[val.value];
}

void CSTGProgramSlot::UpdateVelZoneTopSlope(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x2d] = kVelZoneSlopeTable[val.value];
}

static STGConvertedParam &SetValueGetterTemp(int v)
{
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueOutputBus(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x60]);
}

STGConvertedParam &CSTGProgramSlot::GetValueFXControlBus(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x61]);
}

STGConvertedParam &CSTGProgramSlot::GetValueHDRBus(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x62]);
}

STGConvertedParam &CSTGProgramSlot::GetValueBankSelect(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[9]);
}

STGConvertedParam &CSTGProgramSlot::GetValueProgramId(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0xb]);
}

STGConvertedParam &CSTGProgramSlot::GetValueMIDIChannel(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x10]);
}

STGConvertedParam &CSTGProgramSlot::GetValueTrackStatus(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0xd]);
}

STGConvertedParam &CSTGProgramSlot::GetValueBankSelectEx2MSB(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0xe]);
}

STGConvertedParam &CSTGProgramSlot::GetValueBankSelectEx2LSB(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0xf]);
}

STGConvertedParam &CSTGProgramSlot::GetValueForceOscMode(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x11]);
}

STGConvertedParam &CSTGProgramSlot::GetValueOscSelect(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x12]);
}

STGConvertedParam &CSTGProgramSlot::GetValueOscOnOffCtrl(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x13]);
}

STGConvertedParam &CSTGProgramSlot::GetValuePortamentoTime(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x18]);
}

STGConvertedParam &CSTGProgramSlot::GetValueTranspose(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x17]);
}

STGConvertedParam &CSTGProgramSlot::GetValueDelayType(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x26]);
}

STGConvertedParam &CSTGProgramSlot::GetValueDelayBaseNote(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x27]);
}

STGConvertedParam &CSTGProgramSlot::GetValueDelayTimes(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x25]);
}

STGConvertedParam &CSTGProgramSlot::GetValueKeyZoneBottom(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x28]);
}

STGConvertedParam &CSTGProgramSlot::GetValueKeyZoneBottomSlope(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x29]);
}

STGConvertedParam &CSTGProgramSlot::GetValueKeyZoneTop(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2a]);
}

STGConvertedParam &CSTGProgramSlot::GetValueKeyZoneTopSlope(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2b]);
}

STGConvertedParam &CSTGProgramSlot::GetValueVelZoneBottom(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2e]);
}

STGConvertedParam &CSTGProgramSlot::GetValueVelZoneBottomSlope(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2f]);
}

STGConvertedParam &CSTGProgramSlot::GetValueVelZoneTop(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2c]);
}

STGConvertedParam &CSTGProgramSlot::GetValueVelZoneTopSlope(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x2d]);
}

STGConvertedParam &CSTGProgramSlot::GetValueKeySync(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x3d]);
}

STGConvertedParam &CSTGProgramSlot::GetValueQuantizeTrigger(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x3e]);
}

STGConvertedParam &CSTGProgramSlot::GetValueVJSAssign(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((signed char)((unsigned char *)&ctx)[0x30]);
}

STGConvertedParam &CSTGProgramSlot::GetValueMaxNumNotes(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x14]);
}

STGConvertedParam &CSTGProgramSlot::GetChordModeValue(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x15]);
}

STGConvertedParam &CSTGProgramSlot::GetChordSourceValue(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x16]);
}

STGConvertedParam &CSTGProgramSlot::GetChordSW()
{
	return SetValueGetterTemp(1);
}

STGConvertedParam &CSTGProgramSlot::GetValueUseDrumkitBusSettings(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x44] & 1);
}

/* Round 59 batch (43 methods) -- see header for class A/B/C/D pattern notes. */

STGConvertedParam &CSTGProgramSlot::GetValueMute(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x43] & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValuePriority(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 1) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueUseProgramScale(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 2) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableProgramChange(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 1) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableAfterTouch(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 2) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableDamper(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 3) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnablePortamentoSW(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 4) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableJSXAsAMS(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 5) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableJSY(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 6) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableJSmY(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x44] >> 7) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableRibbon(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x45] & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableKnob1(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp(((unsigned char *)&ctx)[0x47] & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableKnob8(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x47] >> 7) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableSW1(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 5) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableSW2(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 6) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableFootPedal(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 4) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableFootSwitch(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 3) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableOtherCC(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 1) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEnableKarmaWaveformSysex(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x45] >> 2) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueProgVectorVolume(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 3) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueProgVectorCC(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 4) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueCombiVectorCC(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 5) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEQAutoLoadProgram(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 6) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueEQBypass(CSTGProgramSlotMessageContext &ctx) const
{
	return SetValueGetterTemp((((unsigned char *)&ctx)[0x43] >> 7) & 1);
}

STGConvertedParam &CSTGProgramSlot::GetValueTrackPan(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x6f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueTrackLevel(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x73);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueTrackSend1Level(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x77);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueTrackSend2Level(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x7b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValuePitchBendRange(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x19);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueDetune(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x1d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueSwingPercent(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x3f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueVolumeCenter(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x31);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueEQTrim(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x48);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueEQLowGain(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x4c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueEQMidFreq(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x50);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueEQMidGain(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x54);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueEQHighGain(CSTGProgramSlotMessageContext &ctx) const
{
	int v = *(int *)(((unsigned char *)&ctx) + 0x58);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGProgramSlot::GetValueInputSource(CSTGProgramSlotMessageContext &ctx) const
{
	const unsigned char *base = (const unsigned char *)this;
	return SetValueGetterTemp((signed char)base[0x5c + *(int *)(((unsigned char *)&ctx) + 0x18)]);
}

STGConvertedParam &CSTGProgramSlot::GetValueInputChannelSelect(CSTGProgramSlotMessageContext &ctx) const
{
	const unsigned char *base = (const unsigned char *)this;
	return SetValueGetterTemp((signed char)base[0x5e + *(int *)(((unsigned char *)&ctx) + 0x18)]);
}

STGConvertedParam &CSTGProgramSlot::GetValueIFXDrumkitPatch(CSTGProgramSlotMessageContext &ctx) const
{
	const unsigned char *base = (const unsigned char *)this;
	return SetValueGetterTemp((signed char)base[0x63 + *(int *)(((unsigned char *)&ctx) + 0x4)]);
}

void CSTGProgramSlot::UpdateIgnoreSetListTranspose(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[0x46] = (unsigned char)((base[0x46] & 0xfe) | (val.value != 0));
}

void CSTGProgramSlot::UpdateEnableRibbon(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[0x45] = (unsigned char)((base[0x45] & 0xfe) | (val.value != 0));
}

void CSTGProgramSlot::UpdateUseDrumkitBusSettings(CSTGProgramSlotMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[0x44] = (unsigned char)((base[0x44] & 0xfe) | (val.value != 0));
}
