/*
 * res_table.cpp  -  see include/res_table.h.
 */

#include "res_table.h"
#include "res_family.h"
#include "system_api.h"

extern CSystemApi *Api; /* mains.cpp */

namespace {

inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

} // namespace

bool IsOnDemand(unsigned int family)
{
	if (family > 0x1f)
		ApiAssert("ResTable.cpp", 0x4df);
	return *reinterpret_cast<int *>(
		reinterpret_cast<unsigned char *>(&g_atResFamilies[family]) + 0x24) == 0;
}
