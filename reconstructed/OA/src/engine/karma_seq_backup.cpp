// SPDX-License-Identifier: GPL-2.0
/*
 * karma_seq_backup.cpp  -  CKGSeqBackupCommonParam / CKGSeqBackupModuleParam,
 * see include/oa_karma_seq_backup.h for the full struct-layout derivation,
 * the GetValue() deferral rationale, and the CKGBankManager/CSPREngine
 * dependency notes.
 *
 * All 195 generic-shape Set* bodies below were transcribed by a scripted
 * instruction-pattern decoder run against `objdump -dr -M intel` output
 * for the real, contiguous `.text+0x3d1200`..`.text+0x3d3830` range (both
 * classes are laid out back-to-back in the real binary) -- not hand-typed
 * one at a time. The decoder recognizes exactly the instruction vocabulary
 * these 197 tiny functions use (mov reg,[this+0/4/8]; lea reg,[reg+reg*N]
 * index-scaling; add reg,[this+disp]; movzx/movsx reg,BYTE/WORD [mem];
 * sar/shr reg,N or reg,cl; and reg,mask; mov [this+0xc],reg or ,imm; ret)
 * and turns each parsed (base, index*stride, disp, width, signed, shift,
 * mask) tuple into the corresponding C expression below. Every one of the
 * 197 real functions was successfully parsed by this decoder (0 unhandled
 * shapes among the 195 generic ones); the 2 that fell outside the generic
 * shape (SetLinkedSceneId's nibble-pack parity branch, SetModCutoff's
 * `idx+4` dynamic shift amount) are written out by hand below, each with
 * its own derivation comment. verify/test_ckg_seq_backup.cpp independently
 * re-derives the expected value for all 195 generic cases from the SAME
 * parsed (offset, width, shift, mask, signed) facts via a separate Python
 * evaluator (not by re-using this file's C strings), so it catches
 * rendering bugs in this file's C output, not just decoder bugs shared by
 * both.
 *
 * Confirmed cross-references to already-reconstructed code: the 18-byte
 * chord-mem-slot stride used throughout the SetChordMemNote(1..8),
 * SetChordMemNote(1..8)Vel, and SetChordMemChannel group exactly matches
 * karma_chord_trigger.cpp's independently-derived "persistent chord
 * definition" stride (real base DAT_00cc1774, 0x12=18 bytes/pad) --
 * strong evidence both classes share the same underlying per-pad chord
 * table.
 */

#include "oa_karma_seq_backup.h"

unsigned char *CSPREngine::ms_poInstance;

/* ================= CKGSeqBackupCommonParam ================= */

/* .text+0x3d1200, 1 byte: real body is a bare `ret` -- no member is
 * initialized. A real caller must call GetValue() first to populate
 * m_source/m_default/m_index before using any Set* accessor. */
CKGSeqBackupCommonParam::CKGSeqBackupCommonParam()
{
}

/* .text+0x3d1210, 40 bytes. Ignores `this` (clears EAX first). */
void *CKGSeqBackupCommonParam::GetKarmaPerfCommonForSeqBackup()
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return 0;
	unsigned int index = *(unsigned int *)(CKGBankManager::ms_poInstance + 0x97c7d4);
	return ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfCommon(index);
}

/* .text+003d1860 */
void CKGSeqBackupCommonParam::SetTempo()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned short *)(src + 0);
}

/* .text+003d1870 */
void CKGSeqBackupCommonParam::SetTimeSig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x3);
}

/* .text+003d1880 */
void CKGSeqBackupCommonParam::SetPadMode()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x2)) >> 5)) & 0x1);
}

/* .text+003d1890 */
void CKGSeqBackupCommonParam::SetModuleControl()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x2)) & 0x7);
}

/* .text+003d18a0 */
void CKGSeqBackupCommonParam::SetLatch()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x2)) >> 6)) & 0x1);
}

/* .text+003d18b0 */
void CKGSeqBackupCommonParam::SetOnOff()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x2)) >> 7);
}

