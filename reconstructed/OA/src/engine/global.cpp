// SPDX-License-Identifier: GPL-2.0
/*
 * global.cpp  -  see include/oa_global.h.
 * Ground-truthed offset: CSTGGlobal::IncrementMicrosecondCount .text+0x93b0
 * (74 bytes).
 *
 * Maintains a 64-bit microsecond counter (confirmed fields at +0x29c9fb0/
 * +0x29c9fb4) that is NOT incremented by a constant every call -- it cycles
 * a 2-bit phase counter (+0x29c9fb8, values 0..3) and adds 0x29b (667) on
 * three calls out of every four, and 0x29a (666) on the fourth (which also
 * resets the phase to 0). Confirmed exact: 3*667 + 1*666 = 2667, and
 * 2667/4 = 666.75 -- this is a textbook Bresenham-style fractional-rate
 * accumulator, averaging out to exactly 666.75us/call (i.e. this is called
 * at exactly 1500Hz and the counter tracks elapsed microseconds with full
 * precision despite 666.75 not being a whole number of microseconds).
 */

#include "oa_global.h"
#include "oa_engine.h"	/* for CSTGAudioBusManager, used by UpdateLRBusIndivAssign */
#include "oa_engine_init.h"	/* for CSTGMidiDispatcher, used by UpdateProgramChangeEnable/UpdateBankChangeEnable (sec 10.75) */
#include "oa_setup_global_resources.h"	/* for COmapNKS4Driver_SetTestMode, used by SetNKS4TestModeFlag (sec 10.90) */

extern "C" void PushUnsolicitedMessage(void *msg);

CSTGGlobal *CSTGGlobal::sInstance;

void CSTGGlobal::IncrementMicrosecondCount()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *phase = base + 0x29c9fb8;
	unsigned int  *usecLo = (unsigned int *)(base + 0x29c9fb0);
	unsigned int  *usecHi = (unsigned int *)(base + 0x29c9fb4);

	unsigned char newPhase = (unsigned char)(*phase + 1);

	if (newPhase == 3) {
		*phase = 0;
		unsigned int old = *usecLo;
		*usecLo = old + 0x29a;
		if (*usecLo < old)
			(*usecHi)++;
	} else {
		*phase = newPhase;
		unsigned int old = *usecLo;
		*usecLo = old + 0x29b;
		if (*usecLo < old)
			(*usecHi)++;
	}
}

/*
 * UpdateMuteMode (.text+0x1060, 9 bytes) confirmed: stores the incoming
 * param value directly (no bool conversion, unlike the two below).
 *   mov edx,[ecx]                  ; edx = param.value
 *   mov [eax+0x29c9fc4],edx        ; this->fieldAt(0x29c9fc4) = edx
 *   ret
 */
void CSTGGlobal::UpdateMuteMode(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x29c9fc4) = param.value;
}

/*
 * UpdateRearPanelControllerReset (.text+0x1040, 12 bytes) confirmed:
 * stores (param.value != 0) as a byte (a real bool conversion via
 * `setne`, not a raw store).
 *   mov edx,[ecx]                  ; edx = param.value
 *   test edx,edx
 *   setne [eax+0x29cc118]          ; this->fieldAt(0x29cc118) = (edx != 0)
 *   ret
 */
void CSTGGlobal::UpdateRearPanelControllerReset(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x29cc118) = (param.value != 0);
}

/*
 * UpdateTmbrTrkOscTransposeType (.text+0x1050, 12 bytes) confirmed:
 * identical shape to UpdateRearPanelControllerReset immediately above it
 * in the binary, one field over (+0x29cc119 vs +0x29cc118) -- consistent
 * with a run of adjacent boolean flag fields.
 */
void CSTGGlobal::UpdateTmbrTrkOscTransposeType(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x29cc119) = (param.value != 0);
}

/*
 * UpdateUserAllNoteScale (.text+0xda0, 13 bytes) confirmed: the first of
 * this batch that's actually indexed -- writes param.value into a 4-byte
 * array slot selected by ctx.index, not a single fixed field.
 *   mov edx,[edx+0x4]              ; edx = ctx.index
 *   mov ecx,[ecx]                  ; ecx = param.value
 *   mov [eax+edx*4+0x29c9d98],ecx  ; this->fieldAt(0x29c9d98)[ctx.index] = ecx
 *   ret
 */
void CSTGGlobal::UpdateUserAllNoteScale(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	int *table = (int *)((unsigned char *)this + 0x29c9d98);
	table[ctx.index] = param.value;
}

/*
 * UpdateLRBusIndivAssign (.text+0x2890, 20 bytes) confirmed: delegates to
 * CSTGAudioBusManager::SetLRBusIndivAssign() on a pointer computed as
 * this+4 -- see the class comment in oa_global.h for what's confirmed
 * and what isn't about that pointer's real identity.
 */
void CSTGGlobal::UpdateLRBusIndivAssign(CSTGMessageContext &, STGConvertedParam &param)
{
	CSTGAudioBusManager *abm = (CSTGAudioBusManager *)((unsigned char *)this + 4);
	abm->SetLRBusIndivAssign(param.value);
}

/*
 * UpdateSPDIFSampleRate (.text+0xf90, 23 bytes) confirmed:
 *   cmp BYTE PTR [eax+0x6ac],0     ; this->flagAt(0x6ac) == 0 ?
 *   mov ecx,[ecx]                  ; ecx = param.value (read unconditionally)
 *   jne skip                        ; flag != 0 -> skip
 *   test ecx,ecx
 *   je skip                         ; param.value == 0 -> skip
 *   mov [edx+0x10],6                ; ctx.responseCode = 6 (literal, not param.value)
 * skip: ret
 */
void CSTGGlobal::UpdateSPDIFSampleRate(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char flag = *((unsigned char *)this + 0x6ac);
	if (flag == 0 && param.value != 0)
		ctx.responseCode = 6;
}

/*
 * TranslateAudioInputParamId (.text+0x9340, 21 bytes) confirmed: a pure
 * lookup, no `this` dependency at all despite being a real CSTGGlobal
 * member (see oa_global.h). Table confirmed via its real .rodata bytes,
 * not inferred from the instruction alone.
 */
int CSTGGlobal::TranslateAudioInputParamId(unsigned int paramId)
{
	static const int kTable[8] = { 13, 12, 15, 16, 12, 12, 49, 50 };
	unsigned int idx = paramId - 2;
	if (idx > 7)
		return 12;
	return kTable[idx];
}

/*
 * CSTGControllerRTData::SetFootSwitchPolarity (.text+0xd5b0, 4 bytes)
 * confirmed: a single byte store at +0x1, no conversion.
 */
void CSTGControllerRTData::SetFootSwitchPolarity(int polarity)
{
	footSwitchPolarity = (unsigned char)polarity;
}

/*
 * OnExtModePlayMuteSwitchAssignChange()/OnExtModeSelectSwitchAssignChange()
 * (sec 10.126): see oa_global.h for the full confirmed shape.
 */
void CSTGControllerRTData::OnExtModePlayMuteSwitchAssignChange(unsigned int index)
{
	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	status[index + 0x913] = 0;
	CSTGControllerInfo::SendUnsolicitedUIParam(9, index, 0, 1);
}
void CSTGControllerRTData::OnExtModeSelectSwitchAssignChange(unsigned int index)
{
	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	status[index + 0x91b] = 0;
	CSTGControllerInfo::SendUnsolicitedUIParam(10, index, 0, 1);
}

/*
 * OnExtModeKnobAssignChange(unsigned int)/OnExtModeSliderAssignChange(unsigned int)
 * (sec 10.161, `.text+0x19dc0`/`.text+0x19eb0`, 238 bytes each): see
 * oa_global.h for the full confirmed shape. The two are parallel in
 * structure with different literal offsets/strides (Knob: table stride
 * 8, panel +0x90b, per-index arrays at +0x50/+0x54/+0x56, msg dword +8
 * = 0xe; Slider: table stride 9, panel +0x923, per-index arrays at
 * +0x60/+0x6c/+0x6e, msg dword +8 = 0xf) -- written out separately
 * rather than factored into one shared helper, matching the real
 * binary's own two independent (not tail-call-shared) function bodies.
 */
void CSTGControllerRTData::OnExtModeKnobAssignChange(unsigned int index)
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned int mode = g[0x29cc0c8];
	unsigned char cc = g[0x29ca3c8 + mode * 8 + index];
	if (cc == 0xff)
		return;

	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	unsigned char val = CSTGCCInfo::sCCInfoTable[cc * 10 + 0];
	status[index + 0x90b] = val;

	unsigned char *t = (unsigned char *)this;
	if (t[0x2b] != 4)
		return;

	if (!(val & 0x80))
		t[0x56 + index * 3] = val;

	if (g[0x29c9fc0] == 0) {
		t[0x54 + index * 3] = 1;
	} else {
		unsigned char *rec = t + 0x50 + index * 3;
		unsigned char cl = rec[5];
		if (cl == 0xff)
			rec[4] = 0xff;
		else if (cl == rec[6])
			rec[4] = 1;
		else
			rec[4] = ((signed char)cl > (signed char)rec[6]) ? 2 : 0;
	}

	unsigned char msg[0x14];
	*(unsigned short *)(msg + 0x0) = 0x14;
	*(unsigned short *)(msg + 0x2) = 1;
	*(unsigned int *)(msg + 0x4) = 0;
	*(unsigned int *)(msg + 0x8) = 0xe;
	*(unsigned int *)(msg + 0xc) = index;
	*(unsigned int *)(msg + 0x10) = val;
	PushUnsolicitedMessage(msg);
}
void CSTGControllerRTData::OnExtModeSliderAssignChange(unsigned int index)
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned int mode = g[0x29cc0c8];
	unsigned char cc = g[0x29cbc48 + mode * 9 + index];
	if (cc == 0xff)
		return;

	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	unsigned char val = CSTGCCInfo::sCCInfoTable[cc * 10 + 0];
	status[index + 0x923] = val;

	unsigned char *t = (unsigned char *)this;
	if (t[0x2b] != 4)
		return;

	if (!(val & 0x80))
		t[0x6e + index * 3] = val;

	if (g[0x29c9fc0] == 0) {
		t[0x6c + index * 3] = 1;
	} else {
		unsigned char *rec = t + 0x60 + index * 3;
		unsigned char cl = rec[0xd];
		if (cl == 0xff)
			rec[0xc] = 0xff;
		else if (cl == rec[0xe])
			rec[0xc] = 1;
		else
			rec[0xc] = ((signed char)cl > (signed char)rec[0xe]) ? 2 : 0;
	}

	unsigned char msg[0x14];
	*(unsigned short *)(msg + 0x0) = 0x14;
	*(unsigned short *)(msg + 0x2) = 1;
	*(unsigned int *)(msg + 0x4) = 0;
	*(unsigned int *)(msg + 0x8) = 0xf;
	*(unsigned int *)(msg + 0xc) = index;
	*(unsigned int *)(msg + 0x10) = val;
	PushUnsolicitedMessage(msg);
}

/*
 * SetSplitLayerWorkState (.text+0x9740, 7 bytes) confirmed: direct byte
 * store, no conversion.
 */
void CSTGGlobal::SetSplitLayerWorkState(bool state)
{
	*((unsigned char *)this + 0x29cc4e8) = state;
}

/*
 * UpdateFootswitchPolarity (.text+0x28f0, 29 bytes) confirmed:
 *   cmp BYTE PTR [eax+0x6ae],0     ; this->flagAt(0x6ae) != 0 ?
 *   je skip
 *   lea eax,[eax+0x10]              ; eax = this+0x10 (a CSTGControllerRTData*)
 *   mov edx,[ecx]                   ; edx = param.value
 *   call SetFootSwitchPolarity      ; (eax, edx)
 * skip: ret
 */
void CSTGGlobal::UpdateFootswitchPolarity(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char flag = *((unsigned char *)this + 0x6ae);
	if (flag != 0) {
		CSTGControllerRTData *rt = (CSTGControllerRTData *)((unsigned char *)this + 0x10);
		rt->SetFootSwitchPolarity(param.value);
	}
}

/*
 * The `UpdateXXXMIDIChannel` family -- see the class comment in
 * oa_global.h for the full confirmed shape shared by all 23
 * (`UpdateSongPunchMIDIChannel` .text+0x1280 was the first one found;
 * every one of the other 22 was confirmed via a full programmatic
 * disassembly scan, not a spot check, to match instruction-for-
 * instruction, differing only in the two field offsets):
 *   mov edx,[ecx]                          ; edx = param.value
 *   movzx ecx,BYTE PTR [eax+writeOffset+1]  ; ecx = the SELECTOR field (read-only here)
 *   mov BYTE PTR [eax+writeOffset],dl       ; this->fieldAt(writeOffset) = new value (write-only here)
 *   test cl,cl
 *   js skip                                 ; selector is negative (signed byte) -> skip
 *   movzx ecx,cl
 *   mov BYTE PTR [eax+ecx*8+0x29cc11d],dl  ; array[selector] = new value (8-byte stride)
 * skip: ret
 * `writeOffset` and `writeOffset+1` are two SEPARATE fields, confirmed
 * by checking carefully -- none of these 23 read back what they just
 * wrote to `writeOffset`.
 *
 * The original binary duplicates this in all 23 functions independently
 * (confirmed: 23 separate .text addresses). Factored into one shared
 * helper here for maintainability; every public method below is a thin
 * wrapper passing its own confirmed offset.
 */
void CSTGGlobal::UpdateMIDIChannelField(unsigned int writeOffset, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char newValue = (unsigned char)param.value;
	signed char selector = (signed char)base[writeOffset + 1];
	base[writeOffset] = newValue;
	if (selector >= 0)
		base[0x29cc11d + (unsigned int)selector * 8] = newValue;
}

void CSTGGlobal::UpdateSongPunchMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0d0, param); }
void CSTGGlobal::UpdateRibbonLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0d8, param); }
void CSTGGlobal::UpdateJSYLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0fc, param); }
void CSTGGlobal::UpdateIncFuncMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc10e, param); }
void CSTGGlobal::UpdateSongStartMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0ce, param); }
void CSTGGlobal::UpdateChordSwMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc112, param); }
void CSTGGlobal::UpdateOctaveUpMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0d4, param); }
void CSTGGlobal::UpdateAftertouchLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc116, param); }
void CSTGGlobal::UpdateProgramUpMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0ca, param); }
void CSTGGlobal::UpdateSW2FuncMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc10c, param); }
void CSTGGlobal::UpdateJSPYRibLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc106, param); }
void CSTGGlobal::UpdateJSXLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0fa, param); }
void CSTGGlobal::UpdateJSYRibLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc104, param); }
void CSTGGlobal::UpdateProgramDownMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0cc, param); }
void CSTGGlobal::UpdateTapTempoMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0d2, param); }
void CSTGGlobal::UpdateJSMYRibLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc108, param); }
void CSTGGlobal::UpdateDTrackEnableMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc114, param); }
void CSTGGlobal::UpdateJSXRibLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc102, param); }
void CSTGGlobal::UpdateSW1FuncMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc10a, param); }
void CSTGGlobal::UpdateJSMYLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc100, param); }
void CSTGGlobal::UpdateDecFuncMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc110, param); }
void CSTGGlobal::UpdateOctaveDownMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0d6, param); }
void CSTGGlobal::UpdateJSPYLockMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{ UpdateMIDIChannelField(0x29cc0fe, param); }

/*
 * The following twelve are all the simple, no-branch shape -- see the
 * class comment in oa_global.h for each one's confirmed offset/width.
 */
void CSTGGlobal::UpdateSeqParamMidiOutMode(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6dd) = (unsigned char)param.value;
}

void CSTGGlobal::UpdateAfterTouchCurve(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x29c9f9c) = param.value;
}

void CSTGGlobal::UpdateBankMap(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x6e4) = param.value;
}

void CSTGGlobal::UpdateVelocityCurve(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x29c9f98) = param.value;
}

void CSTGGlobal::UpdateSeqTrackMidiOutMode(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6dc) = (unsigned char)param.value;
}

void CSTGGlobal::UpdateVectorMIDIOut(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6c2) = (unsigned char)param.value;
}

void CSTGGlobal::UpdateNoteReceive(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x6b4) = param.value;
}

void CSTGGlobal::UpdateDamperPolarity(CSTGMessageContext &, STGConvertedParam &param)
{
	*(int *)((unsigned char *)this + 0x29c9fbc) = param.value;
}

void CSTGGlobal::UpdateCombiChangeEnable(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6d7) = (param.value != 0);
}

void CSTGGlobal::UpdateAftertouchChangeEnable(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6d8) = (param.value != 0);
}

void CSTGGlobal::UpdateControlChangeEnable(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6d9) = (param.value != 0);
}

void CSTGGlobal::UpdateSysExEnable(CSTGMessageContext &, STGConvertedParam &param)
{
	*((unsigned char *)this + 0x6da) = (param.value != 0);
}

/* Same two module globals CSTGAudioBusManager's constructor resets to
 * unity gain -- see managers.cpp. */
extern float gAllPlusHeadroom[4];
extern float gAllMinusHeadroom[4];

/*
 * UpdateHeadroom (.text+0xf30, 53 bytes) confirmed:
 *   fld DWORD PTR [ecx]     ; load param.value AS A FLOAT (not int -- see oa_global.h)
 *   ... broadcast to xmm1 (shufps xmm1,xmm1,0x0), store to gAllPlusHeadroom
 *   fchs                     ; negate
 *   ... broadcast, store to gAllMinusHeadroom
 */
void CSTGGlobal::UpdateHeadroom(CSTGMessageContext &, STGConvertedParam &param)
{
	float v = *(float *)&param.value;
	gAllPlusHeadroom[0]  = gAllPlusHeadroom[1]  = gAllPlusHeadroom[2]  = gAllPlusHeadroom[3]  =  v;
	gAllMinusHeadroom[0] = gAllMinusHeadroom[1] = gAllMinusHeadroom[2] = gAllMinusHeadroom[3] = -v;
}

/*
 * RunVoiceModelFeedback (.text+0x4690, 123 bytes) confirmed: walks an
 * intrusive singly-linked list (head at `+0x29c9900`, `next` at the
 * node's own `+0x0`; each node's `+0x8` is the "payload" pointer this
 * function actually operates on). For each payload, `+0x38` is a
 * sub-object pointer whose `+0xb73` byte holds two independent gate
 * bits: bit 0 gates a check via `+0xb6b`, bit 1 gates a check via
 * `+0xb6f` -- each check dereferences that field as an object,
 * dispatches its OWN vtable slot 0x1a (`[vtable+0x68]`, identity not
 * confirmed -- modeled as a raw indirect call through the confirmed
 * offset, matching this project's established `CCostProfile`/
 * `CSTGAudioDriverInterface` precedent for an unidentified-but-real
 * vtable slot), and tests bit 0 of the RETURNED object's own `+0xe2`
 * byte. If either gated check passes, `CSTGSlotVoiceData::
 * RunVoiceModelFeedback()` is called on the payload.
 */
void CSTGGlobal::RunVoiceModelFeedback()
{
	typedef void *(*VtableSlot1aFn)(void *);

	unsigned char **node = *(unsigned char ***)((unsigned char *)this + 0x29c9900);
	while (node) {
		unsigned char *payload = *(unsigned char **)((unsigned char *)node + 0x8);
		unsigned char *sub = *(unsigned char **)(payload + 0x38);
		unsigned char *next = *(unsigned char **)node;

		unsigned char flags = *(sub + 0xb73);
		bool feedback = false;

		if (flags & 1) {
			unsigned char *obj = *(unsigned char **)(sub + 0xb6b);
			void **vtable = *(void ***)obj;
			VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
			unsigned char *result = (unsigned char *)fn(obj);
			if (result[0xe2] & 1)
				feedback = true;
			else
				flags = *(sub + 0xb73); /* confirmed real reload */
		}
		if (!feedback && (flags & 2)) {
			unsigned char *obj = *(unsigned char **)(sub + 0xb6f);
			void **vtable = *(void ***)obj;
			VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
			unsigned char *result = (unsigned char *)fn(obj);
			if (result[0xe2] & 1)
				feedback = true;
		}

		if (feedback)
			((CSTGSlotVoiceData *)payload)->RunVoiceModelFeedback();

		node = (unsigned char **)next;
	}
}

/*
 * Initialize (.text+0x8340, 267 bytes) confirmed:
 *   - Calls its OWN vtable slot 7 (`[vtable+0x1c]`) with no extra args --
 *     IDENTITY NOW CONFIRMED (sec 10.228, via `objdump -dr` on
 *     `OA_real.ko`'s `.rodata._ZTV10CSTGGlobal` relocations): resolves to
 *     `CSTGParamsOwner::UseDefaults()`. Reconstructed as a direct
 *     `reinterpret_cast<CSTGParamsOwner *>(this)->UseDefaults()` call,
 *     NOT a raw vtable-pointer dispatch -- see `CSTGParamsOwner::
 *     UseDefaults()`'s own comment in oa_global.h for why (this class
 *     deliberately has no real vtable pointer installed at `self+0x0`,
 *     same reasoning as `ValidateParamChange`'s identical-shaped
 *     forwarding call).
 *   - CSTGWaveSeqData::Initialize() on an embedded sub-object at
 *     `+0x1143c10`.
 *   - Sets flag bit `+0x67f` |= 0x2.
 *   - Constructs a 32-entry (confirmed literal `0x20`) intrusive
 *     doubly-linked list of slot-voice-data nodes, each `0x28e0` bytes
 *     apart starting at `+0x2977cf0`: `CSTGSlotVoiceData::Initialize`
 *     is called on each entry's `+0x4` sub-part with the loop index as
 *     its argument, then the entry's own `+0x8` node is appended to a
 *     list anchored by a head/tail/count triple at `+0x29c98f4`/
 *     `+0x29c98f8`/`+0x29c98fc` -- each node's own `+0x0` field gets
 *     set to point back to a FIXED anchor value (`+0x2977d04`,
 *     confirmed to be the SAME value in every iteration, not a
 *     per-node "next" pointer -- functions as an intrusive node's
 *     back-reference to its owning list/anchor, distinct from the
 *     real prev/next links at the node's own `-0x8`/`-0xc`).
 *   - `CSTGProgramModeProgramSlot::Initialize()` on a sub-object at
 *     `+0x2977b1f`, argument = the byte at `+0x6b8`.
 *   - `CSTGProgramModeDrumTrackSlot::Initialize()` on a sub-object at
 *     `+0x2977c08`, argument = the byte at `+0x6ba`.
 *   - `CSTGPerformanceVarsManager::Initialize()`, confirmed called
 *     with `this = &CSTGPerformanceVarsManager::sInstance` (the
 *     ADDRESS of the singleton pointer itself, not its value) -- real
 *     reason not determined, represented factually as confirmed rather
 *     than "corrected" to a more usual-looking call.
 *   - `CSTGControllerRTData::Initialize()` on the embedded sub-object
 *     at `+0x10` -- this CONFIRMS the aliasing question
 *     `UpdateFootswitchPolarity`'s own earlier comment left open (see
 *     oa_global.h).
 *   - `USTGAliasBankTypes::InitializeAliasBanks()`, no arguments.
 *   - `CSTGGlobal::InitializePerformances()` on `this` itself.
 *   - `CSetListBank::Initialize()` on a sub-object at `+0x293374c`.
 *   - Sets a "initialized" flag byte at `+0x6ae` = 1.
 */
/*
 * The confirmed real list-anchor fields this loop touches (head/tail
 * at +0x29c98f4/+0x29c98f8, a node's own prev/next-shaped fields at
 * +0x0/+0x4/+0xc relative to the node) are genuine 32-bit pointers on
 * the real target, tightly packed with no gaps -- so on THIS 64-bit
 * host, a native 8-byte pointer write to any one of them spills into
 * its immediate neighbor. Modeled here with explicit 32-bit storage
 * (truncating on write, extending on read) rather than native
 * `unsigned char**`, both to avoid that spillover AND because this is
 * genuinely more faithful to what the real 32-bit binary itself does
 * (a 32-bit `mov`, not a 64-bit one) -- caught via a real segfault
 * while building this method's own host KAT.
 */
static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int ToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

