// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_ge_func_stubs.cpp  -  TEST-ONLY empty-body stand-ins for the
 * 313 RT_* KARMA parameter-handler functions gRTParmFunctionTable_GE
 * (see include/oa_rtparm_ge_table.h) stores pointers to.
 *
 * These functions are REAL and defined elsewhere in ground-truth OA.ko
 * -- they are simply not yet reconstructed (all 313 remain `pending` in
 * the manifest, a separate large follow-up family). This file exists
 * ONLY so verify/test_rtparm_ge_table.cpp can fully link as a native
 * host executable and compare `gRTParmFunctionTable_GE[i].funcPtr`
 * against `&RT_xxx` by identity -- it is NEVER linked into OA.ko itself
 * (not part of OA-objs/SRC in the Makefile), matching the project's
 * "clearly-labeled interop stub, not a claim of real behavior"
 * convention already used by src/stub/bar2_stubs*.cpp.
 */

#include "oa_rtparm_ge_table.h"

void RT_bnd_amt(unsigned char, char) { }
void RT_bnd_dir(unsigned char, unsigned char) { }
void RT_bnd_end(unsigned char, unsigned char) { }
void RT_dur_val(unsigned char, unsigned char, short) { }
void RT_ge_mode(unsigned char, unsigned char) { }
void RT_phs_dir(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_vol(unsigned char, char) { }
void RT_vel_bot(unsigned char, unsigned char) { }
void RT_vel_con(unsigned char, unsigned char) { }
void RT_vel_top(unsigned char, unsigned char) { }
void RT_bnd_step(unsigned char, unsigned char) { }
void RT_env_mode(unsigned char, unsigned char, unsigned char) { }
void RT_env_type(unsigned char, unsigned char, unsigned char) { }
void RT_pan_cc_a(unsigned char, char) { }
void RT_pan_cc_b(unsigned char, char) { }
void RT_rhy_mult(unsigned char, unsigned char, short) { }
void RT_rpt_reps(unsigned char, unsigned char) { }
void RT_vel_mode(unsigned char, unsigned char) { }
void RT_wav_type(unsigned char, unsigned char, unsigned char) { }
void RT_bnd_range(unsigned char, unsigned char) { }
void RT_bnd_shape(unsigned char, unsigned char) { }
void RT_bnd_start(unsigned char, unsigned char) { }
void RT_bnd_width(unsigned char, unsigned char) { }
void RT_clu_strum(unsigned char, short) { }
void RT_crb_table(unsigned char, unsigned char) { }
void RT_dix_xpose(unsigned char, char) { }
void RT_drm_vel_0(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_1(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_2(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_3(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_4(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_5(unsigned char, unsigned char, unsigned char, char) { }
void RT_drm_vel_6(unsigned char, unsigned char, unsigned char, char) { }
void RT_pan_fixed(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_0(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_1(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_2(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_3(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_4(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_5(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_6(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_7(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_8(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_9(unsigned char, unsigned char, unsigned char) { }
void RT_phs_start(unsigned char, unsigned char) { }
void RT_phs_total(unsigned char, unsigned char) { }
void RT_phs_xpose(unsigned char, unsigned char, char) { }
void RT_rpt_decay(unsigned char, char) { }
void RT_rpt_table(unsigned char, unsigned char) { }
void RT_rpt_tlock(unsigned char, unsigned char) { }
void RT_rpt_xpose(unsigned char, char) { }
void RT_vel_scale(unsigned char, unsigned char, short) { }
void RT_bnd_dix_on(unsigned char, unsigned char) { }
void RT_drm_ntt_on(unsigned char, unsigned char, unsigned char) { }
void RT_drm_pat_on(unsigned char, unsigned char, unsigned char) { }
void RT_ge_gate_cc(unsigned char, char) { }
void RT_phs_events(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_10(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_11(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_12(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_13(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_14(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_15(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_dix_on(unsigned char, unsigned char) { }
void RT_wav_offset(unsigned char, unsigned char, short) { }
void RT_wav_pat_on(unsigned char, unsigned char) { }
void RT_bnd_dix_amt(unsigned char, char) { }
void RT_bnd_dix_end(unsigned char, unsigned char) { }
void RT_crb_dix_end(unsigned char, unsigned char) { }
void RT_drm_link_on(unsigned char, unsigned char, unsigned char) { }
void RT_dur_dix_val(unsigned char, short) { }
void RT_env_amp_amt(unsigned char, unsigned char, unsigned char) { }
void RT_nte_pat_dbl(unsigned char, unsigned char, unsigned char) { }
void RT_nte_pat_inv(unsigned char, unsigned char, unsigned char) { }
void RT_pan_pat_inv(unsigned char, unsigned char, unsigned char) { }
void RT_phs_tmp_1_4(unsigned char, unsigned char, unsigned char) { }
void RT_phs_tmp_5_8(unsigned char, unsigned char, unsigned char) { }
void RT_phs_tmp_all(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_dur_val(unsigned char, short) { }
void RT_wav_sound_0(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_1(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_2(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_3(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_4(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_5(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_6(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_7(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_8(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_9(unsigned char, unsigned char, unsigned char, short) { }
void RT_bnd_alt_mode(unsigned char, unsigned char) { }
void RT_bnd_dix_step(unsigned char, unsigned char) { }
void RT_bnd_drm_mode(unsigned char, unsigned char) { }
void RT_bnd_key_mode(unsigned char, unsigned char) { }
void RT_clu_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_crb_interval(unsigned char, unsigned char) { }
void RT_crb_nte_type(unsigned char, unsigned char) { }
void RT_crb_sort_dir(unsigned char, unsigned char) { }
void RT_crb_symmetry(unsigned char, unsigned char) { }
void RT_crb_wrap_bot(unsigned char, unsigned char) { }
void RT_crb_wrap_top(unsigned char, unsigned char) { }
void RT_drm_choice_0(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_1(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_2(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_3(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_4(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_5(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_choice_6(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_drm_mult_str(unsigned char, unsigned char, unsigned char) { }
void RT_drm_pat_poly(unsigned char, unsigned char, unsigned char) { }
void RT_drm_rhy_mult(unsigned char, unsigned char, short) { }
void RT_drm_wrap_bot(unsigned char, unsigned char) { }
void RT_drm_wrap_top(unsigned char, unsigned char) { }
void RT_dur_dix_mode(unsigned char, unsigned char) { }
void RT_dur_pat_type(unsigned char, unsigned char, unsigned char) { }
void RT_dur_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_end_loop_phs(unsigned char, unsigned char) { }
void RT_env_all_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_all_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_dec_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_rel_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sus_levl(unsigned char, unsigned char, unsigned char) { }
void RT_ge_gate_type(unsigned char, unsigned char) { }
void RT_nte_pat_type(unsigned char, unsigned char, unsigned char) { }
void RT_nte_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_pan_pat_type(unsigned char, unsigned char, unsigned char) { }
void RT_pan_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_phs_tmp_9_12(unsigned char, unsigned char, unsigned char) { }
void RT_phs_tsig_div(unsigned char, unsigned char, unsigned char) { }
void RT_phs_tsig_num(unsigned char, unsigned char, unsigned char) { }
void RT_rel_dly_menu(unsigned char, unsigned char) { }
void RT_rhy_mult_str(unsigned char, unsigned char, unsigned char) { }
void RT_rhy_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_rpt_dur_mode(unsigned char, unsigned char) { }
void RT_rpt_key_mode(unsigned char, unsigned char) { }
void RT_rpt_wrap_bot(unsigned char, unsigned char) { }
void RT_rpt_wrap_top(unsigned char, unsigned char) { }
void RT_vel_rand_bot(unsigned char, char) { }
void RT_vel_rand_top(unsigned char, char) { }
void RT_vel_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_wav_osc_mode(unsigned char, unsigned char) { }
void RT_wav_sound_10(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_11(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_12(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_13(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_14(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_sound_15(unsigned char, unsigned char, unsigned char, short) { }
void RT_wav_template(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_bnd_dix_shape(unsigned char, unsigned char) { }
void RT_bnd_dix_start(unsigned char, unsigned char) { }
void RT_bnd_dix_width(unsigned char, unsigned char) { }
void RT_bnd_length_ms(unsigned char, short) { }
void RT_clu_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_crb_items_max(unsigned char, unsigned char) { }
void RT_drm_pat_xpose(unsigned char, unsigned char, char) { }
void RT_drm_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_drm_template0(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_template1(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_template2(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_track_kbd(unsigned char, unsigned char, unsigned char) { }
void RT_drm_vel_scale(unsigned char, unsigned char, short) { }
void RT_dur_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_end_loop_mode(unsigned char, unsigned char) { }
void RT_end_loop_size(unsigned char, unsigned char) { }
void RT_env_loop_mode(unsigned char, unsigned char, unsigned char) { }
void RT_env_tempo_rel(unsigned char, unsigned char, unsigned char) { }
void RT_ge_force_mono(unsigned char, unsigned char) { }
void RT_nte_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_pan_ntt_table(unsigned char, unsigned char) { }
void RT_pan_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_phs_pat_items(unsigned char, unsigned char) { }
void RT_phs_tmp_13_16(unsigned char, unsigned char, unsigned char) { }
void RT_phs_xpose_oct(unsigned char, unsigned char, char) { }
void RT_rhy_swing_amt(unsigned char, short) { }
void RT_rhy_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_chord_qtz(unsigned char, unsigned char) { }
void RT_rpt_damp_mode(unsigned char, unsigned char) { }
void RT_rpt_size_menu(unsigned char, unsigned char) { }
void RT_rpt_use_swing(unsigned char, unsigned char) { }
void RT_rpt_wrap_mode(unsigned char, unsigned char) { }
void RT_vel_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_wav_kbd_track(unsigned char, unsigned char, unsigned char) { }
void RT_wav_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_bnd_force_zero(unsigned char, unsigned char) { }
void RT_clu_tab_weight(unsigned char, unsigned char, char) { }
void RT_crb_inv_offset(unsigned char, char) { }
void RT_crb_open_voice(unsigned char, unsigned char) { }
void RT_crb_skip_dupes(unsigned char, unsigned char) { }
void RT_dix_vel_bottom(unsigned char, unsigned char) { }
void RT_dix_vel_offset(unsigned char, unsigned char) { }
void RT_drm_tab_weight(unsigned char, unsigned char, char) { }
void RT_drm_use_length(unsigned char, unsigned char, unsigned char) { }
void RT_drm_vel_offset(unsigned char, unsigned char, char) { }
void RT_dur_tab_weight(unsigned char, unsigned char, char) { }
void RT_env_time_scale(unsigned char, unsigned char, unsigned char) { }
void RT_nte_drunk_step(unsigned char, unsigned char, unsigned char) { }
void RT_nte_tab_weight(unsigned char, unsigned char, char) { }
void RT_pan_tab_weight(unsigned char, unsigned char, char) { }
void RT_phs_beg_offset(unsigned char, unsigned char, unsigned char) { }
void RT_phs_cycle_mode(unsigned char, unsigned char) { }
void RT_phs_end_offset(unsigned char, unsigned char, unsigned char) { }
void RT_phs_start_mode(unsigned char, unsigned char) { }
void RT_rhy_tab_weight(unsigned char, unsigned char, char) { }
void RT_rpt_range_mode(unsigned char, unsigned char) { }
void RT_rpt_rhythm_dot(unsigned char, unsigned char) { }
void RT_rpt_rhythm_reg(unsigned char, unsigned char) { }
void RT_rpt_rhythm_sel(unsigned char, unsigned char) { }
void RT_vel_tab_weight(unsigned char, unsigned char, char) { }
void RT_wav_pat_length(unsigned char, unsigned char, unsigned char) { }
void RT_wav_tab_weight(unsigned char, unsigned char, char) { }
void RT_bnd_length_menu(unsigned char, unsigned char) { }
void RT_crb_input_xpose(unsigned char, char) { }
void RT_drm_on_off_comb(unsigned char, unsigned char) { }
void RT_env_nte_trig_on(unsigned char, unsigned char, unsigned char) { }
void RT_nte_pat_dbl_amt(unsigned char, unsigned char, unsigned char) { }
void RT_phs_length_mode(unsigned char, unsigned char) { }
void RT_rhy_humanize_ms(unsigned char, unsigned char) { }
void RT_rpt_rhythm_sel2(unsigned char, unsigned char) { }
void RT_rpt_rhythm_trip(unsigned char, unsigned char) { }
void RT_wav_start_off_0(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_1(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_2(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_3(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_4(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_5(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_6(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_7(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_8(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_9(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_bnd_dix_alt_mode(unsigned char, unsigned char) { }
void RT_crb_filter_fixed(unsigned char, unsigned char) { }
void RT_crb_replications(unsigned char, short) { }
void RT_drm_mult_str_trp(unsigned char, unsigned char, unsigned char) { }
void RT_drm_notes_played(unsigned char, unsigned char, unsigned char) { }
void RT_dur_dix_ctl_type(unsigned char, unsigned char) { }
void RT_dur_use_rhy_mult(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_dec_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_rel_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_att_sus_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_dec_rel_time(unsigned char, unsigned char, unsigned char) { }
void RT_env_restart_mode(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_att_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_sus_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sus_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_nte_clu_adv_mode(unsigned char, unsigned char, unsigned char) { }
void RT_pan_clu_adv_mode(unsigned char, unsigned char, unsigned char) { }
void RT_pan_use_poffsets(unsigned char, unsigned char) { }
void RT_rhy_mult_str_trp(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_wrap_bot_rel(unsigned char, char) { }
void RT_rpt_wrap_top_rel(unsigned char, char) { }
void RT_vel_clu_adv_mode(unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_10(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_11(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_12(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_13(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_14(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_wav_start_off_15(unsigned char, unsigned char, unsigned char, unsigned char) { }
void RT_bnd_dix_length_ms(unsigned char, short) { }
void RT_bnd_vel_range_bot(unsigned char, unsigned char) { }
void RT_bnd_vel_range_top(unsigned char, unsigned char) { }
void RT_drm_pat_xpose_oct(unsigned char, unsigned char, char) { }
void RT_dur_tie_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_phs_step_xpose_on(unsigned char, unsigned char) { }
void RT_phs_xpose_oct_5th(unsigned char, unsigned char, char) { }
void RT_rel_dly_damp_mode(unsigned char, unsigned char) { }
void RT_rhy_tie_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_rpt_vel_range_bot(unsigned char, unsigned char) { }
void RT_rpt_vel_range_top(unsigned char, unsigned char) { }
void RT_dix_key_trill_mode(unsigned char, unsigned char) { }
void RT_drm_rest_tab_curve(unsigned char, unsigned char, unsigned char) { }
void RT_dur_tie_tab_weight(unsigned char, unsigned char, char) { }
void RT_pan_poct_tab_curve(unsigned char, unsigned char) { }
void RT_pan_poff_tab_curve(unsigned char, unsigned char) { }
void RT_phs_step_xpose_tmp(unsigned char, unsigned char) { }
void RT_rhy_tie_tab_weight(unsigned char, unsigned char, char) { }
void RT_rpt_time_offset_ms(unsigned char, char) { }
void RT_bnd_dix_length_menu(unsigned char, unsigned char) { }
void RT_crb_filter_template(unsigned char, char, RTParmBufferSelect) { }
void RT_drm_rest_tab_weight(unsigned char, unsigned char, char) { }
void RT_nte_pat_dbl_vel_off(unsigned char, unsigned char, unsigned char) { }
void RT_pan_poct_tab_weight(unsigned char, char) { }
void RT_pan_poff_tab_weight(unsigned char, char) { }
void RT_rhy_swing_mult_mode(unsigned char, unsigned char) { }
void RT_rhy_swing_note_menu(unsigned char, unsigned char) { }
void RT_clu_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_mult_str_trp_dot(unsigned char, unsigned char, unsigned char) { }
void RT_dur_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_env_att_sus_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_att_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_att_sus_levl(unsigned char, unsigned char, unsigned char) { }
void RT_env_sta_sus_rel_levl(unsigned char, unsigned char, unsigned char) { }
void RT_nte_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_pan_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_rhy_mult_str_trp_dot(unsigned char, unsigned char, unsigned char) { }
void RT_rhy_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_vel_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_wav_template_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_crb_filt_temp_restore(unsigned char, char, RTParmBufferSelect) { }
void RT_dix_bad_nte_trig_mode(unsigned char, unsigned char) { }
void RT_drm_pat_xpose_oct_5th(unsigned char, unsigned char, char) { }
void RT_drm_template0_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_template1_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_drm_template2_restore(unsigned char, unsigned char, unsigned char, RTParmBufferSelect) { }
void RT_rhy_swing_amt_special(unsigned char, short) { }
void RT_drm_phs_rpt_on_off_pat(unsigned char, unsigned char, unsigned char) { }
void RT_drm_phs_rpt_on_off_comb(unsigned char, unsigned char) { }
void RT_drm_phs_rsy_temp_restore(unsigned char, unsigned char, char, RTParmBufferSelect) { }
void RT_bnd_on(unsigned char, unsigned char) { }
