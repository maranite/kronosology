// SPDX-License-Identifier: GPL-2.0
/*
 * controller_info_send_unsolicited_ui_param.cpp  -  CSTGControllerInfo::
 * SendUnsolicitedUIParam(unsigned int, unsigned int, long, int) (batch
 * 60, ground truth `.text+0x945d0`, 516 bytes -- mangled
 * `_ZN18CSTGControllerInfo22SendUnsolicitedUIParamEjjl14eSTGMidiSource`).
 *
 * Deliberately a SEPARATE translation unit from global.cpp (matching the
 * `CSTGControllerRTData::SetAudioInSolo`/`ResetSendKnobsJumpCatch`
 * precedent, batch 57): test_engine.cpp/test_global.cpp/
 * test_global_ctor.cpp all carry their own pre-existing call-counting
 * mocks for this exact symbol, load-bearing for
 * OnExtModePlayMuteSwitchAssignChange/OnExtModeSelectSwitchAssignChange
 * assertions in global.cpp's own tests -- left untouched.
 *
 * Confirmed real STATIC (no implicit `this`) function, regparm(3):
 * eax=paramId, edx=value, ecx=arg3, midiSource on the stack -- matches
 * oa_global.h's own already-established declaration exactly.
 *
 * Real body is a genuine 4-way message-building dispatch, all four paths
 * converging on the already-real `PushUnsolicitedMessage()` (sec 10.90):
 *
 *   `CSTGGlobal::sInstance->fieldAt(0x29cc4dc)` gates two ENTIRELY
 *   different code shapes:
 *
 *   NONZERO ("structured"/performance-relative path): resolves a
 *   pointer via a `CSTGGlobal::sInstance`-relative formula that is
 *   CLOSE TO but NOT IDENTICAL to the already-real `ResolveCurrentPerformance()`
 *   (global.cpp, sec 10.77) -- confirmed via careful side-by-side
 *   comparison: this function's own mode dispatch collapses modes 0 AND
 *   1 onto `ResolveCurrentPerformance()`'s own mode-1 formula (`+0x69c`/
 *   `+0x690`/`0x1c77f10`/`+6`), only mode 2 diverges onto its own
 *   `+0x6a0`/`0x1cad`/`0x27cd024` formula (mode 2 matches
 *   `ResolveCurrentPerformance()`'s own mode-2 branch exactly) -- i.e.
 *   ground truth's own mode-0 branch (`+0x698`/`+0x68c`/`0x132e4d0`) is
 *   simply never reached by THIS function. Confirmed by direct
 *   instruction-level trace, not assumed from the matching constants --
 *   calling the shared helper here would silently reintroduce ground
 *   truth's own mode-0 behavior this function does NOT have, so the
 *   formula is reproduced inline instead.
 *
 *   The resolved pointer, plus `fieldAt(0x29cc4e0)*0xe8` (a genuine
 *   `CSTGProgramSlot`-stride index -- 0xe8 matches this project's own
 *   already-confirmed `CSTGProgramSlot` embedding stride, sec 10.153/
 *   batch 45), locates a not-independently-typed sub-object at `+0xb60`/
 *   `+0xb63`. Two RAW vtable dispatches follow (`call *0x58(vtbl)`/
 *   `call *0x5c(vtbl)`, i.e. slots 22/23), matching this project's
 *   already-established "raw vtable-slot call for a not-fully-typed
 *   object" idiom (`CSTGEffectRackVars::UpdateDModRoutings`, oa_global.h)
 *   -- both results feed the outgoing message struct.
 *
 *   ZERO path: three-way sub-dispatch on the SAME `+0x684` mode field
 *   (0, 1, or 2 -- values outside `[0,2]` silently drop the message, a
 *   real confirmed early-return, not an oversight), each building the
 *   message from different `CSTGGlobal::sInstance` fields directly (no
 *   pointer resolution at all).
 *
 * Outgoing message layout (relative to its own base, all four paths
 * agree on the shared prefix -- matches `PushUnsolicitedMessage()`'s own
 * already-confirmed `size`/`source` header fields exactly):
 *   +0x00 u16    size tag (0x20 -- structured & zero-path-mode0; 0x24 --
 *                zero-path-mode1/2)
 *   +0x02 u16    midiSource (PushUnsolicitedMessage's own `source` field)
 *   +0x04 u32    constant A (4 -- structured; 2 -- zero-path)
 *   +0x08 u32    constant B (5 -- structured; 2 -- zero-path)
 *   +0x0c u32    path-specific field 1 (vtable-call-1 result / fieldAt(
 *                0x688) / fieldAt(0x690) / 0, one per path)
 *   +0x10 u32    path-specific field 2 (vtable-call-2 result, byte
 *                zero-extended / fieldAt(0x694) / fieldAt(0x69c) /
 *                fieldAt(0x6a0), one per path)
 *   +0x14 u32    paramId
 *   +0x18 u32    value
 *   +0x1c u32    arg3
 *   +0x20 u32    [zero-path mode1/2 ONLY] 0 (mode1) or 2 (mode2)
 */

