// SPDX-License-Identifier: GPL-2.0
/*
 * alias_bank_convert.cpp  -  USTGAliasBankTypes's four MIDI-bank <->
 * internal-bank conversion helpers (sec 10.152):
 *   ConvertAliasPgmBankToMidiBank / ConvertCombiBankToMidiBank
 *   ConvertMidiBankToCombiBank    / ConvertMidiBankToAliasProgramBank
 *
 * Deliberately a separate translation unit from global.cpp, matching
 * alias_bank_init.cpp's own precedent right above these declarations
 * in oa_global.h: all four symbols' mocks in test_engine.cpp/
 * test_global.cpp/test_global_ctor.cpp are load-bearing call-counters
 * for SendPerfChangeToMidiOut()/HandleMidiBankAndPerformanceChange()'s
 * own dispatch verification (sec 10.98/10.99) -- none of those three
 * files link this TU, so their existing mocks are untouched.
 */

#include "oa_global.h"

/*
 * ConvertCombiBankToMidiBank(int bankId, char &out1, char &out2)
 * (.text+0x25660, 157 bytes) confirmed:
 *   out1 = CSTGGlobal::sInstance->fieldAt(0x6e4)!=0 ? 0x3f : 0  -- ALWAYS
 *          written first, regardless of bankId's validity (the same
 *          "GM2/extended bank select MSB" flag ConvertMidiBankToCombiBank
 *          below expects back as its own first argument on a round trip).
 *   out2 = 0 if bankId is out of range (unsigned bankId > 13); otherwise
 *          a literal per-bankId table, confirmed via `readelf -r`
 *          jump-table dereference: {0,1,2,3,4,5,6,8,9,0xa,0xb,0xc,0xd,0xe}
 *          -- a genuine, confirmed real gap (bankId 6 and 7 BOTH map
 *          differently: 6->6, 7->8, value 7 itself is never produced).
 */
void USTGAliasBankTypes::ConvertCombiBankToMidiBank(int bankId, char &out1, char &out2)
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	out1 = (g[0x6e4] != 0) ? (char)0x3f : (char)0;

	static const unsigned char kMidiBankForCombiBank[14] = {
		0, 1, 2, 3, 4, 5, 6, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe,
	};
	if ((unsigned int)bankId > 13) {
		out2 = 0;
		return;
	}
	out2 = (char)kMidiBankForCombiBank[bankId];
}

/*
 * ConvertMidiBankToCombiBank(char midiBankMsb, char midiBankLsb, int &outBankId)
 * (.text+0x25700, 184 bytes) confirmed: the inverse of
 * ConvertCombiBankToMidiBank above -- NOT a literal mirror of that
 * function's own table, confirmed via `readelf -r`'s own SEPARATE
 * 15-entry jump table at .rodata+0xa570 (an easy trap: the two
 * directions' gaps land on DIFFERENT indices, not the same one).
 *   outBankId = 0 by default (written unconditionally up front).
 *   Requires midiBankMsb to match the SAME flag ConvertCombiBankToMidiBank
 *   produces for the current CSTGGlobal::sInstance->fieldAt(0x6e4) mode:
 *   mode==0 requires midiBankMsb==0; mode!=0 requires midiBankMsb==0x3f.
 *   Any other midiBankMsb value (or midiBankLsb > 0xe) leaves outBankId
 *   at its default 0. Valid (midiBankLsb 0..0xe) values come from a
 *   literal per-midiBankLsb table (confirmed via the real jump table's
 *   own case-body immediates): {0,1,2,3,4,5,6,0,7,8,9,0xa,0xb,0xc,0xd}
 *   -- midiBankLsb==7 is a genuine confirmed gap (stays default 0),
 *   the mirror image of ConvertCombiBankToMidiBank's own bankId==7
 *   producing midiBank==8 (never 7) above.
 */
