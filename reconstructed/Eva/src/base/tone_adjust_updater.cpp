// SPDX-License-Identifier: GPL-2.0
/*
 * tone_adjust_updater.cpp  -  CToneAdjustUpdater method bodies (round 48,
 * solo). See include/tone_adjust_updater.h for the full derivation.
 */
#include "tone_adjust_updater.h"

static short s_swProgSwitchValueBuffer[64] = {0};

/* Real .rodata lookup tables, content not independently confirmed --
 * see header comment. Sized to safely cover every real index this
 * round's own callers bounds-check against. */
static unsigned short CSWTCH_165[0x50] = {0};
static unsigned short CSWTCH_168[0x25] = {0};
static unsigned short CSWTCH_171[0x2c] = {0};
static unsigned short CSWTCH_174[0x3f] = {0};
static unsigned short CSWTCH_177[0xe] = {0};
static unsigned char CSWTCH_184[0x25] = {0};
static unsigned char CSWTCH_187[0x1f] = {0};

CToneAdjustUpdater::CToneAdjustUpdater() {}
CToneAdjustUpdater::~CToneAdjustUpdater() {}

short CToneAdjustUpdater::GetProgSwitchValueBuffer(int index)
{
	return s_swProgSwitchValueBuffer[index];
}

void CToneAdjustUpdater::SetProgSwitchValueBuffer(int index, short value)
{
	s_swProgSwitchValueBuffer[index] = value;
}

int CToneAdjustUpdater::ConvertDKitNumToBank(int dkitNum)
{
	if (dkitNum <= 0x27)
		return 0;
	return ((dkitNum - 0x28) >> 4) + 1;
}

int CToneAdjustUpdater::ConvertWSeqNumToBank(int wseqNum)
{
	if (wseqNum <= 0x95)
		return 0;
	return ((wseqNum - 0x96) >> 5) + 1;
}

unsigned short CToneAdjustUpdater::GetFormatterForPCM(int paramIndex)
{
	if ((unsigned int)paramIndex < 0x50)
		return CSWTCH_165[paramIndex];
	return 0x68;
}

unsigned short CToneAdjustUpdater::GetFormatterForCommon(int paramIndex)
{
	if ((unsigned int)paramIndex < 0x25)
		return CSWTCH_168[paramIndex];
	return 0x68;
}

unsigned short CToneAdjustUpdater::GetFormatterForPCMStoredValueForProg(int paramIndex)
{
	if ((unsigned int)(paramIndex - 0x11) < 0x2c)
		return CSWTCH_171[paramIndex - 0x11];
	return 0xd7;
}

unsigned short CToneAdjustUpdater::GetFormatterForPCMStoredValueForTimbre(int paramIndex)
{
	if ((unsigned int)(paramIndex - 0x11) < 0x3f)
		return CSWTCH_174[paramIndex - 0x11];
	return 0xd7;
}

unsigned short CToneAdjustUpdater::GetFormatterForCommonStoredValue(int paramIndex)
{
	if ((unsigned int)(paramIndex - 0x11) < 0xe)
		return CSWTCH_177[paramIndex - 0x11];
	return 0xd7;
}

unsigned char CToneAdjustUpdater::GetAssignTypeForPCM(unsigned char index)
{
	if (index < 0x23)
		return CSWTCH_184[index];
	if ((unsigned char)(index - 0x23) < 0x1f)
		return CSWTCH_187[(unsigned char)(index - 0x23)];
	return 1;
}

unsigned char CToneAdjustUpdater::GetAssignTypeForCommon(unsigned char index)
{
	if (index < 0x25)
		return CSWTCH_184[index];
	return 2;
}

bool CToneAdjustUpdater::IsAssignAvailable(int algorithm, int arg2)
{
	if (algorithm != 1 && algorithm != 10 && algorithm != 0)
		return true;
	return arg2 != 0;
}