void CSTGGlobal::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	reinterpret_cast<CSTGParamsOwner *>(this)->UseDefaults();

	((CSTGWaveSeqData *)(self + 0x1143c10))->Initialize();

	self[0x67f] |= 0x2;

	unsigned int listAnchor = ToU32(self + 0x2977d04);
	unsigned char *entry = self + 0x2977cf0;
	for (unsigned short i = 0; i < 0x20; i++) {
		((CSTGSlotVoiceData *)(entry + 0x4))->Initialize(i);

		/*
		 * Confirmed real: the node's own three 32-bit fields are at
		 * newNode+0x0 (written with the previous tail's own OLD
		 * prev-link, only on the non-empty-list path), newNode+0x4
		 * (written with the OLD tail, only on the non-empty-list
		 * path), and newNode+0xc (ALWAYS written, the fixed anchor
		 * value) -- confirmed via the real disassembly's own
		 * esi-relative offsets (esi == newNode+0xc throughout this
		 * loop, so esi[-8]==newNode+4, esi[-0xc]==newNode+0,
		 * [esi]==newNode+0xc). An earlier draft of this
		 * reconstruction mistakenly modeled these as `newNode-0x8`/
		 * `newNode-0xc` (subtracting instead of the correct
		 * relationship derived from esi's own base) -- caught while
		 * debugging this exact segfault, not by re-reading the
		 * disassembly a second time first.
		 */
		unsigned char *newNode = entry + 0x8;
		unsigned int *tail = (unsigned int *)(self + 0x29c98f8);
		unsigned int *head = (unsigned int *)(self + 0x29c98f4);
		unsigned int *count = (unsigned int *)(self + 0x29c98fc);

		if (*tail == 0) {
			*head = ToU32(newNode);
		} else {
			unsigned char *oldTail = FromU32(*tail);
			*(unsigned int *)(newNode + 0x4) = ToU32(oldTail);
			unsigned int oldTailPrev = *(unsigned int *)oldTail;
			*(unsigned int *)(newNode + 0x0) = oldTailPrev;
			if (oldTailPrev)
				*(unsigned int *)(FromU32(oldTailPrev) + 0x4) = ToU32(newNode);
			*(unsigned int *)oldTail = ToU32(newNode);
		}
		*tail = ToU32(newNode);
		*(unsigned int *)(newNode + 0xc) = listAnchor;
		(*count)++;

		entry += 0x28e0;
	}

	((CSTGProgramModeProgramSlot *)(self + 0x2977b1f))->Initialize(self[0x6b8]);
	((CSTGProgramModeDrumTrackSlot *)(self + 0x2977c08))->Initialize(self[0x6ba]);

	((CSTGPerformanceVarsManager *)&CSTGPerformanceVarsManager::sInstance)->Initialize();

	((CSTGControllerRTData *)(self + 0x10))->Initialize();

	USTGAliasBankTypes::InitializeAliasBanks();

	InitializePerformances();

	((CSetListBank *)(self + 0x293374c))->Initialize();

	self[0x6ae] = 1;
}

/*
 * UpdateMIDIClockSource()/UpdateShowMSWSDKitGraphics() (sec 10.67):
 * confirmed real, genuinely empty -- a single `ret` in the real
 * binary, nothing else. Not a stub; independently confirmed via
 * direct disassembly.
 */
void CSTGGlobal::UpdateMIDIClockSource(CSTGMessageContext &, STGConvertedParam &) {}
void CSTGGlobal::UpdateShowMSWSDKitGraphics(CSTGMessageContext &, STGConvertedParam &) {}

/*
 * The nine `UpdateAudioInputXXX` handlers (sec 10.67): confirmed real,
 * identically-shaped thin thunks -- adjust `this` by the confirmed
 * CSTGAudioInput sub-object offset (+0x608, global_ctor.cpp sec 10.56)
 * and tail-call the correspondingly-named CSTGAudioInput method.
 */
void CSTGGlobal::UpdateAudioInputBusSelect(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateBusSelect(ctx, param);
}
void CSTGGlobal::UpdateAudioInputFXControlBus(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateFXControlBus(ctx, param);
}
void CSTGGlobal::UpdateAudioInputHDRBus(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateHDRBus(ctx, param);
}
void CSTGGlobal::UpdateAudioInputLevel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateLevel(ctx, param);
}
void CSTGGlobal::UpdateAudioInputMute(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateMute(ctx, param);
}
void CSTGGlobal::UpdateAudioInputPan(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdatePan(ctx, param);
}
void CSTGGlobal::UpdateAudioInputSend1Level(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateSend1Level(ctx, param);
}
void CSTGGlobal::UpdateAudioInputSend2Level(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateSend2Level(ctx, param);
}
void CSTGGlobal::UpdateAudioInputSolo(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	((CSTGAudioInput *)(reinterpret_cast<unsigned char *>(this) + 0x608))->UpdateSolo(ctx, param);
}

/*
 * UpdateRTKnobFuncMIDIChannel()/UpdatePadFuncMIDIChannel() (sec
 * 10.67): see the class comment in oa_global.h for the confirmed
 * shape shared by both. `valueArrayOffset`/`selectorArrayOffset` are
 * the two confirmed 8-slot arrays each one uses; the final mirror
 * write reuses the SAME shared array (+0x29cc11d) the plain
 * `UpdateXXXMIDIChannel` family (above) also writes into.
 */
void CSTGGlobal::UpdateIndexedMIDIChannelField(unsigned int valueArrayOffset,
						unsigned int selectorArrayOffset,
						CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (ctx.index > 7)
		return;
	base[valueArrayOffset + ctx.index] = (unsigned char)param.value;
	signed char selector = (signed char)base[selectorArrayOffset + ctx.index];
	if (selector < 0)
		return;
	unsigned char newValue = base[valueArrayOffset + ctx.index];
	base[0x29cc11d + (unsigned int)selector * 8] = newValue;
}
void CSTGGlobal::UpdateRTKnobFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateIndexedMIDIChannelField(0x29cc0da, 0x29cc0e2, ctx, param); }
void CSTGGlobal::UpdatePadFuncMIDIChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateIndexedMIDIChannelField(0x29cc0ea, 0x29cc0f2, ctx, param); }

/*
 * UpdateAudioClockSource() (sec 10.67): see oa_global.h for the
 * confirmed command-code mapping.
 */
void CSTGGlobal::UpdateAudioClockSource(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6ae] == 0)
		return;
	int command;
	if (param.value == 1)
		command = 0x7800000;
	else if (param.value == 2)
		command = 0x7020000;
	else
		command = 0x7000000;
	OmapNKS4OutputFifo_WriteCommand(command);
}

/*
 * UpdateFootSwitchAssign() (sec 10.67): confirmed real 55-entry
 * `.rodata` table, extracted directly from the binary (.rodata+0x20).
 */
static const signed char kFootSwitchAssignMap[55] = {
	0x00, 0x10, 0x0d, 0x0e, 0x0f, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x1a, 0x1b, 0x01, 0x02, 0x0b, 0x0c, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
};
void CSTGGlobal::UpdateFootSwitchAssign(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6ae] == 0)
		return;
	void *self = base + 0x10;
	signed char value = kFootSwitchAssignMap[(unsigned int)param.value];
	((CSTGControllerRTData *)self)->SetControllerAssignment(self, value, false);
}

/*
 * UpdateFootPedalAssign() (sec 10.69): confirmed real 32-entry
 * `.rodata` table, extracted directly from the binary (.rodata+0x0) --
 * a DIFFERENT table from UpdateFootSwitchAssign's own.
 */
static const signed char kFootPedalAssignMap[32] = {
	0x00, 0x13, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x12,
	0x11, 0x01, 0x02, 0x0b, 0x0c, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
	0x22, 0x23, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d,
};
void CSTGGlobal::UpdateFootPedalAssign(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6ae] == 0)
		return;
	signed char lookup = kFootPedalAssignMap[(unsigned int)param.value];
	*(int *)(base + 0x18) = lookup;
	signed char value = (base[0x15] != 0) ? lookup : 0;
	void *self = base + 0x10;
	void *selfRef = base + 0x13;
	((CSTGControllerRTData *)self)->SetControllerAssignment(selfRef, value, true);
}

/*
 * UpdateKnobFaderMode() (sec 10.69): confirmed real read-before-write
 * change-detection, see oa_global.h for the full confirmed shape.
 */
void CSTGGlobal::UpdateKnobFaderMode(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char newValue = (param.value != 0) ? 1 : 0;
	unsigned char oldValue = base[0x29c9fc0];
	base[0x29c9fc0] = newValue;
	if (base[0x6ae] == 0)
		return;
	if (oldValue == newValue)
		return;
	CSTGControllerRTData::sInstance->ResetAllJumpCatch();
}

/*
 * UpdateConvertPosition() (sec 10.70): see oa_global.h for the
 * confirmed shape.
 */
void CSTGGlobal::UpdateConvertPosition(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6ae] != 0 && ((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0)
		CSTGVoiceAllocator::sInstance->StealAllVoices();
	*(int *)(base + 0x6b0) = param.value;
}

/*
 * UpdateUserOctaveScale() (sec 10.70): see oa_global.h for the
 * confirmed shape (division-by-12 index into a [quotient][remainder]
 * table).
 */
void CSTGGlobal::UpdateUserOctaveScale(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	int index = (int)ctx.index;
	int quotient = index / 12;
	int remainder = index % 12;
	unsigned char byteQuotient = (unsigned char)quotient;
	unsigned int offset = 0x29c9a98u + (unsigned int)remainder * 4 + (unsigned int)byteQuotient * 48;
	*(int *)(base + offset) = param.value;
}

/*
 * ResolveActivePerformanceVarsManager() (sec 10.71): shared helper,
 * see CSTGPerformanceVarsManager::sInstance's own declaration in
 * oa_global.h for the confirmed real 12-byte layout this resolves.
 *
 * Host/target pointer-width fix (this project's established pattern):
 * the real target's two pointer slots are packed 32-bit fields, but a
 * native host pointer is 8 bytes on a 64-bit build -- read each slot
 * as a plain 32-bit value and reconstruct a host pointer from it,
 * rather than treating `sInstance` as a native `T*[2]`.
 */
CSTGPerformanceVarsManager *CSTGGlobal::ResolveActivePerformanceVarsManager()
{
	unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
	unsigned char selector = slots[8];
	unsigned int slotValue = *(unsigned int *)(slots + (unsigned int)selector * 4);
	return (CSTGPerformanceVarsManager *)(unsigned long)slotValue;
}

/*
 * UpdatePerfChangeHoldTime() (sec 10.71): see oa_global.h for the
 * confirmed shape.
 */
void CSTGGlobal::UpdatePerfChangeHoldTime(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	float value = *(float *)&param.value;
	float multiplier = *(float *)((unsigned char *)CSTGAudioBusManager::sInstance + 4);
	int converted = (int)(value * multiplier);
	*(int *)(base + 0x6e0) = converted;
	if (base[0x6ae] == 0)
		return;
	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	unsigned char *mgrBytes = (unsigned char *)mgr;
	if (mgrBytes[0x23d1] != 2)
		return;
	if (mgrBytes[0x23dc] != 0)
		return;
	*(int *)(mgrBytes + 0x23e0) = converted;
}

/*
 * UpdateExtSetSelect() (sec 10.71): see oa_global.h for the confirmed
 * shape, including the real quirk of one branch skipping the bit-set
 * entirely.
 */
void CSTGGlobal::UpdateExtSetSelect(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	base[0x29cc0c8] = (unsigned char)param.value;

	if (base[0x6ae] == 0) {
		base[0x29cc0c9] |= 1;
		return;
	}
	if ((base[0x29cc0c9] & 1) != 0) {
		base[0x29cc0c9] |= 1;
		return;
	}
	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	if (((unsigned char *)mgr)[0x23d1] == 2) {
		CSTGControllerRTData::sInstance->OnExtModeSetChange();
		return;
	}
	base[0x29cc0c9] |= 1;
}

/*
 * UpdateExtAssign() (sec 10.72): shared helper for the 8
 * UpdateExtXXXCCAssign/UpdateExtXXXMidiChannel handlers below. See
 * oa_global.h for the full confirmed shape.
 */
template <typename NotifyFn>
void CSTGGlobal::UpdateExtAssign(unsigned int writeOffset, unsigned int stride,
				  CSTGMessageContext &ctx, STGConvertedParam &param,
				  NotifyFn notify)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int slotIndex = base[0x29cc0c8];
	unsigned char *slot = base + slotIndex * stride + ctx.index;
	slot[writeOffset] = (unsigned char)param.value;

	if (base[0x6ae] == 0) {
		base[0x29cc0c9] |= 1;
		return;
	}
	if ((base[0x29cc0c9] & 1) != 0) {
		base[0x29cc0c9] |= 1;
		return;
	}
	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	if (((unsigned char *)mgr)[0x23d1] == 2) {
		notify(ctx.index);
		return;
	}
	base[0x29cc0c9] |= 1;
}

void CSTGGlobal::UpdateExtKnobCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29ca3c8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeKnobAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtKnobMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29c9fc8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeKnobAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtPlayMuteSwitchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29cabc8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModePlayMuteSwitchAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtPlayMuteSwitchMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29ca7c8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModePlayMuteSwitchAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtSelectSwitchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29cb3c8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeSelectSwitchAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtSelectSwitchMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29cafc8, 8, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeSelectSwitchAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtSliderCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29cbc48, 9, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeSliderAssignChange(index);
	});
}
void CSTGGlobal::UpdateExtSliderMidiChannel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	UpdateExtAssign(0x29cb7c8, 9, ctx, param, [](unsigned int index) {
		CSTGControllerRTData::sInstance->OnExtModeSliderAssignChange(index);
	});
}

/*
 * UpdateFXDisable() (sec 10.73): shared helper for UpdateMFX/IFX/
 * TFXDisable. See oa_global.h for the full confirmed shape, including
 * the confirmed algebraic equivalence of the real bit-twiddling
 * sequence to this simpler mask formula.
 */
void CSTGGlobal::UpdateFXDisable(unsigned char mask, unsigned char ccNumber,
				  CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char bit = (param.value != 0) ? 1 : 0;
	unsigned char flags = base[0x6d4];
	flags = (unsigned char)((flags & ~mask) | (bit ? mask : 0));
	base[0x6d4] = flags;

	if (base[0x6ae] == 0)
		return;

	unsigned char msg[3];
	msg[0] = (unsigned char)(base[0x6b8] | 0xb0);
	msg[1] = ccNumber;
	msg[2] = (param.value == 0) ? 0x7f : 0x00;
	CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 3);
}
void CSTGGlobal::UpdateMFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateFXDisable(0x12, 0x5e, ctx, param); }
void CSTGGlobal::UpdateIFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateFXDisable(0x09, 0x5c, ctx, param); }
void CSTGGlobal::UpdateTFXDisable(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateFXDisable(0x24, 0x5f, ctx, param); }

/*
 * UpdateCCAssign() (sec 10.74): shared helper for the 22
 * UpdateXXXCCAssign handlers. See oa_global.h for the full confirmed
 * shape, including the confirmed real jump-into-the-assign-block
 * quirk on the "already sentinel" deassign path.
 */
void CSTGGlobal::UpdateCCAssign(unsigned int selectorOffset, unsigned int tag,
				 CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char paramByte = (unsigned char)param.value;

	if ((signed char)paramByte <= 0x77) {
		for (unsigned int i = 0; i < 0x78; i++) {
			unsigned char *slot = base + 0x29cc11c + i * 8;
			if (slot[2] == 0 && *(unsigned int *)(slot + 4) == tag)
				slot[0] = 0;
		}
	}

	unsigned char freshByte = (unsigned char)param.value;
	if (freshByte == 0xff) {
		signed char oldSelector = (signed char)base[selectorOffset];
		if (oldSelector >= 0) {
			base[selectorOffset] = 0xff;
			(base + 0x29cc11c + (unsigned int)(unsigned char)oldSelector * 8)[0] = 0;
			return;
		}
		/* else falls through into the assign block below with
		 * freshByte still 0xff, matching the real binary's own
		 * jump into the middle of that same code. */
	}

	base[selectorOffset] = freshByte;
	unsigned char *slot = base + 0x29cc11c + (unsigned int)freshByte * 8;
	slot[0] = 1;
	slot[2] = 0;
	unsigned char valueByte = base[selectorOffset - 1];
	*(unsigned int *)(slot + 4) = tag;
	slot[1] = valueByte;
}
void CSTGGlobal::UpdateProgramUpCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0cb, 0x14, ctx, param); }
void CSTGGlobal::UpdateProgramDownCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0cd, 0x15, ctx, param); }
void CSTGGlobal::UpdateSongStartCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0cf, 0x16, ctx, param); }
void CSTGGlobal::UpdateSongPunchCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0d1, 0x17, ctx, param); }
void CSTGGlobal::UpdateTapTempoCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0d3, 0x18, ctx, param); }
void CSTGGlobal::UpdateOctaveUpCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0d5, 0x04, ctx, param); }
void CSTGGlobal::UpdateOctaveDownCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0d7, 0x03, ctx, param); }
void CSTGGlobal::UpdateRibbonLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0d9, 0x09, ctx, param); }
void CSTGGlobal::UpdateJSXLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0fb, 0x05, ctx, param); }
void CSTGGlobal::UpdateJSYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0fd, 0x06, ctx, param); }
void CSTGGlobal::UpdateJSPYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc0ff, 0x07, ctx, param); }
void CSTGGlobal::UpdateJSMYLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc101, 0x08, ctx, param); }
void CSTGGlobal::UpdateJSXRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc103, 0x0a, ctx, param); }
void CSTGGlobal::UpdateJSYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc105, 0x0b, ctx, param); }
void CSTGGlobal::UpdateJSPYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc107, 0x0c, ctx, param); }
void CSTGGlobal::UpdateJSMYRibLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc109, 0x0d, ctx, param); }
void CSTGGlobal::UpdateSW1FuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc10b, 0x24, ctx, param); }
void CSTGGlobal::UpdateSW2FuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc10d, 0x25, ctx, param); }
void CSTGGlobal::UpdateIncFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc10f, 0x3e, ctx, param); }
void CSTGGlobal::UpdateDecFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc111, 0x3f, ctx, param); }
void CSTGGlobal::UpdateDTrackEnableCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc115, 0x41, ctx, param); }
void CSTGGlobal::UpdateAftertouchLockCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc117, 0x0e, ctx, param); }

/*
 * UpdateChordSwCCAssign() (sec 10.75): the 23rd real UpdateXXXCCAssign
 * member -- see oa_global.h for why no special-cased body is needed.
 */
void CSTGGlobal::UpdateChordSwCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateCCAssign(0x29cc113, 0x40, ctx, param); }

/*
 * UpdateChangeEnable() (sec 10.75): shared helper for
 * UpdateProgramChangeEnable/UpdateBankChangeEnable. See oa_global.h
 * for the full confirmed shape.
 */
void CSTGGlobal::UpdateChangeEnable(unsigned int flagOffset, CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char enabled = (param.value != 0) ? 1 : 0;
	base[flagOffset] = enabled;
	if (enabled != 0)
		return;

	unsigned char *disp = (unsigned char *)CSTGMidiDispatcher::sInstance;
	for (unsigned int i = 0; i < 16; i++) {
		disp[0x30 + i] = 0;
		disp[0x50 + i] = 0;
	}
}
void CSTGGlobal::UpdateProgramChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateChangeEnable(0x6d5, ctx, param); }
void CSTGGlobal::UpdateBankChangeEnable(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateChangeEnable(0x6d6, ctx, param); }

/*
 * UpdateMasterTune() (sec 10.75): see oa_global.h for the full
 * confirmed shape.
 *
 * Host/target pointer-width fix (this project's established pattern):
 * the real target's list-node next/payload pointers and the list-head
 * array are plain 32-bit fields, but a native host pointer is 8 bytes
 * on a 64-bit build -- read/write each as a truncated `unsigned int`
 * and reconstruct a host pointer from it, rather than treating them
 * as native `T*` fields.
 */
static void *MasterTuneFromU32(unsigned int v) { return (void *)(unsigned long)v; }

static void UpdateGlobalTuneOnList(unsigned int listHead, float tune)
{
	unsigned int node = listHead;
	while (node != 0) {
		unsigned char *nodePtr = (unsigned char *)MasterTuneFromU32(node);
		unsigned int payload = *(unsigned int *)(nodePtr + 8);
		unsigned int next = *(unsigned int *)(nodePtr + 0);
		((CSTGSlotVoiceData *)MasterTuneFromU32(payload))->UpdateGlobalTune(tune);
		node = next;
	}
}
void CSTGGlobal::UpdateMasterTune(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 0x6bc) = param.value;
	if (base[0x6ae] == 0)
		return;

	float tune = *(float *)&param.value;
	unsigned char *globalBase = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char activeSlot = globalBase[0x6b8]; /* confirmed real: read from
							* CSTGGlobal::sInstance, not
							* `this` (the disassembly
							* explicitly re-fetches
							* sInstance right before this
							* read) */
	for (unsigned int i = 0; i < 16; i++) {
		unsigned int listHead = *(unsigned int *)(globalBase + 0x29c99cc + i * 12);
		UpdateGlobalTuneOnList(listHead, tune);
		if (i == activeSlot) {
			unsigned int specialHead = *(unsigned int *)(globalBase + 0x29c9a8c);
			UpdateGlobalTuneOnList(specialHead, tune);
		}
	}
}

/*
 * UpdateIndexedCCAssign() (sec 10.76): shared helper for
 * UpdateRTKnobFuncCCAssign/UpdatePadFuncCCAssign. See oa_global.h for
 * the full confirmed shape, including how it differs from the plain
 * UpdateCCAssign() helper.
 */
void CSTGGlobal::UpdateIndexedCCAssign(unsigned int valueArrayOffset, unsigned int selectorArrayOffset,
					unsigned int tagBase, CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if (ctx.index > 7)
		return;
	if ((signed char)(unsigned char)param.value > 0x77)
		return;

	unsigned int tag = ctx.index + tagBase;
	for (unsigned int i = 0; i < 0x78; i++) {
		unsigned char *slot = base + 0x29cc11c + i * 8;
		if (slot[2] == 0 && *(unsigned int *)(slot + 4) == tag)
			slot[0] = 0;
	}

	unsigned char freshByte = (unsigned char)param.value;
	if (freshByte == 0xff) {
		signed char oldSelector = (signed char)base[selectorArrayOffset + ctx.index];
		if (oldSelector >= 0) {
			(base + 0x29cc11c + (unsigned int)(unsigned char)oldSelector * 8)[0] = 0;
			base[selectorArrayOffset + ctx.index] = 0xff;
			return;
		}
		/* else falls through into the assign block below with
		 * freshByte still 0xff, matching the real binary's own
		 * jump into the middle of that same code. */
	}

	base[selectorArrayOffset + ctx.index] = freshByte;
	unsigned char selVal = base[selectorArrayOffset + ctx.index];
	unsigned char *slot = base + 0x29cc11c + (unsigned int)selVal * 8;
	slot[0] = 1;
	unsigned char valueByte = base[valueArrayOffset + ctx.index];
	slot[1] = valueByte;
	slot[2] = 0;
	*(unsigned int *)(slot + 4) = ctx.index + tagBase;
}
void CSTGGlobal::UpdateRTKnobFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateIndexedCCAssign(0x29cc0da, 0x29cc0e2, 0x1c, ctx, param); }
void CSTGGlobal::UpdatePadFuncCCAssign(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateIndexedCCAssign(0x29cc0ea, 0x29cc0f2, 0x36, ctx, param); }

/*
 * ResolveCurrentPerformance()/UpdateVJSAssignment() (sec 10.77): see
 * oa_global.h for the full confirmed shape.
 */
