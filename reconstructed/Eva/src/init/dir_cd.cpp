// SPDX-License-Identifier: GPL-2.0
#include "dir_cd.h"

/* Opaque, deliberately-inert stand-in for the real project-internal
 * out-of-range diagnostic call GetPTRecord()'s own ground-truth body
 * makes (`ds:0x930a1f4`'s vtable slot +0x94, passed a source-file
 * string, an expression string, and a line number) -- see dir_cd.h's
 * header comment. Ground truth's own control flow falls through and
 * computes+returns the pointer regardless of whether this fires, so a
 * no-op here changes nothing observable. */
static void CDirCD_AssertOutOfRange(unsigned int index, unsigned int count)
{
	(void)index;
	(void)count;
}

CDirEntry *CDirCD::GetCurrEntry()
{
	return (CDirEntry *)((unsigned char *)this + 0xd8);
}

unsigned long CDirCD::GetRootHandle() const
{
	const unsigned char *media = *(const unsigned char *const *)((const unsigned char *)this + 0x28);
	unsigned long mode = *(const unsigned long *)(media + 0xa0);
	switch (mode) {
	case 1:
		return *(const unsigned long *)((const unsigned char *)this + 0xc9c);
	case 2:
		return (unsigned long)(*((const unsigned char *)this + 0x511)) +
		       *(const unsigned long *)((const unsigned char *)this + 0x518);
	case 3:
		return *(const unsigned long *)((const unsigned char *)this + 0xca0);
	default:
		return 0;
	}
}

unsigned long CDirCD::GetClusterSizeInSect() const
{
	return 1;
}

unsigned long CDirCD::GetMaxDirEntrySize()
{
	const unsigned char *media = *(const unsigned char *const *)((unsigned char *)this + 0x28);
	unsigned long mode = *(const unsigned long *)(media + 0xa0);
	return mode == 4 ? 0x40 : 0;
}

unsigned long CDirCD::GetNumAkaiPartition() const
{
	const unsigned char *begin = *(const unsigned char *const *)((const unsigned char *)this + 0xca8);
	const unsigned char *end = *(const unsigned char *const *)((const unsigned char *)this + 0xcac);
	return (unsigned long)(end - begin) / sizeof(SAkaiPartition);
}

void CDirCD::SetError(int notify)
{
	unsigned char *base = (unsigned char *)this;
	int code;
	switch (notify) {
	case 0: code = 0; break;
	case 2: code = 8; break;
	case 1: code = 0xa; break;
	default: code = (notify != 5) ? 0xd : 0xc; break;
	}
	*(int *)(base + 0x50) = code;
}

void CDirCD::ResetBufferedEntries()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned long *)(base + 0xcb8) = 0xffffffffUL;
	*(unsigned long *)(base + 0xcbc) = 0;
}

unsigned long CDirCD::GetTotalSectors() const
{
	const unsigned char *base = (const unsigned char *)this;
	const unsigned char *media = *(const unsigned char *const *)(base + 0x28);
	unsigned long mode = *(const unsigned long *)(media + 0xa0);
	if (mode == 4)
		return *(const unsigned long *)(base + 0x144);

	unsigned char count = base[0x14b];
	if (count == 0)
		return 0;

	unsigned long total = 0;
	for (unsigned int i = 0; i < count; ++i) {
		const unsigned char *rec = base + 0x14c + i * 8;
		unsigned short value = (unsigned short)(rec[0] | (rec[3] << 8));
		total += (unsigned long)value * 75;
	}
	return total;
}

bool CDirCD::FindPartition(unsigned long id, const SAkaiPartition *&out) const
{
	const unsigned char *base = (const unsigned char *)this;
	const unsigned char *begin = *(const unsigned char *const *)(base + 0xca8);
	const unsigned char *end = *(const unsigned char *const *)(base + 0xcac);

	out = 0;
	for (const unsigned char *p = begin; p < end; p += sizeof(SAkaiPartition)) {
		out = (const SAkaiPartition *)p;
		if (((const SAkaiPartition *)p)->id == id)
			return true;
	}
	return false;
}

const CDirCD::PTRecord *CDirCD::GetPTRecord(unsigned int index) const
{
	const unsigned char *base = (const unsigned char *)this;
	unsigned long count = *(const unsigned long *)(base + 0xc98);
	if (count <= index)
		CDirCD_AssertOutOfRange(index, count);

	const unsigned char *records = *(const unsigned char *const *)(base + 0xc94);
	return (const PTRecord *)(records + (unsigned long)index * sizeof(PTRecord));
}
