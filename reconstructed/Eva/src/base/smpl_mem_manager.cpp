/*
 * smpl_mem_manager.cpp  -  CSmplMemManager. See smpl_mem_manager.h for full
 * class-level provenance, layout, and the deferred-method list.
 *
 * `ramsize`/`ramtop`/`ramfreetop` quirk: ground truth is 3 separate 4-byte
 * bss globals (`nm -C -S`: each size=4, at .bss+0xbe40/0xbe50/0xbe60 -- an
 * incidental 16-byte spacing shared with 2 confirmed-UNRELATED neighbours,
 * `CESSamplingTask::m_poSmplPRManager` and a `getusedsampleno()` static-
 * local guard variable). `getfreetop`/`setfreetop`/`isexistbank`/
 * `getremainsize`/`getremainsmpltimems`/`getremainsmpltimeandsize` all index
 * them via raw pointer arithmetic, e.g. `(&ramfreetop)[bank]` -- reading
 * past the single scalar into whatever the linker happened to place next.
 * Every confirmed real caller (traced via `nm -C` on the enclosing
 * function, same technique as ram_sample.h's own reachability check) passes
 * bank==0, so this is very likely vestigial multi-bank plumbing from an
 * earlier (Trinity/Triton-era) codebase that was never exercised with
 * bank!=0 on real Kronos hardware. Reinterpreted here as genuine small
 * per-bank arrays (kMaxSmplMemBanks) with a defensive bank clamp, so a
 * bank!=0 caller gets safe, deterministic behaviour instead of reading
 * unrelated memory -- NOT a byte-identical reproduction of the ground-truth
 * linker layout (not reproducible, or even meaningful, with a different
 * toolchain/link order).
 */

#include "smpl_mem_manager.h"

#include <cstring>

namespace {

enum { kMaxSmplMemBanks = 4 };
unsigned long sRamSize[kMaxSmplMemBanks];
unsigned long sRamTop[kMaxSmplMemBanks];
unsigned long sRamFreeTop[kMaxSmplMemBanks];

inline unsigned char ClampBank(unsigned char bank)
{
	return (bank < kMaxSmplMemBanks) ? bank : 0;
}

inline unsigned char MsFlag(void *resolved) { return *((unsigned char *)resolved + 0x1a); }
inline bool SmplUsed(void *resolved) { return *(int *)((char *)resolved + 0x1c) != 0; }

int ScanFreeMultisample(CUsrMultisample &h, int from, int toExclusive)
{
	for (int i = from; i < toExclusive; ++i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) == 0)
			return i;
	}
	return -1;
}

int ScanFreeMultisampleDown(CUsrMultisample &h, int fromInclusive)
{
	for (int i = fromInclusive; i >= 0; --i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) == 0)
			return i;
	}
	return -1;
}

int ScanFreeSample(CUsrSample &h, int from, int toExclusive)
{
	for (int i = from; i < toExclusive; ++i) {
		h.Bless((unsigned int)i);
		if (!SmplUsed(h.mResolved))
			return i;
	}
	return -1;
}

int ScanFreeSampleDown(CUsrSample &h, int fromInclusive)
{
	for (int i = fromInclusive; i >= 0; --i) {
		h.Bless((unsigned int)i);
		if (!SmplUsed(h.mResolved))
			return i;
	}
	return -1;
}

} // namespace

CSmplMemManager *CSmplMemManager::theSmplMemManager = 0;

CSmplMemManager::CSmplMemManager()
	: mMsTop(0), mMsCount(0), mSmplTop(0), mSmplCount(0), mRltvCount(0),
	  mScratchBuf(CSmplModeMgr::theSmplModeMgr->mScratchBuf),
	  mHdFreeLo(0), mHdFreeHi(0)
{
	/* ground truth leaves +0x10/+0x14 (mHdFreeLo/mHdFreeHi) untouched --
	 * the real singleton bypasses this ctor entirely (see
	 * csmplmemmanagerstartup()) and mallocs raw, uninitialized storage.
	 * Zero-initializing here (unlike ground truth) is a deliberate, safe
	 * deviation for any OTHER instance that legitimately goes through this
	 * ctor. */
}

