// SPDX-License-Identifier: GPL-2.0
/*
 * auth.h  -  OA.ko copy-protection / authorization layer (Stage 1).
 *
 * CSTGKLMManager is the Korg "KLM" license manager.  Its anti-tamper scheme is a
 * RUNTIME-KEYED CHECKSUM rather than a persisted licence:
 *
 *   - At construction it captures a per-boot key:  dwBootKey = (uint32)rdtsc().
 *   - To authorize an object, it stamps it with  auth = (objectId + 1 + extra) * dwBootKey.
 *   - IsAuthorized*() recomputes that product from the object's live identity and
 *     compares it to the stamped value.
 *
 * Because the multiplier is the boot-time TSC, an authorization value is only valid for
 * the session that produced it: a value copied/forged from another boot will not match,
 * and the product can't be precomputed offline without the (unknowable) key.  For
 * multisample banks the "objectId" is an FNV-1a hash (Korg offset basis) of a 16-byte
 * bank identity.  GetKLMAddressForPatch() returns a pointer to a real 1.0f gain when a
 * patch authorizes, or a decoy pointer when it doesn't, so unauthorized content renders
 * wrong/silent rather than failing loudly.
 *
 * Recovered from OA_322.ko (firmware 3.2.2).  Verified by verify/test_klm_auth.c.
 */

#ifndef OA_AUTH_H
#define OA_AUTH_H

#include "oa_types.h"     /* CSTGKLEG, CSTGVoiceModel, CSTGEffectAlgorithm, CSTGMultisampleBank, CSTGPatch */
#include "oa_authmath.h"  /* oa_fnv1a16(), oa_auth_value() — the verifiable core */

/* ------------------------------------------------------------------------- *
 *  CSTGKLMManager  (36 bytes, singleton sInstance)
 * ------------------------------------------------------------------------- */
struct CSTGKLMManager {
	unsigned int dwBootKey;		/* 0x00 (int)rdtsc() captured in the ctor   */
	struct CSTGKLEG kleg;		/* 0x04 periodically-run sub-engine (RunKLM)*/
	float        fUnityGain;	/* 0x20 == 1.0f; GetKLMAddressForPatch real */

	CSTGKLMManager(void);
	void Initialize(void);
	struct CSTGKLMManager *RunKLM(void);	/* ticks kleg unless globally suppressed */

	/* authorize: stamp the object at index idx with its auth value */
	int  AuthorizeVoiceModel(unsigned int idx);
	int  AuthorizeEffect(unsigned int idx);
	int  AuthorizeMultisampleBank(unsigned int idx, const struct CSTGMultisampleBank *bank);
	int  AuthorizeProduct(struct CSTGEXProductInfo *product);
	void AuthorizeBuiltins(void);

	/* verify: recompute and compare */
	bool IsAuthorizedVoiceModel(const struct CSTGVoiceModel *vm);
	bool IsAuthorizedEffect(const struct CSTGEffectAlgorithm *fx);
	bool IsAuthorizedMultisampleBank(const struct CSTGMultisampleBank *bank);

	/* returns &fUnityGain if the patch authorizes, else a decoy pointer */
	void *GetKLMAddressForPatch(struct CSTGPatch *patch, void *ctx);

	static struct CSTGKLMManager *sInstance;
};

#endif /* OA_AUTH_H */
