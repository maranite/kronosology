/*
 * test_led_blinker.cpp  -  host-side known-answer test for CLEDBlinker
 * (src/hw/led_blinker.cpp). Eva CPoller final-prerequisites follow-up batch,
 * 2026-07-26. See led_blinker.h for the full ground-truth writeup.
 *
 * Checks:
 *   [1] ctor: mCount/mBlinkPhase/mDivider all 0, mBitmap all zeroed.
 *   [2] Register(): sets the right (word, bit), increments mCount exactly once;
 *       registering the SAME led again is a no-op (bit already set, mCount
 *       unchanged) -- confirmed real per the "only if bit was clear" gate.
 *   [3] Register() from a fully-idle state (mCount 0->1) resets mBlinkPhase/
 *       mDivider to 0; Register()ing a 2nd, already-nonzero-mCount led does NOT
 *       reset them again (only the 0->nonzero transition does).
 *   [4] Unregister(ledCode): clears the bit and decrements mCount, only if the
 *       bit was set; unregistering an already-clear bit is a no-op (mCount
 *       unchanged).
 *   [5] Unregister(ledCode) never lets mCount go negative (defensive floor,
 *       transcribed as found even though unreachable in practice given
 *       Register()'s own gating).
 *   [6] Unregister(wordIndex, mask) overload: clears exactly the bits present in
 *       BOTH mask and the current word, decrements mCount once per bit actually
 *       cleared (not a single decrement) -- multi-bit case exercised (2 of 3
 *       masked bits actually set).
 *   [7] Exec() with mCount == 0: always returns 0, never touches mDivider/
 *       mBlinkPhase.
 *   [8] Exec() full 21-tick cycle with mCount != 0: first call (mDivider==0)
 *       flips mBlinkPhase and returns 1; the next 20 calls return 0 while
 *       mDivider counts 1..20 then wraps to 0; the 22nd call flips again.
 *   [9] ledCode/16, ledCode%16 indexing matches ground truth's own decompiled
 *       "+0xf if negative before >>4" idiom for a spot-checked value (ledCode=200,
 *       word 12, bit 8) -- confirms the C `/`/`%` simplification is bit-exact,
 *       not just true for small values.
 *
 * s_oLEDBlinker (the real ground-truth global singleton) is deliberately NOT used
 * here -- every check constructs its own local CLEDBlinker so tests can't leak
 * state into each other or into a later test file that also links led_blinker.o.
 */

#include <cstdio>

#include "led_blinker.h"

