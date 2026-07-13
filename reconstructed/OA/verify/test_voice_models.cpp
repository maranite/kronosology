// SPDX-License-Identifier: GPL-2.0
/*
 * test_voice_models.cpp  -  host-side known-answer test for
 * src/engine/voice_models.cpp (batch 42): CSTGVoiceModel's real base
 * ctor, CSTGVoiceModelManager::Register(), and the ten derived Model
 * ctors' own real bodies + correctly-shaped vtables.
 *
 * Links src/engine/voice_models.cpp + src/mem/bank_memory.cpp directly.
 * `operator new[]` resolves to the HOST's own default global operator
 * (this test does NOT link src/mem/new_delete.cpp) -- same technique
 * already established by test_audio_input_mixer.cpp for the identical
 * `operator new[]` call shape; on the real target it instead resolves to
 * this project's own new_delete.cpp -> stg_kmalloc -> __kmalloc chain,
 * already linked as part of OA-objs.
 *
 * `CSTGAudioManager::sInstance` and `CSTGVoiceModelManager::sInstance`
 * are both faked as small raw buffers (only the specific offsets
 * `CSTGVoiceModel`'s own ctor/`Register()` actually touch are set up),
 * matching this project's established "poison a raw buffer, cast to the
 * real class pointer" convention for classes with large not-fully-typed
 * bodies -- avoids needing either class's own (much larger, unrelated)
 * real ctor for this focused unit test.
 */

