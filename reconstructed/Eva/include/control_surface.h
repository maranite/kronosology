// SPDX-License-Identifier: GPL-2.0
/*
 * control_surface.h  -  CControlSurface: the physical control-surface layer
 * (knobs/faders/switches/LEDs above the KARMA pads and mixer strips).
 * FOUND 2026-07-29 (round 49, solo, fresh class survey, corrected earlier
 * accidental re-discovery of the already-landed CStorageConverterBase --
 * see HARDWARE_REVIEW_LOG.md round 49 entry for the case-sensitivity bug
 * in the survey script that caused that false alarm).
 *
 * 30/161 pending methods landed this round -- the smallest, fully
 * self-contained subset: every method here only touches `this` (raw byte
 * offsets, matching this project's established "decompiler treats the
 * class as an opaque 1-byte type" idiom -- same convention as
 * CSTGWaveSequence/CConvertStorageParam) and/or 2 real `.rodata` lookup
 * tables, never an unreconstructed sibling class.
 *
 * Landed methods split into 4 shapes:
 * (1) 7 real no-op overrides (ground truth: literal 1-byte `return;`) --
 *     `EditKarmaPadVelocityMode`, the 3 `UpdateKnobFaderLED` overloads,
 *     `PressPlayMuteSwitchInExternal`, `PressSelectSwitchInExternal`,
 *     `PressPlayMuteSwitchInGraphicEQ`, `PressSelectSwitchInGraphicEQ`,
 *     `MoveKnobInGraphicEQ`.
 * (2) Plain fixed-offset field get/set (`InitializeSelectSwitchIn{External,
 *     GraphicEQ}`, `InitializePlayMuteSwitchIn{External,GraphicEQ}`,
 *     `EditMasterVolume`, `GetSongForReset` (returns the address of a real
 *     static/global `m_oSongForReset`, `__cdecl`, no `this` -- ground
 *     truth confirmed, content/type not independently confirmed beyond
 *     being an opaque 4-byte-aligned object), `GetSoloSelected` (indexes a
 *     real pointer array at `+0x7ec` by a real index field at `+0x128`,
 *     reads `+0x24` off the result -- all raw memory, no external call
 *     needed even though the pointed-at object is an unreconstructed
 *     `CTrackStatus`), `InitializeModKnob`, `GetExternalSwitch`
 *     (a real bit-test), `UpdateKarmaSceneSelectLED` deferred instead --
 *     see below).
 * (3) `param*0x10 + <base>` real per-channel-array writes (`EditRTKnob`,
 *     `EditSetListEQBandLevel`, `EditAudioVolume`, `EditAudioTrackVolume`,
 *     `EditVolume`, `EditKnobFaderForCustomMod`) and reads
 *     (`GetExternalKnob`/`GetExternalSlider`, different index bases into
 *     the SAME array, `+0x15`/`+0x1d`) -- real per-channel array,
 *     confirmed stride `0x10`, offsets `+0xc` (knob/slider slot) and
 *     `+0x8c` (volume slot, `= +0xc + 8*0x10`); array's own real bounds
 *     not independently confirmed beyond what these callers themselves
 *     prove.
 * (4) `GetModeLEDCode`/`GetKarmaModuleSelectLEDCode` -- bounds-checked
 *     index into a real `.rodata` lookup table (Ghidra's own
 *     `CSWTCH_497`/`CSWTCH_500` placeholder names), out-of-range falls
 *     back to a fixed literal constant -- same "confirmed real, content
 *     unread" treatment as every other such table in this project (e.g.
 *     OA.ko's `STGAPIOutToPhysBusId`, round 55). `IsUseGlobalAudio` and
 *     `EditKnobFaderForCustomMod` both dereference a real but
 *     unreconstructed pointer field at `+0x140` purely as raw bytes (no
 *     type needed for the single-byte reads they do).
 *
 * `EMode`/`EKnobFader`/`ELedCode` are real ground-truth enum types
 * (`CControlSurface::EMode` etc, per the decompiler's own demangled
 * prototypes) whose enumerator names were NOT recovered this round --
 * every parameter of one of these types is typed plain `int` here
 * (matches this project's established substitution for an unrecovered
 * named-enum parameter type, e.g. OA.ko's numerous unnamed-vtable-slot
 * notes).
 *
 * `CControlSurface`'s own real size is NOT independently confirmed --
 * sized conservatively to the highest offset any landed method this
 * round touches (`+0x808`, `IsUseGlobalAudio`), rounded up to `0x810`.
 * Same "declare uncertain size clearly" convention as
 * `storage_converter_base.h`'s `CConvertStorageParam`.
 *
 * === Deferred, 3 reasons (131/161 methods after round 49) ===
 * (1) Calls into a real but wholly unreconstructed sibling class:
 *     `SetAsSoloSelected`/`GetSolo`/`GetAudioTrackSolo`/
 *     `GetAudioInputSolo` (`CTrackStatus::SetAsSoloSelected`/`GetSolo`),
 *     `UpdateLED`/`EditAssignableSwitch` (`CMMI::GetInstance`/`SetLED`),
 *     `ForceGlobalAudio` (`USTGAPIControl::UseGlobalAudioInputSettings`),
 *     `PressSelectSwitchForSolo` (`CTrackStatus::ToggleSolo`,
 *     `USTGUserAPI::mNowStopMessaging`), `UpdateModeLED` (`CMMI`).
 * (2) Calls into an unreconstructed SIBLING `CControlSurface` method:
 *     `UpdateKarmaSceneSelectLED` (calls `UpdatePlayMuteSwitchLED`),
 *     `ForceGlobalAudio` (also calls `SetAsUseGlobalSetting`) -- deferred
 *     together for accuracy rather than guessing at the sibling's real
 *     behavior.
 * (3) Everything else not surveyed this round (up to several KB) -- a
 *     dedicated future round's scope, same discipline as every other
 *     oversized class in this project.
 *
 * === Round 50 batch (2026-07-29, solo): 13 more methods ===
 * All self-contained (raw `this`-relative reads/writes and 2 more real,
 * content-unread packed-pointer fields at `+0x144`/`+0x148`, same
 * `FromU32()` treatment as `+0x140`), no external calls.
 *
 * BUG FIX (round 50): `GetSongForReset()`'s own sibling
 * `SetSongForReset(CSong*)` (real ground truth) copies `0xcc5` (3269)
 * DWORDs -- 13,076 bytes -- from its argument into `m_oSongForReset`,
 * not a single 4-byte scalar. Round 49's `g_oSongForReset` placeholder
 * was a bare `unsigned int` (4 bytes); grown here to
 * `unsigned int[0xcc5]` to match the real copy size ground truth
 * proves, avoiding a buffer overflow the moment `SetSongForReset` is
 * exercised. `GetSongForReset()` itself is unaffected (still returns
 * the array's own address, array-to-pointer decay).
 *
 * `SetBackupMode(EMode)`: real per-mode bitfield router -- modes 0/1
 * write `+0x118`, modes 2/3/7 write `+0x119`, modes 4/5/6/`>=8` are a
 * genuine no-op (ground truth's own two disjoint bitmasks `0x8c`/`3`
 * against `1<<mode`, preserved verbatim, not simplified to a range
 * check).
 *
 * `ShouldSetupFaderAsReverse(EKnobFader, EAlgorithm, uchar)`: unused
 * `this`, pure 3-arg boolean logic -- `knobFader` in `[8,16]` AND
 * `algorithm==3` AND (`value` in `[0x23,0x27]` OR `value<0x1a`).
 *
 * `EditAudioChannelStripKnob`/`EditIFXSend1`/`EditIFXSend2`/
 * `EditAudioPan` all gate on the SAME real `+0x140` pointed-at object's
 * own `+0`/`+2` fields (top-bit-of-byte-0 and low-nibble-of-byte-2)
 * already established by round 49's `IsUseGlobalAudio`/
 * `EditKnobFaderForCustomMod`. `EditIFXSend1`/`EditIFXSend2` are
 * otherwise identical shape (`this[0x72]`/`this[0x6c]` vs
 * `this[0x82]`/`this[0x7c]`), both indexing a new real-but-unread
 * `.rodata` byte table `s_akbyAreaForIFX` by the same `+0x128`
 * "current slot" index field already established by round 49's
 * `GetSoloSelected`.
 *
 * `EditExternalKnob`/`EditExternalSlider`: write the per-channel-array
 * slot (stride `0x10`, offsets `+0xc`/`+0x8c` -- SAME array round 49
 * already confirmed) then mirror a 4-DWORD block of that slot's own
 * neighboring fields into a second array at `+0x154`/`+0x1d4` --
 * reads back its OWN just-written value as part of the mirror (ground
 * truth's real evaluation order preserved exactly: write first, then
 * mirror-copy, so the mirrored slot sees the NEW value not the old
 * one).
 *
 * `GetCurrentKarmaSceneId(int)`/`GetCurrentKarmaScene()`/
 * `InitializePlayMuteSwitchInModKarma()`/
 * `InitializeSelectSwitchInAudioInput()` all dereference the 2 new
 * real packed-pointer fields `+0x144`/`+0x148` (KARMA
 * scene/module-select state, real meaning not independently
 * recovered) purely as raw memory. `GetCurrentKarmaScene()`'s own
 * return value is itself a packed 32-bit pointer (arithmetic on the
 * `+0x144`/`+0x148` values, kept in packed-uint space throughout --
 * never converted to a host pointer -- matching how the real 32-bit
 * target computes and returns it).
 *
 * === Round 51 batch (2026-07-29, solo): 6 more methods ===
 * `InitializeSelectSwitchInAudioTrack`/`EditAudioTrackChannelStripKnob`/
 * `EditAudioTrackPan`/`EditPan`/`EditChannelStripKnob` all reuse the
 * SAME already-confirmed `+0x140` pointed-at object fields (round
 * 49/50) and, for `EditPan`/`EditChannelStripKnob`, the SAME
 * `s_akbyAreaForIFX` table (round 50) -- no new tables/fields needed.
 * `InitializeSelectSwitchInAudioTrack`'s `+0x114==0` branch reuses
 * `+0x140`'s own `+2` byte (top nibble this time, not the low nibble
 * round 49/50 used); its other branch reuses the SAME `+0x7ec`
 * indexed-pointer-array pattern `GetSoloSelected` (round 49) already
 * confirmed, reading a NEW confirmed offset (`+0x12`, a real `short`)
 * off the indexed object.
 *
 * `SetEnabledMIDITrack()` introduces one new real packed-pointer field,
 * `+0x134` (a "current track/channel context" object, real meaning not
 * independently recovered), dereferenced purely as raw memory
 * (`FromU32()` treatment) at 3 confirmed byte offsets (`+0x9fe`/
 * `+0xb29`/`+0xf45`). Landed as a direct, mechanical goto-preserving
 * translation of ground truth's own control flow (multiple named
 * labels reached from more than one branch) rather than restructured,
 * to minimize transcription risk on this one's unusually tangled
 * bitmask/branch logic.
 */

