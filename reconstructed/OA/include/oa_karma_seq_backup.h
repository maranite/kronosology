// SPDX-License-Identifier: GPL-2.0
#ifndef OA_KARMA_SEQ_BACKUP_H
#define OA_KARMA_SEQ_BACKUP_H

#include "oa_engine_init.h"	/* CKGBankManager, CSPREngine */

/*
 * oa_karma_seq_backup.h  -  CKGSeqBackupCommonParam / CKGSeqBackupModuleParam:
 * the KARMA sequencer's "read one live Karma-perf parameter into a scratch
 * backup slot" accessor pair, used by the Karma step-record / undo backup
 * path (name confirmed by the real mangled class names; the actual
 * caller that iterates these was not traced -- out of scope for this
 * batch, which only reconstructs the two classes' own real methods).
 *
 * Ground truth: `CKGSeqBackupCommonParam::*` (`.text+0x3d1200`..
 * `.text+0x3d2070`, 72 real methods) and `CKGSeqBackupModuleParam::*`
 * (`.text+0x3d2070`..`.text+0x3d3830`, 131 real methods), transcribed
 * from `objdump -dr -M intel` against the real OA.ko object directly
 * (same methodology as CSTGLFO/CSTGADSRBase/karma_chord_trigger.cpp).
 *
 * === Common struct layout (both classes, confirmed identical shape) ===
 *   +0x00  m_source   void*  live KarmaPerf record pointer (Common or
 *                            Module, depending on class), populated by
 *                            GetValue() (or NULL if the CSPREngine gate
 *                            is closed / bank manager returned no live
 *                            record) -- see GetValue() note below. Most
 *                            Set* methods read through this.
 *   +0x04  m_default  void*  "default" KarmaPerf record pointer, ALSO
 *                            populated by GetValue() (via
 *                            `CKGBankManager::GetSeqDefaultKarmaPerf
 *                            Common()`), consulted unconditionally (not
 *                            gated on m_source being null) by a subset of
 *                            Set* methods -- e.g. SetSwName/SetKnobName,
 *                            the Sw/Knob-assignment value groups, and the
 *                            RTParam group's A/B/C/D/Min/Max/Value
 *                            fields. Confirmed real (not a guess): these
 *                            methods' own disassembly reads `[eax+0x4]`,
 *                            never `[eax]`.
 *   +0x08  m_index    int    sub-index (assignable-knob/switch number,
 *                            chord-mem note slot, dynamic-MIDI-mapping
 *                            slot, etc.), populated by GetValue()'s
 *                            `subIndex` argument.
 *   +0x0c  m_value    long   the computed backup value -- every Set*
 *                            method's sole output, and also GetValue()'s
 *                            `*out` return path.
 *
 * All 4 fields are left uninitialized by the (real, empty -- confirmed
 * `ret` with no body) default constructor; a real caller is expected to
 * call GetValue() first to populate them before using any individual
 * Set* accessor. This is a faithful transcription of that real behaviour,
 * not a simplification.
 *
 * === GetValue(int paramIndex, int subIndex, long *out) -- DEFERRED ===
 * Both classes' own GetValue() (CommonParam: `.text+0x3d1240`, 1555
 * bytes / 69-case jump table over paramIndex 0..0x44; ModuleParam:
 * `.text+0x3d20d0`, 2893 bytes / 128-case table) is a real, SEPARATE,
 * fully self-contained function -- NOT a thin wrapper that calls the
 * named Set* methods below. Its case bodies are genuinely duplicate
 * compiled code: same field offset/width/shift/mask per case as the
 * correspondingly-named Set* method (confirmed by direct comparison of
 * several cases against their named siblings), just additionally
 * storing through `*out` and returning 1 (vs. 0 for "no live record" /
 * "index out of range"). Deliberately deferred this pass: reconstructing
 * it faithfully needs the exact case-index -> field mapping pinned down
 * (not just "looks the same shape"), which is real additional
 * verification work distinct from what this pass's KAT covers. Left
 * undeclared here (same "don't declare what isn't verified yet"
 * convention as CSTGLFO's ProcessSubRate) rather than ship an unverified
 * index mapping.
 *
 * === CKGBankManager / CSPREngine dependencies ===
 * GetKarmaPerf{Common,Module}ForSeqBackup() (this batch's own 2 tiny
 * "static-like" helpers -- both genuinely ignore `this`, confirmed: the
 * compiler clears EAX as their very first instruction) call 3 real
 * CKGBankManager methods and read CSPREngine::ms_poInstance's gate byte
 * -- see oa_engine_init.h for both classes' own declarations/derivation
 * notes. Their own bodies stay genuinely unresolved (real "Unknown
 * symbol" at `make ko`, expected per this project's own Makefile note).
 */

