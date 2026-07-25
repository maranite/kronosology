// SPDX-License-Identifier: GPL-2.0
/*
 * controller_info_button_handler.cpp  -  CSTGControllerInfo::
 * ButtonPressHandler(eSTGButtonCode, bool) (batch 66, ground truth
 * `.text+0x95fa0`, 5822 bytes, mangled `_ZN18CSTGControllerInfo18
 * ButtonPressHandlerE14eSTGButtonCodeb`).
 *
 * This is the real physical front-panel BUTTON press/release dispatch
 * entry point -- `CSTGFrontPanel::HandleSwitchEvent` (front_panel_
 * handlers.cpp) tail-calls it directly on the "target" `CSTGControllerInfo`
 * sub-object `ResolveControllerInfoTarget` selects, the same idiom
 * `AnalogControllerHandler` (controller_info_analog_handler.cpp, batch 65)
 * uses for knobs/sliders/etc. Ground-truthed via `objdump -dr -M intel`
 * against the real OA.ko plus a `readelf -rW`/raw-byte dump of all THREE
 * `.rodata` dispatch tables.
 *
 * ---------------------------------------------------------------------
 * CORRECTION to the batch-65 characterization: that pass's ~15-entry
 * spot-check found only TWO `.rodata` tables and estimated "~144 entries".
 * A full dump this pass found a THIRD table (release-side counterpart of
 * the first), for a real total of 70+74+70 = 214 dispatch slots (most of
 * which are compiler padding -- see below).
 *
 * TOP-LEVEL STRUCTURE (confirmed via the real entry sequence):
 *
 *   1. Edit-in-context gate: if `CSTGGlobal::sInstance+0x29cc4dc != 0`,
 *      call `HandleEditInContextButton(code, pressed)`; if it returns
 *      true, return immediately; if false, fall through to normal
 *      dispatch anyway (same "gate, not full intercept" idiom
 *      `AnalogControllerHandler` already established for
 *      `HandleEditInContextKnob`/`Slider`).
 *
 *   2. `if (code <= 0x49)`: dispatch via TABLE 2 (`.rodata+0x47bf8`, 74
 *      entries, index = `code` directly). This table covers EVERY call
 *      for `code` 0-0x49 REGARDLESS of `pressed` -- most entries are a
 *      single code block executed on both press and release (computing
 *      `pressed ? 0x7f : 0` inline). A handful of entries (0, 9, 0x2c,
 *      0x35-0x39) instead redirect into the SAME "high code" logic step
 *      3/4 below uses (a compiler quirk: these low codes share the
 *      dispatch machinery of the 12 "special" buttons even though they
 *      are numerically <= 0x49).
 *
 *   3. `if (code > 0x49 && pressed)`: dispatch via TABLE 1
 *      (`.rodata+0x47ae0`, 70 entries, index = `code - 9`), gated by
 *      `pressed == true`. Only 12 of the 70 slots are non-padding
 *      (default/padding = the function's plain epilogue, `.text+0x96009`
 *      -- i.e. most computed indices in this table are dead, since the
 *      `code > 0x49` guard already excludes them at the call site).
 *
 *   4. `if (code > 0x49 && !pressed)`: dispatch via TABLE 3
 *      (`.rodata+0x47d20`, 70 entries, SAME index scheme as table 1),
 *      gated by `pressed == false`. Default/padding = `.text+0x9604e`,
 *      the entry point of the SAME shared "clear active-bit" tail table
 *      2's simple cases jump to on release -- i.e. for these 12 buttons,
 *      MOST of the release-time slots just do the ordinary generic
 *      bit-clear (only 12 real distinct release actions exist).
 *
 * `code == 0` is unreachable in tables 1/3 either way: index = 0-9
 * underflows to a huge unsigned value, always out of the `<= 0x45` range
 * check, always falling to the plain epilogue -- confirmed real no-op.
 *
 * ---------------------------------------------------------------------
 * SHARED STATE: `CSTGControllerRTData::sInstance` byte `+0x2f` is a
 * small flag byte (`&2` = "busy2"/jump-catch-style busy flag, `&8` =
 * "busy"/UI-edit-mode flag -- SAME bit AnalogControllerHandler's own
 * `[0x2f]&8` gate tests), `+0x2b` a signed "controller mode" byte
 * (SAME field AnalogControllerHandler dispatches knob/slider assignment
 * modes from), `+0x21` an unnamed solo-related byte, and `+0x30` a
 * per-button "active" BITMAP (one bit per `eSTGButtonCode` value,
 * `dword[code>>5] & (1<<(code&0x1f))`) -- confirmed real via the shared
 * tail's own bit-clear-on-release code and the two symmetric explicit
 * bit-set/bit-test instructions in tables 1/3.
 *
 * TABLE 2 "PATTERN A" (~40 codes, single block, both press+release):
 *   `CSTGControllerRTData::sInstance->SendUnsolControl2MessageToUI(
 *   msgType, buttonId, pressed ? 0x7f : 0, 1)`, then if `!pressed`,
 *   clear bit `code` in the `+0x30` bitmap. Confirmed for codes 1-8,
 *   10-23, 25-34, 36, 37, 45, 46, 51, 52 (real `(msgType, buttonId)`
 *   pairs enumerated in the switch below, each individually
 *   disassembly-confirmed, not guessed from a formula).
 *
 * TABLE 2 "PATTERN B" (10 codes: 38-43, 47-50) -- own local press/busy
 *   gating instead of relying on the generic tail:
 *     if (!pressed) goto DEFERRED_RELEASE;   // per-code, own action
 *     if (!(flags(0x2f) & 8)) goto DEFERRED_BUSY_CLEAR;  // per-code
 *     set bit `code` in the +0x30 bitmap;
 *     SendUnsolControl2MessageToUI(msgType, buttonId, 0x7f, 1);
 *   The gated/inline part AND both per-code "deferred" branches (20
 *   addresses total) are now FULLY RECONSTRUCTED (follow-up pass) -- see
 *   `HandleButtonPatternBDeferred()`'s own comment for the complete
 *   per-address derivation (two genuinely different real shapes across
 *   the 10 codes, confirmed via disassembly, not assumed uniform).
 *
 * TABLE 2 "MIXER SWITCH" (16 codes, 0x3a-0x49): unconditionally
 *   `this->ProcessMixerSwitchPress(code, pressed)` (a real, distinct
 *   ~0x2xx-byte sibling function, own body not reconstructed -- see
 *   oa_global.h), then the SAME generic tail (clear bit on release).
 *
 * TABLES 1+3 "SPECIAL BUTTONS" (12 codes: 9, 0x2c, 0x35-0x39, 0x4a-0x4e)
 *   -- each has its own genuinely distinct press-time (table 1) and
 *   release-time (table 3) body, individually reconstructed below in
 *   `HandleButtonPressSpecial()`/`HandleButtonReleaseSpecial()`. The 17
 *   "else" sub-branches gated on the busy/busy2 flags (named,
 *   address-documented `ButtonDeferred_0xNNNNN()` functions) are now ALL
 *   FULLY RECONSTRUCTED too (follow-up pass) -- three of them (codes
 *   0x35/0x36/0x39) turned out to cascade into a previously-undiscovered
 *   `ChangeControlSurfaceMode(int)` UI-mode state machine spanning
 *   several more real addresses each; see each function's own comment
 *   for the complete per-address trail.
 *
 * Two of the 12 (buttonCode 0x4c "SetSoloSelected" and 0x4a "SetMixer-
 * KnobMode") call the WEAK-UNDEFINED 2-arg `NotifyParam(unsigned int,
 * long)` overload after their real action -- confirmed via mangled-name
 * relocation (`...NotifyParamEjl`), DISTINCT from the real, strongly
 * defined 4-arg overload used elsewhere in this project. Reproduced here
 * `__attribute__((weak))`, no definition -- same "resolves to a
 * null-pointer call if ever reached" idiom `AnalogControllerHandler`'s
 * T18/T916/A18/A916 slots already established. See HARDWARE_REVIEW_LOG.md.
 */

