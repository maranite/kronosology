// SPDX-License-Identifier: GPL-2.0
/*
 * oa_kg_param_edit_rt_externs.h  -  extern RT_ / KS_ declarations needed by
 * src/engine/kg_param_edit.cpp's real CKGParamEdit method bodies (round 52,
 * solo, 2026-07-29). See oa_ckg_module_param_msg_handler.h's struct
 * CKGParamEdit for the class itself (this round adds real bodies for 101 of
 * its already-declared-but-unimplemented methods, plus a new ctor/
 * GetPadChangeSource/SendLocalControlOn) -- this header is just the free-
 * function surface those bodies call into.
 *
 * FOUND via a fresh manifest survey filtered to pending methods with no
 * in_stack_ffffffXX/unaff_/"Could not recover" warnings, sorted by class
 * average size. Every one of CKGParamEdit's `SendXxx()` methods is a thin
 * relay: forward its own arguments straight to an already-named `RT_*`/
 * `KS_*` free function (many already declared as extern-only callees of
 * gRTParmFunctionTable_PE/GE, oa_rtparm_pe_table.h/oa_rtparm_ge_table.h --
 * this is the "real caller" those tables' own header comments had flagged
 * as a TODO: "verify meaning once a real caller ... is reconstructed").
 *
 * 101/132 CKGParamEdit methods landed this round. Deferred, 2 distinct
 * reasons:
 * (1) 21 methods with genuine in_stack_/unaff_/jumptable-recovery blockers
 *     (SendNoteMapTable/SendNoteMapTableReset/ForceSendOnOff/SendOnOff/
 *     SendAssignableSwitch/SendScene/SendBufferSelect/
 *     RestoreChordMemoryAndNotifyToUI/SendChordMemory/SendKnob/
 *     SendRTPParamAssign/SendChordMemChannel/SendInputCh/SendOutputCh/
 *     SendRxOther/SendTimbreThru/SendStartSeed[the 164B 3-arg variant --
 *     already declared in oa_ckg_param_edit_send_decls.inc, a DIFFERENT
 *     address than this round's own clean 2-arg SendStartSeed overload
 *     below]/SendUseGChAlso/SendGlobalTranspose/SendGlobalMIDICh/
 *     SendForceKarmaOff) -- same class of blocker as every other project
 *     deferral, not attempted.
 * (2) 10 "ForModuleControl"-adjacent GE methods (SendGEValue/
 *     SendGEDestKnob/SendGEPolarity/SendGEMinValue/SendGEMaxValue/
 *     SendGEValueForModuleControl/SendGEDestKnobForModuleControl/
 *     SendGEPolarityForModuleControl/SendGEMinValueForModuleControl/
 *     SendGEMaxValueForModuleControl) -- fully concrete, NO decompiler
 *     warning, but each writes through 2 unconfirmed `.data`/`.bss` symbols
 *     this pass has no independent confirmation for (`CSWTCH_69`, a 4-entry
 *     lookup table, and one of 2 giant per-model front-panel-variable
 *     arrays -- `STGOrganModelFrontVars_mDelayLine776`/
 *     `STGPluckedModelFeedbackVars_mDelayLine1926`, whose own names are
 *     clearly Ghidra's best-guess label for a much larger unrelated
 *     structure, not a real confirmed symbol) -- left undeclared/uncredited
 *     rather than guessed at, a genuinely different blocker than (1).
 *
 * === "this" is not always a real object pointer here ===
 * Several methods' own ground-truth decompile shows `this` (delivered in
 * EAX per this project's established regparm/thiscall-EAX convention, same
 * as oa_ckg_module_param_msg_handler.h and oa_stg_key_track.h's own header
 * comments) being cast DIRECTLY to a value and passed as the call's OWN
 * 3rd/4th real argument (`(char)this`, `(uchar)this`) -- never dereferenced.
 * Translated faithfully by using the reconstructed method's own LAST
 * explicit parameter in that slot instead of a real `this` read (e.g.
 * SendChordMemNote/SendChordMemVelocity/SendCCSource/SendCCValue/
 * SendEnvTrigMode/SendEnvLatchMode/SendModCutOff/SendDynModule/
 * SendRTPModule).
 *
 * === signature reconciliation vs the pre-existing caller-inferred decls ===
 * oa_ckg_param_edit_send_decls.inc's signatures were inferred from EARLIER
 * rounds' own callers (CKGModuleParamMsgHandler/CKGCommonParamMsgHandler/
 * CKGGlobalParamMsgHandler/CKGControlMsgHandler/the CKGSwitch family), at a
 * time when CKGParamEdit's own bodies were unrecovered. Where this round's
 * fresh ground-truth read of the CALLEE side showed a narrower/different
 * type than the caller-inferred one (e.g. SendChordMemNote's own real
 * prototype takes `int, int, int` even though only the low byte of each is
 * ever meaningful, matching the already-declared signature exactly), the
 * EXISTING declared signature always wins (it's what already-compiled
 * callers rely on) -- bodies were written to match it, not the other way
 * around. No signature was changed by this round.
 */

