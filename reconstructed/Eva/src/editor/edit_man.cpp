/*
 * edit_man.cpp  -  see include/edit_man.h.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CEditMan_08e85ea8 (Eva, VA 0x08e85ea8): 7 dwords, exactly
 * {~CEditMan, ~CEditMan(deleting), CEditMan::Setup, CEditMan::Config,
 * CEditMan::Start, CModule::Destroy, CModule::GetErrorMsg} -- same clean
 * 7-slot CModule-shaped match as CDumpManMod's own vtable (dump_man_mod.cpp).
 *
 * CMainTask's own vtable (PTR__CMainTask_08e85ee8, also confirmed by direct byte
 * read) is 7 slots too -- CMainTask adds no NEW virtual methods beyond CTask's own
 * base 7 (every one of its 11 real methods below is a plain non-virtual thiscall
 * member function in the real binary, confirmed by their direct-call, not
 * vtable-indirect, call shape in every decompile read for this batch).
 */

#include "edit_man.h"
#include "edit_server.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* EditApiInstance -- real global (mains.cpp), 0x404-byte placeholder buffer.
 * CEditMan::Setup() stores `this` at its own +4 offset (real ground truth:
 * `EditApiInstance._4_4_ = this;`) -- transcribed here rather than re-declared.
 */
extern "C" unsigned char EditApiInstance[0x404];

/* ===== CEditMan::CMainTask ===== */

CEditMan::CMainTask::CMainTask(const CModule &owner)
	: CTask(owner, "MainTask", 4, 1, 0x804b)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CMainTask_08e85ee8;
	/* Real: this+8 (mIfcThunk, CTask's own field) overwritten with a
	 * CMainTask-specific opaque identity (&DAT_08e85f04) -- never
	 * dereferenced by any reconstructed code, same "opaque final value"
	 * treatment as CTask's own ctor writing &DAT_08e82144 there originally
	 * (task.h).
	 */
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e85f04;

	std::memset(mServers, 0, sizeof(mServers));

	/* Real: embedded COmegaPtrArray(growBy=1, cap=0, own=0) at +0x27c,
	 * vtable-swapped to TPtrArray<CEditClient> (PTR__TPtrArray_08e85f40).
	 */
	new (mClientsArray) COmegaPtrArray(1, 0, 0);
	*reinterpret_cast<void **>(mClientsArray) = (void *)PTR__TPtrArray_08e85f40;
}

int CEditMan::CMainTask::RegisterServer(CEditServer *server)
{
	if ((signed char)server->GetAssignedScope() < 0)
		return 0;

	unsigned scope = server->GetAssignedScope();
	CEditServer *existing = mServers[scope];
	if (existing != 0 && (signed char)existing->GetAssignedScope() >= 0) {
		SetMask(1);
		mServers[existing->GetAssignedScope()] = 0;
		SetMask(0);
	}

	SetMask(1);
	mServers[scope] = server;
	SetMask(0);
	return 1;
}

bool CEditMan::CMainTask::UnregisterServer(CEditServer *server)
{
	if ((signed char)server->GetAssignedScope() < 0)
		return false;

	SetMask(1);
	mServers[server->GetAssignedScope()] = 0;
	SetMask(0);
	return true;
}

unsigned char CEditMan::CMainTask::GetServerScope(const CEditServer *server) const
{
	return server->GetAssignedScope();
}

unsigned CEditMan::CMainTask::GetServerScope(const char *name) const
{
	for (unsigned i = 0; i < 128; ++i) {
		CEditServer *server = mServers[i];
		if (server != 0 && std::strcmp(server->GetName(), name) == 0)
			return server->GetAssignedScope();
	}
	return 0xffffffffu;
}

int CEditMan::CMainTask::RegisterClient(CEditClient *client)
{
	COmegaPtrArray *clients = reinterpret_cast<COmegaPtrArray *>(mClientsArray);
	if (clients->FindIndex(client) == 0x7fffffff)
		clients->Add(client);
	return 1;
}

void CEditMan::CMainTask::UnregisterClient(CEditClient *client)
{
	COmegaPtrArray *clients = reinterpret_cast<COmegaPtrArray *>(mClientsArray);
	unsigned index = clients->FindIndex(client);
	/* Real 3rd argument (callDtorCallback) is whatever garbage happened to
	 * be at this+0x280 at the time -- an undecoded field this reconstruction
	 * doesn't otherwise model; 0 (don't invoke the free-element callback) is
	 * the safe, license-consistent placeholder (matches this project's
	 * "unfaithful value that doesn't change control flow here" convention,
	 * since mClientsArray's own free-callback is EvaVTableStub regardless).
	 */
	clients->RemoveAtIndex(index, 0);
}

int CEditMan::CMainTask::FindDescriptor(unsigned char group, unsigned char index,
                                          unsigned char subIndex, CEditServer **outServer) const
{
	if ((signed char)group < 0)
		return 0;

	CEditServer *server = mServers[group];
	if (server == 0)
		return 0;

	if (outServer != 0)
		*outServer = server;

	return server->FindDescriptor(group, index, subIndex);
}

