// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PAN_OUTPUT_BASE_H
#define OA_STG_PAN_OUTPUT_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pan_output_base.h  -  CSTGPanOutputBase's value-getter family:
 * all 9 real weak-symbol ctx-only candidates reconstructed -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPanOutputBase is the STG pan/output-mixer patch component -- Pan,
 * PatchLevel, Send1/Send2Level, Mute, Solo, per its own method names
 * below -- confirmed genuinely fresh via a word-boundary grep before
 * starting, no pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods despite the class owning a per-call ctx argument.
 *
 * 2 pending symbols excluded up front, not fed to the decoder:
 * GetVoiceLevelEstimate(CSTGVoice const&) is real weak linkage but a
 * different signature entirely -- disassembly confirms it is the same
 * per-voice-runtime-state accessor shape as CSTGOrganOsc's own
 * GetVoiceLevelEstimate, not a static patch-value accessor, despite the
 * matching class-name prefix. SetMute(CSTGPatchMessageContext&, bool) is
 * global ('T') linkage with an extra bool argument beyond ctx -- the
 * standard extra-args exclusion.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue -- Pan,
 *     PatchLevel, Send1Level, Send2Level, PanAMSIntensity (this one is a
 *     fixed dword despite the "AMSIntensity" naming, not ctx-indexed).
 *   - Plain 8-bit signed field: single-writes .value only --
 *     PanAMSSource.
 *   - Single-bit boolean packed into byte 0x21: PanUseDrumkitSetting is
 *     bit 0 (no shift, `movzx` + `and eax,1`), PatchMute is bit 1
 *     (`shr al,1` + `and eax,1`) -- both single-write, the by-now-usual
 *     shift-then-mask bitfield shape.
 *   - GetPatchSolo is a NEW shape for the family: no field read at all,
 *     it unconditionally stores the literal 0 into .value (single-write,
 *     no .displayValue) and returns sValueGetterTemp -- `this` is never
 *     dereferenced. A hardcoded-constant getter, presumably a
 *     not-yet-wired-up patch-solo feature at this level (solo tracked
 *     elsewhere, e.g. per-program or per-timbre) rather than a real
 *     per-instance field. Modeled as a plain literal assignment.
 * No exceptions to the width-vs-dual-write rule found among the real
 * field-backed methods in this class.
 */

struct CSTGPanOutputBase {
	STGConvertedParam &GetPanUseDrumkitSetting(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPanAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPanAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPan(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSend1Level(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSend2Level(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPatchLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPatchMute(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPatchSolo(CSTGPatchMessageContext &ctx);
};

#endif
