// SPDX-License-Identifier: GPL-2.0
/*
 * test_hdr_manager_init.cpp  -  host-side known-answer test for
 * CSTGPlaybackBuffer::Initialize() (both overloads),
 * CSTGSampler::Initialize(), CSTGCDAudioPlay::Initialize(), and
 * CSTGHDRManager::Initialize() (batch 22).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_setup_global_resources.h"	/* pulls in oa_global.h/oa_engine.h/
					 * oa_bank_memory.h together, plus
					 * STGAPIFrontPanelStatus */

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* Local storage for statics referenced by the files this test links
 * (hdr_record_track.cpp/audio_input_mixer.cpp) but never actually
 * exercises the call paths that would need them populated. */
CSTGFileCloser *CSTGFileCloser::sInstance;
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
unsigned char CSTGPerformanceVarsManager::sInstance[12];

/*
 * CSTGHDRCircularBuffer::Initialize() itself lives in managers.cpp
 * (sec 10.158, already independently verified there) -- linking the
 * WHOLE of managers.cpp here would drag in CSTGHDRManager's own giant
 * ctor and a dozen unrelated not-yet-reconstructed dependencies
 * (CSTGVoiceAllocator::FreeVoice/DoPendingMoveVoices,
 * CSTGHDRManager::ProcessPlaybackCommands/ProcessSamplerCommands,
 * CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors,
 * vtable for CSTGAudioManager, several rtwrap_* mocks, etc) that have
 * nothing to do with what THIS test exercises. A local, faithful stand-in
 * (matching this class's own already-documented real behavior for the
 * two fields this test actually checks -- `+0x04`=flag,
 * `+0x18`=totalSize verbatim, oa_engine.h's own class comment) avoids
 * that, matching this project's established "own local mock when the
 * real owning file is too heavy to link" precedent. */
void CSTGHDRCircularBuffer::Initialize(unsigned long totalSize, bool flag, unsigned char)
{
	this->totalSize = totalSize;
	this->flagByte = (unsigned char)flag;
}
unsigned char *STGAPIFrontPanelStatus::sInstance;