CSmplMemManager::~CSmplMemManager()
{
}

/* DEFERRED -- one-time boot bulk-init of 4000 CUsrMultisample + 16000
 * CUsrSample/CUsrDrumsample records' raw internal name fields, plus
 * establishing the RAM heap size. See smpl_mem_manager.h's class comment.
 * Real signature takes no arguments and returns nothing; the RAM-size part
 * (the only piece this class's OWN state depends on) is preserved via
 * setramsize(), which real callers can invoke directly. */
void CSmplMemManager::csmplmemmanagerstartup()
{
	/* TODO: not reconstructed -- see smpl_mem_manager.h. Ground truth also
	 * calls USTGAPIPCMBanks::InitUserSampling() here and Bless()es/zeroes
	 * every one of 4000 multisample + 16000 sample + 16000 drumsample slots'
	 * raw name fields directly, which needs full knowledge of those
	 * classes' real internal layout this project doesn't have. */
	USTGAPIPCMBanks::InitUserSampling();
}

void CSmplMemManager::setramsize()
{
	sRamSize[0] = USTGAPIPCMBanks::GetUsrPCMHeapSize();
	sRamFreeTop[0] = 0;
	sRamTop[0] = 0;
}

void CSmplMemManager::updateramsize(unsigned long newSize)
{
	if ((unsigned long)(sRamFreeTop[0] - sRamTop[0]) <= newSize)
		sRamSize[0] = newSize;
}

/* ---- Multisample (0..3999) slot allocation ---- */

void CSmplMemManager::getnewmsno(int wantSecond, short *outFirst, short *outSecond)
{
	CUsrMultisample h;
	h.Init();

	short firstIdx;
	short searchStartForSecond; /* ground truth "sVar1" */
	int deepScanBase;

	if (mMsTop < 4000) {
		firstIdx = mMsTop;
		*outFirst = firstIdx;
		searchStartForSecond = (short)(mMsTop + 1);
		deepScanBase = 0;
	} else {
		int found = ScanFreeMultisample(h, 0, 4000);
		firstIdx = (found >= 0) ? (short)found : (short)3999;
		*outFirst = firstIdx;
		/* ground truth: sVar1 = the ORIGINAL mMsTop, unconditionally --
		 * always >=4000 here, so the "<4000" shortcut below never fires
		 * in this branch. */
		searchStartForSecond = mMsTop;
		deepScanBase = (found >= 0) ? found : 3999;
	}

	if (!wantSecond) {
		*outSecond = firstIdx;
		return;
	}
	if (searchStartForSecond < 4000) {
		*outSecond = searchStartForSecond;
		return;
	}

	int second = (deepScanBase + 1 < 4000) ? ScanFreeMultisample(h, deepScanBase + 1, 4000) : -1;
	if (second >= 0) {
		*outSecond = (short)second;
	} else {
		*outSecond = 3999;
		if (*outFirst == 3999)
			*outFirst = (short)0xf9e; /* 3998, real ground-truth fallback constant */
	}
}

/* NOTE: unlike getnewmsno(), the "backward" (dec) sibling's outFirst/
 * outSecond end up SWAPPED (real ground-truth quirk, confirmed from the raw
 * pseudocode: the primary downward-scan result is written through
 * *param_3/outSecond, and only a SECOND downward scan -- searching further
 * down from just below the first hit -- feeds outFirst). */
void CSmplMemManager::getnewmsnodec(int wantSecond, short *outFirst, short *outSecond)
{
	CUsrMultisample h;
	h.Init();

	*outFirst = 3999;
	int found = ScanFreeMultisampleDown(h, 3999);
	int secondBase = (found >= 0) ? (found - 1) : -1;
	if (found >= 0)
		*outFirst = (short)found;

	if (!wantSecond) {
		*outSecond = -1;
		return;
	}
	*outSecond = *outFirst;
	*outFirst = 3999;
	if (found < 0 || secondBase < 0)
		return;

	int second = ScanFreeMultisampleDown(h, secondBase);
	if (second >= 0)
		*outFirst = (short)second;
}

