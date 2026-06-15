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

/*
 * An installed EX product record (0xa4 = 164-byte stride; alignment 1 — the binary reads
 * its fields with unaligned loads, so the struct is packed).  All authorization fields are
 * INLINE in the record (there is no separate header pointer): dwExtra @+0x08, entry count
 * @+0x9c, the heap slot of this product's authorization-entry table @+0x9e, and the
 * "authorized" flag @+0xa2.  The 4-char code @+0x00 is what AuthorizeProductByFilename
 * matches on.
 */
struct __attribute__((__packed__)) CSTGEXProductInfo {
	unsigned char  code[4];		/* +0x00 4-char product code            */
	unsigned char  _pad04[4];	/* +0x04                                */
	unsigned int   dwExtra;		/* +0x08 stamped onto authorized banks  */
	unsigned char  _pad0c[0x90];	/* +0x0c .. +0x9b                       */
	unsigned short count;		/* +0x9c number of authorization entries*/
	unsigned int   tableIndex;	/* +0x9e heap slot of the entry table   */
	unsigned char  authorized;	/* +0xa2 set to 1 once authorized       */
	unsigned char  _pada3;		/* +0xa3 (pad to 0xa4 stride)           */
};

/*
 * The installed-products registry (lives at heapbase+0x14).  count @+0x00, and the heap
 * slot of the contiguous CSTGEXProductInfo array @+0x04.
 */
struct CSTGInstalledEXProducts {
	unsigned short count;		/* +0x00 */
	unsigned char  _pad02[2];	/* +0x02 */
	unsigned int   slotIndex;	/* +0x04 heap slot of the product array */

	/* find the product whose 4-char code matches, stamp + AuthorizeProduct it */
	unsigned int AuthorizeProductByFilename(const char *code4);
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