static CSTGPerformance *ResolveCurrentPerformance(unsigned char *base)
{
	int mode = *(int *)(base + 0x684);
	if (mode == 1) {
		int idx = (*(int *)(base + 0x69c)) & 0x7f;
		int bank = *(int *)(base + 0x690);
		int offset = idx * 0x19e7 + bank * 0xcf381 + 0x1c77f10 + 6;
		return (CSTGPerformance *)(base + offset);
	}
	if (mode == 2) {
		int seqIdx = *(int *)(base + 0x6a0);
		int offset = seqIdx * 0x1cad + 0x27cd024;
		return (CSTGPerformance *)(base + offset);
	}
	int progIdx = *(int *)(base + 0x698);
	if (progIdx == 0xfffe)
		return (CSTGPerformance *)(base + 0x2976e33);
	int idx = progIdx & 0x7f;
	int bank = *(int *)(base + 0x68c);
	int offset = idx * 0xcec + bank * 0x67603 + 0x132e4d0 + 3;
	return (CSTGPerformance *)(base + offset);
}
void CSTGGlobal::UpdateVJSAssignment(unsigned int selectorFieldOffset,
				      CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	base[selectorFieldOffset] = (unsigned char)param.value;
	if (base[0x6ae] == 0)
		return;

	CSTGPerformance *perf = ResolveCurrentPerformance(base);
	if (!perf->IsCurrentlyActive())
		return;

	signed char selector = (signed char)base[selectorFieldOffset];
	if (selector < 0)
		return;

	unsigned char channel = base[0x6b9];
	CSTGMidiDispatcher::sInstance->HandleController(channel, (unsigned char)selector, 0x40, 1, -1);
}
void CSTGGlobal::UpdateVJSXAssignment(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateVJSAssignment(0x6c0, ctx, param); }
void CSTGGlobal::UpdateVJSYAssignment(CSTGMessageContext &ctx, STGConvertedParam &param)
{ UpdateVJSAssignment(0x6c1, ctx, param); }

/*
 * CSTGControllerRTData::NotifySoloChange() (sec 10.107, .text+0x1d710,
 * 170 bytes) confirmed: resolves "the current performance object" via
 * the SAME confirmed real 3-way mode dispatch as `ResolveCurrentPerformance()`
 * above (byte-for-byte identical strides/literals confirmed against a
 * fresh disassembly of this function -- a strong independent
 * cross-check of that helper), then makes a genuine VIRTUAL call
 * (through the resolved object's own vtable, slot 0x1b/27 -- NOT the
 * direct non-virtual `IsCurrentlyActive()` call `UpdateVJSAssignment()`
 * uses) with no further arguments beyond the implicit `this`. This
 * class's own virtual method layout isn't otherwise modeled in this
 * project, so the call is made through the raw vtable pointer, matching
 * this project's established convention for not-yet-fully-modeled
 * vtable dispatches (e.g. `RunVoiceModelFeedback()`'s own slot 0x1a
 * calls, above -- unlike `CSTGGlobal::Initialize()`'s OWN dispatch,
 * fixed sec 10.228 to a direct `reinterpret_cast` call instead, since
 * `CSTGGlobal` itself deliberately has no real installed vtable
 * pointer). Ignores its own `this` (a `CSTGControllerRTData*`) entirely,
 * operating purely via the global `CSTGGlobal::sInstance` singleton.
 */
void CSTGControllerRTData::NotifySoloChange()
{
	unsigned char *base = (unsigned char *)CSTGGlobal::sInstance;
	CSTGPerformance *perf = ResolveCurrentPerformance(base);
	typedef void (*VtableSlot1bFn)(void *);
	void **vtable = *(void ***)perf;
	((VtableSlot1bFn)vtable[0x1b])(perf);
}

/*
 * CSTGEffectRackVars::UpdateDModRoutings() (sec 10.135): see
 * oa_global.h for the full confirmed shape.
 */
void CSTGEffectRackVars::UpdateDModRoutings()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *p1 = *(unsigned char **)(self + 0x2114);
	unsigned char *p2 = *(unsigned char **)(p1 + 0x23d4);
	typedef void (*VtableSlot33Fn)(void *, void *);
	void **vtable = *(void ***)p2;
	((VtableSlot33Fn)vtable[0x84 / 4])(p2, self);
}

/*
 * Shared 5-byte SysEx-like message helper used by
 * UpdateKeyTranspose/UpdateLocalControl/UpdateMIDIChannel (sec
 * 10.78): {channelByte, 0x79, byte2, 0x05, 0xff} sent via the
 * embedded CSTGMidiQueueWriter sub-object at
 * CSTGMidiPortManager::sInstance+0x208.
 */
static void SendGlobalMidiMessage(unsigned char channelByte, unsigned char byte2)
{
	unsigned char msg[5] = { channelByte, 0x79, byte2, 0x05, 0xff };
	CSTGMidiQueueWriter *writer =
		(CSTGMidiQueueWriter *)((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
	writer->Write(msg, 5, false);
}

/*
 * UpdateProgramDrumTrackMidiChannel() (sec 10.78): see oa_global.h
 * for the full confirmed shape.
 */
void CSTGGlobal::UpdateProgramDrumTrackMidiChannel(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char oldChannel = base[0x6ba];

	if (base[0x6ae] != 0) {
		if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0) {
			CSTGSmoother::sInstance->CancelAllSmoothers();
			CSTGVoiceAllocator::sInstance->StealAllVoices();
		}
		CSTGMidiDispatcher::sInstance->ResetAllControllers(oldChannel, false);
	}

	unsigned char newChannel = (unsigned char)param.value;
	base[0x6ba] = newChannel;
	((CSTGProgramModeDrumTrackSlot *)(base + 0x2977c08))->OnUpdateProgramDrumTrackMidiChannel(param.value);

	if (base[0x6ae] != 0)
		CSTGMidiDispatcher::sInstance->ResetAllControllers(newChannel, false);
}

/*
 * UpdateKeyTranspose() (sec 10.78): see oa_global.h for the full
 * confirmed shape.
 */
void CSTGGlobal::UpdateKeyTranspose(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;

	if (base[0x6ae] == 0) {
		base[0x6ad] = (unsigned char)param.value;
		return;
	}
	if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0)
		CSTGVoiceAllocator::sInstance->StealAllVoices();

	base[0x6ad] = (unsigned char)param.value;
	if (base[0x6ae] == 0)
		return;

	SendGlobalMidiMessage((unsigned char)(base[0x6b8] | 0xb0), 0x09);
}

/*
 * UpdateLocalControl(bool)/UpdateLocalOn() (sec 10.78): see
 * oa_global.h for the full confirmed shape.
 */
void CSTGGlobal::UpdateLocalControl(bool state)
{
	unsigned char *base = (unsigned char *)this;

	if (base[0x6ae] == 0) {
		base[0x6af] = (unsigned char)state;
		return;
	}
	if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0)
		CSTGVoiceAllocator::sInstance->StealAllVoices();

	unsigned char channel = base[0x6b9];
	CSTGMidiDispatcher::sInstance->ResetAllControllers(channel, false);

	base[0x6af] = (unsigned char)state;
	if (base[0x6ae] == 0)
		return;

	SendGlobalMidiMessage((unsigned char)(base[0x6b8] | 0xb0), state ? 5 : 6);
	CSTGControllerRTData::sInstance->ResetAllJumpCatch();
}
void CSTGGlobal::UpdateLocalOn(CSTGMessageContext &, STGConvertedParam &param)
{
	UpdateLocalControl(param.value != 0);
}

/*
 * UpdateMIDIChannel() (sec 10.78): see oa_global.h for the full
 * confirmed shape.
 */
void CSTGGlobal::UpdateMIDIChannel(CSTGMessageContext &, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;

	if (base[0x6ae] != 0) {
		if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0) {
			CSTGSmoother::sInstance->CancelAllSmoothers();
			CSTGVoiceAllocator::sInstance->StealAllVoices();

			unsigned char oldChannel = base[0x6b8];
			unsigned int node = *(unsigned int *)(base + 0x29c99cc + (unsigned int)oldChannel * 12);
			while (node != 0) {
				unsigned char *nodePtr = (unsigned char *)(unsigned long)node;
				unsigned int payload = *(unsigned int *)(nodePtr + 8);
				unsigned int next = *(unsigned int *)(nodePtr + 0);
				if (payload != 0) {
					unsigned char *heldKeyList = (unsigned char *)(unsigned long)payload + 0x1e80;
					((CSTGHeldKeyList *)heldKeyList)->Reset();
				}
				node = next;
			}
		}
		unsigned char channel = base[0x6b8];
		CSTGMidiDispatcher::sInstance->ResetAllControllers(channel, false);
	}

	unsigned char newChannel = (unsigned char)param.value;
	base[0x6b8] = newChannel;
	CSTGVectorManager::sInstance->OnUpdateGlobalMidiChannel(newChannel); /* sec 10.124 */
	if (*(int *)(base + 0x684) != 2)
		base[0x6b9] = newChannel;
	((CSTGProgramModeProgramSlot *)(base + 0x2977b1f))->OnUpdateGlobalMidiChannel(param.value);

	if (base[0x6ae] == 0)
		return;

	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	unsigned char *mgrBytes = (unsigned char *)mgr;
	if (mgrBytes[0x23d1] == 2) {
		((CSTGEffectRackVars *)(mgrBytes + 0x20))->UpdateDModRoutings();
		return;
	}
	SendGlobalMidiMessage((unsigned char)(base[0x6b8] | 0xb0), 0x08);
}

/*
 * CSTGAudioInput (sec 10.80) -- see oa_global.h for the confirmed
 * layout/behavior summary.
 */

/*
 * ResolveActivePerformanceVarsManagerRaw() -- the same
 * `sInstance[8]`-selector idiom as CSTGGlobal::
 * ResolveActivePerformanceVarsManager() (sec 10.71), replicated as a
 * free function since CSTGAudioInput isn't a CSTGGlobal method. Returns
 * the raw resolved byte pointer (not a typed CSTGPerformanceVarsManager*)
 * since callers here treat the result differently per handler (some
 * dereference +8 directly, one reinterprets it as a CSTGAudioInputMixerBase*).
 */
unsigned char *ResolveActivePerformanceVarsManagerRaw()
{
	unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
	unsigned char selector = slots[8];
	unsigned int slotValue = *(unsigned int *)(slots + (unsigned int)selector * 4);
	return (unsigned char *)(unsigned long)slotValue;
}

/*
 * CSTGAudioInput::CSTGAudioInput() (.text+0xc9ea0, confirmed): sets the
 * vtable pointer, zeroes +0x64..+0x76 (19 bytes) ONLY -- +0x04..+0x63
 * (level/pan/send1/send2) are left as whatever memory already held, a
 * genuine confirmed quirk preserved verbatim. +0x77's flags byte is
 * read-modify-written (bit0 set, bit1 cleared, other bits preserved from
 * pre-existing memory) rather than assigned outright -- also preserved
 * verbatim.
 */
CSTGAudioInput::CSTGAudioInput()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char flags = base[0x77];
	for (int i = 0x64; i <= 0x76; i++)
		base[i] = 0;
	flags = (unsigned char)((flags | 0x1) & ~0x2);
	base[0x77] = flags;
}

/* UpdateLevel (.text+0xc9b30): local store at +0x4+idx*4, active-branch
 * direct write to *(mgr+8)+idx*0x90+0x78. */
void CSTGAudioInput::UpdateLevel(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	int value = param.value;
	*(int *)(base + 0x4 + index * 4) = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	unsigned char *region = *(unsigned char **)(mgr + 8) + index * 0x90;
	*(int *)(region + 0x78) = value;
}

/* UpdateSend1Level (.text+0xc9b70): same shape as UpdateLevel, +0x34 local,
 * +0x7c active. */
void CSTGAudioInput::UpdateSend1Level(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	int value = param.value;
	*(int *)(base + 0x34 + index * 4) = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	unsigned char *region = *(unsigned char **)(mgr + 8) + index * 0x90;
	*(int *)(region + 0x7c) = value;
}

/* UpdateSend2Level (.text+0xc9bb0): same shape, +0x4c local, +0x80 active. */
void CSTGAudioInput::UpdateSend2Level(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	int value = param.value;
	*(int *)(base + 0x4c + index * 4) = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	unsigned char *region = *(unsigned char **)(mgr + 8) + index * 0x90;
	*(int *)(region + 0x80) = value;
}

/*
 * UpdateSolo (.text+0xc9bf0): UNLIKE the others, has NO local storage at
 * all and does nothing when the "performance active" flag is clear --
 * confirmed real, this handler is purely a live pass-through to
 * CSTGControllerRTData::SetAudioInSolo(index, param.value != 0) gated on
 * flags bit1, using ctx.index directly (no per-slot bounds check here --
 * the real disassembly has none for this one method).
 */
void CSTGAudioInput::UpdateSolo(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	if ((base[0x77] & 0x2) == 0)
		return;
	CSTGControllerRTData::sInstance->SetAudioInSolo(ctx.index, param.value != 0);
}

/* UpdateHDRBus (.text+0xc9c20): local byte store at +0x70+idx, active
 * branch calls CSTGAudioInputMixerBase::SetHDRBus(idx, value) directly
 * on the resolved manager pointer reinterpreted as the mixer object
 * (confirmed: no +8/+idx*0x90 offset applied here, unlike Level/etc). */
void CSTGAudioInput::UpdateHDRBus(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	unsigned char value = (unsigned char)param.value;
	base[0x70 + index] = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGAudioInputMixerBase *)mgr)->SetHDRBus(index, (signed char)value);
}

/* UpdateFXControlBus (.text+0xc9c70): same shape, +0x6a local,
 * SetFXCtrlBus active. */
void CSTGAudioInput::UpdateFXControlBus(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	unsigned char value = (unsigned char)param.value;
	base[0x6a + index] = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGAudioInputMixerBase *)mgr)->SetFXCtrlBus(index, (signed char)value);
}

/* UpdatePan (.text+0xc9cc0): local float store at +0x1c+idx*4. Active
 * branch has a confirmed real quirk: if the stored value is < 0, it's
 * first negated-and-scaled by a real constant (0x328/0x140 rodata
 * constants -- a real "center-to-signed-range" pan curve adjustment,
 * exact constant values not independently re-derived here, preserved as
 * the literal float ops the disassembly shows) before being passed to
 * SetPan; if >= 0 it's passed through unmodified. */
void CSTGAudioInput::UpdatePan(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	float value = *(float *)&param.value;
	*(float *)(base + 0x1c + index * 4) = value;
	if ((base[0x77] & 0x2) == 0)
		return;
	float sendValue = value;
	if (value < 0.0f)
		sendValue = (float)((double)-value * 1.0);
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGAudioInputMixerBase *)mgr)->SetPan(index, sendValue);
}

/*
 * UpdateMute (.text+0xc9d30): local bit store/clear in the +0x76
 * mask byte (bit position = index, no explicit bounds check on index in
 * the real disassembly for this handler -- `shl %cl,%edx`/`rol %cl,%edx`
 * with cl>31 would be UB on real x86 too, so the real code implicitly
 * relies on index staying small via its caller). Active branch writes
 * the isolated single bit (0/1) to *(mgr+8)+idx*0x90+0x84.
 */
void CSTGAudioInput::UpdateMute(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	unsigned char mask = base[0x76];
	if (param.value != 0)
		mask = (unsigned char)(mask | (1u << index));
	else
		mask = (unsigned char)(mask & ~(1u << index));
	base[0x76] = mask;
	if ((base[0x77] & 0x2) == 0)
		return;
	unsigned char bit = (unsigned char)((mask >> index) & 0x1);
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	unsigned char *region = *(unsigned char **)(mgr + 8) + index * 0x90;
	region[0x84] = bit;
}

/*
 * UpdateBusSelect (.text+0xc9d90): local byte store at +0x64+idx, active
 * branch calls CSTGAudioInputMixerBase::SetOutputBus(idx, value)
 * directly on the resolved mixer pointer (same shape as UpdateHDRBus/
 * UpdateFXControlBus). ADDITIONALLY -- a confirmed real tail only this
 * handler has -- after the mixer call, resolves one of three physical-
 * bus lookup-table addresses depending on CSTGGlobal::sInstance's own
 * "mode" field (+0x684, the same field UpdateMIDIChannel already reads,
 * sec 10.78), landing on a CSTGControllerRTData-owned table entry; if
 * that table's own +0x2b byte reads 7 AND this object's own slot (+0xae5,
 * matched against the raw ctx.index byte) AND a +0xad7 flag byte are
 * both zero, calls CSTGControllerRTData::ResetSendKnobsJumpCatch(). The
 * three magic per-mode multipliers/offsets (0x67603/0xcec/0x132e4d0 for
 * mode 0 (default)/other, 0xcf381/0x19e7/0x1c77f10 for mode 1, 0x1cad/
 * 0x27cd024 for mode 2) are reproduced as literal constants exactly as
 * disassembled -- not independently re-derived or simplified, matching
 * this project's own "reproduce real quirks faithfully" convention.
 */
void CSTGAudioInput::UpdateBusSelect(CSTGMessageContext &ctx, STGConvertedParam &param)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int index = ctx.index;
	if (index > 5)
		return;
	unsigned char value = (unsigned char)param.value;
	base[0x64 + index] = value;
	if ((base[0x77] & 0x2) == 0)
		return;

	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGAudioInputMixerBase *)mgr)->SetOutputBus(index, (signed char)value);

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	int mode = *(int *)(g + 0x684);
	unsigned char *entry;
	if (mode == 1) {
		unsigned int t = *(unsigned int *)(g + 0x69c) & 0x7f;
		unsigned int mult = *(unsigned int *)(g + 0x690) * 0xcf381u;
		unsigned int addr = mult + t * 0x19e7u + 0x1c77f10u;
		entry = g + addr + 0x6;
	} else if (mode == 2) {
		unsigned int addr = *(unsigned int *)(g + 0x6a0) * 0x1cadu + 0x27cd024u;
		entry = g + addr;
	} else {
		unsigned int tRaw = *(unsigned int *)(g + 0x698);
		if (tRaw == 0xfffe) {
			entry = g + 0x2976e33u;
		} else {
			unsigned int t = tRaw & 0x7f;
			unsigned int mult = *(unsigned int *)(g + 0x68c) * 0x67603u;
			unsigned int addr = mult + t * 0xcecu + 0x132e4d0u;
			entry = g + addr + 0x3;
		}
	}

	unsigned char *rt = (unsigned char *)CSTGControllerRTData::sInstance;
	if (rt[0x2b] != 7)
		return;
	if (entry[0xae5] != (unsigned char)ctx.index)
		return;
	if (entry[0xad7] != 0)
		return;
	CSTGControllerRTData::sInstance->ResetSendKnobsJumpCatch();
}

/*
 * OnUseGlobalSettingsChanged() (sec 10.134): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGAudioInput::OnUseGlobalSettingsChanged()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	unsigned char desiredBit = g[0x680] ? 1 : (unsigned char)(self[0x77] & 1);
	unsigned char currentBit = (unsigned char)((g[0x67f] >> 1) & 1);
	if (desiredBit == currentBit)
		return;

	if (desiredBit != 0) {
		self[0x77] = (unsigned char)(self[0x77] & ~0x2);
		((CSTGAudioInput *)(g + 0x608))->UseSettings();
	} else {
		g[0x67f] = (unsigned char)(g[0x67f] & ~0x2);
		UseSettings();
	}

	unsigned char *rt = (unsigned char *)CSTGControllerRTData::sInstance;
	if (rt[0x2b] == 7)
		CSTGControllerRTData::sInstance->ResetAllJumpCatch();
}

/*
 * CSTGProgramModeProgramSlot/CSTGProgramModeDrumTrackSlot (sec 10.81) --
 * see oa_global.h for the confirmed CSTGProgramSlot base-class
 * discovery. Both derived ctors and both Initialize() bodies are
 * confirmed real (.text+0xb9470/0xb94a0/0xb9780/0xb97b0 in OA_real.ko).
 */

/*
 * CSTGProgramSlot's own real vtable (confirmed 0xf0 bytes/60 slots via
 * `nm -CS` against OA_real.ko's `_ZTV26CSTGProgramModeProgramSlot`/
 * `_ZTV28CSTGProgramModeDrumTrackSlot`, matching the `_ZTV15CSTGProgramSlot`
 * size fix batch 45/sec 10.196 already applied) is genuinely DISPATCHED
 * THROUGH by Initialize() below (CallVtableSlot7) AND, as of this batch,
 * by ChangeProgram() (CallVtableSlot56, see further below) -- unlike this
 * project's other not-yet-reconstructed vtables, this one can't stay
 * zeroed at either slot (a null call would crash). Modeled with NATIVE
 * pointer width (not a byte-precise 32-bit-target layout) since both
 * CallVtableSlotN helpers use native `void**` indexing and these
 * placeholders are purely dispatch targets, never read at a fixed byte
 * offset by anything else -- this keeps host test builds (8-byte
 * pointers) and the real -m32 Kbuild target (4-byte pointers) both
 * correct without needing two different layouts.
 *
 * UPDATE (this batch): the real vtables for CSTGProgramModeProgramSlot and
 * CSTGProgramModeDrumTrackSlot are CONFIRMED DIFFERENT at slot 56 (byte
 * offset 0xe0 from the installed vtable pointer) -- readelf -r against
 * each class's own `_ZTV*` relocation table shows CSTGProgramModeProgramSlot
 * resolving to its own override, `ProcessPreviousSVDOnProgramChange`
 * (.text+0xb9760), while CSTGProgramModeDrumTrackSlot resolves to the BASE
 * CSTGProgramSlot::ProcessPreviousSVDOnProgramChange (.text+0xab030,
 * confirmed NOT overridden by the drum-track class). A single shared
 * `g_programSlotVtable` (as this project used through batch 46, when
 * nothing dispatched past slot 7) can no longer represent both classes
 * correctly -- split into two class-specific arrays below. Every OTHER
 * slot (including slot 7) keeps the exact same inert-trap treatment as
 * before; only slot 56 differs per class.
 */
static void ProgramSlotVtableTrap(void *) { /* deliberately inert */ }

/* CallVtableSlot56's own Fn shape (see further below); forward-declared
 * here so both per-class static arrays can reference the two real
 * ProcessPreviousSVDOnProgramChange implementations by address.
 * Deliberately NOT `static` (unlike ProgramSlotVtableTrap) and also
 * declared `extern` in oa_global.h -- test_global.cpp's own [54] KAT
 * needs to install these exact real function pointers into its OWN
 * mmap32'd, correctly-sized vtable buffers (the file-scope default
 * arrays' own address can't survive 32-bit truncation on a 64-bit host
 * once a prior scenario's own mmap32'd override has been munmap'd --
 * same "move this into a shared header once a sibling needs it" step
 * already used for atmel_deax.cpp's bzzzzzzzzzzzt11/12, sec 10.197). */
bool ProgramSlot_ProcessPreviousSVDOnProgramChange(void *self, CSTGSlotVoiceData *svd);
bool ProgramModeProgramSlot_ProcessPreviousSVDOnProgramChange(void *self, CSTGSlotVoiceData *svd);

/* Index 0/1 = offset-to-top/RTTI header (skipped by _vtablePtr below,
 * per the standard Itanium "vtable pointer already points past the
 * header" convention); indices 2..9 are virtual slots 0..7 (slot 7 is
 * the one CallVtableSlot7/Initialize() reaches); index 2+56=58 is
 * virtual slot 56 (the one CallVtableSlot56/ChangeProgram() reaches) --
 * needs 60 elements total (2 header + 58 real virtual slots, matching
 * the confirmed 0xf0-byte/60-slot real vtable exactly). */
#define PROGRAM_SLOT_VTABLE_TRAPS \
	0, 0, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap, (void *)ProgramSlotVtableTrap, \
	(void *)ProgramSlotVtableTrap /* index 57 = virtual slot 55 */

static void *g_programModeProgramSlotVtableStatic[60] = {
	PROGRAM_SLOT_VTABLE_TRAPS,
	(void *)ProgramModeProgramSlot_ProcessPreviousSVDOnProgramChange, /* index 58 = virtual slot 56 */
	(void *)ProgramSlotVtableTrap, /* index 59 = virtual slot 57 */
};
static void *g_programModeDrumTrackSlotVtableStatic[60] = {
	PROGRAM_SLOT_VTABLE_TRAPS,
	(void *)ProgramSlot_ProcessPreviousSVDOnProgramChange, /* index 58 = virtual slot 56 (base impl, not overridden) */
	(void *)ProgramSlotVtableTrap, /* index 59 = virtual slot 57 */
};
#undef PROGRAM_SLOT_VTABLE_TRAPS
void **g_programModeProgramSlotVtable = g_programModeProgramSlotVtableStatic;
void **g_programModeDrumTrackSlotVtable = g_programModeDrumTrackSlotVtableStatic;

