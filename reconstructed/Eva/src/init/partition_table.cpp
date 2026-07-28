/*
 * partition_table.cpp  -  see include/partition_table.h.
 *
 * Transcribed from the CSector/CLittleEndObj/CPartitionData/CMBR/CPBR/CPBRex/
 * CPBRFat12Fat16/CPBRFat12/CPBRFat16/CPBRFat32 bodies at .text+0x0809fxxx
 * (CLittleEndObj) and .text+0x080d3a30..0x080d58e0 (everything else), plus
 * CSector::SetEndSectSignature at .text+0x0818ab90. See the header for full
 * reachability/provenance.
 */

#include "partition_table.h"
#include "system_api.h"

#include <cstring>

extern CSystemApi *Api; /* mains.cpp */

namespace {

/* Real Api+0x94 soft-assert-report call -- same slot/shape already established
 * in tempo.cpp/config_manager.cpp/etc. Real calls here (not dropped): each one
 * has its own distinct "BootSect.cpp" file/line evidence in ground truth.
 */
inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

/* Real Api+0x9c vtable call -- same slot/shape as timer_engine.cpp/
 * kg_msg_processor.cpp's own ApiGetDefault9c(). */
inline unsigned long ApiGetDefault9c()
{
	typedef unsigned long (*Fn)(void *);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x9c);
	return fn(Api);
}

} // namespace

/* ===================== CSector ===================== */

unsigned long CSector::GetMinSize()
{
	return 0x200;
}

void CSector::SetEndSectSignature(unsigned char *buf)
{
	CLittleEndObj::SetWord(buf + sm_wEndSectSignatureOffset, 0xaa55);
}

/* ===================== CLittleEndObj ===================== */

unsigned short CLittleEndObj::GetWord(const unsigned char *p)
{
	return (unsigned short)(p[0] | (p[1] << 8));
}

unsigned long CLittleEndObj::GetDWord(const unsigned char *p)
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) |
		((unsigned long)p[3] << 24);
}

void CLittleEndObj::SetWord(unsigned char *p, unsigned short v)
{
	*(unsigned short *)p = v;
}

void CLittleEndObj::SetDWord(unsigned char *p, unsigned long v)
{
	/* real ground truth writes byte[1] before byte[0] -- same observable
	 * result, order preserved verbatim */
	p[1] = (unsigned char)(v >> 8);
	p[0] = (unsigned char)v;
	p[2] = (unsigned char)(v >> 0x10);
	p[3] = (unsigned char)(v >> 0x18);
}

/* Real ground-truth duplicates of GetDWord/GetWord (same body, different
 * declared return type) -- see header. */
int CLittleEndObj::GetInt(const unsigned char *p) { return (int)GetDWord(p); }
short CLittleEndObj::GetShort(const unsigned char *p) { return (short)GetWord(p); }
long CLittleEndObj::GetLong(const unsigned char *p) { return (long)GetDWord(p); }
unsigned int CLittleEndObj::GetUInt(const unsigned char *p) { return (unsigned int)GetDWord(p); }
unsigned short CLittleEndObj::GetUShort(const unsigned char *p) { return GetWord(p); }
unsigned long CLittleEndObj::GetULong(const unsigned char *p) { return GetDWord(p); }

void CLittleEndObj::SetUInt(unsigned char *p, unsigned int v) { SetDWord(p, v); }

unsigned long CLittleEndObj::GetU3Byte(const unsigned char *p)
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16);
}

/* ===================== CPartitionData ===================== */

const CPartitionData::SMediaGeometry CPartitionData::sm_tMaxGeometry = { 1024, 255, 63 };

bool CPartitionData::IsEmpty(const unsigned char *buf)
{
	return buf[sm_wTypeOffset] == 0;
}

bool CPartitionData::IsExtended(const unsigned char *buf)
{
	unsigned char t = buf[sm_wTypeOffset];
	if (t != 0x0f && t != 5)
		return t == 0x85;
	return true;
}