#ifndef CONTROL_SURFACE_H
#define CONTROL_SURFACE_H

class CControlSurface {
public:
	// -- (1) real no-op overrides --
	void EditKarmaPadVelocityMode(int mode);
	void UpdateKnobFaderLED();
	void UpdateKnobFaderLED(int knobFader);
	void UpdateKnobFaderLED(int knobFader, unsigned short value);
	void PressPlayMuteSwitchInExternal(int arg1, int arg2);
	void PressSelectSwitchInExternal(int arg1, int arg2);
	void PressPlayMuteSwitchInGraphicEQ(int arg1, int arg2);
	void PressSelectSwitchInGraphicEQ(int arg1, int arg2);
	void MoveKnobInGraphicEQ(int knobFader);

	// -- (2) fixed-offset field get/set --
	static unsigned int *GetSongForReset();
	void InitializeSelectSwitchInGraphicEQ();
	void InitializeSelectSwitchInExternal();
	void InitializePlayMuteSwitchInGraphicEQ();
	void InitializePlayMuteSwitchInExternal();
	void EditMasterVolume(int volume);
	unsigned int GetSoloSelected() const;
	void InitializeModKnob();
	unsigned int GetExternalSwitch(int bitIndex) const;
	unsigned int GetExternalKnobFaderMax(char arg1) const;

