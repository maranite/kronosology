// SPDX-License-Identifier: GPL-2.0
/*
 * stg_program_slot_getters.cpp  -  CSTGProgramSlot's round-57 27-method
 * STG value-getter-family batch (solo). See include/oa_global.h's own
 * header comment above this batch's declarations (end of `struct
 * CSTGProgramSlot`) for the full derivation.
 */
#include "oa_global.h"

extern "C" unsigned char STGProgramSlotParams[];
extern "C" unsigned char sMessageHandlers[];
extern "C" unsigned char sValueGetters[];
extern "C" int STGAPIOutToPhysBusId[16];
extern "C" int STGAPIOutToBusType[16];
extern "C" int STGAPIFXCtrlToWritePhysBusId[16];
extern "C" int STGAPIHDRPhysBusIds[16];
extern "C" int STGAPIHDRBusTypes[16];

void *CSTGProgramSlot::AccessToneAdjust() const
{
	return (unsigned char *)this + 0x7f;
}

float CSTGProgramSlot::GetEQTrim() const
{
	return *(const float *)((const unsigned char *)this + 0x48);
}

float CSTGProgramSlot::GetEQLowGain() const
{
	return *(const float *)((const unsigned char *)this + 0x4c);
}

float CSTGProgramSlot::GetEQMidFreq() const
{
	return *(const float *)((const unsigned char *)this + 0x50);
}

float CSTGProgramSlot::GetEQMidGain() const
{
	return *(const float *)((const unsigned char *)this + 0x54);
}

float CSTGProgramSlot::GetEQHighGain() const
{
	return *(const float *)((const unsigned char *)this + 0x58);
}

float CSTGProgramSlot::GetDetune() const
{
	return *(const float *)((const unsigned char *)this + 0x1d);
}

unsigned char CSTGProgramSlot::GetAliasBankSelect() const
{
	return ((const unsigned char *)this)[9];
}

unsigned char CSTGProgramSlot::GetAliasProgramId() const
{
	return ((const unsigned char *)this)[0xb];
}

unsigned char CSTGProgramSlot::GetMeterIndex() const
{
	return ((const unsigned char *)this)[4];
}

float CSTGProgramSlot::GetSendLevel(unsigned int index) const
{
	return *(const float *)((const unsigned char *)this + 0x77 + index * 4);
}

unsigned int CSTGProgramSlot::GetNumParams()
{
	return 0x52;
}

const void *CSTGProgramSlot::GetParamDescriptors()
{
	return STGProgramSlotParams;
}

const void *CSTGProgramSlot::GetMessageHandlers()
{
	return sMessageHandlers;
}

const void *CSTGProgramSlot::GetValueGetters()
{
	return sValueGetters;
}

unsigned char CSTGProgramSlot::GetInputChannelSelect(unsigned int index) const
{
	return ((const unsigned char *)this)[0x5e + index];
}

bool CSTGProgramSlot::HasToneAdjust()
{
	return true;
}

bool CSTGProgramSlot::ShouldUseSlotEQSettings()
{
	return true;
}

bool CSTGProgramSlot::GetUseDrumkitBusSettings() const
{
	return (((const unsigned char *)this)[0x44] & 1) != 0;
}

bool CSTGProgramSlot::GetEQBypass() const
{
	return (((const unsigned char *)this)[0x43] >> 7) != 0;
}

bool CSTGProgramSlot::UsesProgramChordSource() const
{
	return ((const signed char *)this)[0x16] == 0;
}

unsigned int CSTGProgramSlot::GetMeterBus() const
{
	return (unsigned int)((const unsigned char *)this)[4] * 2 + 0x52;
}

unsigned int CSTGProgramSlot::GetOutputBus() const
{
	return STGAPIOutToPhysBusId[((const signed char *)this)[0x60]];
}

unsigned int CSTGProgramSlot::GetOutputBusType() const
{
	return STGAPIOutToBusType[((const signed char *)this)[0x60]];
}

unsigned int CSTGProgramSlot::GetFXControlBus() const
{
	return STGAPIFXCtrlToWritePhysBusId[((const signed char *)this)[0x61]];
}

unsigned int CSTGProgramSlot::GetHDRBus() const
{
	return STGAPIHDRPhysBusIds[((const signed char *)this)[0x62]];
}

unsigned int CSTGProgramSlot::GetHDRBusType() const
{
	return STGAPIHDRBusTypes[((const signed char *)this)[0x62]];
}