/* .text+003d18c0 */
void CKGSeqBackupCommonParam::SetSwName()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned short *)(dflt + idx*2 + 0x4);
}

/* .text+003d18d0 */
void CKGSeqBackupCommonParam::SetKnobName()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned short *)(dflt + idx*2 + 0x14);
}

/* .text+003d18e0 */
void CKGSeqBackupCommonParam::SetNoteMapTableValue()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)(src + idx + 0x17e);
}

/* .text+003d1900 */
void CKGSeqBackupCommonParam::SetSceneChangeQuantizeValue()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x135)) >> 4);
}

/* .text+003d1920 */
void CKGSeqBackupCommonParam::SetDynMIDIInput()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*6) + 0x24)) & 0x7);
}

/* .text+003d1940 */
void CKGSeqBackupCommonParam::SetDynMIDIPolarity()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x24)) >> 3)) & 0x3);
}

/* .text+003d1960 */
void CKGSeqBackupCommonParam::SetDynMIDISource()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*6) + 0x25);
}

/* .text+003d1980 */
void CKGSeqBackupCommonParam::SetDynMIDIDest()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*6) + 0x26);
}

/* .text+003d19a0 */
void CKGSeqBackupCommonParam::SetDynMIDIUseA()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*6) + 0x27)) & 0x1);
}

/* .text+003d19c0 */
void CKGSeqBackupCommonParam::SetDynMIDIUseB()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x27)) >> 1)) & 0x1);
}

/* .text+003d19e0 */
void CKGSeqBackupCommonParam::SetDynMIDIUseC()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x27)) >> 2)) & 0x1);
}

/* .text+003d1a00 */
void CKGSeqBackupCommonParam::SetDynMIDIUseD()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x27)) >> 3)) & 0x1);
}

/* .text+003d1a20 */
void CKGSeqBackupCommonParam::SetDynMIDIUseLast()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x27)) >> 4)) & 0x1);
}

/* .text+003d1a40 */
void CKGSeqBackupCommonParam::SetDynMIDIUseAction()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*6) + 0x27)) >> 5)) & 0x3);
}

/* .text+003d1a60 */
void CKGSeqBackupCommonParam::SetDynMIDIUseTop()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*6) + 0x28);
}

/* .text+003d1a80 */
void CKGSeqBackupCommonParam::SetDynMIDIUseBottom()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*6) + 0x29);
}

/* .text+003d1aa0 */
void CKGSeqBackupCommonParam::SetRTParamGroup()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*10) + 0x54);
}

/* .text+003d1ac0 */
void CKGSeqBackupCommonParam::SetRTParamAssign()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*10) + 0x55);
}

/* .text+003d1ae0 */
void CKGSeqBackupCommonParam::SetRTParamA()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((*(unsigned char *)((dflt + idx*10) + 0x56)) & 0x1);
}

/* .text+003d1b00 */
void CKGSeqBackupCommonParam::SetRTParamB()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*10) + 0x56)) >> 1)) & 0x1);
}

/* .text+003d1b20 */
void CKGSeqBackupCommonParam::SetRTParamC()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*10) + 0x56)) >> 2)) & 0x1);
}

/* .text+003d1b40 */
void CKGSeqBackupCommonParam::SetRTParamD()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*10) + 0x56)) >> 3)) & 0x1);
}

/* .text+003d1b60 */
void CKGSeqBackupCommonParam::SetRTParamPolarity()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x56)) >> 4)) & 0x7);
}

/* .text+003d1b80 */
void CKGSeqBackupCommonParam::SetRTParamKnob()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x57);
}

/* .text+003d1ba0 */
void CKGSeqBackupCommonParam::SetRTParamMin()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)((dflt + idx*10) + 0x58);
}

/* .text+003d1bc0 */
void CKGSeqBackupCommonParam::SetRTParamMax()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)((dflt + idx*10) + 0x5a);
}

/* .text+003d1be0 */
void CKGSeqBackupCommonParam::SetRTParamValue()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)((dflt + idx*10) + 0x5c);
}

