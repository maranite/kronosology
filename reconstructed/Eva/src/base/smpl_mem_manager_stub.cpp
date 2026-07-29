/*
 * smpl_mem_manager_stub.cpp  -  REAL host-functional backing for the
 * CUsrMultisample/CUsrSample/CUsrDrumsample/CUsrRel/USTGAPIPCMBanks/
 * CSmplModeMgr/CDeviceDesc externs smpl_mem_manager.cpp depends on. None of
 * these classes are reconstructed here (see smpl_mem_manager.h) -- this
 * gives verify/test_smpl_mem_manager.cpp real backing arrays to allocate
 * against, same "genuinely functional, not inert" spirit as
 * file_operation_stub.cpp.
 */

#include "smpl_mem_manager.h"

#include <cstring>

namespace {

struct SMultisampleRec {
	char name[24]; /* +0x00 */
	unsigned short relStart; /* +0x18 */
	unsigned char flagByte; /* +0x1a -- attack-point count, 0 == free */
	char pad;
};

struct SSampleRec {
	unsigned short drumIndex; /* +0x00 */
	char pad0[0x1a - 2];
	unsigned int used; /* +0x1c */
	unsigned long sampleRate;
};

struct SDrumRec {
	char name[24]; /* +0x00 */
};

struct SRelRec {
	unsigned short sampleIndex; /* +0x00 */
};

SMultisampleRec g_multisamples[4000];
SSampleRec g_samples[16000];
SDrumRec g_drums[16000];
SRelRec g_rels[16000];

unsigned long g_pcmHeapSize = 0x400000; /* 4MB fake RAM heap, plenty for KAT tests */
unsigned long g_pcmUsedSize = 0;
unsigned short g_dsStereoMap[16000][2];

char g_scratch[65536];

/* Shared RAM-heap stand-in for RAMBankPCM_read/write -- a real host buffer
 * so cutdata/insertdata/dataclear/copydata KAT round-trips are genuine
 * (write then read-back, not two independent no-op arenas). */
char g_ramArena[1 << 20];

} // namespace

void CUsrMultisample::Init() { mResolved = 0; memset(mPad, 0, sizeof(mPad)); }
void CUsrMultisample::Bless(unsigned int index)
{
	if (index >= 4000)
		index = 4000 - 1;
	mResolved = &g_multisamples[index];
}
void CUsrMultisample::Curse() { }

void CUsrDrumsample::Bless(unsigned int index)
{
	if (index >= 16000)
		index = 16000 - 1;
	mResolved = &g_drums[index];
}

void CUsrRel::Bless(unsigned int index)
{
	if (index >= 16000)
		index = 16000 - 1;
	mResolved = &g_rels[index];
}

void CUsrSample::Init() { mResolved = 0; }
void CUsrSample::Bless(unsigned int index)
{
	if (index >= 16000)
		index = 16000 - 1;
	mResolved = &g_samples[index];
}
void CUsrSample::Curse() { }
unsigned long CUsrSample::GetSampleRate() const
{
	return mResolved ? ((SSampleRec *)mResolved)->sampleRate : 0;
}

CSmplModeMgr *CSmplModeMgr::theSmplModeMgr = 0;
unsigned long CSmplModeMgr::SAMPLINGMODEBUFFSIZE = sizeof(g_scratch);

namespace {
CSmplModeMgr g_smplModeMgrInstance;
struct SSmplModeMgrInit {
	SSmplModeMgrInit()
	{
		g_smplModeMgrInstance.mScratchBuf = g_scratch;
		CSmplModeMgr::theSmplModeMgr = &g_smplModeMgrInstance;
	}
} g_smplModeMgrInit;
} // namespace

unsigned long long CDeviceDesc::get_media_free_size(EDevice_Id /*device*/)
{
	return 0x12345678ULL;
}

void USTGAPIPCMBanks::InitUserSampling()
{
	memset(g_multisamples, 0, sizeof(g_multisamples));
	memset(g_samples, 0, sizeof(g_samples));
	memset(g_drums, 0, sizeof(g_drums));
	memset(g_rels, 0, sizeof(g_rels));
}

unsigned long USTGAPIPCMBanks::GetUsrPCMHeapSize() { return g_pcmHeapSize; }
void USTGAPIPCMBanks::SetUsrPCMUsedSize(unsigned long size) { g_pcmUsedSize = size; }

void USTGAPIPCMBanks::RAMBankPCM_read(void *scratchBuf, unsigned long offset, unsigned long size)
{
	/* Host stand-in RAM heap: treat `offset` as a byte offset into a big
	 * shared arena, wrapped to stay in-bounds so KAT round-trips are
	 * deterministic without needing a real multi-MB buffer per test. */
	unsigned long o = offset % sizeof(g_ramArena);
	unsigned long n = size;
	if (o + n > sizeof(g_ramArena))
		n = sizeof(g_ramArena) - o;
	memcpy(scratchBuf, g_ramArena + o, n);
}

void USTGAPIPCMBanks::RAMBankPCM_write(void *scratchBuf, unsigned long offset, unsigned long size)
{
	unsigned long o = offset % sizeof(g_ramArena);
	unsigned long n = size;
	if (o + n > sizeof(g_ramArena))
		n = sizeof(g_ramArena) - o;
	memcpy(g_ramArena + o, scratchBuf, n);
}

unsigned short *USTGAPIPCMBanks::GetUsrDSStereoMapping(unsigned int drumIndex)
{
	if (drumIndex >= 16000)
		drumIndex = 16000 - 1;
	return g_dsStereoMap[drumIndex];
}

CUsrMultisample USTGAPIPCMBanks::sMult;
CUsrSample USTGAPIPCMBanks::sSamp;
CUsrDrumsample USTGAPIPCMBanks::sDrum;
CUsrRel USTGAPIPCMBanks::sRel;