struct CKGSeqBackupCommonParam {
	void *m_source;
	void *m_default;
	int   m_index;
	long  m_value;

	CKGSeqBackupCommonParam();

	/* .text+0x3d1210, 40 bytes. Ignores `this` entirely (confirmed:
	 * clears EAX as its first instruction) -- returns
	 * CKGBankManager::GetSeqKarmaPerfCommon(bankMgr[+0x97c7d4]) when
	 * CSPREngine::ms_poInstance[+0xa] != 0, else NULL. */
	void *GetKarmaPerfCommonForSeqBackup();

	void SetTempo();
	void SetTimeSig();
	void SetPadMode();
	void SetModuleControl();
	void SetLatch();
	void SetOnOff();
	void SetSwName();
	void SetKnobName();
	void SetNoteMapTableValue();
	void SetSceneChangeQuantizeValue();
	void SetDynMIDIInput();
	void SetDynMIDIPolarity();
	void SetDynMIDISource();
	void SetDynMIDIDest();
	void SetDynMIDIUseA();
	void SetDynMIDIUseB();
	void SetDynMIDIUseC();
	void SetDynMIDIUseD();
	void SetDynMIDIUseLast();
	void SetDynMIDIUseAction();
	void SetDynMIDIUseTop();
	void SetDynMIDIUseBottom();
	void SetRTParamGroup();
	void SetRTParamAssign();
	void SetRTParamA();
	void SetRTParamB();
	void SetRTParamC();
	void SetRTParamD();
	void SetRTParamPolarity();
	void SetRTParamKnob();
	void SetRTParamMin();
	void SetRTParamMax();
	void SetRTParamValue();
	void SetChordMemNote1();
	void SetChordMemNote2();
	void SetChordMemNote3();
	void SetChordMemNote4();
	void SetChordMemNote5();
	void SetChordMemNote6();
	void SetChordMemNote7();
	void SetChordMemNote8();
	void SetChordMemNote1Vel();
	void SetChordMemNote2Vel();
	void SetChordMemNote3Vel();
	void SetChordMemNote4Vel();
	void SetChordMemNote5Vel();
	void SetChordMemNote6Vel();
	void SetChordMemNote7Vel();
	void SetChordMemNote8Vel();
	void SetChordMemVelocity();
	void SetChordMemChannel();
	void SetScene();
	void SetSw1Value();
	void SetSw2Value();
	void SetSw3Value();
	void SetSw4Value();
	void SetSw5Value();
	void SetSw6Value();
	void SetSw7Value();
	void SetSw8Value();
	void SetKnob1Value();
	void SetKnob2Value();
	void SetKnob3Value();
	void SetKnob4Value();
	void SetKnob5Value();
	void SetKnob6Value();
	void SetKnob7Value();
	void SetKnob8Value();
	void SetDTRun();
};

struct CKGSeqBackupModuleParam {
	void *m_source;
	void *m_default;
	int   m_index;
	long  m_value;

	CKGSeqBackupModuleParam();

	/* .text+0x3d2080, 73 bytes. Also ignores `this` (same "clears EAX
	 * first" confirmation) -- takes an explicit `moduleIndex` argument
	 * and returns
	 * CKGBankManager::GetSeqKarmaPerfModule(bankMgr[+0x97c7d4])
	 *   + moduleIndex * 0x2e8
	 * (0x2e8 = 744 bytes/module record, confirmed real `imul ebx,ebx,0x2e8`),
	 * or NULL if GetSeqKarmaPerfModule() itself returned NULL. */
	void *GetKarmaPerfModuleForSeqBackup(int moduleIndex);

