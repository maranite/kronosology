/*
 * ustg_api_wrappers.cpp  -  see include/ustg_api_wrappers.h.
 *
 * Every message-shape struct below is transcribed field-by-field from a direct
 * `objdump -dr -M intel` read of each function's own real body (not inferred by
 * analogy between functions with the same argument COUNT) -- several pairs that
 * look identical in the C++ signature turn out to place arguments into the wire
 * struct in a different order than the parameter list (e.g.
 * UpdateProgramSlotParameter's own real order is a2,a3,a4,a5,a6,a8,a1,a7).
 */

#include "ustg_api_wrappers.h"

#include <cstring>

/* --- USTGAPICombi --- */

/* Shared shape for UpdateCombiParameter/UpdateVectorMotionParameter/
 * UpdateControllerInfoParameter/UpdateAudioInputParameter/
 * UpdateEffectBalanceParameter -- byte-identical field layout, only `subcode`
 * differs per real call site.
 */
struct CombiParamMsgShape {
	unsigned short length;   /* 0x24 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 2 */
	unsigned int   subcode;
	unsigned int   f_a1;     /* base+0xc */
	unsigned int   f_a2;     /* base+0x10 */
	unsigned int   f_a4;     /* base+0x14 -- real order swaps a3/a4 */
	unsigned int   f_a3;     /* base+0x18 */
	unsigned int   f_a5;     /* base+0x1c */
	unsigned int   f_perfType; /* base+0x20 */
};

static void SendCombiParamMsg(unsigned subcode, unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	CombiParamMsgShape msg;
	msg.length = 0x24;
	msg.subtype = 1;
	msg.type = 2;
	msg.subcode = subcode;
	msg.f_a1 = a1;
	msg.f_a2 = a2;
	msg.f_a4 = (unsigned int)a4;
	msg.f_a3 = (unsigned int)a3;
	msg.f_a5 = (unsigned int)a5;
	msg.f_perfType = perfType;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

void USTGAPICombi::UpdateCombiParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	SendCombiParamMsg(0, a1, a2, a3, a4, a5, perfType);
}

void USTGAPICombi::UpdateVectorMotionParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	SendCombiParamMsg(1, a1, a2, a3, a4, a5, perfType);
}

void USTGAPICombi::UpdateControllerInfoParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	SendCombiParamMsg(2, a1, a2, a3, a4, a5, perfType);
}

/* Round 59 (solo): real 4-param convenience overload, forwards into the
 * 6-param version above with a1=0/a2=0xffff hardcoded -- see ustg_api_wrappers.h.
 */
void USTGAPICombi::UpdateControllerInfoParameter(unsigned a1, unsigned a2, int a3, eSTGMsgPerfType perfType)
{
	UpdateControllerInfoParameter(0, 0xffff, a1, a2, a3, perfType);
}

void USTGAPICombi::UpdateAudioInputParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	SendCombiParamMsg(4, a1, a2, a3, a4, a5, perfType);
}

void USTGAPICombi::UpdateEffectBalanceParameter(unsigned a1, unsigned a2, int a3, unsigned a4, int a5, eSTGMsgPerfType perfType)
{
	SendCombiParamMsg(5, a1, a2, a3, a4, a5, perfType);
}

/* .text+0x08e1b960 -- own distinct shape, type=2 subcode=3, 8 payload dwords in
 * real order a1,a2,a3,a4,a6,a5,a7,a8.
 */
struct ToneAdjustMsgShape {
	unsigned short length;   /* 0x2c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 2 */
	unsigned int   subcode;  /* 3 */
	unsigned int   f_a1;
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
	unsigned int   f_a6;
	unsigned int   f_a5;
	unsigned int   f_a7;
	unsigned int   f_a8;
};

