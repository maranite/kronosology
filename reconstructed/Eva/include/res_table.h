/*
 * res_table.h  -  IsOnDemand(), a free function from Eva's real "ResTable.cpp"
 * translation unit (confirmed by its own Api+0x94 soft-assert call site's
 * literal filename string). Round 55 batch (2026-07-29, solo) -- found via
 * CResMan::UnloadAllRes()'s own call site while surveying CFileMan/CResMan's
 * remaining pending methods; UnloadAllRes() itself stays deferred (its other
 * callee, LRUUnloadRes(), is a genuinely deep 1715-byte resource-GC routine,
 * out of scope), but IsOnDemand() is real and fully self-contained on its own.
 *
 * .text+0x08068b20, 78 bytes. Doesn't touch any object -- reads
 * g_atResFamilies[family]'s own +0x24 field directly (res_family.h's usual
 * opaque raw-offset-access convention; this is the method that gives that
 * +0x24 field its "on-demand loading" meaning). Real: out-of-range family
 * (>=32) fires a soft assert (Api+0x94) then reads anyway (ground truth falls
 * through, not an early return -- same "assert-then-continue" idiom as
 * CResMan::SetLoadRes()/res_entry.h's CResInfo(const unsigned char*)).
 */

#ifndef RES_TABLE_H
#define RES_TABLE_H

bool IsOnDemand(unsigned int family);

#endif /* RES_TABLE_H */