/* .text+003d1c00 */
void CKGSeqBackupCommonParam::SetChordMemNote1()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xa4);
}

/* .text+003d1c20 */
void CKGSeqBackupCommonParam::SetChordMemNote2()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xa6);
}

/* .text+003d1c40 */
void CKGSeqBackupCommonParam::SetChordMemNote3()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xa8);
}

/* .text+003d1c60 */
void CKGSeqBackupCommonParam::SetChordMemNote4()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xaa);
}

/* .text+003d1c80 */
void CKGSeqBackupCommonParam::SetChordMemNote5()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xac);
}

/* .text+003d1ca0 */
void CKGSeqBackupCommonParam::SetChordMemNote6()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xae);
}

/* .text+003d1cc0 */
void CKGSeqBackupCommonParam::SetChordMemNote7()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xb0);
}

/* .text+003d1ce0 */
void CKGSeqBackupCommonParam::SetChordMemNote8()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*18) + 0xb2);
}

/* .text+003d1d00 */
void CKGSeqBackupCommonParam::SetChordMemNote1Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xa5);
}

/* .text+003d1d20 */
void CKGSeqBackupCommonParam::SetChordMemNote2Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xa7);
}

/* .text+003d1d40 */
void CKGSeqBackupCommonParam::SetChordMemNote3Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xa9);
}

/* .text+003d1d60 */
void CKGSeqBackupCommonParam::SetChordMemNote4Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xab);
}

/* .text+003d1d80 */
void CKGSeqBackupCommonParam::SetChordMemNote5Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xad);
}

/* .text+003d1da0 */
void CKGSeqBackupCommonParam::SetChordMemNote6Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xaf);
}

/* .text+003d1dc0 */
void CKGSeqBackupCommonParam::SetChordMemNote7Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xb1);
}

/* .text+003d1de0 */
void CKGSeqBackupCommonParam::SetChordMemNote8Vel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xb3);
}

/* .text+003d1e00 */
void CKGSeqBackupCommonParam::SetChordMemVelocity()
{
	/* .text+003d1e00: real body is a bare `ret` -- confirmed genuine no-op, m_value untouched. */
}

/* .text+003d1e10 */
void CKGSeqBackupCommonParam::SetChordMemChannel()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)((src + idx*18) + 0xb5);
}

/* .text+003d1e30 */
void CKGSeqBackupCommonParam::SetScene()
{
	unsigned char *dflt = (unsigned char *)m_default;
	m_value = ((*(unsigned char *)(dflt + 0x135)) & 0x7);
}

/* .text+003d1e50 */
void CKGSeqBackupCommonParam::SetSw1Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((*(unsigned char *)((dflt + idx*9) + 0x136)) & 0x1);
}

/* .text+003d1e70 */
void CKGSeqBackupCommonParam::SetSw2Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 1)) & 0x1);
}

/* .text+003d1e90 */
void CKGSeqBackupCommonParam::SetSw3Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 2)) & 0x1);
}

/* .text+003d1eb0 */
void CKGSeqBackupCommonParam::SetSw4Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 3)) & 0x1);
}

/* .text+003d1ed0 */
void CKGSeqBackupCommonParam::SetSw5Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 4)) & 0x1);
}

/* .text+003d1ef0 */
void CKGSeqBackupCommonParam::SetSw6Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 5)) & 0x1);
}

/* .text+003d1f10 */
void CKGSeqBackupCommonParam::SetSw7Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 6)) & 0x1);
}

/* .text+003d1f30 */
void CKGSeqBackupCommonParam::SetSw8Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((*(unsigned char *)((dflt + idx*9) + 0x136)) >> 7);
}

/* .text+003d1f50 */
void CKGSeqBackupCommonParam::SetKnob1Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x137);
}

/* .text+003d1f70 */
void CKGSeqBackupCommonParam::SetKnob2Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x138);
}

/* .text+003d1f90 */
void CKGSeqBackupCommonParam::SetKnob3Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x139);
}

/* .text+003d1fb0 */
void CKGSeqBackupCommonParam::SetKnob4Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x13a);
}

/* .text+003d1fd0 */
void CKGSeqBackupCommonParam::SetKnob5Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x13b);
}