#ifndef OA_KG_PARAM_EDIT_RT_EXTERNS_H
#define OA_KG_PARAM_EDIT_RT_EXTERNS_H

#include "oa_rtparm_pe_table.h"              /* reuses ~25 already-declared RT_* externs */
#include "oa_ckg_control_ui_msg.h"            /* CKGRTCHandler::ms_poInstance */

/* ---- new RT_ / KS_ externs this round calls, not already declared by any
 * sibling table header. Same "declared extern only, not defined here"
 * convention as oa_rtparm_pe_table.h's own 46-callee block. */
extern "C++" {

void RT_rtc_qtz_window(unsigned int) __attribute__((regparm(3)));
void RT_dt_run(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_oct_replicate(bool) __attribute__((regparm(3)));
void RT_chord_assign(bool) __attribute__((regparm(3)));
void RT_chord_vel_mode(unsigned char) __attribute__((regparm(3)));
void RT_rpp_bank_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpp_min(unsigned char, short) __attribute__((regparm(3)));
void RT_rpp_max(unsigned char, short) __attribute__((regparm(3)));
void RT_rpp_pe_edit(unsigned char, short) __attribute__((regparm(3)));
unsigned long long RT_rpp_module(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpp_pe_control(unsigned char, char, char) __attribute__((regparm(3)));
void RT_rpp_pe_polarity(unsigned char, char) __attribute__((regparm(3)));
void RT_dyn_src_zone(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_src_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_dst_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_src_action(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_src_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_src_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_dst_module(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dyn_dst_polarity(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_chord_trigger_nte(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_chord_trigger_vel(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_solo(unsigned char, unsigned char) __attribute__((regparm(3)));
/* RT_midi_filt_in_tch/bnd/sus/cc1/cc2 NOT re-declared here -- already
 * extern "C" in oa_ckg_module_param_msg_handler.h's own ~50-callee
 * CKGEngine externs block (this header's own includer always includes
 * that one first, per kg_param_edit.cpp). RT_midi_filt_in_cc16 is
 * genuinely new (SendRxRibbon's own real callee, a DIFFERENT name from
 * that block's unrelated RT_midi_filt_in_ctl). */
void RT_midi_filt_in_cc16(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_bnd(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_cca(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_ccb(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_env(unsigned char, char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_notes(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_midi_filt_out_wav(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nbo_interp(unsigned char, unsigned char) __attribute__((regparm(3)));
/* RT_qtz_on/RT_qtz_window: NOT re-declared here -- already in
 * oa_rtparm_pe_table.h (transitively included, see above), same
 * (unsigned char, unsigned char) signature. */
void RT_cc_offset_num(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_cc_offset_val(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_env_trig_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_latch_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_trigger_cutoff(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_seed_offset(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dt_link_to_switch(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dt_switch(bool) __attribute__((regparm(3)));
void RT_dt_switch_mode(unsigned char) __attribute__((regparm(3)));
void RT_rtc_scene_link(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rtc_master_enable(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_latch_state(unsigned char) __attribute__((regparm(3)));
void KGOutGate_RenewDrumTrackLatchStatus();
/* void* target: the real 2nd arg is CKarmaModuleControl*, that class's own
 * layout is unmodeled/out of scope -- every caller here only ever computes
 * the pointer via raw offset arithmetic into CKGBankManager::ms_poInstance
 * and never dereferences the callee's write-back, so a void* is sufficient
 * and faithful (same "opaque pointer, callee internals unmodeled" license
 * as several other cross-class calls project-wide). */
void KS_get_scene_matrix(unsigned char, void *) __attribute__((regparm(3)));

} /* extern "C++" */

/* SendFF/SendRewind's own debug-counter globals -- real, plain ints,
 * genuinely never touched anywhere else in this reconstruction. */
extern int g_iDebugCondition;
extern int gTestTotalBytes;
extern int gTestTotalMsgs;

#endif /* OA_KG_PARAM_EDIT_RT_EXTERNS_H */
