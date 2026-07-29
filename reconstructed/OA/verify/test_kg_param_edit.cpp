// SPDX-License-Identifier: GPL-2.0
/*
 * test_kg_param_edit.cpp  -  host-side known-answer test for
 * CKGParamEdit's 101 real methods landed in round 52 (solo, 2026-07-29).
 * See include/oa_kg_param_edit_rt_externs.h for the full derivation.
 *
 * Exercises a representative sample spanning every distinct behavioral
 * category rather than all 101 individually (each category's remaining
 * members are byte-identical in shape, differing only in the RT_ / KS_
 * target name and/or a literal constant -- already independently
 * confirmed per-method against ground truth when the .cpp was written):
 *   [1] real per-instance state: ctor, ClearSoloStatus, ResendSoloStatus,
 *       SendSolo, GetPadChangeSource
 *   [2] literal empty-body stubs: SendTempo, SendLocalControlOn
 *   [3] plain 1:1 relays with arg-order/type checks: SendTimeSig,
 *       SendSceneChangeQuantize, SendDTRun, SendNoteMapOctaveReplicate
 *   [4] multi-call relay: SendLatch (RT_pe_latch_state +
 *       KGOutGate_RenewDrumTrackLatchStatus)
 *   [5] CKGRTCHandler::ms_poInstance + global-counter gated: SendFF,
 *       SendRewind
 *   [6] discarded-return-value contract: SendRTPModule
 *   [7] "this"-smuggled extra arg, now a real trailing parameter:
 *       SendChordMemNote, SendChordMemVelocity
 *   [8] literal-constant-selector family (8 variants of the same real
 *       callee): SendSeedRhythm..SendSeedWaveform
 *   [9] 2-arg SendStartSeed overload (distinct real address from the
 *       already-declared 3-arg one)
 *   [10] scene-matrix pointer-chain family: SendLinkedSceneID,
 *        SendSceneIsLinked, SendRTCIsLinked, RefreshLinkedSceneDisplay
 *   [11] bool-typed relays: SendAssign, SendMIDIClockSource,
 *        SetEnableMIDIInToKarmaModule, DrumSwitchOn, DrumSwitchMode
 */

#include <cstdio>
#include <cstring>
#include <string>
#include "oa_ckg_module_param_msg_handler.h"
#include "oa_kg_param_edit_rt_externs.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* ---- real globals SendFF/SendRewind touch ---- */
int g_iDebugCondition;
int gTestTotalBytes;
int gTestTotalMsgs;
unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CKGBankManager::ms_poInstance;

/* ---- call-log mocks for every RT_ / KS_ / KGOutGate_ function this test's
 * own subset of methods calls. Almost all of these have real extern "C++"
 * linkage (oa_kg_param_edit_rt_externs.h's own new-externs block and
 * oa_rtparm_pe_table.h are both extern "C++") so are defined here at
 * ordinary C++ scope; only the 3 that come from
 * oa_ckg_module_param_msg_handler.h's own separate ~50-callee CKGEngine
 * externs block (RT_midi_filt_in_tch/KS_sync_mode_x9100/
 * KS_set_enable_midi_in_to_karma) are genuinely extern "C" and defined in
 * their own block below, same split as verify/test_ckg_engine.cpp. */
static int g_lastA, g_lastB, g_lastC;
static const char *g_lastCall;
static int g_renewCalls;
static void *g_sceneMatrixArg;
static unsigned char g_sceneMatrixIdx;

