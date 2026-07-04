// SPDX-License-Identifier: GPL-2.0
/*
 * global_ctor.cpp  -  CSTGGlobal::CSTGGlobal(). See oa_global.h for
 * the new sub-class declarations this constructor references.
 *
 * Ground-truthed via a full objdump -d -r disassembly of the entire
 * 3124-byte constructor (.text+0x38b0). Kept in its own file, separate
 * from global.cpp's own UpdateXXX handler family, matching this
 * project's established "one file per major reconstructed unit"
 * convention (e.g. comport.cpp/comport_init.cpp).
 *
 * Confirmed real construction sequence, in order:
 *   1. vtable pointer (_ZTV10CSTGGlobal+8, standard Itanium primary
 *      vtable offset).
 *   2. CSTGAudioBusManager at +0x4.
 *   3. CSTGControllerRTData at +0x10 (confirming the aliasing question
 *      Initialize()/UpdateFootswitchPolarity's own comments raised,
 *      sec 10.55).
 *   4. CSTGSamplingInterface at +0x98.
 *   5. CSTGAudioInput at +0x608.
 *   6. Two zeroed bytes at +0x6dc/+0x6dd.
 *   7. CSTGDrumKitData at +0x6e8.
 *   8. 598 CSTGWaveSequence objects (confirmed literal 0x256) at
 *      +0x1143c10, stride 0xd14, each with its own vtable pointer, two
 *      zeroed bytes (+0x5/+0x13), and a 64-entry inner zero-fill
 *      (bytes at +0x42+i*0x34 and +0x45+i*0x34).
 *   9. A single, confirmed pure-zero memset over 0x1c08 bytes at
 *      +0x132c8c8 (the real disassembly unrolls this as a 3-dword
 *      pattern per 0xc-byte stride, all zero -- modeled here as a
 *      single memset, matching this project's own established
 *      "unrolled zero-fill is memset-equivalent" precedent from
 *      setup_global_resources.cpp).
 *  10. 2944 CSTGProgram objects (23 confirmed banks x 128 confirmed
 *      programs/bank, matching the real Kronos program-bank
 *      architecture) at +0x132e4d0, each bank prefixed by 3 header
 *      bytes (2 confirmed zeroed, 1 left alone) and 0x67600 bytes of
 *      program data (128 x 0xcec).
 *  11. 1792 CSTGCombi objects (14 banks x 128) at +0x1c77f15.
 *  12. 200 CSTGSequence objects (no banking) at +0x27cd024.
 *  13. 128 CSetList objects at +0x293374c, each with its own vtable
 *      pointer, a 128-entry inner zero-fill (bytes at +0x4+i*0x10),
 *      and a trailing zeroed byte at +0x831.
 *  14. A standalone CSTGSequence at +0x2975186 (a "scratch"/current-
 *      edit object, not part of the 200-entry array above).
 *  15. A standalone CSTGProgram at +0x2977cf4 (same "scratch" pattern).
 *  16. CSTGProgramModeProgramSlot at +0x2977b1f.
 *  17. CSTGProgramModeDrumTrackSlot at +0x2977c08.
 *  18. 32 CSTGSlotVoiceData objects at +0x2977cf4, stride 0x28e0 --
 *      the exact same array `CSTGGlobal::Initialize()` (sec 10.55)
 *      later re-initializes via its own confirmed loop.
 *  19. A large run of confirmed zero-writes across several field
 *      ranges (+0x29c98f4..+0x29c9a94 and a handful of smaller ones)
 *      -- the real disassembly unrolls these individually rather than
 *      looping; modeled here as memset() over each confirmed
 *      contiguous range, same precedent as step 9.
 *   20. sInstance = this.
 *   21. A confirmed real per-slot default-value table: alternating
 *       0x10/0xff bytes across several field ranges (+0x29c9fc8 etc.,
 *       and a smaller +0x29cc0c8-ish range matching several already-
 *       reconstructed UpdateXXXMIDIChannel handlers' own confirmed
 *       field offsets) -- these are genuine non-zero defaults, not
 *       zero-fills, transcribed exactly.
 *   22. A handful of final individual field writes, including two
 *       confirmed non-zero ASCII bytes ('v','w' at +0x6c0/+0x6c1 --
 *       real meaning not determined, transcribed factually) and
 *       several fields this project's own UpdateXXX handlers already
 *       confirmed independently (+0x6d7..+0x6db, +0x29cc118,
 *       +0x29cc4e8).
 */

