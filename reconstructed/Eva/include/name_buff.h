/*
 * name_buff.h  -  CNameBuff, a fixed-slot name/size/kind metadata table used to
 * back a directory-style browse list (disk file browser, per the backing global's
 * own real name -- see below).
 *
 * FOUND 2026-07-28, same `nm -C` class-inventory sweep as stream_family.h's
 * cluster (see PROJECT_BRAIN/status.md). Investigated-and-rejected a larger
 * candidate first: `CFF` (23 methods + `LINUX_FIND_DATA`'s 4, `.text+0x08e50150`),
 * a glob(3)-backed FindFirstFile/FindNextFile-style Linux file-finder with a
 * static 4-slot pattern cache. `CFF` genuinely has ZERO dependency on any other
 * unreconstructed Eva class (only libc: glob/globfree/__xstat/strcmp/strncpy/
 * strrchr/printf) so it passes the "self-contained" filter, but its own
 * INTERNAL algorithm is disproportionately deep for a single pass: `CFF::
 * makedata()` (0x633 bytes) implements real DOS-8.3 short-name generation with
 * duplicate-suffix detection against prior array entries (`~1`, `~2`, ... via
 * `LINUX_FIND_DATA::IncreaseIndexOfFileName()`, 0x22b bytes of its own,
 * 8-way-unrolled carry-propagation logic) PLUS an undeciphered "promote first
 * dot-prefixed entry to array[0]" rotation in `CFF::setupdatabuf()`, PLUS a
 * separate not-yet-triaged "deleted filename" tracking feature
 * (`MarkAsDeleted`/`CheckDeleted`/`RegisterShortFileName`) whose own field
 * usage wasn't fully pinned down in the time available. Reconstructing it
 * properly needs its own dedicated pass, not a shared slot with something
 * else -- deferred, not written here. `CNameBuff` was picked instead: smaller
 * (17 raw symbols, ctor/dtor/init/deletearray are literally empty bodies in
 * this build -- true no-ops, not missed coverage), a single flat field layout,
 * and every method fully traced with no open questions.
 *
 * Real class has NO base, NO virtuals (no vtable/typeinfo symbol at all --
 * confirmed via `nm -C`), and NO members beyond `mBuf`/`mCount` below; all
 * "storage" lives in a big external static array the class points itself at
 * (see NAME_BUF_SLOT_CAPACITY below), not anything CNameBuff itself owns or
 * frees -- consistent with `deletearray()`'s real empty body.
 *
 * SELF-CONTAINMENT: read from real disassembly, `.text+0x0838dd80..0x0838e207`
 * (`objdump -dr -M intel`). The only external references anywhere in this
 * cluster are `strlen`/`strncpy`/`strcpy` (libc) and the single external data
 * symbol `theDiskNameBuf` (`.bss+0x09654460`, `nm`-confirmed real global, size
 * 0x271000 bytes) that `setup()` points `mBuf` at -- not owned by CNameBuff,
 * not allocated by it, just borrowed. No calls into any other Eva class.
 *
 * FIELD LAYOUT (confirmed from every method's own address arithmetic):
 *   +0x00 (Slot*)        mBuf    -- always either NULL (never setup() yet) or
 *                                   exactly &theDiskNameBuf[0] once setup() runs.
 *   +0x04 (unsigned short) mCount -- slot capacity, clamped to at most 10000
 *                                   (0x2710) by setup() -- see the real-bug note
 *                                   on setup() below.
 * Each `Slot` is 0x10c (268) bytes (confirmed both from the `imul x,x,0x10c`
 * stride used by every accessor AND independently from the field offsets
 * below adding up to exactly 0x10c with 3 bytes of trailing alignment pad):
 *   +0x000..+0x0ef (241 bytes incl. forced NUL at +0xf0) char mName[]
 *   +0x0f4 (uint32) mSizeLo, +0x0f8 (uint32) mSizeHi        -- one `unsigned
 *                    long long` size, passed/returned as a 32+32 register
 *                    pair (real ABI for `y`/`unsigned long long` on this
 *                    32-bit target, not reproduced as a real 64-bit field
 *                    here -- see Set/GetSize below)
 *   +0x0fc/+0x100     mSecondarySizeLo/Hi  -- same shape, second size value
 *   +0x104 (int)      mFileKind  -- real ground-truth param type is
 *                      `EFileKind`, an enum whose real enumerator VALUES were
 *                      not recoverable from this cluster alone (never compared
 *                      against a literal anywhere in CNameBuff's own bodies,
 *                      only passed through opaquely) -- declared here as a
 *                      plain `int`-backed opaque enum with no asserted values,
 *                      TODO: pin down real values if a future pass reconstructs
 *                      one of CNameBuff's ~19 real callers (all `.text+0x083a4xxx
 *                      ../0x083ba0xx` range, unreconstructed disk-browser code).
 *   +0x108 (bool)     mAvailable
 *   +0x109..+0x10b    (3 bytes alignment padding, never read/written)
 *
 * `setup(unsigned short n)`'s REAL, CONFIRMED BUG (transcribed as-is, not
 * fixed): `mCount` is clamped to <=10000, but the per-slot "mark available"
 * init loop below it iterates using the ORIGINAL, UNCLAMPED `n` (both the
 * n<=10000 and n>10000 code paths funnel into the identical shared init-loop
 * entry point carrying the pre-clamp value in the same register) -- so a
 * caller passing n > 10000 would write past `theDiskNameBuf`'s real end
 * (10000*0x10c = 0x28e200 bytes > the buffer's real 0x271000-byte size, so
 * even n==10000 itself is already slightly past strict capacity by this
 * math). No real caller observed in this project passes n anywhere near that
 * range, but the real binary's own logic does not guard against it either.
 * The init loop itself (`.text+0x0838ddf8..0x0838df16`) is gcc's usual 8-way
 * Duff's-device unroll of a per-slot single-byte store (confirmed: every one
 * of its 7 unrolled entry variants performs the identical
 * `slot[i].mAvailable = true` store at a strictly-incrementing-by-0x10c
 * offset; the magic-multiply used to pick the unroll remainder entry point
 * does not change the observable per-slot result) -- reproduced below as an
 * ordinary loop, matching stream_family.h's already-established precedent for
 * this exact category of gcc-unrolled tail.
 *
 * Getters (`getname`/`getsize`/`getsecondarysize`/`getfkind`/`isavailable`)
 * CLAMP an out-of-range index to the LAST valid slot (`mCount - 1`), never
 * crash/return NULL. Setters (`setname`/`setsize`/`setsecondarysize`/
 * `setfkind`/`setavailable`) instead SILENTLY NO-OP on an out-of-range index
 * (no clamp, no write at all) -- a real, confirmed asymmetry between the two
 * families, transcribed as observed.
 */

