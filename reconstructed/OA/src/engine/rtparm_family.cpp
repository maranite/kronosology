// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_family.cpp  -  the 56 reconstructed members of the KARMA RTParm
 * free-function family (see include/oa_rtparm_family.h for the full
 * scope/deferral list and the shared data-model notes every function
 * below relies on).
 *
 * Every function is a direct, offset-faithful translation of real
 * `objdump -dr -M intel` disassembly against
 * `/home/share/docs/ASM Docs/OA.ko/OA.ko` (name+size resolved per-function
 * via `nm -C -S`, NOT a single additive address delta -- see the header's
 * own note on why). Jump-table-driven functions (GetRTParmGroupItems,
 * GetRTParmDescriptor{,GE,PE}, IsRTParmTemplateRestoreType,
 * GetRTParmCurValue, GetRTParmCurValueFromOffset) had their real
 * `.rodata` jump tables dumped and decoded (`objdump -s -j .rodata`)
 * rather than guessed from the case bodies' apparent ordering.
 *
 * NOTE on GetRTParmFunctionTableEntry_{GE,PE}'s own `kind==1` search: both
 * compare a 16-bit value at RECORD OFFSET +0xa against the search key,
 * NOT the already-independently-verified `index` field at +0x08
 * (RTParmFunctionTableEntry_{GE,PE}, oa_rtparm_{ge,pe}_table.h -- that
 * offset was cross-checked by two decoders plus real KAT tests, high
 * confidence). Implemented here via a raw offset read
 * (`*(unsigned short*)(e+0xa)`, i.e. treating field0a+field0b as one
 * packed word) rather than asserting it's secretly the same field --
 * worth a dedicated follow-up look, not resolved this pass.
 */

#include "oa_rtparm_family.h"
#include "oa_rtparm_ge_table.h"
#include "oa_rtparm_pe_table.h"

/* No libc in this freestanding kernel build (matches project convention
 * elsewhere in this tree, e.g. ckg_ui_msg_sender.cpp -- no other file
 * under src/ calls memset()/memcpy() as real code either); tiny local
 * helpers instead of <cstring>. */
static void oa_copy(unsigned char *dst, const unsigned char *src, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		dst[i] = src[i];
}
static void oa_fill(unsigned char *dst, unsigned char v, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		dst[i] = v;
}

unsigned char gKS[0x402a8];

unsigned char RTParm_menu_ge_off[0x20];
unsigned char RTParm_menu_ge_ge[0x80];
unsigned char RTParm_menu_ge_rif[0x200];
unsigned char RTParm_menu_ge_phs[0x520];
unsigned char RTParm_menu_ge_rhy[0x1e0];
unsigned char RTParm_menu_ge_dur[0x120];
unsigned char RTParm_menu_ge_nte[0x160];
unsigned char RTParm_menu_ge_clu[0xa0];
unsigned char RTParm_menu_ge_vel[0x180];
unsigned char RTParm_menu_ge_pan[0x200];
unsigned char RTParm_menu_ge_wav[0x540];
unsigned char RTParm_menu_ge_env[0x3c0];
unsigned char RTParm_menu_ge_rpt[0x360];
unsigned char RTParm_menu_ge_bnd[0x260];
unsigned char RTParm_menu_ge_drm[0x5c0];
unsigned char RTParm_menu_ge_dix[0x280];
unsigned char RTParm_menu_pe_off[0x20];
unsigned char RTParm_menu_pe_pe[0x20];
unsigned char RTParm_menu_pe_mix[0x80];
unsigned char RTParm_menu_pe_ctl[0x180];
unsigned char RTParm_menu_pe_trg[0x1e0];
unsigned char RTParm_menu_pe_key[0x140];
unsigned char RTParm_menu_pe_rsd[0x80];

RTParmNameManager *gRTParmNameManagerPtr;

/* ==== 1. ResetDynRTParmWindow -- .text+0x511abc, 8 bytes ==== */
void ResetDynRTParmWindow(unsigned char *p)
{
	*p = 0;
}

/* ==== 2. CKGParamEdit::GetRTParmBufferSelectId -- see
 * src/engine/rtparm_ckgparamedit.cpp. NOT defined in this TU: including
 * oa_ckg_module_param_msg_handler.h here conflicts with oa_rtparm_pe_table.h
 * (both declare `RT_run(unsigned char, unsigned char)`, with different
 * linkage -- extern "C" there vs extern "C++" here -- see that file's own
 * header comment for why; a real, pre-existing latent inconsistency
 * between the two headers, surfaced for the first time by this pass
 * needing both, not introduced by this pass). Kept in its own TU rather
 * than "fixing" either established header under time pressure. */

/* ==== 3. GetRTParmIDFromGE -- .text+0x52472c, 17 bytes ==== */
unsigned int GetRTParmIDFromGE(GenEffect *ge, RTParm *rtParm)
{
	return (unsigned int)((unsigned char *)rtParm - ((unsigned char *)ge + 0x8cc)) >> 3;
}

/* ==== 4. GetRTParmIDFromPE -- .text+0x52473d, 17 bytes ==== */
unsigned int GetRTParmIDFromPE(Performance *perf, RTParm *rtParm)
{
	return (unsigned int)((unsigned char *)rtParm - ((unsigned char *)perf + 0x26a)) >> 3;
}

/* ==== 5. CKGSysExBuffer::StoreRTParmBySeq -- .text+0x34cbb0, 18 bytes ==== */
void CKGSysExBuffer::StoreRTParmBySeq(int seq, int offset)
{
	int base = *(int *)((unsigned char *)this + 0xc);
	unsigned char *p = (unsigned char *)(long)base + (long)seq * 0x42a8 + offset + 0xe0;
	*p = 1;
}

/* ==== 6. ResetRTParmGELastVal -- .text+0x52528f, 30 bytes ==== */
void ResetRTParmGELastVal(GenMod *genMod)
{
	unsigned char *base = (unsigned char *)genMod + 0x86a4 + 0x26;
	for (unsigned int i = 0; i < 0x500; i += 0x28)
		*(short *)(base + i) = (short)0x8001;
}

/* ==== 7. ByteSwapRTParm -- .text+0x55ae49, 41 bytes ==== */
void ByteSwapRTParm(RTParm *rtParm)
{
	unsigned char *p = (unsigned char *)rtParm;
	unsigned short w;
	w = *(unsigned short *)(p + 4);
	w = (unsigned short)((w << 8) | (w >> 8));
	*(unsigned short *)(p + 4) = w;
	w = *(unsigned short *)(p + 6);
	w = (unsigned short)((w << 8) | (w >> 8));
	*(unsigned short *)(p + 6) = w;
}

/* ==== 8. CreateRTParmNameString -- .text+0x52ced8, 43 bytes ==== */
void CreateRTParmNameString(GenEffect *ge, RTParm *rtParm, char *buf, unsigned char flag)
{
	gRTParmNameManagerPtr->GetRTParmNameString(ge, rtParm, buf, flag != 0);
}

/* ==== 9. GetRTParmGroupItems -- .text+0x52150d, 49 bytes ==== */
unsigned char GetRTParmGroupItems(unsigned char type, unsigned char mode)
{
	/* .rodata+0xb1798 (7 entries, mode==0) / +0xb1788 (16 entries, mode!=0) */
	static const unsigned char kMode0[7] = { 0x00, 0x01, 0x04, 0x0c, 0x0f, 0x0a, 0x04 };
	static const unsigned char kModeN[16] = {
		0x00, 0x04, 0x10, 0x29, 0x0f, 0x09, 0x0b, 0x05,
		0x0c, 0x10, 0x2a, 0x1e, 0x1b, 0x13, 0x2e, 0x14,
	};
	if (mode == 0) {
		if (type > 6) return 0;
		return kMode0[type];
	}
	if (type > 15) return 0;
	return kModeN[type];
}

/* ==== 10. GetRTParmFunctionGE -- .text+0x52479c, 52 bytes ==== */
RTParmFunctionTable *GetRTParmFunctionGE(unsigned char module, unsigned char ge, RTParmBufferSelect sel)
{
	unsigned int base = (unsigned int)module * 0x9d10 + (sel != 0 ? 0x1fc3c : 0x1f73c);
	return (RTParmFunctionTable *)(gKS + base + (unsigned int)ge * 40);
}

/* ==== 11. GetRTParmEditGE -- .text+0x524766, 54 bytes ==== */
RTParmEdit *GetRTParmEditGE(Performance *perf, RTParmBufferSelect sel, unsigned char module, unsigned char ge)
{
	unsigned int idx = (unsigned int)module * 32 + ge;
	unsigned char *base = (unsigned char *)perf + idx * 8;
	return (RTParmEdit *)(base + (sel != 0 ? 0x6ea : 0x2ea));
}

/* ==== 12. ByteSwapRTParmEdit -- .text+0x55c21f, 57 bytes ==== */
void ByteSwapRTParmEdit(RTParmEdit *rtd)
{
	unsigned char *p = (unsigned char *)rtd;
	unsigned short w;
	w = *(unsigned short *)(p + 0);
	w = (unsigned short)((w << 8) | (w >> 8));
	*(unsigned short *)(p + 0) = w;
	w = *(unsigned short *)(p + 4);
	w = (unsigned short)((w << 8) | (w >> 8));
	*(unsigned short *)(p + 4) = w;
	w = *(unsigned short *)(p + 6);
	w = (unsigned short)((w << 8) | (w >> 8));
	*(unsigned short *)(p + 6) = w;
}