void CPartitionData::Reset(unsigned char *buf)
{
	buf[sm_wTypeOffset] = 0;
	buf[sm_wStatusOffset] = 0;
	buf[sm_wStartCylAndSectOffset] = 0;
	buf[sm_wStartCylAndSectOffset + 1] = 0;
	buf[sm_wStartHeadOffset] = 0;
	CLittleEndObj::SetDWord(buf + sm_wLBAStartLocationOffset, 0);
	buf[sm_wEndCylAndSectOffset] = 0;
	buf[sm_wEndCylAndSectOffset + 1] = 0;
	buf[sm_wEndHeadOffset] = 0;
	CLittleEndObj::SetDWord(buf + sm_wPartitionSizeOffset, 0);
}

void CPartitionData::SetStatus(unsigned char *buf, EStatus v)
{
	buf[sm_wStatusOffset] = (unsigned char)v;
}

void CPartitionData::SetType(unsigned char *buf, EType v)
{
	buf[sm_wTypeOffset] = (unsigned char)v;
}

unsigned char CPartitionData::GetType(const unsigned char *buf)
{
	return buf[sm_wTypeOffset];
}

void CPartitionData::SetStartCHS(unsigned char *buf, unsigned short cyl, unsigned short head,
	unsigned short sect)
{
	buf[sm_wStartCylAndSectOffset] =
		(unsigned char)((sm_wMaskLowSect & sect) | ((cyl >> 2) & sm_wMaskHighCyl));
	buf[sm_wStartCylAndSectOffset + 1] = (unsigned char)cyl;
	buf[sm_wStartHeadOffset] = (unsigned char)head;
}

void CPartitionData::SetEndCHS(unsigned char *buf, unsigned short cyl, unsigned short head,
	unsigned short sect)
{
	buf[sm_wEndCylAndSectOffset] =
		(unsigned char)((sm_wMaskLowSect & sect) | ((cyl >> 2) & sm_wMaskHighCyl));
	buf[sm_wEndCylAndSectOffset + 1] = (unsigned char)cyl;
	buf[sm_wEndHeadOffset] = (unsigned char)head;
}

void CPartitionData::SetLBAStartLocation(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wLBAStartLocationOffset, v);
}

unsigned long CPartitionData::GetLBAStartLocation(const unsigned char *buf)
{
	return CLittleEndObj::GetDWord(buf + sm_wLBAStartLocationOffset);
}

void CPartitionData::SetPartitionSize(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wPartitionSizeOffset, v);
}

bool CPartitionData::IsEmpty(EType t)
{
	return t == 0;
}

bool CPartitionData::IsExtended(EType t)
{
	if (t != 0x0f && t != 5)
		return t == 0x85;
	return true;
}

bool CPartitionData::IsSupported(EType t)
{
	if (t == 6 || t == 0x0e || t == 1 || t == 4 || t == 0x0c || t == 0x0b)
		return true;
	if (t != 0x0f && t != 5)
		return t == 0x85;
	return true;
}

const CPartitionData::SMediaGeometry *CPartitionData::GetMaxMediaGeometry()
{
	return &sm_tMaxGeometry;
}

int CPartitionData::CHStoLBA(unsigned short cyl, unsigned short head, unsigned short sect,
	const SMediaGeometry &geom)
{
	if (geom.mCylinders <= cyl)
		ApiAssert("BootSect.cpp", 0x125);
	if (geom.mHeads <= head)
		ApiAssert("BootSect.cpp", 0x126);
	if (geom.mSectorsPerTrack < sect)
		ApiAssert("BootSect.cpp", 0x127);
	if (sect == 0)
		ApiAssert("BootSect.cpp", 0x128);

	return (int)((sect - 1) +
		((unsigned int)cyl * (unsigned int)geom.mHeads + (unsigned int)head) *
		(unsigned int)geom.mSectorsPerTrack);
}

