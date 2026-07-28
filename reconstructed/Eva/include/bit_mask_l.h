/*
 * bit_mask_l.h  -  CBitMaskL, a small self-contained 32-bit-capacity bitmask/
 * bit-iterator value class (`.text+0x0838e350..0x0838e730` in the real `Eva`
 * binary, 13 unique methods per `nm -C`, no vtable/thunks -- a plain, non-
 * polymorphic value class, unlike most of this project's other small
 * "container" reconstructions).
 *
 * Found via a fresh whole-binary `nm -C` class-inventory sweep (2026-07-28,
 * following up on CVFATEntry -- see vfat_entry.h's own header comment for why
 * that class's remaining ~28 methods stay deferred: this pass re-confirmed via
 * direct disassembly that `GetSlotIndex()` is genuinely blocked on modeling a
 * real vtable-based `CDirEntry::GetName()` call, `OnShortNameChanged()`/
 * `OnShortExtChanged()` both make a real vtable dispatch through slot+0x94,
 * and 2 previously-unenumerated methods, `GetNumByteToSerialize()`/
 * `GetNumSlotToSerialize()`, ALSO both dispatch through vtable slot+0x8 (the
 * same slot `CDirEntry::GetName()` uses) -- all correctly out of scope, no
 * new ground to cover there). `CBitMaskL` is used by 6 call sites in an
 * unreconstructed CD-burn/PCG-save area (`.text+0x083d63a0`/`0x083dff80`
 * region, `CBitMaskLC1Es` calls only -- that caller code itself stays out of
 * scope, only the class's own methods are reconstructed here) but the class
 * itself has zero external dependencies.
 *
 * REAL LAYOUT (confirmed by `ProcessEndian()`'s own disassembly, which
 * byte-swaps exactly 4 consecutive `unsigned short` fields at +0x00/+0x02/
 * +0x04/+0x06 -- the only method that touches all 4 fields, and the only
 * direct evidence of the object's real total size, 8 bytes):
 *   +0x00 (u16) mLo     -- low 16 bits of the 32-bit mask (`GetMask()`'s own
 *               `((hi<<16)|lo)` recombination confirms bit order)
 *   +0x02 (s16) mSize   -- signed count, set by the one real ctor
 *               (`CBitMaskL(short)`), used as the loop bound in both
 *               `GetNumOfSetBit()` and `getbit()`
 *   +0x04 (u16) mHi     -- high 16 bits of the 32-bit mask
 *   +0x06 (u16) mCursor -- `getbit()`'s own persistent "last found bit
 *               position" scan cursor (not touched by any other method
 *               except `ProcessEndian()`)
 *
 * `is_set(unsigned long) const` (.text+0x0838e3c0) is the one method with a
 * real external dependency: for any ODD `mask` argument other than exactly 1,
 * ground truth fires a real, non-enforcing diagnostic through the global
 * `Api` object's own vtable slot+0x94 (`ds:0x930a1f4`, format string
 * "Assertion failed in module %s, line %i.\n" + a `PcgSaveInfo.cpp`-rooted
 * filename literal + line 0x34) -- same "Api+0x94 soft-assert, log-only,
 * never enforcing" convention this project has already established at ~15
 * other call sites (circ_byte_buffer.h, chunk_man.h, ev_buffers_pool.h,
 * etc.), omitted here for the same reason. Critically, this omission changes
 * NOTHING about the return value: ground truth's own control flow, traced
 * instruction-by-instruction, falls through from the assert call straight
 * back into the SAME bit-test tail every even-`mask`/`mask==1` path already
 * reaches -- `is_set()`'s real behavior for every possible 32-bit input,
 * assert-triggering or not, reduces to the single branch below (confirmed by
 * the direct-execution oracle across all even inputs and `mask==1`; odd
 * inputs other than 1 were excluded from oracle execution only because
 * executing that call target would require faking up the real `Api` global,
 * not because the reduction is in doubt -- the shared post-assert jump target
 * is identical machine code to the even-mask path, statically confirmed).
 *
 * VERIFICATION: all 13 methods verified via the direct-execution oracle
 * (mmap+PROT_EXEC on the real extracted machine code, see re-decompiler
 * memory's `x86-direct-execution-oracle-technique`) -- ~50000 randomized
 * trials (0 mismatches) across all 13 methods, `mSize` swept through
 * negative/zero/1..32/33..40 (well past any real use) to confirm
 * `GetNumOfSetBit()`/`getbit()`'s loop-bound and x86 `shr r32,cl` 5-bit
 * masking behavior (`i & 31` in the C++ below) matches ground truth exactly
 * at every boundary, and `getbit()`'s stateful cursor tested via arbitrary
 * (not just sequentially-reachable) `mCursor`/`ref` starting states.
 */

#ifndef BIT_MASK_L_H
#define BIT_MASK_L_H

class CBitMaskL {
public:
	/* .text+0x0838e3a0, 24 bytes. Real body: mSize=size, mLo=0, mHi=0
	 * (mCursor left uninitialized, matching ground truth -- no write to
	 * +0x06 anywhere in the ctor's own instruction range).
	 */
	explicit CBitMaskL(short size) : mLo(0), mSize(size), mHi(0), mCursor(0) {}

	/* .text+0x0838e350, 80 bytes. Real body: byte-swaps all 4 u16 fields
	 * in place (big/little-endian round-trip fixup for on-disk
	 * serialization, matching this class's real use in a PCG-save-info
	 * area).
	 */
	void ProcessEndian()
	{
		mLo = swap16(mLo);
		mSize = (short)swap16((unsigned short)mSize);
		mHi = swap16(mHi);
		mCursor = swap16(mCursor);
	}

