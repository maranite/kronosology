// SPDX-License-Identifier: GPL-2.0
/*
 * oa_rtparm_ge_table.h  -  gRTParmFunctionTable_GE, the KARMA real-time-
 * parameter (RTParm) dispatch table for the "GE" (per-module Generic
 * Effect / Global Element -- exact scope unconfirmed, see
 * src/engine/rtparm_ge_table.cpp) parameter domain.
 *
 * ground truth: InitializegRTParmFunctionTable_GE(), .text+0x56b18d,
 * 34435 bytes, cdecl, no args. A single straight-line run of 3756
 * `mov [gRTParmFunctionTable_GE+off], value` stores (zero branches,
 * zero calls) populating a 313-entry, 0x28(40)-byte-stride array in
 * .bss (gRTParmFunctionTable_GE, .bss+0x630f40, size 0x30e8 ==
 * 313*0x28 exactly). Reconstructed via a scripted decoder (see
 * decoder notes in the project memory, "ckg-rtparm-ge-function-table"
 * batch) rather than hand transcription -- confirmed self-consistent:
 * 313 entries * 12 field-writes/entry == 3756 raw instructions parsed
 * with 0 leftover/misaligned writes, and the function's own byte size
 * (34435) is independently reproduced by summing each write-opcode's
 * encoded length (2 dword + 1 word + 3 byte + 6 dword writes per
 * entry = 110 bytes/entry * 313 + 5-byte prologue/epilogue = 34435).
 *
 * Sibling table: gRTParmFunctionTable_PE / InitializegRTParmFunctionTable_PE
 * (ground-truth offset 0x573810, 5505 bytes, 50 entries) is the SAME record
 * shape at a smaller scale -- NOT reconstructed by this pass, left pending.
 *
 * Field semantics: only funcPtr (always a real, unique RT_* function
 * pointer, one per entry, 313/313 confirmed distinct) and index (a
 * redundant self-index, index==entry position for all 313 entries --
 * confirmed, presumably an authoring-time sanity/documentation field in
 * the original X-macro-style table) are confirmed by structural
 * evidence. The remaining numeric fields' exact purpose (parameter
 * min/max/default/UI-resource-ID -- consistent with this being an
 * RT-parameter-descriptor table, same family as the CKGEngine PERT/GERT
 * work) is NOT independently confirmed by this pass -- see field
 * comments below and TODOs. field1c/field20/field24 are 0 in all 313
 * entries (confirmed) -- reserved/unused in this table instance.
 *
 * The 313 RT_* callees are real, separate, already-defined functions
 * elsewhere in ground-truth OA.ko (confirmed via symtab cross-check --
 * all 313 resolve to GLOBAL FUNC symbols with real non-zero sizes, none
 * are external/undefined) but are NOT reconstructed by this pass -- all
 * 313 remain `pending` in the manifest, a separate large follow-up
 * family. Declared here as `extern` only, matching this project's
 * standing convention for referencing not-yet-reconstructed OA.ko-
 * internal symbols (they show up as expected "Unknown symbol" in a real
 * Kbuild modpost, same as the ~18000 other pending symbols).
 *
 * Caller: BirthOfKarma() (ground-truth offset 0x507d0c, 7177 bytes, still
 * `pending` in the manifest -- see src/engine/ckg_engine.cpp for other
 * already-reconstructed BirthOfKarma-adjacent pieces) calls
 * InitializegRTParmFunctionTable_GE() immediately followed by
 * InitializegRTParmFunctionTable_PE(), both guarded by the same
 * `test edx,edx; je ...` one-time-init check at ground-truth offset
 * 0x4f8597 -- confirmed real, on the live KARMA-engine-startup path, not
 * dead code.
 */

#ifndef OA_RTPARM_GE_TABLE_H
#define OA_RTPARM_GE_TABLE_H

#define RTPARM_GE_TABLE_SIZE 313

/* Buffer-select discriminator used by several RT_*_template()/
 * RT_*_template_restore() callees (4th argument). Only the TYPE identity
 * (name + being an enum) is confirmed -- it must mangle as
 * "18RTParmBufferSelect" to match ground truth's _Z...18RTParmBufferSelect
 * symbols, which it does (Itanium ABI: enums and classes both mangle as
 * <len><name>, independent of underlying type). Enumerator NAMES/VALUES
 * below are a reasonable inference from CKGEngine::ResetKRTCSlider()'s
 * own comment ("select for a slider is always RTParmBufferSelect literal
 * 1" -- see src/engine/ckg_engine.cpp), NOT independently confirmed --
 * TODO verify against a real caller once one of the 313 RT_* functions
 * using it is reconstructed. */
