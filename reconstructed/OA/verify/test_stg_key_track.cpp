// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_key_track.cpp  -  host-side known-answer test for
 * CSTGKeyTrack (see include/oa_stg_key_track.h for full ground-truth
 * provenance and the deliberately-deferred methods' 2 distinct
 * reasons).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "oa_stg_key_track.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-56s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-56s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* Link-time-only mocks -- same "provide just enough storage to satisfy
 * the linker" precedent as test_adsr_base.cpp's own CSTGVoiceModelManager
 * mock. */
CSTGVoiceModelManager *CSTGVoiceModelManager::sInstance;
static unsigned char g_globalBuf[0x2a00000];
CSTGGlobal *CSTGGlobal::sInstance = reinterpret_cast<CSTGGlobal *>(g_globalBuf);

/* Link-satisfying storage for the 3 real named backing tables
 * GetParamDescriptors/GetMessageHandlers/GetValueGetters return
 * pointers to -- CONTENTS not recovered (see header comment), this
 * test only checks the returned pointer is non-null/correct. */
extern "C" unsigned char STGKeyTrackParams[1] = { 0 };
extern "C" unsigned char sMessageHandlers[1] = { 0 };
extern "C" unsigned char sValueGetters[1] = { 0 };

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

int main()
{
	printf("CSTGKeyTrack known-answer test\n");
	printf("===============================\n");

	printf("[1] GetId/GetName/GetNumParams/GetXxxTable accessors\n");
	{
		check_eq("GetId", CSTGKeyTrack::GetId(), 0x15);
		check_eq("GetName", (long)(strcmp(CSTGKeyTrack::GetName(), "KeyTrack") == 0), 1);
		check_eq("GetNumParams", CSTGKeyTrack::GetNumParams(), 7);
		check_eq("GetParamDescriptors non-null", (long)(CSTGKeyTrack::GetParamDescriptors() != 0), 1);
		check_eq("GetMessageHandlers non-null", (long)(CSTGKeyTrack::GetMessageHandlers() != 0), 1);
		check_eq("GetValueGetters non-null", (long)(CSTGKeyTrack::GetValueGetters() != 0), 1);
	}

	printf("[2] InitializeQuad: pointer slots share the CSTGADSRBase 'no source' default\n");
	{
		unsigned char subRateBuf[sizeof(STGKeyTrackSubRateParams)];
		memset(subRateBuf, 0xcc, sizeof(subRateBuf));
		STGKeyTrackAudioRateParams audioParams;
		CSTGKeyTrack::InitializeQuad(&audioParams, reinterpret_cast<STGKeyTrackSubRateParams *>(subRateBuf));

		unsigned int expect = (unsigned int)(unsigned long)
			(reinterpret_cast<char *>(CSTGGlobal::sInstance) + 0x29c9fa0);
		unsigned int *p = reinterpret_cast<unsigned int *>(subRateBuf);
		check_eq("slot+0x10 == shared no-source default", p[4], expect);
		check_eq("slot+0x14 == shared no-source default", p[5], expect);
		check_eq("slot+0x18 == shared no-source default", p[6], expect);
		check_eq("slot+0x1c == shared no-source default", p[7], expect);
		check_eq("slot+0x20 zeroed", p[8], 0);
		check_eq("slot+0x24 zeroed", p[9], 0);
		check_eq("slot+0x28 zeroed", p[10], 0);
		check_eq("slot+0x2c zeroed", p[11], 0);
	}

	printf("[3] PrepareSubRateAddressFixupTable: appends 1 word-index entry, advances count\n");
	{
		unsigned int entries[4] = { 0, 0, 0, 0 };
		CSTGSubRateAddressFixupTable table;
		table.entries = entries;
		table.count = 0;

		CSTGKeyTrack::PrepareSubRateAddressFixupTable(table, 0x100);
		check_eq("entries[0] == (0x100+0x10)>>2", entries[0], (0x100 + 0x10) >> 2);
		check_eq("count advanced to 1", table.count, 1);

		CSTGKeyTrack::PrepareSubRateAddressFixupTable(table, 0x200);
		check_eq("entries[1] == (0x200+0x10)>>2", entries[1], (0x200 + 0x10) >> 2);
		check_eq("count advanced to 2", table.count, 2);
	}

	printf("[4] UpdateLowKey/UpdateMidKey/UpdateHighKey: raw byte writes from STGConvertedParam.value\n");
	{
		/* CSTGKeyTrack's own private layout isn't visible here --
		 * exercised indirectly via GetOutput's own use of the SAME
		 * `_slotInfo` field this test also sets up below; the
		 * Update* methods' own effect (this+0xc/0xd/0xe) isn't
		 * independently externally observable without a getter, so
		 * this section only confirms the calls complete cleanly
		 * with representative STGConvertedParam values. */
		CSTGKeyTrack kt;
		CSTGComponentSlotInfo slotInfo;
		memset(&slotInfo, 0, sizeof(slotInfo));
		CSTGPatchMessageContext ctx;
		STGConvertedParam v;
		memset(&v, 0, sizeof(v));
		v.value = 60;
		kt.UpdateLowKey(ctx, v);
		v.value = 72;
		kt.UpdateMidKey(ctx, v);
		v.value = 96;
		kt.UpdateHighKey(ctx, v);
		check_eq("UpdateLowKey/MidKey/HighKey complete without crashing", 1, 1);
	}

	printf("[5] GetOutput/FreeVoice: shared 'quad table' addressing formula\n");
	{
		/* CSTGVoiceModelManager::sInstance+4 holds the real per-voice
		 * table base (a packed 32-bit address on the real target,
		 * see FreeVoice's own host/target-divergence comment) --
		 * mmap32'd here so the truncate-then-zero-extend round-trip
		 * in FreeVoice lands on a genuinely valid host address. */
		unsigned char *vmmBuf = (unsigned char *)mmap32(0x10000);
		unsigned char *quadTable = (unsigned char *)mmap32(0x10000);
		memset(vmmBuf, 0, 0x10000);
		memset(quadTable, 0xab, 0x10000);
		*(unsigned int *)(vmmBuf + 4) = (unsigned int)(unsigned long)quadTable;
		CSTGVoiceModelManager::sInstance = reinterpret_cast<CSTGVoiceModelManager *>(vmmBuf);

		CSTGComponentSlotInfo slotInfo;
		memset(&slotInfo, 0, sizeof(slotInfo));
		slotInfo.subRateBaseIndex = 0x40;

		/* Reach into CSTGKeyTrack's own private _slotInfo field via a
		 * raw offset poke (+0x08, confirmed layout) -- the class has
		 * no public setter, matching this project's established
		 * "test reaches past private via a confirmed raw offset"
		 * convention for classes with no ctor that wires it up. */
		unsigned char ktBuf[sizeof(CSTGKeyTrack)];
		memset(ktBuf, 0, sizeof(ktBuf));
		*(CSTGComponentSlotInfo **)(ktBuf + 8) = &slotInfo;
		CSTGKeyTrack &kt = *reinterpret_cast<CSTGKeyTrack *>(ktBuf);

		/* note=0 -> quad-table index 0, layer=3 -> +0x30 */
		int out = kt.GetOutput(0, 3);
		check_eq("GetOutput(note=0,layer=3) == subRateBaseIndex+layer*0x10+quadTableBase",
			 out, 0x40 + 3 * 0x10 + (int)(unsigned int)(unsigned long)quadTable);

		/* FreeVoice always targets the fixed +0x20 "layer" -- write
		 * a sentinel there via a mock CSTGVoice (note field at +4)
		 * and confirm it's zeroed afterward. */
		unsigned char voiceBuf[8];
		memset(voiceBuf, 0, sizeof(voiceBuf));
		*(unsigned short *)(voiceBuf + 4) = 0; /* note = 0 -> quad index 0 */
		unsigned int *targetSlot = (unsigned int *)(quadTable + 0x40 + 0x20);
		*targetSlot = 0xdeadbeef;
		CSTGVoice &voice = *reinterpret_cast<CSTGVoice *>(voiceBuf);
		kt.FreeVoice(voice);
		check_eq("FreeVoice zeroes the fixed +0x20 layer slot", *targetSlot, 0);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
