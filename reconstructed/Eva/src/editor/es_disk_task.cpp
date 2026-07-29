/*
 * es_disk_task.cpp  -  CESDiskTask method bodies (round 45, solo). See
 * include/es_disk_task.h for the full derivation.
 *
 * === Deferred, 2 distinct reasons (104/143 methods) ===
 * (1) 79 "index-gated dialog" accessors with 2-3 real param_1 branches
 *     (vs this round's own single-branch subset) or an unconditional
 *     trailing statement outside any branch (e.g. GetLoad1PatternDialog/
 *     GetLoad1SongDialog always touch one field regardless of param_1,
 *     THEN conditionally touch a second/third) -- fully concrete, no
 *     decompiler warning, genuinely tractable, just deferred for time
 *     budget this round; a clean target for a dedicated follow-up.
 * (2) 4 dtor variants, the 1767-byte ctor (needs the unrecovered
 *     descCESDiskTask* SDescriptor tables), GetFileName/SetFileName
 *     (500+ bytes, real string-buffer manipulation beyond this round's
 *     scope), and GetMakeAudioCD (1934 bytes) -- all deferred as
 *     genuinely larger, separate reconstruction efforts.
 */
#include <cstring>
#include "es_disk_task.h"

/* Real file-scope static globals this round's methods touch. Sizes for
 * the buffer ones (s_cDelFileName/s_cFolderName) match their own
 * CopyBytes() call sites' literal length argument (0xf0). */
static unsigned char s_ucFilerMsg;
static unsigned char s_ucProgress;
static unsigned char s_ucWriteRet;
static unsigned char s_ucMultipleSelect;
static unsigned char s_ucNotifyFileSelected;
static char s_ucDefaultFileName[256];
static unsigned short s_usDestBank;
static unsigned short sDestBankIndex;
static unsigned char s_ucRegionAlloc;
static unsigned char s_ucErCDTyp;
static unsigned char s_cDelFileName[0xf0];
static unsigned char s_cFolderName[0xf0];

unsigned char CLoadSampleDlogMgr::sm_caLoadFileName[0xec];

/* Minimal no-op stand-in (identity pass-through) -- CDiskUtil's own real
 * body is genuinely unrecovered (separate 59-method cluster, out of
 * scope), but Eva's `make verify` links every test against the FULL
 * object tree, so a bare declaration would break every OTHER test
 * binary too. See es_disk_task.h's own header comment. */
unsigned char CDiskUtil::WriteByteToSharedBuffer(unsigned char value)
{
	return value;
}

unsigned int CESDiskTask::GetFilerMsg(unsigned char, unsigned char *value) { *value = s_ucFilerMsg; return 1; }
unsigned int CESDiskTask::SetFilerMsg(unsigned char, const unsigned char *value) { s_ucFilerMsg = *value; return 1; }
unsigned int CESDiskTask::GetResultWriteExcl(unsigned char, unsigned char *value) { *value = s_ucWriteRet; return 1; }
unsigned int CESDiskTask::GetProgress(unsigned char, unsigned char *value) { *value = s_ucProgress; return 1; }
unsigned int CESDiskTask::SetProgress(unsigned char, const unsigned char *value) { s_ucProgress = *value; return 1; }
unsigned int CESDiskTask::GetMultipleSelect(unsigned char, unsigned char *value) { *value = s_ucMultipleSelect; return 1; }
unsigned int CESDiskTask::SetMultipleSelect(unsigned char, const unsigned char *value) { s_ucMultipleSelect = *value; return 1; }
unsigned int CESDiskTask::GetNotifyFileSelected(unsigned char, unsigned char *value) { *value = s_ucNotifyFileSelected; return 1; }
unsigned int CESDiskTask::SetNotifyFileSelected(unsigned char, const unsigned char *value) { s_ucNotifyFileSelected = *value; return 1; }
unsigned int CESDiskTask::SetWriteExcl(unsigned char, const unsigned char *value)
{
	s_ucWriteRet = CDiskUtil::WriteByteToSharedBuffer(*value);
	return 1;
}
char *CESDiskTask::GetDefaultFileName() { return s_ucDefaultFileName; }