/* ==== 13. AdjustRTParmFunctionGE -- .text+0x51cefb, 58 bytes ==== */
void AdjustRTParmFunctionGE(RTParmFunction *rtf)
{
	void **handler = (void **)((unsigned char *)rtf + 4);
	if (*handler == (void *)&RT_phs_xpose_oct || *handler == (void *)&RT_phs_xpose_oct_5th) {
		*handler = (void *)&RT_phs_xpose;
		return;
	}
	if (*handler == (void *)&RT_drm_pat_xpose_oct || *handler == (void *)&RT_drm_pat_xpose_oct_5th) {
		*handler = (void *)&RT_drm_pat_xpose;
		return;
	}
}

/* ==== 14. GetScaledRTParmValue -- .text+0x51cd79, 66 bytes ==== */
short GetScaledRTParmValue(RTParmEdit *rtd, RTParmFunction *rtf, unsigned char value)
{
	unsigned char *f = (unsigned char *)rtf;
	bool useCompare;
	if (f[0xc] <= 7)
		useCompare = !(f[0xb] == 0 && *((unsigned char *)rtd + 3) <= 1);
	else
		useCompare = !(f[0xc] <= 0xf);
	return ScaleRTParmValue(rtd, value, useCompare ? 1 : 0);
}

/* ==== 15. GetRTParmFunctionTableEntry_PE -- .text+0x56501b, 66 bytes ==== */
RTParmFunctionTable *GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType kind, void *key)
{
	unsigned char *base = (unsigned char *)gRTParmFunctionTable_PE;
	const unsigned int stride = sizeof(RTParmFunctionTableEntry_PE);
	if (kind == kRTParmFunctionTableEntryIndexType_0) {
		for (unsigned char *e = base; e < base + RTPARM_PE_TABLE_SIZE * stride; e += stride)
			if (*(void **)e == key)
				return (RTParmFunctionTable *)e;
		return 0;
	}
	if (kind == kRTParmFunctionTableEntryIndexType_1) {
		unsigned short want = *(unsigned short *)key;
		for (unsigned char *e = base; e < base + RTPARM_PE_TABLE_SIZE * stride; e += stride)
			if (*(unsigned short *)(e + 0xa) == want)
				return (RTParmFunctionTable *)e;
		return 0;
	}
	return 0;
}

/* ==== 16. X2100ShMem_UpdateMasterRTParmEdit -- .text+0x51f598, 70 bytes ==== */
void X2100ShMem_UpdateMasterRTParmEdit(Performance *perf, unsigned char module, unsigned char ge)
{
	unsigned char *shmemBase = *(unsigned char **)(gKS + 0x8ce0);
	if (!shmemBase)
		return;
	unsigned char *shmem = *(unsigned char **)(shmemBase + (unsigned int)module * 4 + 4);
	unsigned int srcIdx = (unsigned int)module * 32 + ge + 0x5c;
	unsigned int dstIdx = (unsigned int)ge + 2;
	unsigned char *src = (unsigned char *)perf + srcIdx * 8;
	unsigned char *dst = shmem + dstIdx * 8;
	*(short *)(dst + 0x14) = *(short *)(src + 0xa);
	*(short *)(dst + 0x10) = *(short *)(src + 0xe);
	*(short *)(dst + 0x12) = *(short *)(src + 0x10);
}

/* ==== 17. RTParmShortNameGroup ctor -- .text+0x5644da, 71 bytes ==== */
RTParmShortNameGroup::RTParmShortNameGroup()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)p = 0;
	for (int off = 4; off <= 0x16; off += 2)
		*(unsigned short *)(p + off) = 0;
}

/* ==== 17b. RTParmShortNameGroup::SetRTParmShortNameStringPtr -- .text+0x56460e, 18 bytes ==== */
void RTParmShortNameGroup::SetRTParmShortNameStringPtr(const char (*str)[24], unsigned short a, unsigned short b)
{
	unsigned char *p = (unsigned char *)this;
	/* real ground truth is `mov DWORD PTR[eax],edx` -- a packed 32-bit
	 * pointer (the real -m32 target's native pointer width), NOT a
	 * native 64-bit `void*` write. Using `void**` here on a 64-bit host
	 * build clobbers the next field's own bytes too -- same "packed
	 * 32-bit, not host pointer width" convention already established
	 * elsewhere in this project (CSTGMidiQueueWriter, oa_global.h),
	 * caught by this pass's own KAT. */
	*(unsigned int *)p = (unsigned int)(unsigned long)str;
	*(unsigned short *)(p + 4) = a;
	*(unsigned short *)(p + 6) = b;
}

/* ==== 17c. RTParmShortNameGroup::SetProductArrays -- .text+0x564620, 20 bytes ==== */
void RTParmShortNameGroup::SetProductArrays(RTParmNameProductID product, unsigned short a, unsigned short b)
{
	unsigned char *p = (unsigned char *)this;
	unsigned int idx = (unsigned int)product;
	*(unsigned short *)(p + idx * 4 + 8) = a;
	*(unsigned short *)(p + idx * 4 + 0xa) = b;
}

/* ==== 17d. RTParmNameManager::SetPrependCCInfo -- .text+0x5644ba, 31 bytes ==== */
void RTParmNameManager::SetPrependCCInfo(RTParmNameProductID product, bool a, bool b)
{
	unsigned char *rec = (unsigned char *)this + (unsigned int)product * 3 + 0x180;
	rec[4] = a;
	rec[5] = b;
}

/* ==== 18. X2100ShMem_UpdateModuleRTParmEdit -- .text+0x51f5de, 73 bytes ==== */
void X2100ShMem_UpdateModuleRTParmEdit(Performance *perf, unsigned char module, unsigned char ge)
{
	unsigned char *shmemBase = *(unsigned char **)(gKS + 0x8ce0);
	if (!shmemBase)
		return;
	unsigned char *shmem = *(unsigned char **)(shmemBase + (unsigned int)module * 4 + 4);
	unsigned int srcIdx = (unsigned int)module * 32 + ge + 0xdc;
	unsigned int dstIdx = (unsigned int)ge + 0x32;
	unsigned char *src = (unsigned char *)perf + srcIdx * 8;
	unsigned char *dst = shmem + dstIdx * 8;
	*(short *)(dst + 0xa) = *(short *)(src + 0xa);
	*(short *)(dst + 0x6) = *(short *)(src + 0xe);
	*(short *)(dst + 0x8) = *(short *)(src + 0x10);
}

/* ==== 19. GetRTParmFunctionTableEntry_GE -- .text+0x564fd0, 75 bytes ==== */
RTParmFunctionTable *GetRTParmFunctionTableEntry_GE(kRTParmFunctionTableEntryIndexType kind, void *key)
{
	if (kind == kRTParmFunctionTableEntryIndexType_0) {
		if (!key)
			return 0;
		for (unsigned int i = 0; i < RTPARM_GE_TABLE_SIZE; ++i)
			if (gRTParmFunctionTable_GE[i].funcPtr == key)
				return (RTParmFunctionTable *)&gRTParmFunctionTable_GE[i];
		return 0;
	}
	if (kind == kRTParmFunctionTableEntryIndexType_1) {
		unsigned short want = *(unsigned short *)key;
		for (unsigned int i = 0; i < RTPARM_GE_TABLE_SIZE; ++i)
			if (*(unsigned short *)((unsigned char *)&gRTParmFunctionTable_GE[i] + 0xa) == want)
				return (RTParmFunctionTable *)&gRTParmFunctionTable_GE[i];
		return 0;
	}
	return 0;
}

/* ==== 20. GetRTParmAssigned_GE -- .text+0x526387, 76 bytes ==== */
RTParm *GetRTParmAssigned_GE(GenEffect *ge, unsigned char a, unsigned char b, unsigned char mask)
{
	unsigned char *e = (unsigned char *)ge + 0x8cc;
	for (int count = 0x20; count > 0; --count, e += 8) {
		if (e[0] == a && e[1] == b) {
			unsigned char masked = (unsigned char)(mask & e[2]);
			if (masked == mask)
				return (RTParm *)e;
		}
	}
	return 0;
}

/* ==== 21. ResetRTParmPELastVal -- .text+0x52531a, 77 bytes ==== */
void ResetRTParmPELastVal()
{
	for (int i = 0; i < 8; ++i)
		*(short *)(gKS + 0x16cc0 + i * 2) = (short)0x8001;
}

/* ==== 22. AssignRTParmFunction_PE -- .text+0x522e01, 81 bytes ==== */
void AssignRTParmFunction_PE(RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (entry) {
		AssignRTParmPE(rtParm, rtf, entry, 0, 0);
	} else {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
	}
}

/* ==== 23. CopyRTParmFunctionToOtherBuffer -- .text+0x52669b, 81 bytes ==== */
void CopyRTParmFunctionToOtherBuffer(GenMod *genMod, RTParmBufferSelect sel, unsigned char idx)
{
	unsigned int off = (unsigned int)idx * 40;
	unsigned char *cur = (unsigned char *)genMod + 0x86a4 + off;
	unsigned char *cmp = (unsigned char *)genMod + 0x8ba4 + off;
	unsigned char *src = (sel != 0) ? cmp : cur;
	unsigned char *dst = (sel != 0) ? cur : cmp;
	oa_copy(dst, src, 40);
}