void USTGAPICombi::UpdateToneAdjustParameter(unsigned a1, unsigned a2, unsigned a3, unsigned a4, int a5, unsigned a6, int a7, eSTGMsgPerfType a8)
{
	ToneAdjustMsgShape msg;
	msg.length = 0x2c;
	msg.subtype = 1;
	msg.type = 2;
	msg.subcode = 3;
	msg.f_a1 = a1;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = a4;
	msg.f_a6 = a6;
	msg.f_a5 = (unsigned int)a5;
	msg.f_a7 = (unsigned int)a7;
	msg.f_a8 = a8;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* .text+0x08e1bac0 -- own distinct shape, type=2 subcode=6, 5 payload dwords: a
 * real constant 0, then a1/a2/a3, then a real constant 2. Both constants
 * transcribed exactly, not simplified away.
 */
struct SequenceMetronomeMsgShape {
	unsigned short length;   /* 0x20 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 2 */
	unsigned int   subcode;  /* 6 */
	unsigned int   const0;   /* 0 */
	unsigned int   f_a1;
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   const2;   /* 2 */
};

void USTGAPICombi::UpdateSequenceMetronomeParameter(unsigned a1, int a2, int a3)
{
	SequenceMetronomeMsgShape msg;
	msg.length = 0x20;
	msg.subtype = 1;
	msg.type = 2;
	msg.subcode = 6;
	msg.const0 = 0;
	msg.f_a1 = a1;
	msg.f_a2 = (unsigned int)a2;
	msg.f_a3 = (unsigned int)a3;
	msg.const2 = 2;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* .text+0x08e1b6f0, SharedMemCombiDump -- type=2 subcode=7, 5 payload dwords with
 * a real duplicated bankOrSlot/progId pair (base+0xc/+0x18 and +0x10/+0x14).
 * Polls ReadMessage() up to 8x afterward; returns whether the response's own
 * subcode-position field (response buffer +8) echoes back 7.
 */
struct CombiDumpMsgShape {
	unsigned short length;   /* 0x20 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 2 */
	unsigned int   subcode;  /* 7 */
	unsigned int   f_bankOrSlot;   /* base+0xc */
	unsigned int   f_progId;       /* base+0x10 */
	unsigned int   f_progId_dup;   /* base+0x14 -- real duplicate */
	unsigned int   f_bankOrSlot_dup; /* base+0x18 -- real duplicate */
	unsigned int   f_perfType;     /* base+0x1c */
};

/* Shared "send, then poll ReadMessage() up to 8x for a subcode echo" tail used by
 * SharedMemCombiDump/SharedMemDrumKitDump/SharedMemWaveSequenceDump -- ground
 * truth inlines this loop separately in each function (byte-identical control
 * flow, only the read buffer size and expected subcode differ); factored here
 * since the real behavior is unchanged. Real quirk preserved: if all 8 reads
 * fail, the final compare reads whatever was left in `buf` by the LAST failed
 * ReadMessage() call -- real uninitialized-stack-adjacent state on the very
 * first iteration if `buf` was never touched at all (same "preserve real
 * garbage-read behavior" precedent as CSTGUnsolMsgHandler::EndHandling's own
 * 3-byte uninitialized-stack transmission, stg_unsol_msg_handler.cpp).
 */
static bool WaitForDumpSubcodeEcho(unsigned bufSize, unsigned expectedSubcode)
{
	char buf[32];
	for (int i = 0; i < 8; ++i) {
		if (USTGUserAPI::ReadMessage(buf, bufSize) != 0)
			break;
	}
	unsigned echoedSubcode;
	memcpy(&echoedSubcode, buf + 8, sizeof(echoedSubcode));
	return echoedSubcode == expectedSubcode;
}

bool USTGAPICombi::SharedMemCombiDump(unsigned bankOrSlot, unsigned progId, eSTGMsgPerfType perfType)
{
	CombiDumpMsgShape msg;
	msg.length = 0x20;
	msg.subtype = 1;
	msg.type = 2;
	msg.subcode = 7;
	msg.f_bankOrSlot = bankOrSlot;
	msg.f_progId = progId;
	msg.f_progId_dup = progId;
	msg.f_bankOrSlot_dup = bankOrSlot;
	msg.f_perfType = perfType;

	if (!USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg)))
		return false;

	return WaitForDumpSubcodeEcho(0x20, 7);
}

/* --- USTGAPIEffect / USTGAPIEffectSlot --- */

/* Byte-identical shape, only `type` differs (9 for Effect, 8 for EffectSlot).
 * Real payload order is a2,a3,a4,a5,a6, THEN perfType last (base+0x20) -- not
 * first, despite being the first C++ parameter.
 */
struct EffectParamMsgShape {
	unsigned short length;   /* 0x24 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 9 (Effect) or 8 (EffectSlot) */
	unsigned int   subcode;  /* 0 */
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
	unsigned int   f_a5;
	unsigned int   f_a6;     /* sign-extended s16 */
	unsigned int   f_perfType; /* base+0x20, last */
};