#include "oa_setup_global_resources.h"

/* ---------------------------------------------------------------------
 * Per-button "active" bitmap helpers (CSTGControllerRTData::sInstance+0x30,
 * one bit per eSTGButtonCode value). Confirmed real: table 2's shared
 * tail (`.text+0x9604e`) clears the bit on release for Pattern-A/mixer-
 * switch buttons; Pattern B and the 12 special buttons set/test it
 * explicitly inline.
 */
static inline void ClearButtonActiveBit(unsigned char *rtdBytes, unsigned int code)
{
	unsigned int *bitmap = (unsigned int *)(rtdBytes + 0x30);
	bitmap[code >> 5] &= ~(1u << (code & 0x1f));
}

static inline void SetButtonActiveBit(unsigned char *rtdBytes, unsigned int code)
{
	unsigned int *bitmap = (unsigned int *)(rtdBytes + 0x30);
	bitmap[code >> 5] |= (1u << (code & 0x1f));
}

static inline bool TestButtonActiveBit(unsigned char *rtdBytes, unsigned int code)
{
	unsigned int *bitmap = (unsigned int *)(rtdBytes + 0x30);
	return (bitmap[code >> 5] & (1u << (code & 0x1f))) != 0;
}

/* Table 2 "Pattern A" -- see this file's own header comment. Used
 * identically for both press and release (real ground truth: ONE code
 * block, no branch on `pressed` beyond the 0/0x7f value). */
static void SendButtonPatternA(unsigned int code, bool pressed, int msgType, int buttonId)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	rtd->SendUnsolControl2MessageToUI(msgType, buttonId, pressed ? 0x7f : 0, 1);
	if (!pressed)
		ClearButtonActiveBit((unsigned char *)rtd, code);
}

