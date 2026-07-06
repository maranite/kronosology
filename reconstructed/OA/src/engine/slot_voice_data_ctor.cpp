// SPDX-License-Identifier: GPL-2.0
/*
 * slot_voice_data_ctor.cpp  -  CSTGSlotVoiceData::CSTGSlotVoiceData()
 * (sec 10.155, `.text+0xb2fd0`, 701 bytes) plus its two newly-discovered
 * embedded-sub-object ctors, `CSTGMidiCCFilter::Initialize()`
 * (`.text+0xd05b0`, 45 bytes) and `CSTGHeldKeyList::CSTGHeldKeyList()`
 * (`.text+0xa2470`, 96 bytes) -- matching this project's established
 * "reconstruct a small ctor alongside its own newly-discovered embedded
 * sub-object dependencies in one dedicated TU" precedent (CSTGToneAdjust/
 * CSTGProgramSlot, sec 10.153).
 *
 * Deliberately a separate translation unit from global.cpp/managers.cpp:
 * test_global_ctor.cpp keeps its own load-bearing call-counter mock for
 * CSTGSlotVoiceData's own ctor (`g_slotVoiceDataCalls++`), matching the
 * same per-unit convention already used throughout this project (confirmed
 * via a repo-wide grep for this exact ctor symbol across the verify test
 * directory -- only test_global_ctor.cpp, which does not link this TU).
 *
 * No vtable installs anywhere in this file (confirmed via `nm -C OA.ko |
 * grep "vtable for CSTGSlotVoiceData\|CSTGHeldKeyList\|CSTGMidiCCFilter"`,
 * no matches -- none of these three classes are polymorphic) and no
 * vtable DISPATCH either -- every call is a plain non-virtual real
 * function (two RTAI mutex-setup primitives already established
 * elsewhere in this project, e.g. `CPowerOffTimer::CPowerOffTimer()` in
 * managers.cpp, plus `CSTGBankMemory::AllocAligned`).
 *
 * CSTGSlotVoiceData's own confirmed field map (all via direct
 * objdump -d -r, this pass):
 *   +0x04/+0x08/+0x10  zeroed dwords
 *   +0x0c/+0x1c/+0x2c  confirmed real: each set to the raw `this`
 *               pointer itself (NOT `&self[offset]` -- a genuine
 *               "owner back-pointer" idiom, distinct from the
 *               self-referencing list-sentinel idiom used elsewhere in
 *               this project; real meaning of these three fields not
 *               independently determined)
 *   +0x14/+0x18/+0x20/+0x24/+0x28/+0x30/+0x34/+0x38/+0x3c  zeroed
 *   +0x40/+0x42  zeroed bytes
 *   +0x44/+0x50  the two confirmed real linked-list heads already on
 *               record from EmergencyFreeAllVoices()/Steal() (sec
 *               10.138/10.140) -- zeroed here, independent
 *               cross-confirmation of their real meaning
 *   +0x48/+0x4c/+0x54/+0x58/+0x5c/+0x60/+0x64  zeroed
 *   +0x1468/+0x146c  two real `rtwrap_malloc`'d mutex pointers (packed
 *               32-bit, matching this project's established convention),
 *               each `rtwrap_pthread_mutex_init(mutex, 0)`'d
 *   +0x1470/+0x1474  a zeroed dword and a dword set to 0xffffffff
 *   +0x1478/+0x147c  two `CSTGBankMemory::AllocAligned(0x6c00, 0x10)`
 *               buffer pointers (packed 32-bit)
 *   +0x1488..+0x1a33  a confirmed real 121-entry x 12-byte-stride
 *               array (120 entries via an explicit loop, the 121st
 *               unrolled immediately after -- byte-identical per-entry
 *               shape, see below); real element count/meaning not
 *               independently determined beyond the confirmed shape
 *   +0x1db4..+0x1dc3  the embedded CSTGMidiCCFilter (explicitly zeroed
 *               here, then `Initialize()`d -- see oa_global.h)
 *   +0x1e14/+0x1e18/+0x1e1c/+0x1e3c..+0x1e78/+0x1e7c  a confirmed real,
 *               only PARTIALLY-touched field cluster in the gap between
 *               the embedded CSTGMidiCCFilter (ends +0x1dc4) and the
 *               embedded CSTGHeldKeyList (+0x1e80) -- real gaps at
 *               +0x1dc4..+0x1e13 and +0x1e20..+0x1e3b are confirmed
 *               real untouched, not a transcription omission
 *   +0x1e80..+0x288b  the embedded CSTGHeldKeyList (own ctor, see
 *               oa_global.h)
 *   +0x28c8/+0x28dc/+0x28dd/+0x28de/+0x28df  5 more confirmed zeroed
 *               bytes, AFTER the embedded CSTGHeldKeyList ends (+0x288c
 *               -- so these do NOT overlap it, despite the numerically
 *               close-looking offsets; confirmed by direct arithmetic,
 *               not assumed)
 *
 * Each 12-byte entry in the +0x1488 array shares the exact same
 * confirmed per-entry shape: word@+0x8 zeroed, dword@+0x0 zeroed,
 * dword@+0x4 zeroed, byte@+0xa set to 1, byte@+0xb a confirmed real
 * AND/OR-mask read-modify-write (`(orig | 1) & ~2` -- sets bit 0,
 * clears bit 1, preserves bits 2-7 of whatever was already there on
 * this freshly placement-new'd object). Reproduced as a small helper
 * applied 121 times (120 via loop + 1 unrolled, matching the real
 * disassembly's own instruction split) rather than duplicated 121 times
 * inline.
 */