void USTGAPIEffect::UpdateParam(eSTGMsgPerfType perfType, unsigned short a2, unsigned short a3, unsigned short a4, unsigned short a5, short a6)
{
	EffectParamMsgShape msg;
	msg.length = 0x24;
	msg.subtype = 1;
	msg.type = 9;
	msg.subcode = 0;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = a4;
	msg.f_a5 = a5;
	msg.f_a6 = (unsigned int)(int)a6;
	msg.f_perfType = perfType;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

void USTGAPIEffectSlot::UpdateParam(eSTGMsgPerfType perfType, unsigned short a2, unsigned short a3, unsigned short a4, unsigned short a5, short a6)
{
	EffectParamMsgShape msg;
	msg.length = 0x24;
	msg.subtype = 1;
	msg.type = 8;
	msg.subcode = 0;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = a4;
	msg.f_a5 = a5;
	msg.f_a6 = (unsigned int)(int)a6;
	msg.f_perfType = perfType;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIEffectMgr --- */

struct EffectLFOMsgShape {
	unsigned short length;   /* 0x24 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 7 */
	unsigned int   subcode;  /* 5 */
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
	unsigned int   f_a5;
	unsigned int   f_a6;
	unsigned int   f_perfType; /* base+0x20, last */
};

void USTGAPIEffectMgr::UpdateEffectLFOParameter(eSTGMsgPerfType perfType, int a2, int a3, unsigned a4, unsigned a5, unsigned a6)
{
	EffectLFOMsgShape msg;
	msg.length = 0x24;
	msg.subtype = 1;
	msg.type = 7;
	msg.subcode = 5;
	msg.f_a2 = (unsigned int)a2;
	msg.f_a3 = (unsigned int)a3;
	msg.f_a4 = a4;
	msg.f_a5 = a5;
	msg.f_a6 = a6;
	msg.f_perfType = perfType;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIGlobal --- */

struct GlobalParamMsgShape {
	unsigned short length;   /* 0x18 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 1 */
	unsigned int   subcode;  /* 0 */
	unsigned int   f_a2;     /* base+0xc -- real order is a2 first */
	unsigned int   f_a1;     /* base+0x10 */
	unsigned int   f_a3;     /* base+0x14 */
};

void USTGAPIGlobal::UpdateGlobalParameter(unsigned a1, unsigned a2, int a3)
{
	GlobalParamMsgShape msg;
	msg.length = 0x18;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0;
	msg.f_a2 = a2;
	msg.f_a1 = a1;
	msg.f_a3 = (unsigned int)a3;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIHDRTrack --- */

struct HDRTrackParamMsgShape {
	unsigned short length;   /* 0x1c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 0xe */
	unsigned int   subcode;  /* 0 */
	unsigned int   f_a1;
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
};

void USTGAPIHDRTrack::UpdateHDRTrackParameter(unsigned a1, unsigned a2, unsigned a3, int a4)
{
	HDRTrackParamMsgShape msg;
	msg.length = 0x1c;
	msg.subtype = 1;
	msg.type = 0xe;
	msg.subcode = 0;
	msg.f_a1 = a1;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = (unsigned int)a4;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIProgramSlot --- */

/* .text+0x08e22fd0 -- type=3 subcode=0, real payload order a2,a3,a4,a5,a6,a8,a1,a7. */
struct ProgramSlotParamMsgShape {
	unsigned short length;   /* 0x2c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 3 */
	unsigned int   subcode;  /* 0 */
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
	unsigned int   f_a5;
	unsigned int   f_a6;
	unsigned int   f_a8;
	unsigned int   f_a1;     /* perfType */
	unsigned int   f_a7;
};

void USTGAPIProgramSlot::UpdateProgramSlotParameter(eSTGMsgPerfType a1, unsigned a2, unsigned a3, int a4, unsigned a5, unsigned a6, unsigned a7, int a8)
{
	ProgramSlotParamMsgShape msg;
	msg.length = 0x2c;
	msg.subtype = 1;
	msg.type = 3;
	msg.subcode = 0;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = (unsigned int)a4;
	msg.f_a5 = a5;
	msg.f_a6 = a6;
	msg.f_a8 = (unsigned int)a8;
	msg.f_a1 = a1;
	msg.f_a7 = a7;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* .text+0x08e23050 -- type=3 subcode=2, real payload order a2,a3,a4, const0,
 * const0, a5(bool as dword), a1, const0.
 */
struct ProgramSlotEnabledMsgShape {
	unsigned short length;   /* 0x2c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 3 */
	unsigned int   subcode;  /* 2 */
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
	unsigned int   const0_a; /* 0 */
	unsigned int   const0_b; /* 0 */
	unsigned int   f_a5;     /* bool, stored as a full dword */
	unsigned int   f_a1;     /* perfType */
	unsigned int   const0_c; /* 0 */
};

void USTGAPIProgramSlot::UpdateProgramSlotEnabled(eSTGMsgPerfType a1, unsigned a2, unsigned a3, int a4, bool a5)
{
	ProgramSlotEnabledMsgShape msg;
	msg.length = 0x2c;
	msg.subtype = 1;
	msg.type = 3;
	msg.subcode = 2;
	msg.f_a2 = a2;
	msg.f_a3 = a3;
	msg.f_a4 = (unsigned int)a4;
	msg.const0_a = 0;
	msg.const0_b = 0;
	msg.f_a5 = a5 ? 1u : 0u;
	msg.f_a1 = a1;
	msg.const0_c = 0;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPISetList --- */

struct SetListSlotParamMsgShape {
	unsigned short length;   /* 0x1c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 0x10 */
	unsigned int   subcode;  /* 0 */
	unsigned int   f_a1;
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;
};

void USTGAPISetList::UpdateSlotParam(int a1, int a2, int a3, int a4)
{
	SetListSlotParamMsgShape msg;
	msg.length = 0x1c;
	msg.subtype = 1;
	msg.type = 0x10;
	msg.subcode = 0;
	msg.f_a1 = (unsigned int)a1;
	msg.f_a2 = (unsigned int)a2;
	msg.f_a3 = (unsigned int)a3;
	msg.f_a4 = (unsigned int)a4;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIPatch --- */

struct OscSelectMsgShape {
	unsigned short length;   /* 0x1c */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 5 */
	unsigned int   subcode;  /* 1 */
	unsigned int   f_a1;     /* eSTGProgramBankId */
	unsigned int   f_a2;
	unsigned int   f_a3;
	unsigned int   f_a4;     /* eSTGVoiceModelType */
};

void USTGAPIPatch::UpdateOscSelectByType(eSTGProgramBankId a1, unsigned a2, int a3, eSTGVoiceModelType a4)
{
	OscSelectMsgShape msg;
	msg.length = 0x1c;
	msg.subtype = 1;
	msg.type = 5;
	msg.subcode = 1;
	msg.f_a1 = a1;
	msg.f_a2 = a2;
	msg.f_a3 = (unsigned int)a3;
	msg.f_a4 = a4;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

/* --- USTGAPIDrumkitData --- */

struct CurrentKitIdMsgShape {
	unsigned short length;   /* 0x10 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 1 */
	unsigned int   subcode;  /* 0xd */
	unsigned int   f_a1;
};

void USTGAPIDrumkitData::SetCurrentKitId(unsigned a1)
{
	CurrentKitIdMsgShape msg;
	msg.length = 0x10;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0xd;
	msg.f_a1 = a1;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

struct DrumKitDumpMsgShape {
	unsigned short length;   /* 0x10 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 1 */
	unsigned int   subcode;  /* 0xf */
	unsigned int   f_a1;
};

bool USTGAPIDrumkitData::SharedMemDrumKitDump(int a1)
{
	DrumKitDumpMsgShape msg;
	msg.length = 0x10;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0xf;
	msg.f_a1 = (unsigned int)a1;

	if (!USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg)))
		return false;

	return WaitForDumpSubcodeEcho(0x10, 0xf);
}

/* --- USTGAPIWaveSequenceData --- */

struct CurrentSequenceIdMsgShape {
	unsigned short length;   /* 0x10 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 1 */
	unsigned int   subcode;  /* 0xe */
	unsigned int   f_a1;
};

void USTGAPIWaveSequenceData::SetCurrentSequenceId(unsigned a1)
{
	CurrentSequenceIdMsgShape msg;
	msg.length = 0x10;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0xe;
	msg.f_a1 = a1;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

struct WaveSequenceDumpMsgShape {
	unsigned short length;   /* 0x10 */
	unsigned short subtype;  /* 1 */
	unsigned int   type;     /* 1 */
	unsigned int   subcode;  /* 0x10 */
	unsigned int   f_a1;
};

bool USTGAPIWaveSequenceData::SharedMemWaveSequenceDump(int a1)
{
	WaveSequenceDumpMsgShape msg;
	msg.length = 0x10;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0x10;
	msg.f_a1 = (unsigned int)a1;

	if (!USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg)))
		return false;

	return WaitForDumpSubcodeEcho(0x10, 0x10);
}
