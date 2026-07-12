// SPDX-License-Identifier: GPL-2.0
/*
 * vector_eg_ctors.cpp  -  CSTGVectorEGXOnly/CSTGVectorEGXY/CSTGVectorEGCC's
 * own constructors (sec 10.66), the deliberately-deferred follow-up to
 * CSTGVectorManager's own constructor/Initialize() (sec 10.64/10.65).
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGVectorEGXOnly::CSTGVectorEGXOnly()  .text+0x7ee60,  99 bytes
 *   CSTGVectorEGCC::CSTGVectorEGCC()        .text+0x7bb60, 116 bytes
 *   CSTGVectorEGXY::CSTGVectorEGXY()        .text+0x7df70,  86 bytes
 * (Each symbol appears twice at the identical address/size -- two
 * relocation records against the same real function, not two distinct
 * functions -- matching this project's own established observation for
 * duplicate-address symbol-table entries elsewhere.)
 *
 * All three are small, branch-free, and near-identical in shape: call
 * the (confirmed real, genuinely new) base class `CSTGVectorEGBase`'s
 * own constructor first, set the derived vtable pointer, then a run
 * of confirmed literal field writes. Hand-transcribed directly (each
 * under 120 bytes, no scripted extraction needed at this scale).
 *
 * CSTGVectorEGBase discovery: all three derived constructors' very
 * first instruction after the prologue is a relocated call to
 * `_ZN16CSTGVectorEGBaseC2Ev` -- the standard confirmed real-
 * inheritance pattern in this codebase (matching CCostProfile :
 * public CStartupFile, sec 10.60). Its own body is a confirmed-real,
 * deliberately deferred extern (out of scope here) -- but
 * CSTGVectorEGXY's own constructor proves it touches at least one
 * byte past its return: `and BYTE PTR [this+0x6e],0xfd` clears bit 1
 * of a flags byte WITHOUT first writing the rest of it, meaning
 * something upstream (necessarily the base ctor, since nothing else
 * runs first) already set it.
 *
 * Confirmed real shared field layout (all three, same offsets):
 *   +0x3c/+0x40  intrusive list node (next/prev) -- zeroed here,
 *                populated by CSTGVectorManager::Initialize() (sec
 *                10.65) on EGXOnly/EGXY only (EGCC is never list-
 *                inserted despite having the same zeroed fields)
 *   +0x44        self-pointer (`this`) -- confirmed real, not
 *                independently understood beyond its confirmed value
 *   +0x48        list "owner" backpointer -- zeroed here, populated
 *                by Initialize() on list insertion (EGCC: never)
 */

#include "oa_engine_init.h"
#include "oa_global.h" /* CSTGGlobal::sInstance, read by CSTGVectorEGXOnly::Init() */

/* UPDATE (sec 10.227): these four classes are genuinely C++-polymorphic
 * (confirmed via `objdump -r` on OA_real.ko's own vtable relocations --
 * see oa_engine_init.h's CSTGVectorEGBase header comment for the full
 * ground-truthing). The manual `extern "C" unsigned char _ZTVxxx[]`
 * byte-array trick this file used to reference is GONE -- the compiler
 * now emits these exact mangled vtable symbols itself, from the real
 * `virtual void Init()` declared on each class below, and writes the
 * vtable pointer automatically as part of ordinary base/derived
 * construction (no more manual `*(unsigned char**)p = _ZTVxxx + 8`). */

