// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_set_assignment.cpp  -  CSTGControllerRTData::
 * SetControllerAssignment(TPackedEnum<eControllerAssign>&,
 * eControllerAssign, bool) (batch 16, sec 10.163, `.text+0x1c800`, 322
 * bytes).
 *
 * Deliberately a separate translation unit (matching the
 * WriteSTGMidiOutQueue/CSTGStreamingEventManager/SetControllerValue
 * precedent, sec 10.145/10.158/10.162): `verify/test_engine.cpp` and
 * `verify/test_global_ctor.cpp` both carry pre-existing trivial no-op
 * mocks of this exact symbol, and `verify/test_global.cpp` carries a
 * LOAD-BEARING call-recording mock (`g_lastSetControllerAssignmentThis`
 * etc, used by its own "table[N] == 0xNN" / "exactly 3 calls reached
 * SetControllerAssignment" assertions) -- none of those three link this
 * new file, so their mocks are left completely untouched. This real
 * body is instead exercised by its own dedicated
 * `verify/test_controller_rt_data_set_assignment.cpp`.
 *
 * Confirmed regparm(3): this=EAX (a `CSTGControllerRTData*`, used only
 * for the final `+0xf` clear and the curVal==0x13 float read at
 * `+0x4c`), target=EDX (the `TPackedEnum<eControllerAssign>&`
 * out-param, i.e. `selfRef`), value=ECX (`newValue`, an
 * `eControllerAssign` truncated to a signed byte on write), flag=stack
 * (`notify`, the 4th/last real bool param -- passed straight through to
 * `HandleControllerChange`'s own 4th bool arg).
 *
 * Full confirmed shape: reads `curVal = *target` (sign-extended byte,
 * the OLD assignment value) and classifies it into exactly one of three
 * disjoint ranges (a 4th "none of the above" case does nothing extra);
 * every path then falls into one shared "commit" tail:
 *
 *   curVal in [1, 0x12]  (knob/slider CC-assignable range):
 *     ccId = kControllerCCIdTable[curVal-1]
 *     channel = CSTGGlobal::sInstance[0x6b9]  (the confirmed real
 *       "current channel index" byte used throughout this project,
 *       e.g. CSTGChannelValues::SetControllerValue's own caller-side
 *       lookup, sec 10.162)
 *     candidate = CSTGCCInfo::sCCInfoTable[ccId*10 + 0]  ("b0", the
 *       per-CC default/current value, sec 10.161)
 *     if (ccId <= 0x77) {   // always true for the real table's own 18
 *                           // confirmed values (max 0x5d) -- the
 *                           // "else" arm below is real, confirmed
 *                           // defensive code, but UNREACHABLE given
 *                           // the current kControllerCCIdTable data
 *       liveVal = <active performance's CSTGChannelValues for `channel`>
 *                 .rawArray[ccId].field8 (low byte) -- confirmed real
 *                 address `mgr + channel*0x92c + 0x2410 + ccId*12 + 8`,
 *                 where `mgr` is resolved via the SAME inlined
 *                 `CSTGPerformanceVarsManager::sInstance[8]`-selector
 *                 idiom as `ResolveActivePerformanceVarsManagerRaw()`
 *                 (global.cpp) -- confirmed genuinely INLINED here, not
 *                 a call to that shared helper (no `call` instruction
 *                 at this site), and `mgr+0x2410` independently matches
 *                 `CSTGMidiDispatcher::PerfChangeControllerReset()`'s
 *                 own already-confirmed `channelValuesObj = chanBase +
 *                 0x2410` (global.cpp) -- `field8` is `rawArray[cc]`'s
 *                 own 3rd dword (the `unsigned short` `SetControllerValue`
 *                 already confirmed mirrors verbatim from its own input,
 *                 sec 10.162), read here as its low byte.
 *       if (liveVal != 0xff && candidate != liveVal) notify.
 *     } else {
 *       if (candidate != 0) notify.
 *     }
 *
 *   curVal in [0x2e, 0x3d]  (a fixed "always notify with 0" range):
 *     candidate = 0; ALWAYS notifies (curVal can never be 0x13 in this
 *     range, so the shared curVal==0x13 special-case below never
 *     triggers here) -- a genuine confirmed quirk: this range never
 *     actually skips the `HandleControllerChange` call, unlike the
 *     other two ranges which both have real skip conditions.
 *
 *   curVal in [0x13, 0x41]  (table-driven range,
 *       `kSetControllerAssignmentNotifyTable[curVal-0x13]`):
 *     if (tableVal == -1) -- no notify at all, straight to commit.
 *     else if (curVal == 0x13) {
 *       // confirmed real quirk: compares the table's constant (127)
 *       // against a FRESHLY COMPUTED value, but if they differ,
 *       // notifies with the TABLE's constant anyway, not the computed
 *       // one.
 *       computed = (int)(127.0f * this->fieldAt(0x4c))  -- via a real
 *         x87 `fld dword [.rodata.cst4+0x1c]=127.0f; fmul dword
 *         [this+0x4c]; fistp dword [...]` sequence (round-to-nearest,
 *         the FPU's default control word -- NOT a plain truncating C
 *         cast), reproduced with a small inline-asm helper matching
 *         this project's own `MulRoundToFloat`/`SCVFMul` convention
 *         (memory operands only, sec 10.150/batch15).
 *       notify = (tableVal != computed); candidate = tableVal.
 *     } else {
 *       notify = true (unconditional -- tableVal is already known != -1
 *         at this point); candidate = tableVal.
 *     }
 *
 *   otherwise: no notify, straight to commit.
 *
 *   commit (always runs): `*target = newValue`; calls
 *   `CSTGSlotVoiceData::UpdateAllActiveMIDIFilters()` unconditionally;
 *   if `newValue == 0xe`, clears `this->fieldAt(0xf)` to 0 (a confirmed
 *   real special case, not independently named).
 *
 *   `if (notify) HandleControllerChange((int)curVal, (unsigned char)
 *   candidate, false, notify_param)` -- `HandleControllerChange`'s own
 *   3rd bool arg is confirmed always `false` here (a stack-pushed
 *   literal 0, distinct from this function's OWN 3rd param).
 */

#include "oa_global.h"

/*
 * MulRoundToInt() (sec 10.163): matches the real `fld dword K; fmul
 * dword Y; fistp dword result` sequence exactly, using only memory
 * operands (this project's own established convention for a
 * self-contained, non-chained x87 primitive, sec 10.150/batch15 --
 * `flds`/`fmuls`/`fistpl` rather than register-tied constraints).
 */
static inline int MulRoundToInt(float k, float y)
{
	int result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"fmuls %2\n\t"
		"fistpl %0"
		: "=m" (result)
		: "m" (k), "m" (y)
	);
	return result;
}

/* kControllerCCIdTable declared in oa_global.h; defined here alongside
 * its one real consumer. */
const unsigned char CSTGControllerRTData::kControllerCCIdTable[18] = {
	0x01, 0x02, 0x04, 0x05, 0x07, 0x08, 0x0a, 0x0b, 0x0c, 0x0d, 0x10, 0x12,
	0x41, 0x42, 0x43, 0x52, 0x5b, 0x5d,
};

/*
 * kSetControllerAssignmentNotifyTable (sec 10.163, `.rodata+0x360`, GCC
 * internal name `CSWTCH.306`, 47 signed 32-bit entries) -- confirmed
 * real, dumped directly from the binary (`readelf -x .rodata`), indexed
 * by `curVal - 0x13` for curVal in [0x13, 0x41]. -1 = "no notification
 * at all"; 0 or 127 are the only other confirmed values.
 */
static const int kSetControllerAssignmentNotifyTable[47] = {
	/* idx  0     */ 127,
	/* idx  1- 8  */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* idx  9-16  */ -1, -1, -1, -1, -1, -1, -1, -1,
	/* idx 17-18  */ 0, 0,
	/* idx 19-42  */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
			 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* idx 43-44  */ 0, 0,
	/* idx 45     */ 127,
	/* idx 46     */ 0,
};

void CSTGControllerRTData::SetControllerAssignment(void *selfRef, signed char newValue, bool notify)
{
	unsigned char *thisBytes = (unsigned char *)this;
	signed char *target = (signed char *)selfRef;
	int curVal = *target;	/* sign-extended, matches the real `movsx` */

	bool doNotify = false;
	int candidate = 0;

	if ((unsigned int)(curVal - 1) <= 0x11u) {
		/* --- knob/slider CC-assignable range (curVal 1..0x12) --- */
		unsigned char ccId = kControllerCCIdTable[curVal - 1];
		unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
		unsigned char channel = g[0x6b9];
		unsigned char entryB0 = CSTGCCInfo::sCCInfoTable[(unsigned int)ccId * 10];

		candidate = entryB0;

		if (ccId <= 0x77) {
			/* Inlined ResolveActivePerformanceVarsManagerRaw()
			 * idiom -- confirmed no `call` at this site. */
			unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
			unsigned char curSlot = slots[8];
			unsigned int mgrPacked = *(unsigned int *)(slots + (unsigned int)curSlot * 4);
			unsigned char *mgr = (unsigned char *)(unsigned long)mgrPacked;

			unsigned char *channelValuesObj = mgr + (unsigned int)channel * 0x92cu + 0x2410;
			unsigned char liveVal = channelValuesObj[(unsigned int)ccId * 12 + 8];

			if (liveVal != 0xff && entryB0 != liveVal)
				doNotify = true;
		} else {
			/* Confirmed real defensive code, unreachable given
			 * kControllerCCIdTable's own real values (max 0x5d). */
			if (entryB0 != 0)
				doNotify = true;
		}
	} else if ((unsigned int)(curVal - 0x2e) <= 0xfu) {
		/* --- fixed "always notify with 0" range (curVal 0x2e..0x3d) --- */
		candidate = 0;
		doNotify = true;
	} else if ((unsigned int)(curVal - 0x13) <= 0x2eu) {
		/* --- table-driven range (curVal 0x13..0x41) --- */
		int tableVal = kSetControllerAssignmentNotifyTable[curVal - 0x13];
		if (tableVal != -1) {
			if (curVal == 0x13) {
				int computed = MulRoundToInt(127.0f, *(float *)(thisBytes + 0x4c));
				candidate = tableVal;
				doNotify = (tableVal != computed);
			} else {
				candidate = tableVal;
				doNotify = true;
			}
		}
	}
	/* else: curVal outside all three ranges -- no notify. */

	if (doNotify)
		HandleControllerChange(curVal, (unsigned char)candidate, false, notify);

	*target = newValue;
	CSTGSlotVoiceData::UpdateAllActiveMIDIFilters();
	if ((unsigned char)newValue == 0xe)
		thisBytes[0xf] = 0;
}