/* ---------------------------------------------------------------------
 * Table 2 "Pattern B" -- both deferred branches, NOW FULLY RECONSTRUCTED
 * (follow-up pass, ground-truthed via `objdump -dr -M intel` on every one
 * of the 20 real addresses below -- not a guess/pattern extrapolation).
 * Real ground-truth addresses for the 10 codes' two branches each:
 *
 *   code  msgType/id   !pressed-branch   busy-flag-clear-branch  ccNo  shape
 *   0x26 (38) 11/5      .text+0x96f06     .text+0x97361            0x2d   1
 *   0x27 (39) 11/4      .text+0x96eca     .text+0x97348            0x26   1
 *   0x28 (40) 11/3      .text+0x97180     .text+0x972d8            0x25   1
 *   0x29 (41) 11/2      .text+0x96e8e     .text+0x9732f            0x2c   1
 *   0x2a (42) 11/1      .text+0x9725b     .text+0x97297            0x2b   1
 *   0x2b (43) 11/0      .text+0x96e52     .text+0x97316            0x2a   1
 *   0x2f (47) 3/0       .text+0x971bc     .text+0x972f1            3      2
 *   0x30 (48) 2/0       .text+0x96e20     .text+0x97305            0      2
 *   0x31 (49) 2/1       .text+0x971f1     .text+0x972b0            1      2
 *   0x32 (50) 2/3       .text+0x97226     .text+0x972c4            0x32   2
 *
 * Two genuinely DIFFERENT real shapes exist (confirmed per-code via
 * disassembly, not assumed uniform -- codes 0x26-0x2b share one shape,
 * 0x2f/0x30/0x31/0x32 share the other):
 *
 *   Shape 1 (0x26-0x2b): on release, `SendUnsolControl2MessageToUI(
 *     msgType, id, 0, 1)` is ALWAYS sent; `SendKarmaCCToKG(ccNo, 0)` is
 *     ADDITIONALLY sent first, but only if the button's active bit was
 *     NOT already set. On press-but-not-busy (the "busy-flag-clear"
 *     branch), BOTH `SendKarmaCCToKG(ccNo, 0x7f)` and
 *     `SendUnsolControl2MessageToUI(msgType, id, 0x7f, 1)` are sent
 *     unconditionally, but the active bit is deliberately NOT set (only
 *     the normal busy-flag-SET press path sets it).
 *
 *   Shape 2 (0x2f/0x30/0x31/0x32): on release, the two sends are
 *     MUTUALLY EXCLUSIVE -- `SendUnsolControl2MessageToUI(msgType, id,
 *     0, 1)` only if the active bit WAS set, `SendKarmaCCToKG(ccNo, 0)`
 *     only if it was NOT. On press-but-not-busy, ONLY
 *     `SendKarmaCCToKG(ccNo, 0x7f)` is sent -- no UI message at all.
 *
 * Both shapes always clear the active bit on the release path (matching
 * every other release path in this file).
 */
static void HandleButtonPatternBDeferred(unsigned int code, bool pressed, bool busyClearBranch,
					  int msgType, int buttonId)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	(void)pressed;

	static const struct { unsigned char code; unsigned char shape; int ccNo; } kTable[10] = {
		{ 0x26, 1, 0x2d }, { 0x27, 1, 0x26 }, { 0x28, 1, 0x25 },
		{ 0x29, 1, 0x2c }, { 0x2a, 1, 0x2b }, { 0x2b, 1, 0x2a },
		{ 0x2f, 2, 3    }, { 0x30, 2, 0    }, { 0x31, 2, 1    },
		{ 0x32, 2, 0x32 },
	};
	int shape = 1, ccNo = 0;
	for (unsigned int i = 0; i < 10; i++) {
		if (kTable[i].code == code) {
			shape = kTable[i].shape;
			ccNo = kTable[i].ccNo;
			break;
		}
	}

	if (busyClearBranch) {
		/* pressed == true, busy flag(&8) NOT set. */
		rtd->SendKarmaCCToKG(ccNo, 0x7f);
		if (shape == 1)
			rtd->SendUnsolControl2MessageToUI(msgType, buttonId, 0x7f, 1);
		return;
	}

	/* !pressed (release). */
	bool wasActive = TestButtonActiveBit(rtdBytes, code);
	if (shape == 1) {
		if (!wasActive)
			rtd->SendKarmaCCToKG(ccNo, 0);
		rtd->SendUnsolControl2MessageToUI(msgType, buttonId, 0, 1);
	} else {
		if (wasActive)
			rtd->SendUnsolControl2MessageToUI(msgType, buttonId, 0, 1);
		else
			rtd->SendKarmaCCToKG(ccNo, 0);
	}
	ClearButtonActiveBit(rtdBytes, code);
}

/* Table 2 "Pattern B" -- see this file's own header comment. */
static void HandleButtonPatternB(unsigned int code, bool pressed, int msgType, int buttonId)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	if (!pressed) {
		HandleButtonPatternBDeferred(code, pressed, false, msgType, buttonId);
		return;
	}
	if (!(rtdBytes[0x2f] & 8)) {
		HandleButtonPatternBDeferred(code, pressed, true, msgType, buttonId);
		return;
	}
	SetButtonActiveBit(rtdBytes, code);
	rtd->SendUnsolControl2MessageToUI(msgType, buttonId, 0x7f, 1);
}

/* ---------------------------------------------------------------------
 * Table 1/3 "special button" sub-branches -- named by their real
 * ground-truth address, one per unique far-jump target. ALL 17 NOW
 * RECONSTRUCTED (follow-up pass, ground-truthed via `objdump -dr -M
 * intel` on every address, including their own further targets --
 * several of these (0x35/0x36/0x39) turned out to cascade into a
 * previously-undiscovered `ChangeControlSurfaceMode(int)` state
 * machine spanning MANY more real addresses than the original 17-item
 * count suggested; see each function's own comment for the full
 * per-address trail). Real `CSTGGlobal` fields used below (`+0x684`
 * dword "mode", `+0x6a4` byte flag) are the SAME already-established
 * fields `global.cpp`/`front_panel_handlers.cpp`/etc. read elsewhere in
 * this project -- not new discoveries, cross-confirms these decodes.
 */

/* code 0x2c (44) press, busy flag(&8) CLEAR. Real: `.text+0x97569`
 * falls through into the ALREADY-known "busy SET" send at `.text+
 * 0x96dbe` after its own extra Karma CC send -- i.e. the UI message is
 * sent EITHER way, busy-clear just also sends a Karma CC first. */
