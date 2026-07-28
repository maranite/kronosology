/* SPDX-License-Identifier: GPL-2.0 */
/* Generated list of real CKGParamEdit::SendXxx() member functions actually
 * called from CKGModuleParamMsgHandler's Shape-B methods (see
 * oa_ckg_module_param_msg_handler.h). Own bodies out of scope -- included
 * separately purely to keep the struct body in the main header short. */

	void SendSolo(unsigned char, unsigned char);	/* SetSolo(), the one no-check method */
	void SendAssignableSwitch(int, int, int, bool, bool);	/* Sw1-8Value */
	void SendCCSource(unsigned char, unsigned char, char);
	void SendCCValue(unsigned char, unsigned char, char);
	void SendClkAdvMode(unsigned char, unsigned char);
	void SendClkAdvSize(unsigned char, unsigned char);
	void SendClkAdvTrig(unsigned char, unsigned char);
	void SendClkAdvVelSense(unsigned char, unsigned char);
	void SendCutoffPercent(unsigned char, unsigned char);
	void SendDelayMode(unsigned char, unsigned char);
	void SendDelayTime(unsigned char, short);
	void SendEnvLatchMode(unsigned char, unsigned char, unsigned char);
	void SendEnvTrigMode(unsigned char, unsigned char, unsigned char);
	void SendForceRange(unsigned char, unsigned char);
	void SendForceRangeWrap(unsigned char, unsigned char);
	void SendFreezeLoop(unsigned char, unsigned char);
	void SendFreezeLoopRetrig(unsigned char, unsigned char);
	void SendGEDestKnob(unsigned char, unsigned char, char);
	void SendGEDestKnobForModuleControl(unsigned char, unsigned char, char);
	void SendGEMaxValue(unsigned char, unsigned char, short);
	void SendGEMaxValueForModuleControl(unsigned char, unsigned char, short);
	void SendGEMinValue(unsigned char, unsigned char, short);
	void SendGEMinValueForModuleControl(unsigned char, unsigned char, short);
	void SendGEPolarity(unsigned char, unsigned char, unsigned char);
	void SendGEPolarityForModuleControl(unsigned char, unsigned char, unsigned char);
	void SendGEValue(unsigned char, unsigned char, short);
	void SendGEValueForModuleControl(unsigned char, unsigned char, short);
	void SendInputCh(unsigned char, unsigned char);
	void SendKbdInTranspose(unsigned char, unsigned char);
	void SendKbdOutTranspose(unsigned char, unsigned char);
	void SendKeyBottom(unsigned char, unsigned char);
	void SendKeyTop(unsigned char, unsigned char);
	void SendKeyboardIn(unsigned char, unsigned char);
	void SendKeyboardOut(unsigned char, unsigned char);
	void SendKnob(int, int, int, int, bool);	/* Knob1-8Value */
	void SendLinkedSceneID(unsigned char, unsigned char, unsigned char);	/* SetLinkedSceneId */
	void SendLinkToDT(unsigned char, unsigned char);
	void SendModCutOff(unsigned char, unsigned char, unsigned char);
	void SendNoteLatchMode(unsigned char, unsigned char);
	void SendNoteMap(unsigned char, unsigned char);
	void SendNoteMapChdTrack(unsigned char, unsigned char);
	void SendNoteMapKbdTrack(unsigned char, unsigned char);
	void SendNoteMapOnMode(unsigned char, unsigned char);
	void SendNoteMapTranspose(unsigned char, unsigned char);
	void SendNoteTrigMode(unsigned char, unsigned char);
	void SendOutputCh(unsigned char, unsigned char);
	void SendQuantize(unsigned char, unsigned char);
	void SendQuantizeWindow(unsigned char, unsigned char);
	void SendRTCIsLinked(unsigned char, unsigned char);
	void SendRootPosition(unsigned char, unsigned char);
	void SendRun(unsigned char, unsigned char);
	void SendRxAfter(unsigned char, unsigned char);
	void SendRxBend(unsigned char, unsigned char);
	void SendRxDamper(unsigned char, unsigned char);
	void SendRxJSYM(unsigned char, unsigned char);
	void SendRxJSYP(unsigned char, unsigned char);
	void SendRxOther(unsigned char, unsigned char);
	void SendRxRibbon(unsigned char, unsigned char);
	void SendScene(int, unsigned char, bool);	/* SetScene */
	void SendSceneIsLinked(unsigned char, unsigned char);
	void SendSeedCluster(unsigned char, unsigned char);
	void SendSeedDrum(unsigned char, unsigned char);
	void SendSeedDuration(unsigned char, unsigned char);
	void SendSeedNote(unsigned char, unsigned char);
	void SendSeedPan(unsigned char, unsigned char);
	void SendSeedRhythm(unsigned char, unsigned char);
	void SendSeedVelocity(unsigned char, unsigned char);
	void SendSeedWaveform(unsigned char, unsigned char);
	void SendStartSeed(unsigned char, unsigned char, unsigned char);
	void SendTimbreThru(unsigned char, unsigned char);
	void SendTranspose(unsigned char, char);
	void SendTrigModMode(unsigned char, unsigned char);
	void SendTxBend(unsigned char, unsigned char);
	void SendTxCCA(unsigned char, unsigned char);
	void SendTxCCB(unsigned char, unsigned char);
	void SendTxEnv1(unsigned char, unsigned char);
	void SendTxEnv2(unsigned char, unsigned char);
	void SendTxEnv3(unsigned char, unsigned char);
	void SendTxNote(unsigned char, unsigned char);
	void SendTxWaveform(unsigned char, unsigned char);
	void SendUpdateOnRelease(unsigned char, unsigned char);
	void SendUseGChAlso(unsigned char, unsigned char);

	unsigned int GetRTParmBufferSelectId(int deviceIndex);  /* used by the Shape-C/D Knob/Sw/Scene families */