#ifndef NAME_BUFF_H
#define NAME_BUFF_H

#include <string.h>
#include <stdint.h>

/* Real ground-truth backing store, `.bss+0x09654460`, `nm`-confirmed name and
 * size (0x271000 bytes). Not owned/allocated by CNameBuff -- see header
 * comment. Defined (not just declared) in name_buff.cpp.
 */
extern unsigned char theDiskNameBuf[0x271000];

/* Opaque, real ground-truth enum -- see header comment. Values not recovered
 * from this cluster; only ever passed through by CNameBuff::setfkind/getfkind,
 * never compared. */
enum EFileKind { kFileKind_Opaque_ = 0 };

class CNameBuff {
public:
	static const unsigned kMaxSlots = 10000;   /* 0x2710, real setup() clamp */
	static const unsigned kSlotStride = 0x10c; /* real per-slot byte stride */

	/* .text+0x0838dd80, 16 bytes. Real body: mCount=0, mBuf=NULL. */
	CNameBuff() : mBuf(0), mCount(0) {}

	/* .text+0x0838dda0. Real body: empty (does not free mBuf -- see header
	 * comment, mBuf is always borrowed, never owned). */
	~CNameBuff() {}

	/* .text+0x0838ddb0. Real body: empty (true no-op in this build). */
	void deletearray() {}

	/* .text+0x0838ddc0. Real body: empty (true no-op in this build). */
	void init() {}

	/* .text+0x0838ddd0, 336 bytes. See header comment for the real clamp/
	 * unclamped-loop-count bug, transcribed as-is. */
	void setup(unsigned short n)
	{
		unsigned loopCount = n; /* real: unclamped, even on the clamp path */
		mCount = n;
		if (n > kMaxSlots)
			mCount = kMaxSlots;
		mBuf = reinterpret_cast<unsigned char *>(theDiskNameBuf);
		for (unsigned i = 0; i < loopCount; i++)
			*SlotAvailablePtr(i) = 1;
	}