void USTGAliasBankTypes::ConvertMidiBankToCombiBank(char midiBankMsb, char midiBankLsb, int &outBankId)
{
	outBankId = 0;

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	bool gm2Mode = (g[0x6e4] != 0);
	unsigned char msb = (unsigned char)midiBankMsb;
	if (gm2Mode) {
		if (msb != 0x3f)
			return;
	} else {
		if (msb != 0)
			return;
	}

	unsigned char lsb = (unsigned char)midiBankLsb;
	if (lsb > 0xe)
		return;

	static const unsigned char kCombiBankForMidiBank[15] = {
		0, 1, 2, 3, 4, 5, 6, 0 /* gap */, 7, 8, 9, 0xa, 0xb, 0xc, 0xd,
	};
	outBankId = kCombiBankForMidiBank[lsb];
}

/*
 * ConvertAliasPgmBankToMidiBank(int bankId, char &out1, char &out2)
 * (.text+0x25490, 456 bytes) confirmed: bankId is a 0..30 "alias
 * program bank" index (eSTGAliasProgramBankId, 31 values total,
 * matching InitializeAliasBanks()'s own 30-block table's index range
 * -- sec 10.85). Unlike the Combi conversion above, out1 (MIDI bank
 * MSB) and out2 (MIDI bank LSB / internal program-bank id) are BOTH
 * looked up directly from ONE OF TWO literal 31-entry tables selected
 * by CSTGGlobal::sInstance->fieldAt(0x6e4) (confirmed via `readelf -r`,
 * two SEPARATE jump tables at .rodata+0xa440/+0xa4bc) -- out of range
 * (bankId > 30, unsigned) yields (0,0) in mode 0 or (0x3f,0) in the
 * other mode.
 */
void USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(int bankId, char &out1, char &out2)
{
	static const unsigned char kModeA[31][2] = {
		/* fieldAt(0x6e4) != 0 */
		{0x3f, 0}, {0x3f, 1}, {0x3f, 2}, {0x3f, 3}, {0x3f, 4}, {0x3f, 5},
		{0x79, 0}, {0x79, 1}, {0x79, 2}, {0x79, 3}, {0x79, 4}, {0x79, 5},
		{0x79, 6}, {0x79, 7}, {0x79, 8}, {0x79, 9}, {0x78, 0},
		{0x3f, 8}, {0x3f, 9}, {0x3f, 10}, {0x3f, 11}, {0x3f, 12},
		{0x3f, 13}, {0x3f, 14}, {0x3f, 15}, {0x3f, 16}, {0x3f, 17},
		{0x3f, 18}, {0x3f, 19}, {0x3f, 20}, {0x3f, 21},
	};
	static const unsigned char kModeB[31][2] = {
		/* fieldAt(0x6e4) == 0 */
		{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5},
		{0x79, 0}, {0x79, 1}, {0x79, 2}, {0x79, 3}, {0x79, 4}, {0x79, 5},
		{0x79, 6}, {0x79, 7}, {0x79, 8}, {0x79, 9}, {0x78, 0},
		{0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 12},
		{0, 13}, {0, 14}, {0, 15}, {0, 16}, {0, 17},
		{0, 18}, {0, 19}, {0, 20}, {0, 21},
	};

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	bool gm2Mode = (g[0x6e4] != 0);
	if ((unsigned int)bankId > 30) {
		out1 = gm2Mode ? (char)0x3f : (char)0;
		out2 = 0;
		return;
	}

	const unsigned char (*table)[2] = gm2Mode ? kModeA : kModeB;
	out1 = (char)table[bankId][0];
	out2 = (char)table[bankId][1];
}

