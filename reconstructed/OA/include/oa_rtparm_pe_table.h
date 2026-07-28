// SPDX-License-Identifier: GPL-2.0
/*
 * oa_rtparm_pe_table.h  -  gRTParmFunctionTable_PE, the KARMA real-time-
 * parameter (RTParm) dispatch table for the "PE" (Pattern Engine? Phrase
 * Engine? -- exact scope unconfirmed, matches the "run/clock-advance/note-map/
 * keyboard-thru/freeze" naming cluster below) parameter domain. Sibling table
 * to gRTParmFunctionTable_GE (see include/oa_rtparm_ge_table.h) -- SAME exact
 * 0x28(40)-byte record shape, smaller scale.
 *
 * ground truth: InitializegRTParmFunctionTable_PE(), .text+0x573810, 5505
 * bytes, cdecl, no args. A single straight-line run of 50*12 = 600
 * `mov [gRTParmFunctionTable_PE+off], value` stores (zero branches, zero
 * calls) populating a 50-entry, 0x28-byte-stride array in .bss
 * (gRTParmFunctionTable_PE, .bss size 0x7d0 == 50*0x28 exactly).
 * Reconstructed via the same scripted-decoder technique as the GE table
 * (see that header + project memory "rtparm-ge-table-scripted-decoder" for
 * the full methodology) -- confirmed self-consistent: 50 entries * 12
 * field-writes/entry == 600 raw instructions parsed with 0 leftover/
 * misaligned writes, and the function's own byte size (5505) is
 * independently reproduced by summing each write-opcode's encoded length
 * (110 bytes/entry * 50 + 5-byte prologue/epilogue = 5505).
 *
 * Caller: BirthOfKarma() (ground-truth offset 0x507d0c, still `pending`)
 * calls InitializegRTParmFunctionTable_GE() immediately followed by
 * InitializegRTParmFunctionTable_PE(), both guarded by the same one-time-
 * init check (see oa_rtparm_ge_table.h's own caller note) -- confirmed real
 * via the GE table's reconstruction pass, not re-verified independently here.
 *
 * Field semantics: index (self-index, index==entry position for all 50
 * entries, confirmed) is structurally confirmed same as GE. funcPtr is
 * usually a real, unique RT_* function pointer (46/50 distinct, confirmed)
 * but UNLIKE the GE table, 4 entries (index 5,6,7,8) have a literal NULL
 * funcPtr (single relocation only -- base symbol, no function-pointer
 * relocation -- confirmed by both decoders, not a parse artifact) --
 * these 4 consecutive slots are presumably reserved/placeholder parameter
 * slots in this smaller table, not populated at this point in KARMA's
 * startup. field20 == field24 for all 50 entries (confirmed, unlike GE
 * where both are always 0 -- here they carry a nonzero shared value for
 * most entries). field1c is NOT always equal to field20/field24 here
 * (unlike being uniformly 0 in GE) -- exact relationship unconfirmed.
 * field0c is boolean-shaped over {0,2} in this table (not {0,1} as in GE) --
 * TODO verify meaning once a real caller of one of these RT_pe_..., RT_run,
 * RT_qtz_..., RT_nte_map_..., RT_kbd_thru_..., RT_clk_adv_..., RT_env_...,
 * RT_del_start_..., RT_crb_xpose..., RT_mod_trig_..., RT_root_position,
 * RT_force_range_wrap, RT_nte_latch_mode, RT_nte_trig_mode is reconstructed.
 * All other numeric fields (field04/0a/0b/10/14/18/1c/20/24) remain TODO,
 * same caveat as GE's own header.
 *
 * The 46 non-null RT_* callees are real, separate, already-defined
 * functions elsewhere in ground-truth OA.ko (confirmed via symtab
 * cross-check) but are NOT reconstructed by this pass -- all remain
 * `pending` in the manifest. Declared here as `extern` only, matching
 * this project's standing convention.
 */

#ifndef OA_RTPARM_PE_TABLE_H
#define OA_RTPARM_PE_TABLE_H

#define RTPARM_PE_TABLE_SIZE 50