#include "oa_global.h"
#include "oa_bank_memory.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

extern "C" {
unsigned int get_sizeof_rtwrap_pthread_mutex(void);
void *rtwrap_malloc(unsigned int size);
void rtwrap_pthread_mutex_init(void *mutex, void *attr);
}

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * CSTGMidiCCFilter::Initialize() -- see oa_global.h's own header comment.
 * Confirmed real: `for (cc = 0; cc < 0x78; cc++) bits[cc>>5] |= (1u <<
 * (cc & 0x1f));` -- an OR, not an assignment (relies on the caller having
 * already zeroed this sub-object, which CSTGSlotVoiceData's own ctor
 * confirmed does immediately beforehand).
 */
void CSTGMidiCCFilter::Initialize()
{
	for (unsigned int cc = 0; cc < 0x78; cc++)
		bits[cc >> 5] |= (1u << (cc & 0x1f));
}

/*
 * CSTGHeldKeyList::CSTGHeldKeyList() -- see oa_global.h's own header
 * comment. 128 nodes at 0x14-byte stride, then a 12-byte head/tail/count
 * trailer, then a confirmed real (functionally inert) second pass
 * re-zeroing every node's own +0x0 a second time.
 */
CSTGHeldKeyList::CSTGHeldKeyList()
{
	unsigned char *self = (unsigned char *)this;

	for (unsigned int i = 0; i < 128; i++) {
		unsigned char *node = self + i * 0x14;
		*(unsigned int *)(node + 0xc) = ToU32(node);
		*(unsigned int *)(node + 0x4) = 0;
		*(unsigned int *)(node + 0x8) = 0;
		*(unsigned int *)(node + 0x10) = 0;
		*(unsigned int *)(node + 0x0) = 0;
	}

	*(unsigned int *)(self + 0xa04) = 0;
	*(unsigned int *)(self + 0xa00) = 0;
	*(unsigned int *)(self + 0xa08) = 0;

	/* Confirmed real second pass -- functionally inert, preserved as a
	 * genuine double-write quirk (same class of quirk already on record
	 * for CSTGProgramSlot, sec 10.153). */
	for (unsigned int i = 0; i < 128; i++) {
		unsigned char *node = self + i * 0x14;
		*(unsigned int *)(node + 0x0) = 0;
	}
}

/* Applies the confirmed real per-entry shape of the +0x1488 121-entry
 * array (see this file's own header comment) to one 12-byte entry. */
static void InitVoiceSlotEntry(unsigned char *e)
{
	unsigned char flags = e[0xb];
	*(unsigned short *)(e + 0x8) = 0;
	*(unsigned int *)(e + 0x0) = 0;
	flags = (unsigned char)((flags | 0x1) & ~0x2);
	*(unsigned int *)(e + 0x4) = 0;
	e[0xa] = 1;
	e[0xb] = flags;
}

