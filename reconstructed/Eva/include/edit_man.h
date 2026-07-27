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

/* CEditClient -- RE-TRACED FOR REAL, 2026-07-27 (Eva "size is not depth"
 * re-check, 3rd consecutive session to re-open this exact class after an
 * earlier "needs a genuine open-chaining hash-table + free-list-allocator
 * template, out of scope" verdict).
 *
 * That verdict was RIGHT about the template being real, but WRONG about it
 * blocking ctor/dtor: direct `objdump -dr -M intel` of
 * `CEditClient::CEditClient()` (.text+0x0806e470, 1812 bytes) and both
 * `~CEditClient()` variants (D1 .text+0x0806e3f0/35B, D0 .text+0x0806e420/
 * 55B) shows the construction/destruction path never calls into the hash
 * table's own Add/Find/Remove/Node/Iterator machinery at all -- it only
 * allocates and zero-initializes two small header objects. Confirmed via
 * RTTI: the two vtables the ctor installs (Eva VA 0x8e81560/0x8e81570) have
 * `typeinfo` strings "PointerHash<CEditControl*, CEditControl>" and
 * "PointerHash<long, CEditControl>" (.rodata VA 0x8e815c0/0x8e81600) --
 * `PointerHash<K,V>` is a REAL template class, but a whole-binary xref sweep
 * shows these are its ONLY TWO instantiations anywhere in the 37k-function
 * binary, and both vtable-install addresses are referenced from nowhere but
 * this ctor. So it is not a widely shared piece of infrastructure the way
 * `CZ` is -- it just happens to look generic from the demangled name.
 *
 * REAL ctor sequence (HAL_DisableInterrupts()/HAL_EnableInterrupts()
 * brackets around each malloc dropped, same established reason as every
 * other malloc in this project, e.g. `job_stack.h`):
 *   1. this->mVtbl = PTR__CEditClient_08e814e0 (this class's OWN real
 *      3-slot vtable: ~CEditClient D1, ~CEditClient D0, OnNotify --
 *      confirmed by a direct .rodata read, vfunc[2] == 0x0806f6e0, matching
 *      `CMainTask::Notify()`'s own `vtbl[2]` dispatch in edit_man.cpp).
 *   2. mControlHash = malloc(0x10); install its own vtable
 *      (PTR__PointerHash_CEditControlPtr_08e81560); zero its +0x4 flag byte
 *      and +0xc dword; malloc(0xada4) [=44452 bytes] for its node-pool
 *      buffer, store at +0x8, memset it to 0 (ground truth's own body is a
 *      GCC auto-vectorized `movdqa` zero loop over exactly 11113 dwords --
 *      0xada4/4 -- a decompiler-shape artifact, not real hash-bucket
 *      initialization logic; same "dead auto-vectorized loop" pattern as
 *      `ghidra_oa_export_artifacts.md`).
 *   3. mIndexHash: identical shape, second PointerHash instantiation
 *      (PTR__PointerHash_long_08e81570).
 *   4. EditApiInstance_RegisterClient(this) -- confirmed by a full xref
 *      sweep to be the ONLY caller anywhere of the real
 *      CEditApiInstance::RegisterClient() trampoline (.text+0x080d1ea0),
 *      which itself just forwards to the already-real
 *      `CEditMan::RegisterClient()` (edit_man.h) when `EditApiInstance+4`
 *      (a `CEditMan*`, set by `CEditMan::Setup()` above) is non-null.
 *
 * REAL dtor: only the D1 (base-object) variant is modeled here, because the
 * ONLY known real construction site in this project's own call graph is
 * `CEditor`'s embedded `mEditClient` sub-object (editor.h) -- a plain
 * non-virtual member, torn down by the compiler with a direct D1-shaped
 * call, exactly matching `CEditor::~CEditor()`'s own header comment
 * ("mEditClient... destructed automatically by the compiler... same as
 * ground truth's own explicit ~CEditClient() tail calls"). D1 restores
 * `mVtbl` then calls `EditApiInstance_UnregisterClient(this)` -- confirmed
 * by direct disasm to NOT destruct or free `mControlHash`/`mIndexHash` (a
 * real leak in ground truth, transcribed faithfully, not a modeling gap).
 * The D0 (deleting) variant additionally does `free(this)` -- not modeled,
 * since nothing in this reconstruction's own traced call graph reaches it
 * (the only other real caller, `TPtrArray<CEditClient>::DeletePointer`,
 * .text+0x08186990, is a genuinely separate, unmodeled container).
 *
 * `PointerHash<K,V>`'s own Add/Find/Remove/Node/Iterator methods, and
 * CEditClient's OWN 4 other real named methods (BlockRegister/Register/
 * Unregister/NotifyControls, all real, all still genuinely unreconstructed)
 * are OUT OF SCOPE for this pass -- exactly the same "reconstruct the
 * narrow slice the traced call graph actually needs, leave the rest
 * deferred" precedent as `CJobStack` (job_stack.h).
 */
class CEditClient {
public:
	CEditClient();
	~CEditClient();

	void *mVtbl;         /* +0x00, PTR__CEditClient_08e814e0 */
	void *mControlHash;   /* +0x04, PointerHash<CEditControl*, CEditControl>* -- opaque 0x10-byte header, see above */
	void *mIndexHash;     /* +0x08, PointerHash<long, CEditControl>* -- opaque 0x10-byte header, see above */
};

/* CEditClient's own real vtable (3 slots: ~CEditClient D1, ~CEditClient D0,
 * OnNotify). Slots 0/1 stay EvaVTableStub -- CEditClient is modeled
 * non-virtually here (this project's established "manually vtable-swapped,
 * non-polymorphic-in-C++-terms" idiom, job_stack.h), so nothing dispatches
 * through them; ~CEditClient() below implements D1's real behavior directly
 * as an ordinary member function instead. Slot 2 (OnNotify, real
 * .text+0x0806f6e0) also stays EvaVTableStub -- real and load-bearing for
 * `CMainTask::Notify()`'s own `vtbl[2]` dispatch, but not independently
 * reconstructed by this pass (out of scope, see above).
 */
extern "C" void *PTR__CEditClient_08e814e0[3];

/* PointerHash<CEditControl*, CEditControl>'s own real vtable (2 slots: D1,
 * D0 -- confirmed by direct .rodata read, no other real virtual methods).
 * Both slots stay EvaVTableStub -- nothing in this reconstruction's own
 * call graph ever dispatches through a PointerHash instance's own vtable.
 */
extern "C" void *PTR__PointerHash_CEditControlPtr_08e81560[2];

/* PointerHash<long, CEditControl>'s own real vtable, same shape as above. */
extern "C" void *PTR__PointerHash_long_08e81570[2];

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
