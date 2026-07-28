/*
 * korg_linux_path.cpp  -  CKorgLinuxPath. See include/korg_linux_path.h and
 * korg_path.h for full ground-truth provenance.
 */

#include "korg_linux_path.h"
#include "kontakt_opos_path.h"

#include <cstring>
#include <cstdlib>
#include <dirent.h>

CKorgLinuxPath::CKorgLinuxPath(const char *name)
	: CKorgPath(name)
{
}

CKorgLinuxPath::CKorgLinuxPath(const CKorgPath *other)
	: CKorgPath(other)
{
}

CKorgLinuxPath::~CKorgLinuxPath()
{
}

int CKorgLinuxPath::Valid(const char *name)
{
	if (strcmp(name, ".") == 0)
		return 0;
	if (strcmp(name, "..") == 0)
		return 0;
	return name[0] != '.';
}

CKorgPath *CKorgLinuxPath::Copy() const
{
	return new CKorgLinuxPath(this);
}

char CKorgLinuxPath::Separator() const
{
	return '/';
}

void CKorgLinuxPath::GetOposPath(char *dest, unsigned int maxLen)
{
	UKontaktOposPath::ConvertLinuxToOpos(mFileName, dest, maxLen);
}

void CKorgLinuxPath::SetOposPath(const char *path)
{
	char linuxPath[0x100];
	UKontaktOposPath::ConvertOposToLinux(path, linuxPath, sizeof(linuxPath));
	SetPath(linuxPath);
}

CKorgPath *CKorgLinuxPath::TemporaryFileUsingExtension(const char *ext) const
{
	CKorgLinuxPath *result = new CKorgLinuxPath((const char *)0);

	char buf[0x100];
	strncpy(buf, "/tmp", sizeof(buf));
	buf[sizeof(buf) - 1] = 0;

	char sep[2];
	sep[0] = Separator();
	sep[1] = 0;
	strncat(buf, sep, sizeof(buf) - strlen(buf));
	buf[sizeof(buf) - 1] = 0;

	const char *name = GetPathName();
	if (name) {
		strncat(buf, name, sizeof(buf) - strlen(buf));
		buf[sizeof(buf) - 1] = 0;
		char *dot = strrchr(buf, '.');
		if (dot)
			*dot = 0;
	}

	if (ext) {
		strncat(buf, ext, sizeof(buf) - strlen(buf));
		buf[sizeof(buf) - 1] = 0;
	}

	const char *ownExt = GetPathExtension();
	if (ownExt) {
		strncat(buf, ownExt, sizeof(buf) - strlen(buf));
		buf[sizeof(buf) - 1] = 0;
	}

	result->SetPath(buf);
	return result;
}

CKorgPath *CKorgLinuxPath::FindRecurse(const char *name, const CKorgPath *dir) const
{
	char dirPath[0x100];
	dir->GetPath(dirPath, sizeof(dirPath));

	DIR *d = opendir(dirPath);
	if (!d)
		return 0;

	char sep[2];
	sep[0] = Separator();
	sep[1] = 0;

	CKorgPath *found = 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != 0) {
		if (ent->d_type == DT_REG) {
			if (!Valid(ent->d_name))
				continue;
			if (strcmp(ent->d_name, name) != 0)
				continue;

			closedir(d);
			CKorgLinuxPath *result = new CKorgLinuxPath((const char *)0);
			result->Set(dir, name);
			return result;
		} else if (ent->d_type == DT_DIR) {
			if (!Valid(ent->d_name))
				continue;

			char subPath[0x100];
			strncpy(subPath, dirPath, sizeof(subPath));
			subPath[sizeof(subPath) - 1] = 0;
			strncat(subPath, sep, sizeof(subPath) - strlen(subPath));
			subPath[sizeof(subPath) - 1] = 0;
			strncat(subPath, ent->d_name, sizeof(subPath) - strlen(subPath));
			subPath[sizeof(subPath) - 1] = 0;

			CKorgLinuxPath subdir(subPath);
			CKorgPath *result = FindRecurse(name, &subdir);
			if (result) {
				closedir(d);
				return result;
			}
		}
	}

	closedir(d);
	return found;
}
