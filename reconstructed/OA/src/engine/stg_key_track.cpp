// SPDX-License-Identifier: GPL-2.0
/*
 * stg_key_track.cpp  -  CSTGKeyTrack method bodies (round 51, solo).
 * See include/oa_stg_key_track.h for the full object-layout derivation
 * and the deliberately-deferred methods' 2 distinct reasons.
 */
#include "oa_stg_key_track.h"

extern "C" unsigned char STGKeyTrackParams[];
extern "C" unsigned char sMessageHandlers[];
extern "C" unsigned char sValueGetters[];

CSTGKeyTrack::~CSTGKeyTrack()
{
	/* Real dtor (both D2/D0 variants, identical): resets vptr to
	 * &PTR__CSTGParamsOwner_006c04a8 -- see header comment. No
	 * free()/HAL_DisableInterrupts() call in EITHER variant, a
	 * confirmed real quirk (unlike most dtors in this project, e.g.
	 * Eva's CSysEx* family) -- preserved faithfully. */
	mVptrPlaceholder[0] = mVptrPlaceholder[1] = mVptrPlaceholder[2] = mVptrPlaceholder[3] = 0;
}

int CSTGKeyTrack::GetOutput(int note, int layer) const
{
	/* CSTGVoiceModelManager::sInstance is a typed `CSTGVoiceModelManager*`
	 * -- cast to `unsigned char*` before offset arithmetic (established
	 * convention, e.g. managers.cpp's own `(unsigned char*)
	 * CSTGVoiceModelManager::sInstance`), NOT raw pointer-typed `+4`
	 * (which would scale by sizeof(CSTGVoiceModelManager)). */
	unsigned char *vmm = (unsigned char *)CSTGVoiceModelManager::sInstance;
	return (int)_slotInfo->subRateBaseIndex + *(int *)(vmm + 4) +
	       (int)(((unsigned int)note & 3) + (((unsigned int)note & 0xffff) >> 2) * 0xcc0) * 4 +
	       layer * 0x10;
}

void CSTGKeyTrack::FreeVoice(CSTGVoice &voice)
{
	unsigned short note = *(unsigned short *)((unsigned char *)&voice + 4);
	unsigned char *vmm = (unsigned char *)CSTGVoiceModelManager::sInstance;
	/* The real target's own pointer width is 4 bytes; the sum below is
	 * computed and truncated to `unsigned int` to match exactly (same
	 * wraparound behavior as the real 32-bit target), then
	 * zero-extended to a real host pointer -- this project's
	 * established ToU32/FromU32 host/target-divergence convention
	 * (e.g. file_io.cpp), safe as long as the mock voice-model table
	 * this writes into is allocated within the first 4GB of host
	 * address space (mmap32-style, matching every other test that
	 * exercises this same "quad table" addressing formula). */
	unsigned int off = (unsigned int)_slotInfo->subRateBaseIndex + 0x20 +
			    ((note & 3) + ((unsigned int)note >> 2) * 0xcc0) * 4 +
			    (unsigned int)(*(int *)(vmm + 4));
	*(unsigned int *)(unsigned long)off = 0;
}

void CSTGKeyTrack::UpdateLowKey(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	mLowKey = (signed char)newVal.value;
}

void CSTGKeyTrack::UpdateMidKey(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	mMidKey = (signed char)newVal.value;
}

void CSTGKeyTrack::UpdateHighKey(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	mHighKey = (signed char)newVal.value;
}

void CSTGKeyTrack::InitializeQuad(STGKeyTrackAudioRateParams *, STGKeyTrackSubRateParams *subRateParams)
{
	/* Shared "no AMS source" default address -- same
	 * `CSTGGlobal::sInstance + 0x29c9fa0` neighbourhood already
	 * established for CSTGADSRBase::InitializeQuad (adsr_base.cpp).
	 * Unlike that method's own host-side `void**` writes, this
	 * struct's pointer slots (+0x10..+0x1c, 4 bytes apart) sit
	 * DIRECTLY adjacent to its own int slots (+0x20..+0x2c, also
	 * 4 bytes apart) -- a native 8-byte pointer write here would
	 * overrun into the int block on this 64-bit host. Stored as a
	 * plain 4-byte truncated address instead (this project's
	 * established ToU32-style convention, e.g. file_io.cpp) --
	 * correct on the real 32-bit target either way, and safe here
	 * since nothing dereferences these slots as real pointers, only
	 * compares the stored 32-bit value. */
	unsigned int defaultAddr =
		(unsigned int)(unsigned long)(reinterpret_cast<char *>(CSTGGlobal::sInstance) + 0x29c9fa0);
	unsigned int *p = reinterpret_cast<unsigned int *>(subRateParams->_unrecovered);
	p[4] = defaultAddr; /* +0x10 */
	p[5] = defaultAddr; /* +0x14 */
	p[6] = defaultAddr; /* +0x18 */
	p[7] = defaultAddr; /* +0x1c */
	p[8] = 0;           /* +0x20 */
	p[9] = 0;           /* +0x24 */
	p[10] = 0;          /* +0x28 */
	p[11] = 0;          /* +0x2c */
}

void CSTGKeyTrack::PrepareSubRateAddressFixupTable(CSTGSubRateAddressFixupTable &table, unsigned long note)
{
	table.entries[table.count] = (unsigned int)((note + 0x10) >> 2);
	table.count++;
}

unsigned int CSTGKeyTrack::GetId() { return 0x15; }
const char *CSTGKeyTrack::GetName() { return "KeyTrack"; }
unsigned int CSTGKeyTrack::GetNumParams() { return 7; }
const void *CSTGKeyTrack::GetParamDescriptors() { return STGKeyTrackParams; }
const void *CSTGKeyTrack::GetMessageHandlers() { return sMessageHandlers; }
const void *CSTGKeyTrack::GetValueGetters() { return sValueGetters; }
