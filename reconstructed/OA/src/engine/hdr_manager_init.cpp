// SPDX-License-Identifier: GPL-2.0
/*
 * hdr_manager_init.cpp  -  CSTGPlaybackBuffer::Initialize() (both
 * overloads), CSTGSampler::Initialize(), CSTGCDAudioPlay::Initialize(),
 * and CSTGHDRManager::Initialize() (batch 22).
 *
 * Deliberately a separate translation unit from managers.cpp (matching
 * the CSTGRecordTrack/CSTGStreamingEventManager "give it its own
 * dedicated TU" precedent, sec 10.145/10.162): managers.cpp already
 * owns CSTGHDRManager's own ctor and is linked directly by five
 * separate verify/ binaries; none of them mock `CSTGHDRManager::
 * Initialize()` or either `CSTGPlaybackBuffer::Initialize()` overload,
 * so this new file (with its own dedicated verify/test_hdr_manager_init.cpp)
 * avoids touching any of them.
 *
 * CSTGPlaybackBuffer::Initialize(unsigned long) (`.text+0xd64a0`, 64
 * bytes) confirmed: `((CSTGHDRCircularBuffer*)this)->Initialize(
 * totalSize, true, 0)` (this class's own `CSTGHDRCircularBuffer`
 * embeds at offset 0 -- confirmed via `eax` still holding this
 * function's own unmodified `this` at the sub-call site), then
 * `+0x40 = 0xfa1`, `+0x34 = CSTGBankMemory::AllocAligned(0x1f42, 0x10)`.
 *
 * CSTGPlaybackBuffer::Initialize(unsigned char, unsigned long)
 * (`.text+0xd64e0`, 86 bytes) confirmed: identical shape, but ALSO
 * (after the embedded circular-buffer call) overwrites `+0x0` with the
 * `unsigned char` mode argument -- a real, confirmed instruction-order
 * quirk: the embedded `CSTGHDRCircularBuffer::Initialize()` call itself
 * already sets its own `field00` to 0, and THIS function's caller-side
 * write happens strictly after, stomping it with the mode byte instead.
 */

#include "oa_setup_global_resources.h"	/* pulls in oa_global.h/oa_engine.h/
					 * oa_bank_memory.h together, plus
					 * STGAPIFrontPanelStatus */

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

void CSTGPlaybackBuffer::Initialize(unsigned long totalSize)
{
	unsigned char *base = (unsigned char *)this;

	((CSTGHDRCircularBuffer *)this)->Initialize(totalSize, true, 0);

	*(unsigned int *)(base + 0x40) = 0xfa1;
	*(unsigned int *)(base + 0x34) = ToU32(CSTGBankMemory::AllocAligned(0x1f42, 0x10));
}

void CSTGPlaybackBuffer::Initialize(unsigned char mode, unsigned long totalSize)
{
	unsigned char *base = (unsigned char *)this;

	((CSTGHDRCircularBuffer *)this)->Initialize(totalSize, true, 0);
	*(unsigned int *)(base + 0x0) = (unsigned char)mode;	/* stomps field00, faithfully */

	*(unsigned int *)(base + 0x40) = 0xfa1;
	*(unsigned int *)(base + 0x34) = ToU32(CSTGBankMemory::AllocAligned(0x1f42, 0x10));
}

/*
 * CSTGSampler::Initialize() (`.text+0xd78d0`, 247 bytes) -- see the
 * class's own declaration comment in oa_engine.h. Fully self-contained
 * (single external call: CSTGBankMemory::AllocAligned), despite
 * belonging to a much larger, out-of-scope class.
 */
void CSTGSampler::Initialize()
{
	unsigned char *base = (unsigned char *)this;

	*(float *)(base + 0x177c0) = 1.0f;
	*(float *)(base + 0x177a4) = 1.0f;
	*(unsigned int *)(base + 0x17710) = 0;
	*(unsigned int *)(base + 0x17724) = 0;
	*(unsigned int *)(base + 0x17700) = 0;
	*(unsigned int *)(base + 0x17704) = 1;
	*(unsigned short *)(base + 0x17708) = 0;
	base[0x1770a] = 0;
	base[0x1770b] = 0;
	base[0x1770c] = 0;
	*(unsigned short *)(base + 0x1770e) = 0;
	*(unsigned int *)(base + 0x17728) = 0;
	*(unsigned int *)(base + 0x1772c) = 0;
	*(unsigned int *)(base + 0x17730) = 0;
	*(unsigned int *)(base + 0x17734) = 0;
	*(unsigned int *)(base + 0x17738) = 0;
	*(unsigned int *)(base + 0x177d4) = 0;
	*(unsigned int *)(base + 0x177d0) = 0;
	base[0x177d8] = 2;
	base[0x177c4] = 0;
	base[0x177c5] = 0;
	base[0x177c6] = 1;
	*(unsigned int *)(base + 0x17720) = 0x61;

	*(unsigned int *)(base + 0x17714) = ToU32(CSTGBankMemory::AllocAligned(0x184, 0x10));
}