#include "oa_global.h"

extern "C" void PushUnsolicitedMessage(void *msg);

namespace {
struct Msg {
	unsigned short size;
	unsigned short source;
	unsigned int a;
	unsigned int b;
	unsigned int field1;
	unsigned int field2;
	unsigned int paramId;
	unsigned int value;
	unsigned int arg3;
	unsigned int extra; /* only used/sent for the 0x24-tagged variant */
};
} /* anonymous namespace */

void CSTGControllerInfo::SendUnsolicitedUIParam(unsigned int paramId, unsigned int value,
						  long arg3, int midiSource)
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	Msg msg;
	msg.source = (unsigned short)midiSource;
	msg.paramId = paramId;
	msg.value = value;
	msg.arg3 = (unsigned int)arg3;

	if (*(unsigned int *)(g + 0x29cc4dc) != 0) {
		int mode = *(int *)(g + 0x684);
		unsigned char *perf;
		if (mode == 2) {
			int seqIdx = *(int *)(g + 0x6a0);
			perf = g + seqIdx * 0x1cad + 0x27cd024;
		} else {
			int idx = (*(int *)(g + 0x69c)) & 0x7f;
			int bank = *(int *)(g + 0x690);
			perf = g + idx * 0x19e7 + bank * 0xcf381 + 0x1c77f10 + 6;
		}

		unsigned int slotIdx = *(unsigned int *)(g + 0x29cc4e0);
		unsigned char *slotBase = perf + slotIdx * 0xe8;
		unsigned char *objA = slotBase + 0xb63; /* passed as `this`/eax to both calls */
		unsigned char *objB = slotBase + 0xb60; /* holds the vtable ptr at +3 (unaligned) */

		typedef int (*Fn1)(void *);
		typedef unsigned char (*Fn2)(void *);
		void *vtbl = *(void **)(objB + 3);
		int result1 = (*(Fn1 *)((unsigned char *)vtbl + 0x58))(objA);
		unsigned char result2 = (*(Fn2 *)((unsigned char *)vtbl + 0x5c))(objA);

		msg.size = 0x20;
		msg.a = 4;
		msg.b = 5;
		msg.field1 = (unsigned int)result1;
		msg.field2 = result2;
		PushUnsolicitedMessage(&msg.size);
		return;
	}

	int mode = *(int *)(g + 0x684);
	if (mode == 0) {
		msg.size = 0x20;
		msg.a = 4;
		msg.b = 5;
		msg.field1 = *(unsigned int *)(g + 0x688);
		msg.field2 = *(unsigned int *)(g + 0x694);
		PushUnsolicitedMessage(&msg.size);
		return;
	}
	if (mode < 0 || mode > 2)
		return;

	msg.size = 0x24;
	msg.a = 2;
	msg.b = 2;
	if (mode == 2) {
		msg.field1 = 0;
		msg.field2 = *(unsigned int *)(g + 0x6a0);
		msg.extra = 2;
	} else {
		msg.field1 = *(unsigned int *)(g + 0x690);
		msg.field2 = *(unsigned int *)(g + 0x69c);
		msg.extra = 0;
	}
	PushUnsolicitedMessage(&msg.size);
}