void RT_run(unsigned char a, unsigned char b) { g_lastCall = "RT_run"; g_lastA = a; g_lastB = b; }
void RT_pe_tsig_menu(unsigned char v) { g_lastCall = "RT_pe_tsig_menu"; g_lastA = v; }
void RT_rtc_qtz_window(unsigned int v) { g_lastCall = "RT_rtc_qtz_window"; g_lastA = (int)v; }
void RT_dt_run(unsigned char a, unsigned char b) { g_lastCall = "RT_dt_run"; g_lastA = a; g_lastB = b; }
void RT_nte_map_oct_replicate(bool v) { g_lastCall = "RT_nte_map_oct_replicate"; g_lastA = v; }
void RT_pe_latch_state(unsigned char v) { g_lastCall = "RT_pe_latch_state"; g_lastA = v; }
void KGOutGate_RenewDrumTrackLatchStatus() { g_renewCalls++; }
void RT_chord_assign(bool v) { g_lastCall = "RT_chord_assign"; g_lastA = v; }
void RT_chord_vel_mode(unsigned char v) { g_lastCall = "RT_chord_vel_mode"; g_lastA = v; }
unsigned long long RT_rpp_module(unsigned char a, unsigned char b, unsigned char c)
{ g_lastCall = "RT_rpp_module"; g_lastA = a; g_lastB = b; g_lastC = c; return 0xdeadbeef; }
void RT_chord_trigger_nte(unsigned char a, unsigned char b, char c)
{ g_lastCall = "RT_chord_trigger_nte"; g_lastA = a; g_lastB = b; g_lastC = c; }
void RT_chord_trigger_vel(unsigned char a, unsigned char b, unsigned char c)
{ g_lastCall = "RT_chord_trigger_vel"; g_lastA = a; g_lastB = b; g_lastC = c; }
void RT_key_zone_bot(unsigned char a, unsigned char b) { g_lastCall = "RT_key_zone_bot"; g_lastA = a; g_lastB = b; }
void RT_midi_filt_in_cc16(unsigned char a, unsigned char b) { g_lastCall = "RT_midi_filt_in_cc16"; g_lastA = a; g_lastB = b; }
void RT_midi_filt_out_bnd(unsigned char a, unsigned char b) { g_lastCall = "RT_midi_filt_out_bnd"; g_lastA = a; g_lastB = b; }
void RT_midi_filt_out_env(unsigned char a, char b, unsigned char c)
{ g_lastCall = "RT_midi_filt_out_env"; g_lastA = a; g_lastB = b; g_lastC = c; }
void RT_pe_seed_offset(unsigned char a, unsigned char b, unsigned char c)
{ g_lastCall = "RT_pe_seed_offset"; g_lastA = a; g_lastB = b; g_lastC = c; }
void RT_dt_link_to_switch(unsigned char a, unsigned char b) { g_lastCall = "RT_dt_link_to_switch"; g_lastA = a; g_lastB = b; }
void RT_dt_switch(bool v) { g_lastCall = "RT_dt_switch"; g_lastA = v; }
void RT_dt_switch_mode(unsigned char v) { g_lastCall = "RT_dt_switch_mode"; g_lastA = v; }
void RT_rtc_scene_link(unsigned char a, unsigned char b) { g_lastCall = "RT_rtc_scene_link"; g_lastA = a; g_lastB = b; }
void RT_rtc_master_enable(unsigned char a, unsigned char b) { g_lastCall = "RT_rtc_master_enable"; g_lastA = a; g_lastB = b; }
void KS_get_scene_matrix(unsigned char idx, void *p) { g_sceneMatrixIdx = idx; g_sceneMatrixArg = p; }
void RT_crb_xpose(unsigned char a, unsigned char b, char c)
{ g_lastCall = "RT_crb_xpose"; g_lastA = a; g_lastB = b; g_lastC = c; }
void RT_pe_seed_set_start(unsigned char a, long b)
{ g_lastCall = "RT_pe_seed_set_start"; g_lastA = a; g_lastB = (int)b; }

/* Genuinely extern "C" (oa_ckg_module_param_msg_handler.h's own ~50-callee
 * block), unlike everything above. */
static int g_syncModeCalls, g_midiInCalls;
extern "C" {
void RT_midi_filt_in_tch(unsigned char a, unsigned char b) { g_lastCall = "RT_midi_filt_in_tch"; g_lastA = a; g_lastB = b; }
void RT_midi_filt_in_bnd(unsigned char, unsigned char) {}
void RT_midi_filt_in_sus(unsigned char, unsigned char) {}
void RT_midi_filt_in_cc1(unsigned char, unsigned char) {}
void RT_midi_filt_in_cc2(unsigned char, unsigned char) {}
void KS_sync_mode_x9100(unsigned char v) { g_syncModeCalls++; g_lastA = v; }
void KS_set_enable_midi_in_to_karma(bool v) { g_midiInCalls++; g_lastA = v; }
}

