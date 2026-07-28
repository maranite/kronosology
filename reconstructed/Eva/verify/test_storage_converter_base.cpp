/*
 * test_storage_converter_base.cpp  -  host-side known-answer test for
 * CStorageConverterBase's Ext{X}toInt{Y} matrix (src/init/storage_converter_base.cpp,
 * 2026-07-28 storage-cluster follow-up, storage_converter_base.h).
 *
 * INDEPENDENT verification, not a re-check of the generator's own classification:
 * this test knows nothing about which of the 256 methods are "REAL"/"THUNK"/"STUB"
 * internally. It only asserts one black-box rule, derived directly from ground
 * truth's own real behavior (Int0000 is byte-identical to Ext0000, everything else
 * is unimplemented in this build) rather than from re-deriving the generator's own
 * per-method size/offset classification:
 *
 *   for every (X,Y) in 0..15 x 0..15:
 *     call Ext{X}toInt{Y}(param) with a param pointing at a freshly-sentineled
 *     destination buffer and a distinct, non-sentinel source buffer
 *     if Y == 0:  destination must now equal the source (a real copy happened)
 *     else:       destination must be UNCHANGED from its sentinel (no-op)
 *
 * All 256 combinations are exercised exhaustively via a plain member-function-
 * pointer table (gnu++98, no C++11 features) generated once, mechanically, from
 * the same naming scheme as the class itself -- not hand-typed, to avoid the
 * same risk of a copy-paste slip a 256-entry table would otherwise invite.
 */

#include <cstdio>
#include <cstring>

#include "storage_converter_base.h"

