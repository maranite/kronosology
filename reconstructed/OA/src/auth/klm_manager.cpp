// SPDX-License-Identifier: GPL-2.0
/*
 * klm_manager.cpp  -  CSTGKLMManager: the OA copy-protection license manager.
 *
 * See include/auth.h for the scheme.  The per-object identity ("objectId") and the
 * stamped authorization value are read/written through each object's vtable; the slot
 * offsets below are recovered from OA_322.ko.  The arithmetic — oa_auth_value() and
 * oa_fnv1a16() — is the verifiable core (verify/test_klm_auth.c).
 */

#include "auth.h"
#include "oa_internal.h"   /* CSTGGlobal/VoiceModelManager/EffectManager singletons, rdtsc */

struct CSTGKLMManager *CSTGKLMManager::sInstance;

/* object vtable slots (recovered) */
#define VM_GET_ID      0x0c    /* virtual: object identity                */
#define VM_GET_AUTH    0x34    /* virtual: read stamped authorization     */
#define VM_SET_AUTH    0x38    /* virtual: write stamped authorization    */
#define VM_RECOMPUTE   0x3c
#define FX_GET_ID      0x58
#define FX_GET_AUTH    0x64
#define FX_SET_AUTH    0x68
#define FX_RECOMPUTE   0x6c

static inline unsigned int vcall(const void *obj, int slot)
{
	const void *const *vtbl = *(const void *const *const *)obj;
	return ((unsigned int (*)(const void *))vtbl[slot / 4])(obj);
}

CSTGKLMManager::CSTGKLMManager(void)
{
	new (&kleg) CSTGKLEG();
	sInstance = this;
	dwBootKey = (unsigned int)rdtsc();	/* per-boot authorization multiplier */
	fUnityGain = 1.0f;			/* 0x3f800000 */
}

void CSTGKLMManager::Initialize(void)
{
	kleg.Initialize(this);
}

/* Periodic tick of the embedded license sub-engine, unless globally suppressed. */
struct CSTGKLMManager *CSTGKLMManager::RunKLM(void)
{
	/* CSTGGlobal flag at +0x2975184 disables the KLEG run (e.g. test/installer mode) */
	if (*((char *)CSTGGlobal::sInstance + 0x2975184) == 0)
		return (struct CSTGKLMManager *)kleg.Run();
	return this;
}

/* ---- verification (recompute the stamp and compare) -------------------- */

bool CSTGKLMManager::IsAuthorizedVoiceModel(const struct CSTGVoiceModel *vm)
{
	unsigned int stamped = vcall(vm, VM_GET_AUTH);
	unsigned int id      = vcall(vm, VM_GET_ID);
	unsigned int extra   = *(const unsigned int *)((const char *)vm + VM_EXTRA_OFF);
	return stamped == oa_auth_value(id, extra, dwBootKey);
}

bool CSTGKLMManager::IsAuthorizedEffect(const struct CSTGEffectAlgorithm *fx)
{
	unsigned int stamped = vcall(fx, FX_GET_AUTH);
	unsigned int id      = vcall(fx, FX_GET_ID);
	unsigned int extra   = fx->dwExtra;	/* +0x08 */
	return stamped == oa_auth_value(id, extra, dwBootKey);
}

/*
 * Multisample banks: a bank with the 0x08 "builtin" flag, or with a null id, is
 * implicitly authorized; otherwise its identity (16 bytes @ +0x5d) is FNV-1a hashed and
 * checked against the stamp at +0x6d with the extra at +0x71.
 */
bool CSTGKLMManager::IsAuthorizedMultisampleBank(const struct CSTGMultisampleBank *bank)
{
	const unsigned char *b = (const unsigned char *)bank;
	if ((b[0x5c] & 8) != 0)			/* builtin / always-authorized flag */
		return true;
	if (*(const unsigned int *)(b + 0x04) == 0)
		return true;

	unsigned int hash  = oa_fnv1a16(b + 0x5d);
	unsigned int stamp = *(const unsigned int *)(b + 0x6d);
	unsigned int extra = *(const unsigned int *)(b + 0x71);
	return stamp == oa_auth_value(hash, extra, dwBootKey);
}

/* ---- authorization (stamp the object) ---------------------------------- */
/*
 * Authorize* looks up the object in its manager's table and stamps it with
 * oa_auth_value(id, extra, dwBootKey) via the object's SET_AUTH vtable slot, so a later
 * IsAuthorized* on the same boot passes.  (The stamp args are computed inside the
 * virtual calls in the binary; the value is the same product IsAuthorized* checks.)
 */
int CSTGKLMManager::AuthorizeVoiceModel(unsigned int idx)
{
	if (idx >= 10)
		return 0;
	void *vm = ((void **)((char *)CSTGVoiceModelManager::sInstance + 8))[idx];
	if (!vm)
		return 0;
	stamp_object(vm, VM_GET_ID, VM_SET_AUTH, VM_RECOMPUTE, dwBootKey);
	return 1;
}

int CSTGKLMManager::AuthorizeEffect(unsigned int idx)
{
	if (idx >= 0xc6)
		return 0;
	void *fx = ((void **)((char *)CSTGEffectManager::sInstance + 0x804))[idx];
	if (!fx)
		return 0;
	stamp_object(fx, FX_GET_ID, FX_SET_AUTH, FX_RECOMPUTE, dwBootKey);
	return 1;
}

/*
 * GetKLMAddressForPatch: returns a pointer to the real unity gain (fUnityGain) when the
 * patch's own auth check passes, otherwise a decoy pointer inside the KLEG block — so
 * unauthorized patches read a wrong gain and render incorrectly instead of erroring.
 */
void *CSTGKLMManager::GetKLMAddressForPatch(struct CSTGPatch *patch, void *ctx)
{
	void *comp = patch->GetUpComponent();		/* vtable +0x68 */
	unsigned int stamped = vcall(comp, 0x34);
	unsigned int id      = vcall(comp, VM_GET_ID);
	unsigned int extra   = *((unsigned int *)comp + 0x41);

	if (stamped == oa_auth_value(id, extra, dwBootKey) &&
	    !patch->IsUsingAnyUnauthorizedMultisamples())	/* vtable +0x104 */
		return &fUnityGain;
	return (char *)&kleg + 0x14;			/* decoy */
}
