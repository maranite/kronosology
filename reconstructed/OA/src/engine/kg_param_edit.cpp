// SPDX-License-Identifier: GPL-2.0
/*
 * kg_param_edit.cpp  -  CKGParamEdit method bodies (round 52, solo). See
 * include/oa_kg_param_edit_rt_externs.h for the full derivation, the "this
 * is not always a real object pointer" gotcha, the signature-reconciliation
 * note, and the 2 distinct deferral reasons for the other 31 methods. The
 * ctor lives in the sibling src/engine/kg_param_edit_ctor.cpp instead --
 * see that file's own header comment for why.
 */
#include "oa_ckg_module_param_msg_handler.h"
#include "oa_kg_param_edit_rt_externs.h"

void CKGParamEdit::ClearSoloStatus()
{
	mSoloStatus[0] = mSoloStatus[1] = mSoloStatus[2] = mSoloStatus[3] = 0;
}

void CKGParamEdit::ResendSoloStatus()
{
	RT_solo(0, mSoloStatus[0]);
	RT_solo(1, mSoloStatus[1]);
	RT_solo(2, mSoloStatus[2]);
	RT_solo(3, mSoloStatus[3]);
}

void CKGParamEdit::SendSolo(unsigned char module, unsigned char on)
{
	RT_solo(module, on);
	mSoloStatus[module] = on != 0;
}

int CKGParamEdit::GetPadChangeSource() const
{
	return mPadChangeSource;
}

/* Real ground truth: literal empty bodies, args genuinely unused. */
void CKGParamEdit::SendTempo(unsigned short) {}
void CKGParamEdit::SendLocalControlOn(bool) {}

void CKGParamEdit::SendTimeSig(unsigned char value) { RT_pe_tsig_menu(value); }
void CKGParamEdit::SendSceneChangeQuantize(int value) { RT_rtc_qtz_window((unsigned int)value); }
void CKGParamEdit::SendDTRun(unsigned char module, unsigned char on) { RT_dt_run(module, on != 0); }
void CKGParamEdit::SendNoteMapOctaveReplicate(bool on) { RT_nte_map_oct_replicate(on); }

void CKGParamEdit::SendLatch(bool on)
{
	RT_pe_latch_state(on);
	KGOutGate_RenewDrumTrackLatchStatus();
}

void CKGParamEdit::SendAssign(bool on) { RT_chord_assign(on); }
void CKGParamEdit::SendPadMode(unsigned char mode) { RT_chord_vel_mode(mode); }

void CKGParamEdit::SendFF(bool)
{
	if (*(char *)(CKGRTCHandler::ms_poInstance + 0xe0) != '\0') {
		g_iDebugCondition = g_iDebugCondition + 1;
		gTestTotalBytes = 0;
		gTestTotalMsgs = 0;
	}
}

void CKGParamEdit::SendRewind(bool)
{
	if (*(char *)(CKGRTCHandler::ms_poInstance + 0xe0) != '\0')
		g_iDebugCondition = g_iDebugCondition - 1;
}

void CKGParamEdit::SendRTPParamGroup(unsigned char module, unsigned char group) { RT_rpp_bank_menu(module, group); }
void CKGParamEdit::SendRTPMinValue(unsigned char module, short value) { RT_rpp_min(module, value); }
void CKGParamEdit::SendRTPMaxValue(unsigned char module, short value) { RT_rpp_max(module, value); }
void CKGParamEdit::SendRTPValue(unsigned char module, short value) { RT_rpp_pe_edit(module, value); }

/* Real ground truth returns ulonglong (the tail-called RT_rpp_module's own
 * EAX:EDX pair); this class's pre-existing declared signature (established
 * by CKGCommonParamMsgHandler's own caller, round 27) discards it -- same
 * "eax not part of the real contract" category as several other methods in
 * this project. */
void CKGParamEdit::SendRTPModule(unsigned char module, unsigned char paramIdx, unsigned char subModule)
{
	RT_rpp_module(module, paramIdx, subModule);
}