#include "oa_global.h"
#include "oa_engine.h"	/* for CSTGAudioBusManager */
#include "oa_internal.h" /* placement operator new(size_t, void*) */

/* No <cstring> here -- it conflicts with oa_internal.h's own `strlen`
 * declaration (different exception specifier), same reasoning
 * stgheap_init.cpp/setup_global_resources.cpp already established for
 * inlining their own zero-fill rather than using libc memset. */
static void local_zero(void *p, unsigned long n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned long i = 0; i < n; i++)
		b[i] = 0;
}

CSTGGlobal::CSTGGlobal()
{
	unsigned char *self = (unsigned char *)this;

	/* Step 1: vtable. Modeled as a raw pointer write to the confirmed
	 * relocation target's own address is not reproducible host-side
	 * (the real vtable symbol isn't linked here); the field write
	 * itself is inherent to C++ construction and not separately
	 * modeled -- this reconstruction relies on the derived class's own
	 * (absent, since CSTGGlobal has no virtual methods declared in
	 * this project) vtable if one is ever added. No explicit write
	 * needed here since this project's CSTGGlobal has no virtuals. */

	new (self + 0x4) CSTGAudioBusManager();
	new (self + 0x10) CSTGControllerRTData();
	new (self + 0x98) CSTGSamplingInterface();
	new (self + 0x608) CSTGAudioInput();
	self[0x6dc] = 0;
	self[0x6dd] = 0;
	new (self + 0x6e8) CSTGDrumKitData();

	for (int i = 0; i < 0x256; i++) {
		unsigned char *ws = self + 0x1143c10 + i * 0xd14;
		new (ws) CSTGWaveSequence();
		ws[0x5] = 0;
		ws[0x13] = 0;
		for (unsigned int j = 0; j < 0xd00; j += 0x34) {
			ws[0x42 + j] = 0;
			ws[0x45 + j] = 0;
		}
	}

	local_zero(self + 0x132c8c8, 0x1c08);

	{
		unsigned char *bank = self + 0x132e4d0;
		for (int b = 0; b < 0x17; b++) {
			bank[0] = 0;
			bank[2] = 0;
			unsigned char *prog = bank + 3;
			for (unsigned int p = 0; p < 0x67600; p += 0xcec)
				new (prog + p) CSTGProgram();
			bank += 0x67603;
		}
	}

	{
		unsigned char *bank = self + 0x1c77f15;
		for (int b = 0; b < 14; b++) {
			unsigned char *combi = bank + 1;
			for (unsigned int c = 0; c < 0xcf380; c += 0x19e7)
				new (combi + c) CSTGCombi();
			bank += 0xcf381;
		}
	}

	{
		unsigned char *seq = self + 0x27cd024;
		for (unsigned int s = 0; s < 0x166728; s += 0x1cad)
			new (seq + s) CSTGSequence();
	}

	{
		unsigned char *setList = self + 0x293374c;
		for (int i = 0; i < 0x80; i++) {
			new (setList) CSetList();
			for (unsigned int j = 4; j < 0x800; j += 0x10)
				setList[j] = 0;
			setList[0x831] = 0;
			setList += 0x834;
		}
	}

	new (self + 0x2975186) CSTGSequence();
	new (self + 0x2977cf4) CSTGProgram();
	new (self + 0x2977b1f) CSTGProgramModeProgramSlot();
	new (self + 0x2977c08) CSTGProgramModeDrumTrackSlot();

	{
		unsigned char *entry = self + 0x2977cf4;
		for (unsigned int i = 0; i < 0x51c00; i += 0x28e0)
			new (entry + i) CSTGSlotVoiceData();
	}

	local_zero(self + 0x29c98f4, 0x29c9a94 + 4 - 0x29c98f4);

	sInstance = this;

	self[0x6ae] = 0;
	local_zero(self + 0x29c9fa8, 8);	/* +0x29c9fa8, +0x29c9fac */
	local_zero(self + 0x29c9fb0, 8);	/* +0x29c9fb0, +0x29c9fb4 */
	self[0x29c9fb8] = 0;
	*(unsigned int *)(self + 0x6bc) = 0;
	self[0x6b8] = 0;
	self[0x6b9] = 0;
	self[0x6ad] = 0;
	*(unsigned int *)(self + 0x6a8) = 0;
	self[0x29c9fc0] = 0;
	*(unsigned int *)(self + 0x6e4) = 0;
	self[0x6c0] = 'v';
	self[0x6c1] = 'w';
	self[0x6c2] = 0;
	*(unsigned int *)(self + 0x29c9f98) = 0;
	*(unsigned int *)(self + 0x29c9f9c) = 0;
	self[0x29cc118] = 0;
	self[0x6af] = 1;
	*(unsigned int *)(self + 0x6b0) = 0;
	*(unsigned int *)(self + 0x684) = 0;

	local_zero(self + 0x688, 0x6a0 + 4 - 0x688);
	self[0x6a4] = 0;
	self[0x6a5] = 0;
	self[0x6a6] = 0;

	/*
	 * Two confirmed "current program/combi" scratch sub-fields --
	 * distinct from the standalone CSTGProgram/CSTGSequence objects
	 * constructed above, own real semantics not determined beyond the
	 * confirmed literal values (0/1 flags, not guessed further).
	 */
	self[0x297514c] = 0;
	*(unsigned int *)(self + 0x2975150) = 0;
	*(unsigned int *)(self + 0x2975154) = 0;
	*(unsigned int *)(self + 0x2975158) = 0;
	*(unsigned int *)(self + 0x297515c) = 1;
	*(unsigned int *)(self + 0x2975160) = 0;
	*(unsigned int *)(self + 0x2975164) = 0;
	self[0x2975168] = 0;
	*(unsigned int *)(self + 0x297516c) = 0;
	*(unsigned int *)(self + 0x2975170) = 0;
	*(unsigned int *)(self + 0x2975174) = 0;
	*(unsigned int *)(self + 0x2975178) = 1;
	*(unsigned int *)(self + 0x297517c) = 0;
	*(unsigned int *)(self + 0x2975180) = 0;
	self[0x2975184] = 0;
	self[0x2975185] = 0;

	*(unsigned int *)(self + 0x29c9fa0) = 0;
	*(unsigned int *)(self + 0x29c9fa4) = 0;

	self[0x6d4] &= 0xc0;
	self[0x6d5] = 1;
	self[0x6d6] = 1;
	self[0x6d7] = 1;
	self[0x6d8] = 1;
	self[0x6d9] = 1;
	self[0x6ac] = 0;
	self[0x6da] = 0;
	self[0x29cc0c8] = 0;

	/*
	 * Confirmed real default-value table: alternating 0x10/0xff bytes,
	 * 8 sub-slots per group, 4 groups at a fixed +0x400 stride between
	 * "low"/"high" halves, 0x80 (128) outer iterations at a further
	 * +0x8/+0x9 stride between the two halves respectively -- exact
	 * shape transcribed from the disassembly's own confirmed offsets
	 * rather than algebraically generalized, since the 4th group's own
	 * stride (+0x29cb7c8/+0x29cbc48) is NOT a clean continuation of the
	 * first 3 (which use +0x29c9fc8/+0x29ca3c8, +0x29ca7c8/+0x29cabc8,
	 * +0x29cafc8/+0x29cb3c8 -- each pair exactly 0x400 apart, but the
	 * 4th pair is 0x480 apart) -- a real, confirmed irregularity, not
	 * an error in this transcription.
	 */
	for (unsigned int i = 0; i < 0x80; i++) {
		unsigned char *lo = self + i * 8;
		unsigned char *hi = self + i * 9;
		for (int k = 0; k < 8; k++) {
			lo[0x29c9fc8 + k] = 0x10;
			lo[0x29ca3c8 + k] = 0xff;
			lo[0x29ca7c8 + k] = 0x10;
			lo[0x29cabc8 + k] = 0xff;
			lo[0x29cafc8 + k] = 0x10;
			lo[0x29cb3c8 + k] = 0xff;
			hi[0x29cb7c8 + k] = 0x10;
			hi[0x29cbc48 + k] = 0xff;
		}
	}

	self[0x29cc0c9] = (self[0x29cc0c9] | 1) & ~2;
	self[0x6dc] = 0;
	self[0x6dd] = 0;
	*(unsigned int *)(self + 0x6e0) = 0;
	self[0x6ba] = 0;
	self[0x680] = 0;

	for (unsigned int i = 0; i < 8; i++) {
		self[0x29cc0da + i] = 0x10;
		self[0x29cc0da + i + 8] = 0xff;
	}
	for (unsigned int i = 0; i < 8; i++) {
		self[0x29cc0ea + i] = 0x10;
		self[0x29cc0ea + i + 8] = 0xff;
	}
	self[0x29cc0ca] = 0x10; self[0x29cc0cb] = 0xff;
	self[0x29cc0cc] = 0x10; self[0x29cc0cd] = 0xff;
	self[0x29cc0ce] = 0x10; self[0x29cc0cf] = 0xff;
	self[0x29cc0d0] = 0x10; self[0x29cc0d1] = 0xff;
	self[0x29cc0d2] = 0x10; self[0x29cc0d3] = 0xff;
	self[0x29cc0d4] = 0x10; self[0x29cc0d5] = 0xff;
	self[0x29cc0d6] = 0x10; self[0x29cc0d7] = 0xff;
	self[0x29cc0d8] = 0x10; self[0x29cc0d9] = 0xff;
	self[0x29cc0fa] = 0x10; self[0x29cc0fb] = 0xff;
	self[0x29cc0fc] = 0x10; self[0x29cc0fd] = 0xff;
	self[0x29cc0fe] = 0x10; self[0x29cc0ff] = 0xff;
	self[0x29cc100] = 0x10; self[0x29cc101] = 0xff;
	self[0x29cc102] = 0x10; self[0x29cc103] = 0xff;
	self[0x29cc104] = 0x10; self[0x29cc105] = 0xff;
	self[0x29cc106] = 0x10; self[0x29cc107] = 0xff;
	self[0x29cc108] = 0x10; self[0x29cc109] = 0xff;
	self[0x29cc10a] = 0x10; self[0x29cc10b] = 0xff;
	self[0x29cc10c] = 0x10; self[0x29cc10d] = 0xff;
	self[0x29cc10e] = 0x10; self[0x29cc10f] = 0xff;
	self[0x29cc110] = 0x10; self[0x29cc111] = 0xff;
	self[0x29cc112] = 0x10; self[0x29cc113] = 0xff;
	self[0x29cc114] = 0x10; self[0x29cc115] = 0xff;
	self[0x29cc116] = 0x10; self[0x29cc117] = 0xff;

	/* Confirmed real: this is the SAME array UpdateSongPunchMIDIChannel
	 * and its 22 confirmed siblings (sec 10.33, global.cpp) read/write
	 * at "+0x29cc11d + selector*8" -- this constructor zeroes the
	 * WHOLE 8-byte-stride array (120 confirmed entries) at its OWN
	 * base offset one byte earlier (+0x29cc11c), matching the same
	 * shared per-logical-channel table those 23 handlers already
	 * confirmed. */
	for (unsigned int i = 0; i < 0x78; i++)
		self[0x29cc11c + i * 8] = 0;

	*(unsigned int *)(self + 0x29cc4dc) = 0;
	*(unsigned int *)(self + 0x29cc4e0) = 0;
	*(unsigned short *)(self + 0x29cc4e4) = 0;
	self[0x6db] = 1;
	self[0x29cc4e6] = 0;
	self[0x29cc4e7] = 0;
	self[0x29cc4e8] = 0;
}
