/*
 * stg_handle.cpp  -  CSTGHandle::Access(), see include/eva_types.h.
 *
 * Transcribed from Access@08e31e80.c (356 bytes) -- confirms Eva's shared-memory
 * attach mechanism: `/proc/.shm` opened, two ioctls (100 / 0x65 -- presumably "get
 * offset" and "get size") against the handle's own mode/id field, then mmap()'d
 * MAP_SHARED (0x2001 == MAP_SHARED|0x2000, a real non-standard flag bit combination
 * preserved literally rather than normalized to MAP_SHARED alone) at the page-aligned
 * offset. Results cached per mode/id in a real 1.2MB fixed-size table
 * (CSTGHandleCache::sCachedHandleInfo, 100000 entries x 12 bytes, refcounted) so
 * repeat Access() calls for the same id are cheap.
 *
 * This is the function `USTGUserAPI::Connect()` calls twice (mSharedMem =
 * localHandle.Access(); mFrontPanelStatusAddress = mSharedMem->Access()) -- the
 * second call passes the *pointer value* Access() returned as if it were itself a
 * CSTGHandle (i.e. as a mode/id, since CSTGHandle's only field is that one int) --
 * faithfully preserved, not resolved further (see eva_types.h).
 */

#include "eva_types.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

/* CSTGHandleCache -- real class name (symbols.csv), kept file-local since nothing
 * else in this reconstruction touches it directly.
 */
struct CachedHandleEntry {
	short refCount;
	short pad;
	void *addr;
	int   size;
};

void *sCachedHandleInfo = 0;

void CSTGHandleCache_Initialize()
{
	if (sCachedHandleInfo != 0)
		return;
	sCachedHandleInfo = malloc(1200000);
	memset(sCachedHandleInfo, 0, 1200000);
}

} // namespace

void *CSTGHandle::Access() const
{
	if (sCachedHandleInfo == 0)
		CSTGHandleCache_Initialize();

	if ((int)mode == -1)
		return 0;

	CachedHandleEntry *entry = (CachedHandleEntry *)((char *)sCachedHandleInfo + (int)mode * 0xc);

	if (entry->refCount == -1)
		return 0;

	entry->refCount++;
	if (entry->refCount != 1) {
		/* Already open -- real code re-derives `entry` here (harmless, same
		 * pointer); return the cached mapping.
		 */
		return entry->addr;
	}

	int fd = open("/proc/.shm", O_RDWR);
	if (fd < 0) {
		puts("failed to open /proc/.shm");
		return 0;
	}

	unsigned offset = (unsigned)ioctl(fd, 100, (int)mode);
	int size = ioctl(fd, 0x65, (int)mode);
	void *result = 0;
	if (size != 0) {
		unsigned pageOff = offset & 0xfffff000u;
		unsigned inPage = offset - pageOff;
		void *mapped = mmap(0, inPage + size, PROT_READ | PROT_WRITE, 0x2001, fd, pageOff);
		if (mapped == (void *)-1) {
			printf("mmap failed for size %lu\n", (unsigned long)size);
		} else {
			result = (char *)mapped + inPage;
			entry->addr = result;
			entry->size = size;
		}
	}

	close(fd);
	return result;
}