void CKGParamEdit::SendRTPDestKnob(unsigned char module, char knob) { RT_rpp_pe_control(module, knob, '\0'); }
void CKGParamEdit::SendRTPPolarity(unsigned char module, char polarity) { RT_rpp_pe_polarity(module, polarity); }
void CKGParamEdit::SendDynInputModule(unsigned char zone, unsigned char module) { RT_dyn_src_zone(zone, module); }
void CKGParamEdit::SendDynSource(unsigned char zone, unsigned char source) { RT_dyn_src_menu(zone, source); }
void CKGParamEdit::SendDynDestination(unsigned char zone, unsigned char dest) { RT_dyn_dst_menu(zone, dest); }
void CKGParamEdit::SendDynAction(unsigned char zone, unsigned char action) { RT_dyn_src_action(zone, action); }
void CKGParamEdit::SendDynTop(unsigned char zone, unsigned char top) { RT_dyn_src_top(zone, top); }
void CKGParamEdit::SendDynBottom(unsigned char zone, unsigned char bottom) { RT_dyn_src_bot(zone, bottom); }

void CKGParamEdit::SendDynModule(unsigned char zone, unsigned char module, unsigned char subModule)
{
	RT_dyn_dst_module(zone, module, subModule);
}

void CKGParamEdit::SendDynPolarity(unsigned char zone, unsigned char polarity) { RT_dyn_dst_polarity(zone, polarity); }

void CKGParamEdit::SendChordMemNote(int idx, int note, int module)
{
	RT_chord_trigger_nte((unsigned char)idx, (unsigned char)note, (char)module);
}

void CKGParamEdit::SendChordMemVelocity(int idx, int velocity, unsigned char module)
{
	RT_chord_trigger_vel((unsigned char)idx, (unsigned char)velocity, module);
}

