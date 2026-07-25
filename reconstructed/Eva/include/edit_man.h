/*
 * edit_man.h  -  CEditMan : public CModule + CEditMan::CMainTask : public CTask,
 * Stage 6 breadth sweep, 2026-07-25 (small-derived-module follow-up batch, see
 * dump_man_mod.h's own header comment for the general "MMainXxx 9-member family"
 * survey this batch continues -- CEditMan is the first of the 4 members
 * eva_dumpmanager_cluster_batch.md flagged as not-yet-pursued).
 *
 * GROUND TRUTH: `MMainEditMan()` (mains.cpp, already Tier A) builds a base
 * `CModule("EditMan")` and vtable-swaps in `PTR__CEditMan_08e85ea8` -- unchanged
 * here, matching CDumpManMod's own "mains.cpp already produces the correct object"
 * precedent (dump_man_mod.h). `CModuleManager::Setup()`/`Config()`/`Start()`
 * (module_manager.cpp, already Tier A) dispatch through the module's own vtable
 * slots +8/+0xc/+0x10 -- wired here (omega_vtables.cpp analogue, this file) to real
 * forwarders into `CEditMan::Setup()`/`Config()`/`Start()`.
 *
 * `CEditMan::Setup()` (.text+0x080d2790, 88 bytes) mallocs a real `CMainTask`
 * (.text+0x080d3130 ctor) and registers it via the already-real
 * `CModule::Add(CTask*)` -- same "construct + Add() a sibling CTask" idiom as
 * `CDumpManMod::Setup()`. `Config()`/`Start()` (.text+0x080d2640/0x080d2650, 3 bytes
 * each) are confirmed genuinely empty (`return 0;`), same treatment as
 * `CDumpManMod::Config()`/`Start()`.
 *
 * `CMainTask` (NOT the unrelated `CEditor::CMainTask`, a completely different,
 * genuinely deep Peg/UI-editor-desktop class that happens to share the same
 * unqualified name -- confirmed by symbol-qualified `nm -C`, do not conflate) is a
 * self-contained, real "EditServer scope registry": a fixed 128-slot
 * `CEditServer*[128]` array (this+0x7c..0x27c) indexed by each registered server's
 * own `mAssignedScope` byte (edit_server.h), plus a `CEditClient*` observer list
 * (embedded `COmegaPtrArray` at +0x27c) that `Notify()` fans a (group,index,
 * subIndex) event out to. Every one of its 11 real methods is reconstructed here --
 * all are small (12-503 bytes) and depend only on already-real infrastructure
 * (`CEditServer::FindDescriptor` edit_server.h, `COmegaPtrArray::Add/FindIndex/
 * RemoveAtIndex/Count/Get` omega_ptr_array.h, `CTask::SetMask` task.h) -- no new
 * out-of-scope subsystem pulled in. `CEditMan`'s own 8 public methods are one-line
 * forwarders into this object (stored at CEditMan+0x2c, set by `Setup()`).
 *
 * `Notify()`'s real body is an 8-way Duff's-device-unrolled reverse walk over the
 * client list calling each client's own vtable slot+8 (`CEditClient::Notify`,
 * presumably -- `CEditClient` itself is not independently reconstructed, only used
 * here as an opaque interface pointer with 1 known vtable slot, same treatment as
 * `CIfcUnknown` elsewhere in this project); collapsed to a plain reverse `for` loop
 * here, same license as every other unrolled loop in this project.
 *
 * `GetServerScope(const char*)`'s real body performs an UNROLLED-BY-8 scan (checks
 * offsets +0x7c, +0x80, +0x84, ..., +0x98 per iteration, i.e. 8 consecutive array
 * slots per loop body) over the SAME 128-slot array `RegisterServer()`/
 * `FindDescriptor()`/`SetDefault()` index directly -- collapsed to a plain indexed
 * loop here.
 *
 * `RegisterServer()`'s real body evicts any existing occupant of the target scope
 * slot (reading the occupant's OWN `mAssignedScope` back before clearing -- in
 * practice always equal to the slot index itself, so this is "clear then
 * overwrite", not a real different-scope migration) under a `CTask::SetMask(1)`/
 * `SetMask(0)` critical-section pair, then stores the new server. Transcribed
 * exactly, including the redundant self-referential re-read, matching this
 * project's "preserve real redundant logic" convention (mains.cpp's `Mains()`,
 * edit_server.h's `AddDescriptors()`).
 */

#ifndef EDIT_MAN_H
#define EDIT_MAN_H

#include "module.h"
#include "task.h"

class CEditServer;

/* Opaque real interface -- only known real member is vtable slot +8
 * (`Notify(group, index, subIndex)`, called by `CMainTask::Notify()` below).
 * Not independently reconstructed, same "opaque interface pointer" treatment as
 * `CIfcUnknown` (task.h).
 */
class CEditClient {
public:
	void *mVtbl;
};

class CEditMan : public CModule {
public:
	/* CMainTask -- see file header. Declared nested to match the real binary's
	 * own `CEditMan::CMainTask` mangled scoping (symbols.csv).
	 */
	class CMainTask : public CTask {
	public:
		/* .text+0x080d3130, 221 bytes. */
		explicit CMainTask(const CModule &owner);

