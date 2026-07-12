// SPDX-License-Identifier: GPL-2.0
/*
 * midi_queue_init.cpp  -  CSTGMidiQueue::Initialize()/SetDesc() (sec
 * 10.230/MASTER_REFERENCE).
 *
 * Deliberately its OWN translation unit, separate from midi_queue.cpp
 * (which has GetNumWritableBytes()/Reset()): these two methods need the
 * REAL CSTGHeapManager class (oa_heapmanager.h, `Alloc(unsigned long)`,
 * `_ZN15CSTGHeapManager5AllocEm`), which drags heap_manager.cpp's own
 * `CSTGHeapManager::sInstance` definition into every TU that links this
 * file -- test_global.cpp/test_midi_dispatcher.cpp only ever needed
 * GetNumWritableBytes()/Reset() from midi_queue.cpp and must NOT be made
 * to pull in the heap manager too. Matches this project's established
 * "give it its own TU" convention (see midi_queue_writer.cpp's own
 * header comment for the precedent).
 *
 * Root-caused via `objdump -dr --start-address=0x3ffe0` on OA_real.ko:
 * this class's `Initialize(CSTGMidiQueue::eFormat, unsigned int)`
 * (modeled here as a plain `unsigned int format` -- only two concrete
 * values, 0 and 1, are ever observed at real call sites and neither's
 * enum meaning is independently determined) and `SetDesc(const char*,
 * ...)` were BOTH entirely unimplemented before this pass -- the actual
 * root cause of the `CSTGMidiQueueWriter::Write()` ringCtl-NULL crash
 * traced in sec 10.230 was `CSTGMidiPortManager::Initialize()` being a
 * no-op stub that never called these. See oa_engine_init.h's own
 * declaration comments for the full field-by-field breakdown, and
 * midi_port_manager.cpp for the caller.
 */

#include "oa_global.h"
#include "oa_engine_init.h"	/* for CSTGMidiQueue */
#include "oa_heapmanager.h"	/* for the REAL CSTGHeapManager::Alloc(unsigned
				 * long) -- confirmed via relocation
				 * (`_ZN15CSTGHeapManager5AllocEm`), NOT the
				 * "raw-offset static" `Alloc(unsigned int)`
				 * stand-in used elsewhere in this project.
				 * Self-contained (no #include of its own), so
				 * unlike oa_heap.h this doesn't drag in
				 * oa_types.h's conflicting minimal CSTGGlobal
				 * stand-in -- safe to include directly
				 * alongside oa_global.h's fuller CSTGGlobal. */
/* Plain C <stdarg.h>, not <cstdarg>: this file also builds under the
 * freestanding Kbuild target (-ffreestanding -fno-builtin -m32), where
 * <cstdarg>'s libstdc++ wrapper needs a 32-bit `bits/c++config.h` that
 * isn't installed on every build host -- <stdarg.h> resolves to GCC's
 * own compiler-builtin header instead, no libstdc++ dependency. */
#include <stdarg.h>

extern "C" __attribute__((regparm(3)))
int vsnprintf(char *buf, unsigned long size, const char *fmt, va_list args);

/*
 * SetDesc(const char *fmt, ...) (`.text+0x3ffb0`) confirmed real: a
 * plain `vsnprintf(this+0x21, 0x40, fmt, args)`. Every real call site
 * (CSTGMidiPortManager::Initialize()) passes a literal label string with
 * no variadic arguments -- exercised here for signature fidelity only.
 */
void CSTGMidiQueue::SetDesc(const char *fmt, ...)
{
	unsigned char *self = (unsigned char *)this;
	va_list args;

	va_start(args, fmt);
	vsnprintf((char *)(self + 0x21), 0x40, fmt, args);
	va_end(args);
}

/*
 * Initialize(unsigned int format, unsigned int size) (`.text+0x3ffe0`)
 * confirmed real -- see this method's own declaration comment in
 * oa_engine_init.h for the full field-by-field breakdown. The real
 * disassembly's own FIRST action is `SetDesc(this, "")` -- a literal
 * empty format string at `.rodata.str1.1+0x40d` (confirmed via direct
 * byte read, not a placeholder guess), i.e. a generic "clear the
 * description" default -- BEFORE the real allocation. Every real caller
 * (CSTGMidiPortManager::Initialize()) then immediately calls `SetDesc()`
 * again with its own real label string right after this returns,
 * overwriting this default -- reproduced faithfully rather than
 * "optimized away" as redundant.
 */
void CSTGMidiQueue::Initialize(unsigned int format, unsigned int size)
{
	unsigned char *self = (unsigned char *)this;

	SetDesc("");

	unsigned int handle = CSTGHeapManager::sInstance->Alloc(size);

	*(unsigned int *)(self + 0x0) = handle;
	*(unsigned int *)(self + 0x4) = format;
	*(unsigned int *)(self + 0x8) = size - 1;
	*(unsigned int *)(self + 0xc) = 0;
	*(unsigned int *)(self + 0x10) = 0;
	*(unsigned int *)(self + 0x14) = 0;
	*(unsigned int *)(self + 0x18) = 0;
	*(unsigned int *)(self + 0x1c) = 0;
	self[0x20] = 0;
}