static void ButtonDeferred_0x97569(CSTGControllerRTData *rtd)
{
	rtd->SendKarmaCCToKG(0x27, 0x7f);
	rtd->SendUnsolControl2MessageToUI(0xb, 6, 0x7f, 1);
}

/* code 0x35 (53) press, busy2(&2) CLEAR. Real: `.text+0x9746a` ->
 * (busy flag(&8) SET -> shared send, matches the already-modeled
 * busy2-SET path) -> CSTGGlobal+0x684==0 -> ChangeControlSurfaceMode(0)
 * -> else mode(rtd+0x2b)==0 -> ChangeControlSurfaceMode(1); mode==1 ->
 * ChangeControlSurfaceMode(0); else -> ChangeControlSurfaceMode(rtd+0x2c). */
static void ButtonDeferred_0x9746a(CSTGControllerInfo *self, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	if (rtdBytes[0x2f] & 8) {
		rtd->SendUnsolControl2MessageToUI(7, 0, 0x7f, 1);
		return;
	}
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	if (*(unsigned int *)(g + 0x684) == 0) {
		self->ChangeControlSurfaceMode(0);
		return;
	}
	signed char mode = (signed char)rtdBytes[0x2b];
	if (mode == 0) { self->ChangeControlSurfaceMode(1); return; }
	if (mode == 1) { self->ChangeControlSurfaceMode(0); return; }
	self->ChangeControlSurfaceMode((signed char)rtdBytes[0x2c]);
}

/* code 0x36 (54) press, busy2(&2) CLEAR. Real: `.text+0x9739d` ->
 * (busy flag(&8) SET -> shared send) -> CSTGGlobal+0x684!=2 ->
 * ChangeControlSurfaceMode(7) -> else mode==3 -> CCSM(7); mode==7 ->
 * CCSM(2); mode==2 -> CCSM(3); else -> CCSM(rtd+0x2d). */
static void ButtonDeferred_0x9739d(CSTGControllerInfo *self, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	if (rtdBytes[0x2f] & 8) {
		rtd->SendUnsolControl2MessageToUI(7, 1, 0x7f, 1);
		return;
	}
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	if (*(unsigned int *)(g + 0x684) != 2) {
		self->ChangeControlSurfaceMode(7);
		return;
	}
	signed char mode = (signed char)rtdBytes[0x2b];
	if (mode == 3) { self->ChangeControlSurfaceMode(7); return; }
	if (mode == 7) { self->ChangeControlSurfaceMode(2); return; }
	if (mode == 2) { self->ChangeControlSurfaceMode(3); return; }
	self->ChangeControlSurfaceMode((signed char)rtdBytes[0x2d]);
}

/* code 0x37 (55) press, busy flag(&8) SET. Real: `.text+0x9757d`,
 * simple unconditional send. */
static void ButtonDeferred_0x9757d(CSTGControllerRTData *rtd)
{
	rtd->SendUnsolControl2MessageToUI(7, 2, 0x7f, 1);
}

/* code 0x37 (55) press, busy2(&2) CLEAR (busy flag already confirmed
 * clear). Real: `.text+0x974e0` -- mode(rtd+0x2b)==4 is a confirmed
 * real no-op here (distinct from the caller's own mode==4 check, which
 * only applies on the busy2-SET path); otherwise ChangeControlSurfaceMode(4). */
static void ButtonDeferred_0x974e0(CSTGControllerInfo *self, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	if ((signed char)rtdBytes[0x2b] == 4)
		return; /* confirmed real no-op */
	self->ChangeControlSurfaceMode(4);
}

/* code 0x38 (56) press, busy flag(&8) SET. Real: `.text+0x9752e` --
 * sets the active bit (unlike most busy-SET press paths, which only
 * send) then sends. */
static void ButtonDeferred_0x9752e(unsigned int code, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	SetButtonActiveBit(rtdBytes, code);
	rtd->SendUnsolControl2MessageToUI(7, 3, 0x7f, 1);
}

/* code 0x38 (56) press, busy2(&2) CLEAR (busy flag already confirmed
 * clear). Real: `.text+0x974b5` -- mode(rtd+0x2b)!=5 ->
 * ChangeControlSurfaceMode(5) first; either way, always sends a Karma
 * CC afterward (ccNo 0x31). */
static void ButtonDeferred_0x974b5(CSTGControllerInfo *self, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	if ((signed char)rtdBytes[0x2b] != 5)
		self->ChangeControlSurfaceMode(5);
	rtd->SendKarmaCCToKG(0x31, 0x7f);
}

/* code 0x39 (57) press, busy2(&2) CLEAR. Real: `.text+0x973e0` ->
 * (busy flag(&8) SET -> shared send) -> CSTGGlobal+0x6a4==0 branch:
 * mode(rtd+0x2b)!=6 -> ChangeControlSurfaceMode(6); mode==6 ->
 * (CSTGGlobal+0x684==0 -> no-op; else -> set flags bit&4 + send) --
 * else (+0x6a4!=0) branch: mode==6 -> CCSM(8); mode==8 -> CCSM(6);
 * else -> CCSM(rtd+0x2e). */
