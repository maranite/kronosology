/*
 * korg_ksc.cpp  -  CKorgKsc partial reconstruction. See include/korg_ksc.h
 * for full ground-truth provenance and the deferred-method list.
 */

#include "korg_ksc.h"

#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

CKorgKsc::CKorgKsc(const char *name, const char *uuid, bool fieldA, bool fieldB)
	: CKorgFile(name, ".KSC")
	, mFieldA(fieldA)
	, mFieldB(fieldB)
{
	if (uuid) {
		strncpy(mUUID, uuid, sizeof(mUUID));
		mUUID[sizeof(mUUID) - 1] = 0;
	} else {
		mUUID[0] = 0;
	}

	/* Real ground truth also makes a second, redundant CKorgKsc::SetPath()
	 * call using `name` again here -- not reproduced, see file header.
	 */
}

CKorgKsc::~CKorgKsc()
{
}

void CKorgKsc::GetUUID(char *dest, unsigned int maxLen) const
{
	if (!dest)
		return;
	strncpy(dest, mUUID, maxLen);
	dest[maxLen - 1] = 0;
}

void CKorgKsc::SetUUID(const char *uuid)
{
	if (!uuid) {
		mUUID[0] = 0;
		return;
	}
	strncpy(mUUID, uuid, sizeof(mUUID));
	mUUID[sizeof(mUUID) - 1] = 0;
}

void CKorgKsc::MakeFolder()
{
	char buf[0x100];
	CKorgFile::GetFolder(buf, sizeof(buf));
	mkdir(buf, 0777);
}