enum RTParmBufferSelect {
	RTPARM_BUFFER_CURRENT = 0,
	RTPARM_BUFFER_COMPARE = 1,
};

/* One gRTParmFunctionTable_GE record, 0x28 (40) bytes, matching ground
 * truth's exact write pattern byte-for-byte (verified: sizeof() below
 * must come out to 40 with natural x86-32 alignment, no attribute
 * needed -- the trailing 6 dwords are already 4-aligned after the 3
 * explicit padding bytes). */
struct RTParmFunctionTableEntry_GE
{
	void *funcPtr;          /* +0x00  target RT_* handler for this parameter */
	unsigned int field04;    /* +0x04  TODO: unconfirmed (0..45 observed, mostly 0) */
	unsigned short index;    /* +0x08  self-index; == entry position for all 313 entries */
	unsigned char field0a;   /* +0x0a  TODO: unconfirmed (1..15 observed) */
	unsigned char field0b;   /* +0x0b  TODO: unconfirmed (0..45 observed) */
	unsigned char field0c;   /* +0x0c  boolean-shaped, only 0 or 1 observed */
	unsigned char _pad_0d[3]; /* +0x0d..0x0f  never written in ground truth */
	unsigned int field10;    /* +0x10  TODO: unconfirmed (10..27786 observed) */
	unsigned int field14;    /* +0x14  TODO: unconfirmed (0..27435 observed, 169/313 nonzero) */
	unsigned int field18;    /* +0x18  TODO: unconfirmed (0..27643 observed, nonzero only for a contiguous 72-entry block, indices 171.. -- likely a WAV-parameter-specific field) */
	unsigned int field1c;    /* +0x1c  0 in all 313 entries */
	unsigned int field20;    /* +0x20  0 in all 313 entries */
	unsigned int field24;    /* +0x24  0 in all 313 entries */
};

extern RTParmFunctionTableEntry_GE gRTParmFunctionTable_GE[RTPARM_GE_TABLE_SIZE];

void InitializegRTParmFunctionTable_GE();

/* ---- the 313 RT_* callees themselves (all real, all pending -- see
 * header comment above). Declared here only so gRTParmFunctionTable_GE's
 * own initializer can take their address; NOT defined by this file. */