void CPartitionData::LBAtoCHS(unsigned int lba, unsigned short *cyl, unsigned short *head,
	unsigned short *sect, const SMediaGeometry &geom)
{
	unsigned int heads = geom.mHeads;
	unsigned int spt = geom.mSectorsPerTrack;
	unsigned int c = lba / (heads * spt);
	*cyl = (unsigned short)c;
	unsigned int rem = lba - c * heads * spt;
	unsigned int h = rem / spt;
	*head = (unsigned short)h;
	rem = rem - h * spt;
	if (spt < rem)
		ApiAssert("BootSect.cpp", 0x13e);
	unsigned short s = (unsigned short)(rem + 1);
	*sect = s;
	if (geom.mSectorsPerTrack < s)
		s = geom.mSectorsPerTrack;
	*sect = s;

	if (geom.mHeads == 0)
		*head = 0;
	else if (geom.mHeads <= *head)
		*head = geom.mHeads - 1;

	if (geom.mCylinders == 0) {
		*cyl = 0;
	} else {
		unsigned int cc = *cyl;
		while (geom.mCylinders <= (unsigned short)cc) {
			if (geom.mHeads < (unsigned int)(*head) * 2) {
				*sect = geom.mSectorsPerTrack;
				*head = geom.mHeads - 1;
				cc = geom.mCylinders - 1;
				*cyl = (unsigned short)cc;
			} else {
				*head = *head * 2;
				cc = (unsigned int)((int)cc >> 1);
				*cyl = (unsigned short)cc;
			}
			cc = *cyl;
		}
	}
}

unsigned int CPartitionData::GetSectCeilLBA(unsigned int lba, unsigned long align,
	const SMediaGeometry &geom)
{
	if (align < 2)
		align = geom.mSectorsPerTrack;
	if (lba % align != 0)
		lba = (unsigned int)(align + lba - lba % align);
	return lba;
}

void CPartitionData::AdjustCHS(unsigned short *cyl, unsigned short *head, unsigned short *sect,
	const SMediaGeometry &geom)
{
	if (geom.mSectorsPerTrack < *sect)
		*sect = geom.mSectorsPerTrack;

	if (geom.mHeads == 0)
		*head = 0;
	else if (geom.mHeads <= *head)
		*head = geom.mHeads - 1;

	if (geom.mCylinders == 0) {
		*cyl = 0;
	} else {
		unsigned int cc = *cyl;
		while (geom.mCylinders <= (unsigned short)cc) {
			if (geom.mHeads < (unsigned int)(*head) * 2) {
				*sect = geom.mSectorsPerTrack;
				*head = geom.mHeads - 1;
				cc = geom.mCylinders - 1;
				*cyl = (unsigned short)cc;
			} else {
				*head = *head * 2;
				cc = (unsigned int)((int)cc >> 1);
				*cyl = (unsigned short)cc;
			}
			cc = *cyl;
		}
	}
}

void CPartitionData::GetMaxCHS(unsigned short *cyl, unsigned short *head, unsigned short *sect,
	const SMediaGeometry &geom)
{
	*cyl = geom.mCylinders;
	if (geom.mCylinders != 0)
		*cyl = geom.mCylinders - 1;
	*head = geom.mHeads;
	if (geom.mHeads != 0)
		*head = geom.mHeads - 1;
	*sect = geom.mSectorsPerTrack;
}

CPartitionData::CPartitionData()
{
	Reset();
}

CPartitionData::CPartitionData(const unsigned char *buf)
{
	mStatus = (EStatus)buf[sm_wStatusOffset];
	mType = (EType)buf[sm_wTypeOffset];
	mStartHead = buf[sm_wStartHeadOffset];
	mEndHead = buf[sm_wEndHeadOffset];

	unsigned short w = CLittleEndObj::GetWord(buf + sm_wStartCylAndSectOffset);
	mStartSect = w & sm_wMaskLowSect;
	mStartCyl = (unsigned short)(((w & sm_wMaskHighCyl) << 2) | ((w & sm_wMaskLowCyl) >> 8));

	w = CLittleEndObj::GetWord(buf + sm_wEndCylAndSectOffset);
	mEndSect = w & sm_wMaskLowSect;
	mEndCyl = (unsigned short)(((w & sm_wMaskHighCyl) << 2) | ((w & sm_wMaskLowCyl) >> 8));

	mLBAStart = CLittleEndObj::GetDWord(buf + sm_wLBAStartLocationOffset);
	mPartitionSize = CLittleEndObj::GetDWord(buf + sm_wPartitionSizeOffset);
}