/* .text+003d1ff0 */
void CKGSeqBackupCommonParam::SetKnob6Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x13c);
}

/* .text+003d2010 */
void CKGSeqBackupCommonParam::SetKnob7Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x13d);
}

/* .text+003d2030 */
void CKGSeqBackupCommonParam::SetKnob8Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x13e);
}

/* .text+003d2050 */
void CKGSeqBackupCommonParam::SetDTRun()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)(src + 0x134)) >> idx)) & 0x1);
}

/* ================= CKGSeqBackupModuleParam ================= */

/* .text+0x3d2070, 1 byte: real body is a bare `ret`, same as
 * CKGSeqBackupCommonParam's own default ctor above. */
CKGSeqBackupModuleParam::CKGSeqBackupModuleParam()
{
}

/* .text+0x3d2080, 73 bytes. Also ignores `this` (clears EAX first). */
void *CKGSeqBackupModuleParam::GetKarmaPerfModuleForSeqBackup(int moduleIndex)
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return 0;
	unsigned int index = *(unsigned int *)(CKGBankManager::ms_poInstance + 0x97c7d4);
	unsigned char *base = ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfModule(index);
	if (!base)
		return 0;
	return base + moduleIndex * 0x2e8;
}

/* .text+003d2c20 */
void CKGSeqBackupModuleParam::SetGE()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(short *)(src + 0);
}

/* .text+003d2c30 */
void CKGSeqBackupModuleParam::SetSolo()
{
	m_value = 0;
}

/* .text+003d2c40 */
void CKGSeqBackupModuleParam::SetInputCh()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x2)) & 0x1f);
}

/* .text+003d2c50 */
void CKGSeqBackupModuleParam::SetOutputCh()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x3);
}

/* .text+003d2c60 */
void CKGSeqBackupModuleParam::SetKeyTop()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x5);
}

/* .text+003d2c70 */
void CKGSeqBackupModuleParam::SetKeyBottom()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x6);
}

/* .text+003d2c80 */
void CKGSeqBackupModuleParam::SetRxBend()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x14)) & 0x1);
}

/* .text+003d2c90 */
void CKGSeqBackupModuleParam::SetRxAfter()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 1)) & 0x1);
}

/* .text+003d2ca0 */
void CKGSeqBackupModuleParam::SetRxDamper()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 2)) & 0x1);
}

/* .text+003d2cb0 */
void CKGSeqBackupModuleParam::SetRxJSYP()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 3)) & 0x1);
}

/* .text+003d2cc0 */
void CKGSeqBackupModuleParam::SetRxJSYM()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 4)) & 0x1);
}

/* .text+003d2cd0 */
void CKGSeqBackupModuleParam::SetRxRibbon()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 6)) & 0x1);
}

/* .text+003d2ce0 */
void CKGSeqBackupModuleParam::SetRxOther()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x14)) >> 5)) & 0x1);
}

/* .text+003d2cf0 */
void CKGSeqBackupModuleParam::SetTxBend()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x13)) & 0x1);
}

/* .text+003d2d00 */
void CKGSeqBackupModuleParam::SetTxCCA()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 1)) & 0x1);
}

/* .text+003d2d10 */
void CKGSeqBackupModuleParam::SetTxCCB()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 2)) & 0x1);
}

/* .text+003d2d20 */
void CKGSeqBackupModuleParam::SetTxEnv1()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 3)) & 0x1);
}

/* .text+003d2d30 */
void CKGSeqBackupModuleParam::SetTxEnv2()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 4)) & 0x1);
}

/* .text+003d2d40 */
void CKGSeqBackupModuleParam::SetTxEnv3()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 5)) & 0x1);
}

/* .text+003d2d50 */
void CKGSeqBackupModuleParam::SetTxNote()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x13)) >> 6)) & 0x1);
}

/* .text+003d2d60 */
void CKGSeqBackupModuleParam::SetTxWaveform()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x13)) >> 7);
}