static void ButtonDeferred_0x973e0(CSTGControllerInfo *self, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	if (rtdBytes[0x2f] & 8) {
		rtd->SendUnsolControl2MessageToUI(7, 4, 0x7f, 1);
		return;
	}
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	if (g[0x6a4] == 0) {
		if ((signed char)rtdBytes[0x2b] != 6) {
			self->ChangeControlSurfaceMode(6);
			return;
		}
		if (*(unsigned int *)(g + 0x684) == 0)
			return; /* confirmed real no-op */
		rtdBytes[0x2f] |= 4;
		rtd->SendUnsolControl2MessageToUI(7, 4, 0x7f, 1);
		return;
	}
	signed char mode = (signed char)rtdBytes[0x2b];
	if (mode == 6) { self->ChangeControlSurfaceMode(8); return; }
	if (mode == 8) { self->ChangeControlSurfaceMode(6); return; }
	self->ChangeControlSurfaceMode((signed char)rtdBytes[0x2e]);
}

/* code 0x4a (74) press, mode not in {0,1,2,3,7} OR busy flag(&8) SET
 * (two guards share this one target). Real: `.text+0x9737a`, simple
 * unconditional send. */
static void ButtonDeferred_0x9737a(CSTGControllerRTData *rtd)
{
	rtd->SendUnsolControl2MessageToUI(7, 5, 0x7f, 1);
}

/* code 0x4b (75) press, busy flag(&8) SET. Real: `.text+0x97433` --
 * sets the active bit then sends (SAME (8,2,0x7f,1) the caller's own
 * post-gate real path also sends). */
static void ButtonDeferred_0x97433(unsigned int code, CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	SetButtonActiveBit(rtdBytes, code);
	rtd->SendUnsolControl2MessageToUI(8, 2, 0x7f, 1);
}

/* code 0x4b (75) press, CSTGGlobal::sInstance+0x29cc4e8 byte == 0
 * (busy flag already confirmed clear). Real: `.text+0x97627` -- sets
 * busy2(&2), sends a Karma CC (ccNo 0x2e), then the same UI send. */
static void ButtonDeferred_0x97627(CSTGControllerRTData *rtd)
{
	unsigned char *rtdBytes = (unsigned char *)rtd;
	rtdBytes[0x2f] |= 2;
	rtd->SendKarmaCCToKG(0x2e, 0x7f);
	rtd->SendUnsolControl2MessageToUI(8, 2, 0x7f, 1);
}

/* code 0x4c (76) press, busy flag(&8) SET. Real: `.text+0x975b1`,
 * simple unconditional send. */
static void ButtonDeferred_0x975b1(CSTGControllerRTData *rtd)
{
	rtd->SendUnsolControl2MessageToUI(7, 6, 0x7f, 1);
}

/* code 0x4c (76) press, mode in {0,1,2,3,7} AND busy2(&2) SET. Real:
 * `.text+0x97641`, calls the real sibling `ResetSolo()`. */
static void ButtonDeferred_0x97641(CSTGControllerInfo *self)
{
	self->ResetSolo();
}

/* codes 0x4d/0x4e (77/78) press, busy flag(&8) CLEAR (shared target,
 * id = code-0x4d). Real: `.text+0x97450`, calls the real sibling
 * `ProcessPerfSwitchPress(id, true)` -- a genuinely different callee
 * than the busy-SET path's own inline send+bit-set. */
static void ButtonDeferred_0x97450(CSTGControllerInfo *self, unsigned int code)
{
	int id = (int)(code - 0x4d);
	self->ProcessPerfSwitchPress(id, true);
}

/* code 0x38 (56) release, per-button active bit NOT set. Real:
 * `.text+0x975d4` -- sends a Karma CC (ccNo 0x31) THEN the same UI
 * message the "bit was set" path also sends (matches Pattern B's own
 * shape-1 "always send UI, conditionally also Karma" idiom). Caller
 * clears the active bit itself (already 0 here, harmless). */
static void ButtonDeferred_0x975d4(CSTGControllerRTData *rtd)
{
	rtd->SendKarmaCCToKG(0x31, 0);
	rtd->SendUnsolControl2MessageToUI(7, 3, 0, 1);
}

/* code 0x4b (75) release, per-button active bit NOT set. Real:
 * `.text+0x9751d` -- SAME shape as 0x975d4 above (Karma ccNo 0x2e then
 * the UI send). */
static void ButtonDeferred_0x9751d(CSTGControllerRTData *rtd)
{
	rtd->SendKarmaCCToKG(0x2e, 0);
	rtd->SendUnsolControl2MessageToUI(8, 2, 0, 1);
}

/* codes 0x4d/0x4e (77/78) release, per-button active bit NOT set
 * (shared target). Real: `.text+0x974a3` -- calls the real sibling
 * `ProcessPerfSwitchPress(id, false)` (SAME callee the press-time
 * busy-clear branch above uses, with `pressed=false`). */
static void ButtonDeferred_0x974a3(CSTGControllerInfo *self, unsigned int code)
{
	int id = (int)(code - 0x4d);
	self->ProcessPerfSwitchPress(id, false);
}

/*
 * Table 1 -- PRESS-time bodies for the 12 special buttons (only reached
 * when pressed == true; see this file's own header comment). `self` is
 * the real `this` (`CSTGControllerInfo*`) -- three of these twelve call
 * real sibling member functions on it.
 */
