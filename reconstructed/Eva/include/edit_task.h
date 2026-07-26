/*
 * edit_task.h  -  CEditTask, one of the 2 CTask-derived children
 * `CBatchDiskMan::Setup()` (batch_disk_man.h) constructs -- see that file's own
 * header comment for the full CBatchDiskMan-unlock writeup (Eva Stage 6,
 * 2026-07-26). Unlike its sibling `CBatchDiskMainTask` (batch_disk_main_task.h,
 * genuinely blocked on the already-project-wide-out-of-scope `CZ` string-set
 * container, see that file's header), CEditTask is FULLY tractable: every real
 * dependency it touches (CTask, CEditable, COutLinkMono) was already
 * reconstructed by prior batches, confirmed via nm -C/objdump -dr before writing
 * this file, not assumed from size alone.
 *
 * REAL CLASS SHAPE (CEditTask : public CTask, public CEditable -- confirmed via
 * `nm -C`/`objdump -dr -M intel` against `/home/share/Decomp/EVA_Decomp/Eva`,
 * CEditTask@08243b80.c/~CEditTask@08243af0.c,08243b20.c/DoPreload@08243ac0.c/
 * GetOutLinkName@08243ca0.c):
 *   +0x00..0x7c  CTask base subobject (task.h) -- primary vtable group,
 *                install addr 0x08eac1c8 (vtable for CEditTask = 0x08eac1c0,
 *                +8 = the usual install-addr convention every per-instance
 *                vtable in this project uses)
 *   +0x7c..0x80  CEditable base subobject (editable.h, 4 bytes, no vtable of
 *                its own) -- SECONDARY vtable group install addr is
 *                0x08eac1e4 (CTask's own polymorphic identity is what's
 *                actually installed there; CEditable itself never has a
 *                vtable slot -- this project's own `_ZThn8_...` thunk pair on
 *                CEditTask's dtor confirms the byte-8 this-adjustment matches
 *                CTask's OWN embedded secondary-group convention, same shape
 *                already established for every other CTask+CEditable
 *                double-inheritance class in this project, e.g.
 *                CAlphaKeybIfcTask, alpha_keyb_ifc_task.h)
 *   +0x84  mOutLink  a COutLinkMono* member (out_link.h, already real),
 *                malloc(0x38)'d and constructed via COutLinkMono's own real
 *                4-arg ctor. Ground truth's own ctor immediately overwrites
 *                the freshly-installed COutLinkMono vtable pointer with
 *                `vtable for CBatchDiskCmds + 8` (0x08eac228, confirmed via
 *                direct nm/rodata read) -- i.e. the REAL runtime type of this
 *                field is `CBatchDiskCmds*` (a COutLinkMono-derived class),
 *                not plain COutLinkMono. CBatchDiskCmds itself embeds a
 *                `TVector<CBatchDiskCmds::SGroupElem,1>` whose own
 *                `Insert()` alone is 4290 bytes (functions.csv) -- the same
 *                CZ-adjacent, out-of-proportion scale as CBatchDiskMainTask's
 *                own deferred pieces (batch_disk_main_task.h). Modeled here
 *                as a plain, real COutLinkMono instead (deliberate
 *                simplification, not a reconstruction bug): every real
 *                caller in this reconstruction (DoPreload(), GetOutLinkName())
 *                only ever touches COutLinkMono's OWN already-real,
 *                non-virtual members (`OutMono(unsigned short, unsigned
 *                long)`/`mLink` at +4, both out_link.h), never anything
 *                CBatchDiskCmds itself would add -- so this simplification
 *                doesn't change either real caller's observable behavior.
 *
 * `CEditTask::CEditTask(const CModule &owner)` (.text+0x08243b80, 228 bytes):
 * `CTask::CTask(this, owner, "EditTask", level=4, scheduleFlag=0,
 * lastArg=0x804b)` (immediates confirmed via direct .rodata read at
 * 0x08eabe9e), `CEditable::CEditable(this+0x7c, (CEditServer*)((char*)&owner +
 * 0x2c))` -- the same raw "+0x2c to reach the CEditServer subobject" trick
 * `CBatchDiskMainTask`'s own real ctor also uses (batch_disk_main_task.h),
 * relying on the caller contract that `owner` is really a `CModule` subobject
 * embedded inside a `CBatchDiskMan` (CModule+CEditServer at a fixed +0x2c
 * relative offset, batch_disk_man.h) -- CEditTask's own real ctor signature
 * genuinely only takes `const CModule&`, this is ground truth's own unchecked
 * pointer arithmetic, not an invention of this pass. Then own vtable install,
 * `malloc(0x38)` + `COutLinkMono::COutLinkMono(*this, "Internal",
 * COutLink::eDirectionOut (immediate 0), 0x8030)` (name immediate confirmed at
 * 0x08eabea7), `CTask::Add(outlink)`, `CEditable::AddDescriptorsMap((CObjectBase*)
 * this, &descCEditTask, false)`.
 *
 * `descCEditTask` (.data+0x091b75a0) is real, ground-truth data this pass did
 * NOT transcribe (its own SDescriptor row contents were never read -- only its
 * address is known). Modeled here as a single real, well-formed
 * sentinel-only SDescriptor[1] (`{.group = 0xff}`, edit_server.h's own
 * documented terminator convention) -- AddDescriptorsMap() stops at the first
 * `group==0xff` entry, so this registers zero real descriptor rows instead of
 * ground truth's real (unknown) set. Same "safe, well-defined placeholder,
 * not a claim about the real content" license already used throughout this
 * project (e.g. LookupResourceStub, GetFMApiStub).
 *
 * `CEditTask::~CEditTask()` (.text+0x08243af0, 30 bytes / deleting variant
 * 08243b20, 54 bytes): re-installs both vtable pointers, tail-calls
 * `CTask::~CTask()` (already real, task.h) -- `mOutLink` is never explicitly
 * freed in the real dtor body either (matches ground truth exactly, not a
 * gap).
 *
 * `CEditTask::DoPreload()` (.text+0x08243ac0, 43 bytes): real, literal
 * `mOutLink->OutMono(1, 0)` -- ecb=1, value=0 (both immediates confirmed via
 * direct disassembly read, meaning of the ecb id itself not decoded).
 *
 * `CEditTask::GetOutLinkName() const` (.text+0x08243ca0, 14 bytes): real,
 * literal `return *(char**)(mOutLink + 4)` -- i.e. `mOutLink`'s own inherited
 * `COutLink::mName` field (out_link.h, +0x04), NOT the `mLink` member (a
 * different field, always 0 per out_link.h's own header comment). Exposed
 * here via a new trivial `COutLink::GetName()` public accessor (out_link.h)
 * rather than reaching around that class's own protected `mName`.
 *
 * Real per-instance vtables (0x08eac1c0/+8=install 0x08eac1c8 primary,
 * +0x24=install 0x08eac1e4 secondary) declared EvaVTableStub-backed in
 * omega_vtables.h/.cpp, same "install-only, nothing in this reconstruction's
 * own call graph dispatches through a CTask-derived task's own vtable" status
 * every other CTask-derived per-instance vtable in this project already has
 * (CChunkServerTask, CPanelIfcTask, ...) -- CEditTask is never itself handed
 * to CLevelManager/scheduler on this reconstruction's currently-wired boot
 * path (only CModule::Add() is called, registering it into the OWNING
 * module's mTasks array).
 */

#ifndef EDIT_TASK_H
#define EDIT_TASK_H

#include "task.h"
#include "editable.h"
#include "out_link.h"

class CEditTask : public CTask, public CEditable {
public:
	explicit CEditTask(const CModule &owner);
	~CEditTask();

	/* .text+0x08243ac0, 43 bytes. Real, see header comment. */
	void DoPreload();

	/* .text+0x08243ca0, 14 bytes. Real, see header comment. */
	const char *GetOutLinkName() const;

private:
	COutLinkMono *mOutLink; /* +0x84, see header comment for the real
	                          * CBatchDiskCmds-vs-COutLinkMono simplification */
};

#endif /* EDIT_TASK_H */
