/*
 * led_blinker.cpp  -  see include/led_blinker.h.
 *
 * All 6 methods transcribed directly from Ghidra's own decompile
 * (eva_export/functions/, addresses 089ee1d0..089ee340), cross-checked against
 * `functions.csv`'s own byte sizes. Every method is small enough that the
 * decompile needed no further disassembly cross-checking.
 */

#include "led_blinker.h"

CLEDBlinker s_oLEDBlinker;

CLEDBlinker::CLEDBlinker()
    : mCount(0), mBlinkPhase(0), mDivider(0)
{
	for (int i = 0; i < 0x20; i++)
		mBitmap[i] = 0;
}

CLEDBlinker::~CLEDBlinker()
{
	/* Real: bare `ret`, no vtable, nothing to tear down. */
}

void CLEDBlinker::Register(int ledCode)
{
	/* Real ground truth computes this via a decompiler idiom
	 * (`iVar2 = ledCode + 0xf; if (ledCode >= 0) iVar2 = ledCode; wordIndex =
	 * iVar2 >> 4;`) that is exactly C's own truncating `ledCode / 16` for every
	 * int value -- see header comment. `%` in C already matches the real `bit`
	 * computation the same way.
	 */
	int wordIndex = ledCode / 16;
	int bit       = ledCode % 16;
	unsigned short mask = (unsigned short)(1 << bit);

	if (!(mBitmap[wordIndex] & mask)) {
		if (mCount == 0) {
			mBlinkPhase = 0;
			mDivider = 0;
		}
		mCount++;
		mBitmap[wordIndex] |= mask;
	}
}

void CLEDBlinker::Unregister(int ledCode)
{
	int wordIndex = ledCode / 16;
	int bit       = ledCode % 16;
	unsigned short mask = (unsigned short)(1 << bit);

	if (mBitmap[wordIndex] & mask) {
		mBitmap[wordIndex] &= ~mask;
		if (mCount != 0)
			mCount--;
	}
}

void CLEDBlinker::Unregister(int wordIndex, unsigned short mask)
{
	unsigned short cleared = mask & mBitmap[wordIndex];
	if (cleared != 0) {
		mBitmap[wordIndex] &= ~cleared;
		/* Real: a bit-scan loop decrementing mCount once per matching bit,
		 * not a single decrement -- this overload bulk-clears a whole 16-bit
		 * group at once.
		 */
		unsigned int bits = cleared;
		do {
			if (bits & 1)
				mCount--;
			bits >>= 1;
		} while ((short)bits != 0);
	}
}

int CLEDBlinker::Exec()
{
	if (mCount == 0)
		return 0;

	if (mDivider != 0) {
		int next = mDivider + 1;
		mDivider = (next < 0x15) ? next : 0;
		return 0;
	}

	mDivider = 1;
	mBlinkPhase = (mBlinkPhase == 0) ? 1 : 0;
	return 1;
}
