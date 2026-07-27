/*
 * ustg_api_klm.h  -  USTGAPIKLM, Eva's client-side view of the installed-EXs-product
 * table + the /proc/.oacmd "AU:"/"SO:*" command channel.
 *
 * BACKGROUND: ustg_api_wrappers.h (2026-07-27, commit 2c4b540) confirmed via a
 * direct disassembly spot-check that USTGAPIKLM is materially different-shaped from
 * the USTGAPIXxx "thin STGMessage facade" family it sits alongside -- 15 methods,
 * `CSTGHandle::Access()`-based shared-memory table reads, not message sends -- and
 * deferred it for a dedicated pass. This is that pass.
 *
 * SHAPE: 14 of 15 methods are genuinely tractable. 13 are thin wrappers around one
 * real method, GetProductInfo() (.text+0x08e1d690, 300 bytes), which does the
 * actual shared-memory work: a CSTGHandle{mode=1}.Access() attaches the real
 * per-product table (already-real CSTGHandle::Access(), src/ipc/stg_handle.cpp --
 * SAME shared-memory attach mechanism USTGUserAPI::Connect() already uses, and the
 * SAME "call Access() again on the returned pointer as if it were itself a
 * CSTGHandle" quirk Connect() already exercises for mFrontPanelStatusAddress --
 * confirmed here as a general CSTGHandle idiom, not a one-off). GetProductItemInfo()
 * (.text+0x08e1da70) does the same for a product's own per-item sub-table, one
 * level deeper (3 chained Access() calls). The 15th method, InstallOptionFile(),
 * is deliberately deferred -- see its own comment below.
 *
 * NOTE (auth-systems context): this class is Eva's READ-ONLY client view into a
 * table OA.ko itself populates and has already authorized/decrypted -- it performs
 * no cryptography and touches none of the 3 auth systems CLAUDE.md documents (boot
 * auth / .reauth / EXs auth) directly. It DOES, however, independently corroborate
 * one live detail from CLAUDE.md's EXs Authorization section: SetAuthString()'s
 * real tail-call target formats and sends the literal command string "AU:%s" (the
 * S-file's own generated auth key, cb32-encoded by Tools/expansion_tools/
 * kronos_auth.py on the generation side) to /proc/.oacmd -- the first time this
 * project has traced that command's real client-side sender rather than just its
 * OA.ko-side consumer.
 */

#ifndef USTG_API_KLM_H
#define USTG_API_KLM_H

#include "eva_types.h"

/* Opaque scalar stand-in for the real eSTGOptionType enum (symbols.csv gives only
 * the mangled type name, not the enumerator list) -- same "declared opaque,
 * byte-for-byte faithful" precedent as ustg_api_wrappers.h's own eSTGMsgPerfType.
 */
typedef unsigned int eSTGOptionType;

/* One installed-product record, 0xa4 (164) bytes, transcribed field-by-field from
 * GetProductInfo()'s own real strncpy()/memcpy() lengths and destination offsets
 * (not assumed by field-name intuition -- GetProductShortName() and
 * GetProductOptionFileName() turned out to read the OPPOSITE-sized fields from
 * what their names alone would suggest; see ustg_api_klm.cpp's own comment).
 */
struct STGInstalledProductRecord {
	char mOptionFileCode[5]; /* +0x00, e.g. "S010" -- read by GetProductOptionFileName() */
	char _pad0[3];
	unsigned int mIdentifier; /* +0x08, written into a CValue scalar dword by GetProductIdentifier() */
	char mShortName[16];      /* +0x0c -- read by GetProductShortName() */
	char mLongName[128];      /* +0x1c -- read by GetProductLongName() */
	unsigned short mNumItems; /* +0x9c */
	CSTGHandle mItemTableHandle; /* +0x9e, 4 bytes -- Access()'d directly by GetProductItemInfo() */
	unsigned char mAuthorized;   /* +0xa2 */
	char _pad1[1];                /* +0xa3 */
};

/* One per-item record within a product's item table, 0x98 (152) bytes. */
struct STGProductItemRecord {
	eSTGOptionType mType;       /* +0x00 */
	unsigned char mValueBlob[0x14]; /* +0x04, CValue-shaped self-describing blob (CopyCValueBlob rule) */
	char mName[128];             /* +0x18 */
};