	/* .text+0x0838df40, 155 bytes. Real body: strncpy-truncates to 240 chars
	 * + forced NUL if `name` is longer, else plain strcpy. No-op if idx is
	 * out of range (idx >= mCount). */
	void setname(const char *name, int idx)
	{
		if (static_cast<unsigned>(idx) >= mCount)
			return;
		char *dst = SlotPtr(idx);
		if (strlen(name) > 0xf0) {
			strncpy(dst, name, 0xf0);
			dst[0xf0] = '\0';
		} else {
			strcpy(dst, name);
		}
	}

	/* .text+0x0838dfe0, 67 bytes. No-op if idx out of range. */
	void setsize(unsigned long long size, int idx)
	{
		if (static_cast<unsigned>(idx) >= mCount)
			return;
		WriteU64(SlotPtr(idx) + 0xf4, size);
	}

	/* .text+0x0838e030, 67 bytes. No-op if idx out of range. */
	void setsecondarysize(unsigned long long size, int idx)
	{
		if (static_cast<unsigned>(idx) >= mCount)
			return;
		WriteU64(SlotPtr(idx) + 0xfc, size);
	}

	/* .text+0x0838e080, 35 bytes. No-op if idx out of range. */
	void setfkind(EFileKind kind, int idx)
	{
		if (static_cast<unsigned>(idx) >= mCount)
			return;
		int32_t v = static_cast<int32_t>(kind);
		memcpy(SlotPtr(idx) + 0x104, &v, sizeof v);
	}

	/* .text+0x0838e0b0, 37 bytes. No-op if idx out of range. */
	void setavailable(bool avail, int idx)
	{
		if (static_cast<unsigned>(idx) >= mCount)
			return;
		*reinterpret_cast<unsigned char *>(SlotPtr(idx) + 0x108) = avail ? 1 : 0;
	}

	/* .text+0x0838e0e0, 41 bytes. Clamps idx to mCount-1 (not a crash/NULL
	 * path) if out of range. */
	const char *getname(int idx) const
	{
		return SlotPtr(ClampIdx(idx));
	}

	/* .text+0x0838e110, 61 bytes. Clamps idx to mCount-1 if out of range. */
	unsigned long long getsize(int idx) const
	{
		return ReadU64(SlotPtr(ClampIdx(idx)) + 0xf4);
	}

	/* .text+0x0838e150, 61 bytes. Clamps idx to mCount-1 if out of range. */
	unsigned long long getsecondarysize(int idx) const
	{
		return ReadU64(SlotPtr(ClampIdx(idx)) + 0xfc);
	}

	/* .text+0x0838e190, 55 bytes. Clamps idx to mCount-1 if out of range. */
	EFileKind getfkind(int idx) const
	{
		int32_t v;
		memcpy(&v, SlotPtr(ClampIdx(idx)) + 0x104, sizeof v);
		return static_cast<EFileKind>(v);
	}

	/* .text+0x0838e1d0, 56 bytes. Clamps idx to mCount-1 if out of range. */
	bool isavailable(int idx) const
	{
		return *reinterpret_cast<const unsigned char *>(
			SlotPtr(ClampIdx(idx)) + 0x108) != 0;
	}

private:
	char *SlotPtr(unsigned idx) const
	{
		return reinterpret_cast<char *>(mBuf) + idx * kSlotStride;
	}
	unsigned char *SlotAvailablePtr(unsigned idx) const
	{
		return reinterpret_cast<unsigned char *>(SlotPtr(idx) + 0x108);
	}
	/* Real ground truth: `idx < mCount ? idx : mCount - 1` -- only invoked
	 * from a getter, so mCount >= 1 is assumed the same way ground truth's
	 * own `(mCount-1)` computation does (an mCount==0 clamp-idx call is not
	 * exercised by any known real caller). */
	unsigned ClampIdx(int idx) const
	{
		unsigned uidx = static_cast<unsigned>(idx);
		return (uidx < mCount) ? uidx : static_cast<unsigned>(mCount - 1);
	}
	static void WriteU64(char *dst, unsigned long long v)
	{
		uint32_t lo = static_cast<uint32_t>(v);
		uint32_t hi = static_cast<uint32_t>(v >> 32);
		memcpy(dst + 0, &lo, sizeof lo);
		memcpy(dst + 4, &hi, sizeof hi);
	}
	static unsigned long long ReadU64(const char *src)
	{
		uint32_t lo, hi;
		memcpy(&lo, src + 0, sizeof lo);
		memcpy(&hi, src + 4, sizeof hi);
		return (static_cast<unsigned long long>(hi) << 32) | lo;
	}

	unsigned char *mBuf;   /* +0x00 */
	unsigned short mCount; /* +0x04 */
};

#endif /* NAME_BUFF_H */
