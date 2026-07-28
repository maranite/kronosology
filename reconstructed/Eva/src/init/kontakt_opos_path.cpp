/*
 * kontakt_opos_path.cpp  -  UKontaktOposPath. See include/kontakt_opos_path.h
 * for full ground-truth provenance. Ground truth's own copy/translate loops
 * are GCC-unrolled-by-4 with interleaved bounds checks; reproduced here as
 * plain bounded loops (same "plain C loop, not literal unrolled
 * transcription" convention used project-wide, e.g. korg_file.h) -- the
 * observable behavior (translate one separator class to the other, stop at
 * source NUL or dest capacity) is identical either way.
 */

#include "kontakt_opos_path.h"

#include "long_binary_file.h"	/* CFileOperation::GetLinuxRemapPath() */

#include <cstring>

namespace UKontaktOposPath {

int ConvertOposToLinux(const char *oposPath, char *dest, unsigned int maxLen)
{
	dest[0] = 0;

	if (oposPath[1] != ':')
		return 0;

	unsigned char drive = (unsigned char)oposPath[0];
	if ((unsigned char)(drive - 'A') > 7)
		return 0;

	const char *mount = CFileOperation::GetLinuxRemapPath((EDevice_Id)(drive - 'A'));
	strncpy(dest, mount, maxLen);

	unsigned int pos = strlen(dest);
	const char *src = oposPath + 2;
	while (*src != 0 && pos + 1 < maxLen) {
		char c = *src++;
		dest[pos++] = (c == '\\') ? '/' : c;
	}
	dest[pos] = 0;
	return 1;
}

int ConvertLinuxToOpos(const char *linuxPath, char *dest, unsigned int maxLen)
{
	dest[0] = 0;

	for (int id = 0; id < 8; id++) {
		const char *mount = CFileOperation::GetLinuxRemapPath((EDevice_Id)id);
		unsigned int mountLen = strlen(mount);
		if (strncmp(linuxPath, mount, mountLen) != 0)
			continue;

		if (maxLen < 3) {
			dest[0] = 0;
			return 1;
		}
		dest[0] = (char)('A' + id);
		dest[1] = ':';

		unsigned int pos = 2;
		const char *src = linuxPath + mountLen;
		while (*src != 0 && pos + 1 < maxLen) {
			char c = *src++;
			dest[pos++] = (c == '/') ? '\\' : c;
		}
		dest[pos] = 0;
		return 1;
	}

	return 0;
}

} /* namespace UKontaktOposPath */