static void HandleButtonPressSpecial(CSTGControllerInfo *self, unsigned int code)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;

	switch (code) {
	case 9:
		/* if busy2(&2) set: no-op. Else: set busy flag(&8),
		 * unconditional send (value hardcoded 0x7f -- pressed is
		 * always true in this table, so the usual "pressed?0x7f:0"
		 * simplifies away). Fully real, no deferred piece. */
		if (rtdBytes[0x2f] & 2)
			return;
		rtdBytes[0x2f] |= 8;
		rtd->SendUnsolControl2MessageToUI(8, 0, 0x7f, 1);
		return;

	case 0x2c: /* 44 */
		if (rtdBytes[0x2f] & 8) {
			rtd->SendUnsolControl2MessageToUI(0xb, 6, 0x7f, 1);
			return;
		}
		ButtonDeferred_0x97569(rtd);
		return;

	case 0x35: /* 53 */
		if (rtdBytes[0x2f] & 2) {
			rtd->SendUnsolControl2MessageToUI(7, 0, 0x7f, 1);
			return;
		}
		ButtonDeferred_0x9746a(self, rtd);
		return;

	case 0x36: /* 54 */
		if (rtdBytes[0x2f] & 2) {
			rtd->SendUnsolControl2MessageToUI(7, 1, 0x7f, 1);
			return;
		}
		ButtonDeferred_0x9739d(self, rtd);
		return;

	case 0x37: /* 55 -- confirmed real: if mode==4, ResetAllExtModeControllers(); else no-op. */
		if (rtdBytes[0x2f] & 8) {
			ButtonDeferred_0x9757d(rtd);
			return;
		}
		if (!(rtdBytes[0x2f] & 2)) {
			ButtonDeferred_0x974e0(self, rtd);
			return;
		}
		if ((signed char)rtdBytes[0x2b] == 4)
			self->ResetAllExtModeControllers();
		/* else: confirmed real no-op (mode != 4) */
		return;

	case 0x38: /* 56 -- confirmed real: if mode==5, send + ResetAllKnobCCs() +
		    * a real 5-byte MIDI CC message (same embedded-
		    * CSTGMidiQueueWriter idiom as front_panel_handlers.cpp's
		    * own SendFrontPanelKeyMidiMessage). */
		if (rtdBytes[0x2f] & 8) {
			ButtonDeferred_0x9752e(code, rtd);
			return;
		}
		if (!(rtdBytes[0x2f] & 2)) {
			ButtonDeferred_0x974b5(self, rtd);
			return;
		}
		if ((signed char)rtdBytes[0x2b] != 5)
			return; /* confirmed real no-op */
		{
			rtd->SendUnsolControl2MessageToUI(7, 3, 0x7f, 1);
			self->ResetAllKnobCCs();

			unsigned char channel = ((unsigned char *)CSTGGlobal::sInstance)[0x6b9];
			unsigned char msg[5];
			msg[0] = (unsigned char)(channel | 0xb0);
			msg[1] = 0x79;
			msg[2] = 0x1c;
			msg[3] = 5;
			msg[4] = 0xfe;
			CSTGMidiQueueWriter *writer = (CSTGMidiQueueWriter *)
				((unsigned char *)CSTGMidiPortManager::sInstance + 0x208);
			writer->Write(msg, 5, false);
		}
		return;

	case 0x39: /* 57 */
		if (rtdBytes[0x2f] & 2) {
			rtd->SendUnsolControl2MessageToUI(7, 4, 0x7f, 1);
			return;
		}
		ButtonDeferred_0x973e0(self, rtd);
		return;

	case 0x4a: { /* 74 -- confirmed real: "Mixer Knob Mode" cycle button. */
		signed char mode = (signed char)rtdBytes[0x2b];
		bool inRange = (mode == 7) || ((unsigned char)mode <= 3);
		if (!inRange || (rtdBytes[0x2f] & 8)) {
			ButtonDeferred_0x9737a(rtd);
			return;
		}
		unsigned char *selfBytes = (unsigned char *)self;
		bool newMode = (selfBytes[4] == 0);
		self->SetMixerKnobMode(newMode ? 1 : 0);
		self->NotifyParam(0, (long)(signed char)selfBytes[4]); /* weak/dead, see header */
		return;
	}

	case 0x4b: /* 75 */
		if (rtdBytes[0x2f] & 8) {
			ButtonDeferred_0x97433(code, rtd);
			return;
		}
		if (((unsigned char *)CSTGGlobal::sInstance)[0x29cc4e8] == 0) {
			ButtonDeferred_0x97627(rtd);
			return;
		}
		rtd->SendUnsolControl2MessageToUI(8, 2, 0x7f, 1);
		return;

	case 0x4c: { /* 76 -- confirmed real: SetSoloSelected() toggle button. */
		if (rtdBytes[0x2f] & 8) {
			ButtonDeferred_0x975b1(rtd);
			return;
		}
		signed char mode = (signed char)rtdBytes[0x2b];
		if (mode != 7 && (unsigned char)mode > 3)
			return; /* confirmed real no-op */
		if (rtdBytes[0x2f] & 2) {
			ButtonDeferred_0x97641(self);
			return;
		}
		bool newSelected = !(rtdBytes[0x21] & 1);
		self->SetSoloSelected(newSelected);
		self->NotifyParam(1, (long)newSelected); /* weak/dead, see header */
		return;
	}

	case 0x4d: /* 77 */
	case 0x4e: /* 78 */
		if (!(rtdBytes[0x2f] & 8)) {
			ButtonDeferred_0x97450(self, code);
			return;
		}
		{
			int id = (int)(code - 0x4d);
			rtd->SendUnsolControl2MessageToUI(1, id, 0x7f, 1);
			SetButtonActiveBit(rtdBytes, code);
		}
		return;

	default:
		return; /* confirmed real: table padding, unreachable via the real range guard */
	}
}