void CSmplMemManager::incms(short msno)
{
	if (mMsTop <= msno)
		mMsTop = (short)(msno + 1);
	++mMsCount;
}

void CSmplMemManager::decms(short msno)
{
	if ((short)(mMsTop - 1) == msno) {
		CUsrMultisample h;
		short top = (short)(mMsTop - 1);
		mMsTop = top;
		while (top > 0) {
			h.Bless((unsigned int)(top - 1));
			if (MsFlag(h.mResolved) != 0) {
				--mMsCount;
				return;
			}
			top = (short)(mMsTop - 1);
			mMsTop = top;
		}
	}
	--mMsCount;
}

void CSmplMemManager::addms(short newTop)
{
	CUsrMultisample h;
	for (int i = mMsTop; i < newTop; ++i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) != 0)
			++mMsCount;
	}
	mMsTop = newTop;
}

void CSmplMemManager::clearms()
{
	mMsTop = 0;
	mMsCount = 0;
}

int CSmplMemManager::getfreemsnum(unsigned char *outPercent)
{
	int freeCount = 4000 - mMsCount;
	if (outPercent) {
		unsigned char pct = (unsigned char)((freeCount * 100) / 4000);
		*outPercent = pct;
		if (pct == 0 && freeCount != 0)
			*outPercent = 1;
	}
	return freeCount;
}

unsigned int CSmplMemManager::getslidermsno(short sliderPos)
{
	short count = mMsCount;
	short target = (short)(((int)sliderPos * (int)count) / 3999);
	if (count <= target)
		target = (short)(count - 1);

	CUsrMultisample h;
	for (unsigned int i = 0; i < 4000; ++i) {
		h.Bless(i);
		if (MsFlag(h.mResolved) != 0) {
			if (target == 0)
				return i;
			--target;
		}
	}
	return 3999; /* ground truth: last index examined, i.e. the loop's final iteration value */
}

/* ---- Sample/drumsample (0..15999) slot allocation ---- */

void CSmplMemManager::getnewsmplno(int wantSecond, short *outFirst, short *outSecond, short startFrom)
{
	if (startFrom == -1)
		startFrom = mSmplTop;

	short firstIdx;
	int deepScanBase;      /* ground truth "iVar4" */
	short secondFastPath;  /* ground truth "param_4", reassigned in the fast branch */

	if (startFrom < 16000) {
		firstIdx = startFrom;
		*outFirst = firstIdx;
		deepScanBase = 0;                        /* ground truth: iVar4 = 0, hardcoded */
		secondFastPath = (short)(startFrom + 1); /* ground truth: param_4 = param_4 + 1 */
	} else {
		*outFirst = 15999;
		firstIdx = 15999;
		int found = ScanFreeSample(USTGAPIPCMBanks::sSamp, 0, 16000);
		if (found >= 0) {
			*outFirst = (short)found;
			firstIdx = (short)found;
		}
		/* ground truth: iVar4 = foundIdx+1 on success, else 16000 (loop-exit bookkeeping value) */
		deepScanBase = (found >= 0) ? (found + 1) : 16000;
		secondFastPath = startFrom; /* stays >=16000, so the "<16000" shortcut below never fires */
	}

	if (!wantSecond) {
		*outSecond = firstIdx;
		return;
	}

	if (secondFastPath < 16000) {
		*outSecond = secondFastPath;
		return;
	}

	*outSecond = 15999;
	if (deepScanBase >= 16000)
		return;
	int second = ScanFreeSample(USTGAPIPCMBanks::sSamp, deepScanBase, 16000);
	if (second >= 0)
		*outSecond = (short)second;
}