/*
 * CSTGProgramModeProgramSlot::CSTGProgramModeProgramSlot()
 * (.text+0xb9780): base ctor first (real C++ inheritance handles this
 * automatically), sets the vtable pointer, then a single confirmed
 * write: +0x4 = 0 (a "slot kind" discriminator byte -- 0 for the
 * program-mode slot, 1 for the drum-track slot below, both confirmed
 * from their own ctors' identical-shaped final write).
 */
CSTGProgramModeProgramSlot::CSTGProgramModeProgramSlot()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)base = ToU32((unsigned char *)&g_programModeProgramSlotVtable[2]);
	base[0x4] = 0;
}

/*
 * CSTGProgramModeDrumTrackSlot::CSTGProgramModeDrumTrackSlot()
 * (.text+0xb9470): same shape as CSTGProgramModeProgramSlot's own ctor,
 * plus a confirmed extra zero-write at +0xe8 (the field
 * ChangeDrumTrackProgram later writes the "current drum program"
 * pointer into) and the discriminator byte set to 1 instead of 0.
 */
CSTGProgramModeDrumTrackSlot::CSTGProgramModeDrumTrackSlot()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)base = ToU32((unsigned char *)&g_programModeDrumTrackSlotVtable[2]);
	*(unsigned int *)(base + 0xe8) = 0;
	base[0x4] = 1;
}

/*
 * Both classes' Initialize(unsigned int) bodies are confirmed BYTE-FOR-
 * BYTE IDENTICAL (.text+0xb97b0 / 0xb94a0): a real virtual dispatch
 * through slot 7 first (CallVtableSlot7 -- CSTGProgramSlot's own real
 * vtable layout isn't independently reconstructed), then: zero a 4-byte
 * field at +0x6f (a real confirmed unaligned write, preserved as-is),
 * store the low byte of `arg` at +0x10, store the float constant 1.0
 * at +0x73, and finally a real read-modify-write at +0x43 (clear bit1,
 * set bits 3/4/5 -- 0x38).
 */
void CSTGProgramModeProgramSlot::Initialize(unsigned int arg)
{
	CallVtableSlot7(this);
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x6f) = 0;
	base[0x10] = (unsigned char)arg;
	*(float *)(base + 0x73) = 1.0f;
	unsigned char flags = base[0x43];
	flags = (unsigned char)((flags & ~0x2) | 0x38);
	base[0x43] = flags;
}

void CSTGProgramModeDrumTrackSlot::Initialize(unsigned int arg)
{
	CallVtableSlot7(this);
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x6f) = 0;
	base[0x10] = (unsigned char)arg;
	*(float *)(base + 0x73) = 1.0f;
	unsigned char flags = base[0x43];
	flags = (unsigned char)((flags & ~0x2) | 0x38);
	base[0x43] = flags;
}

/*
 * OnUpdateGlobalMidiChannel(unsigned int) (sec 10.125): see oa_global.h
 * for the full confirmed shape.
 */
void CSTGProgramModeProgramSlot::OnUpdateGlobalMidiChannel(unsigned int channel)
{
	unsigned char *self = (unsigned char *)this;
	self[0x10] = (unsigned char)channel;

	if (!IsActive())
		return;

	unsigned char *vd = (unsigned char *)AccessActiveSlotVoiceData();
	if (!vd)
		return;

	/*
	 * Confirmed real 32-bit packed-pointer fields throughout (this
	 * project's own established host/target pointer-width hazard --
	 * a native 8-byte store here would clobber the ADJACENT 4-byte
	 * field, e.g. writing `head` would corrupt `tail` 4 bytes later).
	 * All pointer-typed field reads/writes below go through
	 * ToU32/FromU32, matching every other packed-pointer field in this
	 * codebase.
	 */
	unsigned char *container = FromU32(*(unsigned int *)(vd + 0x20));
	unsigned char *node = vd + 0x14;

	if (container != 0) {
		if (FromU32(*(unsigned int *)(container + 0)) == node)
			*(unsigned int *)(container + 0) = *(unsigned int *)(vd + 0x14);

		if (FromU32(*(unsigned int *)(container + 4)) == node) {
			*(unsigned int *)(container + 4) = *(unsigned int *)(vd + 0x18);
		} else {
			unsigned char *next = FromU32(*(unsigned int *)(vd + 0x18));
			if (next)
				*(unsigned int *)(next + 0) = *(unsigned int *)(vd + 0x14);
			unsigned char *prev = FromU32(*(unsigned int *)(vd + 0x14));
			if (prev)
				*(unsigned int *)(prev + 4) = *(unsigned int *)(vd + 0x18);
		}

		*(unsigned int *)(vd + 0x14) = 0;
		*(unsigned int *)(vd + 0x18) = 0;
		*(unsigned int *)(vd + 0x20) = 0;
		(*(unsigned int *)(container + 8))--;
	}

	unsigned char newChannel = self[0x10];
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *bucket = g + 0x29c99cc + (unsigned int)newChannel * 0xc;

	unsigned char *oldHead = FromU32(*(unsigned int *)bucket);
	if (oldHead == 0) {
		*(unsigned int *)(bucket + 4) = ToU32(node); /* bucket->tail = node */
	} else {
		unsigned char *tmp = FromU32(*(unsigned int *)(oldHead + 4));
		*(unsigned int *)(vd + 0x18) = ToU32(tmp);
		if (tmp)
			*(unsigned int *)(tmp + 0) = ToU32(node);
		*(unsigned int *)(oldHead + 4) = ToU32(node);
		*(unsigned int *)(vd + 0x14) = ToU32(oldHead);
	}
	*(unsigned int *)bucket = ToU32(node);        /* bucket->head = node */
	*(unsigned int *)(vd + 0x20) = ToU32(bucket);  /* vd->container = bucket */
	(*(unsigned int *)(bucket + 8))++;            /* bucket->count++ */
}

/*
 * OnUpdateProgramDrumTrackMidiChannel(unsigned int) (sec 10.133): see
 * oa_global.h for the full confirmed shape -- byte-for-byte identical
 * logic to CSTGProgramModeProgramSlot::OnUpdateGlobalMidiChannel above,
 * confirmed via relocation to call the same base-class CSTGProgramSlot
 * methods and index the same shared bucket table.
 */
void CSTGProgramModeDrumTrackSlot::OnUpdateProgramDrumTrackMidiChannel(unsigned int channel)
{
	unsigned char *self = (unsigned char *)this;
	self[0x10] = (unsigned char)channel;

	if (!IsActive())
		return;

	unsigned char *vd = (unsigned char *)AccessActiveSlotVoiceData();
	if (!vd)
		return;

	unsigned char *container = FromU32(*(unsigned int *)(vd + 0x20));
	unsigned char *node = vd + 0x14;

	if (container != 0) {
		if (FromU32(*(unsigned int *)(container + 0)) == node)
			*(unsigned int *)(container + 0) = *(unsigned int *)(vd + 0x14);

		if (FromU32(*(unsigned int *)(container + 4)) == node) {
			*(unsigned int *)(container + 4) = *(unsigned int *)(vd + 0x18);
		} else {
			unsigned char *next = FromU32(*(unsigned int *)(vd + 0x18));
			if (next)
				*(unsigned int *)(next + 0) = *(unsigned int *)(vd + 0x14);
			unsigned char *prev = FromU32(*(unsigned int *)(vd + 0x14));
			if (prev)
				*(unsigned int *)(prev + 4) = *(unsigned int *)(vd + 0x18);
		}

		*(unsigned int *)(vd + 0x14) = 0;
		*(unsigned int *)(vd + 0x18) = 0;
		*(unsigned int *)(vd + 0x20) = 0;
		(*(unsigned int *)(container + 8))--;
	}

	unsigned char newChannel = self[0x10];
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *bucket = g + 0x29c99cc + (unsigned int)newChannel * 0xc;

	unsigned char *oldHead = FromU32(*(unsigned int *)bucket);
	if (oldHead == 0) {
		*(unsigned int *)(bucket + 4) = ToU32(node);
	} else {
		unsigned char *tmp = FromU32(*(unsigned int *)(oldHead + 4));
		*(unsigned int *)(vd + 0x18) = ToU32(tmp);
		if (tmp)
			*(unsigned int *)(tmp + 0) = ToU32(node);
		*(unsigned int *)(oldHead + 4) = ToU32(node);
		*(unsigned int *)(vd + 0x14) = ToU32(oldHead);
	}
	*(unsigned int *)bucket = ToU32(node);
	*(unsigned int *)(vd + 0x20) = ToU32(bucket);
	(*(unsigned int *)(bucket + 8))++;
}

/*
 * CSTGProgramModeDrumTrackSlot::ChangeDrumTrackProgram(CSTGProgram*,
 * CSTGProgram*) (.text+0xb94f0): confirmed real, small -- stores arg1
 * at +0xe8 then tail-calls the base class's own confirmed real
 * ChangeProgram(arg2).
 */
void CSTGProgramModeDrumTrackSlot::ChangeDrumTrackProgram(CSTGProgram *newProgram, CSTGProgram *arg2)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xe8) = ToU32((unsigned char *)newProgram);
	ChangeProgram(arg2);
}

/*
 * CSTGMidiQueue::AllocReader() (sec 10.82, .text+0x40090 in OA_real.ko)
 * -- RESOLVES sec 10.63's own "static-shaped ambiguity" note: confirmed
 * a genuine instance method (regparm this=eax), body is a single
 * lock-free atomic `lock xadd $1, [this+0x20]`, returning the PRE-
 * increment byte value (a real reader-slot allocator, wraps at 256).
 */
unsigned char CSTGMidiQueue::AllocReader()
{
	unsigned char *slot = (unsigned char *)this + 0x20;
	return __sync_fetch_and_add(slot, 1);
}

/*
 * CSTGHeldKeyList::Reset() (sec 10.82, .text+0xa24d0 in OA_real.ko) --
 * confirmed real: walks and fully unlinks every node of an intrusive
 * doubly-linked list (head at +0xa00, a "tail marker" at +0xa04, a
 * count at +0xa08; each node's own +0x0/+0x4 are its own next/prev,
 * +0xc a confirmed-real field zeroed on removal but not independently
 * named), decrementing the count once per node, clearing the tail
 * marker whenever it currently equals the node being processed as
 * head (checked both before the loop starts and at the end of each
 * iteration -- reproduced as a single check inside the loop, matching
 * the real control flow's own de-facto behavior exactly since the
 * pre-loop check and the in-loop check are the identical comparison).
 */
void CSTGHeldKeyList::Reset()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int head = *(unsigned int *)(base + 0xa00);
	if (head == 0)
		return;
	for (;;) {
		unsigned char *node = FromU32(head);
		unsigned int next = *(unsigned int *)node;
		*(unsigned int *)(base + 0xa00) = next;
		unsigned int prev = *(unsigned int *)(node + 0x4);
		if (prev != 0)
			*(unsigned int *)FromU32(prev) = next;
		if (next != 0)
			*(unsigned int *)(FromU32(next) + 0x4) = prev;
		*(unsigned int *)node = 0;
		*(unsigned int *)(node + 0x4) = 0;
		*(unsigned int *)(node + 0xc) = 0;

		head = *(unsigned int *)(base + 0xa00);
		(*(unsigned int *)(base + 0xa08))--;
		if (head == 0)
			break;
		if (head == *(unsigned int *)(base + 0xa04))
			*(unsigned int *)(base + 0xa04) = 0;
	}
}

/*
 * Batch (sec 10.90): 13 more CSTGGlobal methods. See oa_global.h for
 * each method's own full confirmed-shape comment.
 */

CSTGGlobal::~CSTGGlobal()
{
	/* D1Ev (complete-object destructor) -- confirmed real, deliberately
	 * deferred (see oa_global.h). Note: a single C++ `~CSTGGlobal()`
	 * definition necessarily generates BOTH D1Ev and D0Ev via the
	 * standard Itanium ABI, so our compiler-generated D0Ev WILL call
	 * `operator delete` after this body runs -- a known, deliberate
	 * divergence from the real D0Ev's own confirmed omission of that
	 * call (see oa_global.h). Harmless: this destructor is never
	 * exercised anywhere in the confirmed init_module boot path this
	 * project targets (CSTGGlobal is a singleton, never `delete`d). */
}

void CSTGGlobal::SetCurrentEditInContextTimbreSolo(unsigned int index, bool solo)
{
	unsigned char *base = (unsigned char *)this;
	unsigned short *flags = (unsigned short *)(base + 0x29cc4e4);
	unsigned short bit = (unsigned short)(1u << index);
	if (solo)
		*flags = (unsigned short)(*flags | bit);
	else
		*flags = (unsigned short)(*flags & ~bit);
}

void CSTGGlobal::CompleteDeferredExtModeChange()
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x29cc0c9] & 1) {
		CSTGControllerRTData::sInstance->OnExtModeSetChange();
		base[0x29cc0c9] &= ~1;
	}
}

bool CSTGGlobal::ShouldDeferExtModeChange() const
{
	const unsigned char *base = (const unsigned char *)this;
	if (base[0x6ae] == 0)
		return true;
	if (base[0x29cc0c9] & 1)
		return true;
	CSTGPerformanceVarsManager *mgr =
		const_cast<CSTGGlobal *>(this)->ResolveActivePerformanceVarsManager();
	return ((unsigned char *)mgr)[0x23d1] != 2;
}

bool CSTGGlobal::CheckDeferExtModeChange()
{
	unsigned char *base = (unsigned char *)this;

	if (base[0x6ae] != 0 && (base[0x29cc0c9] & 1) == 0) {
		CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
		if (((unsigned char *)mgr)[0x23d1] == 2)
			return true;	/* deferred; bit deliberately left clear */
	}

	base[0x29cc0c9] |= 1;
	return false;
}

void CSTGGlobal::RemoveExtCCFunctionAssignment(unsigned int tag)
{
	unsigned char *base = (unsigned char *)this;
	for (unsigned int i = 0; i < 0x78; i++) {
		unsigned char *slot = base + 0x29cc11c + i * 8;
		if (slot[2] == 0 && *(unsigned int *)(slot + 4) == tag)
			slot[0] = 0;
	}
}

void CSTGGlobal::SendFXDisableCCToMidiOut(unsigned char ccNumber, bool enabled)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char msg[3];
	msg[0] = (unsigned char)(base[0x6b8] | 0xb0);
	msg[1] = ccNumber;
	msg[2] = enabled ? 0x00 : 0x7f;
	CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 3);
}

void CSTGGlobal::BeginPerformanceChange(int mode, unsigned int value1, unsigned int value2,
					  unsigned int source)
{
	CSTGPerfChangeRequest request;
	request.tag = 0;
	request.mode = (unsigned int)mode;
	request.value1 = value1;
	request.value2 = value2;
	request.source = source;
	request.field14 = 0;
	request.field18 = 0;
	SubmitPerfChangeRequest(request);
}

void CSTGGlobal::BeginSetListSlotChange(unsigned int value1, unsigned int value2,
					  unsigned int source)
{
	CSTGPerfChangeRequest request;
	request.tag = 1;
	request.mode = 0;
	request.value1 = value1;
	request.value2 = value2;
	request.source = source;
	request.field14 = 0;
	request.field18 = 0;
	SubmitPerfChangeRequest(request);
}

bool CSTGGlobal::GetIsSetListActiveAndSeqPerfType()
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6a4] == 0)
		return false;
	unsigned int idx = base[0x6a5];
	if (idx >= 0x80)
		idx = 0;
	unsigned int idx2 = base[0x6a6];
	unsigned int offset = idx * 0x834 + (idx2 << 4);
	return base[0x2933750 + offset] == 2;
}

void CSTGGlobal::SetUseGlobalAudioInputSettings(bool useGlobal)
{
	unsigned char *base = (unsigned char *)this;
	base[0x680] = (unsigned char)useGlobal;

	if (base[0x6ae] == 0)
		return;

	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	unsigned char *mgrBytes = (unsigned char *)mgr;
	if (mgrBytes[0x23d1] != 2)
		return;

	unsigned int basePtr = *(unsigned int *)(mgrBytes + 0x23d4);
	CSTGAudioInput *audioInput = (CSTGAudioInput *)FromU32(basePtr + 0xae7);
	audioInput->OnUseGlobalSettingsChanged();
}

bool CSTGGlobal::ShouldAllowMidiPerformanceChange() const
{
	const unsigned char *base = (const unsigned char *)this;
	CSTGMidiDispatcher *dispatcher = CSTGMidiDispatcher::sInstance;

	if (((unsigned char *)dispatcher)[0xa2] == 0)
		return false;

	const unsigned char *slot = (base[0x2975185] == 0)
					     ? (base + 0x297514c)
					     : (base + 0x2975168);

	if (slot[0] != 0)
		return true;

	unsigned int field4 = *(unsigned int *)(slot + 4);
	if (field4 != 0)
		return true;

	unsigned int field0xc = *(unsigned int *)(slot + 0xc);
	return field0xc != 0xfffe;
}

void CSTGGlobal::SendUnsolGlobalMessageToUI(int p1, int p2, int p3, unsigned int source)
{
	unsigned char buf[0x18];
	*(unsigned short *)(buf + 0x0) = 0x18;
	*(unsigned short *)(buf + 0x2) = (unsigned short)source;
	*(unsigned int *)(buf + 0x4) = 1;
	*(unsigned int *)(buf + 0x8) = 0;
	*(unsigned int *)(buf + 0xc) = (unsigned int)p2;
	*(unsigned int *)(buf + 0x10) = (unsigned int)p1;
	*(unsigned int *)(buf + 0x14) = (unsigned int)p3;
	PushUnsolicitedMessage(buf);
}

void CSTGGlobal::SetNKS4TestModeFlag(bool testMode)
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x6ac] == (unsigned char)testMode)
		return;

	COmapNKS4Driver_SetTestMode(testMode);

	if (testMode) {
		base[0x6ac] = 1;
		CSTGMidiPortManager::sInstance->NotifyNKS4TestMode();
	} else {
		base[0x6ac] = 0;
	}
}

/*
 * Batch (sec 10.92): 10 more CSTGGlobal methods. See oa_global.h for
 * each method's own full confirmed-shape comment.
 */

void CSTGGlobal::ValidateParamChange(CSTGMessageContext &ctx, unsigned long paramId, const CValue &value)
{
	if (paramId == 0x26) {
		if (ctx.index > 0xbf) {
			ctx.responseCode = 2;
			return;
		}
		ctx.clampFlag = 0;
	} else if (paramId == 0x27) {
		if (ctx.index > 0x7f) {
			ctx.responseCode = 2;
			return;
		}
		ctx.clampFlag = 0;
	}

	reinterpret_cast<CSTGParamsOwner *>(this)->ValidateParamChange(ctx, paramId, value);
}

void CSTGGlobal::RepeatLastPerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x2975184] != 0)
		return;

	unsigned char *slot = (base[0x2975185] == 0) ? (base + 0x297514c) : (base + 0x2975168);
	CSTGPerfChangeRequest request = *(CSTGPerfChangeRequest *)slot;
	request.field14 = 3;
	SubmitPerfChangeRequest(request);
}

void CSTGGlobal::ResetAllControllers()
{
	CSTGControllerValue value;
	/* Confirmed real quirk: reads whatever was already on the stack in
	 * this field before it's ever explicitly written -- see this
	 * struct's own declaration in oa_global.h. */
	value.fieldB = (unsigned char)(value.fieldB | 3);
	value.field0 = 0;
	value.field4 = -1.0f;
	value.field8 = 0;
	value.fieldA = 1;

	HandleController(0x5c, value);
	HandleController(0x5e, value);
	HandleController(0x5f, value);
}

bool CSTGGlobal::DoesPerfChangeRequestMatchType(const CSTGPerfChangeRequest &req, unsigned int type) const
{
	const unsigned char *base = (const unsigned char *)this;

	if (req.tag == 0) {
		unsigned int idx = req.value1;
		if (idx >= 0x80)
			idx = 0;
		unsigned int offset = idx * 0x834 + (req.value2 << 4);
		signed char tableVal = (signed char)base[0x2933750 + offset];
		return (int)type == (int)tableVal;
	}

	unsigned int m = req.mode - 1;
	if (m <= 1)
		return type == *(unsigned int *)(base + 0x64 + m * 4);
	return type == 1;
}

bool CSTGGlobal::GetPerformanceIdFromPerfChangeRequest(const CSTGPerfChangeRequest &req, CPerformanceId *out) const
{
	const unsigned char *base = (const unsigned char *)this;

	if (req.tag == 0) {
		unsigned int idx = req.value1;
		if (idx >= 0x80)
			idx = 0;
		unsigned int offset = idx * 0x834 + (req.value2 << 4);
		const unsigned char *record = base + 0x2933740 + offset;
		unsigned short packed = *(const unsigned short *)(record + 0x10);
		out->byte0 = (unsigned char)(packed & 0xff);
		out->byte1 = (unsigned char)(packed >> 8);
		out->byte2 = record[0x12];
		return true;
	}

	if (req.value2 == 0xfffe)
		return false;

	unsigned int m = req.mode - 1;
	unsigned char byte0 = (m <= 1) ? base[0x64 + m * 4] : 1;
	out->byte0 = byte0;
	out->byte1 = (unsigned char)req.value1;
	out->byte2 = (unsigned char)req.value2;
	return true;
}

void CSTGGlobal::NotifyKarmaPerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char valA, valB;
	int mode = *(int *)(base + 0x684);

	if (mode == 1) {
		valA = (unsigned char)(*(unsigned int *)(base + 0x690) & 0x3f);
		valB = base[0x69c];
	} else if (mode == 2) {
		valA = 0x80;
		valB = base[0x6a0];
	} else {
		valA = (unsigned char)((*(unsigned int *)(base + 0x68c) & 0x3f) | 0x40);
		valB = base[0x698];
	}

	unsigned char msg[5];
	msg[0] = (unsigned char)(base[0x6b8] | 0xc0);
	msg[1] = valA;
	msg[2] = valB;
	msg[3] = 0x15;
	msg[4] = 0xfe;

	unsigned char *queueWriter = (unsigned char *)CSTGMidiPortManager::sInstance + 0x208;
	((CSTGMidiQueueWriter *)queueWriter)->Write(msg, 5, false);
}

void CSTGGlobal::SendProgramChangeToMidiOut(unsigned char p1, unsigned char p2, unsigned char p3)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char msg[6];
	unsigned char channel = base[0x6b8];

	if (base[0x6d6] != 0) {
		msg[0] = (unsigned char)(channel | 0xb0);
		msg[1] = 0x00;
		msg[2] = p1;
		msg[3] = (unsigned char)(channel | 0xb0);
		msg[4] = 0x20;
		msg[5] = p2;
		CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 6);
	}

	msg[0] = (unsigned char)(channel | 0xc0);
	msg[1] = p3;
	CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 2);
}

void CSTGGlobal::EmergencyFreeDyingSlotVoiceData()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int listHead = *(unsigned int *)(base + 0x29c9904);

	for (unsigned int group = 0; group < 2; group++) {
		unsigned int node = listHead;
		while (node != 0) {
			unsigned char *nodePtr = FromU32(node);
			unsigned char *voiceData = FromU32(*(unsigned int *)(nodePtr + 8));
			unsigned int next = *(unsigned int *)(nodePtr + 4);

			unsigned char *sub = FromU32(*(unsigned int *)(voiceData + 0x34));
			unsigned int groupBit = (sub[0x43] >> 1) & 1;

			if (groupBit == group && voiceData[0x40] != 0) {
				unsigned short sum = (unsigned short)(*(unsigned short *)(voiceData + 0x4c) +
								       *(unsigned short *)(voiceData + 0x58));
				bool doFree = !(sum != 0 && voiceData[0x41] != 0);
				((CSTGSlotVoiceData *)voiceData)->EmergencyFreeAllVoices();
				if (doFree)
					((CSTGSlotVoiceData *)voiceData)->FreeSlotVoiceData(true);
				return;
			}
			node = next;
		}
	}
}

