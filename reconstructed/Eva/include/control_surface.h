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
 * === Deferred, 3 reasons (131/161 methods) ===
 * (1) Calls into a real but wholly unreconstructed sibling class:
 *     `SetAsSoloSelected`/`GetSolo`/`GetAudioTrackSolo`/
 *     `GetAudioInputSolo` (`CTrackStatus::SetAsSoloSelected`/`GetSolo`),
 *     `UpdateLED`/`EditAssignableSwitch` (`CMMI::GetInstance`/`SetLED`).
 * (2) Calls into an unreconstructed SIBLING `CControlSurface` method:
 *     `UpdateKarmaSceneSelectLED` (calls `UpdatePlayMuteSwitchLED`, not
 *     yet landed -- deferred together for accuracy rather than guessing
 *     at the sibling's real behavior).
 * (3) Everything else not surveyed this round (~127 methods, up to
 *     several KB) -- a dedicated future round's scope, same discipline
 *     as every other oversized class in this project.
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
};

#endif /* CONTROL_SURFACE_H */
