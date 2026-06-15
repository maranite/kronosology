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

/*
 * Stamp an object through its own virtual SET_AUTH, mirroring the binary's call dance:
 *   obj->vtbl[recompute]();  setAuth = obj->vtbl[set];  obj->vtbl[getId]();  setAuth();
 * The object's SET_AUTH writes the stamp; by construction (and as IsAuthorized* checks)
 * that stamp equals oa_auth_value(getId(), extra, dwBootKey) for this boot.
 */
static inline void stamp_object(void *obj, int idSlot, int setSlot, int recomputeSlot,
				unsigned int /*bootKey*/)
{
	void *const *vtbl = *(void *const *const *)obj;
	((void (*)(void *))vtbl[recomputeSlot / 4])(obj);
	void (*setAuth)(void *) = (void (*)(void *))vtbl[setSlot / 4];
	((unsigned int (*)(const void *))vtbl[idSlot / 4])(obj);	/* primes the id used by setAuth */
	setAuth(obj);
}

/*
 * The multisample-bank manager is a sub-object of the STG global region owned by
 * CSTGHeapManager.  The binary derives its address as:
 *     base = (heap != sentinel) ? *(heap+0x38) + *(heap+0x1e8498) : 0
 *     bankmgr = base + 0x60524
 * Kept as an accessor so the heap-internal offsets stay local to CSTGHeapManager's own
 * reconstruction rather than leaking into the auth layer.
 */
static inline struct CSTGMultisampleBankManager *klm_bank_manager(void)
{
	char *heap = (char *)CSTGHeapManager::sInstance;
	unsigned int base = 0;
	if (heap != 0)		/* binary guards via a -0x2c sentinel == null heap */
		base = *(unsigned int *)(heap + 0x38) + *(unsigned int *)(heap + 0x1e8498);
	return (struct CSTGMultisampleBankManager *)(base + 0x60524);
}

/*
 * Legacy builtin ROM-bank UUID template (16 bytes), recovered from .data:
 *   "KORG"  (sLegacyBankPrefix @ 0x60fb28)
 *   00 00 00 00 00 00 00 00
 *   "MS"   (@ 0x60fb34)
 *   00
 *   <index>   filled per-bank with 0,2,4,...,0x14
 */
static void build_legacy_builtin_uuid(unsigned char uuid[16], unsigned char index)
{
	static const unsigned char tmpl[16] = {
		'K','O','R','G', 0,0,0,0, 0,0,0,0, 'M','S', 0, 0
	};
	for (int i = 0; i < 16; i++)
		uuid[i] = tmpl[i];
	uuid[15] = index;
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
 * Authorize the multisample bank named by uuid.  Looks the bank up by UUID (preferring the
 * direct table, then a legacy-RAM alias), then writes extra@+0x71 and the stamp@+0x6d =
 * oa_auth_value(fnv1a16(uuid), extra, dwBootKey) — the exact value IsAuthorizedMultisample-
 * Bank later recomputes.  Returns 0 if no such bank is loaded.
 */
int CSTGKLMManager::AuthorizeMultisampleBank(unsigned int extra,
					     const struct CSTGMultisampleBankUUID *uuid)
{
	struct CSTGMultisampleBankManager *mgr = klm_bank_manager();
	void *bank = CSTGMultisampleBankManager::AccessBank(mgr, uuid);
	if (!bank)
		bank = CSTGMultisampleBankManager::AccessBankWithLegacyRAMAlias(mgr, uuid);
	if (!bank)
		return 0;

	unsigned char *b = (unsigned char *)bank;
	*(unsigned int *)(b + 0x71) = extra;
	*(unsigned int *)(b + 0x6d) =
		oa_auth_value(oa_fnv1a16((const unsigned char *)uuid), extra, dwBootKey);
	return 1;
}

/*
 * AuthorizeBuiltins: the startup bulk-authorizer.  In three passes it stamps everything that
 * ships in-box, so factory content always verifies on this boot:
 *   1. every loaded voice model whose slot flag (+0x104) is clear and whose id < 10;
 *   2. every effect algorithm whose +0x08 flag is clear and whose id < 0xc6;
 *   3. the 11 legacy ROM multisample banks (UUID = "KORG"+8x00+"MS"+00+{0,2,...,0x14}).
 * Passes 1 and 2 are AuthorizeVoiceModel/AuthorizeEffect inlined over the manager tables.
 */
void CSTGKLMManager::AuthorizeBuiltins(void)
{
	/* 1. voice models: iterate the load list @+0x30, stamp the canonical slot @+8+id*4 */
	char *vmm = (char *)CSTGVoiceModelManager::sInstance;
	unsigned int vmCount = *(unsigned short *)(vmm + 0x58);
	for (unsigned int i = 0; i < vmCount; i++) {
		int *vm = ((int **)(vmm + 0x30))[i];
		if (vm[0x41] != 0)			/* +0x104 in-use/locked flag */
			continue;
		unsigned int id = vcall(vm, VM_GET_ID);
		if (id >= 10)
			continue;
		void *obj = ((void **)(vmm + 8))[id];
		if (obj)
			stamp_object(obj, VM_GET_ID, VM_SET_AUTH, VM_RECOMPUTE, dwBootKey);
	}

	/* 2. effects: iterate the load list @+0, stamp the canonical slot @+0x804+id*4
	 *    (the binary loop is rotated by the optimizer; semantics are this clean form) */
	char *fxm = (char *)CSTGEffectManager::sInstance;
	unsigned int fxCount = *(unsigned int *)(fxm + 0x800);
	for (unsigned int i = 0; i < fxCount; i++) {
		int *fx = ((int **)fxm)[i];
		if (fx[2] != 0)				/* +0x08 in-use/locked flag */
			continue;
		unsigned int id = vcall(fx, FX_GET_ID);
		if (id >= 0xc6)
			continue;
		void *obj = ((void **)(fxm + 0x804))[id];
		if (obj)
			stamp_object(obj, FX_GET_ID, FX_SET_AUTH, FX_RECOMPUTE, dwBootKey);
	}

	/* 3. the 11 legacy builtin ROM banks (index byte steps by 2: 0,2,...,0x14) */
	struct CSTGMultisampleBankManager *mgr = klm_bank_manager();
	for (unsigned int n = 0; n < 11; n++) {
		unsigned char uuid[16];
		build_legacy_builtin_uuid(uuid, (unsigned char)(n * 2));
		void *bank = CSTGMultisampleBankManager::AccessBank(
			mgr, (const struct CSTGMultisampleBankUUID *)uuid);
		if (!bank)
			bank = CSTGMultisampleBankManager::AccessBankWithLegacyRAMAlias(
				mgr, (const struct CSTGMultisampleBankUUID *)uuid);
		if (!bank)
			continue;
		unsigned char *b = (unsigned char *)bank;
		*(unsigned int *)(b + 0x71) = 0;	/* builtins carry extra = 0 */
		*(unsigned int *)(b + 0x6d) = oa_auth_value(oa_fnv1a16(uuid), 0, dwBootKey);
	}
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