CSTGSlotVoiceData::CSTGSlotVoiceData()
{
	unsigned char *self = (unsigned char *)this;

	*(unsigned int *)(self + 0x4) = 0;
	*(unsigned int *)(self + 0x8) = 0;
	*(unsigned int *)(self + 0x10) = 0;
	/* Confirmed real: the raw `this` pointer itself (not `&self[off]`),
	 * an "owner back-pointer" idiom -- see this file's own header
	 * comment. */
	*(unsigned int *)(self + 0xc) = ToU32(self);
	*(unsigned int *)(self + 0x1c) = ToU32(self);
	*(unsigned int *)(self + 0x14) = 0;
	*(unsigned int *)(self + 0x18) = 0;
	*(unsigned int *)(self + 0x20) = 0;
	*(unsigned int *)(self + 0x2c) = ToU32(self);
	*(unsigned int *)(self + 0x24) = 0;
	*(unsigned int *)(self + 0x28) = 0;
	*(unsigned int *)(self + 0x30) = 0;
	*(unsigned int *)(self + 0x48) = 0;
	*(unsigned int *)(self + 0x44) = 0; /* confirmed voice-list head,
					      * see EmergencyFreeAllVoices() */
	*(unsigned int *)(self + 0x4c) = 0;
	*(unsigned int *)(self + 0x54) = 0;
	*(unsigned int *)(self + 0x50) = 0; /* confirmed voice-list head,
					      * see EmergencyFreeAllVoices() */
	*(unsigned int *)(self + 0x58) = 0;
	*(unsigned int *)(self + 0x60) = 0;
	*(unsigned int *)(self + 0x5c) = 0;
	*(unsigned int *)(self + 0x64) = 0;

	unsigned int mutex1Size = get_sizeof_rtwrap_pthread_mutex();
	void *mutex1 = rtwrap_malloc(mutex1Size);
	*(unsigned int *)(self + 0x1468) = ToU32(mutex1);
	rtwrap_pthread_mutex_init(mutex1, 0);

	unsigned int mutex2Size = get_sizeof_rtwrap_pthread_mutex();
	void *mutex2 = rtwrap_malloc(mutex2Size);
	*(unsigned int *)(self + 0x146c) = ToU32(mutex2);
	rtwrap_pthread_mutex_init(mutex2, 0);

	/* 121-entry x 12-byte array at +0x1488: 120 entries via the real
	 * loop, the 121st unrolled immediately after (byte-identical shape,
	 * matching the real disassembly's own instruction split). */
	for (unsigned int i = 0; i < 120; i++)
		InitVoiceSlotEntry(self + 0x1488 + i * 0xc);
	InitVoiceSlotEntry(self + 0x1a28);

	/* Embedded CSTGMidiCCFilter at +0x1db4: zero its 4 dwords, THEN
	 * call Initialize() (which itself only ORs bits in -- see
	 * CSTGMidiCCFilter::Initialize()'s own header comment). */
	*(unsigned int *)(self + 0x1dc0) = 0;
	*(unsigned int *)(self + 0x1dbc) = 0;
	*(unsigned int *)(self + 0x1db8) = 0;
	*(unsigned int *)(self + 0x1db4) = 0;
	((CSTGMidiCCFilter *)(self + 0x1db4))->Initialize();

	/* Confirmed real, only-partially-touched gap fields before the
	 * embedded CSTGHeldKeyList (+0x1e80) -- see this file's own header
	 * comment for the confirmed real untouched sub-ranges. */
	*(unsigned int *)(self + 0x1e18) = 0;
	*(unsigned int *)(self + 0x1e14) = 0;
	*(unsigned int *)(self + 0x1e1c) = 0;
	self[0x1e7c] = 0;

	new (self + 0x1e80) CSTGHeldKeyList();

	self[0x40] = 0;
	self[0x42] = 0;
	*(unsigned int *)(self + 0x38) = 0;
	*(unsigned int *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x3c) = 0;
	*(unsigned int *)(self + 0x1474) = 0xffffffff;
	*(unsigned int *)(self + 0x1470) = 0;

	unsigned char *bufA = CSTGBankMemory::AllocAligned(0x6c00, 0x10);
	*(unsigned int *)(self + 0x1478) = ToU32(bufA);
	unsigned char *bufB = CSTGBankMemory::AllocAligned(0x6c00, 0x10);
	*(unsigned int *)(self + 0x147c) = ToU32(bufB);

	*(unsigned int *)(self + 0x1e3c) = 0;
	*(unsigned int *)(self + 0x1e40) = 0;
	*(unsigned int *)(self + 0x1e44) = 0;
	*(unsigned int *)(self + 0x1e48) = 0;
	*(unsigned int *)(self + 0x1e4c) = 0;
	*(unsigned int *)(self + 0x1e50) = 0;
	*(unsigned int *)(self + 0x1e54) = 0;
	*(unsigned int *)(self + 0x1e58) = 0;
	*(unsigned int *)(self + 0x1e5c) = 0;
	*(unsigned int *)(self + 0x1e60) = 0;
	*(unsigned int *)(self + 0x1e64) = 0;
	*(unsigned int *)(self + 0x1e68) = 0;
	*(unsigned int *)(self + 0x1e6c) = 0;
	*(unsigned int *)(self + 0x1e70) = 0;
	*(unsigned int *)(self + 0x1e74) = 0;
	*(unsigned int *)(self + 0x1e78) = 0;

	/* Confirmed AFTER the embedded CSTGHeldKeyList ends (+0x288c) --
	 * see this file's own header comment for the arithmetic. */
	self[0x28dc] = 0;
	self[0x28de] = 0;
	self[0x28dd] = 0;
	self[0x28df] = 0;
	self[0x28c8] = 0;
}