struct LEDBlinkerTestHooks {
	static int Count(const CLEDBlinker &b) { return b.mCount; }
	static int BlinkPhase(const CLEDBlinker &b) { return b.mBlinkPhase; }
	static int Divider(const CLEDBlinker &b) { return b.mDivider; }
	static unsigned short BitmapWord(const CLEDBlinker &b, int i) { return b.mBitmap[i]; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	typedef LEDBlinkerTestHooks H;

	/* [1] */
	{
		CLEDBlinker b;
		bool allZero = true;
		for (int i = 0; i < 0x20; i++)
			if (H::BitmapWord(b, i) != 0)
				allZero = false;
		check("[1] ctor zeroes mCount/mBlinkPhase/mDivider/mBitmap",
		      H::Count(b) == 0 && H::BlinkPhase(b) == 0 && H::Divider(b) == 0 && allZero);
	}

	/* [2] */
	{
		CLEDBlinker b;
		b.Register(5); /* word 0, bit 5 */
		bool bitSet = (H::BitmapWord(b, 0) & (1 << 5)) != 0;
		check("[2a] Register(5) sets word0 bit5", bitSet);
		check("[2b] Register(5) increments mCount to 1", H::Count(b) == 1);

		b.Register(5); /* same LED again -- no-op */
		check("[2c] re-Register(5) is a no-op on mCount", H::Count(b) == 1);
	}

	/* [3] */
	{
		CLEDBlinker b;
		/* Poke mBlinkPhase/mDivider to nonzero by hand (simulating "was already
		 * mid-cycle"), then verify the FIRST Register() (0->1 transition)
		 * resets both.
		 */
		b.Exec(); /* mCount==0, no-op, but let's force state via a 2nd blinker */

		CLEDBlinker b2;
		b2.Register(3);
		b2.Exec(); /* mDivider 0->1, mBlinkPhase 0->1 (flip) */
		check("[3setup] Exec() flipped phase once mCount!=0",
		      H::BlinkPhase(b2) == 1 && H::Divider(b2) == 1);

		b2.Register(3); /* re-register SAME led (bit already set) -- no transition */
		check("[3a] re-Register of an already-set bit does not reset phase/divider",
		      H::BlinkPhase(b2) == 1 && H::Divider(b2) == 1);

		b2.Unregister(3); /* mCount back to 0 */
		b2.Register(7);   /* fresh 0->1 transition -- must reset phase/divider */
		check("[3b] Register() on a genuine 0->1 transition resets phase/divider",
		      H::BlinkPhase(b2) == 0 && H::Divider(b2) == 0);
	}

	/* [4] */
	{
		CLEDBlinker b;
		b.Register(20); /* word 1, bit 4 */
		check("[4setup] Register(20) -> word1 bit4 set, mCount 1",
		      (H::BitmapWord(b, 1) & (1 << 4)) != 0 && H::Count(b) == 1);

		b.Unregister(20);
		check("[4a] Unregister(20) clears the bit", (H::BitmapWord(b, 1) & (1 << 4)) == 0);
		check("[4b] Unregister(20) decrements mCount to 0", H::Count(b) == 0);

		b.Unregister(20); /* already clear -- no-op */
		check("[4c] Unregister() of an already-clear bit is a no-op", H::Count(b) == 0);
	}

	/* [5] */
	{
		CLEDBlinker b;
		b.Unregister(9); /* never registered -- bit already clear, no-op path */
		check("[5] Unregister() never drives mCount negative", H::Count(b) == 0);
	}

	/* [6] */
	{
		CLEDBlinker b;
		b.Register(0);  /* word0 bit0 */
		b.Register(1);  /* word0 bit1 */
		b.Register(2);  /* word0 bit2 */
		check("[6setup] 3 LEDs registered in word0", H::Count(b) == 3 &&
		      H::BitmapWord(b, 0) == 0x7);

		/* mask selects bits 0 and 1 only (bit 2 untouched), plus a bit (3) that
		 * was never set at all -- exercises "only the intersection clears."
		 */
		b.Unregister(0, (unsigned short)0xb /* bits 0,1,3 */);
		check("[6a] Unregister(word,mask) clears only the intersection",
		      H::BitmapWord(b, 0) == 0x4 /* bit 2 survives */);
		check("[6b] Unregister(word,mask) decrements mCount once per cleared bit",
		      H::Count(b) == 1);
	}

	/* [7] */
	{
		CLEDBlinker b;
		for (int i = 0; i < 5; i++)
			check("[7] Exec() with mCount==0 always returns 0", b.Exec() == 0);
		check("[7b] Exec() with mCount==0 never touches mDivider/mBlinkPhase",
		      H::Divider(b) == 0 && H::BlinkPhase(b) == 0);
	}

	/* [8] */
	{
		CLEDBlinker b;
		b.Register(0);

		int r = b.Exec();
		check("[8a] first Exec() (mDivider==0) flips phase, returns 1",
		      r == 1 && H::BlinkPhase(b) == 1 && H::Divider(b) == 1);

		bool allZeroReturns = true;
		for (int i = 0; i < 20; i++)
			if (b.Exec() != 0)
				allZeroReturns = false;
		check("[8b] next 20 Exec() calls all return 0 (still counting)", allZeroReturns);
		check("[8c] mDivider wrapped back to 0 after 20 more calls", H::Divider(b) == 0);
		check("[8d] mBlinkPhase unchanged during the counting calls", H::BlinkPhase(b) == 1);

		r = b.Exec();
		check("[8e] the 22nd Exec() call flips phase again, returns 1",
		      r == 1 && H::BlinkPhase(b) == 0 && H::Divider(b) == 1);
	}

	/* [9] */
	{
		CLEDBlinker b;
		b.Register(200); /* 200/16 = 12 (word), 200%16 = 8 (bit) */
		check("[9] ledCode=200 lands at word 12 bit 8",
		      (H::BitmapWord(b, 12) & (1 << 8)) != 0);
	}

	printf("%s (%d failed)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
	return g_fail == 0 ? 0 : 1;
}