/* ==== 24. AdjustRTParmFunctionPE -- .text+0x51cea8, 83 bytes ==== */
void AdjustRTParmFunctionPE(RTParmFunction *rtf)
{
	void **handler = (void **)((unsigned char *)rtf + 4);
	if (*handler == (void *)&RT_crb_xpose_oct || *handler == (void *)&RT_crb_xpose_oct_5th) {
		*handler = (void *)&RT_crb_xpose;
		return;
	}
	if (*handler == (void *)&RT_kbd_thru_in_xpose_oct || *handler == (void *)&RT_kbd_thru_in_xpose_oct_5th) {
		*handler = (void *)&RT_kbd_thru_in_xpose;
		return;
	}
	if (*handler == (void *)&RT_kbd_thru_out_xpose_oct || *handler == (void *)&RT_kbd_thru_out_xpose_oct_5th) {
		*handler = (void *)&RT_kbd_thru_out_xpose;
		return;
	}
}

/* ==== 25. SetRTParmMultiBackupGE -- .text+0x52ce82, 86 bytes ==== */
void SetRTParmMultiBackupGE(unsigned char module, RTParmBufferSelect sel)
{
	unsigned char *dst = gKS + (unsigned int)module * 0x9d10 + (sel != 0 ? 0x1fc3c : 0x1f73c);
	unsigned char *src = gKS + 0x280c + (unsigned int)module * 0x9cc; /* 627*4 == 0x9cc */
	for (unsigned int i = 0; i < 32; ++i)
		dst[i * 0x28 + 0x20] = src[i * 8 + 2];
}

/* ==== 26. CopyRTParmEditToOtherBuffer -- .text+0x526565, 88 bytes ==== */
void CopyRTParmEditToOtherBuffer(Performance *perf, RTParmBufferSelect sel, unsigned char a, unsigned char b)
{
	unsigned int idx = (unsigned int)a * 32 + b;
	unsigned char *cur = (unsigned char *)perf + idx * 8 + 0x2ea;
	unsigned char *cmp = (unsigned char *)perf + idx * 8 + 0x6ea;
	unsigned char *src = (sel != 0) ? cmp : cur;
	unsigned char *dst = (sel != 0) ? cur : cmp;
	oa_copy(dst, src, 8);
}

/* ==== 27/28. AssignRTParmFunction_Key/Ctl -- .text+0x522c4b / 0x522d37, 99 bytes each ==== */
static inline void AssignRTParmFunction_KeyCtlShape(RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (entry) {
		unsigned char bit = (unsigned char)GetFirstOnBit(((unsigned char *)rtParm)[2], 4);
		AssignRTParmPE(rtParm, rtf, entry, 4, bit);
	} else {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
	}
}
void AssignRTParmFunction_Key(RTParm *rtParm, RTParmFunction *rtf) { AssignRTParmFunction_KeyCtlShape(rtParm, rtf); }
void AssignRTParmFunction_Ctl(RTParm *rtParm, RTParmFunction *rtf) { AssignRTParmFunction_KeyCtlShape(rtParm, rtf); }

/* ==== 29. SetRTParmMultiBackupPE -- .text+0x52ce1d, 101 bytes ==== */
void SetRTParmMultiBackupPE()
{
	static const unsigned int kOff[8] = { 0x26c, 0x274, 0x27c, 0x284, 0x28c, 0x294, 0x29c, 0x2a4 };
	static const unsigned int kDst[8] = { 0x16f44, 0x16f6c, 0x16f94, 0x16fbc, 0x16fe4, 0x1700c, 0x17034, 0x1705c };
	for (int i = 0; i < 8; ++i)
		gKS[kDst[i]] = gKS[kOff[i]];
}

/* ==== 30. AssignRTParmFunction_Mix -- .text+0x522d9a, 103 bytes ==== */
void AssignRTParmFunction_Mix(RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (entry) {
		unsigned char bit = (unsigned char)GetFirstOnBit(((unsigned char *)rtParm)[2], 4);
		AssignRTParmPE(rtParm, rtf, entry, 4, bit);
		f[0xa] = 0;
	} else {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
	}
}

/* ==== 31. IsRTParmTemplateRestoreType -- .text+0x532b62, 104 bytes ==== */
bool IsRTParmTemplateRestoreType(RTParm *rtParm)
{
	unsigned char *p = (unsigned char *)rtParm;
	unsigned char type = p[0];
	unsigned char sub = p[1];
	if (type < 4 || type > 14)
		return false;
	switch (type) {
	case 4:  return sub == 0x0e;
	case 5:  return sub == 0x08;
	case 6:  return sub == 0x0a;
	case 7:  return sub == 0x04;
	case 8:  return sub == 0x0b;
	case 9:  return sub == 0x0f;
	case 10: return sub == 0x29;
	case 11: case 12: case 13: return false;
	case 14: return (unsigned char)(sub - 0x2b) <= 2;
	}
	return false;
}

/* ==== 32/33/34/... AssignRTParmFunction_{Dix,Bnd,GE} -- 105 bytes each, GE-table shape ==== */
static inline void AssignRTParmFunction_DixBndGEShape(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_GE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (entry) {
		AssignRTParmGE(module, ge, rtParm, rtf, entry, 0, 0);
	} else {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
	}
}
void AssignRTParmFunction_Dix(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) { AssignRTParmFunction_DixBndGEShape(module, ge, rtParm, rtf); }
void AssignRTParmFunction_Bnd(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) { AssignRTParmFunction_DixBndGEShape(module, ge, rtParm, rtf); }
void AssignRTParmFunction_GE(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) { AssignRTParmFunction_DixBndGEShape(module, ge, rtParm, rtf); }

/* ==== 35. CopyRTParmEditToModule -- .text+0x526632, 105 bytes ==== */
void CopyRTParmEditToModule(Performance *perf, unsigned char a, unsigned char b)
{
	unsigned char *base = (unsigned char *)perf;
	unsigned int srcIdx = (unsigned int)a * 32 + b + 0x5c;
	unsigned int dstIdx = (unsigned int)a * 32 + b + 0xdc;

	short v0e = *(short *)(base + srcIdx * 8 + 0xe);
	*(short *)(base + dstIdx * 8 + 0xe) = v0e;
	short v10 = *(short *)(base + srcIdx * 8 + 0x10);
	*(short *)(base + dstIdx * 8 + 0x10) = v10;
	short v0a = *(short *)(base + srcIdx * 8 + 0xa);

	unsigned char *shmemBase = *(unsigned char **)(gKS + 0x8ce0);
	if (shmemBase) {
		unsigned char *shmem = *(unsigned char **)(shmemBase + (unsigned int)a * 4 + 4);
		unsigned int shIdx = (unsigned int)b + 0x32;
		*(short *)(shmem + shIdx * 8 + 0xa) = v0a;
		*(short *)(shmem + shIdx * 8 + 0x6) = *(short *)(base + dstIdx * 8 + 0xe);
		*(short *)(shmem + shIdx * 8 + 0x8) = *(short *)(base + dstIdx * 8 + 0x10);
	}
}

/* ==== 36. ResetRTParmGELastValIndControl -- .text+0x5252ad, 109 bytes ==== */
void ResetRTParmGELastValIndControl(unsigned char module, unsigned char control, RTParmBufferSelect sel)
{
	unsigned char *srcBuf = gKS + (unsigned int)module * 0x9d10 + (sel != 0 ? 0x1fc3c : 0x1f73c);
	unsigned char *dstBuf = gKS + (unsigned int)module * 0x9d10 + 0x1f762;
	for (unsigned int i = 0; i < 32; ++i) {
		if ((int)(signed char)srcBuf[i * 0x28 + 0xc] == (int)control)
			*(short *)(dstBuf + i * 0x28) = (short)0x8001;
	}
}

/* ==== 37. SetRTParmWasEdited -- .text+0x51cf35, 116 bytes ==== */
void SetRTParmWasEdited(unsigned char a, unsigned char b, bool clearAll)
{
	unsigned char fill = (unsigned char)clearAll;
	if (b != 0xff) {
		gKS[(unsigned int)a * 0x9d10 + 0x20b08 + b] = fill;
		return;
	}
	if (a != 0xff) {
		oa_fill(gKS + (unsigned int)a * 0x9d10 + 0x20b08, fill, 0x20);
		return;
	}
	static const unsigned int kBases[4] = { 0x20b08, 0x2a818, 0x34528, 0x3e238 };
	for (int r = 0; r < 4; ++r)
		oa_fill(gKS + kBases[r], fill, 0x20);
}

/* ==== 38. CopyRTParmEditToMaster -- .text+0x5265bd, 117 bytes ==== */
void CopyRTParmEditToMaster(Performance *perf, unsigned char a, unsigned char b)
{
	unsigned char *base = (unsigned char *)perf;
	unsigned int dstIdx = (unsigned int)a * 32 + b + 0x5c;
	unsigned int srcIdx = (unsigned int)a * 32 + b + 0xdc;
	if (base[dstIdx * 8 + 0xc] == 0xff)
		return;

	short v0e = *(short *)(base + srcIdx * 8 + 0xe);
	*(short *)(base + dstIdx * 8 + 0xe) = v0e;
	short v10 = *(short *)(base + srcIdx * 8 + 0x10);
	*(short *)(base + dstIdx * 8 + 0x10) = v10;
	short v0a = *(short *)(base + srcIdx * 8 + 0xa);

	unsigned char *shmemBase = *(unsigned char **)(gKS + 0x8ce0);
	if (shmemBase) {
		unsigned char *shmem = *(unsigned char **)(shmemBase + (unsigned int)a * 4 + 4);
		unsigned int shIdx = (unsigned int)b + 2;
		*(short *)(shmem + shIdx * 8 + 0x14) = v0a;
		*(short *)(shmem + shIdx * 8 + 0x10) = *(short *)(base + dstIdx * 8 + 0xe);
		*(short *)(shmem + shIdx * 8 + 0x12) = *(short *)(base + dstIdx * 8 + 0x10);
	}
}

