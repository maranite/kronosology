// SPDX-License-Identifier: GPL-2.0
/*
 * waveseq_setlist_init.cpp  -  CSTGWaveSeqData::Initialize()/
 * CSetListBank::Initialize() (sec 10.84), plus (batch 12)
 * CSTGWaveSequence::CSTGWaveSequence()/CSetList::CSetList() -- see that
 * pair's own header comment below for why they landed in this same
 * file.
 *
 * Deliberately a SEPARATE translation unit from global.cpp: both
 * symbols' mocks in test_global.cpp/test_global_ctor.cpp/test_engine.cpp
 * are load-bearing call-counting checks for CSTGGlobal::Initialize()'s
 * OWN dispatch (not these methods' internals), and the real bodies
 * below genuinely dispatch through hundreds of sub-objects' own vtable
 * pointers -- exercising that safely needs dedicated, purpose-built
 * memory setup, not a quick patch to the existing large shared tests.
 * Matches this project's own established per-unit file convention
 * (e.g. midi_queue_writer.cpp, sec 10.83).
 */

#include "oa_global.h"

/*
 * CSTGWaveSeqData::Initialize()/CSetListBank::Initialize() (sec 10.84,
 * .text+0x81860/0x2014c0 in OA_real.ko) -- confirmed real, IDENTICAL
 * shape: a loop over N embedded sub-objects (stride confirmed literal
 * per class), dispatching each one's own vtable slot 7 (`call
 * *0x1c(%edx)`, the SAME slot offset CSTGProgramSlot::Initialize()
 * dispatches through).
 * CSTGWaveSeqData: 598 (0x256) sub-objects, stride 0xd14 (3348 bytes),
 * each a CSTGWaveSequence (see this file's own CSTGWaveSequence::
 * CSTGWaveSequence() below -- same array, same stride, constructed by
 * CSTGGlobal::CSTGGlobal() step 8).
 * CSetListBank: 128 (0x80) sub-objects, stride 0x834 (2100 bytes), each
 * a CSetList (CSTGGlobal::CSTGGlobal() step 13).
 *
 * FIX (sec 10.229): this used to call the raw CallVtableSlot7(obj)
 * helper (`vtable[7]` off the object's own +0x0 pointer). Root-caused
 * via `objdump -r`/`objdump -sr -j .rodata._ZTV16CSTGWaveSequence`+
 * `.rodata._ZTV8CSetList` on OA_real.ko: BOTH real vtables are laid out
 * identically to CSTGGlobal's own (sec 10.228) -- 24 CSTGParamsOwner-
 * shaped relocations, with vtable_base+0x24 (== vptr+0x1c, i.e. this
 * exact "slot 7") resolving in EITHER case to the SAME real method,
 * `CSTGParamsOwner::UseDefaults()` (CSTGWaveSequence doesn't override
 * it despite overriding ValidateParamChange at vtable_base+0x1c/slot 5;
 * CSetList doesn't override it either). Exactly the sec 10.228 shape:
 * CSTGWaveSequence/CSetList are this project's other two "vtable
 * install only, no real C++ virtuals modeled" placeholder classes
 * (`_ZTV16CSTGWaveSequence[96]`/`_ZTV8CSetList[96]` below, both left
 * zero-filled since batch 12 -- see their own ctor comments) -- nothing
 * dispatched through them until now, so the placeholder's null slot 7
 * silently produced a real `EIP=0x0`/`CR2=0x0` null-function-pointer
 * crash live (`CSTGWaveSeqData::Initialize()+0x1d`, i==0, the array's
 * first element, called from `CSTGGlobal::Initialize()+0x27`).
 * `CSetListBank::Initialize()` is reachable from the very next
 * statement in `CSTGGlobal::Initialize()` (global.cpp) and has the
 * IDENTICAL bug -- fixed here in the same pass rather than waiting for
 * it to reproduce live one call site later.
 *
 * Fixed the same way as CSTGGlobal::Initialize()/ValidateParamChange's
 * own forwarding calls: a same-address `reinterpret_cast<CSTGParamsOwner
 * *>` call to the real, already-declared, deliberately-deferred no-op
 * `UseDefaults()` (bar2_stubs.cpp) instead of the raw vtable[7] fetch --
 * reproduces the identical `this` pointer the real dispatch would use,
 * without needing a real vtable populated on either per-element array.
 */
void CSTGWaveSeqData::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	for (unsigned int i = 0; i < 0x256; i++)
		reinterpret_cast<CSTGParamsOwner *>(base + i * 0xd14)->UseDefaults();
}

void CSetListBank::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	for (unsigned int i = 0; i < 0x80; i++)
		reinterpret_cast<CSTGParamsOwner *>(base + i * 0x834)->UseDefaults();
}

/*
 * CSTGWaveSequence::CSTGWaveSequence() / CSetList::CSetList() (batch 12)
 * -- confirmed real, but NEITHER has its own standalone symbol anywhere
 * in OA_real.ko (grepped the whole demangled symbol table for
 * `CSTGWaveSequenceC`/`CSetListC`, zero hits, unlike every other
 * model/manager ctor in this project). Both are fully INLINED at their
 * one real call site each: CSTGGlobal::CSTGGlobal()'s own two
 * array-construction loops (see global_ctor.cpp's header comment,
 * steps 8/13; ground truth `.text+0x3910`/`.text+0x3a38`):
 *   3910: movl $0x8,(%ecx)      ; reloc _ZTV16CSTGWaveSequence
 *   3a38: movl $0x8,(%ecx)      ; reloc _ZTV8CSetList
 * i.e. each ctor's ENTIRE real effect is the standard Itanium vtable-
 * pointer install (`_ZTVxxx+8`) -- every other byte
 * CSTGGlobal::CSTGGlobal()'s own loop writes right after (the
 * +0x5/+0x13 zero bytes and 64-entry inner zero-fill per
 * CSTGWaveSequence entry; the 128-entry inner zero-fill and trailing
 * byte per CSetList entry) belongs to CSTGGlobal's OWN ctor code, not
 * either sub-object's ctor -- already modeled that way in
 * global_ctor.cpp. Since the compiler never emitted an out-of-line
 * copy, a real (not guessed) C++ default ctor is provided here purely
 * so global_ctor.cpp's `new (ws) CSTGWaveSequence()`/`new (setList)
 * CSetList()` placement-new expressions keep compiling -- this is the
 * sec 10.153 "vtable install only" safe pattern: neither ctor
 * DISPATCHES through its own vtable, only writes the pointer, so a
 * zero-filled placeholder vtable body is safe.
 *
 * Homed in THIS file (not their own new TU) since this file already
 * owns the closely-related CSTGWaveSeqData::Initialize()/
 * CSetListBank::Initialize() pair operating on arrays of the exact same
 * stride/count (0xd14 x 0x256, 0x834 x 0x80) -- same domain, same
 * author's mental model, per this project's per-unit file convention.
 * `test_global_ctor.cpp` has its own load-bearing call-counting mocks
 * for both these exact ctors (verifying 598/128 real construction
 * counts) and does NOT link this file -- zero collision, matching the
 * CSTGMidiQueueWriter::Write precedent (sec 10.83).
 */
unsigned char _ZTV16CSTGWaveSequence[96];
unsigned char _ZTV8CSetList[96];

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGWaveSequence::CSTGWaveSequence()
{
	*(unsigned int *)this = ToU32(_ZTV16CSTGWaveSequence + 8);
}

CSetList::CSetList()
{
	*(unsigned int *)this = ToU32(_ZTV8CSetList + 8);
}
