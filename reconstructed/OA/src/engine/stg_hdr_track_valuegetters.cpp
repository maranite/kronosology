// SPDX-License-Identifier: GPL-2.0
/*
 * stg_hdr_track_valuegetters.cpp  -  CSTGHDRTrack's
 * GetValue*(CSTGHDRTrackMessageContext&) value-getter family, see
 * include/oa_stg_hdr_track.h for the full class-level derivation notes --
 * all 15 real weak-symbol candidates decoded, zero outliers.
 *
 * verify/test_stg_hdr_track_valuegetters.cpp independently re-derives the
 * expected value for every method here via a separate Python evaluator,
 * not by re-using this file's C output strings.
 */

#include "oa_stg_hdr_track.h"

STGConvertedParam &CSTGHDRTrack::GetValueOutputBus(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueFXCtrlBus(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueHDRBus(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x6);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQInputTrim(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x7);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQLow(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xb);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQMid(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xf);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQMidFreq(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x13);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQHigh(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x17);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValuePan(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueSend1Level(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueSend2Level(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x23);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueLevel(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x27);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueEQBypass(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x2b) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGHDRTrack::GetValueMute(CSTGHDRTrackMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x2b) >> 1) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

/*
 * GetValueSolo -- see include/oa_stg_hdr_track.h for the full derivation.
 * Reads NEITHER `this` nor ctx.index: resolves the real, already-declared
 * global singleton CSTGControllerRTData::sInstance, reads a WORD at its
 * own +0x24, then shifts right by a per-call bit index read from ctx's
 * own +0x18 byte, masked to 5 bits to match x86's own shift-count
 * masking, and takes bit 0 of the result.
 */
STGConvertedParam &CSTGHDRTrack::GetValueSolo(CSTGHDRTrackMessageContext &ctx)
{
	unsigned char *ctxbase = (unsigned char *)&ctx;
	unsigned int shift = *(unsigned char *)(ctxbase + 0x18) & 0x1f;
	unsigned int word = *(unsigned short *)((unsigned char *)CSTGControllerRTData::sInstance + 0x24);
	int v = (int)((word >> shift) & 0x1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
