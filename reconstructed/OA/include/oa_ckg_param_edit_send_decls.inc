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

	/*
	 * CKGCommonParamMsgHandler's own SendXxx() targets (src/engine/
	 * ckg_common_param_handler.cpp) -- same "declare only what's
	 * actually called, bodies out of scope" convention as the Module
	 * group above.
	 */
	void SendTempo(unsigned short);
	void SendTimeSig(unsigned char);
	void SendPadMode(unsigned char);
	void SendBufferSelect(unsigned char);
	void SendLatch(bool);
	void SendOnOff(bool);
	void SendNoteMapTable(int, int);
	void SendSceneChangeQuantize(int);
	void SendDynInputModule(unsigned char, unsigned char);
	void SendDynPolarity(unsigned char, unsigned char);
	void SendDynSource(unsigned char, unsigned char);
	void SendDynDestination(unsigned char, unsigned char);
	void SendDynModule(unsigned char, unsigned char, unsigned char);
	void SendDynAction(unsigned char, unsigned char);
	void SendDynTop(unsigned char, unsigned char);
	void SendDynBottom(unsigned char, unsigned char);
	void SendRTPParamGroup(unsigned char, unsigned char);
	void SendRTPParamAssign(unsigned char, unsigned char);
	void SendRTPModule(unsigned char, unsigned char, unsigned char);
	void SendRTPPolarity(unsigned char, char);
	void SendRTPDestKnob(unsigned char, char);
	void SendRTPMinValue(unsigned char, short);
	void SendRTPMaxValue(unsigned char, short);
	void SendRTPValue(unsigned char, short);
	void SendChordMemNote(int, int, int);
	void SendChordMemVelocity(int, int, unsigned char);
	void SendChordMemChannel(int, unsigned char);
	void SendDTRun(unsigned char, unsigned char);

	/*
	 * CKGGlobalParamMsgHandler's own SendXxx() targets (src/engine/
	 * ckg_global_param_handler.cpp). SetEnableMIDIInToKarmaModule is a
	 * real confirmed exception to the "Send" prefix convention -- its
	 * own real mangled name has no "Send" at all.
	 */
	void SendGlobalTranspose(signed char);
	void SendGlobalMIDICh(signed char);
	void SendMIDIClockSource(bool);
	void SendForceKarmaOff(bool);
	void SetEnableMIDIInToKarmaModule(bool);

	/*
	 * Two more real methods, discovered while reconstructing
	 * CKGControlMsgHandler (src/engine/ckg_control_msg_handler.cpp,
	 * oa_ckg_control_ui_msg.h).
	 */
	void SendNoteMapOctaveReplicate(bool);
	void SendNoteMapTableReset(bool);

	/*
	 * 4 more real methods, discovered while reconstructing the
	 * CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance
	 * widget hierarchy (oa_ckg_switch_family.h). SendChordMemory's
	 * real 3rd parameter is `CKGController::EChangeSource`, a type
	 * declared in a header that (transitively) includes THIS one --
	 * an `asm()` linkage-name override is used instead of the real
	 * enum type to avoid a circular #include, same "explicit real
	 * mangled name, plain-int surface type" technique used any time a
	 * cross-header enum dependency would otherwise be circular. The
	 * real mangled name was read directly off the ground-truth
	 * binary's own relocation, not guessed.
	 */
	void SendFF(bool on);
	void SendRewind(bool on);
	void SendAssign(bool on);
	void SendChordMemory(int index, unsigned char value, int changeSource)
		asm("_ZN12CKGParamEdit15SendChordMemoryEihN13CKGController13EChangeSourceE");