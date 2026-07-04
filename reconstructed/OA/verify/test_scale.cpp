// SPDX-License-Identifier: GPL-2.0
/*
 * test_scale.cpp  -  host-side known-answer tests for the Scale* leaf
 * linear-rescale family (Stage 2, see include/oa_scale.h).
 *
 * As with CSTGBankMemory/CSTGQuad, this is Korg-internal math with no
 * third-party reference -- vectors are hand-computed from the confirmed
 * formula (including its confirmed degenerate-branch behavior when
 * inMin==inMax, which deliberately skips division rather than treating it
 * as division-by-one).
 */

#include <cstdio>
#include "oa_scale.h"

static int g_fail;

static void check_eq_l(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-45s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("Scale* leaf math known-answer test\n");
	printf("===================================\n");

	printf("[1] ScaleLong: normal range mapping\n");
	/* value=5 in [0,10] -> [0,100]: 0 + (5-0)*(100-0)/(10-0) = 50 */
	check_eq_l("ScaleLong(5, 0,10, 0,100)", ScaleLong(5, 0, 10, 0, 100), 50);
	/* value=25 in [0,100] -> [200,400]: 200 + 25*200/100 = 200+50 = 250 */
	check_eq_l("ScaleLong(25, 0,100, 200,400)", ScaleLong(25, 0, 100, 200, 400), 250);
	/* inverted output range: value=5 in [0,10] -> [100,0]: 100 + 5*(-100)/10 = 100-50=50 */
	check_eq_l("ScaleLong inverted output range", ScaleLong(5, 0, 10, 100, 0), 50);
	/* truncating division: value=1 in [0,3] -> [0,10]: 0 + 1*10/3 = 3 (integer truncation) */
	check_eq_l("ScaleLong truncates toward zero", ScaleLong(1, 0, 3, 0, 10), 3);

	printf("[2] ScaleLong: degenerate case (inMin==inMax skips division, not /1)\n");
	/* outMin + (outMax-outMin)*(value-inMin), NO division: 5 + (25-5)*(7-3) = 5+80 = 85 */
	check_eq_l("ScaleLong(7, 3,3, 5,25) degenerate", ScaleLong(7, 3, 3, 5, 25), 85);

	printf("[3] ScaleLongDouble: same formula via double precision, avoids int32 overflow\n");
	/* A range wide enough that the int32 (outMax-outMin)*(value-inMin) product
	 * would overflow: (outMax-outMin)=2,000,000,000, (value-inMin)=2 -> product
	 * 4e9 overflows a 32-bit signed int, but not a double. */
	long got = ScaleLongDouble(2, 0, 4, 0, 2000000000L);
	/* 0 + (2-0)*2000000000/4 = 1000000000 */
	check_eq_l("ScaleLongDouble avoids int32 overflow", got, 1000000000L);
	check_eq_l("ScaleLongDouble matches ScaleLong on a safe range",
		   ScaleLongDouble(5, 0, 10, 0, 100), ScaleLong(5, 0, 10, 0, 100));
	check_eq_l("ScaleLongDouble degenerate case matches ScaleLong's",
		   ScaleLongDouble(7, 3, 3, 5, 25), ScaleLong(7, 3, 3, 5, 25));

	printf("[4] ScaleShort/ScaleWord/ScaleByte/ScaleChar: same formula, narrower widths\n");
	check_eq_l("ScaleShort(5, 0,10, 0,100)", ScaleShort(5, 0, 10, 0, 100), 50);
	check_eq_l("ScaleWord(5, 0,10, 0,100)", ScaleWord(5, 0, 10, 0, 100), 50);
	check_eq_l("ScaleByte(5, 0,10, 0,100)", ScaleByte(5, 0, 10, 0, 100), 50);
	check_eq_l("ScaleChar(5, 0,10, 0,100)", ScaleChar(5, 0, 10, 0, 100), 50);
	/* narrow-width degenerate case too */
	check_eq_l("ScaleByte(7, 3,3, 5,25) degenerate", ScaleByte(7, 3, 3, 5, 25), 85);

	printf("===================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