#include <cstdio>
#include <cstring>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_bank_memory.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-58s %ld\n", label, got); return; }
	printf("  FAIL  %-58s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* The four DSP-callee thunks this test exercises directly (address
 * identity checks against each model's own real vtable slots). */
extern "C" void OA_VoiceModel_Off_Initialize(void *self);
extern "C" unsigned int OA_VoiceModel_Off_GetId(const void *self);
extern "C" void OA_VoiceModel_Off_ProcessSubRate(void *self, unsigned int tick);
extern "C" void OA_VoiceModel_Off_ProcessAudioRate(void *self, unsigned int tick);
/* Link-satisfying host mocks: the other nine models' own Initialize()/
 * ProcessSubRate()/ProcessAudioRate() no-op stubs live in bar2_stubs.cpp,
 * NOT linked here -- this test provides its own trivial stand-ins (same
 * shape, no observable difference) rather than pull in the whole stub
 * file, so voice_models.cpp's own vtable arrays (which take each one's
 * address) still link. */
extern "C" void OA_VoiceModel_PCM_Initialize(void *) {}
extern "C" void OA_VoiceModel_PCM_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_PCM_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_AnalogSync_Initialize(void *) {}
extern "C" void OA_VoiceModel_AnalogSync_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_AnalogSync_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Organ_Initialize(void *) {}
extern "C" void OA_VoiceModel_Organ_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Organ_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Plucked_Initialize(void *) {}
extern "C" void OA_VoiceModel_Plucked_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Plucked_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_MS20_Initialize(void *) {}
extern "C" void OA_VoiceModel_MS20_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_MS20_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Polysix_Initialize(void *) {}
extern "C" void OA_VoiceModel_Polysix_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Polysix_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_VPM_Initialize(void *) {}
extern "C" void OA_VoiceModel_VPM_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_VPM_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Piano_Initialize(void *) {}
extern "C" void OA_VoiceModel_Piano_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_Piano_ProcessAudioRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_EP_Initialize(void *) {}
extern "C" void OA_VoiceModel_EP_ProcessSubRate(void *, unsigned int) {}
extern "C" void OA_VoiceModel_EP_ProcessAudioRate(void *, unsigned int) {}

static unsigned char s_bankArena[1 << 20]; /* 1MB, plenty for a handful of models */
static unsigned char s_audioManagerFake[0x20];
static unsigned char s_voiceModelManagerFake[0x100];

/* This isolated test doesn't link managers.cpp (CSTGAudioManager/
 * CSTGVoiceModelManager's own real ctors) or process_oacmd.cpp
 * (CSTGPianoModel's own separate ecosystem) -- give each its own local
 * storage, same "isolated test needs its own local storage for an
 * extern it doesn't get for free" precedent as test_daemon_lifecycle.cpp. */
CSTGAudioManager *CSTGAudioManager::sInstance;
CSTGVoiceModelManager *CSTGVoiceModelManager::sInstance;
CSTGPianoModel *CSTGPianoModel::sInstance;

int main()
{
	printf("CSTGVoiceModel/CSTGVoiceModelManager::Register known-answer test\n");

	CSTGBankMemory::Initialize(s_bankArena, sizeof(s_bankArena));

	/* CSTGAudioManager::sInstance's own +0x18 field (channel count),
	 * read raw by CSTGVoiceModel's base ctor. */
	memset(s_audioManagerFake, 0xcc, sizeof(s_audioManagerFake));
	*(unsigned int *)(s_audioManagerFake + 0x18) = 3;
	CSTGAudioManager::sInstance = (CSTGAudioManager *)s_audioManagerFake;

	/* CSTGVoiceModelManager::sInstance -- zeroed so Register()'s own
	 * running count (+0x58) starts at a real, confirmed 0. */
	memset(s_voiceModelManagerFake, 0, sizeof(s_voiceModelManagerFake));
	CSTGVoiceModelManager::sInstance = (CSTGVoiceModelManager *)s_voiceModelManagerFake;

	printf("\n[1] CSTGOffModel::CSTGOffModel() -- base ctor + own flag byte\n");
	unsigned char *offStorage = CSTGBankMemory::AllocAligned(0x108, 0x10);
	CSTGOffModel *off = new (offStorage) CSTGOffModel();

	unsigned char *offBytes = (unsigned char *)off;
	check_eq("sInstance == this", (long)(CSTGOffModel::sInstance == off), 1);
	check_eq("+0xe1 == 0x3f (base 0, OR 0x3f)", offBytes[0xe1], 0x3f);
	check_eq("+0xe2 low 2 bits cleared, no extra OR for Off", offBytes[0xe2] & 0x3, 0);
	check_eq("+0x104 == 0", *(unsigned int *)(offBytes + 0x104), 0);
	check_eq("+0xd8 == 0xffff", *(unsigned short *)(offBytes + 0xd8), (unsigned short)0xffff);
	check_eq("+0xf0 == 0", *(unsigned short *)(offBytes + 0xf0), 0);
	check_eq("+0xf8 == 0", *(unsigned short *)(offBytes + 0xf8), 0);
	check_eq("+0xfc == 0", *(unsigned int *)(offBytes + 0xfc), 0);
	check_eq("+0xe0 == 0", offBytes[0xe0], 0);
	check_eq("+0xd4 (channel-record array) non-null", (long)(*(unsigned int *)(offBytes + 0xd4) != 0), 1);
	check_eq("+0xec (0xcc0 pool) non-null", (long)(*(unsigned int *)(offBytes + 0xec) != 0), 1);
	check_eq("+0xf4 (0x6a0 pool) non-null", (long)(*(unsigned int *)(offBytes + 0xf4) != 0), 1);
	check_eq("+0xe4 (0x1a80 pool) non-null", (long)(*(unsigned int *)(offBytes + 0xe4) != 0), 1);
	check_eq("+0xe8 (0x3300 pool) non-null", (long)(*(unsigned int *)(offBytes + 0xe8) != 0), 1);

	printf("\n[2] CallVtableSlot-equivalent dispatch through CSTGOffModel's real vtable\n");
	void **offVtbl = *(void ***)off;
	check_eq("slot 2 == OA_VoiceModel_Off_Initialize",
		 (long)(offVtbl[2] == (void *)&OA_VoiceModel_Off_Initialize), 1);
	check_eq("slot 18 == OA_VoiceModel_Off_ProcessSubRate",
		 (long)(offVtbl[18] == (void *)&OA_VoiceModel_Off_ProcessSubRate), 1);
	check_eq("slot 19 == OA_VoiceModel_Off_ProcessAudioRate",
		 (long)(offVtbl[19] == (void *)&OA_VoiceModel_Off_ProcessAudioRate), 1);
	check_eq("slot 0/1 (offset-to-top/RTTI) null", (long)(offVtbl[0] == 0 && offVtbl[1] == 0), 1);
	/* sec 10.234: real ABI slot 3 (GetId(), array index 5 / offVtbl[3])
	 * was null here until this batch -- CSTGKLMManager::AuthorizeBuiltins()
	 * (klm_manager.cpp) dispatches through exactly this slot for every
	 * loaded voice model, and this project's own vtable being null there
	 * live-boot-crashed (EIP=CR2=0). Now a real, ground-truth-confirmed
	 * accessor returning this model's own eSTGVoiceModelType ordinal. */
	check_eq("slot 3 (GetId) == OA_VoiceModel_Off_GetId",
		 (long)(offVtbl[3] == (void *)&OA_VoiceModel_Off_GetId), 1);
	/* sec 10.234 (second fix in the same pass): the SET_AUTH/RECOMPUTE
	 * slots (real ABI slots 13/14/15 = offVtbl[13]/[14]/[15]) were the
	 * SECOND live-boot NULL-call crash, one instruction further into
	 * AuthorizeBuiltins()'s stamp_object() helper once GetId's own crash
	 * was fixed -- these are shared CSTGVoiceModel base-class methods,
	 * identical across all ten models. */
	check_eq("slot 13 (GetAuthField) == OA_VoiceModel_GetAuthField",
		 (long)(offVtbl[13] == (void *)&OA_VoiceModel_GetAuthField), 1);
	check_eq("slot 14 (SetAuthField) == OA_VoiceModel_SetAuthField",
		 (long)(offVtbl[14] == (void *)&OA_VoiceModel_SetAuthField), 1);
	check_eq("slot 15 (SetProductId) == OA_VoiceModel_SetProductId",
		 (long)(offVtbl[15] == (void *)&OA_VoiceModel_SetProductId), 1);
	check_eq("slot 20 (never dispatched) null", (long)(offVtbl[20] == 0), 1);
	/* Dispatch for real, exactly as engine_init.cpp's CallVtableSlot/
	 * managers.cpp's ProcessSubRate/ProcessAudioRate would -- confirms no
	 * crash through a real (non-null) function pointer. */
	typedef void (*Slot2Fn)(void *);
	typedef unsigned int (*Slot3Fn)(const void *);
	typedef void (*SlotTickFn)(void *, unsigned int);
	((Slot2Fn)offVtbl[2])(off);
	check_eq("dispatched slot 3 (GetId) via vtable == 0 (eVoiceModel_Off)",
		 (long)((Slot3Fn)offVtbl[3])(off), 0);
	((SlotTickFn)offVtbl[18])(off, 42);
	((SlotTickFn)offVtbl[19])(off, 42);
	printf("  ok    dispatched slots 2/3/18/19 without crashing (Off model's real bodies)\n");

	/* Exercise slots 13/14/15 exactly the way klm_manager.cpp's
	 * stamp_object() does: RECOMPUTE(obj, 0) [-> SetProductId, clears
	 * +0x104], then SET_AUTH(obj, value) [-> SetAuthField, writes
	 * +0x100], then confirm GET_AUTH reads it back. */
	typedef void (*SlotWriteFn)(void *, unsigned int);
	typedef unsigned int (*SlotReadFn)(const void *);
	*(unsigned int *)(offBytes + 0x104) = 0xdeadbeef; /* poison before RECOMPUTE clears it */
	((SlotWriteFn)offVtbl[15])(off, 0); /* RECOMPUTE -> SetProductId(0) */
	check_eq("dispatched slot 15 (SetProductId) via vtable cleared +0x104",
		 *(unsigned int *)(offBytes + 0x104), 0);
	((SlotWriteFn)offVtbl[14])(off, 0x12345678); /* SET_AUTH -> SetAuthField(value) */
	check_eq("dispatched slot 14 (SetAuthField) via vtable wrote +0x100",
		 *(unsigned int *)(offBytes + 0x100), 0x12345678);
	check_eq("dispatched slot 13 (GetAuthField) via vtable reads +0x100 back",
		 (long)((SlotReadFn)offVtbl[13])(off), 0x12345678);

	/* +8/+0x30 are packed 32-bit (target-pointer-wide) slots, NOT native
	 * pointers -- compare against the truncated 32-bit representation,
	 * same ToU32 convention as the reconstruction itself (this project's
	 * established host/target pointer-width discipline; a native 64-bit
	 * re-read here would itself be the exact bug class this batch found
	 * and fixed in managers.cpp's ProcessSubRate/ProcessAudioRate). */
#define PACKED32(p) ((unsigned int)(unsigned long)(p))

	printf("\n[3] CSTGVoiceModelManager::Register() bookkeeping\n");
	check_eq("running count (+0x58) == 1 after one Register()",
		 *(unsigned short *)(s_voiceModelManagerFake + 0x58), 1);
	check_eq("append list [+0x30] entry 0 == off",
		 *(unsigned int *)(s_voiceModelManagerFake + 0x30), PACKED32(off));
	check_eq("direct type table [+8 + 0*4] == off (eVoiceModel_Off == 0)",
		 *(unsigned int *)(s_voiceModelManagerFake + 8), PACKED32(off));

	printf("\n[4] A second model (CSTGPCMModel) -- distinct flag byte + Register() append\n");
	unsigned char *pcmStorage = CSTGBankMemory::AllocAligned(0x108, 0x10);
	CSTGPCMModel *pcm = new (pcmStorage) CSTGPCMModel();
	unsigned char *pcmBytes = (unsigned char *)pcm;

	check_eq("PCM sInstance == this", (long)(CSTGPCMModel::sInstance == pcm), 1);
	check_eq("PCM +0xe1 == 0x7f", pcmBytes[0xe1], 0x7f);
	void **pcmVtbl = *(void ***)pcm;
	check_eq("PCM slot 2 == OA_VoiceModel_PCM_Initialize",
		 (long)(pcmVtbl[2] == (void *)&OA_VoiceModel_PCM_Initialize), 1);
	typedef unsigned int (*Slot3FnPcm)(const void *);
	check_eq("PCM slot 3 (GetId) dispatched via vtable == 1 (eVoiceModel_PCM)",
		 (long)((Slot3FnPcm)pcmVtbl[3])(pcm), 1);

	check_eq("running count (+0x58) == 2 after second Register()",
		 *(unsigned short *)(s_voiceModelManagerFake + 0x58), 2);
	check_eq("append list [+0x30] entry 1 == pcm",
		 *(unsigned int *)(s_voiceModelManagerFake + 0x30 + 4), PACKED32(pcm));
	check_eq("direct type table [+8 + 1*4] == pcm (eVoiceModel_PCM == 1)",
		 *(unsigned int *)(s_voiceModelManagerFake + 8 + 4), PACKED32(pcm));
	check_eq("append list entry 0 (off) untouched by the second Register()",
		 *(unsigned int *)(s_voiceModelManagerFake + 0x30), PACKED32(off));

	printf("\n[5] Per-model flag-byte spot checks (Organ: plain MOV; Plucked: read-mask-OR)\n");
	unsigned char *organStorage = CSTGBankMemory::AllocAligned(0x108, 0x10);
	CSTGOrganModel *organ = new (organStorage) CSTGOrganModel();
	check_eq("Organ +0xe1 == 0xc1 (plain MOV)", ((unsigned char *)organ)[0xe1], 0xc1);
	check_eq("Organ +0xe2 low 2 bits untouched (base clears, Organ never ORs)",
		 ((unsigned char *)organ)[0xe2] & 0x3, 0);

	unsigned char *pluckedStorage = CSTGBankMemory::AllocAligned(0x108, 0x10);
	CSTGPluckedModel *plucked = new (pluckedStorage) CSTGPluckedModel();
	check_eq("Plucked +0xe1 == 0x77 (base left 0, (0&0x80)|0x77)",
		 ((unsigned char *)plucked)[0xe1], 0x77);
	check_eq("Plucked +0xe2 bit0 set (OR 0x1)", ((unsigned char *)plucked)[0xe2] & 0x1, 1);

	unsigned char *epStorage = CSTGBankMemory::AllocAligned(0x124, 0x10);
	CSTGEPModel *ep = new (epStorage) CSTGEPModel();
	check_eq("EP +0xe1 == 0xd1", ((unsigned char *)ep)[0xe1], 0xd1);
	check_eq("EP +0xe2 bit1 set (OR 0x2)", ((unsigned char *)ep)[0xe2] & 0x2, 0x2);

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