void CSmplMemManager::getnewsmplnodec(int wantSecond, short *outFirst, short *outSecond, short startFrom)
{
	*outFirst = 15999;
	int firstFound = -1;
	if (startFrom >= 0)
		firstFound = ScanFreeSampleDown(USTGAPIPCMBanks::sSamp, startFrom);
	int secondBase = (firstFound >= 0) ? (firstFound - 1) : -1;
	if (firstFound >= 0)
		*outFirst = (short)firstFound;

	/* ground truth: outSecond only gets a real result if the primary scan
	 * found something (the "still occupied" check after the fast path is
	 * really "did the primary scan actually free something up"). */
	if (wantSecond == 0 || firstFound < 0) {
		*outSecond = -1;
		return;
	}

	*outSecond = 15999;
	if (secondBase < 0)
		return;
	int second = ScanFreeSampleDown(USTGAPIPCMBanks::sSamp, secondBase);
	if (second >= 0)
		*outSecond = (short)second;
}

void CSmplMemManager::incsmpl(short smplno)
{
	if (mSmplTop <= smplno)
		mSmplTop = (short)(smplno + 1);
	++mSmplCount;
}

void CSmplMemManager::decsmpl(short smplno)
{
	if ((short)(mSmplTop - 1) == smplno) {
		short top = (short)(mSmplTop - 1);
		mSmplTop = top;
		while (top > 0) {
			USTGAPIPCMBanks::sSamp.Bless((unsigned int)(top - 1));
			if (SmplUsed(USTGAPIPCMBanks::sSamp.mResolved)) {
				--mSmplCount;
				return;
			}
			top = (short)(mSmplTop - 1);
			mSmplTop = top;
		}
	}
	--mSmplCount;
}

void CSmplMemManager::addsmpl(short newTop)
{
	for (int i = mSmplTop; i < newTop; ++i) {
		USTGAPIPCMBanks::sSamp.Bless((unsigned int)i);
		if (SmplUsed(USTGAPIPCMBanks::sSamp.mResolved))
			++mSmplCount;
	}
	mSmplTop = newTop;
}

void CSmplMemManager::clearsmpl()
{
	mSmplTop = 0;
	mSmplCount = 0;
}

int CSmplMemManager::getfreesmplnum(unsigned char *outPercent)
{
	int freeCount = 16000 - mSmplCount;
	if (outPercent) {
		unsigned char pct = (unsigned char)((freeCount * 100) / 16000);
		*outPercent = pct;
		if (pct == 0 && freeCount != 0)
			*outPercent = 1;
	}
	return freeCount;
}

unsigned int CSmplMemManager::getslidersampleno(short sliderPos)
{
	short count = mSmplCount;
	short target = (short)(((int)sliderPos * (int)count) / 15999);
	if (count <= target)
		target = (short)(count - 1);

	for (unsigned int i = 0; i < 16000; ++i) {
		USTGAPIPCMBanks::sSamp.Bless(i);
		if (SmplUsed(USTGAPIPCMBanks::sSamp.mResolved)) {
			if (target == 0)
				return i;
			--target;
		}
	}
	return 15999;
}

/* ---- "Relative" (loop/attack point) slot bookkeeping ---- */

void CSmplMemManager::addrltv(short n) { mRltvCount = (short)(mRltvCount + n); }
void CSmplMemManager::decrltv(short n) { mRltvCount = (short)(mRltvCount - n); }
void CSmplMemManager::clearrltv() { mRltvCount = 0; }
unsigned short CSmplMemManager::getuserltvnum() { return (unsigned short)mRltvCount; }

int CSmplMemManager::getfreerltvnum(unsigned char *outPercent)
{
	int freeCount = 16000 - mRltvCount;
	if (outPercent) {
		unsigned char pct = (unsigned char)((freeCount * 100) / 16000);
		*outPercent = pct;
		if (pct == 0 && freeCount != 0)
			*outPercent = 1;
	}
	return freeCount;
}

/* ---- Name+rate stereo-pair matching ---- */

/* PARTIAL -- see smpl_mem_manager.h's class comment. Real fast path
 * (name strncmp + attack-count-equality, including the common "both unused"
 * trivial match) is faithful; the deep per-attack-point CUsrRel/CSmplPair
 * renegotiation loop (ground truth .text+0x08d63800-0x08d63a40) is stubbed
 * to conservatively report "not compatible". */