/*
 * ConvertMidiBankToAliasProgramBank(char midiBankMsb, char midiBankLsb, int &outBankId)
 * (.text+0x25320, 362 bytes) confirmed: the inverse of
 * ConvertAliasPgmBankToMidiBank above, reproduced as the same real
 * branch structure rather than a full reverse-table (the real binary
 * does NOT simply invert the 31-entry table -- several MSB byte values
 * are checked directly, and midiBankLsb 6/7 are genuine confirmed gaps
 * with no alias bank producing them, matching ConvertMidiBankToCombiBank's
 * own analogous bankId==7 gap above).
 *   outBankId = 0 by default.
 *   fieldAt(0x6e4)!=0 (gm2Mode) branch:
 *     msb==0x3f -> lsb<=0x15 ? kHighBankTable[lsb] : 0 (default).
 *     msb<0x3f: msb==0x38 -> 6; msb==0x3e -> 0x10; msb==0 -> 6 (a
 *       CONFIRMED real quirk: msb==0 falls through to the exact same
 *       code as msb==0x38, NOT the kHighBankTable path); else -> 0.
 *     msb>0x3f: falls into the shared msb==0x78/0x79 tail below
 *       (checking 0x78 first, since this branch's own msb range
 *       wasn't already narrowed the way the mode==0 branch's is).
 *   fieldAt(0x6e4)==0 branch:
 *     msb==0x78 -> outBankId=0x10.
 *     msb<0x78: msb==0x38 -> 6; msb==0x3e -> 0x10; msb==0 -> lsb<=0x15
 *       ? kHighBankTable[lsb] : 0 (default) -- this is the SAME
 *       kHighBankTable lookup the gm2Mode/msb==0x3f case above uses,
 *       but reached from msb==0 here instead (the two branches'
 *       "msb==0" quirks are genuinely different from each other, not
 *       symmetric); else -> 0.
 *     msb>0x78: falls into the shared msb==0x79 tail below (already
 *       excluding 0x78, so no need to recheck it).
 *   Shared tail (msb==0x79 in both modes): lsb>9 -> 6; else
 *     kLowBankTable[lsb] (lsb 0..9 -> 6..0xf).
 */
void USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(char midiBankMsb, char midiBankLsb, int &outBankId)
{
	static const unsigned char kLowBankTable[10] = {
		6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
	};
	static const int kHighBankTable[22] = {
		0 /* gap */, 1, 2, 3, 4, 5,
		0 /* gap */, 0 /* gap */,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
	};

	outBankId = 0;

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	bool gm2Mode = (g[0x6e4] != 0);
	unsigned char msb = (unsigned char)midiBankMsb;
	unsigned char lsb = (unsigned char)midiBankLsb;

	if (gm2Mode) {
		if (msb == 0x3f) {
			if (lsb <= 0x15)
				outBankId = kHighBankTable[lsb];
			return;
		}
		if (msb <= 0x3f) {
			/* msb < 0x3f here (msb==0x3f already handled above) */
			if (msb == 0x38)
				outBankId = 6;
			else if (msb == 0x3e)
				outBankId = 0x10;
			else if (msb == 0)
				outBankId = 6;	/* confirmed real fallthrough quirk */
			return;
		}
		/* msb > 0x3f: shared tail below still needs its own 0x78
		 * check first (confirmed via the real disassembly's own
		 * L_MODE_GT3f entry point). */
		if (msb == 0x78) {
			outBankId = 0x10;
			return;
		}
	} else {
		if (msb == 0x78) {
			outBankId = 0x10;
			return;
		}
		if (msb <= 0x78) {
			/* msb < 0x78 here (msb==0x78 already handled above) */
			if (msb == 0x38)
				outBankId = 6;
			else if (msb == 0x3e)
				outBankId = 0x10;
			else if (msb == 0) {
				if (lsb <= 0x15)
					outBankId = kHighBankTable[lsb];
			}
			return;
		}
		/* msb > 0x78: falls straight to the shared 0x79 tail (no
		 * need to recheck 0x78, already excluded above). */
	}

	/* Shared tail: msb==0x79 in both modes (confirmed via the real
	 * binary's own shared code layout at .text+0x25384). */
	if (msb != 0x79)
		return;
	if (lsb > 9) {
		outBankId = 6;
		return;
	}
	outBankId = kLowBankTable[lsb];
}
