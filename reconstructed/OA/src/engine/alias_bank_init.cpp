// SPDX-License-Identifier: GPL-2.0
/*
 * alias_bank_init.cpp  -  USTGAliasBankTypes::InitializeAliasBanks()
 * (sec 10.85).
 *
 * Deliberately a separate translation unit from global.cpp: this
 * symbol's mock in test_global.cpp is a load-bearing call-counter for
 * CSTGGlobal::Initialize()'s own dispatch verification, matching the
 * same reasoning as midi_queue_writer.cpp/waveseq_setlist_init.cpp
 * (sec 10.83/10.84).
 */

#include "oa_global.h"

extern "C" int STGAliasToRealPgmBank[30 * 128];
extern "C" int STGAliasBankPgmMap[30 * 128];
int STGAliasToRealPgmBank[30 * 128];
int STGAliasBankPgmMap[30 * 128];

/*
 * USTGAliasBankTypes::InitializeAliasBanks() (.text+0x24ea0 in
 * OA_real.ko) -- confirmed real: fills 30 "alias program bank" blocks
 * of 128 entries each. Each block's own real bank ID is a literal
 * constant (NOT a simple 1:1 mapping -- real bank 7 is aliased by 9
 * distinct alias banks, blocks 7 through 15, confirmed directly from
 * the disassembly's own literal immediates, not a guessed formula).
 * STGAliasBankPgmMap is always an identity mapping (entry i within
 * each block maps to program index i).
 */
void USTGAliasBankTypes::InitializeAliasBanks()
{
	static const int kRealBankForAliasBank[30] = {
		0, 1, 2, 3, 4, 5, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7,
		8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
	};
	for (int block = 0; block < 30; block++) {
		int realBank = kRealBankForAliasBank[block];
		for (int i = 0; i < 128; i++) {
			STGAliasToRealPgmBank[block * 128 + i] = realBank;
			STGAliasBankPgmMap[block * 128 + i] = i;
		}
	}
}