CPartitionData::CPartitionData(const CPartitionData &other)
{
	mStatus = other.mStatus;
	mType = other.mType;
	mStartHead = other.mStartHead;
	mStartCyl = other.mStartCyl;
	mStartSect = other.mStartSect;
	mEndHead = other.mEndHead;
	mEndCyl = other.mEndCyl;
	mEndSect = other.mEndSect;
	mLBAStart = other.mLBAStart;
	mPartitionSize = other.mPartitionSize;
}

CPartitionData &CPartitionData::operator=(const CPartitionData &other)
{
	if (this != &other) {
		mStatus = other.mStatus;
		mType = other.mType;
		mStartHead = other.mStartHead;
		mStartCyl = other.mStartCyl;
		mStartSect = other.mStartSect;
		mEndHead = other.mEndHead;
		mEndCyl = other.mEndCyl;
		mEndSect = other.mEndSect;
		mLBAStart = other.mLBAStart;
		mPartitionSize = other.mPartitionSize;
	}
	return *this;
}

void CPartitionData::Reset()
{
	mStatus = (EStatus)0;
	mStartHead = 0;
	mStartCyl = 0;
	mStartSect = 0;
	mType = (EType)0;
	mEndHead = 0;
	mEndCyl = 0;
	mEndSect = 0;
	mLBAStart = 0;
	mPartitionSize = 0;
}

bool CPartitionData::IsExtended() const
{
	if (mType != 0x0f && mType != 5)
		return mType == 0x85;
	return true;
}

bool CPartitionData::IsSupported() const
{
	if (mType == 6 || mType == 0x0e || mType == 1 || mType == 4 || mType == 0x0c || mType == 0x0b)
		return true;
	if (mType != 0x0f && mType != 5)
		return mType == 0x85;
	return true;
}

bool CPartitionData::IsEmpty() const
{
	return mType == 0;
}

/* ===================== CMBR ===================== */

CMBR::CMBR(const unsigned char *buf, unsigned short /*unused1*/, unsigned short /*unused2*/,
	unsigned long extBase)
{
	mVptr = 0; /* real vtable-swap: all-EvaVTableStub, see header -- omitted here,
	            * no reconstructed caller destroys a CMBR polymorphically */
	mEndSectSignature = CLittleEndObj::GetWord(buf + CSector::sm_wEndSectSignatureOffset);
	mSupported0 = mSupported1 = mSupported2 = mSupported3 = 0;

	const unsigned char *entry = buf + sm_wPartitionTableOffset;
	CPartitionData *slots[4] = { &mPartition0, &mPartition1, &mPartition2, &mPartition3 };
	int *flags[4] = { &mSupported0, &mSupported1, &mSupported2, &mSupported3 };

	for (int i = 0; i < 4; ++i) {
		CPartitionData pd(entry + i * sm_byPartitionEntrySize);
		int flag = 0;
		if (pd.mType != 0 && (pd.mStatus == 0 || pd.mStatus == 0x80) &&
			pd.mLBAStart != 0 && pd.mPartitionSize != 0) {
			unsigned long size = pd.mPartitionSize;
			if (pd.mType == 0x0f || pd.mType == 5 || pd.mType == (CPartitionData::EType)0x85)
				size = extBase + pd.mPartitionSize;
			(void)size; /* real ground truth computes this augmented size but never
			             * stores it back into the decoded CPartitionData -- see
			             * header, transcribed verbatim */
			if (pd.mType == 6 || pd.mType == 0x0e || pd.mType == 1 || pd.mType == 4 ||
				pd.mType == 0x0c || pd.mType == 0x0b || pd.mType == 0x0f || pd.mType == 5 ||
				pd.mType == (CPartitionData::EType)0x85)
				flag = 1;
		}
		*flags[i] = flag;
		*slots[i] = pd;
	}
}

CMBR::~CMBR()
{
	mVptr = 0;
}

bool CMBR::IsValid() const
{
	return mEndSectSignature == 0xaa55;
}

const CPartitionData *CMBR::GetFirstValidPartitionData() const
{
	const CPartitionData *parts[4] = { &mPartition0, &mPartition1, &mPartition2, &mPartition3 };
	const int *flags[4] = { &mSupported0, &mSupported1, &mSupported2, &mSupported3 };
	for (int i = 0; i < 4; ++i) {
		if (*flags[i] == 1 && parts[i]->mType != 0)
			return parts[i];
	}
	return 0;
}

