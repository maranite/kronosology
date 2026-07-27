/*
 * bd_api_instance.cpp  -  see include/bd_api_instance.h.
 */

#include "bd_api_instance.h"
#include "batch_disk_man.h"

#include <cstdlib>
#include <cstring>

CBDApiInstance BDApiInstance;

/* TVector<CBatchDiskMan*,1>::MakeCapacity(unsigned int), .text+0x082456d0, 536
 * bytes -- see bd_api_instance.h header comment. Ground truth passes `this+4`
 * (i.e. `&mLoaders`' own leading slot) as its own `tvec` and operates on
 * `tvec+4`/`tvec+8`/`tvec+0xc` as begin/end/cap via raw pointer arithmetic;
 * reproduced here operating directly on CBDApiInstance's own typed
 * mBegin/mEnd/mCap fields instead (same values, no raw-offset aliasing
 * needed since this reconstruction already has real typed members for them).
 * Real growth policy (confirmed from THIS instantiation's own disassembly,
 * not assumed by analogy to TVector<CTask::SRegisteredIfc,1>'s different
 * curve): minimum 0x20 (32) elements, doubling thereafter until `n` fits.
 */
static void TVector_CBatchDiskManPtr_MakeCapacity(CBatchDiskMan ***beginField,
                                                    CBatchDiskMan ***endField,
                                                    CBatchDiskMan ***capField,
                                                    unsigned int n)
{
	const unsigned int kElemSize = sizeof(CBatchDiskMan *);

	unsigned int curCapElems = (unsigned int)(*capField - *beginField);
	if (n <= curCapElems)
		return;

	unsigned int newCapElems = 0x20;
	while (n > newCapElems)
		newCapElems *= 2;
	unsigned int newBytes = newCapElems * kElemSize;

	/* Real: HAL_DisableInterrupts()/HAL_EnableInterrupts() bracket both malloc
	 * and free below -- not modeled, same established convention as every
	 * other HAL_* bracket in this project.
	 */
	CBatchDiskMan **newBlock = (CBatchDiskMan **)malloc(newBytes);

	CBatchDiskMan **oldBegin = *beginField;
	CBatchDiskMan **oldEnd   = *endField;
	unsigned int copyElems = 0;
	if (oldEnd != oldBegin) {
		copyElems = (unsigned int)(oldEnd - oldBegin);
		/* Real: GCC's own Duff's-device-unrolled (x8, mod-8) 4-byte-element
		 * copy loop -- collapsed to memcpy, identical result (CBatchDiskMan*
		 * is trivially-copyable POD, no side effects to preserve).
		 */
		memcpy(newBlock, oldBegin, copyElems * kElemSize);
	}

	*endField = newBlock + copyElems;
	free(oldBegin);
	*beginField = newBlock;
	*capField = newBlock + newCapElems;
}

int CBDApiInstance::RegisterLoader(CBatchDiskMan *loader)
{
	if (loader == 0)
		return -1;

	if (mEnd == mCap) {
		unsigned int used = (unsigned int)(mEnd - mBegin);
		TVector_CBatchDiskManPtr_MakeCapacity(&mBegin, &mEnd, &mCap, used + 1);
	}

	*mEnd = loader;
	++mEnd;

	return (int)(mEnd - mBegin);
}

/* Real: soft Api+0x94 diagnostic if mLoaders.size() != 1, then an unconditional,
 * unchecked dereference of mBegin[0] -- see header comment.
 */
bool CBDApiInstance::IsBusy() const
{
	return mBegin[0]->IsBusy();
}

bool CBDApiInstance::IsPreloadRunning() const
{
	return mBegin[0]->IsPreloadRunning();
}

bool CBDApiInstance::IsPreloadRunning(unsigned char group, const char *name) const
{
	return mBegin[0]->IsPreloadRunning(group, name);
}
