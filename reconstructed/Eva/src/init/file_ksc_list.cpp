// SPDX-License-Identifier: GPL-2.0
#include "file_ksc_list.h"
#include "system_api.h"

/* Real module-scope global (mains.cpp), same extern already used by
 * config_manager.cpp's own FMApi vtable wrappers. */
extern CSystemApi *FMApi;

namespace {

/* FMApi's own vtable slot +0x1bc ("read a positional field from an open
 * record handle") -- same raw-vtable-dispatch idiom as config_manager.cpp's
 * FMApiGetDriverFactory/FMApiRegisterDriver. `len` is an in/out max-length;
 * real callers always preset it before calling. */
inline int FMApiReadField(void *handle, void *buf, unsigned int *len)
{
	typedef int (*Fn)(void *, void *, void *, unsigned int *);
	void *vtbl = *(void **)FMApi;
	Fn fn = *(Fn *)((char *)vtbl + 0x1bc);
	return fn(FMApi, handle, buf, len);
}

/* FMApi's own vtable slot +0x1c0 ("write a positional field"). */
inline int FMApiWriteField(void *handle, const void *buf, unsigned int *len)
{
	typedef int (*Fn)(void *, void *, const void *, unsigned int *);
	void *vtbl = *(void **)FMApi;
	Fn fn = *(Fn *)((char *)vtbl + 0x1c0);
	return fn(FMApi, handle, buf, len);
}

} // namespace

/* ==== "string" fields: direct buffer, bool(rc==1) return ==== */

bool CFileKscList::ReadVendorId(char *out)
{
	unsigned int len = 8;
	return FMApiReadField(mHandle, out, &len) == 1;
}

bool CFileKscList::SaveVendorId(const char *in)
{
	unsigned int len = 8;
	return FMApiWriteField(mHandle, in, &len) == 1;
}

bool CFileKscList::ReadProductId(char *out)
{
	unsigned int len = 0x10;
	return FMApiReadField(mHandle, out, &len) == 1;
}

bool CFileKscList::SaveProductId(const char *in)
{
	unsigned int len = 0x10;
	return FMApiWriteField(mHandle, in, &len) == 1;
}

bool CFileKscList::ReadSerialNumber(char *out)
{
	unsigned int len = 0x80;
	return FMApiReadField(mHandle, out, &len) == 1;
}

bool CFileKscList::SaveSerialNumber(const char *in)
{
	unsigned int len = 0x80;
	return FMApiWriteField(mHandle, in, &len) == 1;
}

/* ==== "byte" fields: len=2 scratch, byte[1] is the real value ==== */

bool CFileKscList::ReadAutoLoad(char *out)
{
	unsigned int len = 2;
	char scratch[2];
	bool ok = FMApiReadField(mHandle, scratch, &len) == 1;
	*out = scratch[1];
	return ok;
}

bool CFileKscList::SaveAutoLoad(const char *in)
{
	unsigned int len = 2;
	char scratch[2];
	scratch[0] = 0;
	scratch[1] = *in;
	return FMApiWriteField(mHandle, scratch, &len) == 1;
}

bool CFileKscList::ReadBitDepth(char *out)
{
	unsigned int len = 2;
	char scratch[2];
	bool ok = FMApiReadField(mHandle, scratch, &len) == 1;
	*out = scratch[1];
	return ok;
}

bool CFileKscList::SaveBitDepth(const char *in)
{
	unsigned int len = 2;
	char scratch[2];
	scratch[0] = 0;
	scratch[1] = *in;
	return FMApiWriteField(mHandle, scratch, &len) == 1;
}

bool CFileKscList::ReadLoadMethod(char *out)
{
	unsigned int len = 2;
	char scratch[2];
	bool ok = FMApiReadField(mHandle, scratch, &len) == 1;
	*out = scratch[1];
	return ok;
}

bool CFileKscList::SaveLoadMethod(const char *in)
{
	unsigned int len = 2;
	char scratch[2];
	scratch[0] = 0;
	scratch[1] = *in;
	return FMApiWriteField(mHandle, scratch, &len) == 1;
}

/* ==== record-format framing ====
 *
 * ReadHeaderId()/SaveHeaderId() read/write a fixed 4-byte magic ("#KSC",
 * confirmed via a direct .rodata byte dump at 0x8ef2e23); a real string
 * literal in ground truth, not a local buffer, is compared/sent.
 * ReadDot()/WriteDot() read/write a literal "\r\n" (2 bytes, confirmed at
 * 0x8ef2e20) -- the record-delimiter between fields, real length taken via
 * a real strlen() call on the literal in ground truth (reproduced here as
 * the equivalent constant 2, not re-derived via strlen at runtime, since
 * the literal never varies).
 */

bool CFileKscList::ReadHeaderId()
{
	unsigned int len = 4;
	char scratch[4];
	if (FMApiReadField(mHandle, scratch, &len) != 1)
		return false;
	return scratch[0] == '#' && scratch[1] == 'K' && scratch[2] == 'S' && scratch[3] == 'C';
}

bool CFileKscList::SaveHeaderId()
{
	unsigned int len = 4;
	return FMApiWriteField(mHandle, "#KSC", &len) == 1;
}

bool CFileKscList::ReadDot()
{
	unsigned int len = 2;
	char scratch[2];
	if (FMApiReadField(mHandle, scratch, &len) != 1)
		return false;
	return scratch[0] == '\r' && scratch[1] == '\n';
}

bool CFileKscList::WriteDot()
{
	unsigned int len = 2;
	return FMApiWriteField(mHandle, "\r\n", &len) == 1;
}
