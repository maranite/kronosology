/*
 * cvalue.cpp  -  the 2 decoded CValue serialization rules, see include/eva_types.h.
 *
 * Both rules were confirmed independently against real disassembly of
 * USTGAPIKLM::GetProductInfo() (scalar dword) and USTGAPIKLM::GetProductItemInfo()
 * (variable-length blob), 2026-07-27 -- not new guesses, just the first time this
 * project has needed to actually perform either operation rather than merely note
 * that CValue exists.
 */

#include "eva_types.h"

#include <cstring>

void WriteCValueDword(CValue &dest, unsigned int value)
{
	dest.raw[0] = 1; /* tag */
	dest.raw[1] = 4; /* length-in-bytes of the payload that follows */
	dest.raw[2] = 0;
	dest.raw[3] = 0;
	memcpy(&dest.raw[4], &value, sizeof(value));
}

void CopyCValueBlob(CValue &dest, const void *src)
{
	const unsigned char *s = static_cast<const unsigned char *>(src);
	size_t len = static_cast<size_t>(s[1]) + 4;
	memcpy(&dest, src, len);
}