/* Host/target pointer-width fix (this project's established pattern,
 * e.g. vector_manager_init.cpp): the real target's pointer fields are
 * plain 32-bit, tightly packed 4 bytes apart -- a native 8-byte host
 * pointer write here would overlap and corrupt the adjacent field. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/*
 * CSTGVectorEGBase::CSTGVectorEGBase() is real now too (sec 10.148,
 * `.text+0x7f820`, 22 bytes, C1Ev/C2Ev folded to the same address --
 * see oa_engine_init.h's own header comment for the full correction
 * this forced to sec 10.66's own earlier speculation about +0x6e).
 * Confirmed real body: base vtable pointer (standard Itanium "+8"
 * convention, immediately overwritten by each derived ctor's own
 * vtable pointer right after -- same real double-write pattern already
 * confirmed for CCostProfile : public CStartupFile, sec 10.60), then
 * `*(byte*)(this+0xc) = 0`, `*(byte*)(this+0xf) = 0`, and
 * `*(dword*)(this+8) = 0`. UPDATE (sec 10.227): the vtable-pointer store
 * is no longer hand-written -- this class (and its three derived
 * siblings) is now genuinely C++-polymorphic (real `virtual void
 * Init()`, see oa_engine_init.h), so the compiler emits that store
 * automatically, same as every other real-vtable class in this project
 * (e.g. CSTGAudioDriverInterfaceKorgUsb, sec 10.225).
 */
CSTGVectorEGBase::CSTGVectorEGBase()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned int *)(p + 0x8) = 0;
	p[0xc] = 0;
	p[0xf] = 0;
}

/*
 * CSTGVectorEGBase::Init() (.text+0x7f810, 5 bytes) confirmed real and
 * fully self-contained: resets the same +0xf flag byte the constructor
 * above already zeroes (an idempotent re-prime, not independently
 * understood beyond its confirmed effect). Called directly (non-
 * virtually) by each derived override below, matching the confirmed
 * disassembly.
 */
void CSTGVectorEGBase::Init()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	p[0xf] = 0;
}

CSTGVectorEGXOnly::CSTGVectorEGXOnly()
{
	/* CSTGVectorEGBase::CSTGVectorEGBase() (just above) runs implicitly
	 * as this class's own base subobject construction (standard C++, no
	 * explicit call needed here). The derived vtable pointer is also no
	 * longer hand-written here (sec 10.227) -- C++ does that
	 * automatically now that this class has a real `virtual void
	 * Init()` (oa_engine_init.h). */
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned int *)(p + 0x44) = (unsigned int)(unsigned long)p; /* self-pointer */
	*(unsigned int *)(p + 0x3c) = 0; /* list node: next */
	*(unsigned int *)(p + 0x40) = 0; /* list node: prev */
	*(unsigned int *)(p + 0x48) = 0; /* list owner backpointer */
	*(unsigned int *)(p + 0x64) = 0;
	*(unsigned int *)(p + 0x60) = 0;
	*(unsigned int *)(p + 0x68) = 0;
	*(unsigned int *)(p + 0x70) = 0;
	*(unsigned int *)(p + 0x6c) = 0;
	*(unsigned int *)(p + 0x74) = 0;
}

/*
 * CSTGVectorEGXOnly::Init() (.text+0x7ece0, 379 bytes) -- see
 * oa_engine_init.h's own header comment on this method for the full
 * confirmed-reachable-vs-confirmed-but-deferred breakdown. Implements
 * exactly the reachable-at-boot portion.
 */
void CSTGVectorEGXOnly::Init()
{
	CSTGVectorEGBase::Init();
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	p[0x80] = 0;

	/* Confirmed real: mirrors CSTGGlobal's own confirmed "mode" field
	 * (+0x684, the same field global.cpp's UpdateMIDIChannel/etc. family
	 * already reads -- sec 10.117 onward) into this object's own +0x4c. */
	unsigned int mode = *(unsigned int *)(
		reinterpret_cast<unsigned char *>(CSTGGlobal::sInstance) + 0x684);

	*(unsigned int *)(p + 0x5c) = 0;
	*(unsigned int *)(p + 0x58) = 0;
	*(unsigned int *)(p + 0x54) = 0;
	*(unsigned int *)(p + 0x50) = 0;
	*(unsigned int *)(p + 0x4c) = mode;

	/* Confirmed real, but PROVABLY unreachable the first (and, on this
	 * boot path, only) time Init() ever runs on a given object -- see
	 * the header comment. Left deferred rather than modeling the
	 * unidentified external pool-manager object these branches touch. */
	if (*(unsigned int *)(p + 0x60) != 0) {
		/* deferred: pool-A self-removal + external +0xb0/+0xb8 update */
	}
	if (*(unsigned int *)(p + 0x6c) != 0) {
		/* deferred: pool-B self-removal + external +0xb0/+0xb8 update */
	}
}