void CSTGGlobal::SetTune(float tune)
{
	unsigned char *base = (unsigned char *)this;
	*(float *)(base + 0x6bc) = tune;
	if (base[0x6ae] == 0)
		return;

	unsigned char *globalBase = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char activeSlot = globalBase[0x6b8];
	for (unsigned int i = 0; i < 16; i++) {
		unsigned int listHead = *(unsigned int *)(globalBase + 0x29c99cc + i * 12);
		UpdateGlobalTuneOnList(listHead, tune);
		if (i == activeSlot) {
			unsigned int specialHead = *(unsigned int *)(globalBase + 0x29c9a8c);
			UpdateGlobalTuneOnList(specialHead, tune);
		}
	}
}

void CSTGGlobal::SetMode(int mode, unsigned int source)
{
	unsigned char *base = (unsigned char *)this;
	CSTGPerfChangeRequest request;
	request.tag = 0;
	request.mode = (unsigned int)mode;
	request.source = source;
	request.field14 = 0;
	request.field18 = 0;

	if (mode == 1) {
		request.value1 = *(unsigned int *)(base + 0x690);
		request.value2 = *(unsigned int *)(base + 0x69c);
	} else if (mode == 2) {
		request.value1 = 0;
		request.value2 = *(unsigned int *)(base + 0x6a0);
	} else {
		request.value1 = *(unsigned int *)(base + 0x688);
		request.value2 = *(unsigned int *)(base + 0x694);
	}

	SubmitPerfChangeRequest(request);
}

/*
 * RunVoiceModelStaticFront()/RunVoiceModelStaticBack() (sec 10.93): see
 * oa_global.h for the full confirmed shape, including how they relate
 * to RunVoiceModelFeedback (sec 10.55).
 */
void CSTGGlobal::RunVoiceModelStaticFront(unsigned int param)
{
	typedef void *(*VtableSlot1aFn)(void *);
	unsigned char *globalBase = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char **node = *(unsigned char ***)(globalBase + 0x29c9900);

	while (node) {
		unsigned char *payload = *(unsigned char **)((unsigned char *)node + 0x8);
		unsigned char *next = *(unsigned char **)node;

		if (payload[0x28dc] == (unsigned char)param || payload[0x28dd] == (unsigned char)param) {
			unsigned char *sub = *(unsigned char **)(payload + 0x38);
			unsigned char flags = sub[0xb73];
			bool run = false;

			if (flags & 1) {
				unsigned char *obj = *(unsigned char **)(sub + 0xb6b);
				void **vtable = *(void ***)obj;
				VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
				unsigned char *result = (unsigned char *)fn(obj);
				if (result[0xe1] & 0x40)
					run = true;
				else
					flags = sub[0xb73]; /* confirmed real reload */
			}
			if (!run && (flags & 2)) {
				unsigned char *obj = *(unsigned char **)(sub + 0xb6f);
				void **vtable = *(void ***)obj;
				VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
				unsigned char *result = (unsigned char *)fn(obj);
				if (result[0xe1] & 0x40)
					run = true;
			}

			if (run)
				((CSTGSlotVoiceData *)payload)->RunVoiceModelStaticFront(param);
		}

		node = (unsigned char **)next;
	}
}

void CSTGGlobal::RunVoiceModelStaticBack(unsigned int param)
{
	typedef void *(*VtableSlot1aFn)(void *);
	unsigned char *globalBase = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char **node = *(unsigned char ***)(globalBase + 0x29c9900);

	while (node) {
		unsigned char *payload = *(unsigned char **)((unsigned char *)node + 0x8);
		unsigned char *next = *(unsigned char **)node;

		if (payload[0x28de] == (unsigned char)param || payload[0x28df] == (unsigned char)param) {
			unsigned char *sub = *(unsigned char **)(payload + 0x38);
			unsigned char flags = sub[0xb73];
			bool run = false;

			if (flags & 1) {
				unsigned char *obj = *(unsigned char **)(sub + 0xb6b);
				void **vtable = *(void ***)obj;
				VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
				unsigned char *result = (unsigned char *)fn(obj);
				if ((signed char)result[0xe1] < 0)
					run = true;
				else
					flags = sub[0xb73]; /* confirmed real reload */
			}
			if (!run && (flags & 2)) {
				unsigned char *obj = *(unsigned char **)(sub + 0xb6f);
				void **vtable = *(void ***)obj;
				VtableSlot1aFn fn = (VtableSlot1aFn)vtable[0x1a];
				unsigned char *result = (unsigned char *)fn(obj);
				if ((signed char)result[0xe1] < 0)
					run = true;
			}

			if (run)
				((CSTGSlotVoiceData *)payload)->RunVoiceModelStaticBack(param);
		}

		node = (unsigned char **)next;
	}
}

/*
 * Batch (sec 10.94): 3 more CSTGGlobal methods. See oa_global.h for
 * each method's own full confirmed-shape comment.
 */

void CSTGGlobal::HandleMidiPerformanceChange(unsigned char param)
{
	unsigned char *base = (unsigned char *)this;
	CSTGMidiDispatcher *dispatcher = CSTGMidiDispatcher::sInstance;
	if (((unsigned char *)dispatcher)[0xa2] == 0)
		return;

	unsigned char *slot = (base[0x2975185] == 0) ? (base + 0x297514c) : (base + 0x2975168);

	CSTGPerfChangeRequest request;
	request.field14 = 0;
	request.field18 = 0;
	request.value2 = param;
	request.source = 2;

	if (slot[0] != 0) {
		request.tag = 1;
		request.mode = 0;
		request.value1 = *(unsigned int *)(slot + 8);
	} else {
		if (*(unsigned int *)(slot + 4) == 0 && *(unsigned int *)(slot + 0xc) == 0xfffe)
			return;

		unsigned int mode = *(unsigned int *)(slot + 4);
		if (mode > 1)
			return;

		request.tag = 0;
		request.mode = mode;
		request.value1 = *(unsigned int *)(slot + 8);
	}

	SubmitPerfChangeRequest(request);
}

unsigned long CSTGGlobal::StealDyingSlotVoiceDatasForCost(unsigned long targetCost)
{
	unsigned char *base = (unsigned char *)this;
	unsigned long accumCost = 0;
	if (targetCost == 0)
		return 0;

	for (unsigned int group = 0; group <= 1; group++) {
		unsigned int node = *(unsigned int *)(base + 0x29c9904);
		while (accumCost < targetCost && node != 0) {
			unsigned char *nodePtr = FromU32(node);
			unsigned char *payload = FromU32(*(unsigned int *)(nodePtr + 8));
			node = *(unsigned int *)(nodePtr + 4);

			unsigned char *sub = FromU32(*(unsigned int *)(payload + 0x34));
			unsigned int groupBit = (sub[0x43] >> 1) & 1;
			if (groupBit != group)
				continue;
			if (payload[0x40] == 0)
				continue;
			if (*(unsigned int *)(payload + 0x28c4) == 1)
				continue;
			unsigned char mode = sub[0xd];
			if (mode != 1 && mode != 2)
				continue;
			if (payload[0x42] != 0)
				continue;

			unsigned long cost1 = 0, cost2 = 0;
			((CSTGSlotVoiceData *)payload)->GetTotalStaticCosts(&cost1, &cost2);
			unsigned long sum = cost1 + cost2;
			if (sum == 0)
				continue;

			accumCost += sum;
			((CSTGSlotVoiceData *)payload)->Steal();
		}
	}

	return accumCost;
}

void CSTGGlobal::FreeSlotVoiceData(CSTGSlotVoiceData *node)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *nodeBytes = (unsigned char *)node;
	unsigned int selfLink = ToU32(nodeBytes + 0x24);

	if (selfLink == *(unsigned int *)(base + 0x29c9900)) {
		/* node is the current head of the "active" list (+0x29c9900). */
		unsigned int next = *(unsigned int *)(nodeBytes + 0x24);
		bool alsoDyingHead = (selfLink == *(unsigned int *)(base + 0x29c9904));
		*(unsigned int *)(base + 0x29c9900) = next;
		if (alsoDyingHead) {
			/* Confirmed real quirk: a node can apparently be the
			 * head of BOTH lists at once -- also update the
			 * dying-list head exactly like the standalone case
			 * below. */
			unsigned int prev = *(unsigned int *)(nodeBytes + 0x28);
			*(unsigned int *)(base + 0x29c9904) = prev;
		}
	} else if (selfLink == *(unsigned int *)(base + 0x29c9904)) {
		unsigned int prev = *(unsigned int *)(nodeBytes + 0x28);
		*(unsigned int *)(base + 0x29c9904) = prev;
	}

	unsigned int prev = *(unsigned int *)(nodeBytes + 0x28);
	unsigned int next = *(unsigned int *)(nodeBytes + 0x24);
	if (prev != 0)
		*(unsigned int *)FromU32(prev) = next;
	if (next != 0)
		*(unsigned int *)(FromU32(next) + 4) = prev;

	*(unsigned int *)(nodeBytes + 0x24) = 0;
	*(unsigned int *)(nodeBytes + 0x28) = 0;
	*(unsigned int *)(nodeBytes + 0x30) = 0;

	unsigned int freeHead = *(unsigned int *)(base + 0x29c98f4);
	(*(int *)(base + 0x29c9908))--;
	unsigned int newLink = ToU32(nodeBytes + 4);
	unsigned int ownerAddr = ToU32(base + 0x29c98f4);

	if (freeHead == 0) {
		*(unsigned int *)(base + 0x29c98f8) = newLink;
	} else {
		unsigned int backLink = *(unsigned int *)(FromU32(freeHead) + 4);
		*(unsigned int *)(nodeBytes + 8) = backLink;
		if (backLink != 0)
			*(unsigned int *)FromU32(backLink) = newLink;
		*(unsigned int *)(FromU32(freeHead) + 4) = newLink;
	}

	*(unsigned int *)(nodeBytes + 4) = freeHead;
	*(unsigned int *)(base + 0x29c98f4) = newLink;
	*(unsigned int *)(nodeBytes + 0x10) = ownerAddr;
	(*(unsigned int *)(base + 0x29c98fc))++;
}

/*
 * PreprocessPerformanceChange() (sec 10.95): see oa_global.h for the
 * full confirmed shape.
 */
extern "C" void *sXCmd;
extern "C" unsigned int kAudXBZD;
extern "C" float allPlusOne[4];
extern "C" float allMinusOne[4];

/*
 * Shared by PreprocessPerformanceChange() and ProcessPerfChangeRequest()'s
 * own "not a SetListSlotChange" branch -- confirmed via disassembly to
 * be genuinely byte-identical logic at two separate .text addresses in
 * the real binary (not merely similar-shaped). Factored into one
 * helper here rather than duplicated, since there's no behavioral
 * difference to preserve by keeping them textually separate (unlike
 * e.g. UpdateFXDisable vs. SendFXDisableCCToMidiOut, sec 10.90, which
 * ARE kept separate because their real callers are genuinely distinct
 * use sites with no confirmed relationship).
 */
static void RunPerformanceDeactivateSequence(unsigned char *base)
{
	unsigned char *msgProc = (unsigned char *)CSTGMessageProcessor::sInstance;
	if (msgProc[0x48] != 0)
		return;

	bool skipHeadroomReset = (sXCmd != 0 &&
				   *(unsigned int *)((unsigned char *)sXCmd + 5) == 0x22fb39cc);

	if (!skipHeadroomReset) {
		for (int i = 0; i < 4; i++) {
			allPlusOne[i] = 0.7f;
			allMinusOne[i] = -0.2f;
		}
		kAudXBZD = 0x1f;
	}

	base[0x2975184] = 1;
	CSTGSmoother::sInstance->FinalizeAllSmoothers();

	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGPerformanceVars *)mgr)->SetIsDying();

	((unsigned char *)CSTGMidiDispatcher::sInstance)[0] = 1;
	msgProc[0x54] = 1;

	unsigned char msg[5];
	msg[0] = (unsigned char)(base[0x6b8] | 0xb0);
	msg[1] = 0x79;
	msg[2] = 0x03;
	msg[3] = 0x05;
	msg[4] = 0xfe;

	unsigned char *queueWriter = (unsigned char *)CSTGMidiPortManager::sInstance + 0x208;
	((CSTGMidiQueueWriter *)queueWriter)->Write(msg, 5, false);
}

void CSTGGlobal::PreprocessPerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	RunPerformanceDeactivateSequence(base);
}

/*
 * IsSetListSlotChangeOnly() (sec 10.96): see oa_global.h for the full
 * confirmed shape, including its cross-confirmation of SetMode's own
 * mode encoding (sec 10.92).
 */
bool CSTGGlobal::IsSetListSlotChangeOnly(const CSTGPerfChangeRequest &req)
{
	unsigned char *base = (unsigned char *)this;
	CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
	if (((unsigned char *)mgr)[0x23d1] != 2)
		return false;
	if (base[0x6a4] == 0)
		return false;
	if (req.tag == 0)
		return false;

	unsigned int idx = req.value1;
	if (idx != base[0x6a5])
		return false;
	unsigned int clampedIdx = (idx >= 0x80) ? 0 : idx;
	unsigned int idx2 = req.value2;

	int mode = *(int *)(base + 0x684);
	unsigned int offset = clampedIdx * 0x834 + (idx2 << 4);
	unsigned char *record = base + 0x2933740 + offset;

	if (mode == 1) {
		if (record[0x10] != 0)
			return false;
		if (record[0x11] != *(unsigned int *)(base + 0x690))
			return false;
		return record[0x12] == *(unsigned int *)(base + 0x69c);
	} else if (mode == 2) {
		if (record[0x10] != 2)
			return false;
		return record[0x12] == *(unsigned int *)(base + 0x6a0);
	} else {
		if (record[0x10] != 1)
			return false;
		if (record[0x11] != *(unsigned int *)(base + 0x688))
			return false;
		return record[0x12] == *(unsigned int *)(base + 0x694);
	}
}

/*
 * ProcessSetListSlotOnlyChange() (sec 10.97): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGGlobal::ProcessSetListSlotOnlyChange()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int idx = *(unsigned int *)(base + 0x2975154);
	if (idx >= 0x80)
		idx = 0;
	unsigned int idx2 = *(unsigned int *)(base + 0x2975158);
	unsigned int offset = idx * 0x834 + (idx2 << 4);
	base[0x6a6] = (unsigned char)idx2;

	unsigned char *record = base + 0x2933750 + offset;

	int mode = *(int *)(base + 0x684);
	if (mode == 2) {
		unsigned int seqIndex = *(unsigned int *)(base + 0x6a0);
		unsigned char extraByte = record[3];
		unsigned char *seqObj = base + seqIndex * 0x1cad + 0x27cd024;
		typedef void (*VtableSlot1eFn)(void *, unsigned int);
		void **vtable = *(void ***)seqObj;
		VtableSlot1eFn fn = (VtableSlot1eFn)vtable[0x1e];
		fn(seqObj, extraByte);
	}

	((CSetListSlot *)record)->Activate();

	unsigned int field15c = *(unsigned int *)(base + 0x297515c);
	if (field15c == 1 || field15c == 2) {
		unsigned char msg[0x18];
		*(unsigned short *)(msg + 0x0) = 0x18;
		*(unsigned short *)(msg + 0x2) = 1;
		*(unsigned int *)(msg + 0x4) = 0;
		*(unsigned int *)(msg + 0x8) = 0x1f;
		*(unsigned int *)(msg + 0xc) = base[0x6a5];
		*(unsigned int *)(msg + 0x10) = idx2;
		*(unsigned int *)(msg + 0x14) = 3;
		PushUnsolicitedMessage(msg);
		/* Confirmed real: the disassembly re-reads +0x297515c here
		 * before deciding on SendPerfChangeToMidiOut below -- a no-op
		 * given nothing in this function mutates that field, but
		 * preserved as the literal condition structure rather than
		 * silently merged with the check below. */
	}
	if (field15c == 0 || field15c == 1) {
		CSTGPerfChangeRequest *request = (CSTGPerfChangeRequest *)(base + 0x297514c);
		SendPerfChangeToMidiOut(*request);
	}
}

/*
 * SendPerfChangeToMidiOut() (sec 10.98): see oa_global.h for the full
 * confirmed shape. `SendBankSelectAndProgramChange` mirrors the real
 * binary's own shared code (modes 0/1 and the tag!=0 path all converge
 * on the same Bank-Select+Program-Change send, just with different
 * "value byte" sources feeding it).
 */
static void SendBankSelectAndProgramChange(unsigned char *base, unsigned int value2,
					     unsigned char bankMsbValue, unsigned char bankLsbValue)
{
	unsigned char msg[6];
	if (base[0x6d6] != 0) {
		msg[0] = (unsigned char)(base[0x6b8] | 0xb0);
		msg[1] = 0;
		msg[2] = bankMsbValue;
		msg[3] = (unsigned char)(base[0x6b8] | 0xb0);
		msg[4] = 0x20;
		msg[5] = bankLsbValue;
		CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 6);
	}
	msg[0] = (unsigned char)(base[0x6b8] | 0xc0);
	msg[1] = (unsigned char)value2;
	CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 2);
}

void CSTGGlobal::SendPerfChangeToMidiOut(const CSTGPerfChangeRequest &req)
{
	unsigned char *base = (unsigned char *)this;

	if (req.tag != 0) {
		if (base[0x6d5] == 0)
			return;
		/* Confirmed real quirk: the Bank Select MSB's own value byte
		 * is always a literal 0 here, never derived from req.value1
		 * -- only the LSB carries it. */
		SendBankSelectAndProgramChange(base, req.value2, 0, (unsigned char)req.value1);
		return;
	}

	int mode = (int)req.mode;
	if (mode == 0) {
		if (base[0x6d5] == 0)
			return;
		char out1 = 0, out2 = 0;
		USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank((int)req.value1, out1, out2);
		SendBankSelectAndProgramChange(base, req.value2, (unsigned char)out1, (unsigned char)out2);
	} else if (mode == 1) {
		if (base[0x6d5] == 0)
			return;
		if (base[0x6d7] == 0)
			return;
		char out1 = 0, out2 = 0;
		USTGAliasBankTypes::ConvertCombiBankToMidiBank((int)req.value1, out1, out2);
		SendBankSelectAndProgramChange(base, req.value2, (unsigned char)out1, (unsigned char)out2);
	} else if (mode == 2) {
		if (req.value2 > 0x7f) {
			SKSTGGate_ShouldSyncExternalClock();
			return;
		}
		bool shouldSync = SKSTGGate_ShouldSyncExternalClock();
		if (!shouldSync) {
			unsigned char msg[2];
			msg[0] = 0xf3;
			msg[1] = (unsigned char)req.value2;
			CSTGMidiPortManager::sInstance->WriteSTGMidiOutQueue(msg, 2);
		}
		/* Confirmed real quirk: called a second time here, its own
		 * return value discarded both times in this branch. */
		SKSTGGate_ShouldSyncExternalClock();
	}
	/* else: mode not in {0,1,2} -> no-op */
}

/*
 * HandleMidiBankAndPerformanceChange() (sec 10.99): see oa_global.h for
 * the full confirmed shape, including the two-different-dual-slots
 * quirk.
 */
void CSTGGlobal::HandleMidiBankAndPerformanceChange(unsigned char p1, unsigned char p2, unsigned char p3)
{
	unsigned char *base = (unsigned char *)this;
	CSTGMidiDispatcher *dispatcher = CSTGMidiDispatcher::sInstance;
	if (((unsigned char *)dispatcher)[0xa2] == 0)
		return;

	unsigned char selector = base[0x2975185];
	unsigned char *slotA = (selector == 0) ? (base + 0x297514c) : (base + 0x2975168);
	if (slotA[0] == 0) {
		if (*(unsigned int *)(slotA + 4) == 0 &&
		    *(unsigned int *)(slotA + 0xc) == 0xfffe)
			return;
	}

	unsigned char *slotB = (selector != 0) ? (base + 0x297514c) : (base + 0x2975168);

	CSTGPerfChangeRequest request;
	request.source = 2;
	request.field14 = 0;
	request.field18 = 0;
	request.value2 = p3;

	if (slotB[0] != 0) {
		request.tag = 1;
		request.mode = 0;
		request.value1 = (p1 != 0) ? *(unsigned int *)(slotB + 8) : p2;
	} else {
		request.tag = 0;
		unsigned int slotBField4 = *(unsigned int *)(slotB + 4);
		if (slotBField4 != 0) {
			if (slotBField4 != 1)
				return;
			int combiBank = 0;
			USTGAliasBankTypes::ConvertMidiBankToCombiBank((char)p1, (char)p2, combiBank);
			request.mode = 1;
			request.value1 = (unsigned int)combiBank;
		} else {
			int aliasBank = 0;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank((char)p1, (char)p2, aliasBank);
			request.mode = 0;
			request.value1 = (unsigned int)aliasBank;
		}
	}

	SubmitPerfChangeRequest(request);
}

/*
 * GetFreeSlotVoiceData() (sec 10.100): see oa_global.h for the full
 * confirmed shape, including the two confirmed real hazards preserved
 * verbatim rather than guarded against.
 */
CSTGSlotVoiceData *CSTGGlobal::GetFreeSlotVoiceData()
{
	unsigned char *base = (unsigned char *)this;

	for (;;) {
		if (*(unsigned int *)(base + 0x29c98fc) != 0) {
			unsigned int freeHead = *(unsigned int *)(base + 0x29c98f4);
			if (freeHead == *(unsigned int *)(base + 0x29c98f8))
				*(unsigned int *)(base + 0x29c98f8) = 0;

			/* Confirmed real: dereferences freeHead unconditionally
			 * here, even if it's 0 (see this method's own header
			 * comment). */
			unsigned char *link = FromU32(freeHead);
			unsigned int next = *(unsigned int *)link;
			*(unsigned int *)(base + 0x29c98f4) = next;
			unsigned int prev = *(unsigned int *)(link + 4);
			if (prev != 0)
				*(unsigned int *)FromU32(prev) = next;
			if (next != 0)
				*(unsigned int *)(FromU32(next) + 4) = prev;
			*(unsigned int *)(link + 0) = 0;
			*(unsigned int *)(link + 4) = 0;
			*(unsigned int *)(link + 0xc) = 0;
			(*(unsigned int *)(base + 0x29c98fc))--;

			unsigned char *activeNode = FromU32(*(unsigned int *)(link + 8));

			unsigned int activeHead = *(unsigned int *)(base + 0x29c9900);
			unsigned int selfLink = ToU32(activeNode + 0x24);
			if (activeHead == 0) {
				*(unsigned int *)(base + 0x29c9904) = selfLink;
			} else {
				unsigned int backLink = *(unsigned int *)(FromU32(activeHead) + 4);
				*(unsigned int *)(activeNode + 0x28) = backLink;
				if (backLink != 0)
					*(unsigned int *)FromU32(backLink) = selfLink;
				*(unsigned int *)(FromU32(activeHead) + 4) = selfLink;
			}
			*(unsigned int *)(base + 0x29c9900) = selfLink;
			*(unsigned int *)(activeNode + 0x30) = ToU32(base + 0x29c9900);
			(*(unsigned int *)(base + 0x29c9908))++;

			return (CSTGSlotVoiceData *)link;
		}

		/* Free list empty -- steal a dying voice (same group-bit scan
		 * as EmergencyFreeDyingSlotVoiceData, sec 10.92). */
		bool stole = false;
		for (unsigned int group = 0; group < 2 && !stole; group++) {
			unsigned int node = *(unsigned int *)(base + 0x29c9904);
			while (node != 0) {
				unsigned char *nodePtr = FromU32(node);
				unsigned char *voiceData = FromU32(*(unsigned int *)(nodePtr + 8));
				unsigned int next = *(unsigned int *)(nodePtr + 4);

				unsigned char *sub = FromU32(*(unsigned int *)(voiceData + 0x34));
				unsigned int groupBit = (sub[0x43] >> 1) & 1;
				if (groupBit != group) {
					node = next;
					continue;
				}
				if (voiceData[0x40] == 0) {
					node = next;
					continue;
				}

				unsigned short sum = (unsigned short)(*(unsigned short *)(voiceData + 0x4c) +
								       *(unsigned short *)(voiceData + 0x58));
				bool alsoFree = !(sum != 0 && voiceData[0x41] != 0);

				((CSTGSlotVoiceData *)voiceData)->EmergencyFreeAllVoices();
				if (alsoFree)
					((CSTGSlotVoiceData *)voiceData)->FreeSlotVoiceData(true);

				stole = true;
				break;
			}
		}
		/* Confirmed real: if no qualifying dying voice is found in
		 * either group, or the matching one didn't call
		 * FreeSlotVoiceData(true), the real code retries the whole
		 * function anyway -- see this method's own header comment. */
	}
}

