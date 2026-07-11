// SPDX-License-Identifier: GPL-2.0
/*
 * hdr_sampler_commands.cpp  -  CSTGHDRManager::ProcessSamplerCommands()
 * and CSTGSampler::StandbyDisk()/StandbyRAM()/Start(bool)/Stop() (batch
 * 50).
 *
 * Deliberately a SEPARATE translation unit from managers.cpp/
 * hdr_record_track.cpp, matching the established CSTGStreamingEventManager/
 * CSTGRecordTrack precedent (sec 10.145/10.162): keeps the five pre-existing
 * managers.cpp-linking verify/ binaries (test_managers/test_engine/
 * test_global/test_global_ctor/test_engine_startup_bits2) untouched by this
 * batch's new symbols.
 *
 * `CSTGHDRManager::ProcessSamplerCommands()` (`.text+0xd5c50`, 380 bytes)
 * confirmed: a ring-buffer consumer loop, same overall shape as
 * `ProcessRecordCommands()`/`CSTGCDWorker::ProcessCommands()` (sec
 * 10.158/10.162), but over its OWN THIRD ring (the "sampler" ring --
 * `CSTGHDRManager`'s own ctor comment in oa_engine.h already flagged three
 * `CSTGBankMemory::AllocAligned()`-backed rings at `+0x18ad8`/`+0x18ae8`/
 * `+0x18af8`; `+0x18ae8` is `ProcessRecordCommands()`'s own ring, so
 * `+0x18af8` -- confirmed directly by THIS function's own disassembly, not
 * by elimination -- is this one):
 *   +0x18af8 ring base pointer (packed 32-bit)
 *   +0x18afc producer index (never written here)
 *   +0x18b00 consumer index (advanced here)
 *   +0x18b04 capacity (modulus for wraparound)
 * Each entry is 44 bytes (0x2c), confirmed via `imul eax,edx,0x2c`. Every
 * field from `+0x04`..`+0x28` is loaded UNCONDITIONALLY before the tag
 * branch (matching the real disassembly's own shared prologue, faithfully
 * reproduced rather than loading only the fields each branch needs) --
 * `+0x04` and `+0x08`/`+0x0c` are a real, confirmed field OVERLAP (a
 * `const char *` name for tag 0 vs. two `short *` RAM buffer pointers for
 * tag 1, never both meaningful in the same entry), while `+0x14`..`+0x28`
 * are shared across both:
 *   +0x00 tag (only the low byte read)
 *   +0x04 nameOrUnused   (tag 0: `const char *` sample name)
 *   +0x08 ramBufAOrUnused (tag 1: `short *`)
 *   +0x0c ramBufBOrUnused (tag 1: `short *`)
 *   +0x10 p2OrUnused     (tag 0: `unsigned int`)
 *   +0x14 p3             (`unsigned long`, both tags)
 *   +0x18 busId          (`eSTGBusID`, both tags)
 *   +0x1c busType        (`eSTGBusType`, both tags)
 *   +0x20 mode           (`eSTGSamplingInterfaceMode`, both tags)
 *   +0x24 p7             (`unsigned long`, both tags)
 *   +0x28 p8             (`unsigned char`, both tags)
 * Dispatch on `tag` (this=`&this->sampler`, i.e. `this+0x1190`, the
 * embedded `CSTGSampler` -- see oa_engine.h's own `CSTGHDRManager` class
 * comment for that offset's own confirmation):
 *   tag==0: `sampler->StandbyDisk((const char*)entry[0x04], entry[0x10],
 *            entry[0x14], entry[0x18], entry[0x1c], entry[0x20],
 *            entry[0x24], entry[0x28])`
 *   tag==1: `sampler->StandbyRAM((short*)entry[0x08], (short*)entry[0x0c],
 *            entry[0x14], entry[0x18], entry[0x1c], entry[0x20],
 *            entry[0x24], entry[0x28])`
 *   tag==2: `sampler->Start(true)` -- the `bool` argument is a real literal
 *            `1` in the disassembly, NOT read from the ring entry.
 *   tag==3: `sampler->Stop()` -- no arguments beyond the receiver.
 *   any other tag: no-op (entry silently consumed, consumer index still
 *   advances) -- matches the established "unhandled tag" convention already
 *   confirmed for every other ring-buffer consumer in this codebase.
 *
 * ALL FOUR call targets confirmed via direct `.rel.text` relocation
 * resolution against ground truth (not inferred from signature shape
 * alone): `.text+0xd5d26` -> `CSTGSampler::StandbyDisk(...)`
 * (`.text+0xd7ba0`), `.text+0xd5d6f` -> `CSTGSampler::Stop()`
 * (`.text+0xd81d0`), `.text+0xd5d82` -> `CSTGSampler::Start(bool)`
 * (`.text+0xd8690`), `.text+0xd5dc3` -> `CSTGSampler::StandbyRAM(...)`
 * (`.text+0xd7a90`) -- an exact match for the tag-order derivation above.
 *
 * CSTGSampler::StandbyDisk()/StandbyRAM()/Start(bool)/Stop() themselves are
 * confirmed real (527/272/143/340 bytes respectively) but are genuine
 * disk/RAM sampling-hardware standby setup and transport start/stop control
 * on the still-unreconstructed rest of this enormous (~101KB+) class --
 * audio-DSP/hardware-adjacent, out of scope per the sec 10.185 policy.
 * Deliberately deferred no-op bodies, given their own dedicated bodies here
 * (rather than in bar2_stubs.cpp, which is never linked into any verify/
 * binary) purely so this file links standalone -- the exact same treatment
 * already established for `CSTGRecordTrack::StandbyRec()` (sec 10.162).
 */

