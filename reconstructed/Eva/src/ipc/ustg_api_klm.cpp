/*
 * ustg_api_klm.cpp  -  see include/ustg_api_klm.h.
 *
 * GetProductInfo()/GetProductItemInfo() transcribed instruction-by-instruction from
 * `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva, .text+0x08e1d690/0x08e1da70); every
 * other USTGAPIKLM method below is a thin wrapper confirmed the same way, argument
 * slot by argument slot -- several place their one real output pointer into a
 * DIFFERENT GetProductInfo() argument slot than a name-based guess would suggest
 * (see header comment on GetProductOptionFileName()/GetProductShortName()).
 */

#include "ustg_api_klm.h"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

/* --- /proc/.oacmd command channel ---
 *
 * SendCommandRescanInstalledProducts()/SendCommandAuthorizeOption() (real free
 * functions, .text+0x08e48da0/0x08e48bc0) share this shape: open("/proc/.oacmd",
 * O_RDWR), write() the command string, read() back exactly 4 bytes (a
 * little-endian int status), return whether that status == 0. Confirms/extends
 * CLAUDE.md's already-documented "AU:" trigger with its general request/response
 * envelope, decoded here for the first time. A 3rd sibling in the same real
 * function cluster, SendCommandRescanPianoTypes() ("PT:*", .text+0x08e48f40), has
 * no USTGAPIKLM caller and is out of scope here.
 */
static bool SendOacmdCommand(const char *cmd)
{
	int fd = open("/proc/.oacmd", O_RDWR);
	if (fd == -1)
		return false;

	size_t len = strlen(cmd);
	if ((size_t)write(fd, cmd, len) != len) {
		close(fd);
		return false;
	}

	int status;
	if (read(fd, &status, sizeof(status)) != (int)sizeof(status)) {
		close(fd);
		return false;
	}

	close(fd);
	return status == 0;
}

/* .text+0x08e48da0. Real embedded literal, 5 bytes incl. terminator ("SO:*\0"). */
static bool SendCommandRescanInstalledProducts()
{
	return SendOacmdCommand("SO:*");
}

/* .text+0x08e48bc0. Real format string at .rodata+0x8fd950b, snprintf()'d into a
 * 256-byte stack buffer before sending.
 */
static bool SendCommandAuthorizeOption(const char *authString)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "AU:%s", authString);
	return SendOacmdCommand(buf);
}

bool USTGAPIKLM::RescanInstalledProducts()
{
	return SendCommandRescanInstalledProducts();
}

bool USTGAPIKLM::GetNumberOfProductsInstalled(unsigned int &count)
{
	CSTGHandle tableHandle;
	tableHandle.mode = 1;
	void *table = tableHandle.Access();

	count = *reinterpret_cast<unsigned short *>(static_cast<char *>(table) + 0x14);

	tableHandle.Release();
	return true;
}

bool USTGAPIKLM::GetProductInfo(unsigned int productIndex, CValue &identifier, bool &authorized,
                                 unsigned int &numItems, char *longName, char *shortName,
                                 char *optionFileCode)
{
	CSTGHandle tableHandle;
	tableHandle.mode = 1;
	void *table = tableHandle.Access();

	unsigned short count = *reinterpret_cast<unsigned short *>(static_cast<char *>(table) + 0x14);
	if (productIndex >= count) {
		tableHandle.Release();
		return false;
	}

	STGInstalledProductRecord *recordsRaw = reinterpret_cast<STGInstalledProductRecord *>(
	    static_cast<char *>(table) + 0x18);

	/* Same "call Access() again on the returned pointer as if it were itself a
	 * CSTGHandle" quirk USTGUserAPI::Connect() already exercises for
	 * mFrontPanelStatusAddress (see eva_types.h) -- Access() treats whatever int
	 * sits at `this` as a cache-table index (stg_handle.cpp), so this is a real,
	 * well-defined (if unusual) second shared-memory attach, not UB.
	 */
	void *records2 = reinterpret_cast<CSTGHandle *>(recordsRaw)->Access();
	STGInstalledProductRecord *record = reinterpret_cast<STGInstalledProductRecord *>(
	    static_cast<char *>(records2) + productIndex * sizeof(STGInstalledProductRecord));

	WriteCValueDword(identifier, record->mIdentifier);
	authorized = record->mAuthorized != 0;
	numItems = record->mNumItems;

	if (longName) {
		strncpy(longName, record->mLongName, 0x80);
		longName[0x7f] = '\0';
	}
	if (shortName) {
		strncpy(shortName, record->mShortName, 0x10);
		shortName[0x0f] = '\0';
	}
	if (optionFileCode) {
		strncpy(optionFileCode, record->mOptionFileCode, 5);
		optionFileCode[4] = '\0';
	}

	reinterpret_cast<CSTGHandle *>(recordsRaw)->Release();
	tableHandle.Release();
	return true;
}