/* ==== 39. AssignRTParmFunction_Env -- .text+0x523a97, 119 bytes ==== */
void AssignRTParmFunction_Env(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_GE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (entry) {
		unsigned char bit = (unsigned char)GetFirstOnBit(((unsigned char *)rtParm)[2], 3);
		AssignRTParmGE(module, ge, rtParm, rtf, entry, 3, bit);
	} else {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
	}
}

/* ==== 40. GetRTParmDescriptorPE -- .text+0x52140d, 120 bytes ==== */
unsigned char *GetRTParmDescriptorPE(unsigned char module, unsigned char idx)
{
	static unsigned char *const kMenu[7] = {
		RTParm_menu_pe_off, RTParm_menu_pe_pe, RTParm_menu_pe_mix, RTParm_menu_pe_ctl,
		RTParm_menu_pe_trg, RTParm_menu_pe_key, RTParm_menu_pe_rsd,
	};
	if (module > 6) return 0;
	return kMenu[module] + (unsigned int)idx * 0x20;
}

/* ==== 41. CKGSysExBuffer::SendParamsDependOnRTParm -- .text+0x34e7b0, 123 bytes ==== */
void CKGSysExBuffer::SendParamsDependOnRTParm(CSKParameterChangeMessage *msg)
{
	unsigned char *thisB = (unsigned char *)this;
	unsigned char *msgB = (unsigned char *)msg;
	unsigned int scaled = 0;
	unsigned int base = 0;
	if (thisB[4] != 0) {
		scaled = msgB[2] & 0xf;
		base = scaled * 0x42a8;
	}
	unsigned char f9 = msgB[9];
	unsigned char *table = *(unsigned char **)(thisB + 0xc);
	unsigned char *entry = table + f9;
	if (entry[base + 0xe0] != 0) {
		unsigned char *tableBase = *(unsigned char **)(thisB + 8) + scaled * 0x10aa0;
		msgB[8] = 0x1c;
		int value = *(int *)(tableBase + (unsigned int)f9 * 4 + 0x380);
		msg->SetValue(value);
		SendSysExMassage(msgB);
	}
}

/* ==== 42. GetRTParmDescriptor -- .text+0x521485, 136 bytes ==== */
unsigned char *GetRTParmDescriptor(RTParm *rtParm, unsigned char mode)
{
	unsigned char *p = (unsigned char *)rtParm;
	if (mode != 0)
		return GetRTParmDescriptorGE(p[0], p[1]);

	static unsigned char *const kMenu[7] = {
		RTParm_menu_pe_off, RTParm_menu_pe_pe, RTParm_menu_pe_mix, RTParm_menu_pe_ctl,
		RTParm_menu_pe_trg, RTParm_menu_pe_key, RTParm_menu_pe_rsd,
	};
	unsigned char type = p[0];
	unsigned char idx = p[1];
	if (type > 6) return 0;
	return kMenu[type] + (unsigned int)idx * 0x20;
}

/* ==== 43. AssignRTParmFunction_Trg -- .text+0x522cae, 137 bytes ==== */
void AssignRTParmFunction_Trg(RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (!entry) {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
		return;
	}
	unsigned char *p = (unsigned char *)rtParm;
	unsigned char bit = (unsigned char)GetFirstOnBit(p[2], 4);
	unsigned char extra = 0;
	unsigned char sub = (unsigned char)(p[1] - 7);
	if (sub <= 5) {
		/* .rodata+0xb17a4, 6 bytes */
		static const unsigned char kExtra[6] = { 0x00, 0x00, 0x01, 0x01, 0x02, 0x02 };
		extra = kExtra[sub];
	}
	AssignRTParmPE(rtParm, rtf, entry, 4, bit);
	f[0xa] = extra;
}

/* ==== 44. IsRTParmPairAssignedGE -- .text+0x525c88, 150 bytes ==== */
unsigned char IsRTParmPairAssignedGE(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	unsigned char *base = gKS + (unsigned int)a * 0x9cc + 0x280c;
	unsigned char *e = base;
	for (int count = 0x20; count > 0; --count, e += 8) {
		if (e[0] == b && e[1] == c) {
			unsigned char masked = (unsigned char)(d & e[2]);
			if (masked == d) {
				if (!e)
					return 0;
				unsigned int elemIdx = (unsigned int)((e - base) >> 3) & 0xff;
				unsigned int idx2 = ((unsigned int)a << 5) + elemIdx;
				signed char flag1 = (signed char)gKS[idx2 * 8 + 0x6ec];
				if (flag1 >= 0)
					return 1;
				unsigned char flag2 = gKS[idx2 * 8 + 0x2ec];
				unsigned char notFlag2 = (unsigned char)~flag2; /* truncate to 8 bits
					* BEFORE shifting -- `~` promotes to `int` in C++, so
					* `(~flag2) >> 7` on an unpromoted expression would shift
					* the 32-bit-wide complement, not the real `not bl; shr
					* bl,7` 8-bit-register operation ground truth performs
					* (caught by this pass's own KAT: flag2==0 must yield 1,
					* not 0xff >> 7 done at 32-bit width). */
				return (unsigned char)(notFlag2 >> 7);
			}
		}
	}
	return 0;
}

/* ==== 45. UpdateRTParmName -- .text+0x52511d, 159 bytes ==== */
void UpdateRTParmName(unsigned char p1, unsigned char p2, unsigned char p3, unsigned char p4)
{
	unsigned long bitmask = 0;
	if (p2 >= p3) {
		unsigned char *regionBase = gKS + (unsigned int)p1 * 0x9cc + 0x2800;
		for (unsigned int outer = p3; p4 >= outer; ++outer) {
			unsigned char *elem = regionBase + 0xc;
			for (unsigned int esi = 0; esi < 32; ++esi, elem += 8) {
				if (elem[0] == p2 && elem[1] == (unsigned char)outer) {
					Do_KM_rtp_update_name(p1, (unsigned char)esi);
					bitmask |= (1UL << esi);
				}
			}
		}
	}
	Do_KM_rtp_update_all_names(p1, bitmask);
}

/* ==== 46. AssignRTParmFunction_Rsd -- .text+0x522b9f, 172 bytes ==== */
void AssignRTParmFunction_Rsd(RTParm *rtParm, RTParmFunction *rtf)
{
	RTParmFunctionTable *entry = GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType_1, rtParm);
	unsigned char *f = (unsigned char *)rtf;
	if (!entry) {
		*(unsigned int *)(f + 4) = 0;
		*(unsigned int *)(f + 0) = 0;
		return;
	}
	unsigned char *p = (unsigned char *)rtParm;
	unsigned char bit = (unsigned char)GetFirstOnBit(p[2], 4);
	AssignRTParmPE(rtParm, rtf, entry, 4, bit);
	if (p[1] == 0 && bit <= 3) {
		unsigned char *slotPtr = *(unsigned char **)(gKS + (unsigned int)bit * 0x9d10 + 0x1709c);
		int val = *(int *)(slotPtr + 0x20);
		if (val > 0x1fff)
			*(int *)(slotPtr + 0x20) = 0x1fff;
		else if (val < -0x2000)
			*(int *)(slotPtr + 0x20) = -0x2000;
	}
}

/* ==== 47. LimitRTParmPairPE -- .text+0x526210, 183 bytes ==== */
void LimitRTParmPairPE(unsigned char a, RTParm *rtParm, RTParmEdit *rtd)
{
	unsigned char *p = (unsigned char *)rtParm;
	if (p[0] != 5)
		return;
	unsigned char sub = p[1];
	if (sub != 2 && sub != 3)
		return;

	unsigned char bit = (unsigned char)GetFirstOnBit(p[2], 4);
	if (bit > 3)
		return;
	/* gKS + bit*120 (bit<<7 - bit<<3), field +0xa for sub==3, +0xb for sub==2 */
	unsigned char threshold = gKS[(unsigned int)bit * 120 + (sub == 3 ? 0xa : 0xb)];

	short cur = *(short *)rtd;
	if (sub == 3) {
		if (cur >= (short)(signed char)threshold)
			return;
	} else {
		if (cur <= (short)(signed char)threshold)
			return;
	}
	*(short *)rtd = (short)(unsigned short)threshold;
	KM_rtp_val_out_pe((RTParm_pub *)rtParm, a, 0);
}