CSTGVectorEGXY::CSTGVectorEGXY()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned int *)(p + 0x44) = (unsigned int)(unsigned long)p;
	*(unsigned int *)(p + 0x3c) = 0;
	*(unsigned int *)(p + 0x40) = 0;
	*(unsigned int *)(p + 0x48) = 0;
	*(unsigned int *)(p + 0x60) = 0;
	*(unsigned int *)(p + 0x5c) = 0;
	*(unsigned int *)(p + 0x64) = 0;
	p[0x6d] = 0;
	p[0x6e] &= 0xfd; /* clear bit 1 of a flags byte the base ctor already set */
}

/*
 * CSTGVectorEGXY::Init() (.text+0x7de90, 211 bytes) -- reachable-at-boot
 * portion only, same deferral rationale as CSTGVectorEGXOnly::Init()
 * above (see oa_engine_init.h's header comment on this method).
 */
void CSTGVectorEGXY::Init()
{
	CSTGVectorEGBase::Init();
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	p[0x6e] &= 0xfd; /* same bit-clear the constructor already did (idempotent) */
	*(unsigned int *)(p + 0x54) = 0;
	*(unsigned int *)(p + 0x58) = 0;
	*(unsigned int *)(p + 0x50) = 0;
	*(unsigned int *)(p + 0x4c) = 0;

	if (*(unsigned int *)(p + 0x5c) != 0) {
		/* deferred: pool self-removal + external +0xb4/+0xb8 update,
		 * provably unreachable the first time Init() ever runs (same
		 * argument as CSTGVectorEGXOnly::Init()). */
	}
}

CSTGVectorEGCC::CSTGVectorEGCC()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned int *)(p + 0x44) = (unsigned int)(unsigned long)p;
	*(unsigned int *)(p + 0x3c) = 0;
	*(unsigned int *)(p + 0x40) = 0;
	*(unsigned int *)(p + 0x48) = 0;

	/* Confirmed real: four fields all pointing at the SAME shared
	 * default table (a real relocated global, not a literal zero). */
	*(unsigned int *)(p + 0x54) = ToU32(STGVJSAssignInfo);
	*(unsigned int *)(p + 0x58) = ToU32(STGVJSAssignInfo);
	*(unsigned int *)(p + 0x5c) = ToU32(STGVJSAssignInfo);
	*(unsigned int *)(p + 0x60) = ToU32(STGVJSAssignInfo);

	/* Four confirmed real centered defaults (signed 16-bit mid-range). */
	*(unsigned short *)(p + 0x66) = 0x8000;
	*(unsigned short *)(p + 0x68) = 0x8000;
	*(unsigned short *)(p + 0x6a) = 0x8000;
	*(unsigned short *)(p + 0x6c) = 0x8000;

	*(unsigned int *)(p + 0x4c) = 0;
}

/*
 * CSTGVectorEGCC::Init() (.text+0x7bb10, 69 bytes) confirmed real and
 * FULLY self-contained (no deferred portion, unlike its two siblings
 * above) -- see oa_engine_init.h's header comment on this method.
 */
void CSTGVectorEGCC::Init()
{
	CSTGVectorEGBase::Init();
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	unsigned short index = *(unsigned short *)(p + 0x4);
	*(unsigned int *)(p + 0x4c) = 0;
	if (index == 0x10)
		return;

	*(unsigned int *)(p + 0x54) = 0;
	*(unsigned int *)(p + 0x58) = 0;
	*(unsigned int *)(p + 0x5c) = 0;
	*(unsigned int *)(p + 0x60) = 0;
}