	void SetGE();
	void SetSolo();
	void SetInputCh();
	void SetOutputCh();
	void SetKeyTop();
	void SetKeyBottom();
	void SetRxBend();
	void SetRxAfter();
	void SetRxDamper();
	void SetRxJSYP();
	void SetRxJSYM();
	void SetRxRibbon();
	void SetRxOther();
	void SetTxBend();
	void SetTxCCA();
	void SetTxCCB();
	void SetTxEnv1();
	void SetTxEnv2();
	void SetTxEnv3();
	void SetTxNote();
	void SetTxWaveform();
	void SetTranspose();
	void SetCollapse();
	void SetForceRangeWrap();
	void SetTZoneBypass();
	void SetDelayTime();
	void SetDelayMode();
	void SetRun();
	void SetKbdInZone();
	void SetKbdOutZone();
	void SetQuantize();
	void SetThru();
	void SetRootPosition();
	void SetGenCC();
	void SetGenCCValue();
	void SetNoteTrig();
	void SetNoteLatch();
	void SetEnv1Trig();
	void SetEnv2Trig();
	void SetEnv3Trig();
	void SetEnv1Latch();
	void SetEnv2Latch();
	void SetEnv3Latch();
	void SetClkAdvMode();
	void SetClkAdvSize();
	void SetClkAdvCtrig();
	void SetClkAdvVSence();
	void SetTrigModule();
	void SetModPercent();
	void SetModCutoff();
	void SetKIZoneTrans();
	void SetKOZoneTrans();
	void SetRndRhythm();
	void SetRndDuration();
	void SetRndNote();
	void SetRndCluster();
	void SetRndVelocity();
	void SetRndPan();
	void SetRndDrum();
	void SetRndWaveform();
	void SetSeed();
	void SetFreezeLoop();
	void SetFreezeRetrig();
	void SetUseGChAlso();
	void SetNoteMap();
	void SetNoteMapTranspose();
	void SetNoteMapOnMode();
	void SetNoteMapChdTrack();
	void SetNoteMapKbdTrack();
	void SetUseNoteOffs();
	void SetValue();
	void SetMinValue();
	void SetMaxValue();
	void SetKnob();
	void SetPolarity();
	void SetValueForModuleControl();
	void SetMinValueForModuleControl();
	void SetMaxValueForModuleControl();
	void SetKnobForModuleControl();
	void SetPolarityForModuleControl();
	void SetLinkedSceneId();
	void SetSceneIsLinked();
	void SetRTCIsLinked();
	void SetModifiedSw1Value();
	void SetModifiedSw2Value();
	void SetModifiedSw3Value();
	void SetModifiedSw4Value();
	void SetModifiedSw5Value();
	void SetModifiedSw6Value();
	void SetModifiedSw7Value();
	void SetModifiedSw8Value();
	void SetModifiedSw1Status();
	void SetModifiedSw2Status();
	void SetModifiedSw3Status();
	void SetModifiedSw4Status();
	void SetModifiedSw5Status();
	void SetModifiedSw6Status();
	void SetModifiedSw7Status();
	void SetModifiedSw8Status();
	void SetModifiedKnob1();
	void SetModifiedKnob2();
	void SetModifiedKnob3();
	void SetModifiedKnob4();
	void SetModifiedKnob5();
	void SetModifiedKnob6();
	void SetModifiedKnob7();
	void SetModifiedKnob8();
	void SetSwName();
	void SetKnobName();
	void SetScene();
	void SetSw1Value();
	void SetSw2Value();
	void SetSw3Value();
	void SetSw4Value();
	void SetSw5Value();
	void SetSw6Value();
	void SetSw7Value();
	void SetSw8Value();
	void SetKnob1Value();
	void SetKnob2Value();
	void SetKnob3Value();
	void SetKnob4Value();
	void SetKnob5Value();
	void SetKnob6Value();
	void SetKnob7Value();
	void SetKnob8Value();
	void SetQuantizeWindow();
	void SetLinkToDT();
};

#endif