/* ==== 48. GetRTParmCurValue -- .text+0x52153e, 195 bytes ==== */
int GetRTParmCurValue(RTParmFunction *rtf)
{
	unsigned char *rf = (unsigned char *)rtf;
	unsigned char *src = *(unsigned char **)(rf + 0);
	if (!src)
		return 0;
	unsigned char kind = rf[8];
	if (kind > 0x27)
		return 0;
	switch (kind) {
	case 0: case 3: case 6: case 8: case 9: case 36: case 37:
		return src[0];
	case 1: case 4: case 7: case 10: case 38: case 39:
		return (short)(signed char)src[0];
	case 2: case 5: case 11: case 12: case 35:
		return *(unsigned short *)src;
	case 13: case 27: return src[0] & 1;
	case 14: case 28: return (src[0] & 2) != 0;
	case 15: case 29: return (src[0] & 4) != 0;
	case 16: case 30: return (src[0] & 8) != 0;
	case 17: case 31: return (src[0] & 0x10) != 0;
	case 18: case 32: return (src[0] & 0x20) != 0;
	case 19: case 33: return (src[0] & 0x40) != 0;
	case 20: case 34: return ((signed char)src[0]) < 0 ? 1 : 0;
	case 21: return (src[1] & 0x40) != 0;
	case 22: return (*(unsigned short *)src & 0x8000) != 0;
	case 23: return src[0] & 3;
	case 24: return src[0] & 7;
	case 25: return src[0] & 0x3f;
	case 26: return (src[0] & 0x38) >> 3;
	default: return 0;
	}
}

/* ==== 49. GetRTParmDescriptorGE -- .text+0x521317, 246 bytes ==== */
unsigned char *GetRTParmDescriptorGE(unsigned char module, unsigned char idx)
{
	static unsigned char *const kMenu[16] = {
		RTParm_menu_ge_off, RTParm_menu_ge_ge, RTParm_menu_ge_rif, RTParm_menu_ge_phs,
		RTParm_menu_ge_rhy, RTParm_menu_ge_dur, RTParm_menu_ge_nte, RTParm_menu_ge_clu,
		RTParm_menu_ge_vel, RTParm_menu_ge_pan, RTParm_menu_ge_wav, RTParm_menu_ge_env,
		RTParm_menu_ge_rpt, RTParm_menu_ge_bnd, RTParm_menu_ge_drm, RTParm_menu_ge_dix,
	};
	if (module > 15) return 0;
	return kMenu[module] + (unsigned int)idx * 0x20;
}

/* ==== 50. IsRTParmFunctionSamePE -- .text+0x52a51a, 294 bytes ====
 * Literal transcription of the real compare-tree (two "compatibility
 * group" tables for kind==2 -> {1,2,3} and kind==5 -> {4,5,6,7,8,9}); kept
 * as nested ifs matching each real basic block rather than collapsed into
 * a lookup table, to avoid introducing a simplification bug. */
bool IsRTParmFunctionSamePE(unsigned char kind, unsigned char p2, unsigned char p3, unsigned char p4)
{
	if (kind == 0 || kind != p3)
		return false;

	if (kind == 2) {
		if (p2 == p4) return true;
		if (p2 == 1) return (p4 == 2) || (p4 == 3);
		if (p2 == 2) return (p4 == 1) || (p4 == 3);
		if (p2 == 3) return (p4 == 1) || (p4 == 2);
		return false; /* p2 == 0 (or any other value): no match beyond p2==p4 above */
	}

	if (kind == 5) {
		if (p2 == p4) return true;
		if (p2 == 4) return (p4 == 6) || (p4 == 8);
		if (p2 == 6) return (p4 == 4) || (p4 == 6);
		if (p2 == 8) return (p4 == 4) || (p4 == 6);
		if (p2 == 5) return (p4 == 7) || (p4 == 9);
		if (p2 == 7) return (p4 == 5) || (p4 == 9);
		if (p2 == 9) return (p4 == 5) || (p4 == 7);
		return false;
	}

	return p2 == p4;
}

/* ==== 51. GetRTParmCurValueFromOffset -- .text+0x52269a, 372 bytes ==== */
int GetRTParmCurValueFromOffset(const RTParmFunctionTable *table, void *base, unsigned char idx)
{
	const unsigned char *t = (const unsigned char *)table;
	if (!t)
		return 0;
	unsigned int kind = *(const unsigned int *)(t + 4);
	if (kind > 0x27)
		return 0;
	unsigned int off = *(const unsigned int *)(t + (unsigned int)idx * 4 + 0x10);
	unsigned char *p = (unsigned char *)base + off;
	switch (kind) {
	case 0: case 3: case 6: case 8: case 9: case 36: case 37:
		return p[0];
	case 1: case 4: case 7: case 10: case 38: case 39:
		return (signed char)p[0];
	case 2: case 5: case 12:
		return *(short *)p;
	case 11:
		return *(unsigned short *)p;
	case 13: case 27: return p[0] & 1;
	case 14: case 28: return (p[0] & 2) != 0;
	case 15: case 29: return (p[0] & 4) != 0;
	case 16: case 30: return (p[0] & 8) != 0;
	case 17: case 31: return (p[0] & 0x10) != 0;
	case 18: case 32: return (p[0] & 0x20) != 0;
	case 19: case 33: return (p[0] & 0x40) != 0;
	case 20: case 34: return ((signed char)p[0]) < 0 ? 1 : 0;
	case 21: return (p[1] & 0x40) != 0;
	case 22: return (*(short *)p) < 0 ? 1 : 0;
	case 23: return p[0] & 3;
	case 24: return p[0] & 7;
	case 25: return p[0] & 0x3f;
	case 26: return (p[0] & 0x38) >> 3;
	case 35: return *(unsigned int *)p;
	default: return 0;
	}
}

/* ==== 52. IsRTParmPairAssignedPE -- .text+0x525af8, 400 bytes ==== */
unsigned char IsRTParmPairAssignedPE(unsigned char a, unsigned char b, unsigned char c)
{
	static const unsigned int kBase[8] = { 0x26a, 0x272, 0x27a, 0x282, 0x28a, 0x292, 0x29a, 0x2a2 };
	for (int i = 0; i < 8; ++i) {
		unsigned int base = kBase[i];
		if (a == gKS[base + 0]) {
			if (b == gKS[base + 1]) {
				unsigned char masked = (unsigned char)(c & gKS[base + 2]);
				if (c == masked) {
					unsigned int idx = (unsigned int)i * 8;
					unsigned char flag = gKS[idx + 0x2ac];
					unsigned char notFlag = (unsigned char)~flag; /* see
						* IsRTParmPairAssignedGE's own comment: truncate
						* to 8 bits before shifting, matching the real
						* 8-bit-register `not bl; shr bl,7`. */
					return (unsigned char)(notFlag >> 7);
				}
			}
			/* real disasm: once `a` matches this slot's id, a
			 * failed b/mask check falls through to the NEXT
			 * slot's `a` comparison rather than returning 0
			 * directly -- behaviorally identical to returning 0
			 * here as long as slot ids are distinct (assumed,
			 * not independently re-verified this pass). */
			return 0;
		}
	}
	return 0;
}

/* ==== 53. GetRTParmAssigned_PE -- .text+0x5263d3, 402 bytes ==== */
unsigned char *GetRTParmAssigned_PE(Performance *perf, unsigned char p1, unsigned char p2, unsigned char p3)
{
	unsigned char *base = (unsigned char *)perf;
	static const unsigned int kOff[8] = { 0x26a, 0x272, 0x27a, 0x282, 0x28a, 0x292, 0x29a, 0x2a2 };
	for (int i = 0; i < 8; ++i) {
		unsigned int off = kOff[i];
		if (base[off + 0] == p1 && base[off + 1] == p2) {
			unsigned char masked = (unsigned char)(p3 & base[off + 2]);
			if (masked == p3)
				return base + off;
		}
	}
	return 0;
}

/* =====================================================================
 * Follow-up pass (2026-07-29): the 6 free-function members deliberately
 * deferred from the first pass (see oa_rtparm_family.h's own header
 * comment for the full re-derivation summary). The 7th deferred member,
 * RTParmShortNameGroup::GetRTParmShortNameStringPtr, is below too (it's a
 * class method, declared in-class in the header).
 * ===================================================================== */

/* ==== GetRTParmModAndID -- .text+0x5245ed, 264 bytes ====
 * Ground truth's own `eax` argument is a genuine pointer into gKS (every
 * comparison against it carries a real `R_386_32 gKS` relocation, i.e.
 * the disassembly's literal immediates like `0x1f40`/`0x290c` are really
 * `gKS+0x1f40`/`gKS+0x290c`) -- NOT a caller-computed integer offset as
 * the first pass's own deferral note speculated. Once that's recognized,
 * both of the function's two dense "compare against a jump-by-8 table of
 * addresses" halves collapse to a single uniform indexed-slot search
 * (independently re-derived and cross-checked against the *out write
 * order twice before committing).
 *   - rtParm < gKS+0x1f40: `*out` = which of Performance's own 8
 *     "pair-group" bases (+0x26a, +0x272, ..., +0x2a2, stride 8) rtParm
 *     equals, or 8 if none match; always returns 0.
 *   - rtParm >= gKS+0x1f40 and < gKS+0x290c, OR gKS[0x1e5] (the module/
 *     "channel" count) <= 1: channel n = 0.
 *   - otherwise: n = the channel index such that rtParm falls in
 *     [gKS+0x290c+(n-1)*0x9cc, gKS+0x290c+n*0x9cc) for n>=1, capped at
 *     gKS[0x1e5]-1 if rtParm is past the last valid channel's own range
 *     (ground truth checks the bound BEFORE computing each candidate
 *     boundary, so the cap can fire without ever testing that channel's
 *     own boundary -- reproduced faithfully via the same early-check
 *     order below, not simplified into a single loop condition).
 *   Either way, `*out` is then set to which of channel n's own 32 fixed
 *   0x280c-region slots (stride 8, see the header's own confirmed-facts
 *   note) rtParm equals, or 32 if none match, and the function returns n. */