bool CSmplMemManager::multisamplecompare(CUsrMultisample *candidate, char *name, CUsrMultisample *target)
{
	char *candName = (char *)candidate->mResolved;
	if (strncmp(candName, name, 0x18) != 0)
		return false;

	unsigned char candFlag = MsFlag(candidate->mResolved);
	unsigned char targFlag = MsFlag(target->mResolved);
	if (candFlag != targFlag)
		return false;
	if (candFlag == 0)
		return true; /* both unused -- trivially compatible, ground truth's own fast exit */

	/* TODO: deep per-attack-point CUsrRel/CSmplPair stereo-pair
	 * renegotiation not reconstructed -- see class header comment. */
	return false;
}

bool CSmplMemManager::samplecompare(CUsrSample *candidate, char *name, CUsrSample *target)
{
	CUsrDrumsample d;
	d.Bless((unsigned int)*(unsigned short *)candidate->mResolved);
	char drumName[24];
	memcpy(drumName, d.mResolved, 24);
	if (strncmp(drumName, name, 24) != 0)
		return false;
	return candidate->GetSampleRate() == target->GetSampleRate();
}

/* ---- Linear stereo-partner search ---- */

/* Two-phase backward scan: phase 1 covers [floor, start-1] (nearest to
 * `start` first), phase 2 covers [start, ceil-1] (both, backward). Both
 * bound arguments are real/used here (unlike the CUsrSample sibling below).
 * Exact loop micro-structure not raw-disassembly-verified beyond the
 * overall direction/range (read from Ghidra pseudocode only) -- low stakes
 * in practice since multisamplecompare()'s own deep path is stubbed, so
 * this rarely returns a match either way; documented rather than silently
 * assumed. */
int CSmplMemManager::searchstereonoless(char *name, short start, CUsrMultisample *target, short floor, short ceil)
{
	CUsrMultisample h;
	h.Init();
	int result = -1;
	for (int i = start - 1; result < 0 && i >= floor; --i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) != 0 && multisamplecompare(&h, name, target))
			result = i;
	}
	for (int i = ceil - 1; result < 0 && i >= start; --i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) != 0 && multisamplecompare(&h, name, target))
			result = i;
	}
	h.Curse();
	return result;
}

/* Two-phase forward scan: phase 1 covers [start+1, ceil-1], phase 2 covers
 * [floor, start-1] (both forward). Same caveats as searchstereonoless()
 * above. */
int CSmplMemManager::searchstereonomore(char *name, short start, CUsrMultisample *target, short floor, short ceil)
{
	CUsrMultisample h;
	h.Init();
	int result = -1;
	for (int i = start + 1; result < 0 && i < ceil; ++i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) != 0 && multisamplecompare(&h, name, target))
			result = i;
	}
	for (int i = floor; result < 0 && i < start; ++i) {
		h.Bless((unsigned int)i);
		if (MsFlag(h.mResolved) != 0 && multisamplecompare(&h, name, target))
			result = i;
	}
	h.Curse();
	return result;
}

/* CONFIRMED via raw disassembly (Ghidra's own pseudocode mis-split the
 * parameters here -- it detected __cdecl with no `this`, since the body
 * never references `this`, but then mis-offset the stack reads, producing
 * garbled `_param_2`/`in_stack_...` names). Real layout, arg slots after
 * the prologue: [name][start][target][floor][ceil]. This function reads
 * only [name]/[start]/[target]/[floor] -- `ceil` (the 5th argument) is
 * NEVER referenced in the body, confirmed by reading through to `ret`. */
int CSmplMemManager::searchstereonoless(char *name, short start, CUsrSample *target, short floor, short /*unusedCeil*/)
{
	CUsrSample h;
	h.Init();
	int result = -1;
	for (int i = start - 1; result < 0 && i >= floor; --i) {
		h.Bless((unsigned int)i);
		if (SmplUsed(h.mResolved) && samplecompare(&h, name, target))
			result = i;
	}
	h.Curse();
	return result;
}

