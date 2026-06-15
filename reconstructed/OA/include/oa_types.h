// SPDX-License-Identifier: GPL-2.0
/*
 * oa_types.h  -  shared STG object types referenced by the OA auth layer (Stage 0).
 *
 * These are the *minimal faithful* shapes needed to compile the Stage-1 auth translation
 * unit: only the offsets the copy-protection code actually touches are named; the rest of
 * each object is opaque padding, to be filled in as later stages reconstruct those classes.
 * Offsets are recovered from OA_322.ko (firmware 3.2.2).  The target is x86-32, so all
 * pointers and the listed fields are 32-bit.
 */

#ifndef OA_TYPES_H
#define OA_TYPES_H

struct CSTGKLMManager;	/* defined in auth.h */

/* 16-byte multisample-bank identity, FNV-1a-hashed by the KLM manager. */
struct CSTGMultisampleBankUUID {
	unsigned char bytes[16];
};

/*
 * CSTGKLEG — the license sub-engine embedded in CSTGKLMManager at +0x04 (28 bytes, so the
 * manager's fUnityGain lands at +0x20).  Ticked periodically by RunKLM().
 */
struct CSTGKLEG {
	unsigned char _opaque[0x1c];

	CSTGKLEG();
	void  Initialize(struct CSTGKLMManager *owner);
	void *Run();
};

/*
 * A tone "component"/voice model.  The auth layer only uses its vtable (+0x00: id @slot
 * 0x0c, auth @0x34/0x38, recompute @0x3c) and its extra word at +0x104 (VM_EXTRA_OFF).
 */
struct CSTGVoiceModel {
	void *const *vtbl;		/* +0x00 */
};

/* An effect algorithm: vtable + an "extra" word at +0x08 (id @0x58, auth @0x64/0x68). */
struct CSTGEffectAlgorithm {
	void *const *vtbl;		/* +0x00 */
	unsigned int _pad04;		/* +0x04 */
	unsigned int dwExtra;		/* +0x08 */
};

/* A loaded multisample bank.  Byte-addressed by the auth layer (+0x5c flags, +0x5d id,
 * +0x6d stamp, +0x71 extra), so it is left opaque here. */
struct CSTGMultisampleBank;

/* A patch references an "up component" and may use multisamples; both are checked when the
 * engine asks where to read a patch's KLM gain from. */
struct CSTGPatch {
	void *GetUpComponent();			/* vtable +0x68 */
	bool  IsUsingAnyUnauthorizedMultisamples();	/* vtable +0x104 */
};

/* An installed EX product; pAuthHeader (+0x04) points at the product's authorization
 * header (entry count @+0x98, slot index @+0x9a, dwExtra @+0x04). */
struct CSTGEXProductInfo {
	void *_pad00;			/* +0x00 */
	void *pAuthHeader;		/* +0x04 */
};

/*
 * The multisample-bank registry.  In the binary these are instance methods invoked under
 * the -mregparm=3 ABI (this in EAX); we model them as static members taking the instance
 * pointer explicitly so the recovered call sites translate directly.
 */
struct CSTGMultisampleBankManager {
	static void *AccessBank(struct CSTGMultisampleBankManager *self,
				const struct CSTGMultisampleBankUUID *uuid);
	static void *AccessBankWithLegacyRAMAlias(struct CSTGMultisampleBankManager *self,
				const struct CSTGMultisampleBankUUID *uuid);
};

/* ---- process-wide singletons, established during OA init -------------------
 * Declared as char* so the recovered byte-offset accesses (e.g. sInstance + 0x2975184)
 * translate verbatim; their definitions live with each subsystem's own reconstruction. */
struct CSTGGlobal            { static char *sInstance; };
struct CSTGVoiceModelManager { static char *sInstance; };
struct CSTGEffectManager     { static char *sInstance; };
struct CSTGHeapManager       { static char *sInstance; };

#endif /* OA_TYPES_H */