/*
 * ProcessCCSpecialMapping() (sec 10.101): see oa_global.h for the full
 * confirmed shape, including the cross-confirmed per-mode target bases.
 */
int CSTGGlobal::ProcessCCSpecialMapping(unsigned char p1, unsigned char p2, unsigned char p3)
{
	unsigned char *base = (unsigned char *)this;
	if (p1 > 0x77)
		return 0;

	unsigned char *record = base + 0x29cc11c + (unsigned int)p1 * 8;
	if (record[0] == 0)
		return 0;

	unsigned char value = record[1];
	if (value != p2) {
		if (value != 0x10)
			return 0;
		if (p2 != base[0x6b8])
			return 0;
	}

	unsigned int tag = *(unsigned int *)(record + 4);

	if (record[2] == 0) {
		CSTGControllerRTData::sInstance->HandleControllerChange((int)tag, p3, false, false);
		return 1;
	}

	unsigned char *target;
	int mode = *(int *)(base + 0x684);
	if (mode == 1) {
		unsigned int idx = *(unsigned int *)(base + 0x69c) & 0x7f;
		unsigned int bankIdx = *(unsigned int *)(base + 0x690);
		target = base + idx * 0x19e7 + bankIdx * 0xcf381 + 0x1c77f10 + 6;
	} else if (mode == 2) {
		unsigned int idx = *(unsigned int *)(base + 0x6a0);
		target = base + idx * 0x1cad + 0x27cd024;
	} else {
		unsigned int fieldAt698 = *(unsigned int *)(base + 0x698);
		if (fieldAt698 == 0xfffe) {
			target = base + 0x2976e33;
		} else {
			unsigned int idx = fieldAt698 & 0x7f;
			unsigned int bankIdx = *(unsigned int *)(base + 0x68c);
			target = base + idx * 0xcec + bankIdx * 0x67603 + 0x132e4d0 + 3;
		}
	}

	target[0xadf] = (unsigned char)tag;
	bool velGate = (p3 > 0x3f);
	((CSTGControllerInfo *)(target + 0xad3))->SetPerfSwitch(2, velGate);
	return 1;
}

/*
 * CompletePerformanceActivation() (sec 10.102): see oa_global.h for the
 * full confirmed shape, including the confirmed real quirk where
 * OnPerformanceActivate() is called unconditionally with a pointer
 * that's only meaningfully set on one branch.
 */
void CSTGGlobal::CompletePerformanceActivation()
{
	unsigned char *base = (unsigned char *)this;
	base[0x2975184] = 3;

	unsigned char *perf; /* deliberately left uninitialized on the
			       * +0x6a4==0 path -- see header comment. */

	if (base[0x6a4] != 0) {
		CSTGPerformanceVarsManager *mgr = ResolveActivePerformanceVarsManager();
		perf = FromU32(*(unsigned int *)((unsigned char *)mgr + 0x23d4));

		unsigned int idx = base[0x6a5];
		if (idx >= 0x80)
			idx = 0;
		unsigned int idx2 = base[0x6a6];
		unsigned int offset = idx * 0x834 + (idx2 << 4);

		unsigned char *setListObj = base + idx * 0x834 + 0x293374c;
		unsigned char *setListSlot = base + 0x2933750 + offset;

		((CSetList *)setListObj)->Activate();
		((CSetListSlot *)setListSlot)->Activate();
	}

	if (base[0x29cc0c9] & 1) {
		CSTGControllerRTData::sInstance->OnExtModeSetChange();
		base[0x29cc0c9] &= ~1;
	}

	unsigned char valA, valB;
	int mode = *(int *)(base + 0x684);
	if (mode == 1) {
		valA = (unsigned char)(*(unsigned int *)(base + 0x690) & 0x3f);
		valB = base[0x69c];
	} else if (mode == 2) {
		valA = 0x80;
		valB = base[0x6a0];
	} else {
		valA = (unsigned char)((*(unsigned int *)(base + 0x68c) & 0x3f) | 0x40);
		valB = base[0x698];
	}

	unsigned char *queueWriter = (unsigned char *)CSTGMidiPortManager::sInstance + 0x208;

	unsigned char msg1[5];
	msg1[0] = (unsigned char)(base[0x6b8] | 0xc0);
	msg1[1] = valA;
	msg1[2] = valB;
	msg1[3] = 0x15;
	msg1[4] = 0xfe;
	((CSTGMidiQueueWriter *)queueWriter)->Write(msg1, 5, false);

	CSTGControllerRTData::sInstance->OnPerformanceActivate(*(CSTGPerformance *)perf);

	unsigned char msg2[5];
	msg2[0] = (unsigned char)(base[0x6b8] | 0xb0);
	msg2[1] = 0x79;
	msg2[2] = 0x04;
	msg2[3] = 0x05;
	msg2[4] = 0xfe;
	((CSTGMidiQueueWriter *)queueWriter)->Write(msg2, 5, false);
}

/*
 * IncrementCombiIndex/IncrementAliasProgramIndex/DecrementCombiIndex/
 * DecrementAliasProgramIndex (sec 10.127): see oa_global.h for the full
 * confirmed shape.
 */
void USTGAliasBankTypes::IncrementCombiIndex(int bankId, unsigned int index,
					       int &outBankId, unsigned int &outIndex)
{
	unsigned int newIndex = index + 1;
	if (newIndex <= 0x7f) {
		outIndex = newIndex;
		outBankId = bankId;
		return;
	}
	int newBank = bankId + 1;
	outIndex = 0;
	outBankId = (newBank < 0xe) ? newBank : 0;
}
void USTGAliasBankTypes::IncrementAliasProgramIndex(int bankId, unsigned int index,
						      int &outBankId, unsigned int &outIndex)
{
	unsigned int newIndex = index + 1;
	if (newIndex <= 0x7f) {
		outIndex = newIndex;
		outBankId = bankId;
		return;
	}
	int newBank = bankId + 1;
	outIndex = 0;
	outBankId = (newBank < 0x1f) ? newBank : 0;
}
void USTGAliasBankTypes::DecrementCombiIndex(int bankId, unsigned int index,
					       int &outBankId, unsigned int &outIndex)
{
	if (index != 0) {
		outIndex = index - 1;
		outBankId = bankId;
		return;
	}
	outIndex = 0x7f;
	outBankId = (bankId != 0) ? (bankId - 1) : 0xd;
}
void USTGAliasBankTypes::DecrementAliasProgramIndex(int bankId, unsigned int index,
						      int &outBankId, unsigned int &outIndex)
{
	if (index != 0) {
		outIndex = index - 1;
		outBankId = bankId;
		return;
	}
	outIndex = 0x7f;
	outBankId = (bankId != 0) ? (bankId - 1) : 0x1e;
}

/*
 * IncrementPerformance()/DecrementPerformance() (sec 10.103): see
 * oa_global.h for the full confirmed shape, including the shared
 * dual-slot quirk with HandleMidiBankAndPerformanceChange (sec 10.99).
 */
void CSTGGlobal::IncrementPerformance()
{
	unsigned char *base = (unsigned char *)this;
	CSTGMidiDispatcher *dispatcher = CSTGMidiDispatcher::sInstance;
	if (((unsigned char *)dispatcher)[0xa2] == 0)
		return;

	unsigned char selector = base[0x2975185];
	unsigned char *slotA = (selector == 0) ? (base + 0x297514c) : (base + 0x2975168);
	if (slotA[0] == 0) {
		if (*(unsigned int *)(slotA + 4) == 0 &&
		    *(unsigned int *)(slotA + 0xc) == 0xfffe)
			return;
	}

	unsigned char *slotB = (selector != 0) ? (base + 0x297514c) : (base + 0x2975168);

	CSTGPerfChangeRequest request;
	request.source = 1;
	request.field14 = 0;
	request.field18 = 0;

	if (slotB[0] != 0) {
		unsigned int value1 = *(unsigned int *)(slotB + 8);
		unsigned int value2 = *(unsigned int *)(slotB + 0xc);
		request.tag = 1;
		request.mode = 0;

		if (value2 <= 0x7e) {
			request.value1 = value1;
			request.value2 = value2 + 1;
		} else if (value1 <= 0x7e) {
			request.value1 = value1 + 1;
			request.value2 = 0;
		} else {
			request.value1 = 0;
			request.value2 = 0;
		}
	} else {
		unsigned int slotBField4 = *(unsigned int *)(slotB + 4);
		unsigned int value1 = *(unsigned int *)(slotB + 8);
		unsigned int value2 = *(unsigned int *)(slotB + 0xc);
		request.tag = 0;

		if (slotBField4 != 0) {
			if (slotBField4 != 1)
				return;
			int outBankId = 0;
			unsigned int outIdx = 0;
			USTGAliasBankTypes::IncrementCombiIndex((int)value1, value2, outBankId, outIdx);
			request.mode = 1;
			request.value1 = (unsigned int)outBankId;
			request.value2 = outIdx;
		} else {
			int outBankId = 0;
			unsigned int outIdx = 0;
			USTGAliasBankTypes::IncrementAliasProgramIndex((int)value1, value2, outBankId, outIdx);
			request.mode = 0;
			request.value1 = (unsigned int)outBankId;
			request.value2 = outIdx;
		}
	}

	SubmitPerfChangeRequest(request);
}

void CSTGGlobal::DecrementPerformance()
{
	unsigned char *base = (unsigned char *)this;
	CSTGMidiDispatcher *dispatcher = CSTGMidiDispatcher::sInstance;
	if (((unsigned char *)dispatcher)[0xa2] == 0)
		return;

	unsigned char selector = base[0x2975185];
	unsigned char *slotA = (selector == 0) ? (base + 0x297514c) : (base + 0x2975168);
	if (slotA[0] == 0) {
		if (*(unsigned int *)(slotA + 4) == 0 &&
		    *(unsigned int *)(slotA + 0xc) == 0xfffe)
			return;
	}

	unsigned char *slotB = (selector != 0) ? (base + 0x297514c) : (base + 0x2975168);

	CSTGPerfChangeRequest request;
	request.source = 1;
	request.field14 = 0;
	request.field18 = 0;

	if (slotB[0] != 0) {
		unsigned int value1 = *(unsigned int *)(slotB + 8);
		unsigned int value2 = *(unsigned int *)(slotB + 0xc);
		request.tag = 1;
		request.mode = 0;

		if (value2 != 0) {
			request.value1 = value1;
			request.value2 = value2 - 1;
		} else if (value1 != 0) {
			request.value1 = value1 - 1;
			request.value2 = 0x7f;
		} else {
			request.value1 = 0x7f;
			request.value2 = 0x7f;
		}
	} else {
		unsigned int slotBField4 = *(unsigned int *)(slotB + 4);
		unsigned int value1 = *(unsigned int *)(slotB + 8);
		unsigned int value2 = *(unsigned int *)(slotB + 0xc);
		request.tag = 0;

		if (slotBField4 != 0) {
			if (slotBField4 != 1)
				return;
			int outBankId = 0;
			unsigned int outIdx = 0;
			USTGAliasBankTypes::DecrementCombiIndex((int)value1, value2, outBankId, outIdx);
			request.mode = 1;
			request.value1 = (unsigned int)outBankId;
			request.value2 = outIdx;
		} else {
			int outBankId = 0;
			unsigned int outIdx = 0;
			USTGAliasBankTypes::DecrementAliasProgramIndex((int)value1, value2, outBankId, outIdx);
			request.mode = 0;
			request.value1 = (unsigned int)outBankId;
			request.value2 = outIdx;
		}
	}

	SubmitPerfChangeRequest(request);
}

/*
 * ProcessPerfChangeRequest() (sec 10.104): see oa_global.h for the full
 * confirmed shape, including the byte-identical relationship with
 * PreprocessPerformanceChange's own deactivate sequence.
 */
void CSTGGlobal::ProcessPerfChangeRequest(const CSTGPerfChangeRequest &req)
{
	unsigned char *base = (unsigned char *)this;
	CSTGPerfChangeRequest *slotA = (CSTGPerfChangeRequest *)(base + 0x297514c);
	*slotA = req;

	if (IsSetListSlotChangeOnly(*slotA)) {
		unsigned int idx = slotA->value1;
		if (idx >= 0x80)
			idx = 0;
		unsigned int idx2 = slotA->value2;
		unsigned int offset = idx * 0x834 + (idx2 << 4);

		base[0x6a6] = (unsigned char)idx2;
		unsigned char *record = base + 0x2933750 + offset;

		int mode = *(int *)(base + 0x684);
		if (mode == 2) {
			unsigned int seqIndex = *(unsigned int *)(base + 0x6a0);
			unsigned char *seqObj = base + seqIndex * 0x1cad + 0x27cd024;
			unsigned char extraByte = record[3];
			typedef void (*VtableSlot1eFn)(void *, unsigned int);
			void **vtable = *(void ***)seqObj;
			VtableSlot1eFn fn = (VtableSlot1eFn)vtable[0x1e];
			fn(seqObj, extraByte);
		}

		((CSetListSlot *)record)->Activate();

		unsigned int source = slotA->source;
		if (source == 1 || source == 2) {
			unsigned char msg[0x18];
			*(unsigned short *)(msg + 0x0) = 0x18;
			*(unsigned short *)(msg + 0x2) = 1;
			*(unsigned int *)(msg + 0x4) = 0;
			*(unsigned int *)(msg + 0x8) = 0x1f;
			*(unsigned int *)(msg + 0xc) = base[0x6a5];
			*(unsigned int *)(msg + 0x10) = idx2;
			*(unsigned int *)(msg + 0x14) = 3;
			PushUnsolicitedMessage(msg);
		}
		if (source == 0 || source == 1)
			SendPerfChangeToMidiOut(*slotA);
	} else {
		RunPerformanceDeactivateSequence(base);
	}
}

/*
 * StartPendingPerformanceChange() (sec 10.105): see oa_global.h for the
 * full confirmed shape, including why this simply forwards to
 * ProcessPerfChangeRequest rather than duplicating its body.
 */
void CSTGGlobal::StartPendingPerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	if (base[0x2975184] != 0)
		return;
	if (base[0x2975185] == 0)
		return;
	base[0x2975185] = 0;

	ProcessPerfChangeRequest(*(CSTGPerfChangeRequest *)(base + 0x2975168));
}

/*
 * GetMostRecentlyRequestedPerformanceIdForType() (sec 10.106): see
 * oa_global.h for the full confirmed shape, including the confirmed
 * real match-vs-resolve directional asymmetry.
 */
bool CSTGGlobal::GetMostRecentlyRequestedPerformanceIdForType(unsigned int type, CPerformanceId *out) const
{
	const unsigned char *base = (const unsigned char *)this;

	if (base[0x2975185] != 0) {
		unsigned char tag = base[0x2975168];
		bool matched;
		if (tag != 0) {
			unsigned int v1 = *(const unsigned int *)(base + 0x2975170);
			unsigned int v2 = *(const unsigned int *)(base + 0x2975174);
			unsigned int idx = (v1 >= 0x80) ? 0 : v1;
			unsigned int offset = idx * 0x834 + (v2 << 4);
			signed char recType = (signed char)base[0x2933750 + offset];
			matched = ((int)type == (int)recType);
		} else {
			unsigned int mode = *(const unsigned int *)(base + 0x297516c);
			unsigned int m = mode - 1;
			unsigned int cmpVal = (m <= 1) ? *(const unsigned int *)(base + 0x64 + m * 4) : 1;
			matched = (type == cmpVal);
		}

		if (matched) {
			if (tag != 0) {
				unsigned int v1 = *(const unsigned int *)(base + 0x2975170);
				unsigned int v2 = *(const unsigned int *)(base + 0x2975174);
				unsigned int idx = (v1 >= 0x80) ? 0 : v1;
				unsigned int offset2 = idx * 0x834 + (v2 << 4);
				const unsigned char *record2 = base + 0x2933740 + offset2;
				unsigned short packed = *(const unsigned short *)(record2 + 0x10);
				out->byte0 = (unsigned char)(packed & 0xff);
				out->byte1 = (unsigned char)(packed >> 8);
				out->byte2 = record2[0x12];
			} else {
				unsigned int v2 = *(const unsigned int *)(base + 0x2975174);
				if (v2 == 0xfffe)
					return false;
				unsigned int mode = *(const unsigned int *)(base + 0x297516c);
				unsigned int v1 = *(const unsigned int *)(base + 0x2975170);
				unsigned int m = mode - 1;
				out->byte0 = (m <= 1) ? 1 : base[0x64 + m * 4];
				out->byte1 = (unsigned char)v1;
				out->byte2 = (unsigned char)v2;
			}
			return true;
		}
	}

	{
		unsigned char tag = base[0x297514c];
		bool matched;
		if (tag != 0) {
			unsigned int v1 = *(const unsigned int *)(base + 0x2975154);
			unsigned int v2 = *(const unsigned int *)(base + 0x2975158);
			unsigned int idx = (v1 >= 0x80) ? 0 : v1;
			unsigned int offset = idx * 0x834 + (v2 << 4);
			signed char recType = (signed char)base[0x2933750 + offset];
			matched = ((int)type == (int)recType);
		} else {
			unsigned int mode = *(const unsigned int *)(base + 0x2975150);
			unsigned int m = mode - 1;
			unsigned int cmpVal = (m <= 1) ? *(const unsigned int *)(base + 0x64 + m * 4) : 1;
			matched = (type == cmpVal);
		}

		if (matched) {
			if (tag != 0) {
				unsigned int v1 = *(const unsigned int *)(base + 0x2975154);
				unsigned int v2 = *(const unsigned int *)(base + 0x2975158);
				unsigned int idx = (v1 >= 0x80) ? 0 : v1;
				unsigned int offset2 = idx * 0x834 + (v2 << 4);
				const unsigned char *record2 = base + 0x2933740 + offset2;
				unsigned short packed = *(const unsigned short *)(record2 + 0x10);
				out->byte0 = (unsigned char)(packed & 0xff);
				out->byte1 = (unsigned char)(packed >> 8);
				out->byte2 = record2[0x12];
			} else {
				unsigned int v2 = *(const unsigned int *)(base + 0x2975158);
				if (v2 == 0xfffe)
					return false;
				unsigned int mode = *(const unsigned int *)(base + 0x2975150);
				unsigned int v1 = *(const unsigned int *)(base + 0x2975154);
				unsigned int m = mode - 1;
				out->byte0 = (m <= 1) ? 1 : base[0x64 + m * 4];
				out->byte1 = (unsigned char)v1;
				out->byte2 = (unsigned char)v2;
			}
			return true;
		}
	}

	if (type == 1) {
		unsigned int f694 = *(const unsigned int *)(base + 0x694);
		if (f694 == 0xfffe)
			return false;
		unsigned int f688 = *(const unsigned int *)(base + 0x688);
		out->byte0 = 1;
		out->byte1 = (unsigned char)f688;
		out->byte2 = (unsigned char)f694;
		return true;
	} else if (type == 2) {
		unsigned int f6a0 = *(const unsigned int *)(base + 0x6a0);
		out->byte0 = 2;
		out->byte1 = 0;
		out->byte2 = (unsigned char)f6a0;
		return true;
	} else if (type == 0) {
		unsigned int f69c = *(const unsigned int *)(base + 0x69c);
		unsigned int f690 = *(const unsigned int *)(base + 0x690);
		out->byte0 = 0;
		out->byte1 = (unsigned char)f690;
		out->byte2 = (unsigned char)f69c;
		return true;
	}

	return false;
}

/*
 * SetEditInContextState() (sec 10.107): see oa_global.h for the full
 * confirmed shape, including why the 16 individual bit operations
 * reduce to a single word copy.
 */
void CSTGGlobal::SetEditInContextState(int type, unsigned int value)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + 0x29cc4dc) = type;
	*(unsigned int *)(base + 0x29cc4e0) = value;

	if (type == 0)
		return;

	unsigned char *ctrlRT = (unsigned char *)CSTGControllerRTData::sInstance;
	*(unsigned short *)(base + 0x29cc4e4) = *(unsigned short *)(ctrlRT + 0x22);
	*(unsigned short *)(ctrlRT + 0x22) = 0;
	CSTGControllerRTData::sInstance->NotifySoloChange();
}

/*
 * CompletePerformanceChange() (sec 10.108): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGGlobal::CompletePerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int source = *(unsigned int *)(base + 0x297515c);
	unsigned int m = source - 1;
	base[0x2975184] = 0;

	if (m <= 1) {
		unsigned char msg[0x1c];
		if (base[0x297514c] != 0) {
			unsigned int value1 = *(unsigned int *)(base + 0x2975154);
			unsigned int value2 = *(unsigned int *)(base + 0x2975158);
			*(unsigned short *)(msg + 0x0) = 0x18;
			*(unsigned short *)(msg + 0x2) = 1;
			*(unsigned int *)(msg + 0x4) = 0;
			*(unsigned int *)(msg + 0x8) = 0x1f;
			*(unsigned int *)(msg + 0xc) = value1;
			*(unsigned int *)(msg + 0x10) = value2;
			*(unsigned int *)(msg + 0x14) = 3;
			PushUnsolicitedMessage(msg);
		} else {
			unsigned int mode = *(unsigned int *)(base + 0x2975150);
			unsigned int value1 = *(unsigned int *)(base + 0x2975154);
			unsigned int value2 = *(unsigned int *)(base + 0x2975158);
			unsigned int mm = mode - 1;
			unsigned int resolvedType = (mm <= 1) ? *(unsigned int *)(base + 0x64 + mm * 4) : 1;

			*(unsigned short *)(msg + 0x0) = 0x1c;
			*(unsigned short *)(msg + 0x2) = 1;
			*(unsigned int *)(msg + 0x4) = 0;
			*(unsigned int *)(msg + 0x8) = 0;
			*(unsigned int *)(msg + 0xc) = resolvedType;
			*(unsigned int *)(msg + 0x10) = value1;
			*(unsigned int *)(msg + 0x14) = value2;
			*(unsigned int *)(msg + 0x18) = 3;
			PushUnsolicitedMessage(msg);
		}
	}

	((unsigned char *)CSTGMidiDispatcher::sInstance)[0] = 0;
	((unsigned char *)CSTGMessageProcessor::sInstance)[0x54] = 0;
	CSTGControllerRTData::sInstance->ResetAllJumpCatch();

	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	for (unsigned int off = 0x29138; off <= 0x291f4; off += 4)
		*(unsigned int *)(status + off) = 0;
}

/*
 * CSetListSlot::BeginActivation() (sec 10.110): see oa_global.h for the
 * full confirmed shape.
 */
void CSetListSlot::BeginActivation()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	mgr[0x23ec] = base[0x8];
}

/*
 * CSetListSlot::Activate() (sec 10.141): see oa_global.h for the full
 * confirmed shape.
 */
void CSetListSlot::Activate()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	*(unsigned int *)(mgr + 0x23f0) = *(unsigned int *)(base + 4);
	*(unsigned int *)(mgr + 0x23e0) = *(unsigned int *)(base + 0xc);
}

/*
 * CSetList::Activate() (batch 41, sec 10.192, .text+0x2012e0, 266 bytes)
 * confirmed: resolves the active CSTGPerformanceVarsManager via the same
 * shared ResolveActivePerformanceVarsManagerRaw() helper as its sibling
 * just above, then treats `mgr+0x2160` as an embedded CSetListEQ
 * sub-object:
 *   - mgr+0x2168 (target+0x8) = this->fieldAt(0x828)!=0 ? 0.0f : 1.0f --
 *     a real "mute gain" multiplier. Confirmed via a `cmovne` selecting
 *     between the hardcoded 1.0f default and a `.rodata.cst4+0xc08`
 *     constant that decodes to exactly 0.0f.
 *   - mgr+0x2170 (target+0x10) = this->fieldAt(0x82c), a raw dword copy
 *     (no interpretation).
 *   - calls SetBand(band, gain) on the mgr+0x2160 CSetListEQ nine times,
 *     band = 0..8, gain = this's own nine contiguous floats at
 *     +0x804..+0x824 (stride 4).
 * SetBand()'s own body is genuine SSE/x87 EQ-coefficient DSP -- out of
 * scope per the sec 10.185 policy, so it stays a confirmed-real,
 * deliberately-deferred no-op (bar2_stubs.cpp). This function's own
 * control flow / argument marshaling is fully real regardless of that.
 */