extern "C++" {

void RT_bnd_amt(unsigned char, char) __attribute__((regparm(3)));
void RT_bnd_dir(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_end(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_val(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_ge_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_dir(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_vol(unsigned char, char) __attribute__((regparm(3)));
void RT_vel_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_con(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_step(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_type(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_cc_a(unsigned char, char) __attribute__((regparm(3)));
void RT_pan_cc_b(unsigned char, char) __attribute__((regparm(3)));
void RT_rhy_mult(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_rpt_reps(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_type(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_range(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_shape(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_start(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_width(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clu_strum(unsigned char, short) __attribute__((regparm(3)));
void RT_crb_table(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dix_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_0(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_1(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_2(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_3(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_4(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_5(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_vel_6(unsigned char, unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_pan_fixed(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_0(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_1(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_2(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_3(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_4(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_5(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_6(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_7(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_8(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_9(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_start(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_total(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_xpose(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_rpt_decay(unsigned char, char) __attribute__((regparm(3)));
void RT_rpt_table(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_tlock(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_vel_scale(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_bnd_dix_on(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_ntt_on(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_pat_on(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_ge_gate_cc(unsigned char, char) __attribute__((regparm(3)));
void RT_phs_events(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_10(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_11(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_12(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_13(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_14(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_15(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_dix_on(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_offset(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_pat_on(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_amt(unsigned char, char) __attribute__((regparm(3)));
void RT_bnd_dix_end(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_dix_end(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_link_on(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_dix_val(unsigned char, short) __attribute__((regparm(3)));
void RT_env_amp_amt(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_pat_dbl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_pat_inv(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_pat_inv(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tmp_1_4(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tmp_5_8(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tmp_all(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_dur_val(unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_0(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_1(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_2(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_3(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_4(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_5(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_6(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_7(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_8(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_9(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_bnd_alt_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_step(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_drm_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_key_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clu_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_crb_interval(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_nte_type(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_sort_dir(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_symmetry(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_wrap_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_wrap_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_0(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_1(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_2(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_3(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_4(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_5(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_choice_6(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_mult_str(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_pat_poly(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_rhy_mult(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_drm_wrap_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_wrap_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_dix_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_pat_type(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_end_loop_phs(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_all_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_all_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_dec_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_rel_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sus_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_ge_gate_type(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_pat_type(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_pan_pat_type(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_phs_tmp_9_12(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tsig_div(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tsig_num(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rel_dly_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_mult_str(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_rpt_dur_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_key_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_wrap_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_wrap_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_rand_bot(unsigned char, char) __attribute__((regparm(3)));
void RT_vel_rand_top(unsigned char, char) __attribute__((regparm(3)));
void RT_vel_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_wav_osc_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_sound_10(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_11(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_12(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_13(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_14(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_sound_15(unsigned char, unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_wav_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_bnd_dix_shape(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_start(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_width(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_length_ms(unsigned char, short) __attribute__((regparm(3)));
void RT_clu_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_items_max(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_pat_xpose(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_template0(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_template1(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_template2(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_track_kbd(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_vel_scale(unsigned char, unsigned char, short) __attribute__((regparm(3)));
void RT_dur_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_end_loop_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_end_loop_size(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_loop_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_tempo_rel(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_ge_force_mono(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_ntt_table(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_pat_items(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_tmp_13_16(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_xpose_oct(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_rhy_swing_amt(unsigned char, short) __attribute__((regparm(3)));
void RT_rhy_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_chord_qtz(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_damp_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_size_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_use_swing(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_wrap_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_kbd_track(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_force_zero(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clu_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_crb_inv_offset(unsigned char, char) __attribute__((regparm(3)));
void RT_crb_open_voice(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_skip_dupes(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dix_vel_bottom(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dix_vel_offset(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_use_length(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_vel_offset(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_dur_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_env_time_scale(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_drunk_step(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_pan_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_phs_beg_offset(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_cycle_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_end_offset(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_start_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_rpt_range_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_rhythm_dot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_rhythm_reg(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_rhythm_sel(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_vel_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_wav_pat_length(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_bnd_length_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_input_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_drm_on_off_comb(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_nte_trig_on(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_pat_dbl_amt(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_length_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_humanize_ms(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_rhythm_sel2(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_rhythm_trip(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_0(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_1(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_2(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_3(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_4(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_5(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_6(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_7(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_8(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_9(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_alt_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_filter_fixed(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_replications(unsigned char, short) __attribute__((regparm(3)));
void RT_drm_mult_str_trp(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_notes_played(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_dix_ctl_type(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_use_rhy_mult(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_dec_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_rel_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_att_sus_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_dec_rel_time(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_restart_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_att_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_sus_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sus_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_clu_adv_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_clu_adv_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_use_poffsets(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_mult_str_trp(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_wrap_bot_rel(unsigned char, char) __attribute__((regparm(3)));
void RT_rpt_wrap_top_rel(unsigned char, char) __attribute__((regparm(3)));
void RT_vel_clu_adv_mode(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_10(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_11(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_12(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_13(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_14(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_wav_start_off_15(unsigned char, unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_dix_length_ms(unsigned char, short) __attribute__((regparm(3)));
void RT_bnd_vel_range_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_bnd_vel_range_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_pat_xpose_oct(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_dur_tie_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_step_xpose_on(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_xpose_oct_5th(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_rel_dly_damp_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_tie_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_vel_range_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rpt_vel_range_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dix_key_trill_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_rest_tab_curve(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_tie_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_pan_poct_tab_curve(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_poff_tab_curve(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_phs_step_xpose_tmp(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_tie_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_rpt_time_offset_ms(unsigned char, char) __attribute__((regparm(3)));
void RT_bnd_dix_length_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_filter_template(unsigned char, char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_rest_tab_weight(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_nte_pat_dbl_vel_off(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pan_poct_tab_weight(unsigned char, char) __attribute__((regparm(3)));
void RT_pan_poff_tab_weight(unsigned char, char) __attribute__((regparm(3)));
void RT_rhy_swing_mult_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_swing_note_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clu_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_mult_str_trp_dot(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_dur_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_env_att_sus_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_att_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_att_sus_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_sta_sus_rel_levl(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_pan_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_rhy_mult_str_trp_dot(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_rhy_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_vel_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_wav_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_crb_filt_temp_restore(unsigned char, char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_dix_bad_nte_trig_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_pat_xpose_oct_5th(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_drm_template0_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_template1_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_drm_template2_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_rhy_swing_amt_special(unsigned char, short) __attribute__((regparm(3)));
void RT_drm_phs_rpt_on_off_pat(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_phs_rpt_on_off_comb(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_drm_phs_rsy_temp_restore(unsigned char, unsigned char, char, RTParmBufferSelect) __attribute__((regparm(3)));
void RT_bnd_on(unsigned char, unsigned char) __attribute__((regparm(3)));

} /* extern "C++" */

#endif /* OA_RTPARM_GE_TABLE_H */