int CEditMan::CMainTask::SetDefault(unsigned char group, unsigned char index,
                                      unsigned char subIndex) const
{
	if ((signed char)group < 0)
		return 0;

	CEditServer *server = mServers[group];
	if (server == 0)
		return 0;

	if (server->FindDescriptor(group, index, subIndex) == 0)
		return 0;

	server->InvokeSetDefaultSlot();
	return 0; /* real return value is whatever the (inert) slot dispatch yields */
}

int CEditMan::CMainTask::GetField(unsigned char group, unsigned char index, unsigned char subIndex,
                                    void * /*buf*/, unsigned int /*bufLen*/) const
{
	/* NOTE ON PARAM RECOVERY: Ghidra mistyped this real thiscall method as
	 * __cdecl and folded `this` into what it prints as its own first
	 * parameter (`_param_1`, referenced only via pointer arithmetic, never
	 * as a byte value) -- confirmed by cross-checking against
	 * FindDescriptor()'s own clean decompile, which performs the identical
	 * `mServers[group]` indexing one visible parameter earlier. `buf`
	 * itself is never bound to a named parameter at all: the real tail call
	 * through the found server's own vtable slot+8 is an unresolved
	 * indirect jump ("Could not recover jumptable... too many branches",
	 * same Ghidra artifact class already hit for CTask::Add(COutLink*) and
	 * CDumpTask::OnTimeout()) that Ghidra shows taking zero arguments; the
	 * real forwarding of buf/bufLen into that call is not independently
	 * verified, but is moot here regardless since the target
	 * (CEditServer's own Get-slot override) is EvaVTableStub in this
	 * reconstruction. Group/index/subIndex gating logic IS the real,
	 * confirmed part -- transcribed exactly.
	 */
	if ((signed char)group < 0)
		return 0;

	CEditServer *server = mServers[group];
	if (server == 0)
		return 0;

	if (server->FindDescriptor(group, index, subIndex) == 0)
		return 0;

	server->InvokeGetSlot();
	return 0;
}

int CEditMan::CMainTask::SetField(unsigned char group, unsigned char index, unsigned char subIndex,
                                    const void * /*buf*/, unsigned int /*bufLen*/, int /*source*/) const
{
	/* Same param-recovery caveat as GetField() above. */
	if ((signed char)group < 0)
		return 0;

	CEditServer *server = mServers[group];
	if (server == 0)
		return 0;

	if (server->FindDescriptor(group, index, subIndex) == 0)
		return 0;

	server->InvokeSetSlot();
	return 0;
}

void CEditMan::CMainTask::Notify(unsigned char group, unsigned char index,
                                   unsigned char subIndex) const
{
	const COmegaPtrArray *clients = reinterpret_cast<const COmegaPtrArray *>(mClientsArray);
	unsigned count = clients->Count();
	for (unsigned i = count; i-- > 0; ) {
		CEditClient *client = reinterpret_cast<CEditClient *>(clients->Get(i));
		if (client != 0) {
			typedef void (*NotifyFn)(CEditClient *, unsigned char, unsigned char, unsigned char);
			NotifyFn fn = (NotifyFn)(((void **)client->mVtbl)[2]);
			fn(client, group, index, subIndex);
		}
	}
}

void CEditMan::CMainTask::Exec()
{
	/* Tier B -- genuinely out of scope, see edit_man.h. */
}

/* ===== CEditMan ===== */

CEditMan::CEditMan()
	: CModule("EditMan"), mMainTask(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CEditMan_08e85ea8;
}

void CEditMan::Setup()
{
	void *raw = malloc(0x294);
	mMainTask = new (raw) CMainTask(*this);
	Add(mMainTask);

	*reinterpret_cast<CEditMan **>(EditApiInstance + 4) = this;
}

void CEditMan::Config()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CEditMan::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

extern "C" void CEditManSetupVSlot(void *obj)
{
	static_cast<CEditMan *>(obj)->Setup();
}

extern "C" void CEditManConfigVSlot(void *obj)
{
	static_cast<CEditMan *>(obj)->Config();
}

extern "C" void CEditManStartVSlot(void *obj)
{
	static_cast<CEditMan *>(obj)->Start();
}

/* Real vtable definitions -- moved here from omega_vtables.cpp now that slots
 * 2/3/4 have real forwarders to wire in, same "define locally where the real
 * forwarders live" precedent as dump_man_mod.cpp/es_common.cpp.
 */
void *PTR__CEditMan_08e85ea8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CEditManSetupVSlot, (void *)CEditManConfigVSlot, (void *)CEditManStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CMainTask's own real vtable -- install-only, matching CModule's own base-vtable
 * "never actually dispatched through" status (nothing in this reconstruction's own
 * call graph destroys a CMainTask or calls Destroy()/GetErrorMsg() on one).
 */
void *PTR__CMainTask_08e85ee8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* TPtrArray<CEditClient> vtable-swap identity for CMainTask's own embedded client
 * list (3 slots -- confirmed via the same installed-pointer-to-next-symbol
 * methodology as every other TPtrArray/TNamedPtrArray entry in this project,
 * omega_vtables.h).
 */
void *PTR__TPtrArray_08e85f40[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* Opaque data placeholder CMainTask's ctor stores the ADDRESS of at +0x08 (mIfcThunk)
 * -- never dereferenced by any reconstructed code, same treatment as
 * EvaDataPlaceholder_08e82144 (omega_vtables.cpp).
 */
int EvaDataPlaceholder_08e85f04;