/*
 * Table 3 -- RELEASE-time bodies for the same 12 special buttons (only
 * reached when pressed == false). Mostly simple, unconditional
 * `SendUnsolControl2MessageToUI(msgType, id, 0, 1)` -- confirmed real,
 * NOT symmetric with the press-time actions above (several of these
 * buttons had no send at all on press, e.g. code 0x37/0x39/0x4a/0x4c).
 *
 * EVERY real (non-deferred) path here falls into the SAME shared
 * "clear active bit" tail table 2's simple cases use (`.text+0x9604e`,
 * confirmed via the real `jmp 9604e` ending every one of these 12
 * blocks) -- i.e. releasing any of these 12 buttons ALSO always clears
 * bit `code` in the `+0x30` bitmap, on top of whatever else the
 * individual case does (including cases that never set that bit on
 * press, e.g. 44/53/54/55/74/76 -- harmless/idempotent, but genuinely
 * executed in the real binary, reproduced here for fidelity).
 */
static void HandleButtonReleaseSpecial(CSTGControllerInfo *self, unsigned int code)
{
	CSTGControllerRTData *rtd = CSTGControllerRTData::sInstance;
	unsigned char *rtdBytes = (unsigned char *)rtd;
	bool didRealAction = true;

	switch (code) {
	case 9:
		/* Confirmed real: symmetric with the press-time action --
		 * clears the SAME busy flag(&8) bit code 9's press handler
		 * set (a DIFFERENT bit than the generic +0x30 tail below),
		 * then sends. */
		rtdBytes[0x2f] &= ~8;
		rtd->SendUnsolControl2MessageToUI(8, 0, 0, 1);
		break;

	case 0x2c: /* 44 */
		rtd->SendUnsolControl2MessageToUI(0xb, 6, 0, 1);
		break;

	case 0x35: /* 53 */
		rtd->SendUnsolControl2MessageToUI(7, 0, 0, 1);
		break;

	case 0x36: /* 54 */
		rtd->SendUnsolControl2MessageToUI(7, 1, 0, 1);
		break;

	case 0x37: /* 55 -- confirmed real: unconditional send on release,
		    * unlike the press side (which only acted on mode==4). */
		rtd->SendUnsolControl2MessageToUI(7, 2, 0, 1);
		break;

	case 0x38: /* 56 -- gated on the SAME per-button active bit tables 1/3 share. */
		if (!TestButtonActiveBit(rtdBytes, code)) {
			ButtonDeferred_0x975d4(rtd);
			didRealAction = false;
			break;
		}
		rtd->SendUnsolControl2MessageToUI(7, 3, 0, 1);
		break;

	case 0x39: /* 57 -- confirmed real: clears flags bit &4 (a THIRD,
		    * otherwise-unnamed flag bit -- NOT the &2 "busy2" bit
		    * the press-time handler gated on), unconditional send. */
		rtdBytes[0x2f] &= ~4;
		rtd->SendUnsolControl2MessageToUI(7, 4, 0, 1);
		break;

	case 0x4a: /* 74 -- confirmed real: unconditional send, unlike the
		    * press side (which called SetMixerKnobMode instead). */
		rtd->SendUnsolControl2MessageToUI(7, 5, 0, 1);
		break;

	case 0x4b: /* 75 -- confirmed real: clears busy2(&2) UNCONDITIONALLY
		    * first (before the gate below), then gated on the
		    * per-button active bit. */
		rtdBytes[0x2f] &= ~2;
		if (!TestButtonActiveBit(rtdBytes, code)) {
			ButtonDeferred_0x9751d(rtd);
			didRealAction = false;
			break;
		}
		rtd->SendUnsolControl2MessageToUI(8, 2, 0, 1);
		break;

	case 0x4c: /* 76 -- confirmed real: unconditional send, unlike the
		    * press side (which called SetSoloSelected instead). */
		rtd->SendUnsolControl2MessageToUI(7, 6, 0, 1);
		break;

	case 0x4d: /* 77 */
	case 0x4e: /* 78 -- shared target, gated on the per-button active bit
		    * tables 1/3 share (the press-time handler above sets it). */
		if (!TestButtonActiveBit(rtdBytes, code)) {
			ButtonDeferred_0x974a3(self, code);
			didRealAction = false;
			break;
		}
		{
			int id = (int)(code - 0x4d);
			rtd->SendUnsolControl2MessageToUI(1, id, 0, 1);
		}
		break;

	default:
		/* confirmed real: table padding -- unreachable via the real
		 * range guards this file's callers apply (see header
		 * comment), but if it WERE reached, real ground truth is
		 * the plain clear-bit tail with no message send. */
		break;
	}

	if (didRealAction)
		ClearButtonActiveBit(rtdBytes, code);
}