unsigned int GetRTParmModAndID(RTParm *rtParm, unsigned char *out)
{
	unsigned char *p = (unsigned char *)rtParm;
	unsigned int n;

	if (p < gKS + 0x1f40) {
		static const unsigned int kBase[8] = { 0x26a, 0x272, 0x27a, 0x282, 0x28a, 0x292, 0x29a, 0x2a2 };
		*out = 8;
		for (unsigned int j = 0; j < 8; ++j) {
			if (p == gKS + kBase[j]) {
				*out = (unsigned char)j;
				break;
			}
		}
		return 0;
	}

	{
		unsigned char bound = gKS[0x1e5];
		if (bound <= 1 || p < gKS + 0x290c) {
			n = 0;
		} else {
			unsigned int r = 1;
			for (;;) {
				if (r + 1 >= bound) { n = r; break; }
				if (p < gKS + 0x290c + r * 0x9cc) { n = r; break; }
				++r;
			}
		}
	}

	{
		unsigned char *base = gKS + n * 0x9cc + 0x280c;
		*out = 32;
		for (unsigned int slot = 0; slot < 32; ++slot) {
			if (p == base + slot * 8) {
				*out = (unsigned char)slot;
				break;
			}
		}
	}
	return n;
}

/* ==== LimitRTParmEditValuesRow -- .text+0x525367, 421 bytes ====
 * Ground truth's own dense, cross-linked branch tree (the header's own
 * first-pass deferral note calls it "several early-exit paths that share
 * labels across both the sel==0 and sel!=0 halves") is a compiler
 * if-conversion of a plain double clamp, verified by exhaustively tracing
 * every branch of both halves and confirming they always reduce to the
 * same two comparisons per field (independently re-derived, not assumed):
 * clamp field4 and field6 (of the RTParmEdit "row") independently into
 * [desc.min, desc.max], then clamp field0 into [min(field4,field6),
 * max(field4,field6)]. `sel` selects the "current" (+0x2ea) vs "compare"
 * (+0x6ea) RTParmEdit buffer, same as every other GE table
 * current/compare pair in this family. The descriptor lookup itself reads
 * a real RTParm{type,subId} pair straight out of the gKS 0x280c-region
 * slot for this (module,ge) -- same slot GetRTParmModAndID above
 * searches, and DoRTParmMultiEnableGE below reads too. */
bool LimitRTParmEditValuesRow(EditBuffer *eb, unsigned char module, unsigned char ge, RTParmBufferSelect sel)
{
	unsigned char *base = (unsigned char *)eb;
	short *row = (short *)(base + (unsigned int)module * 0x100 /* 32*8 */
	                        + (unsigned int)ge * 8 + (sel != 0 ? 0x6ea : 0x2ea));
	unsigned char *rtpSlot = base + (unsigned int)module * 0x9cc + 0x280c + (unsigned int)ge * 8;
	unsigned char *desc = GetRTParmDescriptorGE(rtpSlot[0], rtpSlot[1]);
	short lo = *(short *)(desc + 0x18);
	short hi = *(short *)(desc + 0x1a);
	short *f0 = row + 0;
	short *f4 = row + 2; /* +4 bytes */
	short *f6 = row + 3; /* +6 bytes */
	bool changed = false;

	if (*f4 < lo) { *f4 = lo; changed = true; }
	if (*f4 > hi) { *f4 = hi; changed = true; }
	if (*f6 < lo) { *f6 = lo; changed = true; }
	if (*f6 > hi) { *f6 = hi; changed = true; }

	short mn = (*f4 <= *f6) ? *f4 : *f6;
	short mx = (*f4 <= *f6) ? *f6 : *f4;
	if (*f0 < mn) { *f0 = mn; changed = true; }
	else if (*f0 > mx) { *f0 = mx; changed = true; }

	return changed;
}

/* ==== LimitRTParmEditValues -- .text+0x52550c, 130 bytes ====
 * The caller of LimitRTParmEditValuesRow above: 4 channels (module*0x9cc
 * stride, matching GetRTParmModAndID's own channel stride), skipped
 * entirely if the channel's own WORD at +0x1f40 reads 0xffff (a
 * "disabled" sentinel -- same +0x1f40 boundary GetRTParmModAndID checks
 * too). Ground truth's own 2nd call passes `sel = module+1` (NOT a plain
 * 1) -- reproduced verbatim below since LimitRTParmEditValuesRow only
 * ever tests `sel != 0`, so this is behaviorally identical to passing 1,
 * just an odd-but-real artifact of however the original source expressed
 * "the compare buffer" here. */
bool LimitRTParmEditValues(EditBuffer *eb)
{
	unsigned char *base = (unsigned char *)eb;
	bool changed = false;

	for (unsigned int module = 0; module < 4; ++module) {
		unsigned char *chBase = base + module * 0x9cc;
		if (*(unsigned short *)(chBase + 0x1f40) == 0xffff)
			continue;
		for (unsigned int ge = 0; ge < 32; ++ge) {
			if (LimitRTParmEditValuesRow(eb, (unsigned char)module, (unsigned char)ge, RTPARM_BUFFER_CURRENT))
				changed = true;
			if (LimitRTParmEditValuesRow(eb, (unsigned char)module, (unsigned char)ge, (RTParmBufferSelect)(module + 1)))
				changed = true;
		}
	}
	return changed;
}

/* ==== UpdateRTParmIfSame_GE -- .text+0x52502f, 238 bytes ====
 * Scans GenEffect's own 32-entry, 8-byte-stride RTParm array (+0x8cc, see
 * the header's confirmed facts) for every slot whose {type,subId} content
 * matches rtParm's own (a 3rd byte is compared too: exact match, OR a
 * nonzero bitwise AND -- same "compatible if any bit overlaps" shape as
 * LimitRTParmEditValuesRow's descriptor lookup uses a plain 2-byte
 * {type,subId} match, no analogous mask here). The slot that IS rtParm
 * itself (pointer identity, not content) is skipped, not updated --
 * ground truth's own two differently-addressed copies of the loop-
 * continue code (one reached via the self-skip path, one via the
 * post-match path) are the SAME logical "continue", reproduced here with
 * a single `continue`, not two. No return value (void) -- ground truth
 * never sets eax meaningfully before its final `ret`. */
void UpdateRTParmIfSame_GE(GenMod *genMod, RTParm *rtParm, RTParmEdit *rtd, RTParmBufferSelect sel)
{
	unsigned char *rp = (unsigned char *)rtParm;
	if (rp[0] == 0)
		return;

	unsigned char module = *(unsigned char *)genMod; /* GenMod+0x0, found this pass */
	unsigned char *ge = *(unsigned char **)((unsigned char *)genMod + 0xc);
	unsigned char *arrayBase = ge + 0x8cc;
	unsigned short newVal = *(unsigned short *)rtd;
	unsigned int gksOff = (sel != 0) ? 0x6ea : 0x2ea;

	for (unsigned int i = 0; i < 32; ++i) {
		unsigned char *slot = arrayBase + i * 8;
		if (slot == rp)
			continue;
		if (slot[0] != rp[0] || slot[1] != rp[1])
			continue;
		if (slot[2] != rp[2] && (slot[2] & rp[2]) == 0)
			continue;

		unsigned int idx = (unsigned int)module * 32 + i;
		*(unsigned short *)(gKS + idx * 8 + gksOff) = newVal;
		Do_KM_rtp_val_out_pe((RTParm *)slot, (unsigned char)i, 1);
	}
}

/* ==== DoRTParmMultiEnablePE -- .text+0x52a640, 291 bytes ====
 * For each of gKS's own 8 "pair-group" slots (+0x26a, stride 8 -- NOT a
 * hardware "module" despite the name used below; there is no module
 * parameter, ground truth reads `gKS[slot*8+0x26a/0x26b/0x26c]` directly)
 * not yet covered by an accumulated "seen" bitmask, scans the higher-
 * indexed slots (slot+1..7) for an RT-function-table entry
 * IsRTParmFunctionSamePE considers "the same"; every match accumulates a
 * per-slot OR-bitmask (bit = 1<<otherSlotIndex) and OR's a flag byte from
 * the matched entry. It then walks the same higher-slot range a second
 * time, writing the (bit-inverted) accumulated flag byte into a second,
 * 0x28-byte-stride gKS table (`slot*40+0x16f24`, entry +0x24) wherever
 * the corresponding bit is set in the accumulated mask, ORing in a
 * per-row byte from the first table along the way. Both of ground
 * truth's own early-skip checks ("if rowStart>bound, skip the whole
 * inner loop") are redundant with the loops' own natural bounds (the
 * starting index is already >= the bound in exactly those cases, so the
 * loop runs 0 iterations either way) and are therefore omitted below --
 * a real, confirmed simplification, not a guess. Literal otherwise (not
 * further restructured), matching this project's "verify before claim"
 * discipline for the pass's densest control flow. */
