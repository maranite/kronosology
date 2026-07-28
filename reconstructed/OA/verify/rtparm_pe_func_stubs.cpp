// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_pe_func_stubs.cpp  -  TEST-ONLY empty-body stand-ins for the 46
 * non-null RT_* KARMA parameter-handler functions gRTParmFunctionTable_PE
 * (see include/oa_rtparm_pe_table.h) stores pointers to.
 *
 * These functions are REAL and defined elsewhere in ground-truth OA.ko --
 * they are simply not yet reconstructed. This file exists ONLY so
 * verify/test_rtparm_pe_table.cpp can fully link as a native host
 * executable and compare gRTParmFunctionTable_PE[i].funcPtr against &RT_xxx
 * by identity -- it is NEVER linked into OA.ko itself, matching
 * verify/rtparm_ge_func_stubs.cpp's own convention.
 */

#include "oa_rtparm_pe_table.h"

void RT_clk_adv_mode(unsigned char, unsigned char) { }
void RT_clk_adv_size_menu(unsigned char, unsigned char) { }
void RT_clk_adv_trig_mode(unsigned char, unsigned char) { }
void RT_clk_adv_vel_bot(unsigned char, unsigned char) { }
void RT_crb_xpose(unsigned char, unsigned char, char) { }
void RT_crb_xpose_oct(unsigned char, unsigned char, char) { }
void RT_crb_xpose_oct_5th(unsigned char, unsigned char, char) { }
void RT_del_start_menu(unsigned char, unsigned char) { }
void RT_del_start_ms(unsigned char, short) { }
void RT_env_latch_mode_0(unsigned char, unsigned char, unsigned char) { }
void RT_env_latch_mode_1(unsigned char, unsigned char, unsigned char) { }
void RT_env_latch_mode_2(unsigned char, unsigned char, unsigned char) { }
void RT_env_trig_mode_0(unsigned char, unsigned char, unsigned char) { }
void RT_env_trig_mode_1(unsigned char, unsigned char, unsigned char) { }
void RT_env_trig_mode_2(unsigned char, unsigned char, unsigned char) { }
void RT_force_range_wrap(unsigned char, unsigned char) { }
void RT_kbd_thru_in(unsigned char, unsigned char) { }
void RT_kbd_thru_in_xpose(unsigned char, char) { }
void RT_kbd_thru_in_xpose_oct(unsigned char, char) { }
void RT_kbd_thru_in_xpose_oct_5th(unsigned char, char) { }
void RT_kbd_thru_out(unsigned char, unsigned char) { }
void RT_kbd_thru_out_xpose(unsigned char, char) { }
void RT_kbd_thru_out_xpose_oct(unsigned char, char) { }
void RT_kbd_thru_out_xpose_oct_5th(unsigned char, char) { }
void RT_key_zone_bot(unsigned char, unsigned char) { }
void RT_key_zone_top(unsigned char, unsigned char) { }
void RT_mod_trig_mode(unsigned char, unsigned char) { }
void RT_mod_trig_rif_perc(unsigned char, unsigned char) { }
void RT_nbo_interp(unsigned char, unsigned char) { }
void RT_nbo_off_mode(unsigned char, unsigned char) { }
void RT_nte_latch_mode(unsigned char, unsigned char) { }
void RT_nte_map_chd_track(unsigned char, unsigned char) { }
void RT_nte_map_kbd_track(unsigned char, unsigned char) { }
void RT_nte_map_on_mode(unsigned char, unsigned char) { }
void RT_nte_map_table(unsigned char, unsigned char) { }
void RT_nte_map_xpose(unsigned char, char) { }
void RT_nte_trig_mode(unsigned char, unsigned char) { }
void RT_pe_freeze_loop(unsigned char, unsigned char) { }
void RT_pe_freeze_loop_reset(unsigned char, unsigned char) { }
void RT_pe_freeze_retrig(unsigned char, unsigned char) { }
void RT_pe_seed_set_start(unsigned char, long) { }
void RT_pe_tsig_menu(unsigned char) { }
void RT_qtz_on(unsigned char, unsigned char) { }
void RT_qtz_window(unsigned char, unsigned char) { }
void RT_root_position(unsigned char, unsigned char) { }
void RT_run(unsigned char, unsigned char) { }
