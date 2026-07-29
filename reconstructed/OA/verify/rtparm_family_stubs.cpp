// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_family_stubs.cpp  -  TEST-ONLY empty-body stand-ins for the real,
 * still-`pending` functions src/engine/rtparm_family.cpp calls (real KARMA
 * callees, not RT_* table entries -- those come from
 * verify/rtparm_ge_func_stubs.cpp / verify/rtparm_pe_func_stubs.cpp,
 * reused as-is by test_rtparm_family.cpp). Never linked into OA.ko itself,
 * matching this project's established stub-file convention.
 */

#include "oa_rtparm_family.h"

unsigned long GetFirstOnBit(unsigned long mask, unsigned char width)
{
	for (unsigned char i = 0; i < width; ++i)
		if (mask & (1UL << i))
			return i;
	return 0;
}

void AssignRTParmGE(unsigned char, GenEffect *, RTParm *, RTParmFunction *,
                     const RTParmFunctionTable *, unsigned char, unsigned char) { }
void AssignRTParmPE(RTParm *, RTParmFunction *, const RTParmFunctionTable *,
                     unsigned char, unsigned char) { }
short ScaleRTParmValue(RTParmEdit *, unsigned char, unsigned char) { return 0; }
void KM_rtp_val_out_pe(RTParm_pub *, unsigned char, unsigned char) { }
void Do_KM_rtp_update_name(unsigned char, unsigned char) { }
void Do_KM_rtp_update_all_names(unsigned char, unsigned long) { }
void CSKParameterChangeMessage::SetValue(int) { }
void CSysExBuffer::SendSysExMassage(unsigned char *) { }

void RTParmNameManager::GetRTParmNameString(GenEffect *, RTParm *, char *, bool) { }

/* Added for the follow-up (2026-07-29) pass's 7 deferred members. */
int g_do_km_rtp_val_out_pe_calls;
void Do_KM_rtp_val_out_pe(RTParm *, unsigned char, unsigned char) { ++g_do_km_rtp_val_out_pe_calls; }
bool IsRTParmFunctionSameGE(unsigned char, unsigned char, unsigned char, unsigned char) { return false; }
unsigned long CountOnBits(unsigned long mask, unsigned char width)
{
	unsigned long n = 0;
	for (unsigned char i = 0; i < width; ++i)
		if (mask & (1UL << i))
			++n;
	return n;
}