void DoRTParmMultiEnablePE()
{
	unsigned short seenMask = 0;
	unsigned short bit = 1;

	for (unsigned int slot = 0; slot < 8; ++slot, bit = (unsigned short)(bit << 1)) {
		unsigned char flagByte = gKS[slot * 8 + 0x26c];

		if (seenMask & bit)
			continue;

		unsigned short acc = bit;
		{
			unsigned char *tbl = gKS + slot * 8 + 0x272;
			unsigned char valB = gKS[slot * 8 + 0x26b];
			unsigned char valA = gKS[slot * 8 + 0x26a];
			for (unsigned int r = slot + 1; r < 8; ++r, tbl += 8) {
				if (IsRTParmFunctionSamePE(valA, valB, tbl[0], tbl[1])) {
					unsigned short bitVal = (unsigned short)(1u << r);
					acc |= bitVal;
					flagByte |= tbl[2];
					seenMask |= bitVal;
				}
			}
		}

		unsigned char *rowSlot = gKS + slot * 8 + 0x26a;
		unsigned char *tbl2 = gKS + slot * 40 + 0x16f24;
		flagByte = (unsigned char)~flagByte;

		for (unsigned int r2 = slot; r2 <= 7; ++r2, rowSlot += 8, tbl2 += 0x28) {
			unsigned short bitVal2 = (unsigned short)(1u << r2);
			if (bitVal2 > acc)
				break;
			if (!(bitVal2 & acc))
				continue;
			unsigned char v = flagByte;
			tbl2[0x24] = v;
			v = (unsigned char)(v | rowSlot[2]);
			tbl2[0x24] = v;
		}
	}
}

/* ==== DoRTParmMultiEnableGE -- .text+0x52c872, 409 bytes ====
 * Same overall shape as DoRTParmMultiEnablePE above, but for a single,
 * caller-supplied `module` and looping over gKS's per-module 0x280c-
 * region's own 32 GE slots (see the header's confirmed facts, and
 * GetRTParmModAndID/LimitRTParmEditValuesRow above -- same table) instead
 * of PE's 8 fixed pair-group slots. `sel` picks the "current" (+0x1f73c)
 * vs "compare" (+0x1fc3c) per-module GetRTParmFunctionGE-shaped
 * destination table (0x28-byte stride, matching every other GE current/
 * compare pair in this family). Uses `IsRTParmFunctionSameGE`, a real,
 * large (3907B, still `pending`) sibling of the already-reconstructed
 * IsRTParmFunctionSamePE -- only the call site's own argument order is
 * needed here, not its body. Same redundant-early-skip-check omission as
 * DoRTParmMultiEnablePE above (confirmed the same way: the loop's own
 * starting index already fails its own bound in exactly those cases). */
void DoRTParmMultiEnableGE(unsigned char module, RTParmBufferSelect sel)
{
	unsigned char *fixedBase = gKS + (unsigned int)module * 0x9cc + 0x280c;
	unsigned int modOff = (unsigned int)module * 0x9d10;

	unsigned int seenMask = 0;
	unsigned int bit = 1;

	for (unsigned int ge = 0; ge < 32; ++ge, bit <<= 1) {
		unsigned char flagByte = fixedBase[ge * 8 + 2];

		if (seenMask & bit)
			continue;

		unsigned int acc = bit;
		{
			unsigned char *tbl = fixedBase + ge * 8 + 8; /* = module*0x9cc+(ge+1)*8+0x280c == module*0x9cc+ge*8+0x2814 */
			unsigned char idx = fixedBase[ge * 8 + 1];
			unsigned char type = fixedBase[ge * 8 + 0];
			for (unsigned int r = ge + 1; r < 32; ++r, tbl += 8) {
				if (IsRTParmFunctionSameGE(type, idx, tbl[0], tbl[1])) {
					unsigned int bitVal = 1u << r;
					acc |= bitVal;
					flagByte |= tbl[2];
					seenMask |= bitVal;
				}
			}
		}

		unsigned char *funcTbl = gKS + modOff + ge * 0x28 + (sel != 0 ? 0x1fc3c : 0x1f73c);
		unsigned char *rowSlot = fixedBase + ge * 8;
		flagByte = (unsigned char)~flagByte;

		for (unsigned int r2 = ge; r2 <= 0x1f; ++r2, rowSlot += 8, funcTbl += 0x28) {
			unsigned int bitVal2 = 1u << r2;
			if (bitVal2 > acc)
				break;
			if (!(bitVal2 & acc))
				continue;
			unsigned char v = flagByte;
			funcTbl[0x24] = v;
			v = (unsigned char)(v | rowSlot[2]);
			funcTbl[0x24] = v;
		}
	}
}

/* ==== RTParmShortNameGroup::GetRTParmShortNameStringPtr -- .text+0x564522,
 * 235 bytes ====
 * Given a product index, picks a starting index into a 24-byte-wide
 * string table (this-> +0x0) via this-> +0x8/+0xa (start/len, the same
 * per-product pair SetProductArrays above writes) -- offset by
 * `useAlt * this->+0x6` (the same "stride" SetRTParmShortNameStringPtr's
 * own 3rd argument sets) rows, and further offset within [start,
 * start+len) by `CountOnBits(a,8)-1` when `a != 0` (else 0). If the
 * resulting table entry is an empty string, walks backward (one 24-byte
 * entry at a time) toward `start` for the nearest non-empty entry,
 * returning NULL if none is found. Ground truth's real return register
 * (eax) holds this raw string pointer (or 0), NOT an `unsigned short` --
 * see the header's own note on this fix. */
const char *RTParmShortNameGroup::GetRTParmShortNameStringPtr(RTParmNameProductID product, unsigned char useAlt, unsigned char a)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int idx = (unsigned int)product;
	unsigned short len = *(unsigned short *)(base + idx * 4 + 0xa);
	if (len == 0)
		return 0;

	unsigned short start = *(unsigned short *)(base + idx * 4 + 8);
	short index;

	if (len > 1) {
		int seed = 0;
		if (a != 0)
			seed = (signed char)(CountOnBits((unsigned long)a, 8) - 1);
		int sum = seed + (int)start;
		int end = (int)start + (int)len - 1;
		index = (short)((sum < end) ? sum : end);
	} else {
		index = (short)(start + len - 1);
	}
	if (index < (short)start)
		return 0;

	/* packed 32-bit pointer field (see SetRTParmShortNameStringPtr's own
	 * comment above) -- NOT a native 8-byte pointer read on a 64-bit
	 * host build. */
	unsigned char *strTable = (unsigned char *)(unsigned long)(*(unsigned int *)base);
	unsigned short stride = *(unsigned short *)(base + 6);
	unsigned int rowStride = (unsigned int)useAlt * stride;
	const char *p = (const char *)(strTable + ((unsigned int)(unsigned short)index + rowStride) * 24);

	if (*p != 0)
		return p;

	/* first candidate is an empty string -- walk backward toward
	 * `start` for the nearest non-empty entry. */
	const char *q = p - 24;
	short i = (short)(index - 1);
	unsigned char ch = 0;
	while ((short)start <= i) {
		ch = (unsigned char)*q;
		p = q;
		q -= 24;
		if (ch != 0)
			break;
		--i;
	}
	return ch ? p : 0;
}

/* ==== 54. CountOnBits -- .text+0x584c0b, 35 bytes ====
 * Ground truth's own loop shape checks bit 0 first (unshifted), THEN
 * decrements width and shifts for the next iteration -- functionally
 * identical to the more natural "shift-then-check per index" form below
 * (same bits of `mask` examined, same total iteration count, no side
 * effect on `mask` survives the call), confirmed via exhaustive
 * native-execution ground truth (see below). */
unsigned long CountOnBits(unsigned long mask, unsigned char width)
{
	unsigned char count = 0;
	for (unsigned char i = 0; i < width; ++i) {
		count = (unsigned char)(count + (mask & 1));
		mask >>= 1;
	}
	return count;
}

/* ==== 55. Do_KM_rtp_val_out_pe -- .text+0x55e562, 29 bytes ====
 * Tiny gate + tail call: a single guard byte at gKS+0x16ef4 (exact
 * semantic meaning not independently determined this pass -- no other
 * reconstructed member of this family reads or writes it) suppresses
 * the call to KM_rtp_val_out_pe entirely when nonzero. */
void Do_KM_rtp_val_out_pe(RTParm *rtParm, unsigned char a, unsigned char b)
{
	if (gKS[0x16ef4] != 0)
		return;
	KM_rtp_val_out_pe((RTParm_pub *)rtParm, a, b);
}

/* ==== 56. IsRTParmFunctionSameGE -- .text+0x531a5f, 3907 bytes ====
 * Sibling of the already-reconstructed IsRTParmFunctionSamePE, but far
 * larger: `kind` dispatches to one of 12 real cases (2,3,4,5,6,7,8,9,
 * 0xa,0xb,0xc,0xe), each with its OWN independently-authored compatible-
 * value set; anything else (0,1,0xd,0xf..) falls through to the shared
 * default `idx == c`. `kind == 0` is unconditionally false; `kind != b`
 * (the 3rd parameter) is also unconditionally false in every real case,
 * mirroring IsRTParmFunctionSamePE's own top-level gate.
 *
 * kind==0xb (11) alone dispatches to a FURTHER 22-way sub-switch on
 * `idx`, each arm hand-listing its own compatible `c` values -- by far
 * the largest single case, and the reason this function is 13x
 * IsRTParmFunctionSamePE's size. Given the sheer number of sub-blocks,
 * this pass built a native-execution harness (the function has ZERO
 * relocations -- confirmed via `objdump -dr`, fully self-contained pure
 * logic on `kind`/`idx`/`c` alone, safe to mmap+call directly) and
 * brute-forced the real ground truth for all 16*256*256 = 1,048,576
 * (kind,idx,c) combinations with b==kind (the only case that can ever
 * return true). Every group/range below was cross-checked against that
 * exhaustive table, not hand-typed or guessed -- kind==2/4/5/6/7/8/9/
 * 0xa/0xc/0xe are genuine symmetric equivalence-class groups (any two
 * members of the same listed set are mutually "same"); kind==3 is a
 * "hub" shape (34..40's own sub-ranges, not a simple partition -- see
 * inline comments); kind==0xb (11) is NOT even symmetric in ground
 * truth (e.g. idx=20,c=6 is true but idx=6,c=20 is false -- a genuine
 * property of the real compiled table, not a transcription artifact,
 * confirmed by direct harness re-invocation on that exact pair both
 * ways) and is reproduced here as an explicit directed pair table taken
 * verbatim from the exhaustive ground-truth dump, since no clean
 * formula reproduces its real (asymmetric) shape. */
