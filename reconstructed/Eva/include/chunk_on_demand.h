/*
 * chunk_on_demand.h  -  CChunkOnDemand, a small fixed-size record CResMan embeds 257
 * of (res_man.h). Reconstructed as a dependency of CResMan::CResMan() (Stage 6 breadth
 * sweep, 2026-07-25) -- not the CChunkMan/CChunkOnDemand-adjacent territory another
 * concurrent session is working (that's `CChunkMan` itself, chunk_man.h, a distinct
 * class); this one is CResMan's own, unrelated embedded record type.
 *
 * Real layout confirmed from CChunkOnDemand@0814d080.c (the only reconstructed
 * method, default ctor, 70 bytes) -- 0x20 (32) bytes:
 *   +0x00  mUnknown00  ctor zeroes; real meaning not decoded
 *   +0x04  mSlots      embedded COmegaPtrArray (0x18 bytes), 3-int ctor
 *                       (growBy=2, initialCapacity=4, ownFlag=1), vtable-swapped to
 *                       the real `TPtrArray<CChunkOnDemand::STripletOnDemand>`
 *                       (confirmed via `nm -C`'s own "vtable for
 *                       TPtrArray<CChunkOnDemand::STripletOnDemand>" symbol at
 *                       0x08e88598, +8 = 0x08e885a0, matching this ctor's own
 *                       `PTR__TPtrArray_08e885a0` write exactly) -- `STripletOnDemand`
 *                       itself is not reconstructed, out of scope.
 *   +0x1c  mUnknown1c  ctor zeroes; real meaning not decoded
 *
 * Only the ctor exists in this reconstruction -- CResMan's own 257 embedded instances
 * (res_man.h) are all default-constructed implicitly (array member, no explicit loop
 * needed in CResMan::CResMan() itself), matching the real ctor's own construction
 * order exactly (sequential, index 0 first).
 */

#ifndef CHUNK_ON_DEMAND_H
#define CHUNK_ON_DEMAND_H

#include "omega_ptr_array.h"

class CChunkOnDemand {
public:
	CChunkOnDemand();

private:
	int            mUnknown00;
	COmegaPtrArray mSlots;
	int            mUnknown1c;
};

#endif /* CHUNK_ON_DEMAND_H */