const CPartitionData *CMBR::GetFirstValidPartitionData(int mode) const
{
	const CPartitionData *parts[4] = { &mPartition0, &mPartition1, &mPartition2, &mPartition3 };
	const int *flags[4] = { &mSupported0, &mSupported1, &mSupported2, &mSupported3 };

	if (mode == 1) {
		for (int i = 0; i < 3; ++i) {
			if (*flags[i] == 1 && parts[i]->mType != 0)
				return parts[i];
		}
		if (*flags[3] != 1 || parts[3]->mType == 0)
			return 0;
		return parts[3];
	}

	for (int i = 0; i < 3; ++i) {
		CPartitionData::EType t = parts[i]->mType;
		if (*flags[i] == 1 && t != 0 && t != 0x0f && t != 5 && t != (CPartitionData::EType)0x85)
			return parts[i];
	}
	CPartitionData::EType t3 = parts[3]->mType;
	if (*flags[3] != 1 || t3 == 0)
		return 0;
	if (t3 == 0x0f || t3 == 5 || t3 == (CPartitionData::EType)0x85)
		return 0;
	return parts[3];
}

const CPartitionData *CMBR::GetFirstValidPrimaryPartitionData() const
{
	const CPartitionData *parts[4] = { &mPartition0, &mPartition1, &mPartition2, &mPartition3 };
	const int *flags[4] = { &mSupported0, &mSupported1, &mSupported2, &mSupported3 };

	for (int i = 0; i < 3; ++i) {
		CPartitionData::EType t = parts[i]->mType;
		if (*flags[i] == 1 && t != 0 && t != 0x0f && t != 5 && t != (CPartitionData::EType)0x85)
			return parts[i];
	}
	CPartitionData::EType t3 = parts[3]->mType;
	if (*flags[3] != 1 || t3 == 0)
		return 0;
	if (t3 == 0x0f || t3 == 5)
		return 0;
	if (t3 == (CPartitionData::EType)0x85)
		return 0;
	return parts[3];
}

/* ===================== CPBR ===================== */

CPBR::CPBR(const unsigned char *buf)
{
	mEndSectSignature = CLittleEndObj::GetWord(buf + CSector::sm_wEndSectSignatureOffset);
	std::memcpy(mOEMName, buf + sm_wOEMnameOffset, 8);
	mBytePerSector = CLittleEndObj::GetWord(buf + sm_wBytePerSectorOffset);
	mSectPerCluster = buf[sm_wSectPerClusterOffset];
	mFatOffset = CLittleEndObj::GetWord(buf + sm_wFatOffsetOffset);
	mFatNum = buf[sm_wFatNumOffset];
	mMaxNumRootEntries = CLittleEndObj::GetWord(buf + sm_wMaxNumRootEntriesOffset);
	mNumSectors = CLittleEndObj::GetWord(buf + sm_wNumSectorsOffset);
	mMediaType = buf[sm_wMediaTypeOffset];
	mNumFatSectors = CLittleEndObj::GetWord(buf + sm_wNumFatSectorsOffset);
	mSectorPerTrack = CLittleEndObj::GetWord(buf + sm_wSectorPerTrackOffset);
	mNumHeads = CLittleEndObj::GetWord(buf + sm_wNumHeadsOffset);
	mNumHiddenSectors = CLittleEndObj::GetDWord(buf + sm_wNumHiddenSectorsOffset);
	mNumSectorsHuge = CLittleEndObj::GetDWord(buf + sm_wNumSectorsHugeOffset);
}

void CPBR::SetBytePerSector(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wBytePerSectorOffset, v);
}

void CPBR::SetSectPerCluster(unsigned char *buf, unsigned char v)
{
	buf[sm_wSectPerClusterOffset] = v;
}

void CPBR::SetFatOffset(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wFatOffsetOffset, v);
}

void CPBR::SetFatNum(unsigned char *buf, unsigned char v)
{
	buf[sm_wFatNumOffset] = v;
}

void CPBR::SetMaxNumRootEntries(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wMaxNumRootEntriesOffset, v);
}

