/*
 * korg_file.cpp  -  CKorgFile. See include/korg_file.h for full ground-truth
 * provenance (vtable slot layout, object layout, fopen mode strings, and the
 * rejected-candidate writeup for this batch's own "shared root + siblings"
 * class-inventory sweep).
 */

#include "korg_file.h"

#include <cstdlib>
#include <cstring>

/* Bounded strlen -- ground truth is a GCC 8-unrolled compare chain in every
 * one of NameLength()/MakeName()/MakeNameStereo()'s own real bodies;
 * reproduced once here as a shared helper rather than four separate literal
 * unrolled transcriptions (see korg_file.h's own "plain C helper" note).
 */
static unsigned int BoundedNameLen(const char *s, unsigned int maxLen)
{
	unsigned int i = 0;
	while (i < maxLen && s[i] != 0)
		i++;
	return i;
}

/* Trims trailing spaces from s[0..len), nulling them in place. Returns the
 * new length. Shared by MakeName()/MakeNameStereo().
 */
static unsigned int TrimTrailingSpaces(char *s, unsigned int len)
{
	while (len > 0 && s[len - 1] == ' ')
		s[--len] = 0;
	return len;
}

CKorgFile::CKorgFile(const char *name, const char *ext)
{
	mFile = 0;

	strncpy(mExtension, ext, sizeof(mExtension));
	mExtension[sizeof(mExtension) - 1] = 0;

	if (!name) {
		mFileName[0] = 0;
		return;
	}

	strncpy(mFileName, name, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;

	char *dot = strrchr(mFileName, '.');
	if (!dot || strcasecmp(dot, mExtension) != 0) {
		size_t len = strlen(mFileName);
		strncat(mFileName, mExtension, sizeof(mFileName) - len);
		mFileName[sizeof(mFileName) - 1] = 0;
	}
}

CKorgFile::~CKorgFile()
{
}

int CKorgFile::Read()
{
	FILE *f = fopen(mFileName, "r");
	if (!f)
		return -1;
	int result = ImportToBank();
	fclose(f);
	return result;
}

int CKorgFile::Write()
{
	FILE *f = fopen(mFileName, "w");
	if (!f)
		return -1;
	int result = LoadChunk();
	fclose(f);
	return result;
}

void CKorgFile::TransferFromBegin(unsigned int /*startOffset*/)
{
	mFile = fopen(mFileName, "r");
	fseek(mFile, 0, SEEK_SET);
}

void CKorgFile::TransferToBegin(unsigned int /*startOffset*/)
{
	mFile = fopen(mFileName, "r+");
	fseek(mFile, 0, SEEK_SET);
}

int CKorgFile::TransferFrom(void *buf, unsigned int size, unsigned int count)
{
	fread(buf, size, count, mFile);
	return 0;
}

int CKorgFile::TransferTo(const void *buf, unsigned int size, unsigned int count)
{
	fwrite(buf, size, count, mFile);
	return 0;
}

void CKorgFile::TransferFromEnd()
{
	fclose(mFile);
	mFile = 0;
}

void CKorgFile::TransferToEnd()
{
	fclose(mFile);
	mFile = 0;
}

void CKorgFile::SetPath(const char *path)
{
	if (!path) {
		mFileName[0] = 0;
		return;
	}

	strncpy(mFileName, path, sizeof(mFileName));
	mFileName[sizeof(mFileName) - 1] = 0;

	char *dot = strrchr(mFileName, '.');
	if (!dot || strcasecmp(dot, mExtension) != 0) {
		size_t len = strlen(mFileName);
		strncat(mFileName, mExtension, sizeof(mFileName) - len);
		mFileName[sizeof(mFileName) - 1] = 0;
	}
}

const char *CKorgFile::GetPathName() const
{
	const char *slash = strrchr(mFileName, '/');
	return slash ? slash + 1 : mFileName;
}

void CKorgFile::GetPathNameNoExtension(char *dest, unsigned int maxLen) const
{
	const char *slash = strrchr(mFileName, '/');
	const char *src = slash ? slash + 1 : mFileName;

	strncpy(dest, src, maxLen);
	dest[maxLen - 1] = 0;

	char *dot = strrchr(dest, '.');
	if (dot)
		*dot = 0;
}

int CKorgFile::GetFolder(char *dest, unsigned int maxLen)
{
	strncpy(dest, mFileName, maxLen);
	dest[maxLen - 1] = 0;

	char *dot = strrchr(dest, '.');
	if (dot) {
		*dot = 0;
		return 1;
	}
	return 0;
}

int CKorgFile::MakePathFromFolder(char *dest, const char *folder, unsigned int maxLen)
{
	strncpy(dest, mFileName, maxLen);
	dest[maxLen - 1] = 0;

	int hadExt = 0;
	char *dot = strrchr(dest, '.');
	if (dot) {
		*dot = 0;
		hadExt = 1;
	}

	strncat(dest, "/", maxLen - strlen(dest));
	dest[maxLen - 1] = 0;

	strncat(dest, folder, maxLen - strlen(dest));
	dest[maxLen - 1] = 0;

	return hadExt;
}

int CKorgFile::HasExtension(const char *name, const char *ext)
{
	const char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	return strcasecmp(dot, ext) == 0;
}

void CKorgFile::AddExtension(char *name, unsigned int maxLen, const char *ext)
{
	size_t len = strlen(name);
	strncat(name, ext, maxLen - len);
	name[maxLen - 1] = 0;
}

int CKorgFile::RemoveExtension(char *name)
{
	char *dot = strrchr(name, '.');
	if (!dot)
		return 0;
	*dot = 0;
	return 1;
}

int CKorgFile::RemoveExtension(char *name, char *dest, unsigned int maxLen)
{
	dest[0] = 0;

	char *dot = strrchr(name, '.');
	if (!dot)
		return 0;

	unsigned int len = (unsigned int)(dot - name);
	if (len > maxLen)
		len = maxLen;
	strncpy(dest, name, len);
	dest[len] = 0;
	*dot = 0;
	return 1;
}

int CKorgFile::ValidExtension(const char *ext)
{
	return ext != 0 && ext[0] == '.';
}

int CKorgFile::ExtractName(const char *path, char *dest, unsigned int maxLen)
{
	const char *slash = strrchr(path, '/');
	const char *src = slash ? slash + 1 : path;

	strncpy(dest, src, maxLen);
	dest[maxLen - 1] = 0;

	char *dot = strrchr(dest, '.');
	if (dot) {
		*dot = 0;
		return 1;
	}
	return 0;
}

void CKorgFile::Sanitize(char *name)
{
	char *dst = name;
	for (char *src = name; *src != 0; src++) {
		unsigned char c = (unsigned char)*src;
		if (c == '-' || c <= ' ')
			continue;
		*dst++ = *src;
	}
	*dst = 0;
}

int CKorgFile::Capitalized(const char *name)
{
	for (const char *p = name; *p != 0; p++) {
		unsigned char c = (unsigned char)*p;
		if ((unsigned char)(c - 'a') <= 0x19)
			return 0;
	}
	return 1;
}

void CKorgFile::WriteEmptyFile(FILE *file, unsigned int startOffset, unsigned int count)
{
	static const unsigned int kChunk = 4096;
	unsigned char *buf = (unsigned char *)calloc(kChunk, 1);

	unsigned int written = startOffset;
	while (written < count) {
		unsigned int n = count - written;
		if (n > kChunk)
			n = kChunk;
		fwrite(buf, 1, n, file);
		written += n;
	}

	free(buf);
}

unsigned int CKorgFile::NameLength(const char *name, unsigned int maxLen)
{
	if (maxLen == 0 || name[0] == 0)
		return 0;
	return BoundedNameLen(name, maxLen);
}

void CKorgFile::MakeName(const char *name, char *dest, unsigned int maxLen)
{
	strncpy(dest, name, maxLen);

	if (maxLen == 0 || dest[0] == 0)
		return;

	unsigned int len = BoundedNameLen(dest, maxLen);
	TrimTrailingSpaces(dest, len);
}

void CKorgFile::MakeNameStereo(const char *name, char *dest, unsigned int maxLen, char ch)
{
	strncpy(dest, name, maxLen);
	if (maxLen == 0 || dest[0] == 0)
		return;

	unsigned int len = BoundedNameLen(dest, maxLen);

	if (len > 2 && dest[len - 2] == '-' && dest[len - 1] == ch) {
		dest[len - 2] = 0;
		if (dest[0] == 0)
			return;
	}

	len = BoundedNameLen(dest, maxLen);
	len = TrimTrailingSpaces(dest, len);

	if (maxLen == 0 || dest[0] == 0)
		return;

	for (unsigned int i = len; i + 2 < maxLen; i++)
		dest[i] = ' ';

	if (maxLen >= 2) {
		dest[maxLen - 2] = '-';
		dest[maxLen - 1] = ch;
	}
}

void CKorgFile::MakeNameRight(const char *name, char *dest, unsigned int maxLen)
{
	MakeNameStereo(name, dest, maxLen, 'R');
}

void CKorgFile::MakeNameLeft(const char *name, char *dest, unsigned int maxLen)
{
	MakeNameStereo(name, dest, maxLen, 'L');
}

void CKorgFile::MakeFileName(char *name, unsigned int maxLen, const char *ext)
{
	unsigned int nameLen = (unsigned int)strlen(name);
	unsigned int extLen = (unsigned int)strlen(ext);
	unsigned int avail = nameLen - extLen;

	if (avail > maxLen) {
		name[avail] = 0;
		nameLen = (unsigned int)strlen(name);
	}

	strncat(name, ext, maxLen - nameLen);
	name[maxLen - 1] = 0;
}