/* .text+003d2d70 */
void CKGSeqBackupModuleParam::SetTranspose()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(signed char *)(src + 0x4);
}

/* .text+003d2d80 */
void CKGSeqBackupModuleParam::SetCollapse()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x7)) >> 3)) & 0x7);
}

/* .text+003d2d90 */
void CKGSeqBackupModuleParam::SetForceRangeWrap()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x192);
}

/* .text+003d2da0 */
void CKGSeqBackupModuleParam::SetTZoneBypass()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 1)) & 0x1);
}

/* .text+003d2dc0 */
void CKGSeqBackupModuleParam::SetDelayTime()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(short *)(src + 0xa);
}

/* .text+003d2dd0 */
void CKGSeqBackupModuleParam::SetDelayMode()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0xc);
}

/* .text+003d2de0 */
void CKGSeqBackupModuleParam::SetRun()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 5)) & 0x1);
}

/* .text+003d2e00 */
void CKGSeqBackupModuleParam::SetKbdInZone()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 3)) & 0x1);
}

/* .text+003d2e20 */
void CKGSeqBackupModuleParam::SetKbdOutZone()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 4)) & 0x1);
}

/* .text+003d2e40 */
void CKGSeqBackupModuleParam::SetQuantize()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x126)) & 0x1);
}

/* .text+003d2e50 */
void CKGSeqBackupModuleParam::SetThru()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 2)) & 0x1);
}

/* .text+003d2e70 */
void CKGSeqBackupModuleParam::SetRootPosition()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x7)) & 0x1);
}

/* .text+003d2e80 */
void CKGSeqBackupModuleParam::SetGenCC()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)(src + idx*2 + 0x11e);
}

/* .text+003d2ea0 */
void CKGSeqBackupModuleParam::SetGenCCValue()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)(src + idx*2 + 0x11f);
}

/* .text+003d2ec0 */
void CKGSeqBackupModuleParam::SetNoteTrig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0xf)) & 0xf);
}

/* .text+003d2ed0 */
void CKGSeqBackupModuleParam::SetNoteLatch()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0xf)) >> 4)) & 0x1);
}

/* .text+003d2ee0 */
void CKGSeqBackupModuleParam::SetEnv1Trig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x10)) & 0xf);
}

/* .text+003d2ef0 */
void CKGSeqBackupModuleParam::SetEnv2Trig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x11)) & 0xf);
}

/* .text+003d2f00 */
void CKGSeqBackupModuleParam::SetEnv3Trig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x12)) & 0xf);
}

/* .text+003d2f10 */
void CKGSeqBackupModuleParam::SetEnv1Latch()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x10)) >> 4);
}

/* .text+003d2f20 */
void CKGSeqBackupModuleParam::SetEnv2Latch()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x11)) >> 4);
}

/* .text+003d2f30 */
void CKGSeqBackupModuleParam::SetEnv3Latch()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x12)) >> 4);
}

/* .text+003d2f40 */
void CKGSeqBackupModuleParam::SetClkAdvMode()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x7)) >> 6);
}

/* .text+003d2f50 */
void CKGSeqBackupModuleParam::SetClkAdvSize()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x8)) >> 4);
}

/* .text+003d2f60 */
void CKGSeqBackupModuleParam::SetClkAdvCtrig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x8)) & 0xf);
}

/* .text+003d2f70 */
void CKGSeqBackupModuleParam::SetClkAdvVSence()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x9);
}

/* .text+003d2f80 */
void CKGSeqBackupModuleParam::SetTrigModule()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0xd)) & 0xf);
}

/* .text+003d2f90 */
void CKGSeqBackupModuleParam::SetModPercent()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0xe);
}

/* .text+003d2fc0 */
void CKGSeqBackupModuleParam::SetKIZoneTrans()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(signed char *)(src + 0x17);
}

/* .text+003d2fd0 */
void CKGSeqBackupModuleParam::SetKOZoneTrans()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(signed char *)(src + 0x18);
}

/* .text+003d2fe0 */
void CKGSeqBackupModuleParam::SetRndRhythm()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x15)) & 0x3);
}