		/* .text+0x080d3230, 172 bytes. Real return: 1 if `server`'s own
		 * mAssignedScope is >= 0 (i.e. it was actually registered), 0
		 * otherwise.
		 */
		int RegisterServer(CEditServer *server);

		/* .text+0x080d32e0, 89 bytes. Real return: same >=0 scope-valid
		 * check as RegisterServer(), discarded by CEditMan's own forwarder.
		 */
		bool UnregisterServer(CEditServer *server);

		/* .text+0x080d3340, 12 bytes. Just reads back server's own
		 * mAssignedScope byte -- doesn't touch `this` at all.
		 */
		unsigned char GetServerScope(const CEditServer *server) const;

		/* .text+0x080d3350, 344 bytes. Linear scan (8 slots/iteration) of
		 * the 128-slot array for a registered CEditServer whose own mName
		 * matches; returns its mAssignedScope, or 0xffffffff if none
		 * found.
		 */
		unsigned GetServerScope(const char *name) const;

		/* .text+0x080d34b0, 77 bytes. */
		int RegisterClient(CEditClient *client);

		/* .text+0x080d3500, 73 bytes. */
		void UnregisterClient(CEditClient *client);

		/* .text+0x080d3550, 144 bytes. */
		int FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex,
		                    CEditServer **outServer) const;

		/* .text+0x080d3780, 153 bytes. Dispatches through the found
		 * server's own vtable slot +0x10 (CEditServer's SetDefault-analog
		 * slot) once FindDescriptor() confirms a hit.
		 */
		int SetDefault(unsigned char group, unsigned char index, unsigned char subIndex) const;

		/* .text+0x08d35f0/0x080d36b0 real names are just "Get"/"Set" in
		 * ground truth (free __cdecl-looking functions per Ghidra, but
		 * really CMainTask methods per symbols.csv demangling) -- kept
		 * distinctly named here to avoid clashing with any future
		 * CEditServer::Get/Set overload in the same translation unit.
		 * .text+0x080d35f0, 182 bytes / 0x080d36b0, 198 bytes.
		 */
		int GetField(unsigned char group, unsigned char index, unsigned char subIndex,
		             void *buf, unsigned int bufLen) const;
		int SetField(unsigned char group, unsigned char index, unsigned char subIndex,
		             const void *buf, unsigned int bufLen, int source) const;

		/* .text+0x080d3820, 503 bytes. Fans out to every registered
		 * CEditClient's own vtable slot+8. See file header.
		 */
		void Notify(unsigned char group, unsigned char index, unsigned char subIndex) const;

		/* .text+0x080d2de0, 741 bytes. Real per-tick task body -- genuinely
		 * out of scope for this pass (real-time editor-desktop refresh
		 * logic, pulls in further undecoded UI/Peg state -- same boundary
		 * as every other CTask::Exec() override left Tier B elsewhere in
		 * this project, e.g. CBufferingTask/CDumpMachine).
		 */
		void Exec();

	private:
		CEditServer *mServers[128]; /* +0x7c..0x27c, indexed by mAssignedScope */
		unsigned char mClientsArray[0x18]; /* +0x27c, embedded COmegaPtrArray<CEditClient*> */

		friend struct EditManTestHooks;
	};

	/* .text+0x080d2810, 38 bytes. Ground truth's own real boot-path caller
	 * (mains.cpp's MMainEditMan()) builds an equivalent object by hand instead
	 * of calling this -- same "provided for structural completeness" status as
	 * CDumpManMod::CDumpManMod().
	 */
	CEditMan();

	/* .text+0x080d2790, 88 bytes. Real body -- see file header. */
	void Setup();

	/* .text+0x080d2640, 3 bytes. Confirmed genuinely `return 0;`. */
	void Config();

	/* .text+0x080d2650, 3 bytes. Confirmed genuinely `return 0;`. */
	void Start();

	/* 8 one-line forwarders into mMainTask -- all Tier A, all real. */
	int RegisterServer(CEditServer *server) { return mMainTask->RegisterServer(server); }
	bool UnregisterServer(CEditServer *server) { return mMainTask->UnregisterServer(server); }
	unsigned char GetServerScope(const CEditServer *server) const { return mMainTask->GetServerScope(server); }
	unsigned GetServerScope(const char *name) const { return mMainTask->GetServerScope(name); }
	int RegisterClient(CEditClient *client) { return mMainTask->RegisterClient(client); }
	void UnregisterClient(CEditClient *client) { mMainTask->UnregisterClient(client); }
	int FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex,
	                    CEditServer **outServer) const {
		return mMainTask->FindDescriptor(group, index, subIndex, outServer);
	}
	int SetDefault(unsigned char group, unsigned char index, unsigned char subIndex) const {
		return mMainTask->SetDefault(group, index, subIndex);
	}

private:
	CMainTask *mMainTask; /* +0x2c */

	friend struct EditManTestHooks;
};

extern "C" void CEditManSetupVSlot(void *obj);
extern "C" void CEditManConfigVSlot(void *obj);
extern "C" void CEditManStartVSlot(void *obj);

#endif /* EDIT_MAN_H */