void CKGParamEdit::SendRun(unsigned char module, unsigned char on) { RT_run(module, on); }
void CKGParamEdit::SendKeyBottom(unsigned char module, unsigned char note) { RT_key_zone_bot(module, note); }
void CKGParamEdit::SendKeyTop(unsigned char module, unsigned char note) { RT_key_zone_top(module, note); }
void CKGParamEdit::SendRxAfter(unsigned char module, unsigned char on) { RT_midi_filt_in_tch(module, on); }
void CKGParamEdit::SendRxBend(unsigned char module, unsigned char on) { RT_midi_filt_in_bnd(module, on); }
void CKGParamEdit::SendRxDamper(unsigned char module, unsigned char on) { RT_midi_filt_in_sus(module, on); }
void CKGParamEdit::SendRxJSYP(unsigned char module, unsigned char on) { RT_midi_filt_in_cc1(module, on); }
void CKGParamEdit::SendRxJSYM(unsigned char module, unsigned char on) { RT_midi_filt_in_cc2(module, on); }
void CKGParamEdit::SendRxRibbon(unsigned char module, unsigned char on) { RT_midi_filt_in_cc16(module, on); }
void CKGParamEdit::SendTxBend(unsigned char module, unsigned char on) { RT_midi_filt_out_bnd(module, on); }
void CKGParamEdit::SendTxCCA(unsigned char module, unsigned char on) { RT_midi_filt_out_cca(module, on); }
void CKGParamEdit::SendTxCCB(unsigned char module, unsigned char on) { RT_midi_filt_out_ccb(module, on); }
void CKGParamEdit::SendTxEnv1(unsigned char module, unsigned char on) { RT_midi_filt_out_env(module, '\0', on); }
void CKGParamEdit::SendTxEnv2(unsigned char module, unsigned char on) { RT_midi_filt_out_env(module, '\x01', on); }
void CKGParamEdit::SendTxEnv3(unsigned char module, unsigned char on) { RT_midi_filt_out_env(module, '\x02', on); }
void CKGParamEdit::SendTxNote(unsigned char module, unsigned char on) { RT_midi_filt_out_notes(module, on); }
void CKGParamEdit::SendTxWaveform(unsigned char module, unsigned char on) { RT_midi_filt_out_wav(module, on); }
void CKGParamEdit::SendTranspose(unsigned char module, char semitones) { RT_crb_xpose(module, '\0', semitones); }
void CKGParamEdit::SendForceRange(unsigned char module, unsigned char mode) { RT_nbo_interp(module, mode); }
void CKGParamEdit::SendRootPosition(unsigned char module, unsigned char on) { RT_root_position(module, on); }
void CKGParamEdit::SendDelayMode(unsigned char module, unsigned char mode) { RT_del_start_menu(module, mode); }
void CKGParamEdit::SendDelayTime(unsigned char module, short ms) { RT_del_start_ms(module, ms); }
void CKGParamEdit::SendKeyboardIn(unsigned char module, unsigned char on) { RT_kbd_thru_in(module, on); }
void CKGParamEdit::SendKeyboardOut(unsigned char module, unsigned char on) { RT_kbd_thru_out(module, on); }
void CKGParamEdit::SendKbdInTranspose(unsigned char module, unsigned char semitones) { RT_kbd_thru_in_xpose(module, semitones); }
void CKGParamEdit::SendKbdOutTranspose(unsigned char module, unsigned char semitones) { RT_kbd_thru_out_xpose(module, semitones); }
void CKGParamEdit::SendQuantize(unsigned char module, unsigned char on) { RT_qtz_on(module, on); }
void CKGParamEdit::SendQuantizeWindow(unsigned char module, unsigned char window) { RT_qtz_window(module, window); }
void CKGParamEdit::SendCCSource(unsigned char module, unsigned char cc, char idx) { RT_cc_offset_num(module, cc, idx); }
void CKGParamEdit::SendCCValue(unsigned char module, unsigned char cc, char idx) { RT_cc_offset_val(module, cc, idx); }
void CKGParamEdit::SendNoteTrigMode(unsigned char module, unsigned char mode) { RT_nte_trig_mode(module, mode); }
void CKGParamEdit::SendNoteLatchMode(unsigned char module, unsigned char mode) { RT_nte_latch_mode(module, mode); }

void CKGParamEdit::SendEnvTrigMode(unsigned char module, unsigned char env, unsigned char mode)
{
	RT_env_trig_mode(module, env, mode);
}

void CKGParamEdit::SendEnvLatchMode(unsigned char module, unsigned char env, unsigned char mode)
{
	RT_env_latch_mode(module, env, mode);
}

void CKGParamEdit::SendClkAdvMode(unsigned char module, unsigned char mode) { RT_clk_adv_mode(module, mode); }
void CKGParamEdit::SendClkAdvSize(unsigned char module, unsigned char size) { RT_clk_adv_size_menu(module, size); }
void CKGParamEdit::SendClkAdvTrig(unsigned char module, unsigned char mode) { RT_clk_adv_trig_mode(module, mode); }
void CKGParamEdit::SendClkAdvVelSense(unsigned char module, unsigned char value) { RT_clk_adv_vel_bot(module, value); }
void CKGParamEdit::SendTrigModMode(unsigned char module, unsigned char mode) { RT_mod_trig_mode(module, mode); }
void CKGParamEdit::SendCutoffPercent(unsigned char module, unsigned char percent) { RT_mod_trig_rif_perc(module, percent); }

void CKGParamEdit::SendModCutOff(unsigned char module, unsigned char env, unsigned char mode)
{
	RT_trigger_cutoff(module, env, mode);
}