	// -- (3) per-channel array get/set (stride 0x10) --
	unsigned int GetExternalKnob(int index) const;
	unsigned int GetExternalSlider(int index) const;
	void EditRTKnob(int index, int value);
	void EditSetListEQBandLevel(unsigned int index, int value);
	void EditAudioVolume(int index, int value);
	void EditAudioTrackVolume(int index, int value);
	void EditVolume(int index, int value);
	void EditKnobFaderForCustomMod(unsigned int arg1, int knobFader, int value);

	// -- (4) lookup-table LED code + misc real logic --
	unsigned int GetModeLEDCode(int mode) const;
	unsigned int GetKarmaModuleSelectLEDCode(int index) const;
	bool IsUseGlobalAudio() const;

	// -- round 50 batch --
	static void SetSongForReset(const void *song);
	void SetBackupMode(int mode);
	bool ShouldSetupFaderAsReverse(int knobFader, int algorithm, unsigned char value) const;
	void EditAudioChannelStripKnob(unsigned int arg1, int value, int knobFader);
	void EditIFXSend1(int arg1, int value);
	void EditIFXSend2(int arg1, int value);
	void EditExternalKnob(int index, int value);
	void EditExternalSlider(int index, int value);
	unsigned char GetCurrentKarmaSceneId(int arg1) const;
	void InitializeSelectSwitchInAudioInput();
	void EditAudioPan(int index, int value);
	unsigned int GetCurrentKarmaScene() const;
	void InitializePlayMuteSwitchInModKarma();

	// -- round 51 batch --
	void InitializeSelectSwitchInAudioTrack();
	void EditAudioTrackChannelStripKnob(unsigned int arg1, int value, int knobFader);
	void EditAudioTrackPan(int index, int value);
	void EditPan(int index, int value);
	void EditChannelStripKnob(unsigned int arg1, int value, int knobFader);
	void SetEnabledMIDITrack();
};

#endif /* CONTROL_SURFACE_H */