/* Real __thiscall, single forward scan [start+1, ceil-1]; `floor` (4th
 * argument) confirmed unused by ground truth's own (correctly-detected)
 * pseudocode. */
unsigned int CSmplMemManager::searchstereonomore(char *name, short start, CUsrSample *target, short /*unusedFloor*/, short ceil)
{
	CUsrSample h;
	h.Init();
	unsigned int result = 0xffffffff;
	for (int i = start + 1; (int)result == -1 && i < ceil; ++i) {
		h.Bless((unsigned int)i);
		if (SmplUsed(h.mResolved) && samplecompare(&h, name, target))
			result = (unsigned int)i;
	}
	h.Curse();
	return result;
}

/* ---- Stereo auto-pairing on record ---- */

/* DEFERRED -- see smpl_mem_manager.h's class comment. Real ground truth
 * builds a "-L"/"-R" partner name from the drum name at `msno` and searches
 * for it via searchstereonomore/searchstereonoless with a cross-argument
 * thread between this function's own 3 args and the inner searches' 5;
 * getting it bit-exact needs the same raw-disassembly-level tracing the
 * CUsrSample searchstereonoless() overload above needed, for both the "-L"
 * and "-R" branches. No-op stub -- callers (adjuststereomsnosub/
 * adjuststereomsno()) still run and call this faithfully by name. */
void CSmplMemManager::adjuststereomsno(short /*msno*/, short /*floor*/, short /*ceil*/)
{
	/* TODO: not reconstructed -- see smpl_mem_manager.h. */
}

void CSmplMemManager::adjuststereomsnosub(short floor, short ceil)
{
	for (short i = floor; i < ceil; ++i)
		adjuststereomsno(i, floor, ceil);
}

void CSmplMemManager::adjuststereomsno()
{
	for (short i = 0; i < 4000; ++i)
		adjuststereomsno(i, 0, 4000);
}

/* DEFERRED -- see adjuststereomsno() above and smpl_mem_manager.h. Partial
 * real findings from raw-disassembly tracing (see header comment) are
 * preserved here as a lead for a future pass rather than acted on, since
 * only the sample-family "-L"/"-R" branch's call ARGUMENTS were confirmed,
 * not both full branches end-to-end. */
void CSmplMemManager::adjuststereosampleno(short /*smplno*/, short /*floor*/, short /*ceil*/)
{
	/* TODO: not reconstructed -- see smpl_mem_manager.h. */
}

void CSmplMemManager::adjuststereosamplenosub(short floor, short ceil)
{
	for (short i = floor; i < ceil; ++i)
		adjuststereosampleno(i, floor, ceil);
}

void CSmplMemManager::adjuststereosampleno()
{
	for (short i = 0; i < 16000; ++i)
		adjuststereosampleno(i, 0, 16000);
}

/* ---- Per-bank bookkeeping ---- */

bool CSmplMemManager::isexistbank(unsigned char bank)
{
	return sRamSize[ClampBank(bank)] != 0;
}

unsigned long CSmplMemManager::getfreetop(unsigned char bank)
{
	return sRamFreeTop[ClampBank(bank)];
}

void CSmplMemManager::setfreetop(unsigned char bank, unsigned long value)
{
	sRamFreeTop[ClampBank(bank)] = value;
	USTGAPIPCMBanks::SetUsrPCMUsedSize(value);
}

int CSmplMemManager::getremainsize(unsigned char bank)
{
	unsigned char b = ClampBank(bank);
	if (sRamSize[b] == 0)
		return 0;
	return (int)((sRamTop[b] + sRamSize[b]) - sRamFreeTop[b]);
}

void CSmplMemManager::getremainsize(unsigned long *out)
{
	*out = (sRamSize[0] != 0) ? (sRamTop[0] + sRamSize[0]) - sRamFreeTop[0] : 0;
}

