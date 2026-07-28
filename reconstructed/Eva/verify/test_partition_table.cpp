/*
 * test_partition_table.cpp  -  host-side known-answer test for
 * CSector/CLittleEndObj/CPartitionData/CMBR/CPBR/CPBRex/CPBRFat12Fat16/
 * CPBRFat12/CPBRFat16/CPBRFat32 (src/init/partition_table.cpp). See
 * include/partition_table.h for full ground-truth provenance.
 *
 * A fake CSystemApi vtable is installed (Api+0x94 soft-assert, Api+0x9c
 * "get default" call) so the raw calls this code makes are exercised for
 * real, same convention as test_tempo.cpp/test_special_func_cc_map.cpp.
 */

#include <cstdio>
#include <cstring>

#include "partition_table.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* ---- fake CSystemApi, matching test_task.cpp's own convention: Api is a real
 * global defined in mains.cpp (CSystemApi *Api = 0;) -- just reassign it here,
 * don't redefine it. ---- */
extern CSystemApi *Api;

static int g_assertCount;
static unsigned long g_default9cValue = 0xcafebabeUL;

extern "C" void FakeApiAssert(void *, const char *, const char *file, int line)
{
	g_assertCount++;
	printf("      (soft-assert fired: %s:%d)\n", file, line);
}

extern "C" unsigned long FakeApiGetDefault9c(void *)
{
	return g_default9cValue;
}