/* .text+003d2ff0 */
void CKGSeqBackupModuleParam::SetRndDuration()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x15)) >> 2)) & 0x3);
}

/* .text+003d3000 */
void CKGSeqBackupModuleParam::SetRndNote()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x15)) >> 4)) & 0x3);
}

/* .text+003d3010 */
void CKGSeqBackupModuleParam::SetRndCluster()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x15)) >> 6);
}

/* .text+003d3020 */
void CKGSeqBackupModuleParam::SetRndVelocity()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x16)) & 0x3);
}

/* .text+003d3030 */
void CKGSeqBackupModuleParam::SetRndPan()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x16)) >> 2)) & 0x3);
}

/* .text+003d3040 */
void CKGSeqBackupModuleParam::SetRndDrum()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x16)) >> 4)) & 0x3);
}

/* .text+003d3050 */
void CKGSeqBackupModuleParam::SetRndWaveform()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x16)) >> 6);
}

/* .text+003d3060 */
void CKGSeqBackupModuleParam::SetSeed()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(unsigned char *)(src + idx + 0x1a);
}

/* .text+003d3070 */
void CKGSeqBackupModuleParam::SetFreezeLoop()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x19)) & 0x3f);
}

/* .text+003d3080 */
void CKGSeqBackupModuleParam::SetFreezeRetrig()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x19)) >> 7);
}

/* .text+003d3090 */
void CKGSeqBackupModuleParam::SetUseGChAlso()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x2)) >> 7);
}

/* .text+003d30a0 */
void CKGSeqBackupModuleParam::SetNoteMap()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(unsigned char *)(src + 0x190);
}

/* .text+003d30b0 */
void CKGSeqBackupModuleParam::SetNoteMapTranspose()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = *(signed char *)(src + 0x193);
}

/* .text+003d30c0 */
void CKGSeqBackupModuleParam::SetNoteMapOnMode()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x191)) & 0x3);
}

/* .text+003d30d0 */
void CKGSeqBackupModuleParam::SetNoteMapChdTrack()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x191)) >> 6)) & 0x1);
}

/* .text+003d30f0 */
void CKGSeqBackupModuleParam::SetNoteMapKbdTrack()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x191)) >> 7);
}

/* .text+003d3110 */
void CKGSeqBackupModuleParam::SetUseNoteOffs()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((((*(unsigned char *)(src + 0x126)) >> 6)) & 0x1);
}

/* .text+003d3130 */
void CKGSeqBackupModuleParam::SetValue()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x24);
}

/* .text+003d3140 */
void CKGSeqBackupModuleParam::SetMinValue()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x20);
}

/* .text+003d3150 */
void CKGSeqBackupModuleParam::SetMaxValue()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x22);
}

/* .text+003d3160 */
void CKGSeqBackupModuleParam::SetKnob()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(signed char *)(dflt + idx*8 + 0x1e);
}

/* .text+003d3170 */
void CKGSeqBackupModuleParam::SetPolarity()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)(dflt + idx*8 + 0x1f);
}

/* .text+003d3180 */
void CKGSeqBackupModuleParam::SetValueForModuleControl()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x19a);
}

/* .text+003d31a0 */
void CKGSeqBackupModuleParam::SetMinValueForModuleControl()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x196);
}

/* .text+003d31c0 */
void CKGSeqBackupModuleParam::SetMaxValueForModuleControl()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(short *)(dflt + idx*8 + 0x198);
}

/* .text+003d31e0 */
void CKGSeqBackupModuleParam::SetKnobForModuleControl()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(signed char *)(dflt + idx*8 + 0x194);
}

/* .text+003d3200 */
void CKGSeqBackupModuleParam::SetPolarityForModuleControl()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)(dflt + idx*8 + 0x195);
}

/* .text+003d3270 */
void CKGSeqBackupModuleParam::SetSceneIsLinked()
{
	unsigned char *dflt = (unsigned char *)m_default;
	m_value = ((((*(unsigned char *)(dflt + 0x2e4)) >> 3)) & 0x1);
}