int CSmplMemManager::getremainsmpltimems(unsigned char bank, int highRate)
{
	unsigned char b = ClampBank(bank);
	if (sRamSize[b] == 0)
		return 0;
	unsigned int remain = (sRamSize[b] + sRamTop[b]) - sRamFreeTop[b];
	if (remain <= 0x3f)
		return 0;
	return highRate ? (int)(remain / 0xc0 + 1) : (int)(remain / 0x60 + 1);
}

unsigned int CSmplMemManager::getremainsmpltimeandsize(unsigned char bank, unsigned long *outKb, unsigned char *outPercent)
{
	unsigned char b = ClampBank(bank);
	unsigned int usedGap = (unsigned int)(sRamFreeTop[b] - sRamTop[b]);
	unsigned int remain = (usedGap == 0) ? (unsigned int)sRamSize[b] : (unsigned int)(sRamSize[b] - usedGap);

	unsigned int kb = remain >> 10;
	if (kb == 0 && remain != 0)
		kb = 1;
	*outKb = kb;

	if (sRamSize[b] == 0) {
		*outPercent = 0;
	} else {
		unsigned char pct = (unsigned char)(((unsigned long long)remain * 100) / sRamSize[b]);
		*outPercent = pct;
		if (pct != 0)
			return remain / 0x2580;
	}
	if (remain != 0)
		*outPercent = 1;
	return remain / 0x2580;
}

/* ---- Raw RAM byte-range operations ---- */

void CSmplMemManager::writedata(unsigned char /*bank*/, char *dst, char *src, unsigned long size)
{
	USTGAPIPCMBanks::RAMBankPCM_write(src, (unsigned long)dst & ~1ul, size & ~1ul);
}

void CSmplMemManager::readdata(unsigned char /*bank*/, char *dst, char *src, unsigned long size)
{
	USTGAPIPCMBanks::RAMBankPCM_read(dst, (unsigned long)src & ~1ul, size & ~1ul);
}

void CSmplMemManager::cutdata(unsigned char bank, unsigned long dstOffset, unsigned long srcOffset)
{
	unsigned long chunk = CSmplModeMgr::SAMPLINGMODEBUFFSIZE;
	unsigned long remain = sRamFreeTop[ClampBank(bank)] - srcOffset;
	if (remain == 0)
		return;
	unsigned long alignedChunk = chunk & ~1ul;
	for (;;) {
		if (remain < chunk) {
			USTGAPIPCMBanks::RAMBankPCM_read(mScratchBuf, srcOffset & ~1ul, remain & ~1ul);
			USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dstOffset & ~1ul, remain & ~1ul);
			return;
		}
		USTGAPIPCMBanks::RAMBankPCM_read(mScratchBuf, srcOffset & ~1ul, alignedChunk);
		remain -= chunk;
		USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dstOffset & ~1ul, alignedChunk);
		if (remain == 0)
			break;
		dstOffset += chunk;
		srcOffset += chunk;
	}
}

void CSmplMemManager::insertdata(unsigned char bank, unsigned long fromOffset, unsigned long toOffset)
{
	unsigned long chunk = CSmplModeMgr::SAMPLINGMODEBUFFSIZE;
	unsigned long src = sRamFreeTop[ClampBank(bank)];
	unsigned long remain = src - fromOffset;
	unsigned long step = chunk;
	while (remain != 0) {
		unsigned long next;
		if (remain < chunk) {
			step = remain;
			next = 0;
		} else {
			next = remain - step;
		}
		src -= step;
		USTGAPIPCMBanks::RAMBankPCM_read(mScratchBuf, src & ~1ul, step & ~1ul);
		USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, (toOffset + src) & ~1ul, step & ~1ul);
		remain = next;
	}

	memset(mScratchBuf, 0, chunk);

	unsigned long dst = fromOffset;
	unsigned long left = toOffset;
	if (left == 0)
		return;
	unsigned long alignedChunk = chunk & ~1ul;
	if (chunk <= left) {
		do {
			USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dst & ~1ul, alignedChunk);
			if (left == chunk)
				return;
			left -= chunk;
			dst += chunk;
		} while (chunk <= left);
	}
	USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dst & ~1ul, left & ~1ul);
}