bool USTGAPIKLM::IsProductAuthorized(unsigned int productIndex)
{
	CValue dummyIdentifier;
	unsigned int dummyNumItems;
	bool authorized = false;
	GetProductInfo(productIndex, dummyIdentifier, authorized, dummyNumItems, 0, 0, 0);
	return authorized;
}

void USTGAPIKLM::GetProductIdentifier(unsigned int productIndex, CValue &identifier)
{
	unsigned int dummyNumItems;
	bool dummyAuthorized;
	GetProductInfo(productIndex, identifier, dummyAuthorized, dummyNumItems, 0, 0, 0);
}

void USTGAPIKLM::GetProductOptionFileName(unsigned int productIndex, char optionFileCode[5])
{
	CValue dummyIdentifier;
	unsigned int dummyNumItems;
	bool dummyAuthorized;
	GetProductInfo(productIndex, dummyIdentifier, dummyAuthorized, dummyNumItems, 0, 0, optionFileCode);
}

void USTGAPIKLM::GetProductLongName(unsigned int productIndex, char longName[128])
{
	CValue dummyIdentifier;
	unsigned int dummyNumItems;
	bool dummyAuthorized;
	GetProductInfo(productIndex, dummyIdentifier, dummyAuthorized, dummyNumItems, longName, 0, 0);
}

bool USTGAPIKLM::GetProductFullName(unsigned int productIndex, char *fullName)
{
	CValue dummyIdentifier;
	unsigned int dummyNumItems;
	bool dummyAuthorized;
	char longNameBuf[128];
	char shortNameBuf[16];

	bool found = GetProductInfo(productIndex, dummyIdentifier, dummyAuthorized, dummyNumItems,
	                             longNameBuf, shortNameBuf, 0);
	if (!found)
		return false;

	sprintf(fullName, "%s %s", shortNameBuf, longNameBuf);
	return true;
}

void USTGAPIKLM::GetProductShortName(unsigned int productIndex, char shortName[16])
{
	CValue dummyIdentifier;
	unsigned int dummyNumItems;
	bool dummyAuthorized;
	GetProductInfo(productIndex, dummyIdentifier, dummyAuthorized, dummyNumItems, 0, shortName, 0);
}

bool USTGAPIKLM::GetNumberOfItemsInProduct(unsigned int productIndex, unsigned int &numItems)
{
	CValue dummyIdentifier;
	bool dummyAuthorized;
	return GetProductInfo(productIndex, dummyIdentifier, dummyAuthorized, numItems, 0, 0, 0);
}

bool USTGAPIKLM::GetProductItemInfo(unsigned int productIndex, unsigned int itemIndex,
                                     eSTGOptionType &type, CValue &value, char *name)
{
	CSTGHandle tableHandle;
	tableHandle.mode = 1;
	void *table = tableHandle.Access();

	STGInstalledProductRecord *recordsRaw = reinterpret_cast<STGInstalledProductRecord *>(
	    static_cast<char *>(table) + 0x18);
	void *records2 = reinterpret_cast<CSTGHandle *>(recordsRaw)->Access();
	STGInstalledProductRecord *record = reinterpret_cast<STGInstalledProductRecord *>(
	    static_cast<char *>(records2) + productIndex * sizeof(STGInstalledProductRecord));

	CSTGHandle *itemTableHandle = &record->mItemTableHandle;
	void *itemTable = itemTableHandle->Access();
	STGProductItemRecord *item = reinterpret_cast<STGProductItemRecord *>(
	    static_cast<char *>(itemTable) + itemIndex * sizeof(STGProductItemRecord));

	type = item->mType;
	CopyCValueBlob(value, item->mValueBlob);
	if (name) {
		strncpy(name, item->mName, 0x80);
		name[0x7f] = '\0';
	}

	itemTableHandle->Release();
	reinterpret_cast<CSTGHandle *>(recordsRaw)->Release();
	tableHandle.Release();
	return true;
}

void USTGAPIKLM::GetProductItemType(unsigned int productIndex, unsigned int itemIndex, eSTGOptionType &type)
{
	CValue dummyValue;
	GetProductItemInfo(productIndex, itemIndex, type, dummyValue, 0);
}

void USTGAPIKLM::GetProductItemName(unsigned int productIndex, unsigned int itemIndex, char *name)
{
	eSTGOptionType dummyType;
	CValue dummyValue;
	GetProductItemInfo(productIndex, itemIndex, dummyType, dummyValue, name);
}

bool USTGAPIKLM::SetAuthString(unsigned int /*productIndex*/, char *authString)
{
	return SendCommandAuthorizeOption(authString);
}