unsigned int CESDiskTask::GetBankProgToWrite(unsigned char, char *value) const
{
	*(unsigned short *)value = (unsigned short)mBankToWrite * 0x80 + (unsigned short)mProgToWrite;
	return 1;
}

unsigned int CESDiskTask::GetBankProgToWriteFullRange(unsigned char, unsigned char *value) const
{
	*(unsigned short *)value = (unsigned short)mFullRangeBankToWrite * 0x80 + (unsigned short)mProgToWrite;
	return 1;
}

unsigned int CESDiskTask::GetBankCombiToWrite(unsigned char, char *value) const
{
	*(unsigned short *)value = (unsigned short)mCombiBankToWrite * 0x80 + (unsigned short)mCombiToWrite;
	return 1;
}

unsigned int CESDiskTask::GetOscTypeToWrite(unsigned char, unsigned char *value) const
{
	*(short *)value = (short)mOscTypeToWrite;
	return 1;
}

unsigned int CESDiskTask::SetOscTypeToWrite(unsigned char, const unsigned char *value)
{
	mOscTypeToWrite = (unsigned int)*value;
	return 1;
}

/* --- single-branch "index-gated dialog" family (real param_1=='\0' only) --- */
unsigned int CESDiskTask::GetLdCombiBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = s_usDestBank; return 1; }
unsigned int CESDiskTask::SetLdCombiBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_usDestBank = *(const unsigned short *)value; return 1; }
unsigned int CESDiskTask::GetLdDkitBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = sDestBankIndex; return 1; }
unsigned int CESDiskTask::SetLdDkitBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) sDestBankIndex = *(const unsigned short *)value; return 1; }
unsigned int CESDiskTask::GetLdKarmaGEBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = s_usDestBank; return 1; }
unsigned int CESDiskTask::SetLdKarmaGEBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_usDestBank = *(const unsigned short *)value; return 1; }
unsigned int CESDiskTask::GetLdTemplateBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = s_usDestBank; return 1; }
unsigned int CESDiskTask::SetLdTemplateBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_usDestBank = *(const unsigned short *)value; return 1; }
unsigned int CESDiskTask::GetLdProgBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = s_usDestBank; return 1; }
unsigned int CESDiskTask::SetLdProgBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_usDestBank = *(const unsigned short *)value; return 1; }
unsigned int CESDiskTask::GetLdWseqBankDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *(unsigned short *)value = s_usDestBank; return 1; }
unsigned int CESDiskTask::SetLdWseqBankDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_usDestBank = *(const unsigned short *)value; return 1; }

unsigned int CESDiskTask::GetLoadSampleDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) CopyBytes(value, CLoadSampleDlogMgr::sm_caLoadFileName, 0xec); return 1; }
unsigned int CESDiskTask::SetLoadSampleDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) CopyBytes(CLoadSampleDlogMgr::sm_caLoadFileName, value, 0xec); return 1; }

unsigned int CESDiskTask::GetLoadRegionsDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *value = s_ucRegionAlloc; return 1; }
unsigned int CESDiskTask::SetLoadRegionsDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_ucRegionAlloc = *value; return 1; }

unsigned int CESDiskTask::GetEraseCDRWDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) *value = s_ucErCDTyp; return 1; }
unsigned int CESDiskTask::SetEraseCDRWDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) s_ucErCDTyp = *value; return 1; }

unsigned int CESDiskTask::GetDeleteDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) CopyBytes(value, s_cDelFileName, 0xf0); return 1; }
unsigned int CESDiskTask::SetDeleteDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) CopyBytes(s_cDelFileName, value, 0xf0); return 1; }

unsigned int CESDiskTask::GetNewDirDialog(unsigned char param1, unsigned char *value)
{ if (param1 == 0) CopyBytes(value, s_cFolderName, 0xf0); return 1; }
unsigned int CESDiskTask::SetNewDirDialog(unsigned char param1, const unsigned char *value)
{ if (param1 == 0) CopyBytes(s_cFolderName, value, 0xf0); return 1; }

void CESDiskTask::CopyBytes(unsigned char *dst, const unsigned char *src, int len)
{
	if (len >= 1)
		memcpy(dst, src, (size_t)len);
	if (len <= 0xf0)
		dst[len] = '\0';
}
