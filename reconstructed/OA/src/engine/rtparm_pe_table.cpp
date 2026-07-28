// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_pe_table.cpp  -  InitializegRTParmFunctionTable_PE()
 * .text+0x573810, 5505 bytes, cdecl, no args.
 *
 * See include/oa_rtparm_pe_table.h for the full derivation notes. Same
 * technique/shape as src/engine/rtparm_ge_table.cpp: static const template
 * array + a copy loop, matching ground truth's bit-identical final table
 * contents (verified by verify/test_rtparm_pe_table.cpp) without replaying
 * 600 individual field-assignment statements verbatim.
 */

#include "oa_rtparm_pe_table.h"

RTParmFunctionTableEntry_PE gRTParmFunctionTable_PE[RTPARM_PE_TABLE_SIZE];

static const RTParmFunctionTableEntry_PE kRTParmTable_PE[RTPARM_PE_TABLE_SIZE] =
{
	{ (void *)&RT_pe_tsig_menu, 36, 0, 1, 0, 2, {0,0,0}, 484, 0, 0, 0, 0, 0 }, /* [0] */
	{ (void *)&RT_run, 0, 1, 2, 0, 2, {0,0,0}, 6, 126, 246, 366, 6, 6 }, /* [1] */
	{ (void *)&RT_crb_xpose, 7, 2, 2, 1, 0, {0,0,0}, 7, 2515, 5023, 7531, 7, 7 }, /* [2] */
	{ (void *)&RT_crb_xpose_oct, 7, 3, 2, 2, 0, {0,0,0}, 7, 2515, 5023, 7531, 7, 7 }, /* [3] */
	{ (void *)&RT_crb_xpose_oct_5th, 7, 4, 2, 3, 0, {0,0,0}, 7, 2515, 5023, 7531, 7, 7 }, /* [4] */
	{ (void *)0, 6, 5, 2, 4, 2, {0,0,0}, 54, 174, 294, 414, 54, 54 }, /* [5] */
	{ (void *)0, 6, 6, 2, 5, 2, {0,0,0}, 56, 176, 296, 416, 56, 56 }, /* [6] */
	{ (void *)0, 6, 7, 2, 6, 2, {0,0,0}, 57, 177, 297, 417, 57, 57 }, /* [7] */
	{ (void *)0, 6, 8, 2, 7, 2, {0,0,0}, 7, 127, 247, 367, 7, 7 }, /* [8] */
	{ (void *)&RT_nbo_interp, 0, 9, 3, 0, 2, {0,0,0}, 17, 137, 257, 377, 17, 17 }, /* [9] */
	{ (void *)&RT_force_range_wrap, 0, 10, 3, 1, 2, {0,0,0}, 19, 139, 259, 379, 19, 19 }, /* [10] */
	{ (void *)&RT_root_position, 16, 11, 3, 2, 2, {0,0,0}, 14, 134, 254, 374, 14, 14 }, /* [11] */
	{ (void *)&RT_clk_adv_mode, 0, 12, 3, 3, 2, {0,0,0}, 44, 164, 284, 404, 44, 44 }, /* [12] */
	{ (void *)&RT_clk_adv_size_menu, 0, 13, 3, 4, 2, {0,0,0}, 45, 165, 285, 405, 45, 45 }, /* [13] */
	{ (void *)&RT_clk_adv_vel_bot, 0, 14, 3, 5, 2, {0,0,0}, 46, 166, 286, 406, 46, 46 }, /* [14] */
	{ (void *)&RT_clk_adv_trig_mode, 0, 15, 3, 6, 2, {0,0,0}, 47, 167, 287, 407, 47, 47 }, /* [15] */
	{ (void *)&RT_nte_map_on_mode, 23, 16, 3, 7, 2, {0,0,0}, 43, 163, 283, 402, 43, 43 }, /* [16] */
	{ (void *)&RT_nte_map_table, 0, 17, 3, 8, 2, {0,0,0}, 42, 162, 282, 402, 42, 42 }, /* [17] */
	{ (void *)&RT_nte_map_xpose, 1, 18, 3, 9, 2, {0,0,0}, 41, 161, 281, 401, 41, 41 }, /* [18] */
	{ (void *)&RT_nte_map_chd_track, 19, 19, 3, 10, 2, {0,0,0}, 43, 163, 283, 402, 43, 43 }, /* [19] */
	{ (void *)&RT_nte_map_kbd_track, 20, 20, 3, 11, 2, {0,0,0}, 43, 163, 283, 402, 43, 43 }, /* [20] */
	{ (void *)&RT_qtz_on, 0, 21, 4, 0, 2, {0,0,0}, 12, 132, 252, 372, 12, 12 }, /* [21] */
	{ (void *)&RT_qtz_window, 0, 22, 4, 1, 2, {0,0,0}, 20, 140, 260, 380, 20, 20 }, /* [22] */
	{ (void *)&RT_del_start_menu, 0, 23, 4, 2, 2, {0,0,0}, 21, 141, 261, 381, 21, 21 }, /* [23] */
	{ (void *)&RT_del_start_ms, 2, 24, 4, 3, 2, {0,0,0}, 22, 142, 262, 382, 22, 22 }, /* [24] */
	{ (void *)&RT_nte_trig_mode, 0, 25, 4, 4, 2, {0,0,0}, 26, 146, 266, 386, 26, 26 }, /* [25] */
	{ (void *)&RT_nte_latch_mode, 0, 26, 4, 5, 2, {0,0,0}, 27, 147, 267, 387, 27, 27 }, /* [26] */
	{ (void *)&RT_nbo_off_mode, 0, 27, 4, 6, 2, {0,0,0}, 18, 138, 258, 378, 18, 18 }, /* [27] */
	{ (void *)&RT_env_trig_mode_0, 6, 28, 4, 7, 2, {0,0,0}, 28, 148, 268, 388, 28, 28 }, /* [28] */
	{ (void *)&RT_env_latch_mode_0, 6, 29, 4, 8, 2, {0,0,0}, 31, 151, 271, 391, 31, 31 }, /* [29] */
	{ (void *)&RT_env_trig_mode_1, 6, 30, 4, 9, 2, {0,0,0}, 29, 149, 269, 389, 29, 29 }, /* [30] */
	{ (void *)&RT_env_latch_mode_1, 6, 31, 4, 10, 2, {0,0,0}, 32, 152, 272, 392, 32, 32 }, /* [31] */
	{ (void *)&RT_env_trig_mode_2, 6, 32, 4, 11, 2, {0,0,0}, 30, 150, 270, 390, 30, 30 }, /* [32] */
	{ (void *)&RT_env_latch_mode_2, 6, 33, 4, 12, 2, {0,0,0}, 33, 153, 273, 393, 33, 33 }, /* [33] */
	{ (void *)&RT_mod_trig_mode, 0, 34, 4, 13, 2, {0,0,0}, 24, 144, 264, 384, 24, 24 }, /* [34] */
	{ (void *)&RT_mod_trig_rif_perc, 0, 35, 4, 14, 2, {0,0,0}, 25, 145, 265, 385, 25, 25 }, /* [35] */
	{ (void *)&RT_kbd_thru_in, 13, 36, 5, 0, 2, {0,0,0}, 14, 134, 254, 374, 14, 14 }, /* [36] */
	{ (void *)&RT_kbd_thru_out, 15, 37, 5, 1, 2, {0,0,0}, 14, 134, 254, 374, 14, 14 }, /* [37] */
	{ (void *)&RT_key_zone_bot, 0, 38, 5, 2, 2, {0,0,0}, 10, 130, 250, 370, 10, 10 }, /* [38] */
	{ (void *)&RT_key_zone_top, 0, 39, 5, 3, 2, {0,0,0}, 11, 131, 251, 371, 11, 11 }, /* [39] */
	{ (void *)&RT_kbd_thru_in_xpose, 1, 40, 5, 4, 2, {0,0,0}, 15, 135, 255, 375, 15, 15 }, /* [40] */
	{ (void *)&RT_kbd_thru_out_xpose, 1, 41, 5, 5, 2, {0,0,0}, 16, 136, 256, 376, 16, 16 }, /* [41] */
	{ (void *)&RT_kbd_thru_in_xpose_oct, 1, 42, 5, 6, 2, {0,0,0}, 15, 135, 255, 375, 15, 15 }, /* [42] */
	{ (void *)&RT_kbd_thru_out_xpose_oct, 1, 43, 5, 7, 2, {0,0,0}, 16, 136, 256, 376, 16, 16 }, /* [43] */
	{ (void *)&RT_kbd_thru_in_xpose_oct_5th, 1, 44, 5, 8, 2, {0,0,0}, 15, 135, 255, 375, 15, 15 }, /* [44] */
	{ (void *)&RT_kbd_thru_out_xpose_oct_5th, 1, 45, 5, 9, 2, {0,0,0}, 16, 136, 256, 376, 16, 16 }, /* [45] */
	{ (void *)&RT_pe_seed_set_start, 35, 46, 6, 0, 2, {0,0,0}, 36, 156, 276, 396, 36, 36 }, /* [46] */
	{ (void *)&RT_pe_freeze_loop, 25, 47, 6, 1, 2, {0,0,0}, 40, 160, 280, 400, 40, 40 }, /* [47] */
	{ (void *)&RT_pe_freeze_loop_reset, 25, 48, 6, 2, 2, {0,0,0}, 40, 160, 280, 400, 40, 40 }, /* [48] */
	{ (void *)&RT_pe_freeze_retrig, 20, 49, 6, 3, 2, {0,0,0}, 40, 160, 280, 400, 40, 40 }, /* [49] */
};

void InitializegRTParmFunctionTable_PE()
{
	for (unsigned int i = 0; i < RTPARM_PE_TABLE_SIZE; ++i)
		gRTParmFunctionTable_PE[i] = kRTParmTable_PE[i];
}