#include "oa_global.h"
#include "oa_engine.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

void CSTGHDRManager::ProcessSamplerCommands()
{
	unsigned char *base = (unsigned char *)this;
	CSTGSampler *sampler = (CSTGSampler *)(base + 0x1190);

	unsigned int consumerIdx = *(unsigned int *)(base + 0x18b00);
	unsigned int producerIdx = *(unsigned int *)(base + 0x18afc);

	while (consumerIdx != producerIdx) {
		unsigned char *entry = FromU32(*(unsigned int *)(base + 0x18af8)) + consumerIdx * 0x2c;

		unsigned char tag = *(unsigned char *)(entry + 0x00);
		unsigned int  f04 = *(unsigned int *)(entry + 0x04);
		unsigned int  f08 = *(unsigned int *)(entry + 0x08);
		unsigned int  f0c = *(unsigned int *)(entry + 0x0c);
		unsigned int  f10 = *(unsigned int *)(entry + 0x10);
		unsigned int  p3  = *(unsigned int *)(entry + 0x14);
		unsigned int  busId   = *(unsigned int *)(entry + 0x18);
		unsigned int  busType = *(unsigned int *)(entry + 0x1c);
		unsigned int  mode    = *(unsigned int *)(entry + 0x20);
		unsigned int  p7  = *(unsigned int *)(entry + 0x24);
		unsigned char p8  = *(unsigned char *)(entry + 0x28);

		unsigned int nextIdx = (consumerIdx + 1) % *(unsigned int *)(base + 0x18b04);
		*(unsigned int *)(base + 0x18b00) = nextIdx;

		if (tag == 0) {
			sampler->StandbyDisk((const char *)FromU32(f04), f10, p3,
					      (int)busId, (int)busType, (int)mode, p7, p8);
		} else if (tag == 1) {
			sampler->StandbyRAM((short *)FromU32(f08), (short *)FromU32(f0c), p3,
					     (int)busId, (int)busType, (int)mode, p7, p8);
		} else if (tag == 2) {
			sampler->Start(true);
		} else if (tag == 3) {
			sampler->Stop();
		}

		consumerIdx = *(unsigned int *)(base + 0x18b00);
		producerIdx = *(unsigned int *)(base + 0x18afc);
	}
}

void CSTGSampler::StandbyDisk(const char *, unsigned int, unsigned long,
			       int, int, int, unsigned long, unsigned char)
{
}

void CSTGSampler::StandbyRAM(short *, short *, unsigned long,
			      int, int, int, unsigned long, unsigned char)
{
}

void CSTGSampler::Start(bool)
{
}

void CSTGSampler::Stop()
{
}