void CSetList::Activate()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	CSetListEQ *target = (CSetListEQ *)(mgr + 0x2160);

	*(float *)(mgr + 0x2168) = (base[0x828] != 0) ? 0.0f : 1.0f;
	*(unsigned int *)(mgr + 0x2170) = *(unsigned int *)(base + 0x82c);

	for (unsigned int band = 0; band < 9; band++)
		target->SetBand(band, *(float *)(base + 0x804 + band * 4));
}

/*
 * CSTGProgramSlot::GetProperMidiChannel() const (sec 10.110): see
 * oa_global.h for the full confirmed shape.
 */
unsigned char CSTGProgramSlot::GetProperMidiChannel() const
{
	const unsigned char *base = (const unsigned char *)this;
	unsigned char channel = base[0x10];
	if (channel == 0x10)
		return ((const unsigned char *)CSTGGlobal::sInstance)[0x6b8];
	return channel;
}

/*
 * CSTGProgramSlot::ResolveActiveVoiceDataNode() (sec 10.142): see
 * oa_global.h for the full confirmed shape shared by IsActive()/
 * AccessActiveSlotVoiceData()/HasActiveSlotVoiceData()/HasActiveVoices().
 */
static unsigned char *ResolveActiveVoiceDataNode(const CSTGProgramSlot *slot)
{
	const unsigned char *base = (const unsigned char *)slot;
	unsigned int idx = base[0x4];
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned int nodePacked = *(unsigned int *)(g + 0x29c990c + idx * 12);
	return (unsigned char *)(unsigned long)nodePacked;
}

bool CSTGProgramSlot::IsActive() const
{
	unsigned char *node = ResolveActiveVoiceDataNode(this);
	if (!node)
		return false;
	unsigned int payloadPacked = *(unsigned int *)(node + 0x8);
	unsigned char *payload = (unsigned char *)(unsigned long)payloadPacked;
	unsigned int ownerPacked = *(unsigned int *)(payload + 0x34);
	return ownerPacked == (unsigned int)(unsigned long)this;
}

void *CSTGProgramSlot::AccessActiveSlotVoiceData() const
{
	unsigned char *node = ResolveActiveVoiceDataNode(this);
	if (!node)
		return 0;
	unsigned int payloadPacked = *(unsigned int *)(node + 0x8);
	return (void *)(unsigned long)payloadPacked;
}

bool CSTGProgramSlot::HasActiveSlotVoiceData() const
{
	return ResolveActiveVoiceDataNode(this) != 0;
}

bool CSTGProgramSlot::HasActiveVoices() const
{
	unsigned char *node = ResolveActiveVoiceDataNode(this);
	if (!node)
		return false;
	unsigned int payloadPacked = *(unsigned int *)(node + 0x8);
	if (!payloadPacked)
		return false;
	unsigned char *payload = (unsigned char *)(unsigned long)payloadPacked;
	unsigned short a = *(unsigned short *)(payload + 0x4c);
	unsigned short b = *(unsigned short *)(payload + 0x58);
	return (unsigned short)(a + b) != 0;
}

/*
 * CSTGProgramSlot::ProcessPreviousSVDOnProgramChange(CSTGSlotVoiceData*)
 * (.text+0xab030, 81 bytes) -- confirmed real virtual slot 56 BASE
 * implementation (installed directly, unoverridden, into
 * `_ZTV28CSTGProgramModeDrumTrackSlot`'s own slot 56 -- confirmed via
 * readelf -r against that vtable's own relocation table, which resolves
 * straight to this symbol rather than to any drum-track-specific
 * override). Modeled as a plain free function matching CallVtableSlot56's
 * own `Fn` shape (see below) rather than a real C++ virtual method --
 * matching this whole class's own established "manual vtable-pointer
 * field, no real C++ virtuals" convention (CallVtableSlot7 above). Never
 * called directly by name anywhere in ground truth (confirmed via a
 * project-wide relocation search) -- reached ONLY via vtable dispatch
 * from ChangeProgram() below.
 *
 * Reuses the SAME confirmed real "voice count" pair (`+0x4c`/`+0x58`,
 * summed as a 16-bit add) `CSTGProgramSlot::HasActiveVoices()` already
 * established above -- an independent cross-confirmation of that pair's
 * meaning, not a fresh guess: nonzero means the previous slot voice data
 * still has voices sounding (mark it dying, set `+0x41` -- the SAME
 * "being stolen" flag `Steal()`/`SetIsDying()` already established,
 * oa_global.h's own `SetIsDying()` comment), zero means it's already
 * silent (safe to free outright via `FreeSlotVoiceData(false)`). Both
 * non-null-svd branches return true; only a null `svd` returns false
 * (defensive -- ChangeProgram's own call site below only ever reaches
 * this with a confirmed non-null payload, per its own node/payload gate).
 */
bool ProgramSlot_ProcessPreviousSVDOnProgramChange(void *selfVoid, CSTGSlotVoiceData *svd)
{
	(void)selfVoid;
	if (!svd)
		return false;
	unsigned char *s = (unsigned char *)svd;
	unsigned short voiceCount = (unsigned short)(*(unsigned short *)(s + 0x4c) +
						      *(unsigned short *)(s + 0x58));
	if (voiceCount != 0) {
		svd->SetIsDying();
		s[0x41] = 1;
	} else {
		svd->FreeSlotVoiceData(false);
	}
	return true;
}

/*
 * CSTGProgramModeProgramSlot::ProcessPreviousSVDOnProgramChange(CSTGSlotVoiceData*)
 * (.text+0xb9760, 23 bytes) -- confirmed real OVERRIDE, installed into
 * `_ZTV26CSTGProgramModeProgramSlot`'s own slot 56 (same readelf -r
 * cross-check as the base version above). A real, confirmed quirk,
 * verified directly in the raw disassembly (not a transcription slip):
 * this ALWAYS returns false, even on the branch that calls
 * `SetIsDying()` -- the real code's final `xor eax,eax` is the
 * unconditional fallthrough target of BOTH the null-svd and non-null-svd
 * paths. Net effect at ChangeProgram's own call site: ordinary
 * (non-drum-track) program slots never carry the previous slot's live
 * channel-values snapshot forward across a program change; drum-track
 * slots (base impl above) do, whenever a previous voice is still
 * sounding.
 */
bool ProgramModeProgramSlot_ProcessPreviousSVDOnProgramChange(void *selfVoid, CSTGSlotVoiceData *svd)
{
	(void)selfVoid;
	if (svd)
		svd->SetIsDying();
	return false;
}

/*
 * Real vtable slot 56 dispatch (`call *0xe0(%ecx)` in ground truth,
 * where %ecx is the object's own installed vtable pointer) -- same raw-
 * indirect-dispatch treatment as CallVtableSlot7 above. Takes one
 * explicit argument (the previous slot voice data payload, confirmed
 * regparm(3) `this=eax, arg1=edx` at the real call site) and returns a
 * bool in AL, both confirmed from ChangeProgram()'s own disassembly.
 */
static inline bool CallVtableSlot56(void *obj, CSTGSlotVoiceData *svd)
{
	typedef bool (*Fn)(void *, CSTGSlotVoiceData *);
	unsigned int vtablePacked = *(unsigned int *)obj;
	void **vtable = (void **)(unsigned long)vtablePacked;
	Fn fn = (Fn)vtable[56];
	return fn(obj, svd);
}

/*
 * CSTGProgramSlot::ChangeProgram(CSTGProgram*) (.text+0xac530, 300 bytes)
 * -- confirmed real, batch 47. Full disassembly (regparm(3): this=eax,
 * arg1=edx=newProgram):
 *
 * 1. `CSTGSmoother::sInstance->FinalizeAllSmoothers()` -- confirmed real
 *    (unconditional dereference of `sInstance`, no null check, matching
 *    this project's own established "deliberately-deferred-with-
 *    unconditional-dereference" idiom) -- FinalizeAllSmoothers() itself
 *    remains a deliberately deferred no-op stub (bar2_stubs.cpp), which
 *    is a safe callee here per the usual "calling a still-deferred stub
 *    is fine" precedent.
 * 2. Default-initializes a local 0x92c-byte scratch buffer (confirmed
 *    the exact real size of `CSTGChannelValues`, sec 10.151/oa_engine_init.h)
 *    -- the first 0x5a0 bytes (121 confirmed real 12-byte records) get a
 *    real, confirmed per-record write pattern (+0x0/+0x4 dwords zeroed,
 *    +0x8 word zeroed, +0xa byte set to 1, +0xb byte "keep garbage bits
 *    except force bit0 set/bit1 clear"); ground truth emits this as a
 *    120-iteration loop plus one manually-unrolled 121st record, folded
 *    here into one 121-iteration loop since both forms are byte-for-byte
 *    equivalent (confirmed by direct comparison of the unrolled tail
 *    against the loop body -- a real compiler code-gen artifact, not a
 *    semantic difference). The remaining 0x5a0..0x92c bytes are left as
 *    genuine uninitialized stack garbage in ground truth too (never
 *    written before either being fully overwritten by the copy below or
 *    never being read at all) -- confirmed dead/don't-care, not modeled.
 * 3. Looks up this slot's own active-voice-data node via the SAME
 *    `CSTGGlobal::sInstance + 0x29c990c + fieldAt(4)*12` table
 *    `ResolveActiveVoiceDataNode()` above already established (this
 *    slot's own `+0x4` "kind" byte is the SAME index) -- confirmed via a
 *    literal byte-for-byte match of the lookup sequence, not assumed.
 *    If the node OR its own `+0x8` payload is null, the local buffer
 *    stays discarded and `channelValues` (below) stays null (real
 *    behavior: no previous slot voice data to carry anything from).
 * 4. Otherwise: copies the payload's own `+0x1488` sub-object (the
 *    payload's embedded `CSTGChannelValues`, matching the 0x92c copy
 *    size exactly) into the local scratch buffer via an explicit byte
 *    loop (no memcpy, matching this project's own established
 *    freestanding-build convention, sec 10.56), THEN dispatches
 *    `ProcessPreviousSVDOnProgramChange()` (CallVtableSlot56) on the
 *    payload -- if it returns true, `channelValues` becomes the local
 *    buffer (now holding the previous slot's live channel values);
 *    otherwise it stays null. The buffer is unconditionally overwritten
 *    BEFORE this bool is even checked either way -- confirmed real, not
 *    a translation choice.
 * 5. Stores `newProgram` at `this->+0x5` (an unaligned dword field --
 *    this slot's own "current program" pointer, confirmed via the same
 *    regparm(3) `edx` register carried live across steps 1/3/4).
 * 6. `CSTGGlobal::sInstance->GetFreeSlotVoiceData()` -- confirmed real
 *    (sec 10.100). ITS OWN return value is a small free-list/active-list
 *    bookkeeping NODE (confirmed ~0x40 bytes via that function's own
 *    disassembly and test_global.cpp's own [38] scenario, which mocks it
 *    at exactly that size), NOT a full `CSTGSlotVoiceData` object despite
 *    the function's existing `CSTGSlotVoiceData*` return-type annotation
 *    (a harmless pre-existing type looseness -- nothing before this batch
 *    ever dereferenced the return value's own fields). `ChangeProgram` is
 *    the FIRST real caller to do so: it reads the node's own `+0x8` field
 *    to get the ACTUAL `CSTGSlotVoiceData*` payload to operate on --
 *    exactly the same node/payload split already established for
 *    `ResolveActiveVoiceDataNode()` above, now confirmed a second,
 *    independent time via this completely different table/free-list
 *    family. NOT a bug in the existing `GetFreeSlotVoiceData()`
 *    reconstruction (its own return value, and the `test_global.cpp`
 *    scenario checking it, are both still correct) -- just the first
 *    real call site to need the extra `+0x8` dereference.
 * 7. `slotVoiceData->Setup(this, newProgram, channelValues)` (confirmed
 *    real regparm(3): this=eax=slotVoiceData, arg1=edx=this(ProgramSlot),
 *    arg2=ecx=newProgram, arg3=[stack]=channelValues -- a genuine 4th
 *    argument beyond regparm(3)'s 3 register slots) then
 *    `this->CompleteLoadProgram(slotVoiceData)` (confirmed real
 *    regparm(3): this=eax=ProgramSlot, arg1=edx=slotVoiceData). BOTH are
 *    confirmed real (via direct PC32 relocations, and independently
 *    confirmed as a SECOND real caller exists too --
 *    `CSTGProgramSlot::LoadCombiTrackForPerformanceChangeEv`, not
 *    reconstructed in this project, out of scope) but substantially
 *    large (0xe44/3652 bytes and 0x35b/859 bytes respectively) -- out of
 *    scope per the sec 10.185 audio-DSP policy. This batch reconstructs
 *    ChangeProgram() itself (the caller) for real and gives both callees
 *    safe confirmed-real-but-deferred no-op stand-ins (bar2_stubs.cpp),
 *    matching the established reconstruct-caller-DSP-stub-callee pattern
 *    (sec 10.187 CSetListEQ::SetBand precedent). Ground truth's own
 *    final `mov eax,edi` (returning the SlotVoiceData pointer) is
 *    preserved in neither call site that reaches this function (both
 *    confirmed to discard EAX immediately after the call) -- kept `void`
 *    to match the pre-existing declaration, not independently re-verified
 *    beyond the two call sites checked.
 */
void CSTGProgramSlot::ChangeProgram(CSTGProgram *newProgram)
{
	CSTGSmoother::sInstance->FinalizeAllSmoothers();

	unsigned char localChannelValues[0x92c];
	for (int i = 0; i < 121; i++) {
		unsigned char *rec = localChannelValues + i * 0xc;
		*(unsigned int *)(rec + 0x0) = 0;
		*(unsigned int *)(rec + 0x4) = 0;
		*(unsigned short *)(rec + 0x8) = 0;
		rec[0xa] = 1;
		rec[0xb] = (unsigned char)((rec[0xb] | 1) & ~2);
	}

	unsigned char *node = ResolveActiveVoiceDataNode(this);
	void *channelValues = 0;
	if (node) {
		unsigned int payloadPacked = *(unsigned int *)(node + 0x8);
		if (payloadPacked) {
			unsigned char *payload = (unsigned char *)(unsigned long)payloadPacked;
			const unsigned char *src = payload + 0x1488;
			for (unsigned int i = 0; i < 0x92c; i++)
				localChannelValues[i] = src[i];
			/* Dispatches through THIS SLOT's own vtable (confirmed real
			 * `ecx=[ebx]` i.e. `this->+0x0`, NOT the payload's) -- `this`
			 * is the vtable source/class selector, `payload` is only the
			 * explicit argument (regparm(3) this=eax=ProgramSlot,
			 * arg1=edx=payload at the real call site). */
			bool keepPrevious = CallVtableSlot56(this, (CSTGSlotVoiceData *)payload);
			if (keepPrevious)
				channelValues = localChannelValues;
		}
	}

	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x5) = (unsigned int)(unsigned long)newProgram;

	unsigned char *freeNode = (unsigned char *)CSTGGlobal::sInstance->GetFreeSlotVoiceData();
	unsigned int svdPacked = *(unsigned int *)(freeNode + 0x8);
	CSTGSlotVoiceData *svd = (CSTGSlotVoiceData *)(unsigned long)svdPacked;

	svd->Setup(this, newProgram, (const CSTGChannelValues *)channelValues);
	CompleteLoadProgram(svd);
}

/*
 * CSTGPerformanceVars::BeginActivation() (sec 10.110): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGPerformanceVars::BeginActivation(CSTGPerformance *perf, bool flag)
{
	unsigned char *base = (unsigned char *)this;
	base[0x23ec] = 0;
	/* +0x23d8 is a packed 32-bit pointer in the real binary, not an
	 * 8-byte native one -- a native store here would clobber +0x23dd
	 * (the real `flag` byte, only 5 bytes past the pointer's own real
	 * start) on a 64-bit host. Same hazard this project has hit many
	 * times before (sec 10.55/10.56/10.58/etc). */
	*(unsigned int *)(base + 0x23d8) = ToU32((unsigned char *)perf);
	base[0x23dd] = flag;
	if (base[0x23d1] == 0)
		EnterActivatingState();
}

/*
 * USTGAliasBankTypes::GetAliasPgmBankMapping() (sec 10.110): see
 * oa_global.h for the full confirmed shape.
 */
void USTGAliasBankTypes::GetAliasPgmBankMapping(int bankId, unsigned int index, int &outBankId, unsigned int &outIndex)
{
	if (index == 0xfffe) {
		outBankId = 0;
		outIndex = 0xfffe;
		return;
	}

	unsigned int combinedIdx = ((unsigned int)bankId << 7) + index;
	outBankId = STGAliasToRealPgmBank[combinedIdx];
	outIndex = (unsigned int)STGAliasBankPgmMap[combinedIdx];
}

/*
 * CSTGPerformanceVars::FreeVoicelessDyingSlots() (sec 10.110): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGPerformanceVars::FreeVoicelessDyingSlots()
{
	unsigned char *base = (unsigned char *)this;
	if ((signed char)base[0x23d1] <= 2)
		return;

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *node = FromU32(*(unsigned int *)(global + 0x29c9900));
	if (node == 0)
		return;

	bool freedAny = false;
	unsigned char groupId = base[0x23d0];
	while (node != 0) {
		unsigned char *voiceData = FromU32(*(unsigned int *)(node + 0x8));
		unsigned char *next = FromU32(*(unsigned int *)(node + 0x0));

		if (voiceData[0x28c8] == groupId) {
			unsigned short sum = (unsigned short)(*(unsigned short *)(voiceData + 0x4c) +
							       *(unsigned short *)(voiceData + 0x58));
			if (sum == 0) {
				((CSTGSlotVoiceData *)voiceData)->FreeSlotVoiceData(false);
				freedAny = true;
			}
		}
		node = next;
	}

	if (freedAny)
		CLoadBalancer::sInstance->BalanceStaticLoad();
}

/*
 * CSTGPerformanceVarsManager::AllocPerformanceVars() (sec 10.112): see
 * oa_global.h for the full confirmed shape, including the confirmed
 * real but unreachable PushUnsolicitedMessage block.
 */
CSTGPerformanceVars *CSTGPerformanceVarsManager::AllocPerformanceVars()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int selector = (base[0x8] + 1) & 1;
	base[0x8] = (unsigned char)selector;

	unsigned char *mgr = FromU32(*(unsigned int *)(base + selector * 4));
	signed char state = (signed char)mgr[0x23d1];

	if (state == 0)
		return (CSTGPerformanceVars *)mgr;

	if (state == 5) {
		float ratio = *(float *)(mgr + 0x23fc) / 36.0f;
		if (ratio > *(float *)(mgr + 0x2400))
			*(float *)(mgr + 0x2400) = ratio;
	} else {
		mgr[0x23d1] = 5;

		if (state <= 1) {
			unsigned char *mgrSlot1 = FromU32(*(unsigned int *)(base + 4));
			unsigned char *mgrSlot0 = FromU32(*(unsigned int *)(base + 0));
			unsigned int count = (unsigned int)((signed char)mgrSlot1[0x23d1] > 1) +
					     (unsigned int)((signed char)mgrSlot0[0x23d1] > 1);
			*(unsigned int *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x1094) = count;

			/* Confirmed real but unreachable: mgr[0x23d1] was just
			 * forcibly set to 5 above, so this real guard is always
			 * false here and this block never runs in the compiled
			 * binary -- reproduced faithfully as dead code, matching
			 * this project's "preserve real quirks" convention. */
			if ((signed char)mgr[0x23d1] <= 1) {
				unsigned char oldFlag = mgr[0x240c];
				unsigned char msg[0x10];
				*(unsigned short *)(msg + 0x0) = 0x10;
				*(unsigned short *)(msg + 0x2) = 1;
				*(unsigned int *)(msg + 0x4) = 0;
				*(unsigned int *)(msg + 0x8) = 0x20;
				*(unsigned int *)(msg + 0xc) = oldFlag;
				PushUnsolicitedMessage(msg);
				mgr[0x240c] = 0;
			}
		}

		*(float *)(mgr + 0x23fc) = 1.0f;
		*(float *)(mgr + 0x2400) = 1.0f / 36.0f;
	}

	*(unsigned int *)(mgr + 0x23d8) = 0;
	return (CSTGPerformanceVars *)mgr;
}

/*
 * ProcessPerformanceChange() (sec 10.113): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGGlobal::ProcessPerformanceChange()
{
	unsigned char *base = (unsigned char *)this;
	base[0x2975184] = 2;

	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	((CSTGPerformanceVars *)mgr)->FreeVoicelessDyingSlots();

	CSTGPerformanceVars *newPerf =
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->AllocPerformanceVars();

	unsigned int category, val1, val2;

	if (base[0x297514c] != 0) {
		unsigned int rawValue1 = *(unsigned int *)(base + 0x2975154);
		unsigned int rawValue2 = *(unsigned int *)(base + 0x2975158);
		unsigned int clampedValue1 = (rawValue1 <= 0x7f) ? rawValue1 : 0;
		unsigned int offset = clampedValue1 * 0x834 + (rawValue2 << 4);
		signed char recordType = (signed char)base[0x2933750 + offset];

		base[0x6a5] = (unsigned char)rawValue1;
		base[0x6a4] = 1;
		base[0x6a6] = (unsigned char)rawValue2;

		unsigned char *record = base + offset + 0x2933740;
		val1 = record[0x11];
		val2 = record[0x12];

		/* Confirmed real: an UNSIGNED comparison -- a negative
		 * recordType byte sign-extends to a huge unsigned value and
		 * never matches, so only 0/1/2 select the table lookup. */
		if ((unsigned int)(int)recordType <= 2)
			category = *(unsigned int *)(base + 0x58 + (unsigned int)(int)recordType * 4);
		else
			category = 0;
	} else {
		category = *(unsigned int *)(base + 0x2975150);
		val1 = *(unsigned int *)(base + 0x2975154);
		val2 = *(unsigned int *)(base + 0x2975158);
		base[0x6a4] = 0;
	}

	/* Confirmed real: category values other than 0/1/2 (only reachable
	 * via the +0x58 table lookup above) skip ALL THREE of these
	 * field-write blocks entirely -- there is no catch-all "default"
	 * branch in the real disassembly, unlike every other mode dispatch
	 * in this cluster. +0x684 (and the other fields below) are left at
	 * whatever stale value a prior call left them at. Reproduced
	 * faithfully rather than adding a sanitizing default. */
	if (category == 1) {
		*(unsigned int *)(base + 0x684) = 1;
		*(unsigned int *)(base + 0x690) = val1;
		*(unsigned int *)(base + 0x69c) = val2;
	} else if (category == 2) {
		*(unsigned int *)(base + 0x684) = 2;
		*(unsigned int *)(base + 0x6a0) = val2;
	} else if (category == 0) {
		*(unsigned int *)(base + 0x688) = val1;
		*(unsigned int *)(base + 0x684) = 0;
		*(unsigned int *)(base + 0x694) = val2;
		int outBankId = 0;
		unsigned int outIndex = 0;
		USTGAliasBankTypes::GetAliasPgmBankMapping((int)val1, val2, outBankId, outIndex);
		*(unsigned int *)(base + 0x68c) = (unsigned int)outBankId;
		*(unsigned int *)(base + 0x698) = outIndex;
	}
	bool didProgramSlotChannel = (category == 2);

	if (didProgramSlotChannel) {
		unsigned int seqIdx = *(unsigned int *)(base + 0x6a0);
		unsigned int slotByte;
		if (base[0x6a4] == 0) {
			slotByte = base[seqIdx * 0x1cad + 0x27cdb07];
		} else {
			unsigned int idx = base[0x6a5];
			if (idx >= 0x80)
				idx = 0;
			unsigned int idx2 = base[0x6a6];
			unsigned int recOffset = idx * 0x834 + (idx2 << 4);
			slotByte = base[recOffset + 0x2933753];
		}
		unsigned char *progSlot = base + slotByte * 0xe8 + seqIdx * 0x1cad + 0x27cdb80 + 7;
		unsigned char channel = ((CSTGProgramSlot *)progSlot)->GetProperMidiChannel();
		base[0x6b9] = channel;
	} else {
		base[0x6b9] = base[0x6b8];
	}

	unsigned int source = *(unsigned int *)(base + 0x297515c);
	if (source > 1) {
		CSTGMidiDispatcher::sInstance->PerfChangeControllerReset();
	} else {
		SendPerfChangeToMidiOut(*(CSTGPerfChangeRequest *)(base + 0x297514c));
		CSTGMidiDispatcher::sInstance->PerfChangeControllerReset();
	}

	if (val2 == 0xfffe)
		((CSTGPerformanceVarsManager *)CSTGPerformanceVarsManager::sInstance)->StealAllDyingPerformanceVars();

	unsigned char *target;
	unsigned int mode = *(unsigned int *)(base + 0x684);
	if (mode == 1) {
		unsigned int bank = *(unsigned int *)(base + 0x69c) & 0x7f;
		unsigned int seqPart = *(unsigned int *)(base + 0x690) * 0xcf381;
		target = base + bank * 0x19e7 + seqPart + 0x1c77f10 + 6;
	} else if (mode == 2) {
		target = base + *(unsigned int *)(base + 0x6a0) * 0x1cad + 0x27cd024;
	} else {
		if (*(unsigned int *)(base + 0x698) == 0xfffe) {
			target = base + 0x2976e33;
		} else {
			unsigned int bank = *(unsigned int *)(base + 0x68c) & 0x7f;
			unsigned int seqPart = *(unsigned int *)(base + 0x694) * 0x67603;
			target = base + bank * 0xcec + seqPart + 0x132e4d0 + 3;
		}
	}

	newPerf->BeginActivation((CSTGPerformance *)target, base[0x6a4] != 0);

	if (base[0x6a4] != 0) {
		unsigned int idx = base[0x6a5];
		if (idx >= 0x80)
			idx = 0;
		unsigned int idx2 = base[0x6a6];
		unsigned int offset = idx * 0x834 + (idx2 << 4);
		unsigned char *slot = base + offset + 0x2933740 + 0x10;
		((CSetListSlot *)slot)->BeginActivation();
	}
}