void CPBR::SetNumSectors(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wNumSectorsOffset, v);
}

void CPBR::SetMediaType(unsigned char *buf, unsigned char v)
{
	buf[sm_wMediaTypeOffset] = v;
}

void CPBR::SetNumFatSectors(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wNumFatSectorsOffset, v);
}

void CPBR::SetSectorPerTrack(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wSectorPerTrackOffset, v);
}

void CPBR::SetNumHeads(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wNumHeadsOffset, v);
}

void CPBR::SetNumHiddenSectors(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wNumHiddenSectorsOffset, v);
}

void CPBR::SetNumSectorsHuge(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wNumSectorsHugeOffset, v);
}

unsigned char CPBR::GetDefaultFatNum()
{
	return sm_byDefaultFatNum;
}

/* ===================== CPBRex ===================== */

char *CPBRex::sm_sDefaultVolumeName = (char *)"NO NAME    ";
char *CPBRex::sm_sDefaultOEMName = (char *)"OMEGA FS";

CPBRex::CPBRex(const unsigned char *buf) : CPBR(buf)
{
}

char *CPBRex::GetDefaultVolumeName()
{
	return sm_sDefaultVolumeName;
}

unsigned long CPBRex::GetVolumeNameSize()
{
	return 0x0b;
}

void CPBRex::GetNewPartitionSerialNum()
{
	/* real ground truth: dispatches through Api+0x9c and discards the result */
	ApiGetDefault9c();
}

/* ===================== CPBRFat12Fat16 ===================== */

CPBRFat12Fat16::CPBRFat12Fat16(const unsigned char *buf) : CPBR(buf)
{
	mDriveNumber = CLittleEndObj::GetWord(buf + sm_wDriveNumberOffset);
	mExtSignature = buf[sm_wExtSignatureOffset];
	mPartitionSerialNum = CLittleEndObj::GetDWord(buf + sm_wPartitionSerialNumOffset);
	std::memcpy(mVolumeName, buf + sm_wVolumeNameOffset, 11);
	std::memcpy(mFileSystemName, buf + sm_wFileSystemNameOffset, 8);
}

void CPBRFat12Fat16::SetBeginSectSignature(unsigned char *buf)
{
	/* real: 3-byte x86 JMP+NOP boot-sector stub (0xEB 0x3E 0x90) packed with a
	 * following literal byte as one dword write, then the default OEM name */
	CLittleEndObj::SetDWord(buf, 0x20903eeb);
	strncpy((char *)(buf + CPBR::sm_wOEMnameOffset), CPBRex::sm_sDefaultOEMName, 8);
}

unsigned short CPBRFat12Fat16::GetDefaultFatOffset()
{
	return sm_wDefaultFatOffset;
}

unsigned short CPBRFat12Fat16::GetVolumeNameOffset()
{
	return sm_wVolumeNameOffset;
}

void CPBRFat12Fat16::SetVolumeName(unsigned char *buf, const char *name)
{
	const char *src = name ? name : CPBRex::sm_sDefaultVolumeName;
	strncpy((char *)(buf + sm_wVolumeNameOffset), src, 0x0b);
}

/* ===================== CPBRFat12 ===================== */

char CPBRFat12::sm_sDefaultFileSystemName[9] = "FAT12   ";

CPBRFat12::CPBRFat12(const unsigned char *buf) : CPBRFat12Fat16(buf)
{
}

void CPBRFat12::SetFileSystemName(unsigned char *buf)
{
	size_t n = strlen(sm_sDefaultFileSystemName);
	strncpy((char *)(buf + CPBRFat12Fat16::sm_wFileSystemNameOffset), sm_sDefaultFileSystemName, n);
}

char *CPBRFat12::GetDefaultFileSystemName()
{
	return sm_sDefaultFileSystemName;
}

unsigned long CPBRFat12::GetDefaultFileSystemNameLen()
{
	return strlen(sm_sDefaultFileSystemName);
}