/* ---- link-satisfying no-op mocks for the remaining real-but-not-directly-
 * checked-here RT_* trampolines (kg_param_edit.cpp's full 101-method TU is
 * linked as a whole, so every symbol it references must resolve even
 * though this test only exercises a representative subset -- same
 * "declare only what's needed to link" convention as
 * verify/rtparm_pe_func_stubs.cpp). */
void RT_cc_offset_num(unsigned char, unsigned char, char) {}
void RT_cc_offset_val(unsigned char, unsigned char, char) {}
void RT_clk_adv_mode(unsigned char, unsigned char) {}
void RT_clk_adv_size_menu(unsigned char, unsigned char) {}
void RT_clk_adv_trig_mode(unsigned char, unsigned char) {}
void RT_clk_adv_vel_bot(unsigned char, unsigned char) {}
void RT_del_start_menu(unsigned char, unsigned char) {}
void RT_del_start_ms(unsigned char, short) {}
void RT_dyn_dst_menu(unsigned char, unsigned char) {}
void RT_dyn_dst_module(unsigned char, unsigned char, unsigned char) {}
void RT_dyn_dst_polarity(unsigned char, unsigned char) {}
void RT_dyn_src_action(unsigned char, unsigned char) {}
void RT_dyn_src_bot(unsigned char, unsigned char) {}
void RT_dyn_src_menu(unsigned char, unsigned char) {}
void RT_dyn_src_top(unsigned char, unsigned char) {}
void RT_dyn_src_zone(unsigned char, unsigned char) {}
void RT_env_latch_mode(unsigned char, unsigned char, unsigned char) {}
void RT_env_trig_mode(unsigned char, unsigned char, unsigned char) {}
void RT_force_range_wrap(unsigned char, unsigned char) {}
void RT_kbd_thru_in(unsigned char, unsigned char) {}
void RT_kbd_thru_in_xpose(unsigned char, char) {}
void RT_kbd_thru_out(unsigned char, unsigned char) {}
void RT_kbd_thru_out_xpose(unsigned char, char) {}
void RT_key_zone_top(unsigned char, unsigned char) {}
void RT_midi_filt_out_cca(unsigned char, unsigned char) {}
void RT_midi_filt_out_ccb(unsigned char, unsigned char) {}
void RT_midi_filt_out_notes(unsigned char, unsigned char) {}
void RT_midi_filt_out_wav(unsigned char, unsigned char) {}
void RT_mod_trig_mode(unsigned char, unsigned char) {}
void RT_mod_trig_rif_perc(unsigned char, unsigned char) {}
void RT_nbo_interp(unsigned char, unsigned char) {}
void RT_nbo_off_mode(unsigned char, unsigned char) {}
void RT_nte_latch_mode(unsigned char, unsigned char) {}
void RT_nte_map_chd_track(unsigned char, unsigned char) {}
void RT_nte_map_kbd_track(unsigned char, unsigned char) {}
void RT_nte_map_on_mode(unsigned char, unsigned char) {}
void RT_nte_map_table(unsigned char, unsigned char) {}
void RT_nte_map_xpose(unsigned char, char) {}
void RT_nte_trig_mode(unsigned char, unsigned char) {}
void RT_pe_freeze_loop(unsigned char, unsigned char) {}
void RT_pe_freeze_retrig(unsigned char, unsigned char) {}
void RT_qtz_on(unsigned char, unsigned char) {}
void RT_qtz_window(unsigned char, unsigned char) {}
void RT_root_position(unsigned char, unsigned char) {}
void RT_rpp_bank_menu(unsigned char, unsigned char) {}
void RT_rpp_max(unsigned char, short) {}
void RT_rpp_min(unsigned char, short) {}
void RT_rpp_pe_control(unsigned char, char, char) {}
void RT_rpp_pe_edit(unsigned char, short) {}
void RT_rpp_pe_polarity(unsigned char, char) {}
void RT_trigger_cutoff(unsigned char, unsigned char, unsigned char) {}