static void *g_fakeVtable[0x9c / 4 + 1];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void InstallFakeApi()
{
	for (unsigned i = 0; i < sizeof(g_fakeVtable) / sizeof(g_fakeVtable[0]); ++i)
		g_fakeVtable[i] = 0;
	g_fakeVtable[0x94 / 4] = (void *)FakeApiAssert;
	g_fakeVtable[0x9c / 4] = (void *)FakeApiGetDefault9c;
	g_fakeApiObj.vtbl = g_fakeVtable;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main()
{
	InstallFakeApi();

	printf("CSector/CLittleEndObj/CPartitionData/CMBR/CPBR-family known-answer test\n");
	printf("==========================================================================\n");

	printf("[1] CLittleEndObj round trips\n");
	{
		unsigned char buf[8] = { 0 };
		CLittleEndObj::SetWord(buf, 0x1234);
		check("SetWord/GetWord round trip", CLittleEndObj::GetWord(buf) == 0x1234);
		check("SetWord byte order (LE)", buf[0] == 0x34 && buf[1] == 0x12);

		CLittleEndObj::SetDWord(buf, 0x89abcdefUL);
		check("SetDWord/GetDWord round trip", CLittleEndObj::GetDWord(buf) == 0x89abcdefUL);
		check("SetDWord byte order (LE)",
			buf[0] == 0xef && buf[1] == 0xcd && buf[2] == 0xab && buf[3] == 0x89);

		check("GetU3Byte", CLittleEndObj::GetU3Byte(buf) == 0x00abcdefUL);
		check("GetUInt == GetDWord", CLittleEndObj::GetUInt(buf) == CLittleEndObj::GetDWord(buf));
		check("GetUShort == GetWord", CLittleEndObj::GetUShort(buf) == CLittleEndObj::GetWord(buf));
	}

	printf("[2] CSector\n");
	{
		check("GetMinSize", CSector::GetMinSize() == 0x200);
		unsigned char buf[0x200] = { 0 };
		CSector::SetEndSectSignature(buf);
		check("SetEndSectSignature offset/value",
			CLittleEndObj::GetWord(buf + 0x1fe) == 0xaa55);
	}

	printf("[3] CPartitionData raw-buffer layer + decode round trip\n");
	{
		unsigned char entry[16] = { 0 };
		CPartitionData::SetStatus(entry, CPartitionData::eActive);
		CPartitionData::SetType(entry, CPartitionData::eFat32);
		CPartitionData::SetStartCHS(entry, 5, 3, 10);
		CPartitionData::SetEndCHS(entry, 100, 200, 50);
		CPartitionData::SetLBAStartLocation(entry, 0x1000);
		CPartitionData::SetPartitionSize(entry, 0x20000);

		check("raw GetType", CPartitionData::GetType(entry) == CPartitionData::eFat32);
		check("raw IsEmpty false", !CPartitionData::IsEmpty(entry));
		check("raw IsExtended false", !CPartitionData::IsExtended(entry));
		check("raw GetLBAStartLocation", CPartitionData::GetLBAStartLocation(entry) == 0x1000UL);

		CPartitionData pd(entry);
		check("decoded mStatus", pd.mStatus == CPartitionData::eActive);
		check("decoded mType", pd.mType == CPartitionData::eFat32);
		check("decoded mStartHead", pd.mStartHead == 3);
		check("decoded mStartSect", pd.mStartSect == 10);
		check("decoded mStartCyl", pd.mStartCyl == 5);
		check("decoded mEndHead", pd.mEndHead == 200);
		check("decoded mEndSect", pd.mEndSect == 50);
		check("decoded mEndCyl", pd.mEndCyl == 100);
		check("decoded mLBAStart", pd.mLBAStart == 0x1000UL);
		check("decoded mPartitionSize", pd.mPartitionSize == 0x20000UL);
		check("decoded IsSupported (FAT32)", pd.IsSupported());
		check("decoded IsExtended false", !pd.IsExtended());
		check("decoded IsEmpty false", !pd.IsEmpty());

		CPartitionData copy(pd);
		check("copy ctor", copy.mLBAStart == pd.mLBAStart && copy.mType == pd.mType);

		CPartitionData assigned;
		assigned = pd;
		check("operator=", assigned.mPartitionSize == pd.mPartitionSize);

		assigned.Reset();
		check("Reset zeroes", assigned.mType == 0 && assigned.mLBAStart == 0 &&
			assigned.mPartitionSize == 0);

		unsigned char rawEmpty[16] = { 0 };
		check("raw Reset() matches manual zero buffer",
			(CPartitionData::Reset(rawEmpty), CPartitionData::IsEmpty(rawEmpty)));

		check("EType-overload IsEmpty", CPartitionData::IsEmpty(CPartitionData::eEmpty));
		check("EType-overload IsExtended", CPartitionData::IsExtended(CPartitionData::eExtended));
		check("EType-overload IsSupported FAT16",
			CPartitionData::IsSupported(CPartitionData::eFat16));
		check("EType-overload IsSupported unknown type",
			!CPartitionData::IsSupported((CPartitionData::EType)0x99));
	}

	printf("[4] CPartitionData CHS<->LBA geometry math\n");
	{
		const CPartitionData::SMediaGeometry *geom = CPartitionData::GetMaxMediaGeometry();
		check("max geometry cylinders", geom->mCylinders == 1024);
		check("max geometry heads", geom->mHeads == 255);
		check("max geometry sectors", geom->mSectorsPerTrack == 63);

		/* classic CHS->LBA: LBA = (C*HPC + H)*SPT + (S-1) */
		int lba = CPartitionData::CHStoLBA(2, 3, 5, *geom);
		check("CHStoLBA formula", lba == (int)((2 * 255 + 3) * 63 + 4));

		unsigned short cyl, head, sect;
		CPartitionData::LBAtoCHS((unsigned int)lba, &cyl, &head, &sect, *geom);
		check("LBAtoCHS round trip (cyl)", cyl == 2);
		check("LBAtoCHS round trip (head)", head == 3);
		check("LBAtoCHS round trip (sect)", sect == 5);

		g_assertCount = 0;
		CPartitionData::CHStoLBA((unsigned short)(geom->mCylinders + 1), 0, 1, *geom);
		check("CHStoLBA out-of-range cyl fires soft-assert", g_assertCount == 1);

		check("GetSectCeilLBA aligns up",
			CPartitionData::GetSectCeilLBA(100, 64, *geom) == 128);
		check("GetSectCeilLBA already aligned",
			CPartitionData::GetSectCeilLBA(128, 64, *geom) == 128);

		unsigned short c2 = 0, h2 = 0, s2 = 70; /* sect beyond SPT */
		CPartitionData::AdjustCHS(&c2, &h2, &s2, *geom);
		check("AdjustCHS clamps sector to SPT", s2 == 63);

		unsigned short cm, hm, sm;
		CPartitionData::GetMaxCHS(&cm, &hm, &sm, *geom);
		check("GetMaxCHS", cm == 1023 && hm == 254 && sm == 63);
	}

	printf("[5] CMBR\n");
	{
		unsigned char sector[512];
		std::memset(sector, 0, sizeof(sector));
		CLittleEndObj::SetWord(sector + 0x1fe, 0xaa55);

		unsigned char *e0 = sector + CMBR::sm_wPartitionTableOffset;
		CPartitionData::SetStatus(e0, CPartitionData::eActive);
		CPartitionData::SetType(e0, CPartitionData::eFat32);
		CPartitionData::SetLBAStartLocation(e0, 0x800);
		CPartitionData::SetPartitionSize(e0, 0x100000);

		unsigned char *e1 = e0 + CMBR::sm_byPartitionEntrySize;
		CPartitionData::SetStatus(e1, CPartitionData::eInactive);
		CPartitionData::SetType(e1, CPartitionData::eEmpty);

		CMBR mbr(sector, 0, 0, 0);
		check("CMBR::IsValid", mbr.IsValid());

		const CPartitionData *first = mbr.GetFirstValidPartitionData();
		check("GetFirstValidPartitionData found entry 0", first != 0 &&
			first->mLBAStart == 0x800UL);

		const CPartitionData *firstPrimary = mbr.GetFirstValidPrimaryPartitionData();
		check("GetFirstValidPrimaryPartitionData found entry 0",
			firstPrimary != 0 && firstPrimary->mPartitionSize == 0x100000UL);

		const CPartitionData *mode1 = mbr.GetFirstValidPartitionData(1);
		check("GetFirstValidPartitionData(1) found entry 0", mode1 != 0);

		std::memset(sector, 0, sizeof(sector)); /* no signature */
		CMBR blank(sector, 0, 0, 0);
		check("CMBR::IsValid false on all-zero sector", !blank.IsValid());
		check("no valid partitions on blank sector", blank.GetFirstValidPartitionData() == 0);
	}

	printf("[6] CPBR / decoded FAT12/16/32 boot-sector classes\n");
	{
		unsigned char buf[512];
		std::memset(buf, 0, sizeof(buf));
		CLittleEndObj::SetWord(buf + 0x1fe, 0xaa55);
		CPBR::SetBytePerSector(buf, 512);
		CPBR::SetSectPerCluster(buf, 4);
		CPBR::SetFatOffset(buf, 32);
		CPBR::SetFatNum(buf, CPBR::GetDefaultFatNum());
		CPBR::SetMaxNumRootEntries(buf, 512);
		CPBR::SetNumSectors(buf, 0); /* huge count used instead */
		CPBR::SetMediaType(buf, 0xf8);
		CPBR::SetNumFatSectors(buf, 100);
		CPBR::SetSectorPerTrack(buf, 63);
		CPBR::SetNumHeads(buf, 255);
		CPBR::SetNumHiddenSectors(buf, 63);
		CPBR::SetNumSectorsHuge(buf, 1000000);
		std::memcpy(buf + CPBR::sm_wOEMnameOffset, "MYOEM   ", 8);

		CPBR pbr(buf);
		check("CPBR decode BytePerSector", pbr.mBytePerSector == 512);
		check("CPBR decode SectPerCluster", pbr.mSectPerCluster == 4);
		check("CPBR decode FatOffset", pbr.mFatOffset == 32);
		check("CPBR decode FatNum", pbr.mFatNum == 2);
		check("CPBR decode MediaType", pbr.mMediaType == 0xf8);
		check("CPBR decode NumFatSectors", pbr.mNumFatSectors == 100);
		check("CPBR decode NumSectorsHuge", pbr.mNumSectorsHuge == 1000000UL);
		check("CPBR decode OEM name", std::memcmp(pbr.mOEMName, "MYOEM   ", 8) == 0);
		check("CPBR GetDefaultFatNum", CPBR::GetDefaultFatNum() == 2);

		CPBRFat12Fat16::SetBeginSectSignature(buf);
		check("SetBeginSectSignature JMP bytes", buf[0] == 0xeb);
		check("SetBeginSectSignature OEM default written",
			std::memcmp(buf + CPBR::sm_wOEMnameOffset, "OMEGA FS", 8) == 0);

		CPBRFat16::SetDriveNumber(buf);
		CPBRFat16::SetExtSignature(buf);
		CPBRFat16::SetPartitionSerialNum(buf);
		CPBRFat12Fat16::SetVolumeName(buf, 0); /* NULL -> default */
		CPBRFat16::SetFileSystemName(buf);

		CPBRFat16 fat16(buf);
		check("CPBRFat16 decode DriveNumber", fat16.mDriveNumber == 0);
		check("CPBRFat16 decode ExtSignature", fat16.mExtSignature == 0x29);
		check("CPBRFat16 decode PartitionSerialNum", fat16.mPartitionSerialNum == g_default9cValue);
		check("CPBRFat16 decode VolumeName default",
			std::memcmp(fat16.mVolumeName, "NO NAME    ", 11) == 0);
		check("CPBRFat16 decode FileSystemName",
			std::memcmp(fat16.mFileSystemName, "FAT16   ", 8) == 0);
		check("CPBRFat16::GetDefaultMaxNumRootEntries", CPBRFat16::GetDefaultMaxNumRootEntries() == 512);

		CPBRFat12Fat16::SetVolumeName(buf, "MYVOL");
		CPBRFat12 fat12(buf);
		check("CPBRFat12 decode custom VolumeName",
			std::strncmp(fat12.mVolumeName, "MYVOL", 5) == 0);

		std::memset(buf, 0, sizeof(buf));
		CLittleEndObj::SetWord(buf + 0x1fe, 0xaa55);
		CPBR::SetBytePerSector(buf, 512);
		CPBR::SetSectPerCluster(buf, 8);
		CPBR::SetFatOffset(buf, 32);
		CPBR::SetNumSectorsHuge(buf, 2000000);
		CPBRFat32::SetBeginSectSignature(buf);
		check("CPBRFat32 SetBeginSectSignature JMP bytes", buf[0] == 0xeb && buf[1] == 0x58);
		CPBRFat32::SetNumFatSectorsHuge(buf, 500);
		CPBRFat32::SetFatFlag(buf, 0, 1); /* mirrored -> bit 0x80 clear */
		CPBRFat32::SetFat32Version(buf, 0, 0);
		CPBRFat32::SetFirstRootCluster(buf, 2);
		CPBRFat32::SetDriveNumber(buf);
		CPBRFat32::SetExtSignature(buf);
		CPBRFat32::SetVolumeName(buf, 0);
		CPBRFat32::SetFileSystemName(buf);
		CPBRFat32::SetBackupPBROffset(buf);

		CPBRFat32 fat32(buf);
		check("CPBRFat32 decode NumFatSectorsHuge", fat32.mNumFatSectorsHuge == 500UL);
		check("CPBRFat32 decode FirstRootCluster", fat32.mFirstRootCluster == 2UL);
		check("CPBRFat32 decode DriveNumber", fat32.mDriveNumber == 0x80);
		check("CPBRFat32 decode ExtSignature", fat32.mExtSignature == 0x29);
		check("CPBRFat32 decode FileSystemName",
			std::memcmp(fat32.mFileSystemName, "FAT32   ", 8) == 0);
		check("CPBRFat32 decode BackupPBROffset", fat32.mBackupPBROffset == 6);
		check("CPBRFat32::GetBackupPBROffsetDefault", CPBRFat32::GetBackupPBROffsetDefault() == 6);

		unsigned int nc = fat32.GetNumCluster();
		unsigned int expectNc = (unsigned int)((2000000UL - 32 - (unsigned int)fat32.mFatNum * 500) / 8);
		check("CPBRFat32::GetNumCluster", nc == expectNc);
	}

	printf("[7] CPBRex\n");
	{
		check("GetDefaultVolumeName", std::strcmp(CPBRex::GetDefaultVolumeName(), "NO NAME    ") == 0);
		check("GetVolumeNameSize", CPBRex::GetVolumeNameSize() == 0x0b);
		CPBRex::GetNewPartitionSerialNum(); /* just exercise the dispatch, no return value */
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