int CPBRFat12::GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
	unsigned char numFats, unsigned char rootEntries32, unsigned short bytesPerSector,
	unsigned short sectPerCluster)
{
	int a = (int)(((totalSectors - reserved) - (unsigned int)bytesPerSector) /
		((unsigned int)numFats + ((unsigned int)rootEntries32 * 2 * (unsigned int)sectPerCluster) / 3) + 1);
	int b = (int)(0x1800 / (long long)(int)(unsigned int)sectPerCluster) - 1;
	if ((unsigned short)a <= (unsigned short)b)
		b = a;
	return b;
}

/* ===================== CPBRFat16 ===================== */

char CPBRFat16::sm_sDefaultFileSystemName[9] = "FAT16   ";

CPBRFat16::CPBRFat16(const unsigned char *buf) : CPBRFat12Fat16(buf)
{
}

void CPBRFat16::SetFileSystemName(unsigned char *buf)
{
	size_t n = strlen(sm_sDefaultFileSystemName);
	strncpy((char *)(buf + CPBRFat12Fat16::sm_wFileSystemNameOffset), sm_sDefaultFileSystemName, n);
}

void CPBRFat16::SetDriveNumber(unsigned char *buf)
{
	CLittleEndObj::SetWord(buf + CPBRFat12Fat16::sm_wDriveNumberOffset, 0);
}

void CPBRFat16::SetExtSignature(unsigned char *buf)
{
	buf[CPBRFat12Fat16::sm_wExtSignatureOffset] = 0x29;
}

void CPBRFat16::SetPartitionSerialNum(unsigned char *buf)
{
	unsigned long v = ApiGetDefault9c();
	CLittleEndObj::SetDWord(buf + CPBRFat12Fat16::sm_wPartitionSerialNumOffset, v);
}

char *CPBRFat16::GetDefaultFileSystemName()
{
	return sm_sDefaultFileSystemName;
}

unsigned long CPBRFat16::GetDefaultFileSystemNameLen()
{
	return strlen(sm_sDefaultFileSystemName);
}

unsigned short CPBRFat16::GetDefaultMaxNumRootEntries()
{
	return sm_wDefaultMaxNumRootEntries;
}

int CPBRFat16::GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
	unsigned char numFats, unsigned char rootEntries32, unsigned short bytesPerSector,
	unsigned short sectPerCluster)
{
	int a = (int)(((totalSectors - reserved) - (unsigned int)bytesPerSector) /
		((unsigned int)(((int)((unsigned int)rootEntries32 * (unsigned int)sectPerCluster)) >> 1) +
			(unsigned int)numFats) + 1);
	int b = (int)(0x20000 / (long long)(int)(unsigned int)sectPerCluster) - 1;
	if ((unsigned short)a <= (unsigned short)b)
		b = a;
	return b;
}

/* ===================== CPBRFat32 ===================== */

char CPBRFat32::sm_sDefaultFileSystemName[9] = "FAT32   ";

CPBRFat32::CPBRFat32(const unsigned char *buf) : CPBR(buf)
{
	mNumFatSectorsHuge = CLittleEndObj::GetDWord(buf + sm_wNumFatSectorsHugeOffset);
	mFatFlag = CLittleEndObj::GetWord(buf + sm_wFatFlagOffset);

	unsigned short ver = CLittleEndObj::GetWord(buf + sm_wFat32VersionOffset);
	mFat32VersionMinor = (unsigned char)ver;
	mFat32VersionMajor = (unsigned char)(ver >> 8);

	mFirstRootCluster = CLittleEndObj::GetDWord(buf + sm_wFirstRootClusterOffset);
	mFSISOffset = CLittleEndObj::GetWord(buf + sm_wFSISOffsetOffset);
	mBackupPBROffset = CLittleEndObj::GetWord(buf + sm_wBackupPBROffsetOffset);

	mDriveNumber = CLittleEndObj::GetWord(buf + sm_wDriveNumberOffset);
	mExtSignature = buf[sm_wExtSignatureOffset];
	mPartitionSerialNum = CLittleEndObj::GetDWord(buf + sm_wPartitionSerialNumOffset);
	std::memcpy(mVolumeName, buf + sm_wVolumeNameOffset, 11);
	std::memcpy(mFileSystemName, buf + sm_wFileSystemNameOffset, 8);
}

