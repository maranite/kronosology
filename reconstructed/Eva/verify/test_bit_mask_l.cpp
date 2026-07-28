/*
 * test_bit_mask_l.cpp  -  host-side known-answer test for CBitMaskL's 13
 * reconstructed methods (include/bit_mask_l.h). See that header's own
 * comment for full ground-truth addresses/provenance.
 *
 * Expected values below were computed by a small standalone Python model of
 * the real per-method logic, cross-checked against the real extracted
 * machine code executed via this project's direct-execution-oracle technique
 * (mmap+PROT_EXEC) -- ~50000 randomized trials (0 mismatches, `mSize` swept
 * negative/zero/1..32/33..40) -- NOT hand-computed. The `getbit()` sequence
 * below reproduces a real 5-call bit-iteration walk over mask 0x25 (bits
 * 0,2,5 set), matching the oracle-verified model exactly.
 */

#include <cstdio>

#include "bit_mask_l.h"

struct CBitMaskLTestHooks {
	static void SetFields(CBitMaskL &b, unsigned short lo, short size,
	                       unsigned short hi, unsigned short cursor)
	{
		b.mLo = lo;
		b.mSize = size;
		b.mHi = hi;
		b.mCursor = cursor;
	}
	static unsigned short Lo(const CBitMaskL &b) { return b.mLo; }
	static short Size(const CBitMaskL &b) { return b.mSize; }
	static unsigned short Hi(const CBitMaskL &b) { return b.mHi; }
	static unsigned short Cursor(const CBitMaskL &b) { return b.mCursor; }
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
	printf("CBitMaskL:\n");

	/* ctor */
	{
		CBitMaskL b(5);
		check("ctor(5): size==5", CBitMaskLTestHooks::Size(b) == 5);
		check("ctor(5): lo==0", CBitMaskLTestHooks::Lo(b) == 0);
		check("ctor(5): hi==0", CBitMaskLTestHooks::Hi(b) == 0);
		CBitMaskL neg(-1);
		check("ctor(-1): size==-1", CBitMaskLTestHooks::Size(neg) == -1);
	}

	/* set/GetMask/is_set/is_clear/clear/GetNumOfSetBit */
	{
		CBitMaskL b(10);
		b.set(0x00030005ul);
		check("set(0x30005): lo==0x5", CBitMaskLTestHooks::Lo(b) == 0x5);
		check("set(0x30005): hi==0x3", CBitMaskLTestHooks::Hi(b) == 0x3);
		check("GetMask() == 0x30005", b.GetMask() == 0x00030005ul);
		check("is_set(1) true", b.is_set(1) == true);
		check("is_set(4) true", b.is_set(4) == true);
		check("is_set(0x20000) true", b.is_set(0x20000) == true);
		check("is_set(0x40000) false", b.is_set(0x40000) == false);
		check("is_clear(0x40000) true", b.is_clear(0x40000) == true);
		check("is_clear(1) false", b.is_clear(1) == false);
		check("GetNumOfSetBit() == 2", b.GetNumOfSetBit() == 2);

		b.clear(4);
		check("clear(4): lo==0x1", CBitMaskLTestHooks::Lo(b) == 0x1);
		check("clear(4): hi==0x3 (unaffected)", CBitMaskLTestHooks::Hi(b) == 0x3);
	}

	/* operator=/operator|=/init */
	{
		CBitMaskL b(4);
		b = 0x12345678ul;
		check("operator=: lo==0x5678", CBitMaskLTestHooks::Lo(b) == 0x5678);
		check("operator=: hi==0x1234", CBitMaskLTestHooks::Hi(b) == 0x1234);
		b.init();
		check("init(): lo==0", CBitMaskLTestHooks::Lo(b) == 0);
		check("init(): hi==0", CBitMaskLTestHooks::Hi(b) == 0);
		check("init(): size unaffected (still 4)", CBitMaskLTestHooks::Size(b) == 4);
		b |= 0x00010001ul;
		check("operator|=: lo==1", CBitMaskLTestHooks::Lo(b) == 1);
		check("operator|=: hi==1", CBitMaskLTestHooks::Hi(b) == 1);
		b.init(0x00020002ul);
		check("init(mask): lo==2", CBitMaskLTestHooks::Lo(b) == 2);
		check("init(mask): hi==2", CBitMaskLTestHooks::Hi(b) == 2);
	}

	/* GetNumOfSetBit boundary sweeps (past the field's own 32-bit width) */
	{
		CBitMaskL full(40);
		full.set(0xfffffffful);
		check("GetNumOfSetBit oversize(40, all-set) == 40", full.GetNumOfSetBit() == 40);

		CBitMaskL negsz(-5);
		negsz.set(0xfffffffful);
		check("GetNumOfSetBit negative size == 0", negsz.GetNumOfSetBit() == 0);
	}

	/* getbit(): 5-call walk over mask 0x25 (bits 0,2,5), size=8 */
	{
		CBitMaskL b(8);
		b.set(0x25ul);
		unsigned long ref = 0;
		unsigned long r;

		r = b.getbit(ref);
		check("getbit #1 == 1 (bit0)", r == 1);
		check("getbit #1: ref==1", ref == 1);
		check("getbit #1: cursor==1", CBitMaskLTestHooks::Cursor(b) == 1);

		r = b.getbit(ref);
		check("getbit #2 == 4 (bit2)", r == 4);
		check("getbit #2: cursor==3", CBitMaskLTestHooks::Cursor(b) == 3);

		r = b.getbit(ref);
		check("getbit #3 == 32 (bit5)", r == 32);
		check("getbit #3: cursor==6", CBitMaskLTestHooks::Cursor(b) == 6);

		r = b.getbit(ref);
		check("getbit #4 == 0 (exhausted)", r == 0);
		check("getbit #4: cursor==8", CBitMaskLTestHooks::Cursor(b) == 8);

		r = b.getbit(ref);
		check("getbit #5 == 0 (still exhausted)", r == 0);
		check("getbit #5: cursor==9", CBitMaskLTestHooks::Cursor(b) == 9);
	}

	/* ProcessEndian() */
	{
		CBitMaskL b(0);
		CBitMaskLTestHooks::SetFields(b, 0x1234, (short)0x0102, 0xabcd, 0x00ff);
		b.ProcessEndian();
		check("ProcessEndian: lo==0x3412", CBitMaskLTestHooks::Lo(b) == 0x3412);
		check("ProcessEndian: size==0x0201", CBitMaskLTestHooks::Size(b) == (short)0x0201);
		check("ProcessEndian: hi==0xcdab", CBitMaskLTestHooks::Hi(b) == 0xcdab);
		check("ProcessEndian: cursor==0xff00", CBitMaskLTestHooks::Cursor(b) == 0xff00);
	}

	printf("%s (%d failure%s)\n", g_fail ? "FAILED" : "PASSED",
	       g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
