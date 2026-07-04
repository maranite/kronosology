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

/* Real vtable data symbols, referenced via the `extern "C"` byte-array
 * trick established in sec 10.58/10.60 (CSTGRecordEvent/CCostProfile):
 * lets this file point at the confirmed-real symbols without needing
 * to fully define their contents (none of these three classes' own
 * virtual methods are reconstructed yet). */
extern "C" unsigned char _ZTV17CSTGVectorEGXOnly[];
extern "C" unsigned char _ZTV14CSTGVectorEGXY[];
extern "C" unsigned char _ZTV14CSTGVectorEGCC[];

/* Host/target pointer-width fix (this project's established pattern,
 * e.g. vector_manager_init.cpp): the real target's pointer fields are
 * plain 32-bit, tightly packed 4 bytes apart -- a native 8-byte host
 * pointer write here would overlap and corrupt the adjacent field. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGVectorEGXOnly::CSTGVectorEGXOnly()
{
	/* CSTGVectorEGBase::CSTGVectorEGBase() -- confirmed real, deferred,
	 * runs implicitly as this class's own base subobject construction
	 * (standard C++, no explicit call needed here). */
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	/* Derived vtable pointer, standard Itanium "+8 to skip offset-to-
	 * top/RTTI" convention (confirmed via the real relocation's own
	 * addend). */
	*(unsigned char **)p = _ZTV17CSTGVectorEGXOnly + 8;

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

CSTGVectorEGXY::CSTGVectorEGXY()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned char **)p = _ZTV14CSTGVectorEGXY + 8;

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

CSTGVectorEGCC::CSTGVectorEGCC()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned char **)p = _ZTV14CSTGVectorEGCC + 8;

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