/*
 * CSTGPerformanceVarsManager::StealAllDyingPerformanceVars() (sec
 * 10.114): see oa_global.h for the full confirmed shape, including the
 * confirmed-unreachable dead-code block.
 */
void CSTGPerformanceVarsManager::StealAllDyingPerformanceVars()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int selector = base[0x8];
	unsigned int otherSlot = (selector + 1) & 1;

	unsigned char *mgr = FromU32(*(unsigned int *)(base + otherSlot * 4));
	signed char state = (signed char)mgr[0x23d1];

	if (state <= 2)
		return;

	CSTGMidiDispatcher::sInstance->StealingRequiresOneTickStall();

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *node = FromU32(*(unsigned int *)(global + 0x29c9900));
	unsigned char groupId = mgr[0x23d0];
	while (node != 0) {
		unsigned char *voiceData = FromU32(*(unsigned int *)(node + 0x8));
		unsigned char *next = FromU32(*(unsigned int *)(node + 0x0));
		if (voiceData[0x28c8] == groupId)
			voiceData[0x42] = 1;
		node = next;
	}

	if (state == 5) {
		if (*(float *)(mgr + 0x23fc) > *(float *)(mgr + 0x2400))
			*(float *)(mgr + 0x2400) = *(float *)(mgr + 0x23fc);
	} else {
		mgr[0x23d1] = 5;

		/* Confirmed real but PROVABLY UNREACHABLE: the outer state>2
		 * gate above plus this branch's own state!=5 already exclude
		 * every value this block's own <=1 guard would need --
		 * reproduced faithfully, never executed, matching
		 * AllocPerformanceVars's own dead-code precedent (sec
		 * 10.112). */
		if (state <= 1) {
			unsigned char *mgrSlot1 = FromU32(*(unsigned int *)(base + 4));
			unsigned char *mgrSlot0 = FromU32(*(unsigned int *)(base + 0));
			unsigned int count = (unsigned int)((signed char)mgrSlot1[0x23d1] > 1) +
					     (unsigned int)((signed char)mgrSlot0[0x23d1] > 1);
			*(unsigned int *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x1094) = count;

			if ((signed char)mgr[0x23d1] <= 1) {
				unsigned char oldFlag = mgr[0x240c];
				unsigned char msg[0x10];
				*(unsigned short *)(msg + 0x0) = 0x10;
				*(unsigned short *)(msg + 0x2) = 1;
				*(unsigned int *)(msg + 0x4) = 0;
				*(unsigned int *)(msg + 0x8) = 0x20;
				*(unsigned int *)(msg + 0xc) = oldFlag;
				PushUnsolicitedMessage(msg);
			}
		}

		*(float *)(mgr + 0x23fc) = 1.0f;
		*(float *)(mgr + 0x2400) = 1.0f;
	}

	*(unsigned int *)(mgr + 0x23d8) = 0;
	mgr[0x240c] = 1;
}

/*
 * CSTGPerformanceVarsManager::RunEffects() (batch 49): see oa_global.h
 * for the full confirmed shape. Unlike StealAllDyingPerformanceVars()/
 * ResolveActivePerformanceVarsManager(), walks BOTH slots directly, no
 * sInstance[8] selector consulted, no null-check (matches ground truth).
 */
void CSTGPerformanceVarsManager::RunEffects()
{
	unsigned char *base = (unsigned char *)this;

	for (unsigned int i = 0; i < 2; i++) {
		unsigned char *perfVars = FromU32(*(unsigned int *)(base + i * 4));
		if ((signed char)perfVars[0x23d1] > 1) {
			CSTGPerformance *perf =
				(CSTGPerformance *)FromU32(*(unsigned int *)(perfVars + 0x23d4));
			perf->RunEffects((CSTGPerformanceVars *)perfVars);
		}
	}
}

/*
 * CSTGMidiDispatcher::StealingRequiresOneTickStall() (sec 10.136): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGMidiDispatcher::StealingRequiresOneTickStall()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	*(unsigned int *)(self + 0xa4) = *(unsigned int *)(g + 0x29c9fa8) + 1;
}

/*
 * Shared scaling formula confirmed real from both call sites in
 * CSTGMidiDispatcher::PerfChangeControllerReset() (sec 10.115): see
 * that method's own header comment for the full confirmed shape.
 */
static void ScaleControllerByteToFloatPair(unsigned char val, float &low, float &centered)
{
	if (val == 0xff) {
		low = 0.0f;
		centered = 0.0f;
		return;
	}
	/* Confirmed real for the realistic MIDI 0-127 domain (movsx on a
	 * value >0x7f would sign-extend differently; not independently
	 * confirmed to matter since controller bytes here are 7-bit). */
	float scaled = (val <= 0x40) ? ((float)(short)val / 128.0f) : (((float)((int)val - 1)) / 126.0f);
	low = scaled;
	centered = scaled + scaled - 1.0f;
}

/*
 * CSTGMidiDispatcher::PerfChangeControllerReset() (sec 10.115): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGMidiDispatcher::PerfChangeControllerReset()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	CSTGSmoother::sInstance->CancelAllCCSmoothers();

	/* Confirmed real: this byte lives in the SAME stack scratch slot
	 * reused for every constructed CSTGControllerValue below, across
	 * both call sites and every loop iteration -- bits 0/1 are always
	 * forced to {1,0} respectively (confirmed), bits 2-7 carry whatever
	 * was there before (a genuine uninitialized-stack-memory quirk on
	 * real hardware; modeled here with a deterministic starting value
	 * of 0 rather than true stack garbage, since bits 2-7 have no
	 * independently confirmed meaning and true stack-garbage modeling
	 * isn't practical across a persistent static). */
	static unsigned char sStaleFieldB;

	for (unsigned int ch = 0; ch < 0x10; ch++) {
		unsigned char *chanBase = mgr + ch * 0x92c;
		unsigned char mVal = chanBase[0x2718];
		unsigned char lVal = chanBase[0x2730];
		unsigned char *channelValuesObj = chanBase + 0x2410;
		unsigned char *valueSlot = chanBase + 0x29b0;

		((CSTGChannelValues *)channelValuesObj)->Reset();

		unsigned char *statusFill = (unsigned char *)STGAPIFrontPanelStatus::sInstance + (ch + 2) * 0x80 + 0xb;
		for (int i = 0; i < 0x78; i++)
			statusFill[i] = 0xff;

		self[ch * 4 + 0x60] = 0;
		self[ch * 4 + 0x61] = 0;
		((CSTGChannelValues *)channelValuesObj)->SetPitchBend(*(CSTGControllerValue *)valueSlot, true);

		CSTGControllerValue cv1;
		float scaled1, centered1;
		ScaleControllerByteToFloatPair(mVal, scaled1, centered1);
		*(float *)&cv1.field0 = scaled1;
		cv1.field4 = centered1;
		cv1.field8 = mVal;
		cv1.fieldA = 1;
		sStaleFieldB = (unsigned char)((sStaleFieldB | 1) & ~2);
		cv1.fieldB = sStaleFieldB;
		((CSTGChannelValues *)channelValuesObj)->SetControllerValue(0x40, cv1);

		((unsigned char *)STGAPIFrontPanelStatus::sInstance)[ch * 0x80 + 0x14b] = mVal;

		CSTGControllerValue cv2;
		float scaled2, centered2;
		ScaleControllerByteToFloatPair(lVal, scaled2, centered2);
		*(float *)&cv2.field0 = scaled2;
		cv2.field4 = centered2;
		cv2.field8 = lVal;
		cv2.fieldA = 1;
		sStaleFieldB = (unsigned char)((sStaleFieldB | 1) & ~2);
		cv2.fieldB = sStaleFieldB;
		((CSTGChannelValues *)channelValuesObj)->SetControllerValue(0x42, cv2);

		((unsigned char *)STGAPIFrontPanelStatus::sInstance)[ch * 0x80 + 0x14d] = lVal;
	}
}

/*
 * SubmitPerfChangeRequest() (sec 10.116): see oa_global.h for the full
 * confirmed shape.
 */
void CSTGGlobal::SubmitPerfChangeRequest(CSTGPerfChangeRequest &request)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *compareSlot = (base[0x2975185] == 0) ? (base + 0x297514c) : (base + 0x2975168);

	if (request.source == 2) {
		request.field14 = *(unsigned int *)(base + 0x29c9fa8);
		request.field18 = *(unsigned int *)(base + 0x29c9fac);

		CSTGPerfChangeRequest *cmp = (CSTGPerfChangeRequest *)compareSlot;
		bool sameTarget = (request.tag == cmp->tag) &&
			(request.tag != 0 || request.mode == cmp->mode) &&
			(request.value1 == cmp->value1) &&
			(request.value2 == cmp->value2);
		if (sameTarget) {
			unsigned long long reqTime = ((unsigned long long)request.field18 << 32) | request.field14;
			unsigned long long cmpTime = ((unsigned long long)cmp->field18 << 32) | cmp->field14;
			if (reqTime - cmpTime <= 0x95)
				return; /* debounce: drop a near-duplicate request */
		}
	}

	if (base[0x2975184] != 0) {
		*(CSTGPerfChangeRequest *)(base + 0x2975168) = request;
		base[0x2975185] = 1;
		return;
	}

	unsigned char *queueWriter = (unsigned char *)CSTGMidiPortManager::sInstance + 0x208;
	unsigned int writable = ((CSTGMidiQueue *)queueWriter)->GetNumWritableBytes();
	if (writable <= 0x4f) {
		*(CSTGPerfChangeRequest *)(base + 0x2975168) = request;
		base[0x2975185] = 1;
		return;
	}

	ProcessPerfChangeRequest(request);
}

/*
 * MulRoundToFloat()/FYL2X() (sec 10.117): small inline-asm helpers
 * backing SetCurrentModeTempo below -- there is no libm available in a
 * kernel build, so log2 is computed via the real x87 `fyl2x`
 * instruction directly, and the tempo/120 intermediate is rounded to
 * float via a real x87 multiply-then-implicit-store (matching the real
 * disassembly's own "compute at extended precision, store as float,
 * reload" sequence) rather than plain double-precision C++ arithmetic,
 * to avoid any double-rounding ambiguity.
 */
static float MulRoundToFloat(float a, double b)
{
	float result;
	__asm__ __volatile__(
		"fldl %2\n\t"
		"fmulp %%st,%%st(1)"
		: "=t" (result)
		: "0" (a), "m" (b)
	);
	return result;
}

static float FYL2X(float x)
{
	float result;
	__asm__ __volatile__(
		"fld1\n\t"
		"fxch %%st(1)\n\t"
		"fyl2x"
		: "=t" (result)
		: "0" (x)
	);
	return result;
}

/*
 * CSTGGlobal::SetCurrentModeTempo() (sec 10.117): see oa_global.h for
 * the full confirmed shape.
 */
void CSTGGlobal::SetCurrentModeTempo(float tempo)
{
	unsigned char *base = (unsigned char *)this;

	/* Confirmed real quirk: tempo<1.0 never computes tempo/120 at all --
	 * it substitutes the fixed ratio as if tempo were exactly 1.0. */
	float ratio = (tempo < 1.0f) ? (1.0f / 120.0f) : MulRoundToFloat(tempo, 1.0 / 120.0);

	float result = FYL2X(ratio);
	/* Confirmed real but unreachable in practice: both of ratio's own
	 * possible sources keep log2(ratio) >= ~-6.9, well above -16.0 --
	 * reproduced faithfully as real defensive code, not removed. */
	if (result < -16.0f)
		result = -16.0f;
	else if (result > 16.0f)
		result = 16.0f;

	*(float *)(base + 0x29c9fa4) = result;
}

/*
 * Shared tail for HandleController's own CC 0x5c/0x5e/0x5f branches
 * (sec 10.118): builds and sends the confirmed real 24-byte
 * PushUnsolicitedMessage shape common to all three (and to CC 0x7a's
 * own tail message below).
 */
static void PushControllerMessage(short fieldAWord, unsigned int msgType, unsigned int lastField)
{
	unsigned char msg[0x18];
	*(unsigned short *)(msg + 0x0) = 0x18;
	*(short *)(msg + 0x2) = fieldAWord;
	*(unsigned int *)(msg + 0x4) = 1;
	*(unsigned int *)(msg + 0x8) = 0;
	*(unsigned int *)(msg + 0xc) = 0;
	*(unsigned int *)(msg + 0x10) = msgType;
	*(unsigned int *)(msg + 0x14) = lastField;
	PushUnsolicitedMessage(msg);
}

/*
 * Shared CC 0x5c/0x5e/0x5f handling (sec 10.118): `fullMask` is the
 * confirmed real 2-bit mask (primary+secondary), `primaryMask` the
 * primary bit alone -- confirmed to EXACTLY match UpdateFXDisable's own
 * already-reconstructed masks for these same 3 CC numbers (sec 10.73:
 * 0x09/0x12/0x24 for CC 0x5c/0x5e/0x5f respectively), a strong
 * independent cross-confirmation of both derivations.
 */
static void HandleControllerFXDisableCC(unsigned char *base, bool cl, bool doSecondary,
					 unsigned int field8, short fieldAWord,
					 unsigned char fullMask, unsigned char primaryMask,
					 unsigned int msgType)
{
	unsigned char flags = base[0x6d4];
	unsigned int value;

	if (!cl) {
		value = (field8 == 0) ? 1 : 0;
		unsigned char mask = doSecondary ? fullMask : primaryMask;
		flags = (unsigned char)((flags & ~mask) | (value ? mask : 0));
	} else {
		/* Confirmed real: ignores field8 entirely, instead copying the
		 * secondary bit's CURRENT value into the primary bit. */
		unsigned char secondaryMask = (unsigned char)(fullMask & ~primaryMask);
		value = (flags & secondaryMask) ? 1 : 0;
		flags = (unsigned char)((flags & ~primaryMask) | (value ? primaryMask : 0));
	}

	base[0x6d4] = flags;
	PushControllerMessage(fieldAWord, msgType, value);
}

/*
 * CSTGGlobal::HandleController() (sec 10.118): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGGlobal::HandleController(unsigned int ccNumber, const CSTGControllerValue &value)
{
	unsigned char *base = (unsigned char *)this;
	int fieldA = (signed char)value.fieldA;
	bool cl = ((value.fieldB & 2) != 0) || (fieldA == 6);
	bool doSecondary = !(fieldA == 3 || fieldA == 4 || fieldA == 5);
	short fieldAWord = (short)(signed char)fieldA;

	if (ccNumber == 0x5c) {
		HandleControllerFXDisableCC(base, cl, doSecondary, value.field8, fieldAWord, 0x09, 0x01, 4);
		return;
	}
	if (ccNumber == 0x5e) {
		HandleControllerFXDisableCC(base, cl, doSecondary, value.field8, fieldAWord, 0x12, 0x02, 5);
		return;
	}
	if (ccNumber == 0x5f) {
		HandleControllerFXDisableCC(base, cl, doSecondary, value.field8, fieldAWord, 0x24, 0x04, 6);
		return;
	}
	if (ccNumber != 0x7a)
		return;

	if (cl)
		return;

	/* Confirmed real SIGNED comparison, unlike the "==0" checks above. */
	unsigned int above3f = ((short)value.field8 > 0x3f) ? 1 : 0;

	if (base[0x6ae] != 0) {
		/* Confirmed real: StealAllVoices() is called ONLY when the
		 * message processor gate is clear -- either way, execution
		 * falls through to the SAME ResetAllControllers() call right
		 * after (not an either/or dispatch). */
		unsigned char *msgProc = (unsigned char *)CSTGMessageProcessor::sInstance;
		if (msgProc[0x48] == 0)
			CSTGVoiceAllocator::sInstance->StealAllVoices();

		unsigned char channel = base[0x6b9];
		CSTGMidiDispatcher::sInstance->ResetAllControllers(channel, false);

		base[0x6af] = (unsigned char)above3f;

		unsigned char globalChannel = base[0x6b8];
		unsigned char msg[5];
		msg[0] = (unsigned char)(globalChannel | 0xb0);
		msg[1] = 0x79;
		msg[2] = (unsigned char)(6 - above3f);
		msg[3] = 5;
		msg[4] = 0xff;
		unsigned char *queueWriter = (unsigned char *)CSTGMidiPortManager::sInstance + 0x208;
		((CSTGMidiQueueWriter *)queueWriter)->Write(msg, 5, false);

		/* Confirmed real: unconditional on this sub-path, unrelated to
		 * the message-processor gate above. */
		CSTGControllerRTData::sInstance->ResetAllJumpCatch();
	} else {
		base[0x6af] = (unsigned char)above3f;
	}

	PushControllerMessage(fieldAWord, 0x15, above3f);
}

/*
 * CSTGChannelValues::SetPitchBend() (sec 10.128): see oa_engine_init.h
 * for the full confirmed shape.
 */
void CSTGChannelValues::SetPitchBend(const CSTGControllerValue &value, bool flag)
{
	unsigned char *self = (unsigned char *)this;
	const unsigned char *src = (const unsigned char *)&value;
	*(unsigned int *)(self + 0x5a0) = *(const unsigned int *)(src + 0);
	*(unsigned int *)(self + 0x5a4) = *(const unsigned int *)(src + 4);
	*(unsigned int *)(self + 0x5a8) = *(const unsigned int *)(src + 8);
	if (flag)
		*(unsigned int *)(self + 0x634) = *(const unsigned int *)(src + 0);
}

/*
 * CSTGChannelValues::Initialize() (.text+0x26a50, 75 bytes, sec 10.151)
 * confirmed: lazily runs InitializeLongHand() on a hidden static
 * template object EXACTLY ONCE process-wide (guarded by
 * `sTemplateReady`), then unconditionally copies the resulting 0x92c-byte
 * `sTemplate` over `this` on every call (including the first) -- the
 * real body's own `rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]` with
 * `ecx=0x24b` (587 dwords == 0x92c bytes), reproduced here as an
 * explicit loop (no <cstring>/memcpy -- this project's own established
 * convention, sec 10.56, avoids it on the freestanding -m32 Kbuild
 * target). InitializeLongHand() itself (.text+0x26820, 550 bytes) is a
 * confirmed real, deliberately deferred dependency -- see bar2_stubs.cpp.
 */
void CSTGChannelValues::Initialize()
{
	if (!CSTGChannelValues::sTemplateReady) {
		((CSTGChannelValues *)CSTGChannelValues::sTemplate)->InitializeLongHand();
		CSTGChannelValues::sTemplateReady = 1;
	}

	unsigned char *dst = (unsigned char *)this;
	const unsigned char *src = CSTGChannelValues::sTemplate;
	for (unsigned int i = 0; i < sizeof(CSTGChannelValues::sTemplate); i++)
		dst[i] = src[i];
}

/*
 * CSTGControllerRTData::ResetAllJumpCatch() (sec 10.129): see
 * oa_global.h for the full confirmed shape.
 */
void CSTGControllerRTData::ResetAllJumpCatch()
{
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	if (mgr[0x23d1] != 2)
		return;
	ResetKnobsJumpCatch();
	ResetSlidersJumpCatch();
	ResetRTKKnobSmoothers();
}

/*
 * CSTGSmoother::CancelAllCCSmoothers() (sec 10.130): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGSmoother::CancelAllCCSmoothers()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *node = *(unsigned char **)(self + 0xf010);
	while (node) {
		unsigned char *next = *(unsigned char **)(node + 0);
		unsigned char *mapping = *(unsigned char **)(node + 8);
		unsigned int typeVal = *(unsigned int *)(mapping + 0x10);
		if (typeVal == 2 || typeVal == 8)
			FinalizeSmoother(node, false);
		node = next;
	}
}

/*
 * CSTGSlotVoiceData::EmergencyFreeAllVoices() (sec 10.138): see
 * oa_global.h for the full confirmed shape.
 */
void CSTGSlotVoiceData::EmergencyFreeAllVoices()
{
	unsigned char *self = (unsigned char *)this;
	CSTGVoiceAllocator::sInstance->EmergencyFreeVoiceList(self + 0x44);
	CSTGVoiceAllocator::sInstance->EmergencyFreeVoiceList(self + 0x50);
}

/*
 * CSTGSlotVoiceData::Steal() (sec 10.140): see oa_global.h for the
 * full confirmed shape.
 */
void CSTGSlotVoiceData::Steal()
{
	unsigned char *self = (unsigned char *)this;
	self[0x42] = 1;
	self[0x41] = 1;
	CSTGVoiceAllocator::sInstance->StealVoiceList(self + 0x44);
	CSTGVoiceAllocator::sInstance->StealVoiceList(self + 0x50);
}

/*
 * CSTGSlotVoiceData::Initialize(unsigned short) (sec 10.150): see
 * oa_global.h for the full confirmed shape (quad/lane decomposition,
 * LFO/step-seq sub-rate-parameter pointer computation).
 */
void CSTGSlotVoiceData::Initialize(unsigned short slotIndex)
{
	unsigned char *self = (unsigned char *)this;
	*(unsigned short *)self = slotIndex;

	unsigned int quadIndex = (unsigned int)(unsigned short)slotIndex >> 2;
	unsigned int subIndex = slotIndex & 3;

	unsigned char *lfoPtr = (unsigned char *)CSTGCommonLFO::sSubRateParams
				 + quadIndex * 0x250 + subIndex * 4;
	unsigned char *stepSeqPtr = (unsigned char *)CSTGCommonStepSeq::sSubRateParams
				     + quadIndex * 0x100 + subIndex * 4;

	/* Packed 32-bit fields (confirmed real `mov %ecx,0x1480(%eax)`
	 * dword stores, 4 bytes apart), NOT native pointer writes -- a
	 * native 8-byte write here on this 64-bit host would stomp the
	 * first 4 bytes of the ADJACENT +0x1484 field with its own upper
	 * 32 bits, corrupting both fields; caught via a real test failure
	 * (own dedicated [15] Initialize scenario in test_global.cpp)
	 * before landing on this fix, not by re-reading the disassembly a
	 * second time. Same class of hazard as CSTGAudioInputMixerBase's
	 * own fields, sec 10.150. */
	*(unsigned int *)(self + 0x1480) = ToU32(lfoPtr);
	*(unsigned int *)(self + 0x1484) = ToU32(stepSeqPtr);

	((CSTGChannelValues *)(self + 0x1488))->Initialize();
}