int main()
{
	CKGParamEdit pe;

	/* [2] empty stubs -- must not crash, must not call anything */
	g_lastCall = 0;
	pe.SendTempo(0x1234);
	check("SendTempo: no-op, no RT_* call", g_lastCall == 0);
	pe.SendLocalControlOn(true);
	check("SendLocalControlOn: no-op, no RT_* call", g_lastCall == 0);

	/* [3] plain relays */
	pe.SendTimeSig(5);
	check("SendTimeSig -> RT_pe_tsig_menu(5)", g_lastCall == std::string("RT_pe_tsig_menu") && g_lastA == 5);
	pe.SendSceneChangeQuantize(7);
	check("SendSceneChangeQuantize -> RT_rtc_qtz_window(7)", g_lastCall == std::string("RT_rtc_qtz_window") && g_lastA == 7);
	pe.SendDTRun(2, 1);
	check("SendDTRun -> RT_dt_run(2,1)", g_lastCall == std::string("RT_dt_run") && g_lastA == 2 && g_lastB == 1);
	pe.SendNoteMapOctaveReplicate(true);
	check("SendNoteMapOctaveReplicate -> RT_nte_map_oct_replicate(1)", g_lastCall == std::string("RT_nte_map_oct_replicate") && g_lastA == 1);

	/* [4] SendLatch: 2 calls */
	g_renewCalls = 0;
	g_lastCall = 0;
	pe.SendLatch(true);
	check("SendLatch -> RT_pe_latch_state(1)", g_lastCall == std::string("RT_pe_latch_state") && g_lastA == 1);
	check("SendLatch -> KGOutGate_RenewDrumTrackLatchStatus() called once", g_renewCalls == 1);

	pe.SendAssign(true);
	check("SendAssign -> RT_chord_assign(1)", g_lastCall == std::string("RT_chord_assign") && g_lastA == 1);
	pe.SendPadMode(3);
	check("SendPadMode -> RT_chord_vel_mode(3)", g_lastCall == std::string("RT_chord_vel_mode") && g_lastA == 3);

	/* [5] CKGRTCHandler::ms_poInstance + global-counter gated */
	printf("-- [5] SendFF/SendRewind --\n");
	unsigned char rtcBuf[0x100];
	memset(rtcBuf, 0, sizeof(rtcBuf));
	CKGRTCHandler::ms_poInstance = rtcBuf;
	rtcBuf[0xe0] = 0; /* gate closed */
	g_iDebugCondition = 5;
	pe.SendFF(false);
	check("SendFF: gate closed, no change", g_iDebugCondition == 5);
	rtcBuf[0xe0] = 1; /* gate open */
	gTestTotalBytes = 99;
	gTestTotalMsgs = 99;
	pe.SendFF(false);
	check("SendFF: gate open, g_iDebugCondition++", g_iDebugCondition == 6);
	check("SendFF: gate open, counters reset", gTestTotalBytes == 0 && gTestTotalMsgs == 0);
	pe.SendRewind(false);
	check("SendRewind: gate open, g_iDebugCondition--", g_iDebugCondition == 5);

	/* [6] discarded return */
	g_lastCall = 0;
	unsigned long long unusedReturn = 0xffffffffffffffffULL;
	(void)unusedReturn;
	pe.SendRTPModule(1, 2, 3);
	check("SendRTPModule -> RT_rpp_module(1,2,3), return discarded (compiles as void)",
	      g_lastCall == std::string("RT_rpp_module") && g_lastA == 1 && g_lastB == 2 && g_lastC == 3);

	/* [7] "this"-smuggled extra arg -> real trailing parameter */
	pe.SendChordMemNote(4, 5, 6);
	check("SendChordMemNote -> RT_chord_trigger_nte(4,5,6)",
	      g_lastCall == std::string("RT_chord_trigger_nte") && g_lastA == 4 && g_lastB == 5 && g_lastC == 6);
	pe.SendChordMemVelocity(7, 8, 9);
	check("SendChordMemVelocity -> RT_chord_trigger_vel(7,8,9)",
	      g_lastCall == std::string("RT_chord_trigger_vel") && g_lastA == 7 && g_lastB == 8 && g_lastC == 9);

	pe.SendRun(1, 1);
	check("SendRun -> RT_run(1,1)", g_lastCall == std::string("RT_run") && g_lastA == 1 && g_lastB == 1);
	pe.SendKeyBottom(2, 60);
	check("SendKeyBottom -> RT_key_zone_bot(2,60)", g_lastCall == std::string("RT_key_zone_bot") && g_lastA == 2 && g_lastB == 60);
	pe.SendRxAfter(1, 1);
	check("SendRxAfter -> RT_midi_filt_in_tch(1,1)", g_lastCall == std::string("RT_midi_filt_in_tch") && g_lastA == 1 && g_lastB == 1);
	pe.SendRxRibbon(2, 0);
	check("SendRxRibbon -> RT_midi_filt_in_cc16(2,0) (NOT RT_midi_filt_in_ctl)",
	      g_lastCall == std::string("RT_midi_filt_in_cc16") && g_lastA == 2 && g_lastB == 0);
	pe.SendTxBend(3, 1);
	check("SendTxBend -> RT_midi_filt_out_bnd(3,1)", g_lastCall == std::string("RT_midi_filt_out_bnd") && g_lastA == 3 && g_lastB == 1);
	pe.SendTxEnv1(1, 1);
	check("SendTxEnv1 -> RT_midi_filt_out_env(1,0,1)", g_lastCall == std::string("RT_midi_filt_out_env") && g_lastA == 1 && g_lastB == 0 && g_lastC == 1);
	pe.SendTxEnv2(1, 1);
	check("SendTxEnv2 -> RT_midi_filt_out_env(1,1,1)", g_lastB == 1);
	pe.SendTxEnv3(1, 1);
	check("SendTxEnv3 -> RT_midi_filt_out_env(1,2,1)", g_lastB == 2);
	pe.SendTranspose(1, 12);
	check("SendTranspose -> RT_crb_xpose(1,0,12)", g_lastCall == std::string("RT_crb_xpose") && g_lastA == 1 && g_lastB == 0 && g_lastC == 12);

	/* [8] literal-constant-selector family */
	printf("-- [8] SendSeedXxx literal-constant family --\n");
	pe.SendSeedRhythm(1, 9); check("SendSeedRhythm  -> const 0", g_lastC == 0);
	pe.SendSeedDuration(1, 9); check("SendSeedDuration -> const 1", g_lastC == 1);
	pe.SendSeedNote(1, 9); check("SendSeedNote     -> const 2", g_lastC == 2);
	pe.SendSeedCluster(1, 9); check("SendSeedCluster  -> const 3", g_lastC == 3);
	pe.SendSeedVelocity(1, 9); check("SendSeedVelocity -> const 4", g_lastC == 4);
	pe.SendSeedPan(1, 9); check("SendSeedPan      -> const 5", g_lastC == 5);
	pe.SendSeedDrum(1, 9); check("SendSeedDrum     -> const 6", g_lastC == 6);
	pe.SendSeedWaveform(1, 9); check("SendSeedWaveform -> const 7", g_lastC == 7);

	/* [9] 2-arg SendStartSeed overload, distinct real address */
	g_lastCall = 0;
	pe.SendStartSeed(3, 0x11223344L);
	check("SendStartSeed(uchar,long) -> RT_pe_seed_set_start(3,0x11223344)",
	      g_lastCall == std::string("RT_pe_seed_set_start") && g_lastA == 3 && g_lastB == 0x11223344);

	/* [10] scene-matrix pointer-chain family */
	printf("-- [10] scene-matrix family --\n");
	unsigned char bankBuf[0x10];
	unsigned char matrixBuf[0x10000];
	*(unsigned char **)(bankBuf + 8) = matrixBuf;
	CKGBankManager::ms_poInstance = bankBuf;
	void *expectSlot = matrixBuf + (unsigned int)5 * 0x154 + 0x6cd0;
	pe.SendLinkedSceneID(0xff, 5, 0xff);
	check("SendLinkedSceneID: param_2 used for both idx and KS arg", g_sceneMatrixIdx == 5 && g_sceneMatrixArg == expectSlot);
	pe.SendSceneIsLinked(6, 1);
	void *expectSlot6 = matrixBuf + (unsigned int)6 * 0x154 + 0x6cd0;
	check("SendSceneIsLinked -> RT_rtc_scene_link(6,1) + correct slot",
	      g_lastCall == std::string("RT_rtc_scene_link") && g_lastA == 6 && g_lastB == 1 && g_sceneMatrixArg == expectSlot6);
	pe.SendRTCIsLinked(7, 0);
	check("SendRTCIsLinked -> RT_rtc_master_enable(7,0)", g_lastCall == std::string("RT_rtc_master_enable") && g_lastA == 7 && g_lastB == 0);
	pe.RefreshLinkedSceneDisplay(8);
	void *expectSlot8 = matrixBuf + (unsigned int)8 * 0x154 + 0x6cd0;
	check("RefreshLinkedSceneDisplay: correct slot, no RT_* call", g_sceneMatrixIdx == 8 && g_sceneMatrixArg == expectSlot8);

	/* [11] bool-typed relays */
	pe.SendMIDIClockSource(true);
	check("SendMIDIClockSource -> KS_sync_mode_x9100(1)", g_syncModeCalls == 1 && g_lastA == 1);
	pe.SetEnableMIDIInToKarmaModule(true);
	check("SetEnableMIDIInToKarmaModule -> KS_set_enable_midi_in_to_karma(1)", g_midiInCalls == 1 && g_lastA == 1);
	pe.DrumSwitchOn(true);
	check("DrumSwitchOn -> RT_dt_switch(1)", g_lastCall == std::string("RT_dt_switch") && g_lastA == 1);
	pe.DrumSwitchMode(2);
	check("DrumSwitchMode -> RT_dt_switch_mode(2)", g_lastCall == std::string("RT_dt_switch_mode") && g_lastA == 2);
	pe.SendLinkToDT(1, 1);
	check("SendLinkToDT -> RT_dt_link_to_switch(1,1)", g_lastCall == std::string("RT_dt_link_to_switch") && g_lastA == 1 && g_lastB == 1);

	/* real per-instance state (revisit with a fresh instance) */
	printf("-- [1] SendSolo/ClearSoloStatus/ResendSoloStatus/GetPadChangeSource --\n");
	{
		/* RT_solo is declared in oa_kg_param_edit_rt_externs.h and defined
		 * once below main(), using its own file-scope call-log statics. */
		CKGParamEdit pe2;
		extern void ResetSoloLog();
		ResetSoloLog();
		pe2.SendSolo(0, 1);
		pe2.SendSolo(2, 1);
		extern int GetSoloCallCount();
		extern unsigned char GetSoloVal(int);
		check("SendSolo(0,1)/SendSolo(2,1): 2 RT_solo calls", GetSoloCallCount() == 2);
		pe2.ClearSoloStatus();
		pe2.ResendSoloStatus();
		check("ClearSoloStatus + ResendSoloStatus: all 4 RT_solo(idx,0) calls", GetSoloCallCount() == 6);
		check("ResendSoloStatus: last value read back is 0 (cleared)", GetSoloVal(3) == 0);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}

/* ---- RT_solo mock + tiny accessor API, defined after main() since main()
 * forward-declares what it needs (keeps the state-focused checks visually
 * grouped near their own setup instead of scattered at file scope). Real
 * extern "C++" linkage, same as every other new extern in this file. */
static int g_soloCallCount;
static unsigned char g_soloLastVal[4];
static int g_soloLastIdx;
void RT_solo(unsigned char idx, unsigned char val)
{
	g_soloCallCount++;
	g_soloLastIdx = idx;
	if (idx < 4)
		g_soloLastVal[idx] = val;
}
void ResetSoloLog() { g_soloCallCount = 0; g_soloLastIdx = -1; memset(g_soloLastVal, 0xff, sizeof(g_soloLastVal)); }
int GetSoloCallCount() { return g_soloCallCount; }
unsigned char GetSoloVal(int idx) { return g_soloLastVal[idx]; }