void CSmplMemManager::dataclear(unsigned char /*bank*/, unsigned long offset, unsigned long size)
{
	unsigned long chunk = CSmplModeMgr::SAMPLINGMODEBUFFSIZE;
	memset(mScratchBuf, 0, chunk);

	if (size == 0)
		return;
	unsigned long alignedChunk = chunk & ~1ul;
	if (chunk <= size) {
		do {
			USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, offset & ~1ul, alignedChunk);
			if (chunk == size)
				return;
			size -= chunk;
			offset += chunk;
		} while (chunk <= size);
	}
	USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, offset & ~1ul, size & ~1ul);
}

void CSmplMemManager::copydata(unsigned char /*dstBank*/, unsigned long dstOffset, unsigned char /*srcBank*/, unsigned long srcOffset, unsigned long size)
{
	unsigned long chunk = CSmplModeMgr::SAMPLINGMODEBUFFSIZE;
	if (size == 0)
		return;
	unsigned long alignedChunk = chunk & ~1ul;
	for (;;) {
		if (size < chunk) {
			USTGAPIPCMBanks::RAMBankPCM_read(mScratchBuf, srcOffset & ~1ul, size & ~1ul);
			USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dstOffset & ~1ul, size & ~1ul);
			return;
		}
		USTGAPIPCMBanks::RAMBankPCM_read(mScratchBuf, srcOffset & ~1ul, alignedChunk);
		size -= chunk;
		USTGAPIPCMBanks::RAMBankPCM_write(mScratchBuf, dstOffset & ~1ul, alignedChunk);
		if (size == 0)
			break;
		dstOffset += chunk;
		srcOffset += chunk;
	}
}

/* Real static-local iterator state (C++ "magic statics" idiom in ground
 * truth -- __cxa_guard_acquire/__cxa_atexit); single-instance/non-reentrant,
 * exactly matching ground truth. Call with *ioReset!=0 to (re)start
 * iterating msno's relative slots, ==0 to continue from where it left off. */
unsigned int CSmplMemManager::getusedsampleno(int msno, int *outIndex, int *ioReset)
{
	static CUsrMultisample sMsPtr;
	static bool sMsPtrInit = false;
	static int sRelNo = 0;

	if (!sMsPtrInit) {
		sMsPtr.Init();
		sMsPtrInit = true;
	}

	if (*ioReset != 0) {
		*ioReset = 0;
		sRelNo = 0;
		sMsPtr.Bless((unsigned int)msno);
	}

	for (;;) {
		if (sRelNo >= (int)MsFlag(sMsPtr.mResolved))
			return 0xffffffff;

		unsigned short relStart = *(unsigned short *)((char *)sMsPtr.mResolved + 0x18);
		CUsrRel r;
		r.Bless((unsigned int)(relStart + sRelNo));
		++sRelNo;
		unsigned short sampleIdx = *(unsigned short *)r.mResolved;
		*outIndex = sRelNo;
		USTGAPIPCMBanks::sSamp.Bless((unsigned int)sampleIdx);
		if (SmplUsed(USTGAPIPCMBanks::sSamp.mResolved))
			return sampleIdx;
	}
}

void CSmplMemManager::refreshhdfreesize(EDevice_Id device)
{
	unsigned long long size = CDeviceDesc::get_media_free_size(device);
	mHdFreeLo = (unsigned int)size;
	mHdFreeHi = (int)(size >> 32);
}

unsigned int CSmplMemManager::gethdfreesize()
{
	if (mHdFreeHi < 1) {
		if (mHdFreeHi < 0)
			return mHdFreeLo;
		if (mHdFreeLo < 0x5265c071u)
			return mHdFreeLo;
	}
	return 0x5265c070u;
}

void CSmplMemManager::dechdfreesize(unsigned long amount)
{
	unsigned int before = mHdFreeLo;
	mHdFreeLo = before - (unsigned int)amount;
	if (before < amount)
		--mHdFreeHi;
}
