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

#include "oa_quad.h"	/* CSTGQuad/CSTGQuadList -- CSTGVoiceModel::AddQuad/RemoveQuad below */

struct CSTGKLMManager;	/* defined in auth.h */

/* Forward declarations only -- full method declarations live in
 * process_oacmd.cpp (the only current caller, via ProcessOACmd's "PT:"/"PC:"
 * /proc/.oacmd handlers). Declared here only so oa_heap.h's
 * oa_pcm_precache_manager() helper has a type to return a pointer to. */
struct CSTGPianoModel;
struct CSTGPCMPrecacheManager;

/* 16-byte multisample-bank identity, FNV-1a-hashed by the KLM manager. */
struct CSTGMultisampleBankUUID {
	unsigned char bytes[16];
};

/*
 * CUUID -- a general-purpose 16-byte UUID type, distinct from
 * CSTGMultisampleBankUUID (a different mangled C++ type in the binary,
 * _ZN5CUUID15ConvertFromTextEPKc), used by ProcessOACmd to parse the
 * "<uuid>" text field common to the LM/LD/CM/CD /proc/.oacmd commands
 * (process_oacmd.cpp). ConvertFromText is a real member function: `this`
 * (the output UUID) is EAX, the text pointer is EDX under -mregparm=3 --
 * i.e. `uuid.ConvertFromText(text)`, not a free function.
 */
struct CUUID {
	unsigned char bytes[16];

	/* Parses a 36-character dashed hex UUID string. Returns true on success. */
	bool ConvertFromText(const char *text);
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
 *
 * +0xd4/+0xd8 added (2026-07-01) while reconstructing the CSTGQuad list
 * primitives (oa_quad.h): confirmed via AddQuad/RemoveQuad's disassembly.
 * +0xd4 is a pointer to this voice model's CSTGQuadList bucket table
 * (12 bytes/entry, indexed by a quad's own mBucketIndex); +0xd8 is a 2-byte
 * "last-added/still-present quad's priority" cache, 0xffff when invalid/none.
 */
struct CSTGVoiceModel {
	void *const *vtbl;		/* +0x00 */
	unsigned char _pad04[0xd0];	/* +0x04 .. +0xd3, not yet recovered */
	struct CSTGQuadList *quadBuckets;	/* +0xd4 */
	unsigned short cachedQuadPriority;	/* +0xd8 */

	/*
	 * Insert `quad` into `quadBuckets[quad->mBucketIndex]`, sorted ascending
	 * by mPriority (ties insert the new quad before existing equal-priority
	 * entries). See oa_quad.h for the full confirmed algorithm.
	 */
	void AddQuad(struct CSTGQuad *quad);

	/*
	 * Unlink `quad` from whichever CSTGQuadList it currently belongs to
	 * (a silent no-op if it isn't linked into any list). See oa_quad.h.
	 */
	void RemoveQuad(struct CSTGQuad *quad);
};

/* An effect algorithm: vtable + an "extra" word at +0x08 (id @0x58, auth @0x64/0x68). */
struct CSTGEffectAlgorithm {
	void *const *vtbl;		/* +0x00 */
	unsigned int _pad04;		/* +0x04 */
	unsigned int dwExtra;		/* +0x08 */
};

/*
 * A loaded multisample bank.  Byte-addressed by the auth layer (+0x5c flags,
 * +0x5d id, +0x6d stamp, +0x71 extra) -- data layout otherwise still left
 * opaque, EXCEPT +0x00, which is a confirmed "reserved but not yet loaded"
 * marker (-1 / 0xffffffff) that LM/LD/CM/CD's /proc/.oacmd handlers
 * (process_oacmd.cpp) all check identically before dispatching. The methods
 * below are declared (no bodies yet) so those handlers compile against real
 * signatures, confirmed via relocation:
 *   _ZN19CSTGMultisampleBank15LoadMultisampleEmbhb
 *   _ZN19CSTGMultisampleBank14LoadDrumSampleEmbhb
 *   _ZN19CSTGMultisampleBank16LoadBankMetaDataEv
 * CORRECTED (2026-07-01): LoadBankMetaData's return type was guessed as
 * void; the LM/LD/CM/CD handlers all test its return value (same "bool
 * return in AL" pattern as ReInitialize/AfterProcess), so it's actually bool.
 */
struct CSTGMultisampleBank {
	bool LoadMultisample(unsigned long uuidLow, bool flag1, unsigned char flag2, bool variant);
	bool LoadDrumSample(unsigned long uuidLow, bool flag1, unsigned char flag2, bool variant);
	bool LoadBankMetaData(void);
	/* "CL:<uuid>" /proc/.oacmd command (process_oacmd.cpp) -- closes this
	 * one bank's PCM data files, confirmed via relocation, always called
	 * under PcmModuleMutex. */
	void ClosePCMDataFiles(void);
};

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

	/*
	 * Verify an /proc/.oacmd "AU:<24-char-string>" payload (dongle-gated,
	 * see VerifyAuthorizationString) and, on a code match against an
	 * installed product, authorize it and append the string to
	 * kAuthFileName. See products.cpp for the faithfully-preserved quirk
	 * where the success-path return value is actually the trailing-newline
	 * file write's result, not the authorization result.
	 */
	bool VerifyAndSaveAuthString(const char *authString);

	/*
	 * Rescans installed EX products ("SO:*" /proc/.oacmd command,
	 * process_oacmd.cpp). CORRECTED return type (2026-07-01): the SO:
	 * handler's disassembly uses this call's return value (XORed with 1,
	 * same "0=ok,1=fail" convention as VerifyAndSaveAuthString, via the
	 * exact same shared return-tail code in ProcessOACmd) -- so this is
	 * NOT void as an earlier pass guessed; it returns a bool. Body not yet
	 * reconstructed -- declared for ProcessOACmd's dispatch to compile
	 * against; belongs to a later stage.
	 */
	bool ReInitialize(void);
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
	/* Called by LM/LD/CM/CD's /proc/.oacmd handlers (process_oacmd.cpp) only
	 * when a bank is stuck in the "reserved but not loaded" marker state
	 * (CSTGMultisampleBank +0x00 == -1) AND LoadBankMetaData() fails to
	 * recover it -- i.e. cleanup on a specific failure path, not a general
	 * "close" operation. Not yet reconstructed bodies -- declared for
	 * compile-time use, confirmed via relocation. */
	static void ReleaseBank(struct CSTGMultisampleBankManager *self,
				const struct CSTGMultisampleBankUUID *uuid);
	static void CloseAllBankFiles(struct CSTGMultisampleBankManager *self);
};

/* ---- process-wide singletons, established during OA init -------------------
 * Declared as char* so the recovered byte-offset accesses (e.g. sInstance + 0x2975184)
 * translate verbatim; their definitions live with each subsystem's own reconstruction. */
struct CSTGGlobal            { static char *sInstance; };
struct CSTGVoiceModelManager { static char *sInstance; };
struct CSTGEffectManager     { static char *sInstance; };
struct CSTGHeapManager       { static char *sInstance; };

#endif /* OA_TYPES_H */
