/*
 * cz_util.cpp  -  see include/cz_util.h.
 */

#include "cz_util.h"

#include <cctype>

unsigned CZ::StrCmpIgnoreCase(const char *a, const char *b)
{
	if (a == 0 && b == 0)
		return 0;
	if (a == 0)
		return 0xffffffff; /* NULL sorts before any real string */
	if (b == 0)
		return 1;

	for (;;) {
		unsigned char ca = (unsigned char)*a;
		if (ca == 0)
			break;
		if (*b == 0)
			return 1; /* a longer than b */

		if ((signed char)ca >= 0)
			ca = (unsigned char)toupper(ca);
		unsigned char cb = (unsigned char)*b;
		if ((signed char)cb >= 0)
			cb = (unsigned char)toupper(cb);

		if (ca < cb)
			return 0xffffffff;
		if (cb < ca)
			return 1;

		a++;
		b++;
	}

	return (*b == 0) ? 0 : 0xffffffff; /* a shorter than (a prefix of) b */
}
