// SPDX-License-Identifier: GPL-2.0
/*
 * audio_input_use_settings.cpp  -  CSTGAudioInput::UseSettings() (batch 18,
 * `.text+0xc9f30`, 962 bytes).
 *
 * Deliberately a SEPARATE translation unit from global.cpp (matching the
 * CSTGAudioInputMixerBase-setters precedent, sec 10.150): `verify/
 * test_engine.cpp` and `verify/test_global_ctor.cpp` both carry
 * pre-existing trivial no-op mocks of this exact symbol, and `verify/
 * test_global.cpp` carries a LOAD-BEARING call-counting mock
 * (`g_useSettingsCalls`/`g_lastUseSettingsThis`, used by its own
 * "OnUseGlobalSettingsChanged -> UseSettings called" assertion) -- none of
 * those three link this new file, so their mocks are left completely
 * untouched. This real body is instead exercised by its own dedicated
 * `verify/test_audio_input_use_settings.cpp`, which links the REAL
 * `audio_input_mixer.cpp` (SetPan/SetOutputBus/SetFXCtrlBus/SetHDRBus,
 * sec 10.150/10.151) directly, so the Set*Bus/SetPan calls below are
 * exercised end-to-end, not stubbed.
 *
 * Confirmed layout (regparm(3): this=EAX, no other args). A confirmed
 * real, fully unrolled loop over the 6 fixed audio-input buses (0..5) --
 * every field offset below cross-checked against this class's own
 * ALREADY-REAL per-bus `UpdateLevel`/`UpdateSend1Level`/`UpdateSend2Level`/
 * `UpdateMute`/`UpdatePan`/`UpdateBusSelect`/`UpdateFXControlBus`/
 * `UpdateHDRBus` handlers (sec 10.80, src/engine/global.cpp) -- this
 * function is the "apply everything at once" counterpart those individual
 * live-update handlers each apply one field/bus at a time (gated on the
 * `+0x77` bit1 "performance active" flag those handlers check; UseSettings
 * itself has NO such gate -- it unconditionally pushes every cached local
 * field out to the resolved mixer, then sets bit1 itself at the very end,
 * i.e. this is what flips that gate on in the first place, presumably
 * called once when a performance/program becomes the active one).
 *
 * Per bus `i` (0..5), unconditionally:
 *   - `region = FromU32(mixer->mixerStateArray32) + i*0x90` (the SAME
 *     `mixerStateArray + busIndex*0x90` region `CSTGAudioInputMixerBase`'s
 *     own setters index into, confirmed sec 10.150/10.151).
 *   - `region+0x78 = this->fieldAt(0x4+i*4)` (raw dword copy -- matches
 *     `UpdateLevel`'s own confirmed `region+0x78` target exactly).
 *   - `region+0x7c = this->fieldAt(0x34+i*4)` (matches `UpdateSend1Level`).
 *   - `region+0x80 = this->fieldAt(0x4c+i*4)` (matches `UpdateSend2Level`).
 *   - `region+0x84 = (this->fieldAt(0x76) >> i) & 1` (matches `UpdateMute`'s
 *     own isolated-bit target).
 *   - Pan: `diff = this->fieldAt(0x1c+i*4) - 1.0f`; `pan = (diff <= 0.0f)
 *     ? 0.0f : diff * (1.0f/126.0f)` (confirmed real: the scale constant
 *     is `.rodata.cst8+0x140` = the IEEE-754 double `1/126`
 *     (0.007936507936507936), independently confirmed via `readelf -x`;
 *     this is a DIFFERENT transform from `UpdatePan`'s own live-update
 *     path, which passes its stored float through basically unchanged
 *     except for a sign flip on negative values -- the two functions are
 *     NOT required to share a formula, and a full disassembly confirms
 *     they genuinely don't). Calls `mixer->SetPan(i, pan)`.
 *   - `mixer->SetOutputBus(i, (signed char)this->fieldAt(0x64+i))`,
 *     `SetFXCtrlBus(i, (signed char)this->fieldAt(0x6a+i))`,
 *     `SetHDRBus(i, (signed char)this->fieldAt(0x70+i))` -- called
 *     directly on the resolved `mixer` object itself (NOT on `region`),
 *     matching `UpdateHDRBus`/`UpdateFXControlBus`/`UpdateBusSelect`'s own
 *     established convention of casting the raw resolved pointer straight
 *     to `CSTGAudioInputMixerBase*`.
 * After all 6 buses: unconditionally `this->fieldAt(0x77) |= 0x2` (sets
 * the SAME "performance active" bit the eight per-bus Update* handlers
 * gate their own live-mixer writes on).
 */

