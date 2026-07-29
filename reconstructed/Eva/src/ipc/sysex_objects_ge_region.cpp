// SPDX-License-Identifier: GPL-2.0
#include "sysex_objects_ge_region.h"
#include "sysex_control_objects.h"
#include "storage_converter_ext_stubs.h"

// ---- CSysExGlobal -----------------------------------------------------
unsigned int CSysExGlobal::GetStorageId() const { return 5; }
unsigned int CSysExGlobal::GetNumBanks() const { return 1; }
unsigned int CSysExGlobal::HasDigests() const { return 1; }
unsigned int CSysExGlobal::GetVersion() const { return 2; }
unsigned int CSysExGlobal::GetObjectSize() const { return 0x6084; }
unsigned int CSysExGlobal::GetObjectSizeForExport() const { return 0x6084; }
int CSysExGlobal::GetSysExBankId(int) const { return 0; }
unsigned int CSysExGlobal::GetNumOfObject(int) const { return 1; }
unsigned int CSysExGlobal::GetNumObjectsForDigest(int) const { return 1; }
void CSysExGlobal::GetObjectPointer(int, int) const
{
	CStorage::GetInstance();
	CStorage().GetGlobal();
}

// ---- CSysExKarmaGE -----------------------------------------------------
unsigned int CSysExKarmaGE::GetStorageId() const { return 6; }
unsigned int CSysExKarmaGE::GetNumBanks() const { return 0xc; }
unsigned int CSysExKarmaGE::HasDigests() const { return 1; }
unsigned int CSysExKarmaGE::GetVersion() const { return 0; }
unsigned int CSysExKarmaGE::GetObjectSize() const { return 0x9ec; }
unsigned int CSysExKarmaGE::GetObjectSizeForExport() const { return 0x9f0; }
int CSysExKarmaGE::GetSysExBankId(int param1) const { return param1; }
unsigned int CSysExKarmaGE::GetNumOfObject(int) const { return 0x80; }
int CSysExKarmaGE::GetObjectPointer(int param1, int param2) const
{
	return CKGUtil::GetUserGE(param1) + param2 * 0x9ec;
}

// ---- CSysExGETemplate ----------------------------------------------------
unsigned int CSysExGETemplate::GetStorageId() const { return 7; }
unsigned int CSysExGETemplate::GetNumBanks() const { return 4; }
unsigned int CSysExGETemplate::HasDigests() const { return 1; }
unsigned int CSysExGETemplate::GetVersion() const { return 0; }
unsigned int CSysExGETemplate::GetObjectSize() const { return 0x10580; }
unsigned int CSysExGETemplate::GetObjectSizeForExport() const { return 0x10584; }
int CSysExGETemplate::GetSysExBankId(int param1) const { return param1; }
unsigned int CSysExGETemplate::GetNumOfObject(int) const { return 1; }
int CSysExGETemplate::GetObjectPointer(int param1, int param2) const
{
	return CKGUtil::GetUserKarmaTemplate(param1) + param2 * 0x10580;
}

// ---- CSysExRegion --------------------------------------------------------
unsigned int CSysExRegion::GetStorageId() const { return 0xb; }
unsigned int CSysExRegion::GetNumBanks() const { return 1; }
unsigned int CSysExRegion::HasDigests() const { return 1; }
unsigned int CSysExRegion::GetVersion() const { return 1; }
unsigned int CSysExRegion::GetObjectSize() const { return 0x130; }
unsigned int CSysExRegion::GetObjectSizeForExport() const { return 0x130; }
int CSysExRegion::GetSysExBankId(int) const { return 0; }
unsigned int CSysExRegion::GetNumOfObject(int) const { return 10000; }

int CSysExRegion::GetObjectPointer(int, int param2) const
{
	unsigned int idx = ((unsigned int)param2 < 10000) ? (unsigned int)param2 : 0;
	return (int)(idx * 0x130 + CKGUtil::sm_poRegionHolder);
}

/*
 * Real ground truth ignores BOTH explicit arguments entirely -- always
 * sums across the full [0,10000) region range regardless of the
 * requested [param1,param2] slice, transcribed verbatim (not "fixed"
 * to actually respect the range).
 */
int CSysExRegion::GetTotalSizeForExport(int, int) const
{
	int total = 0;
	for (unsigned int i = 0; i < 10000; i++) {
		unsigned char active = *(unsigned char *)(CKGUtil::sm_poRegionHolder + 0x18 + (unsigned long)i * 0x130);
		if (active != 0)
			total += 0x130;
	}
	return total;
}