int main(void)
{
	printf("CSTGPlaybackBuffer/CSTGSampler/CSTGCDAudioPlay/CSTGHDRManager::Initialize() KAT\n");
	printf("=========================================================\n");

	unsigned char *pool = mmap32(0x200000);
	CSTGBankMemory::Initialize(pool, 0x200000);

	printf("[1] CSTGPlaybackBuffer::Initialize(unsigned long) -- 1-arg overload\n");
	{
		unsigned char *buf = mmap32(0x1000);
		memset(buf, 0xcc, 0x1000);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)buf;
		pb->Initialize(0x190000ul);

		CSTGHDRCircularBuffer *cb = (CSTGHDRCircularBuffer *)buf;
		check_eq("embedded circbuf totalSize == 0x190000", cb->totalSize, 0x190000);
		check_eq("embedded circbuf flagByte == 1 (true)", cb->flagByte, 1);
		check_eq("+0x40 == 0xfa1", *(unsigned int *)(buf + 0x40), 0xfa1);
		check_eq("+0x34 (alloc'd buffer) non-null", *(unsigned int *)(buf + 0x34) != 0, 1);
	}

	printf("[2] CSTGPlaybackBuffer::Initialize(unsigned char, unsigned long) -- 2-arg overload\n");
	{
		unsigned char *buf = mmap32(0x1000);
		memset(buf, 0xcc, 0x1000);
		CSTGPlaybackBuffer *pb = (CSTGPlaybackBuffer *)buf;
		pb->Initialize((unsigned char)3, 0x30000ul);

		check_eq("+0x0 stomped to mode byte (3)", *(unsigned int *)(buf + 0x0), 3);
		check_eq("+0x40 == 0xfa1", *(unsigned int *)(buf + 0x40), 0xfa1);
		check_eq("+0x34 (alloc'd buffer) non-null", *(unsigned int *)(buf + 0x34) != 0, 1);

		CSTGHDRCircularBuffer *cb = (CSTGHDRCircularBuffer *)buf;
		check_eq("embedded circbuf totalSize == 0x30000", cb->totalSize, 0x30000);
	}

	printf("[3] CSTGSampler::Initialize()\n");
	{
		unsigned char *buf = mmap32(0x20000);
		memset(buf, 0xcc, 0x20000);
		CSTGSampler *s = (CSTGSampler *)buf;
		s->Initialize();

		check_eq("+0x177c0 == 1.0f", *(unsigned int *)(buf + 0x177c0), 0x3f800000);
		check_eq("+0x177a4 == 1.0f", *(unsigned int *)(buf + 0x177a4), 0x3f800000);
		check_eq("+0x17704 == 1", *(unsigned int *)(buf + 0x17704), 1);
		check_eq("+0x177d8 == 2", buf[0x177d8], 2);
		check_eq("+0x177c6 == 1", buf[0x177c6], 1);
		check_eq("+0x17720 ring capacity == 0x61", *(unsigned int *)(buf + 0x17720), 0x61);
		check_eq("+0x17714 ring buffer non-null", *(unsigned int *)(buf + 0x17714) != 0, 1);
	}

	printf("[4] CSTGCDAudioPlay::Initialize()\n");
	{
		unsigned char *buf = mmap32(0x1000);
		memset(buf, 0xcc, 0x1000);
		CSTGCDAudioPlay *cd = (CSTGCDAudioPlay *)buf;
		cd->Initialize();

		CSTGHDRCircularBuffer *cb = (CSTGHDRCircularBuffer *)buf;
		check_eq("embedded circbuf totalSize == 0x24c00", cb->totalSize, 0x24c00);
		check_eq("embedded circbuf flagByte == 0 (false)", cb->flagByte, 0);
		check_eq("+0x30 == 0", buf[0x30], 0);
		check_eq("+0x54 == 0xac44 (sample rate)", *(unsigned int *)(buf + 0x54), 0xac44);
		check_eq("+0x9c (meter buffer) non-null", *(unsigned int *)(buf + 0x9c) != 0, 1);

		CSTGAudioInputMixerBase *mixer = (CSTGAudioInputMixerBase *)(buf + 0xa0);
		check_eq("embedded mixer numBuses == 2", buf[0xa0 + 0x4], 2);
		unsigned char *mixerState = (unsigned char *)(unsigned long)mixer->mixerStateArray32;
		unsigned int meterBuf = *(unsigned int *)(buf + 0x9c);
		check_eq("mixerState[0]+0x60 == meterBuf",
			 *(unsigned int *)(mixerState + 0x60), meterBuf);
		check_eq("mixerState[1]+0x60 == meterBuf+0x80",
			 *(unsigned int *)(mixerState + 0x90 + 0x60), meterBuf + 0x80);

		check_eq("+0xb0 == 0x19", buf[0xb0], 0x19);
		check_eq("+0xb1 == 0x19", buf[0xb1], 0x19);
		check_eq("+0xb2 == 0", buf[0xb2], 0);
	}

	printf("[5] CSTGHDRManager::Initialize() -- full orchestration, all 16 pairs\n");
	{
		/* Real confirmed minimum object size ~0x18b04 -- round up generously. */
		unsigned char *buf = mmap32(0x20000);
		memset(buf, 0xcc, 0x20000);

		unsigned char *panelBuf = mmap32(0x2000);
		memset(panelBuf, 0xcc, 0x2000);
		STGAPIFrontPanelStatus::sInstance = panelBuf;

		/* Every one of the 16 CSTGRecordTrack's own `+0x24` fields is
		 * EXACTLY the embedded CSTGMonitorMixerChannel's own `+0x4`
		 * field (`recordTrack[i]+0x20+0x4` == `recordTrack[i]+0x24`,
		 * see CSTGRecordTrack::Initialize()'s own header comment in
		 * hdr_record_track.cpp) -- ONE shared buffer per track, not
		 * two independent ones. This test exercises `Initialize()`
		 * directly on a raw poisoned buffer (no ctor call), so it
		 * needs an explicit valid backing buffer here regardless of
		 * the ctor's own real behavior (batch 23; see managers.cpp). */
		unsigned char *mmcTargets[16];
		for (int i = 0; i < 16; i++) {
			mmcTargets[i] = mmap32(0x1000);
			memset(mmcTargets[i], 0xcc, 0x1000);
			unsigned char *rt = buf + 0x584 + i * 0xc0;
			*(unsigned int *)(rt + 0x24) = ToU32(mmcTargets[i]);
		}

		CSTGHDRManager *hdr = (CSTGHDRManager *)buf;
		hdr->Initialize();

		check_eq("panel+0x10ec == 0x190000", *(unsigned int *)(panelBuf + 0x10ec), 0x190000);

		printf("  -- pair 0 --\n");
		unsigned char *pb0 = buf + 0x4;
		check_eq("pb[0]+0x0 mode byte == 0", *(unsigned int *)(pb0 + 0x0), 0);
		check_eq("pb[0]+0x50 == 0", pb0[0x50], 0);
		check_eq("pb[0]+0x54 == &sGlobalBusSet[12*0x80]",
			 *(unsigned int *)(pb0 + 0x54),
			 ToU32(CSTGAudioBusManager::sGlobalBusSet + 12 * 0x80));
		unsigned char *rt0 = buf + 0x584;
		check_eq("rt[0]+0x0 trackIdx == 0", *(unsigned short *)(rt0 + 0x0), 0);
		check_eq("rt[0] embedded MonitorMixerChannel +0x0 == 0", rt0[0x20], 0);

		printf("  -- pair 15 (last) --\n");
		unsigned char *pb15 = buf + 0x4 + 15 * 0x58;
		check_eq("pb[15]+0x0 mode byte == 15", *(unsigned int *)(pb15 + 0x0), 15);
		check_eq("pb[15]+0x54 == &sGlobalBusSet[27*0x80]",
			 *(unsigned int *)(pb15 + 0x54),
			 ToU32(CSTGAudioBusManager::sGlobalBusSet + 27 * 0x80));
		unsigned char *rt15 = buf + 0x584 + 15 * 0xc0;
		check_eq("rt[15]+0x0 trackIdx == 15", *(unsigned short *)(rt15 + 0x0), 15);
		check_eq("mmcTargets[15]+0x60 == &sGlobalBusSet[15*0x80]",
			 *(unsigned int *)(mmcTargets[15] + 0x60),
			 ToU32(CSTGAudioBusManager::sGlobalBusSet + 15 * 0x80));

		printf("  -- tail section --\n");
		unsigned char *masterPb = buf + 0x18970;
		CSTGHDRCircularBuffer *masterCb = (CSTGHDRCircularBuffer *)masterPb;
		check_eq("master playback buf totalSize == 0x190000", masterCb->totalSize, 0x190000);
		check_eq("+0x189c0 == 0", buf[0x189c0], 0);
		check_eq("+0x189c2 == 1", buf[0x189c2], 1);

		unsigned char *sampler = buf + 0x1190;
		check_eq("sampler +0x17720 ring capacity == 0x61",
			 *(unsigned int *)(sampler + 0x17720), 0x61);

		unsigned char *cdAudio = buf + 0x189c8;
		check_eq("CD audio +0x54 == 0xac44", *(unsigned int *)(cdAudio + 0x54), 0xac44);

		check_eq("+0x18ae4 == 0xfa1", *(unsigned int *)(buf + 0x18ae4), 0xfa1);
		check_eq("+0x18ad8 ring alloc non-null", *(unsigned int *)(buf + 0x18ad8) != 0, 1);
		check_eq("+0x18af4 == 0xc9", *(unsigned int *)(buf + 0x18af4), 0xc9);
		check_eq("+0x18ae8 ring alloc non-null", *(unsigned int *)(buf + 0x18ae8) != 0, 1);
		check_eq("+0x18b04 == 0xb", *(unsigned int *)(buf + 0x18b04), 0xb);
		check_eq("+0x18af8 ring alloc non-null", *(unsigned int *)(buf + 0x18af8) != 0, 1);
		check_eq("+0x18a90 == 0", *(unsigned int *)(buf + 0x18a90), 0);
		check_eq("+0x18a8c == 0", buf[0x18a8c], 0);
		check_eq("+0x18a95 == 0", buf[0x18a95], 0);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