/* Table 2 -- the main `code <= 0x49` dispatch. */
static void DispatchButtonCodeLow(CSTGControllerInfo *self, unsigned int code, bool pressed)
{
	switch (code) {
	/* Compiler quirk: these low codes redirect into the SAME
	 * press/release special-button dispatch tables 1/3 use for
	 * `code > 0x49` -- confirmed real via the shared `.rodata` table
	 * entry (`.text+0x95fd0`), not a guess. code 0 always underflows
	 * the table-1/3 range check (real no-op, both branches). */
	case 0:
		return;
	case 9: case 0x2c: case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
		if (pressed)
			HandleButtonPressSpecial(self, code);
		else
			HandleButtonReleaseSpecial(self, code);
		return;

	/* Pattern A: uniform simple send (real (msgType,buttonId) pairs,
	 * individually disassembly-confirmed). */
	case 1:  SendButtonPatternA(code, pressed, 7, 8);  return;
	case 2:  SendButtonPatternA(code, pressed, 7, 9);  return;
	case 3:  SendButtonPatternA(code, pressed, 7, 10); return;
	case 4:  SendButtonPatternA(code, pressed, 7, 11); return;
	case 5:  SendButtonPatternA(code, pressed, 7, 12); return;
	case 6:  SendButtonPatternA(code, pressed, 7, 13); return;
	case 7:  SendButtonPatternA(code, pressed, 7, 14); return;
	case 8:  SendButtonPatternA(code, pressed, 6, 2);  return;
	case 10: SendButtonPatternA(code, pressed, 8, 1);  return;
	case 11: SendButtonPatternA(code, pressed, 6, 6);  return;
	case 12: SendButtonPatternA(code, pressed, 6, 7);  return;
	case 13: SendButtonPatternA(code, pressed, 6, 8);  return;
	case 14: SendButtonPatternA(code, pressed, 6, 9);  return;
	case 15: SendButtonPatternA(code, pressed, 6, 10); return;
	case 16: SendButtonPatternA(code, pressed, 6, 11); return;
	case 17: SendButtonPatternA(code, pressed, 6, 12); return;
	case 18: SendButtonPatternA(code, pressed, 6, 13); return;
	case 19: SendButtonPatternA(code, pressed, 6, 14); return;
	case 20: SendButtonPatternA(code, pressed, 6, 15); return;
	case 21: SendButtonPatternA(code, pressed, 6, 5);  return;
	case 22: SendButtonPatternA(code, pressed, 6, 4);  return;
	case 23: SendButtonPatternA(code, pressed, 6, 16); return;
	case 24: SendButtonPatternA(code, pressed, 9, 0);  return;
	case 25: SendButtonPatternA(code, pressed, 9, 1);  return;
	case 26: SendButtonPatternA(code, pressed, 9, 2);  return;
	case 27: SendButtonPatternA(code, pressed, 9, 3);  return;
	case 28: SendButtonPatternA(code, pressed, 9, 4);  return;
	case 29: SendButtonPatternA(code, pressed, 9, 5);  return;
	case 30: SendButtonPatternA(code, pressed, 9, 6);  return;
	case 31: SendButtonPatternA(code, pressed, 9, 7);  return;
	case 32: SendButtonPatternA(code, pressed, 9, 8);  return;
	case 33: SendButtonPatternA(code, pressed, 9, 9);  return;
	case 34: SendButtonPatternA(code, pressed, 9, 10); return;
	case 0x23 /*35*/: SendButtonPatternA(code, pressed, 9, 11); return;
	case 0x24 /*36*/: SendButtonPatternA(code, pressed, 9, 12); return;
	case 0x25 /*37*/: SendButtonPatternA(code, pressed, 9, 13); return;
	case 0x2d /*45*/: SendButtonPatternA(code, pressed, 10, 0); return;
	case 0x2e /*46*/: SendButtonPatternA(code, pressed, 10, 1); return;
	case 0x33 /*51*/: SendButtonPatternA(code, pressed, 6, 0);  return;
	case 0x34 /*52*/: SendButtonPatternA(code, pressed, 6, 1);  return;

	/* Pattern B: own press/busy gating, sets the active bit inline. */
	case 0x26 /*38*/: HandleButtonPatternB(code, pressed, 11, 5); return;
	case 0x27 /*39*/: HandleButtonPatternB(code, pressed, 11, 4); return;
	case 0x28 /*40*/: HandleButtonPatternB(code, pressed, 11, 3); return;
	case 0x29 /*41*/: HandleButtonPatternB(code, pressed, 11, 2); return;
	case 0x2a /*42*/: HandleButtonPatternB(code, pressed, 11, 1); return;
	case 0x2b /*43*/: HandleButtonPatternB(code, pressed, 11, 0); return;
	case 0x2f /*47*/: HandleButtonPatternB(code, pressed, 3, 0);  return;
	case 0x30 /*48*/: HandleButtonPatternB(code, pressed, 2, 0);  return;
	case 0x31 /*49*/: HandleButtonPatternB(code, pressed, 2, 1);  return;
	case 0x32 /*50*/: HandleButtonPatternB(code, pressed, 2, 3);  return;

	/* Mixer-switch buttons (0x3a-0x49): unconditional dispatch to the
	 * real sibling ProcessMixerSwitchPress(), then the SAME generic
	 * tail (clear active bit on release). */
	case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
	case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45:
	case 0x46: case 0x47: case 0x48: case 0x49:
		self->ProcessMixerSwitchPress(code, pressed);
		if (!pressed)
			ClearButtonActiveBit((unsigned char *)CSTGControllerRTData::sInstance, code);
		return;

	default:
		return; /* confirmed real: unreachable, out of the real 0-0x49 domain */
	}
}

void CSTGControllerInfo::ButtonPressHandler(unsigned int code, bool pressed)
{
	if (*(unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29cc4dc) != 0) {
		if (HandleEditInContextButton(code, pressed))
			return;
	}

	if (code <= 0x49) {
		DispatchButtonCodeLow(this, code, pressed);
		return;
	}

	if (!pressed) {
		unsigned int idx = code - 9;
		if (idx > 0x45)
			return; /* confirmed real: out of range, no-op */
		HandleButtonReleaseSpecial(this, code);
		return;
	}

	unsigned int idx = code - 9;
	if (idx > 0x45)
		return; /* confirmed real: out of range, no-op */
	HandleButtonPressSpecial(this, code);
}
