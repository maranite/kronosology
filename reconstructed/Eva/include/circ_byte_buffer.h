/*
 * circ_byte_buffer.h  -  CCircByteBuffer, a power-of-two-sized circular byte ring
 * buffer, Stage 6 breadth sweep, 2026-07-25 (DumpManager cluster batch).
 *
 * REACHABILITY: this is the base storage every `CDumpManStateMachine` (and, through
 * it, every `CDumpMachine`/`CDumpTask`) embeds via `CDumpBuffer` (dump_buffer.h), and
 * `CDumpBuffer` is itself embedded inside `CBufferingTask` (buffering_task.h) --
 * constructed for real once `CDumpManMod::Setup()` (dump_man_mod.h) runs, itself
 * dispatched by `CModuleManager::Setup()`'s already-real per-module vtable+8 loop
 * (module_manager.cpp) once `MMainDumpMan()` (mains.cpp, already Tier A) registers a
 * `CDumpManMod` module. Found via the same broad `nm -C` class-inventory sweep
 * technique this project has used throughout Stage 6 (checking what real classes the
 * `CModule`+vtable-swap idiom's smaller derived modules, like `CDumpManMod`, actually
 * construct once their own `Setup()`/`Config()`/`Start()` are read instead of assumed
 * "too deep, Tier-B" by size alone -- `CDumpManMod` itself is only 9 real methods,
 * `nm -C`-counted, much smaller than the CForm/CSK-scale god-objects this project
 * correctly defers).
 *
 * REAL LAYOUT (0x18 bytes, confirmed from CCircByteBuffer@080ce1c0.c):
 *   +0x00  vtbl        installed once at construction (PTR__CCircByteBuffer_08e85b68,
 *                       3 slots -- install-only, matching this project's "never
 *                       dispatched through by any reconstructed code" convention,
 *                       omega_vtables.h)
 *   +0x04  mBuffer      `new unsigned char[mCapacity]`
 *   +0x08  mWritePos    write cursor, wraps mod mCapacity
 *   +0x0c  mReadPos     read cursor, wraps mod mCapacity
 *   +0x10  mCount       bytes currently held (mWritePos - mReadPos, tracked
 *                       separately rather than derived, matching ground truth)
 *   +0x14  mCapacity    the REQUESTED size rounded DOWN to the nearest power of two
 *                       (real ctor's own find-highest-set-bit loop, collapsed here to
 *                       an equivalent `while (cap > 1 && !(cap & requested)) cap >>= 1`
 *                       -- same license as every other Duff's-device/manually-unrolled
 *                       loop collapse in this project). Used as `mCapacity - 1` for
 *                       every wraparound mask, i.e. genuinely required to stay a power
 *                       of two for Read()/Write() to behave correctly -- real ctor has
 *                       2 soft, non-enforcing asserts (capacity >= 2, capacity <=
 *                       0x4000) omitted here per this project's established "soft
 *                       assert, doesn't affect control flow" convention. Every real
 *                       caller in this cluster passes an already-power-of-two request
 *                       (`CDumpBuffer::CDumpBuffer(0x800)`), so this edge case is not
 *                       exercised on the traced call path either way.
 *
 * Read()/Write() both real ("bounded, split into at most 2 memcpy's across the wrap
 * boundary" -- the standard circular-buffer shape): Read() only proceeds (returns
 * true, advances mReadPos, decrements mCount) if `len <= mCount`; Write() only
 * proceeds if `len <= mCapacity - mCount` (else logs `Api+0x90` "overflow!" -- soft,
 * not modeled, matching every other Api+0x90/+0x94 diagnostic-only slot in this
 * project) and always mCount += len on success.
 */

#ifndef CIRC_BYTE_BUFFER_H
#define CIRC_BYTE_BUFFER_H

#include <cstddef>

class CCircByteBuffer {
public:
	/* .text+0x080ce1c0, 330 bytes. */
	explicit CCircByteBuffer(unsigned long requestedCapacity);

	/* .text+0x080ce160/0x080ce190 (non-deleting/deleting D1/D0 pair, same shape as
	 * every other class in this project) -- frees mBuffer if non-null.
	 */
	~CCircByteBuffer();

	/* .text+0x080ce140, 26 bytes. */
	void Reset();

	/* .text+0x080ce3e0, 167 bytes. Returns true and consumes `len` bytes iff
	 * `len <= mCount` (a straight boolean-as-int in ground truth); leaves the
	 * buffer untouched otherwise.
	 */
	bool Read(unsigned char *dst, unsigned long len);

	/* .text+0x080ce310, 202 bytes. Returns true and appends `len` bytes iff
	 * `len <= mCapacity - mCount`; logs (not modeled) and returns false otherwise.
	 */
	bool Write(const unsigned char *src, unsigned long len);

private:
	void          *mVtbl;
	unsigned char *mBuffer;
	unsigned int   mWritePos;
	unsigned int   mReadPos;
	unsigned int   mCount;
	unsigned int   mCapacity;

	friend struct CircByteBufferTestHooks;
};

#endif /* CIRC_BYTE_BUFFER_H */