/* .text+003d3290 */
void CKGSeqBackupModuleParam::SetRTCIsLinked()
{
	unsigned char *dflt = (unsigned char *)m_default;
	m_value = ((*(unsigned char *)(dflt + 0x2e4)) >> 7);
}

/* .text+003d32b0 */
void CKGSeqBackupModuleParam::SetModifiedSw1Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*10) + 0x294)) & 0x1);
}

/* .text+003d32d0 */
void CKGSeqBackupModuleParam::SetModifiedSw2Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 1)) & 0x1);
}

/* .text+003d32f0 */
void CKGSeqBackupModuleParam::SetModifiedSw3Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 2)) & 0x1);
}

/* .text+003d3310 */
void CKGSeqBackupModuleParam::SetModifiedSw4Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 3)) & 0x1);
}

/* .text+003d3330 */
void CKGSeqBackupModuleParam::SetModifiedSw5Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 4)) & 0x1);
}

/* .text+003d3350 */
void CKGSeqBackupModuleParam::SetModifiedSw6Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 5)) & 0x1);
}

/* .text+003d3370 */
void CKGSeqBackupModuleParam::SetModifiedSw7Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x294)) >> 6)) & 0x1);
}

/* .text+003d3390 */
void CKGSeqBackupModuleParam::SetModifiedSw8Value()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*10) + 0x294)) >> 7);
}

/* .text+003d33b0 */
void CKGSeqBackupModuleParam::SetModifiedSw1Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*10) + 0x295)) & 0x1);
}

/* .text+003d33d0 */
void CKGSeqBackupModuleParam::SetModifiedSw2Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 1)) & 0x1);
}

/* .text+003d33f0 */
void CKGSeqBackupModuleParam::SetModifiedSw3Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 2)) & 0x1);
}

/* .text+003d3410 */
void CKGSeqBackupModuleParam::SetModifiedSw4Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 3)) & 0x1);
}

/* .text+003d3430 */
void CKGSeqBackupModuleParam::SetModifiedSw5Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 4)) & 0x1);
}

/* .text+003d3450 */
void CKGSeqBackupModuleParam::SetModifiedSw6Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 5)) & 0x1);
}

/* .text+003d3470 */
void CKGSeqBackupModuleParam::SetModifiedSw7Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((src + idx*10) + 0x295)) >> 6)) & 0x1);
}

/* .text+003d3490 */
void CKGSeqBackupModuleParam::SetModifiedSw8Status()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)((src + idx*10) + 0x295)) >> 7);
}

/* .text+003d34b0 */
void CKGSeqBackupModuleParam::SetModifiedKnob1()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x296);
}

/* .text+003d34d0 */
void CKGSeqBackupModuleParam::SetModifiedKnob2()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x297);
}

/* .text+003d34f0 */
void CKGSeqBackupModuleParam::SetModifiedKnob3()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x298);
}

/* .text+003d3510 */
void CKGSeqBackupModuleParam::SetModifiedKnob4()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x299);
}

/* .text+003d3530 */
void CKGSeqBackupModuleParam::SetModifiedKnob5()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x29a);
}

/* .text+003d3550 */
void CKGSeqBackupModuleParam::SetModifiedKnob6()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x29b);
}

/* .text+003d3570 */
void CKGSeqBackupModuleParam::SetModifiedKnob7()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x29c);
}

/* .text+003d3590 */
void CKGSeqBackupModuleParam::SetModifiedKnob8()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = *(signed char *)((src + idx*10) + 0x29d);
}

/* .text+003d35b0 */
void CKGSeqBackupModuleParam::SetSwName()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned short *)(dflt + idx*2 + 0x138);
}

/* .text+003d35d0 */
void CKGSeqBackupModuleParam::SetKnobName()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned short *)(dflt + idx*2 + 0x138);
}

/* .text+003d35f0 */
void CKGSeqBackupModuleParam::SetScene()
{
	unsigned char *dflt = (unsigned char *)m_default;
	m_value = *(unsigned char *)(dflt + 0x127);
}

