// SPDX-License-Identifier: GPL-2.0
#include "sysex_control_objects.h"

unsigned long CKGUtil::sm_poKGUIInfo;
unsigned long CKGUtil::sm_poSeqDataManager;

int CSysExKarmaGEInfo::GetObjectPointer(int, int) const
{
	return (int)(CKGUtil::sm_poKGUIInfo + 0x3c90);
}
unsigned int CSysExKarmaGEInfo::GetStorageId() const { return 0x26; }
unsigned int CSysExKarmaGEInfo::GetNumBanks() const { return 1; }
unsigned int CSysExKarmaGEInfo::GetVersion() const { return 0; }
unsigned int CSysExKarmaGEInfo::GetObjectSize() const { return 0x7a0; }
unsigned int CSysExKarmaGEInfo::GetObjectSizeForExport() const { return 0x7a0; }
int CSysExKarmaGEInfo::GetSysExBankId(int param1) const { return param1; }
unsigned int CSysExKarmaGEInfo::GetNumOfObject(int) const { return 0; }
unsigned int CSysExKarmaGEInfo::GetTotalSizeForExport(int, int) const { return 0; }

void CSysExSongControl::GetObjectPointer(int, int param2) const
{
	((CSeqDataManager *)CKGUtil::sm_poSeqDataManager)->GetRegistoredSong(param2);
}
unsigned int CSysExSongControl::GetStorageId() const { return 0xc; }
unsigned int CSysExSongControl::GetVersion() const { return 0; }
unsigned int CSysExSongControl::GetObjectSize() const { return 0x1490; }
unsigned int CSysExSongControl::GetObjectSizeForExport() const { return 0x1490; }
