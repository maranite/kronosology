/*
 * batch_disk_main_task.h  -  Tier-B substitute for the real, out-of-scope
 * CBatchDiskMainTask (see batch_disk_man.h's own header comment for the full
 * CBatchDiskMan-unlock writeup, Eva Stage 6, 2026-07-26). Same "deliberately
 * simplified substitute" convention es_common.h already established for
 * CESCommonTask (that file's own header comment) -- reused verbatim, not
 * reinvented.
 *
 * WHY THE REAL CLASS IS OUT OF SCOPE (confirmed via `objdump -dr -M intel`
 * against `/home/share/Decomp/EVA_Decomp/Eva`, not assumed from size alone):
 * `CBatchDiskMainTask::CBatchDiskMainTask(CModule const&, char const*)`
 * (.text+0x08241920, 450 bytes) is a genuine CTask + CEditable + CRMApiCallBack
 * triple-inheritance class (vtable-pointer stores at this+0/this+8/this+0x80,
 * confirmed via the real ctor's own 3 `mov [ebx+N], <vtable>` stores and the
 * dtor's matching `_ZThn8_`/`_ZThn128_` this-adjustment thunks) that directly
 * placement-constructs a real `CZ` member (`lea eax,[ebx+0x14c]; call
 * CZ::CZ(eax, 1)`, .text+0x080ba5f0) -- the SAME `CZ` string-set CONTAINER
 * class (247 methods per `nm -C`) `config_manager.h`'s own
 * `CreateResourceFamilies()` header comment and `cz_util.h`'s own header
 * comment already, independently, flag as project-wide out of scope
 * ("genuinely deep, out of proportion with this batch's scope"). The same
 * ctor also constructs a `CDirEntry` member (0x0807ee80... no,
 * `CDirEntry::CDirEntry()`, .text+0x08071640, 231 bytes) and a heap `CRMJob`
 * (.text+0x081660d0, 150 bytes) and a heap `COutLinkMulti`
 * (.text+0x0807d620, 70 bytes) -- none of these 4 supporting classes exist
 * anywhere in this reconstruction yet either. Its own 4 heaviest methods
 * (`PreloadDir` 2940B, `PreloadGroup` 1148B, `PrepareGroupsForPreload` 1336B,
 * `AddItemToPreload` 359B, plus `Exec(CMessage&)` 703B) are themselves the
 * real "CZ-driven directory-scan/group-parsing" business logic -- the same
 * CChunkServer/CTimerEngine-scale, multi-hour dedicated-batch effort
 * [[eva_mopup_sweep_2026-07-26_negative]] already sized this class at before
 * this batch started. Two further sibling classes, `CBatchDiskCmds`
 * (embeds `TVector<CBatchDiskCmds::SGroupElem,1>`, whose own `Insert()`
 * alone is 4290 bytes) and `CBatchDiskSignals`, are also real ground-truth
 * dependencies of the FULL class that this substitute does not attempt.
 *
 * WHAT THIS SUBSTITUTE DOES INSTEAD: a real, working `CTask`-derived class
 * (CTask's own already-reconstructed 5-arg ctor, task.h) that satisfies
 * `CBatchDiskMan::Setup()`'s own real `CModule::Add(CTask*)` contract (the
 * SAME contract CEditTask -- fully real, edit_task.h -- and CESCommonTask
 * both already satisfy this way), plus the 2 trivial boolean getters
 * `CBatchDiskMan::IsBusy()`/`IsPreloadRunning()` (batch_disk_man.h) forward
 * to. `mBusy`/`mPreloadRunning` are this substitute's OWN invented state
 * (never set true by anything -- the real state-setting logic lives inside
 * the deferred `PreloadDir`/`AddItemToPreload`/`Exec()` bodies above), NOT a
 * transcription of the real `+0xd8` field ground truth's own `IsBusy()`/
 * `IsPreloadRunning()` read -- same "doesn't change this pass's own control
 * flow" license already used for CESCommonTask's own `level`/`scheduleFlag`
 * placeholders.
 *
 * `PrepareGroupsForPreload()` is real ground truth's OWN ctor-time call
 * (`CBatchDiskMainTask::CBatchDiskMainTask()` calls it directly, unconditionally,
 * passing the ctor's own 2nd argument) -- modeled here as a stubbed no-op
 * method with the real signature declared, matching the project's
 * established "declare real signature, stub body" convention for exactly
 * this situation (e.g. `CPoller::InitButtons()`/`InitAnalogs()` before this
 * same convention resolved them, poller.h's own prior header comment).
 */

#ifndef BATCH_DISK_MAIN_TASK_H
#define BATCH_DISK_MAIN_TASK_H

#include "task.h"

class CBatchDiskMainTask : public CTask {
public:
	CBatchDiskMainTask(const CModule &owner, const char *preloadList)
		: CTask(owner, "BatchDiskMainTask", 0, 0, 0),
		  mBusy(false), mPreloadRunning(false)
	{
		PrepareGroupsForPreload(preloadList);
	}

	/* Real signature (CBatchDiskMainTask::PrepareGroupsForPreload(char
	 * const*), .text+0x08241340, 1336 bytes) -- Tier-B stub, see header
	 * comment. Real body parses a ';'-separated resource-family list (the
	 * ctor's own 2nd argument, e.g. "EditResources.Unlocalized;
	 * EditResources.Localized.ENG" for CBatchDiskMan's own real call site,
	 * batch_disk_man.h) via `CZ` -- out of scope.
	 */
	void PrepareGroupsForPreload(const char * /*groupList*/) {}

	bool IsBusy() const { return mBusy; }
	bool IsPreloadRunning() const { return mPreloadRunning; }

private:
	bool mBusy;
	bool mPreloadRunning;
};

#endif /* BATCH_DISK_MAIN_TASK_H */