/* .text+003d3600 */
void CKGSeqBackupModuleParam::SetSw1Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((*(unsigned char *)((dflt + idx*9) + 0x148)) & 0x1);
}

/* .text+003d3620 */
void CKGSeqBackupModuleParam::SetSw2Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 1)) & 0x1);
}

/* .text+003d3640 */
void CKGSeqBackupModuleParam::SetSw3Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 2)) & 0x1);
}

/* .text+003d3660 */
void CKGSeqBackupModuleParam::SetSw4Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 3)) & 0x1);
}

/* .text+003d3680 */
void CKGSeqBackupModuleParam::SetSw5Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 4)) & 0x1);
}

/* .text+003d36a0 */
void CKGSeqBackupModuleParam::SetSw6Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 5)) & 0x1);
}

/* .text+003d36c0 */
void CKGSeqBackupModuleParam::SetSw7Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 6)) & 0x1);
}

/* .text+003d36e0 */
void CKGSeqBackupModuleParam::SetSw8Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = ((*(unsigned char *)((dflt + idx*9) + 0x148)) >> 7);
}

/* .text+003d3700 */
void CKGSeqBackupModuleParam::SetKnob1Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x149);
}

/* .text+003d3720 */
void CKGSeqBackupModuleParam::SetKnob2Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14a);
}

/* .text+003d3740 */
void CKGSeqBackupModuleParam::SetKnob3Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14b);
}

/* .text+003d3760 */
void CKGSeqBackupModuleParam::SetKnob4Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14c);
}

/* .text+003d3780 */
void CKGSeqBackupModuleParam::SetKnob5Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14d);
}

/* .text+003d37a0 */
void CKGSeqBackupModuleParam::SetKnob6Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14e);
}

/* .text+003d37c0 */
void CKGSeqBackupModuleParam::SetKnob7Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x14f);
}

/* .text+003d37e0 */
void CKGSeqBackupModuleParam::SetKnob8Value()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	m_value = *(unsigned char *)((dflt + idx*9) + 0x150);
}

/* .text+003d3800 */
void CKGSeqBackupModuleParam::SetQuantizeWindow()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0xf)) >> 5);
}

/* .text+003d3810 */
void CKGSeqBackupModuleParam::SetLinkToDT()
{
	unsigned char *src = (unsigned char *)m_source;
	m_value = ((*(unsigned char *)(src + 0x126)) >> 7);
}

/*
 * .text+0x3d3220, 61 bytes. Falls outside the generic decoder shape: a
 * real parity branch (`test dl,1; jne odd`) picks between two nibbles of
 * one packed byte instead of a plain shift+mask. Reconstructed by hand:
 *   even path (idx&1==0, the real fall-through, no jump taken):
 *     v = dflt[0x2e4 + (idx>>1)]; result = v & 7;
 *   odd path (idx&1==1, real `jne` target at .text+0x3d3248):
 *     v = dflt[0x2e4 + (idx>>1)]; result = (v >> 4) & 7;
 * i.e. a 2-scenes-per-byte, 3-bit-field, low-nibble/high-nibble pack.
 */
void CKGSeqBackupModuleParam::SetLinkedSceneId()
{
	unsigned char *dflt = (unsigned char *)m_default;
	int idx = m_index;
	unsigned char packed = dflt[0x2e4 + (idx >> 1)];
	m_value = (idx & 1) ? ((packed >> 4) & 0x7) : (packed & 0x7);
}

/*
 * .text+0x3d2fa0, 21 bytes. Falls outside the generic decoder shape: the
 * shift amount is `idx + 4` (real `add ecx,0x4` before `sar edx,cl`),
 * not a plain `idx` or constant -- every other dynamic-shift setter in
 * this cluster (SetDTRun) shifts by a bare idx. Reconstructed by hand:
 *   m_value = (src[0xd] >> (idx + 4)) & 1;
 */
void CKGSeqBackupModuleParam::SetModCutoff()
{
	unsigned char *src = (unsigned char *)m_source;
	int idx = m_index;
	m_value = ((*(unsigned char *)(src + 0xd)) >> (idx + 4)) & 0x1;
}
