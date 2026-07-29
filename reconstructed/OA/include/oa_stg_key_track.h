// SPDX-License-Identifier: GPL-2.0
/*
 * oa_stg_key_track.h  -  CSTGKeyTrack: the key-tracking (keyboard-
 * position modulation) DSP component, shared across many STG voice
 * models.
 *
 * FOUND 2026-07-29 (round 51, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md). Confirmed via
 * /home/share/Decomp/oa_export's own per-function decompiles + symbols.csv
 * mangled-name cross-check for every method below. 15/24 methods landed
 * this round; 9 deliberately deferred (see two distinct reasons below).
 *
 * Confirmed real object layout (this project's regparm(3) ABI: `this` is
 * EAX, ground truth's own decompile shows it as an unused declared
 * parameter with the real body reading an `in_EAX` pseudo-variable --
 * same gotcha as oa_ckg_midi_msg_handler.h):
 *   +0x00  vptr (real: dtor sets it to `&PTR__CSTGParamsOwner_006c04a8`
 *          -- CSTGKeyTrack genuinely derives from `CSTGParamsOwner`,
 *          which this project's own established convention (see
 *          oa_global.h's own CSTGParamsOwner comment) deliberately does
 *          NOT model as a real C++ base class, to avoid disturbing
 *          already-confirmed absolute offsets elsewhere -- modeled here
 *          as an opaque placeholder field, same treatment)
 *   +0x08  CSTGComponentSlotInfo* _slotInfo -- SAME struct/offset
 *          already established by CSTGADSRBase/CSTGLFO
 *          (oa_adsr_base.h); its own `subRateBaseIndex` (+0x04) field
 *          combines with `CSTGVoiceModelManager::sInstance`'s own +0x4
 *          table pointer via the SAME "quad table" formula
 *          (`(note&3)+(note>>2)*0xcc0`) already documented there and
 *          in oa_lfo.h, confirmed independently by THIS class's own
 *          `GetOutput`/`FreeVoice` bodies.
 *   +0x0c  mLowKey (signed char) -- `UpdateLowKey`'s own target
 *   +0x0d  mMidKey (signed char) -- `UpdateMidKey`'s own target
 *   +0x0e  mHighKey (signed char) -- `UpdateHighKey`'s own target
 *   +0x0f..+0x12  4 more signed-char "ramp" fields (low/midLow/
 *          midHigh/high), touched ONLY by the deferred `UpdateXxxRamp`
 *          family below -- left as opaque padding to preserve the real
 *          offsets for whichever future batch reconstructs those.
 *
 * `GetOutput(int, int)`/`FreeVoice(CSTGVoice&)` both independently
 * confirm the exact same "quad table" per-voice addressing formula
 * already established for CSTGADSRBase/CSTGLFO -- `GetOutput`'s own
 * 2nd explicit arg is multiplied by `0x10` (a "layer" stride,
 * unconfirmed real name), `FreeVoice` always operates on the fixed
 * `+0x20` layer (2*0x10) -- both raw offsets preserved verbatim, not
 * given invented semantic names beyond what's independently confirmed.
 *
 * `InitializeQuad`/`PrepareSubRateAddressFixupTable` are `static`
 * (ground truth's own decompile shows NO `this` parameter at all for
 * either, `cc=__regparm3`, unlike every genuinely-`this`-taking method
 * here which Ghidra always shows an explicit `CSTGKeyTrack *this` for)
 * -- `PrepareSubRateAddressFixupTable` reuses the SAME
 * `CSTGSubRateAddressFixupTable` struct already declared in
 * oa_adsr_base.h (confirmed identical `entries`/`count` shape), unlike
 * CSTGADSRBase's own version which appends 8 entries per call, this
 * one appends exactly 1.
 *
 * `STGKeyTrackAudioRateParams`/`STGKeyTrackSubRateParams` modeled as
 * opaque byte blobs, same "don't over-fit a named-field layout this
 * cluster can't fully justify" convention as
 * `STGADSRBaseSubRateParams` (oa_adsr_base.h) -- `InitializeQuad`'s
 * own real body (transcribed verbatim in the .cpp) only ever touches
 * raw offsets +0x10/+0x14/+0x18/+0x1c (pointers, defaulted to the SAME
 * shared "no source" address `CSTGGlobal::sInstance+0x29c9fa0` already
 * established by CSTGADSRBase's own InitializeQuad) and +0x20/+0x24/
 * +0x28/+0x2c (ints, zeroed).
 *
 * `GetId`/`GetName`/`GetNumParams`/`GetParamDescriptors`/
 * `GetMessageHandlers`/`GetValueGetters` -- the SAME CSTGParamsOwner
 * framework-metadata accessor family already reconstructed for
 * CSTGGlobal/CSTGLFO (round 48-adjacent), all `static`-shaped
 * (cc=__cdecl, no `this` at all) trivial literal/pointer returns.
 * `GetId()`=0x15, `GetName()`="KeyTrack", `GetNumParams()`=7; the
 * other 3 return real named backing-table pointers whose CONTENTS are
 * not recovered (same "table contents unknown, accessor body IS
 * reconstructable" distinction already established for CSTGGlobal's
 * own framework accessors, oa_global.h).
 *
 * === Deferred, 2 distinct reasons ===
 * (1) `PrecomputeData`/`UpdateLowRamp`/`UpdateMidLowRamp`/
 *     `UpdateMidHighRamp`/`UpdateHighRamp` -- each makes a genuine
 *     virtual call through THIS class's own vtable at raw byte offset
 *     0xc0 from the vptr (`(**(code**)(*this+0xc0))()`), a real,
 *     fully-concrete call (no "could not recover" warning) but with
 *     no independent confirmation of which named method occupies that
 *     slot -- same deferral class as OA.ko's `CFileStream::
 *     SetPositionBeginning` (round 49) and Eva's `CSysExKarmaGE::
 *     GetTotalSizeForExport` (round 42).
 * (2) `ConvertIntRampToSlope`/`ConvertSlopeToIntRamp`/
 *     `CalculateKeyTracking` (and its 3 callers `InitVoice`/
 *     `InitVoiceUsingInput`/`ProcessSubRate`) -- fully concrete control
 *     flow, genuinely recoverable index/offset math, but each compares
 *     against or multiplies by real named-but-unrecovered floating-
 *     point .rodata CONSTANTS (`_DAT_006bab6c` etc, Ghidra's own
 *     placeholder names for literal float bit patterns this pass has
 *     no way to read without a matching live binary) -- a genuinely
 *     different blocker than (1), not a vtable-slot ambiguity but a
 *     missing-literal-value one. Left undeclared/uncredited rather
 *     than guessed at.
 */