/*
 * CSTGCDAudioPlay::Initialize() (`.text+0xd3a20`, 159 bytes) -- see the
 * class's own declaration comment in oa_engine.h.
 */
void CSTGCDAudioPlay::Initialize()
{
	unsigned char *base = (unsigned char *)this;

	((CSTGHDRCircularBuffer *)this)->Initialize(0x24c00, false, 0);

	base[0x30] = 0;
	*(unsigned int *)(base + 0x54) = 0xac44;

	unsigned char *meterBuf = CSTGBankMemory::AllocAligned(0x100, 0x10);
	*(unsigned int *)(base + 0x9c) = ToU32(meterBuf);

	CSTGAudioInputMixerBase *mixer = (CSTGAudioInputMixerBase *)(base + 0xa0);
	mixer->Initialize(2);

	unsigned char *mixerState = FromU32(mixer->mixerStateArray32);
	*(unsigned int *)(mixerState + 0x60) = ToU32(meterBuf);
	*(unsigned int *)(mixerState + 0x90 + 0x60) = ToU32(meterBuf + 0x80);

	base[0xb0] = 0x19;
	base[0xb2] = 0;
	base[0xb4] = 0;
	base[0xb1] = 0x19;
	base[0xb3] = 0;
	base[0xb5] = 0;
}

/*
 * CSTGHDRManager::Initialize() (`.text+0xd41c0`, 1284 bytes) confirmed
 * -- see this method's own declaration comment in oa_engine.h for the
 * full picture (including the confirmed proof of the
 * CSTGRecordTrack/monitorMixerChannelSlots relationship).
 */
void CSTGHDRManager::Initialize()
{
	unsigned char *base = (unsigned char *)this;

	base[0x18a94] = 0;
	*(unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x10ec) = 0x190000;

	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *pb = base + 0x4 + i * 0x58;
		CSTGPlaybackBuffer *pbObj = (CSTGPlaybackBuffer *)pb;
		pbObj->Initialize((unsigned char)i, 0x30000);

		pb[0x50] = 0;
		pb[0x51] = 0;
		pb[0x52] = 0;
		*(unsigned int *)(pb + 0x54) =
			ToU32(CSTGAudioBusManager::sGlobalBusSet + (12 + i) * 0x80);

		unsigned char *rt = base + 0x584 + i * 0xc0;
		CSTGRecordTrack *rtObj = (CSTGRecordTrack *)rt;
		rtObj->Initialize((unsigned short)i);
	}

	CSTGPlaybackBuffer *masterPb = (CSTGPlaybackBuffer *)(base + 0x18970);
	masterPb->Initialize(0x190000ul);

	base[0x189c0] = 0;
	base[0x189c1] = 0;
	base[0x189c2] = 1;
	CSTGSampler *sampler = (CSTGSampler *)(base + 0x1190);
	sampler->Initialize();

	CSTGCDAudioPlay *cdAudio = (CSTGCDAudioPlay *)(base + 0x189c8);
	cdAudio->Initialize();

	*(unsigned int *)(base + 0x18ae4) = 0xfa1;
	*(unsigned int *)(base + 0x18ad8) = ToU32(CSTGBankMemory::AllocAligned(0xbb8c, 0x10));
	*(unsigned int *)(base + 0x18af4) = 0xc9;
	*(unsigned int *)(base + 0x18ae8) = ToU32(CSTGBankMemory::AllocAligned(0x15fc, 0x10));
	*(unsigned int *)(base + 0x18b04) = 0xb;
	*(unsigned int *)(base + 0x18af8) = ToU32(CSTGBankMemory::AllocAligned(0x1e4, 0x10));

	*(unsigned int *)(base + 0x18a90) = 0;
	base[0x18a8c] = 0;
	base[0x18a95] = 0;
}
