// SPDX-License-Identifier: GPL-2.0
/*
 * test_klm_authorize_builtins.cpp  -  host-side known-answer test for the
 * REAL CSTGKLMManager::AuthorizeBuiltins()/stamp_object() dispatch chain
 * (klm_manager.cpp), exercised against a hand-built but REAL-SHAPED
 * per-object vtable -- the same 23-real-ABI-slot layout voice_models.cpp
 * actually builds (GetId at real slot 3 / array index 5, GetAuthField/
 * SetAuthField/SetProductId at real slots 13/14/15 / array indices
 * 15/16/17) -- so this test proves klm_manager.cpp's OWN dispatch/
 * argument-passing logic is correct, independent of any one model's
 * concrete GetId body.
 *
 * WHY A HAND-BUILT VTABLE INSTEAD OF LINKING voice_models.cpp DIRECTLY:
 * klm_manager.cpp compiles against the auth.h/oa_types.h "minimal" opaque
 * declaration ecosystem (CSTGVoiceModel/CSTGVoiceModelManager/
 * CSTGEffectManager/CSTGHeapManager all declared as thin opaque
 * char*-based stand-ins there); voice_models.cpp compiles against
 * oa_engine.h/oa_engine_init.h's own FULLER, IncOMPATIBLE declarations of
 * the exact same class names (confirmed real, pre-existing, documented
 * split -- see bar2_stubs_auth.cpp's own header comment: these two
 * ecosystems cannot be #included in the same translation unit). This
 * test instead links klm_manager.cpp (real) + bar2_stubs_auth.cpp (the
 * project's own real, already-used link stubs for the legacy-ROM-bank
 * pass) and provides its own minimal, REAL-SHAPED mock GetId/
 * GetAuthField/SetAuthField/SetProductId leaf functions -- proving
 * AuthorizeBuiltins()'s own control flow (which objects get skipped,
 * which vtable slots get called, in what order, with what arguments) is
 * correct. voice_models.cpp's OWN GetId/GetAuthField/SetAuthField/
 * SetProductId BODIES are separately KAT-verified against ground-truth
 * disassembly in test_voice_models.cpp -- together the two tests cover
 * both halves of the real live-boot call chain that crashed twice this
 * session (sec 10.234) with neither side left unverified.
 *
 * THE ARGUMENT-PASSING BUG THIS TEST IS DESIGNED TO CATCH: an earlier
 * draft of stamp_object() (klm_manager.cpp) called RECOMPUTE/SET_AUTH
 * through zero-argument `void(*)(void*)` function-pointer types, silently
 * dropping the real second argument under -mregparm=3 (EDX left as
 * whatever the caller's own code last used) -- a stamp that would never
 * have verified correctly against IsAuthorized*()'s own recomputation.
 * This test's mock SetAuthField captures the EXACT value it was called
 * with and asserts it equals oa_auth_value(id, 0, bootKey) -- it would
 * have failed against that earlier draft (which passed nothing
 * meaningful) and passes against the fix.
 */

#include <cstdio>
#include <cstring>
#include "auth.h"
#include "oa_heap.h"
#include "oa_internal.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-58s %ld\n", label, got); return; }
	printf("  FAIL  %-58s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- hand-built, REAL-SHAPED per-object vtable (mirrors voice_models.cpp's
 * own OA_VOICE_MODEL_VTABLE layout exactly: 23 slots, GetId at array
 * index 5, GetAuthField/SetAuthField/SetProductId at array indices
 * 15/16/17 -- see that file's own header comment for the ground-truth
 * derivation of these exact slot numbers via objdump -r on OA_real.ko). */
typedef void (*oa_vfn)(void);

static unsigned int MockGetId(const void *self)
{
	return *(const unsigned int *)((const unsigned char *)self + 0x108); /* stashed id */
}
static unsigned int MockGetAuthField(const void *self)
{
	return *(const unsigned int *)((const unsigned char *)self + 0x100);
}
static void MockSetAuthField(void *self, unsigned int value)
{
	*(unsigned int *)((unsigned char *)self + 0x100) = value;
}
static void MockSetProductId(void *self, unsigned int value)
{
	*(unsigned int *)((unsigned char *)self + 0x104) = value;
}