#ifndef OA_STG_KEY_TRACK_H
#define OA_STG_KEY_TRACK_H

#include "oa_adsr_base.h"	/* CSTGComponentSlotInfo, CSTGSubRateAddressFixupTable */
#include "oa_global.h"		/* STGConvertedParam, CSTGGlobal::sInstance */

class CSTGVoice;		/* opaque, only ever offset-accessed */
struct CSTGPatchMessageContext;

/* Opaque, see header comment -- real sizes/full layouts not recovered. */
struct STGKeyTrackAudioRateParams {
	unsigned char _unrecovered[0x10];
};
struct STGKeyTrackSubRateParams {
	unsigned char _unrecovered[0x30];
};

class CSTGKeyTrack {
public:
	~CSTGKeyTrack();

	int GetOutput(int note, int layer) const;
	void FreeVoice(CSTGVoice &voice);

	void UpdateLowKey(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateMidKey(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);
	void UpdateHighKey(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal);

	static void InitializeQuad(STGKeyTrackAudioRateParams *audioParams,
				    STGKeyTrackSubRateParams *subRateParams);
	static void PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table,
						     unsigned long note);

	static unsigned int GetId();
	static const char *GetName();
	static unsigned int GetNumParams();
	static const void *GetParamDescriptors();
	static const void *GetMessageHandlers();
	static const void *GetValueGetters();

private:
	/* 4 bytes (not `void*`) so +0x08 below lands at the real target's
	 * own confirmed `_slotInfo` offset even on this 64-bit host --
	 * never dereferenced as a real pointer, just an opaque marker
	 * (see header comment). */
	unsigned char mVptrPlaceholder[4]; /* +0x00 */
	unsigned char mUnknown04[4];	/* +0x04..+0x07 */
	CSTGComponentSlotInfo *_slotInfo; /* +0x08 */
	signed char mLowKey;		/* +0x0c */
	signed char mMidKey;		/* +0x0d */
	signed char mHighKey;		/* +0x0e */
	unsigned char mUnknownRamps[4]; /* +0x0f..+0x12, see header comment */
};

#endif /* OA_STG_KEY_TRACK_H */