/* One gRTParmFunctionTable_PE record, 0x28 (40) bytes -- IDENTICAL layout
 * to RTParmFunctionTableEntry_GE (see oa_rtparm_ge_table.h), duplicated here
 * as its own named type rather than reused, matching how ground truth keeps
 * these as two textually-separate tables/initializers. */
struct RTParmFunctionTableEntry_PE
{
	void *funcPtr;           /* +0x00  target RT_* handler; NULL (literal, no relocation) for indices 5..8 */
	unsigned int field04;    /* +0x04  TODO: unconfirmed */
	unsigned short index;    /* +0x08  self-index; == entry position for all 50 entries */
	unsigned char field0a;   /* +0x0a  TODO: unconfirmed (1..6 observed) */
	unsigned char field0b;   /* +0x0b  TODO: unconfirmed (0..14 observed) */
	unsigned char field0c;   /* +0x0c  boolean-shaped, only 0 or 2 observed (unlike GE's 0/1); 0 only for indices 2..4 (the RT_crb_xpose* trio), 2 for all other 47 entries */
	unsigned char _pad_0d[3]; /* +0x0d..0x0f  never written in ground truth */
	unsigned int field10;    /* +0x10  TODO: unconfirmed */
	unsigned int field14;    /* +0x14  TODO: unconfirmed */
	unsigned int field18;    /* +0x18  TODO: unconfirmed */
	unsigned int field1c;    /* +0x1c  TODO: unconfirmed, NOT always 0 (unlike GE) and NOT always == field20/field24 */
	unsigned int field20;    /* +0x20  TODO: unconfirmed, == field24 for all 50 entries (confirmed) */
	unsigned int field24;    /* +0x24  TODO: unconfirmed, == field20 for all 50 entries (confirmed) */
};

extern RTParmFunctionTableEntry_PE gRTParmFunctionTable_PE[RTPARM_PE_TABLE_SIZE];

void InitializegRTParmFunctionTable_PE();

/* ---- the 46 non-null RT_* callees (all real, all pending -- see header
 * comment above). Declared here only so gRTParmFunctionTable_PE's own
 * initializer can take their address; NOT defined by this file. */
extern "C++" {

void RT_clk_adv_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clk_adv_size_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clk_adv_trig_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_clk_adv_vel_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_crb_xpose(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_crb_xpose_oct(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_crb_xpose_oct_5th(unsigned char, unsigned char, char) __attribute__((regparm(3)));
void RT_del_start_menu(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_del_start_ms(unsigned char, short) __attribute__((regparm(3)));
void RT_env_latch_mode_0(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_latch_mode_1(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_latch_mode_2(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_trig_mode_0(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_trig_mode_1(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_env_trig_mode_2(unsigned char, unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_force_range_wrap(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_kbd_thru_in(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_kbd_thru_in_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_kbd_thru_in_xpose_oct(unsigned char, char) __attribute__((regparm(3)));
void RT_kbd_thru_in_xpose_oct_5th(unsigned char, char) __attribute__((regparm(3)));
void RT_kbd_thru_out(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_kbd_thru_out_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_kbd_thru_out_xpose_oct(unsigned char, char) __attribute__((regparm(3)));
void RT_kbd_thru_out_xpose_oct_5th(unsigned char, char) __attribute__((regparm(3)));
void RT_key_zone_bot(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_key_zone_top(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_mod_trig_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_mod_trig_rif_perc(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nbo_interp(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nbo_off_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_latch_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_chd_track(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_kbd_track(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_on_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_table(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_nte_map_xpose(unsigned char, char) __attribute__((regparm(3)));
void RT_nte_trig_mode(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_freeze_loop(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_freeze_loop_reset(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_freeze_retrig(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_pe_seed_set_start(unsigned char, long) __attribute__((regparm(3)));
void RT_pe_tsig_menu(unsigned char) __attribute__((regparm(3)));
void RT_qtz_on(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_qtz_window(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_root_position(unsigned char, unsigned char) __attribute__((regparm(3)));
void RT_run(unsigned char, unsigned char) __attribute__((regparm(3)));

} /* extern "C++" */

#endif /* OA_RTPARM_PE_TABLE_H */