#include "oa_global.h"

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/* FSub()/FMulK()/FLessEqZero() -- small x87 inline-asm primitives, same
 * rationale/style as audio_input_mixer.cpp's own FMul/FAdd/FLess/FLessEq
 * (sec 10.151) and global.cpp's MulRoundToFloat/FYL2X (sec 10.117): this
 * kernel build is `-msoft-float -mno-sse`, so plain C float arithmetic on
 * a non-constant value would otherwise pull in unresolvable libgcc
 * soft-float helpers. All operands are memory ("m"), the whole push/pop
 * sequence self-contained in one asm statement each -- avoids the
 * register-tied-constraint fragility already found (and fixed) once in
 * this project's history (sec 10.151's own "chained across nested calls"
 * gotcha).
 */
static inline float FSub(float a, float b)
{
	float result;
	/* NOTE: operand push order is (b, a), NOT (a, b) like the commutative
	 * FMulK below -- confirmed via a standalone empirical test that GNU
	 * `as`'s `fsubp %st,%st(1)` computes ST(1) = ST(0) - ST(1) (the
	 * OPPOSITE of what the AT&T-syntax operand order suggests at a
	 * glance, a long-standing binutils quirk kept for compatibility).
	 * Pushing b first (so it becomes ST(1)) then a (ST(0)) makes the
	 * result ST(0) - ST(1) = a - b, which is what this function's own
	 * name promises. Caught by a real KAT FAILED result (own-draft bug,
	 * not a ground-truth quirk) -- see test_audio_input_use_settings.cpp. */
	__asm__ __volatile__(
		"flds %2\n\t"
		"flds %1\n\t"
		"fsubp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

static inline float FMulK(float a, float b)
{
	float result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"flds %2\n\t"
		"fmulp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

static inline int FLessEqZero(float a)
{
	unsigned char r;
	float zero = 0.0f;
	__asm__ __volatile__(
		"flds %2\n\t"
		"flds %1\n\t"
		"fucomip %%st(1), %%st\n\t"
		"fstp %%st(0)\n\t"
		"setbe %0"
		: "=q" (r)
		: "m" (a), "m" (zero)
		: "cc"
	);
	return r;
}

void CSTGAudioInput::UseSettings()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	CSTGAudioInputMixerBase *mixer = (CSTGAudioInputMixerBase *)mgr;

	for (unsigned int i = 0; i < 6; i++) {
		unsigned char *region = FromU32(mixer->mixerStateArray32) + i * 0x90;

		*(int *)(region + 0x78) = *(int *)(base + 0x4 + i * 4);
		*(int *)(region + 0x7c) = *(int *)(base + 0x34 + i * 4);
		*(int *)(region + 0x80) = *(int *)(base + 0x4c + i * 4);

		unsigned char muteMask = base[0x76];
		region[0x84] = (unsigned char)((muteMask >> i) & 0x1);

		float raw = *(float *)(base + 0x1c + i * 4);
		float diff = FSub(raw, 1.0f);
		float pan = FLessEqZero(diff) ? 0.0f : FMulK(diff, (1.0f / 126.0f));
		mixer->SetPan(i, pan);

		mixer->SetOutputBus(i, (signed char)base[0x64 + i]);
		mixer->SetFXCtrlBus(i, (signed char)base[0x6a + i]);
		mixer->SetHDRBus(i, (signed char)base[0x70 + i]);
	}

	base[0x77] |= 0x2;
}

/*
 * CSTGAudioInput::OnPerformanceDeactivate() (batch 20, `.text+0xc9f00`,
 * 39 bytes). The exact counterpart to UseSettings() above: where
 * UseSettings() SETS the `+0x77` bit1 "performance active" gate, this
 * CLEARS bit1 -- but from one of two places depending on whose settings
 * are live, mirroring OnUseGlobalSettingsChanged()'s own two-target
 * pattern (sec 10.134, oa_global.h):
 *
 *   if (CSTGGlobal::sInstance[0x680] != 0 || (this[0x77] & 1) != 0)
 *       CSTGGlobal::sInstance[0x67f] &= ~0x2;     // clear the GLOBAL bit
 *   else
 *       this[0x77] &= ~0x2;                        // clear THIS input's bit
 *
 * `+0x680` is the global-settings-enabled flag and `+0x67f` bit1 is the
 * global "active" bit -- both the exact same fields OnUseGlobalSettingsChanged
 * already documents. The `||` faithfully models the real short-circuit
 * (the asm's `jne` skips the `this[0x77]` read entirely when `+0x680` is
 * set) and is safe to write as `||` here specifically because the skipped
 * operand is a plain field READ with no side effect -- unlike the sec
 * 10.167 no-short-circuit case, there is no call to be elided. No calls,
 * no vtable dispatch; pure field read-modify-write.
 *
 * Called from CSTGPerformance::SetIsDying (batch 19) on the embedded
 * sub-object at `perf+0xae7`.
 */
void CSTGAudioInput::OnPerformanceDeactivate()
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (g[0x680] != 0 || (base[0x77] & 0x1) != 0)
		g[0x67f] &= (unsigned char)~0x2;
	else
		base[0x77] &= (unsigned char)~0x2;
}