namespace {

typedef void (CStorageConverterBase::*MethodPtr)(const CConvertStorageParam &) const;

/* table[y][x] == &CStorageConverterBase::Ext{x:04X}toInt{y:04X} */
const MethodPtr kTable[16][16] = {
	&CStorageConverterBase::Ext0000toInt0000, &CStorageConverterBase::Ext0001toInt0000, &CStorageConverterBase::Ext0002toInt0000, &CStorageConverterBase::Ext0003toInt0000,
	&CStorageConverterBase::Ext0004toInt0000, &CStorageConverterBase::Ext0005toInt0000, &CStorageConverterBase::Ext0006toInt0000, &CStorageConverterBase::Ext0007toInt0000,
	&CStorageConverterBase::Ext0008toInt0000, &CStorageConverterBase::Ext0009toInt0000, &CStorageConverterBase::Ext000AtoInt0000, &CStorageConverterBase::Ext000BtoInt0000,
	&CStorageConverterBase::Ext000CtoInt0000, &CStorageConverterBase::Ext000DtoInt0000, &CStorageConverterBase::Ext000EtoInt0000, &CStorageConverterBase::Ext000FtoInt0000,
	&CStorageConverterBase::Ext0000toInt0001, &CStorageConverterBase::Ext0001toInt0001, &CStorageConverterBase::Ext0002toInt0001, &CStorageConverterBase::Ext0003toInt0001,
	&CStorageConverterBase::Ext0004toInt0001, &CStorageConverterBase::Ext0005toInt0001, &CStorageConverterBase::Ext0006toInt0001, &CStorageConverterBase::Ext0007toInt0001,
	&CStorageConverterBase::Ext0008toInt0001, &CStorageConverterBase::Ext0009toInt0001, &CStorageConverterBase::Ext000AtoInt0001, &CStorageConverterBase::Ext000BtoInt0001,
	&CStorageConverterBase::Ext000CtoInt0001, &CStorageConverterBase::Ext000DtoInt0001, &CStorageConverterBase::Ext000EtoInt0001, &CStorageConverterBase::Ext000FtoInt0001,
	&CStorageConverterBase::Ext0000toInt0002, &CStorageConverterBase::Ext0001toInt0002, &CStorageConverterBase::Ext0002toInt0002, &CStorageConverterBase::Ext0003toInt0002,
	&CStorageConverterBase::Ext0004toInt0002, &CStorageConverterBase::Ext0005toInt0002, &CStorageConverterBase::Ext0006toInt0002, &CStorageConverterBase::Ext0007toInt0002,
	&CStorageConverterBase::Ext0008toInt0002, &CStorageConverterBase::Ext0009toInt0002, &CStorageConverterBase::Ext000AtoInt0002, &CStorageConverterBase::Ext000BtoInt0002,
	&CStorageConverterBase::Ext000CtoInt0002, &CStorageConverterBase::Ext000DtoInt0002, &CStorageConverterBase::Ext000EtoInt0002, &CStorageConverterBase::Ext000FtoInt0002,
	&CStorageConverterBase::Ext0000toInt0003, &CStorageConverterBase::Ext0001toInt0003, &CStorageConverterBase::Ext0002toInt0003, &CStorageConverterBase::Ext0003toInt0003,
	&CStorageConverterBase::Ext0004toInt0003, &CStorageConverterBase::Ext0005toInt0003, &CStorageConverterBase::Ext0006toInt0003, &CStorageConverterBase::Ext0007toInt0003,
	&CStorageConverterBase::Ext0008toInt0003, &CStorageConverterBase::Ext0009toInt0003, &CStorageConverterBase::Ext000AtoInt0003, &CStorageConverterBase::Ext000BtoInt0003,
	&CStorageConverterBase::Ext000CtoInt0003, &CStorageConverterBase::Ext000DtoInt0003, &CStorageConverterBase::Ext000EtoInt0003, &CStorageConverterBase::Ext000FtoInt0003,
	&CStorageConverterBase::Ext0000toInt0004, &CStorageConverterBase::Ext0001toInt0004, &CStorageConverterBase::Ext0002toInt0004, &CStorageConverterBase::Ext0003toInt0004,
	&CStorageConverterBase::Ext0004toInt0004, &CStorageConverterBase::Ext0005toInt0004, &CStorageConverterBase::Ext0006toInt0004, &CStorageConverterBase::Ext0007toInt0004,
	&CStorageConverterBase::Ext0008toInt0004, &CStorageConverterBase::Ext0009toInt0004, &CStorageConverterBase::Ext000AtoInt0004, &CStorageConverterBase::Ext000BtoInt0004,
	&CStorageConverterBase::Ext000CtoInt0004, &CStorageConverterBase::Ext000DtoInt0004, &CStorageConverterBase::Ext000EtoInt0004, &CStorageConverterBase::Ext000FtoInt0004,
	&CStorageConverterBase::Ext0000toInt0005, &CStorageConverterBase::Ext0001toInt0005, &CStorageConverterBase::Ext0002toInt0005, &CStorageConverterBase::Ext0003toInt0005,
	&CStorageConverterBase::Ext0004toInt0005, &CStorageConverterBase::Ext0005toInt0005, &CStorageConverterBase::Ext0006toInt0005, &CStorageConverterBase::Ext0007toInt0005,
	&CStorageConverterBase::Ext0008toInt0005, &CStorageConverterBase::Ext0009toInt0005, &CStorageConverterBase::Ext000AtoInt0005, &CStorageConverterBase::Ext000BtoInt0005,
	&CStorageConverterBase::Ext000CtoInt0005, &CStorageConverterBase::Ext000DtoInt0005, &CStorageConverterBase::Ext000EtoInt0005, &CStorageConverterBase::Ext000FtoInt0005,
	&CStorageConverterBase::Ext0000toInt0006, &CStorageConverterBase::Ext0001toInt0006, &CStorageConverterBase::Ext0002toInt0006, &CStorageConverterBase::Ext0003toInt0006,
	&CStorageConverterBase::Ext0004toInt0006, &CStorageConverterBase::Ext0005toInt0006, &CStorageConverterBase::Ext0006toInt0006, &CStorageConverterBase::Ext0007toInt0006,
	&CStorageConverterBase::Ext0008toInt0006, &CStorageConverterBase::Ext0009toInt0006, &CStorageConverterBase::Ext000AtoInt0006, &CStorageConverterBase::Ext000BtoInt0006,
	&CStorageConverterBase::Ext000CtoInt0006, &CStorageConverterBase::Ext000DtoInt0006, &CStorageConverterBase::Ext000EtoInt0006, &CStorageConverterBase::Ext000FtoInt0006,
	&CStorageConverterBase::Ext0000toInt0007, &CStorageConverterBase::Ext0001toInt0007, &CStorageConverterBase::Ext0002toInt0007, &CStorageConverterBase::Ext0003toInt0007,
	&CStorageConverterBase::Ext0004toInt0007, &CStorageConverterBase::Ext0005toInt0007, &CStorageConverterBase::Ext0006toInt0007, &CStorageConverterBase::Ext0007toInt0007,
	&CStorageConverterBase::Ext0008toInt0007, &CStorageConverterBase::Ext0009toInt0007, &CStorageConverterBase::Ext000AtoInt0007, &CStorageConverterBase::Ext000BtoInt0007,
	&CStorageConverterBase::Ext000CtoInt0007, &CStorageConverterBase::Ext000DtoInt0007, &CStorageConverterBase::Ext000EtoInt0007, &CStorageConverterBase::Ext000FtoInt0007,
	&CStorageConverterBase::Ext0000toInt0008, &CStorageConverterBase::Ext0001toInt0008, &CStorageConverterBase::Ext0002toInt0008, &CStorageConverterBase::Ext0003toInt0008,
	&CStorageConverterBase::Ext0004toInt0008, &CStorageConverterBase::Ext0005toInt0008, &CStorageConverterBase::Ext0006toInt0008, &CStorageConverterBase::Ext0007toInt0008,
	&CStorageConverterBase::Ext0008toInt0008, &CStorageConverterBase::Ext0009toInt0008, &CStorageConverterBase::Ext000AtoInt0008, &CStorageConverterBase::Ext000BtoInt0008,
	&CStorageConverterBase::Ext000CtoInt0008, &CStorageConverterBase::Ext000DtoInt0008, &CStorageConverterBase::Ext000EtoInt0008, &CStorageConverterBase::Ext000FtoInt0008,
	&CStorageConverterBase::Ext0000toInt0009, &CStorageConverterBase::Ext0001toInt0009, &CStorageConverterBase::Ext0002toInt0009, &CStorageConverterBase::Ext0003toInt0009,
	&CStorageConverterBase::Ext0004toInt0009, &CStorageConverterBase::Ext0005toInt0009, &CStorageConverterBase::Ext0006toInt0009, &CStorageConverterBase::Ext0007toInt0009,
	&CStorageConverterBase::Ext0008toInt0009, &CStorageConverterBase::Ext0009toInt0009, &CStorageConverterBase::Ext000AtoInt0009, &CStorageConverterBase::Ext000BtoInt0009,
	&CStorageConverterBase::Ext000CtoInt0009, &CStorageConverterBase::Ext000DtoInt0009, &CStorageConverterBase::Ext000EtoInt0009, &CStorageConverterBase::Ext000FtoInt0009,
	&CStorageConverterBase::Ext0000toInt000A, &CStorageConverterBase::Ext0001toInt000A, &CStorageConverterBase::Ext0002toInt000A, &CStorageConverterBase::Ext0003toInt000A,
	&CStorageConverterBase::Ext0004toInt000A, &CStorageConverterBase::Ext0005toInt000A, &CStorageConverterBase::Ext0006toInt000A, &CStorageConverterBase::Ext0007toInt000A,
	&CStorageConverterBase::Ext0008toInt000A, &CStorageConverterBase::Ext0009toInt000A, &CStorageConverterBase::Ext000AtoInt000A, &CStorageConverterBase::Ext000BtoInt000A,
	&CStorageConverterBase::Ext000CtoInt000A, &CStorageConverterBase::Ext000DtoInt000A, &CStorageConverterBase::Ext000EtoInt000A, &CStorageConverterBase::Ext000FtoInt000A,
	&CStorageConverterBase::Ext0000toInt000B, &CStorageConverterBase::Ext0001toInt000B, &CStorageConverterBase::Ext0002toInt000B, &CStorageConverterBase::Ext0003toInt000B,
	&CStorageConverterBase::Ext0004toInt000B, &CStorageConverterBase::Ext0005toInt000B, &CStorageConverterBase::Ext0006toInt000B, &CStorageConverterBase::Ext0007toInt000B,
	&CStorageConverterBase::Ext0008toInt000B, &CStorageConverterBase::Ext0009toInt000B, &CStorageConverterBase::Ext000AtoInt000B, &CStorageConverterBase::Ext000BtoInt000B,
	&CStorageConverterBase::Ext000CtoInt000B, &CStorageConverterBase::Ext000DtoInt000B, &CStorageConverterBase::Ext000EtoInt000B, &CStorageConverterBase::Ext000FtoInt000B,
	&CStorageConverterBase::Ext0000toInt000C, &CStorageConverterBase::Ext0001toInt000C, &CStorageConverterBase::Ext0002toInt000C, &CStorageConverterBase::Ext0003toInt000C,
	&CStorageConverterBase::Ext0004toInt000C, &CStorageConverterBase::Ext0005toInt000C, &CStorageConverterBase::Ext0006toInt000C, &CStorageConverterBase::Ext0007toInt000C,
	&CStorageConverterBase::Ext0008toInt000C, &CStorageConverterBase::Ext0009toInt000C, &CStorageConverterBase::Ext000AtoInt000C, &CStorageConverterBase::Ext000BtoInt000C,
	&CStorageConverterBase::Ext000CtoInt000C, &CStorageConverterBase::Ext000DtoInt000C, &CStorageConverterBase::Ext000EtoInt000C, &CStorageConverterBase::Ext000FtoInt000C,
	&CStorageConverterBase::Ext0000toInt000D, &CStorageConverterBase::Ext0001toInt000D, &CStorageConverterBase::Ext0002toInt000D, &CStorageConverterBase::Ext0003toInt000D,
	&CStorageConverterBase::Ext0004toInt000D, &CStorageConverterBase::Ext0005toInt000D, &CStorageConverterBase::Ext0006toInt000D, &CStorageConverterBase::Ext0007toInt000D,
	&CStorageConverterBase::Ext0008toInt000D, &CStorageConverterBase::Ext0009toInt000D, &CStorageConverterBase::Ext000AtoInt000D, &CStorageConverterBase::Ext000BtoInt000D,
	&CStorageConverterBase::Ext000CtoInt000D, &CStorageConverterBase::Ext000DtoInt000D, &CStorageConverterBase::Ext000EtoInt000D, &CStorageConverterBase::Ext000FtoInt000D,
	&CStorageConverterBase::Ext0000toInt000E, &CStorageConverterBase::Ext0001toInt000E, &CStorageConverterBase::Ext0002toInt000E, &CStorageConverterBase::Ext0003toInt000E,
	&CStorageConverterBase::Ext0004toInt000E, &CStorageConverterBase::Ext0005toInt000E, &CStorageConverterBase::Ext0006toInt000E, &CStorageConverterBase::Ext0007toInt000E,
	&CStorageConverterBase::Ext0008toInt000E, &CStorageConverterBase::Ext0009toInt000E, &CStorageConverterBase::Ext000AtoInt000E, &CStorageConverterBase::Ext000BtoInt000E,
	&CStorageConverterBase::Ext000CtoInt000E, &CStorageConverterBase::Ext000DtoInt000E, &CStorageConverterBase::Ext000EtoInt000E, &CStorageConverterBase::Ext000FtoInt000E,
	&CStorageConverterBase::Ext0000toInt000F, &CStorageConverterBase::Ext0001toInt000F, &CStorageConverterBase::Ext0002toInt000F, &CStorageConverterBase::Ext0003toInt000F,
	&CStorageConverterBase::Ext0004toInt000F, &CStorageConverterBase::Ext0005toInt000F, &CStorageConverterBase::Ext0006toInt000F, &CStorageConverterBase::Ext0007toInt000F,
	&CStorageConverterBase::Ext0008toInt000F, &CStorageConverterBase::Ext0009toInt000F, &CStorageConverterBase::Ext000AtoInt000F, &CStorageConverterBase::Ext000BtoInt000F,
	&CStorageConverterBase::Ext000CtoInt000F, &CStorageConverterBase::Ext000DtoInt000F, &CStorageConverterBase::Ext000EtoInt000F, &CStorageConverterBase::Ext000FtoInt000F,
};

} // namespace

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	if (!ok)
		printf("  FAIL  %s\n", label);
}

