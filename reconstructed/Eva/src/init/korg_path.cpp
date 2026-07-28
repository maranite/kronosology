/*
 * korg_path.cpp  -  CKorgPath. See include/korg_path.h for full ground-truth
 * provenance (vtable slot layout, object layout, and the rejected-candidate
 * writeup for this batch's own "shared root + siblings" class-inventory
 * sweep).
 */

#include "korg_path.h"
#include "korg_linux_path.h"	/* CKorgPath::Make() always constructs one -- see header comment */

#include <cstdio>
#include <cstring>

CKorgPath::CKorgPath(const char *name)
{
	if (!name) {
		mFileName[0] = 0;
		return;
	}
	strncpy(mFileName, name, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;
}

CKorgPath::CKorgPath(const CKorgPath *other)
{
	if (!other) {
		mFileName[0] = 0;
		return;
	}
	strncpy(mFileName, other->mFileName, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;
}

CKorgPath::~CKorgPath()
{
}

int CKorgPath::HasExtension(const char *name, const char *ext)
{
	const char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	return strcasecmp(dot, ext) == 0;
}

void CKorgPath::AddExtension(char *name, unsigned int maxLen, const char *ext)
{
	unsigned int len = strlen(name);
	strncat(name, ext, maxLen - len);
	name[maxLen - 1] = 0;
}

int CKorgPath::RemoveExtension(char *name)
{
	char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	*dot = 0;
	return 1;
}

int CKorgPath::RemoveExtension(char *name, char *dest, unsigned int maxLen)
{
	dest[0] = 0;
	char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	strncpy(dest, dot, maxLen);
	dest[maxLen - 1] = 0;
	*dot = 0;
	return 1;
}

int CKorgPath::ValidExtension(const char *ext)
{
	return ext && ext[0] == '.';
}

void CKorgPath::Sanitize(char *name)
{
	char *w = name;
	const char *r = name;
	for (;;) {
		unsigned char c = (unsigned char)*r++;
		if (c == 0) {
			*w = 0;
			return;
		}
		if (c == '-' || c <= ' ' || c == '_')
			continue;
		*w++ = (char)c;
	}
}

int CKorgPath::Capitalized(const char *name)
{
	for (const char *p = name; *p; p++) {
		unsigned char c = (unsigned char)*p;
		if ((unsigned char)(c - 'a') <= 0x19)
			return 0;
	}
	return 1;
}

CKorgPath *CKorgPath::Make(const char *path)
{
	return new CKorgLinuxPath(path);
}

void CKorgPath::Set(const CKorgPath *base, const char *name)
{
	char sep[2];
	sep[0] = Separator();
	sep[1] = 0;

	strncpy(mFileName, base->mFileName, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;

	unsigned int len = strlen(mFileName);
	strncat(mFileName, sep, sizeof(mFileName) - len);
	mFileName[sizeof(mFileName) - 1] = 0;

	len = strlen(mFileName);
	strncat(mFileName, name, sizeof(mFileName) - len);
	mFileName[sizeof(mFileName) - 1] = 0;
}

void CKorgPath::GetPath(char *dest, unsigned int maxLen) const
{
	strncpy(dest, mFileName, maxLen);
	dest[maxLen - 1] = 0;
}

void CKorgPath::SetPath(const char *path)
{
	if (!path) {
		mFileName[0] = 0;
		return;
	}
	strncpy(mFileName, path, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;
}

const char *CKorgPath::GetPathName() const
{
	char sep = Separator();
	const char *found = strrchr(mFileName, sep);
	return found ? found + 1 : mFileName;
}

const char *CKorgPath::GetPathExtension() const
{
	return strrchr(mFileName, '.');
}

int CKorgPath::GetPathNameNoExtension(char *dest, unsigned int maxLen) const
{
	char sep = Separator();
	const char *found = strrchr(mFileName, sep);
	const char *src = found ? found + 1 : mFileName;

	strncpy(dest, src, maxLen);
	dest[maxLen - 1] = 0;

	char *dot = strrchr(dest, '.');
	if (!dot)
		return 0;
	*dot = 0;
	return 1;
}

CKorgPath *CKorgPath::GetFolder() const
{
	CKorgPath *clone = Copy();
	char sep = Separator();
	char *found = strrchr(clone->mFileName, sep);
	if (found)
		*found = 0;
	return clone;
}

int CKorgPath::MakePathFromFolder(const char * /*folder*/) const
{
	/* Ground truth builds+mutates a Copy()'d clone here but never frees
	 * or returns it (a genuine dead-computation/leak, see header
	 * comment) -- every real code path converges on returning 0. */
	return 0;
}

CKorgPath *CKorgPath::Find(const CKorgPath &other) const
{
	FILE *fp = fopen(other.mFileName, "r");
	if (fp) {
		fclose(fp);
		return other.Copy();
	}

	char otherSep = other.Separator();
	const char *otherFound = strrchr(other.mFileName, otherSep);
	const char *searchName = otherFound ? otherFound + 1 : other.mFileName;

	CKorgPath *dirClone = Copy();
	char thisSep = Separator();
	char *dirFound = strrchr(dirClone->mFileName, thisSep);
	if (dirFound)
		*dirFound = 0;

	CKorgPath *result = 0;
	if (dirClone)
		result = FindRecurse(searchName, dirClone);

	delete dirClone;
	return result;
}

void CKorgPath::GetOposPath(char *dest, unsigned int maxLen)
{
	strncpy(dest, mFileName, maxLen);
	dest[maxLen - 1] = 0;
}

void CKorgPath::SetOposPath(const char *path)
{
	if (!path) {
		mFileName[0] = 0;
		return;
	}
	strncpy(mFileName, path, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;
}