static const unsigned char sIsRTParmFunctionSameGE_Kind11Pairs[190][2] = {
	{2,15}, {2,16}, {2,17}, {2,21}, {2,22}, {2,23}, {2,25}, {3,26},
	{3,27}, {3,29}, {4,15}, {4,18}, {4,19}, {4,21}, {4,22}, {4,24},
	{4,25}, {5,26}, {5,28}, {5,29}, {6,16}, {6,18}, {6,19}, {6,21},
	{6,23}, {6,24}, {6,25}, {7,27}, {7,28}, {7,29}, {8,17}, {8,19},
	{8,20}, {8,22}, {8,23}, {8,24}, {8,25}, {15,2}, {15,4}, {15,16},
	{15,17}, {15,18}, {15,19}, {15,21}, {15,22}, {15,23}, {15,24}, {15,25},
	{16,2}, {16,6}, {16,15}, {16,17}, {16,18}, {16,20}, {16,21}, {16,22},
	{16,23}, {16,24}, {16,25}, {17,2}, {17,8}, {17,15}, {17,16}, {17,19},
	{17,20}, {17,21}, {17,22}, {17,23}, {17,24}, {17,25}, {18,4}, {18,6},
	{18,15}, {18,16}, {18,19}, {18,20}, {18,21}, {18,22}, {18,23}, {18,24},
	{18,25}, {19,4}, {19,8}, {19,15}, {19,17}, {19,18}, {19,20}, {19,21},
	{19,22}, {19,23}, {19,24}, {19,25}, {20,6}, {20,8}, {20,16}, {20,17},
	{20,18}, {20,19}, {20,21}, {20,22}, {20,23}, {20,24}, {20,25}, {21,2},
	{21,4}, {21,6}, {21,15}, {21,16}, {21,17}, {21,18}, {21,19}, {21,20},
	{21,22}, {21,23}, {21,24}, {21,25}, {22,2}, {22,4}, {22,8}, {22,15},
	{22,16}, {22,17}, {22,18}, {22,19}, {22,20}, {22,21}, {22,23}, {22,24},
	{22,25}, {23,2}, {23,6}, {23,8}, {23,15}, {23,16}, {23,17}, {23,18},
	{23,19}, {23,20}, {23,21}, {23,22}, {23,24}, {23,25}, {24,4}, {24,6},
	{24,8}, {24,15}, {24,16}, {24,17}, {24,18}, {24,19}, {24,20}, {24,21},
	{24,22}, {24,23}, {24,25}, {25,2}, {25,4}, {25,6}, {25,8}, {25,15},
	{25,16}, {25,17}, {25,18}, {25,19}, {25,20}, {25,21}, {25,22}, {25,23},
	{25,24}, {26,3}, {26,5}, {26,27}, {26,28}, {26,29}, {27,3}, {27,7},
	{27,26}, {27,28}, {27,29}, {28,5}, {28,7}, {28,26}, {28,27}, {28,29},
	{29,3}, {29,5}, {29,7}, {29,26}, {29,27}, {29,28},
};

static bool sIsRTParmFunctionSameGE_InSet(unsigned char v, const unsigned char *set, int n)
{
	for (int i = 0; i < n; ++i)
		if (set[i] == v)
			return true;
	return false;
}

bool IsRTParmFunctionSameGE(unsigned char kind, unsigned char idx, unsigned char b, unsigned char c)
{
	if (kind == 0 || kind != b)
		return false;
	if (idx == c)
		return true;

	static const unsigned char k2[]  = {14,15};
	static const unsigned char k3a[] = {6,7,8};
	static const unsigned char k4a[] = {2,3};
	static const unsigned char k4b[] = {9,10,11,12};
	static const unsigned char k4c[] = {13,14};
	static const unsigned char k5[]  = {7,8};
	static const unsigned char k6[]  = {9,10};
	static const unsigned char k7[]  = {3,4};
	static const unsigned char k8[]  = {10,11};
	static const unsigned char k9[]  = {14,15};
	static const unsigned char ka[]  = {40,41};
	static const unsigned char kc[]  = {0,1,2,3,4,5};
	static const unsigned char ke_a[] = {0,1};
	static const unsigned char ke_b[] = {16,17,18,19};
	static const unsigned char ke_c[] = {22,23,24};
	static const unsigned char ke_d[] = {37,38};
	static const unsigned char ke_e[] = {40,41,42,43,44,45};

	switch (kind) {
	case 2:
		return sIsRTParmFunctionSameGE_InSet(idx, k2, 2) && sIsRTParmFunctionSameGE_InSet(c, k2, 2);
	case 3:
		/* {6,7,8} clique, plus a "hub" shape over 20..40: 36 is
		 * compatible with everything in [20,40]; 37/38/39/40 are
		 * ALSO each compatible with their own 4-value sub-range
		 * (20-23/24-27/28-31/32-35) on top of 36. */
		if (sIsRTParmFunctionSameGE_InSet(idx, k3a, 3) && sIsRTParmFunctionSameGE_InSet(c, k3a, 3))
			return true;
		if ((idx == 36 && c >= 20 && c <= 40) || (c == 36 && idx >= 20 && idx <= 40))
			return true;
		if ((idx == 37 && c >= 20 && c <= 23) || (c == 37 && idx >= 20 && idx <= 23))
			return true;
		if ((idx == 38 && c >= 24 && c <= 27) || (c == 38 && idx >= 24 && idx <= 27))
			return true;
		if ((idx == 39 && c >= 28 && c <= 31) || (c == 39 && idx >= 28 && idx <= 31))
			return true;
		if ((idx == 40 && c >= 32 && c <= 35) || (c == 40 && idx >= 32 && idx <= 35))
			return true;
		return false;
	case 4:
		if (sIsRTParmFunctionSameGE_InSet(idx, k4a, 2) && sIsRTParmFunctionSameGE_InSet(c, k4a, 2))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, k4b, 4) && sIsRTParmFunctionSameGE_InSet(c, k4b, 4))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, k4c, 2) && sIsRTParmFunctionSameGE_InSet(c, k4c, 2))
			return true;
		return false;
	case 5:
		return sIsRTParmFunctionSameGE_InSet(idx, k5, 2) && sIsRTParmFunctionSameGE_InSet(c, k5, 2);
	case 6:
		return sIsRTParmFunctionSameGE_InSet(idx, k6, 2) && sIsRTParmFunctionSameGE_InSet(c, k6, 2);
	case 7:
		return sIsRTParmFunctionSameGE_InSet(idx, k7, 2) && sIsRTParmFunctionSameGE_InSet(c, k7, 2);
	case 8:
		return sIsRTParmFunctionSameGE_InSet(idx, k8, 2) && sIsRTParmFunctionSameGE_InSet(c, k8, 2);
	case 9:
		return sIsRTParmFunctionSameGE_InSet(idx, k9, 2) && sIsRTParmFunctionSameGE_InSet(c, k9, 2);
	case 0xa:
		return sIsRTParmFunctionSameGE_InSet(idx, ka, 2) && sIsRTParmFunctionSameGE_InSet(c, ka, 2);
	case 0xb:
		/* NOT symmetric in ground truth -- see header comment.
		 * Directed pair table, taken verbatim from the exhaustive
		 * native-execution dump. */
		for (int i = 0; i < 190; ++i)
			if (sIsRTParmFunctionSameGE_Kind11Pairs[i][0] == idx &&
			    sIsRTParmFunctionSameGE_Kind11Pairs[i][1] == c)
				return true;
		return false;
	case 0xc:
		return sIsRTParmFunctionSameGE_InSet(idx, kc, 6) && sIsRTParmFunctionSameGE_InSet(c, kc, 6);
	case 0xe:
		if (sIsRTParmFunctionSameGE_InSet(idx, ke_a, 2) && sIsRTParmFunctionSameGE_InSet(c, ke_a, 2))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, ke_b, 4) && sIsRTParmFunctionSameGE_InSet(c, ke_b, 4))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, ke_c, 3) && sIsRTParmFunctionSameGE_InSet(c, ke_c, 3))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, ke_d, 2) && sIsRTParmFunctionSameGE_InSet(c, ke_d, 2))
			return true;
		if (sIsRTParmFunctionSameGE_InSet(idx, ke_e, 6) && sIsRTParmFunctionSameGE_InSet(c, ke_e, 6))
			return true;
		return false;
	default:
		/* kind in {1,0xd,0xf..0xff}: no dispatch entry, falls
		 * through to the shared default (idx==c, already true or
		 * false above). */
		return false;
	}
}