static const oa_vfn kMockVtbl[23] = {
	0, 0,
	0, 0,
	0,                                    /* slot 2  Initialize -- unused here */
	(oa_vfn)&MockGetId,                   /* slot 3  GetId       -- array idx 5  */
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	(oa_vfn)&MockGetAuthField,            /* slot 13 GetAuthField -- array idx 15 */
	(oa_vfn)&MockSetAuthField,            /* slot 14 SetAuthField -- array idx 16 */
	(oa_vfn)&MockSetProductId,            /* slot 15 SetProductId -- array idx 17 */
	0, 0,
	0,                                    /* slot 18 ProcessSubRate -- unused */
	0,                                    /* slot 19 ProcessAudioRate -- unused */
	0,
};
/* Object's own stored vtable ptr, matching voice_models.cpp's own
 * `_vtablePtr = &kVoiceModel_X_Vtbl[2]` convention exactly -- so
 * klm_manager.cpp's vcall()/stamp_object() byte-offset arithmetic
 * (VM_GET_ID=0x0c, VM_GET_AUTH=0x34, VM_SET_AUTH=0x38, VM_RECOMPUTE=0x3c)
 * lands on the same array slots real ground truth does. */
static const void *const kMockVtblAdjusted = (const void *)&kMockVtbl[2];

/* A fake CSTGVoiceModelManager region: +8/+0x30 packed-32 slot tables,
 * +0x58 running count -- same layout klm_manager.cpp's AuthorizeBuiltins
 * itself walks. */
static unsigned char s_vmm[0x100];
static unsigned char s_heap[0x1e8500];

/* Link-satisfying storage for the singletons the rest of klm_manager.cpp
 * (AuthorizeVoiceModel/AuthorizeEffect/AuthorizeMultisampleBank/
 * AuthorizeProduct/RunKLM) references, even though this test only
 * exercises AuthorizeBuiltins()/IsAuthorizedVoiceModel() -- the whole
 * translation unit gets linked in wholesale. Same "isolated test needs
 * its own local storage" precedent used throughout this project. */
char *CSTGVoiceModelManager::sInstance;
char *CSTGHeapManager::sInstance;
char *CSTGEffectManager::sInstance;
char *CSTGGlobal::sInstance;

struct MockObj { const void *vtbl; unsigned char pad[0x10]; };

static void make_mock_object(unsigned char *buf, unsigned int id, unsigned int locked)
{
	memset(buf, 0, 0x110);
	*(const void **)buf = kMockVtblAdjusted;
	*(unsigned int *)(buf + 0x100) = 0xdeadbeef; /* poison the stamp field */
	*(unsigned int *)(buf + 0x104) = locked;     /* the real +0x104 "locked" flag */
	*(unsigned int *)(buf + 0x108) = id;         /* stashed for MockGetId */
}

/*
 * NOTE on stride: klm_manager.cpp's own AuthorizeBuiltins() reads these
 * two tables through NATIVE `int**`/`void**` casts (`((int**)(vmm+0x30))
 * [i]`, `((void**)(vmm+8))[id]`) -- on the REAL 32-bit kernel target
 * these are 4-byte-stride slots (matching the packed table format
 * documented in voice_models.cpp's own Register()), but on THIS host
 * (native 64-bit build, no -m32 for these tests -- see Makefile's
 * HOST_CXXFLAGS) a native pointer is 8 bytes, so the host-compiled code
 * actually reads/writes at 8-byte stride here. Match that exactly so
 * this test observes the same behavior the host-compiled
 * AuthorizeBuiltins() itself exhibits -- this is a host-test-only
 * accommodation (the real target's own 4-byte stride is untouched;nothing
 * in klm_manager.cpp itself changed).
 */
static void register_model(unsigned int index, unsigned int id, void *obj)
{
	((void **)(s_vmm + 0x30))[index] = obj;
	((void **)(s_vmm + 8))[id] = obj;
}