unsigned int CPBRFat32::GetNumCluster() const
{
	return (unsigned int)((mNumSectorsHuge - (unsigned int)mFatOffset -
		(unsigned int)mFatNum * (int)mNumFatSectorsHuge) / (unsigned int)mSectPerCluster);
}

unsigned short CPBRFat32::GetFirstRootClusterOffset()
{
	return sm_wFirstRootClusterOffset;
}

unsigned short CPBRFat32::GetBackupPBROffsetDefault()
{
	return sm_wBackupPBROffsetDefault;
}

void CPBRFat32::SetBeginSectSignature(unsigned char *buf)
{
	CLittleEndObj::SetDWord(buf, 0x209058eb);
	strncpy((char *)(buf + CPBR::sm_wOEMnameOffset), CPBRex::sm_sDefaultOEMName, 8);
}

void CPBRFat32::SetFileSystemName(unsigned char *buf)
{
	size_t n = strlen(sm_sDefaultFileSystemName);
	strncpy((char *)(buf + sm_wFileSystemNameOffset), sm_sDefaultFileSystemName, n);
}

void CPBRFat32::SetNumFatSectorsHuge(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wNumFatSectorsHugeOffset, v);
}

void CPBRFat32::SetFatFlag(unsigned char *buf, unsigned char activeFat, int mirrored)
{
	CLittleEndObj::SetWord(buf + sm_wFatFlagOffset,
		(unsigned short)((-(unsigned short)(mirrored == 0) & 0x80) | (activeFat & 0x1f)));
}

void CPBRFat32::SetFat32Version(unsigned char *buf, unsigned char major, unsigned char minor)
{
	CLittleEndObj::SetWord(buf + sm_wFat32VersionOffset,
		(unsigned short)(((unsigned short)major << 8) | minor));
}

void CPBRFat32::SetFirstRootCluster(unsigned char *buf, unsigned long v)
{
	CLittleEndObj::SetDWord(buf + sm_wFirstRootClusterOffset, v);
}

void CPBRFat32::SetFSISOffset(unsigned char *buf, unsigned short v)
{
	CLittleEndObj::SetWord(buf + sm_wFSISOffsetOffset, v);
}

void CPBRFat32::SetBackupPBROffset(unsigned char *buf)
{
	CLittleEndObj::SetWord(buf + sm_wBackupPBROffsetOffset, sm_wBackupPBROffsetDefault);
}

void CPBRFat32::SetDriveNumber(unsigned char *buf)
{
	CLittleEndObj::SetWord(buf + sm_wDriveNumberOffset, 0x80);
}

void CPBRFat32::SetExtSignature(unsigned char *buf)
{
	buf[sm_wExtSignatureOffset] = 0x29;
}

void CPBRFat32::SetPartitionSerialNum(unsigned char *buf)
{
	unsigned long v = ApiGetDefault9c();
	CLittleEndObj::SetDWord(buf + sm_wPartitionSerialNumOffset, v);
}

void CPBRFat32::SetVolumeName(unsigned char *buf, const char *name)
{
	const char *src = name ? name : CPBRex::sm_sDefaultVolumeName;
	strncpy((char *)(buf + sm_wVolumeNameOffset), src, 0x0b);
}

char *CPBRFat32::GetDefaultFileSystemName()
{
	return sm_sDefaultFileSystemName;
}

unsigned long CPBRFat32::GetDefaultFileSystemNameLen()
{
	return strlen(sm_sDefaultFileSystemName);
}

unsigned short CPBRFat32::GetDefaultFatOffset()
{
	return sm_wDefaultFatOffset;
}

unsigned short CPBRFat32::GetVolumeNameOffset()
{
	return sm_wVolumeNameOffset;
}

unsigned int CPBRFat32::GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
	unsigned char numFats, unsigned short bytesPerSector, unsigned short sectPerCluster)
{
	unsigned int a = (unsigned int)((totalSectors - reserved) /
		((unsigned int)(((int)((unsigned int)bytesPerSector * (unsigned int)sectPerCluster)) >> 2) +
			(unsigned int)numFats) + 1);
	unsigned int b = (unsigned int)(0x40000000 / (long long)(int)(unsigned int)sectPerCluster) - 1;
	if (a <= b)
		b = a;
	return b;
}