int main()
{
	CStorageConverterBase conv;
	unsigned char realFailCount = 0, noopFailCount = 0;

	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 16; ++x) {
			unsigned char src[8];
			unsigned char dst[8];
			for (int i = 0; i < 8; ++i) {
				src[i] = static_cast<unsigned char>(0xA0 + i);   /* distinct, non-sentinel */
				dst[i] = 0xCC;                                    /* sentinel */
			}

			CConvertStorageParam param;
			std::memset(&param, 0, sizeof param);
			param.m_internalBuf = dst;
			param.m_externalBuf = src;
			param.m_size = sizeof dst;

			(conv.*kTable[y][x])(param);

			bool copied = (std::memcmp(dst, src, sizeof dst) == 0);
			bool untouched = true;
			for (int i = 0; i < 8; ++i)
				if (dst[i] != 0xCC)
					untouched = false;

			char label[64];
			std::sprintf(label, "Ext%04XtoInt%04X", x, y);

			if (y == 0) {
				if (!copied)
					++realFailCount;
				check(label, copied);
			} else {
				if (!untouched)
					++noopFailCount;
				check(label, untouched);
			}
		}
	}

	printf("Ext*toInt0000 (real-copy) failures: %u/16\n", (unsigned)realFailCount);
	printf("Ext*toInt0001..000F (no-op) failures: %u/240\n", (unsigned)noopFailCount);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