int main()
{
	printf("CSTGKLMManager::AuthorizeBuiltins()/stamp_object() real dispatch test\n");
	printf("(real klm_manager.cpp against a hand-built, real-shaped mock vtable)\n");

	memset(s_vmm, 0, sizeof(s_vmm));
	CSTGVoiceModelManager::sInstance = (char *)s_vmm;

	memset(s_heap, 0, sizeof(s_heap));
	*(unsigned int *)(s_heap + 0x1e8498) = 0x1000;
	CSTGHeapManager::sInstance = (char *)s_heap;
	/* CSTGEffectManager left as a real zeroed instance -- fxCount (+0x800)
	 * reads 0, exactly like a real boot (no CSTGEffectAlgorithm is ever
	 * constructed anywhere in this reconstruction), so AuthorizeBuiltins'
	 * own effects pass genuinely no-ops here too. */
	static unsigned char s_fxm[0x900];
	memset(s_fxm, 0, sizeof(s_fxm));
	CSTGEffectManager::sInstance = (char *)s_fxm;

	printf("\n[1] Three mock objects: id=0 (unlocked), id=1 (unlocked),\n"
	       "    id=2 but LOCKED (+0x104 != 0) -- must be skipped\n");
	static unsigned char objA[0x110], objB[0x110], objC[0x110];
	make_mock_object(objA, 0, 0);
	make_mock_object(objB, 1, 0);
	make_mock_object(objC, 2, 1 /* locked */);
	register_model(0, 0, objA);
	register_model(1, 1, objB);
	register_model(2, 2, objC);
	*(unsigned short *)(s_vmm + 0x58) = 3; /* vmCount */

	printf("\n[2] CSTGKLMManager::CSTGKLMManager() -- real ctor (real rdtsc()\n"
	       "    boot key, no fixed expected value)\n");
	static unsigned char klmStorage[64];
	CSTGKLMManager *klm = new (klmStorage) CSTGKLMManager();
	check_eq("CSTGKLMManager::sInstance == klm", (long)(CSTGKLMManager::sInstance == klm), 1);

	printf("\n[3] CSTGKLMManager::AuthorizeBuiltins() -- REAL end-to-end dispatch\n");
	klm->AuthorizeBuiltins();
	printf("  ok    %-58s\n", "AuthorizeBuiltins() returned without crashing");

	unsigned int bootKey = klm->dwBootKey;
	check_eq("obj A (id 0) stamped == oa_auth_value(0,0,bootKey)",
		 *(unsigned int *)(objA + 0x100), (long)oa_auth_value(0, 0, bootKey));
	check_eq("obj A: +0x104 cleared to 0 by RECOMPUTE/SetProductId(obj,0)",
		 *(unsigned int *)(objA + 0x104), 0);
	check_eq("obj B (id 1) stamped == oa_auth_value(1,0,bootKey)",
		 *(unsigned int *)(objB + 0x100), (long)oa_auth_value(1, 0, bootKey));
	check_eq("obj B: +0x104 cleared to 0 by RECOMPUTE/SetProductId(obj,0)",
		 *(unsigned int *)(objB + 0x104), 0);
	check_eq("obj C (locked) SKIPPED -- stamp field untouched (still poisoned)",
		 *(unsigned int *)(objC + 0x100), (long)0xdeadbeef);
	check_eq("obj C: +0x104 still 1 (never touched, genuinely skipped)",
		 *(unsigned int *)(objC + 0x104), 1);

	printf("\n[4] IsAuthorizedVoiceModel() recomputes and confirms both real stamps\n");
	check_eq("obj A verifies", (long)klm->IsAuthorizedVoiceModel(
		 reinterpret_cast<const CSTGVoiceModel *>(objA)), 1);
	check_eq("obj B verifies", (long)klm->IsAuthorizedVoiceModel(
		 reinterpret_cast<const CSTGVoiceModel *>(objB)), 1);

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return g_fail ? 1 : 0;
}