void CKGParamEdit::SendSeedRhythm(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\0'); }
void CKGParamEdit::SendSeedDuration(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x01'); }
void CKGParamEdit::SendSeedNote(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x02'); }
void CKGParamEdit::SendSeedCluster(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x03'); }
void CKGParamEdit::SendSeedVelocity(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x04'); }
void CKGParamEdit::SendSeedPan(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x05'); }
void CKGParamEdit::SendSeedDrum(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\x06'); }
void CKGParamEdit::SendSeedWaveform(unsigned char module, unsigned char value) { RT_pe_seed_offset(module, value, '\a'); }

/* New 2-arg overload -- .text+0x003c0df0, distinct address from the
 * already-declared 3-arg SendStartSeed (see header comment). */
void CKGParamEdit::SendStartSeed(unsigned char module, long seed) { RT_pe_seed_set_start(module, seed); }

void CKGParamEdit::SendFreezeLoop(unsigned char module, unsigned char on) { RT_pe_freeze_loop(module, on); }
void CKGParamEdit::SendFreezeLoopRetrig(unsigned char module, unsigned char on) { RT_pe_freeze_retrig(module, on); }
void CKGParamEdit::SendNoteMap(unsigned char module, unsigned char table) { RT_nte_map_table(module, table); }
void CKGParamEdit::SendNoteMapTranspose(unsigned char module, unsigned char semitones) { RT_nte_map_xpose(module, (char)semitones); }
void CKGParamEdit::SendNoteMapOnMode(unsigned char module, unsigned char mode) { RT_nte_map_on_mode(module, mode); }
void CKGParamEdit::SendNoteMapChdTrack(unsigned char module, unsigned char on) { RT_nte_map_chd_track(module, on); }
void CKGParamEdit::SendNoteMapKbdTrack(unsigned char module, unsigned char on) { RT_nte_map_kbd_track(module, on); }
void CKGParamEdit::SendForceRangeWrap(unsigned char module, unsigned char mode) { RT_force_range_wrap(module, mode); }
void CKGParamEdit::SendUpdateOnRelease(unsigned char module, unsigned char on) { RT_nbo_off_mode(module, on); }
void CKGParamEdit::SendLinkToDT(unsigned char module, unsigned char on) { RT_dt_link_to_switch(module, on != 0); }
void CKGParamEdit::SendMIDIClockSource(bool external) { KS_sync_mode_x9100(external); }
void CKGParamEdit::SetEnableMIDIInToKarmaModule(bool on) { KS_set_enable_midi_in_to_karma(on); }
void CKGParamEdit::DrumSwitchOn(bool on) { RT_dt_switch(on); }
void CKGParamEdit::DrumSwitchMode(unsigned char mode) { RT_dt_switch_mode(mode); }

/* Shared "scene matrix slot" address, real in all 4 methods below:
 * *(int*)(CKGBankManager::ms_poInstance+8) is itself a pointer, read back
 * (NOT just an offset add), then idx*0x154+0x6cd0 locates the per-scene
 * CKarmaModuleControl slot within it. */
static inline void *SceneMatrixSlot(unsigned char idx)
{
	unsigned char *base = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	return base + (unsigned int)idx * 0x154 + 0x6cd0;
}

void CKGParamEdit::SendLinkedSceneID(unsigned char, unsigned char linkedId, unsigned char)
{
	/* param_1/param_3 genuinely unused in ground truth -- linkedId (the
	 * real param_2) serves as both KS_get_scene_matrix's own 1st arg and
	 * the slot index. */
	KS_get_scene_matrix(linkedId, SceneMatrixSlot(linkedId));
}

void CKGParamEdit::SendSceneIsLinked(unsigned char sceneIdx, unsigned char linked)
{
	RT_rtc_scene_link(sceneIdx, linked != 0);
	KS_get_scene_matrix(sceneIdx, SceneMatrixSlot(sceneIdx));
}

void CKGParamEdit::SendRTCIsLinked(unsigned char sceneIdx, unsigned char on)
{
	RT_rtc_master_enable(sceneIdx, on != 0);
	KS_get_scene_matrix(sceneIdx, SceneMatrixSlot(sceneIdx));
}

void CKGParamEdit::RefreshLinkedSceneDisplay(unsigned char sceneIdx)
{
	KS_get_scene_matrix(sceneIdx, SceneMatrixSlot(sceneIdx));
}