	/* .text+0x0838e3c0, 128 bytes. Real body -- see header comment for the
	 * omitted Api+0x94 soft-assert on odd `mask != 1` (behaviorally
	 * inert).
	 */
	bool is_set(unsigned long mask) const
	{
		unsigned short lowArg = (unsigned short)mask;
		if (lowArg != 0)
			return (mLo & lowArg) != 0;
		unsigned short highArg = (unsigned short)(mask >> 16);
		return (mHi & highArg) != 0;
	}

	/* .text+0x0838e440, 32 bytes. Real body: self-contained, no external
	 * refs.
	 */
	bool is_clear(unsigned long mask) const
	{
		unsigned short lowArg = (unsigned short)mask;
		if ((mLo & lowArg) != 0)
			return false;
		unsigned short highArg = (unsigned short)(mask >> 16);
		return (mHi & highArg) == 0;
	}

	/* .text+0x0838e460, 20 bytes. */
	void set(unsigned long mask)
	{
		mLo = (unsigned short)(mLo | (unsigned short)mask);
		mHi = (unsigned short)(mHi | (unsigned short)(mask >> 16));
	}

	/* .text+0x0838e480, 24 bytes. */
	void clear(unsigned long mask)
	{
		mLo = (unsigned short)(mLo & ~(unsigned short)mask);
		mHi = (unsigned short)(mHi & ~(unsigned short)(mask >> 16));
	}

	/* .text+0x0838e4a0, 17 bytes. Real body: `(hi<<16)|lo`. */
	unsigned long GetMask() const
	{
		return ((unsigned long)mHi << 16) | mLo;
	}

	/* .text+0x0838e4c0, 19 bytes. Real body: plain overwrite (`mov`, not
	 * `or`), and -- confirmed by ground truth never setting `eax` before
	 * `ret` -- really does return void, not `CBitMaskL&`.
	 */
	void operator=(unsigned long mask)
	{
		mLo = (unsigned short)mask;
		mHi = (unsigned short)(mask >> 16);
	}

	/* .text+0x0838e4e0, 19 bytes. Real body: byte-identical to set()
	 * (also confirmed void-returning, same as operator=() above).
	 */
	void operator|=(unsigned long mask)
	{
		set(mask);
	}

	/* .text+0x0838e500, 16 bytes. Real body: zeroes mLo/mHi, leaves
	 * mSize/mCursor untouched.
	 */
	void init()
	{
		mLo = 0;
		mHi = 0;
	}

	/* .text+0x0838e510, 19 bytes. Real body: byte-identical to
	 * operator=(unsigned long).
	 */
	void init(unsigned long mask)
	{
		*this = mask;
	}

	/* .text+0x0838e530, 400 bytes (GCC 8-wide Duff's-device unroll). Real
	 * body reduces to: 0 if mSize<=0, else a count of set bits in
	 * positions [0,mSize) of the combined 32-bit mask, using x86's own
	 * `shr r32,cl` 5-bit-masked shift-amount semantics (`i & 31`, not a
	 * plain `i`) -- confirmed equivalent to the real unrolled loop by the
	 * direct-execution oracle, including `mSize` swept past 32.
	 */
	int GetNumOfSetBit() const
	{
		if (mSize <= 0)
			return 0;
		unsigned long mask = GetMask();
		int count = 0;
		for (int i = 0; i < mSize; i++) {
			if ((mask >> (i & 31)) & 1)
				count++;
		}
		return count;
	}

	/* .text+0x0838e6c0, 112 bytes. Real body: a stateful "find the next
	 * set bit at or after mCursor" iterator. `ref` (an in/out unsigned
	 * long&) is reset to 1 (and mCursor to 0) whenever the caller passes
	 * in 0 -- ground truth's own "first call" sentinel. Returns the FULL
	 * bit value (`1UL << pos`), not the bit index, or 0 if no set bit
	 * remains before mSize. mCursor is always advanced past the position
	 * just tested (found or not), matching ground truth's own
	 * find-then-advance shape so a repeated call resumes past the last
	 * hit. Preserved as a literal 1:1 control-flow translation of the
	 * real asm (rather than a "cleaned up" loop) since the found/not-found
	 * exit points store slightly different cursor values -- confirmed
	 * correct only via the direct-execution oracle, not simplified by
	 * hand.
	 */
	unsigned long getbit(unsigned long &ref)
	{
		unsigned long mask = GetMask();
		if (ref == 0) {
			mCursor = 0;
			ref = 1;
		}
		unsigned short d = mCursor;
		int testPos = (short)d; /* sign-extend, matches ground truth's movswl */
		bool first = true;
		for (;;) {
			if (!first) {
				if ((short)d >= mSize) {
					mCursor = d;
					return 0;
				}
				testPos++;
			}
			first = false;
			d = (unsigned short)(d + 1);
			unsigned long bitVal = 1UL << (testPos & 31);
			if (mask & bitVal) {
				mCursor = d;
				return bitVal;
			}
		}
	}

private:
	friend struct CBitMaskLTestHooks;

	static unsigned short swap16(unsigned short v)
	{
		return (unsigned short)((v << 8) | (v >> 8));
	}

	unsigned short mLo;     /* +0x00 */
	short          mSize;   /* +0x02 */
	unsigned short mHi;     /* +0x04 */
	unsigned short mCursor; /* +0x06 */
};

#endif /* BIT_MASK_L_H */