class USTGAPIKLM {
public:
	/* .text+0x08e1d640, tail-calls the real free function
	 * SendCommandRescanInstalledProducts() (see ustg_api_klm.cpp) -- writes "SO:*"
	 * to /proc/.oacmd. Forwards that function's own bool result.
	 */
	static bool RescanInstalledProducts();

	static bool GetNumberOfProductsInstalled(unsigned int &count);

	/* .text+0x08e1d690 -- the one real worker every other method below (except
	 * RescanInstalledProducts/SetAuthString/InstallOptionFile) is a thin wrapper
	 * around. Real parameter order confirmed from disassembly: longName (128B)
	 * before shortName (16B) before optionFileCode (5B) -- NOT alphabetical, NOT
	 * size order.
	 */
	static bool GetProductInfo(unsigned int productIndex, CValue &identifier, bool &authorized,
	                            unsigned int &numItems, char *longName, char *shortName,
	                            char *optionFileCode);

	static bool IsProductAuthorized(unsigned int productIndex);
	static void GetProductIdentifier(unsigned int productIndex, CValue &identifier);

	/* Real quirk, faithfully preserved: this reads the SHORT 5-byte
	 * mOptionFileCode field (record+0x00), not the 16-byte mShortName field --
	 * the opposite of what the two names would suggest by analogy.
	 */
	static void GetProductOptionFileName(unsigned int productIndex, char optionFileCode[5]);

	static void GetProductLongName(unsigned int productIndex, char longName[128]);

	/* .text+0x08e1d930 -- the one method here that does NOT go through
	 * GetProductInfo()'s normal buffer-fill path alone: it also sprintf()s
	 * "%s %s" (mShortName, mLongName) into the caller's buffer. Real quirk
	 * preserved: mOptionFileCode is not part of this composition.
	 */
	static bool GetProductFullName(unsigned int productIndex, char *fullName);

	/* Real quirk, faithfully preserved: this reads the 16-byte mShortName field
	 * (record+0x0c) -- the opposite of GetProductOptionFileName() above.
	 */
	static void GetProductShortName(unsigned int productIndex, char shortName[16]);

	static bool GetNumberOfItemsInProduct(unsigned int productIndex, unsigned int &numItems);

	/* .text+0x08e1da70 -- one level deeper than GetProductInfo(): a 3rd
	 * CSTGHandle::Access() call against the product record's own
	 * mItemTableHandle field. Uses CopyCValueBlob() for the item's CValue --
	 * genuinely exercises the variable-length blob rule (not the fixed
	 * scalar-dword one GetProductInfo() uses for the product identifier).
	 */
	static bool GetProductItemInfo(unsigned int productIndex, unsigned int itemIndex,
	                                eSTGOptionType &type, CValue &value, char *name);

	static void GetProductItemType(unsigned int productIndex, unsigned int itemIndex, eSTGOptionType &type);
	static void GetProductItemName(unsigned int productIndex, unsigned int itemIndex, char *name);

	/* .text+0x08e1dbd0 -- tail-calls SendCommandAuthorizeOption(authString), NOT
	 * GetProductInfo()-shaped at all. Real quirk, faithfully preserved:
	 * `productIndex` is dead -- read off the stack but never forwarded.
	 */
	static bool SetAuthString(unsigned int productIndex, char *authString);

	/* DELIBERATELY DEFERRED (.text+0x08e1dbf0). Calls
	 * CSTGInstalledEXProducts::InstallProductFile(char const*) on the same
	 * table this class's own Access()-derived pointer addresses (table+0x14 --
	 * consistent with mNumItems/the record array both being CSTGInstalledEXProducts'
	 * own members, i.e. the whole shared-memory table IS a CSTGInstalledEXProducts
	 * instance). That class' real S-file-parsing subsystem is genuinely deep --
	 * InstallProductFile()/LoadProductFile_private() (.text+0x08e33600/0x08e33470)
	 * plus CSTGEXProductInfo::InitializeFromBuffer()/InitializeItems() and
	 * CSTGEXProductItemInfo::InitializeFromBuffer()/Initialize() (.text+0x08e32bb0/
	 * 0x08e32c40/0x08e32750/0x08e338b0) and 2 callbacks (CountProductCallback/
	 * LoadProductFile_Callback) -- a real binary S-file/option-file parser and
	 * installer, a project of its own, matching this batch's "confirmed
	 * different-shaped" verdict for the family as a whole. Not implemented here.
	 */
	/* static bool InstallOptionFile(const char *path); */
};

#endif /* USTG_API_KLM_H */
