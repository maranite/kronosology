/*
 * config_manager.cpp  -  see include/config_manager.h.
 *
 * AssignEditServerIDs() transcribed from AssignEditServerIDs@080562f0.c (334 bytes).
 *
 * SetupSysex() UPGRADED to Tier A (Stage 6 breadth sweep, 2026-07-25) -- transcribed
 * from SetupSysex@08056b90.c / direct objdump-dr re-derivation of
 * CConfigManager::SetupSysex() (.text+0x08056b90, 486 bytes). This is the real caller
 * chain that makes `CClientCommServer`/`CSysExMsgTaskBase` (client_comm_server.h/
 * sysex_msg_task_base.h) genuinely boot-path reachable -- see those headers' own
 * writeups for the fuller chain. Called from `CKernel::InitUserLayer()` (ckernel.cpp,
 * already real).
 *
 * The other 8 CConfigManager methods (CKernel::InitUserLayer()'s own bring-up
 * sequence) are Tier-B link-stubs, not individually looked up in the decompile export
 * -- genuinely out of scope for this pass (each is presumably its own substantial
 * per-subsystem bring-up routine, matching the scale of CModuleManager's own methods).
 */

#include "config_manager.h"

/* Real module-scope global (mains.cpp): `void *SysExApi = 0;`, later pointed at
 * `g_oSysExApiInstance` by CSysExApiInstance's own static constructor
 * (ConstructSysExApiInstance(), mains.cpp) -- the SAME global SetupSysex()'s real
 * disassembly dereferences directly (`ds:0x931b310`, confirmed via `nm`).
 */
extern void *SysExApi;

namespace {

/* CSysExApi -- the abstract per-subsystem SysEx registration facade `SysExApi`
 * implements at runtime (real base of CSysExApiInstance, ground truth). Real vtable
 * slots confirmed this pass by direct `objdump -dr` re-derivation of
 * CConfigManager::SetupSysex()'s own call sites (not guessed):
 *   +0x20  AssignSysExID(name, id) -- unconditional per non-null-name entry, 2 args
 *          (name, byte). Confirmed: matches CSysExApiInstance::AssignSysExID's own
 *          real signature (`AssignSysExID(char const*, unsigned char)`, nm -C) and is
 *          the only 2-arg (name,byte) call in this function.
 *   +0x28  1-arg (name) call, guarded by the entry's own +0x08 field. Arg shape
 *          matches CSysExApiInstance::RegisterReceiver(char const*) -- plausible but
 *          NOT independently confirmed via address cross-reference (both
 *          RegisterSender/RegisterReceiver share the same 1-arg shape); modeled as
 *          RegisterReceiver on that basis, flagged here as inferred, not proven.
 *   +0x2c  1-arg (name) call, guarded by the entry's own +0x04 field. Same caveat --
 *          modeled as RegisterSender.
 *   +0x30  RegisterMessageClient(name, ecb, mode, service) -- fully confirmed, see
 *          client_comm_server.h's own reachability writeup (this is the real slot
 *          `CSysExApiInstance::RegisterMessageClient`/`CSexServiceTask::
 *          RegisterMessageClient` sit at, cross-checked against a direct .rodata
 *          vtable-slot byte read).
 * Modeled as raw vtable-slot calls (CallVSlot-style, module_manager.cpp's own
 * convention) rather than real C++ virtuals, since a real virtual's compiler-assigned
 * slot order is not guaranteed to match this ABI's.
 */
class CSysExApi {
public:
	void AssignSysExID(const char *name, unsigned char id)
	{
		typedef void (*Fn)(CSysExApi *, const char *, unsigned char);
		Fn fn = (Fn)(*(void ***)this)[0x20 / 4];
		fn(this, name, id);
	}

	void RegisterReceiver(const char *name)
	{
		typedef void (*Fn)(CSysExApi *, const char *);
		Fn fn = (Fn)(*(void ***)this)[0x28 / 4];
		fn(this, name);
	}

	void RegisterSender(const char *name)
	{
		typedef void (*Fn)(CSysExApi *, const char *);
		Fn fn = (Fn)(*(void ***)this)[0x2c / 4];
		fn(this, name);
	}

	void RegisterMessageClient(const char *name, unsigned char ecb, int mode, int service)
	{
		typedef void (*Fn)(CSysExApi *, const char *, unsigned char, int, int);
		Fn fn = (Fn)(*(void ***)this)[0x30 / 4];
		fn(this, name, ecb, mode, service);
	}

private:
	void *mVtbl;
};

/* Real per-entry layout of CConfigManager::sm_ptSysExModuleInfo's own table, stride
 * 0x18 (24) bytes -- confirmed field-by-field this pass by re-deriving every argument
 * register in SetupSysex()'s own disassembly back to its `[ebx+N]` source.
 */
struct SysExModuleInfoEntry {
	const char   *name;       /* +0x00 */
	int           hasSender;  /* +0x04, nonzero guards RegisterSender(name) */
	int           hasReceiver;/* +0x08, nonzero guards RegisterReceiver(name) */
	int           commMode;   /* +0x0c, CSysExApi::ECommMode, passed to
	                            * RegisterMessageClient */
	int           service;    /* +0x10, CSysExApi::EService, passed to
	                            * RegisterMessageClient */
	unsigned char sysExId;    /* +0x14, passed to the unconditional AssignSysExID() */
	unsigned char ecbOrSkip;  /* +0x15, 0xff skips RegisterMessageClient entirely;
	                            * otherwise also the byte arg RegisterMessageClient
	                            * itself takes */
	unsigned char pad16, pad17; /* alignment only, not individually confirmed real */
};

} // namespace

/* CEditApiInstance::AssignScope()/EditApiInstance are Tier-B: real signature/global
 * confirmed from the decompile, but with this pass's own zero-initialized
 * sm_ptEditServerInfo placeholder (config_info.cpp), AssignEditServerIDs()'s loop
 * body that would call this is real but unreachable (see below) -- so an empty body
 * here is not a behavioral gap for anything this pass's data can exercise.
 */
class CEditApiInstance {
public:
	void AssignScope(const char *name, unsigned char scope);
};

void CEditApiInstance::AssignScope(const char * /*name*/, unsigned char /*scope*/)
{
	/* Tier-B link-stub. */
}

/* Real global, shared with mains.cpp's own MMainEditMan() (the same EditApiInstance
 * the real binary registers via CSysApiInstance::RegisterApi(), Api's vtable slot
 * +0xa4) -- defined once there, not redefined here. CORRECTED 2026-07-23: this is a
 * real ~1028-byte object (byte buffer, not a pointer) -- see mains.cpp's own
 * declaration comment; array-to-pointer decay keeps every cast below unchanged.
 */
extern unsigned char EditApiInstance[];

void CConfigManager::AssignEditServerIDs()
{
	if (sm_ptCreateInfo == 0)
		return;

	/* Real per-entry table: 7 packed {name, scope} pairs per 0x10-dword row,
	 * terminated by a null name. With this pass's own zero-initialized
	 * sm_ptEditServerInfo placeholder (config_info.cpp), the first name is already
	 * null, so this loop body is real but never executes -- transcribed faithfully
	 * anyway (same license as USTGAPILCDControl::LoadStoredSettings()'s dead
	 * `local_10` read).
	 */
	unsigned *row = (unsigned *)sm_ptEditServerInfo;
	char *name = (char *)row[0];
	while (name != 0) {
		((CEditApiInstance *)EditApiInstance)->AssignScope(name, (unsigned char)row[1]);
		if ((char *)row[2] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[2], (unsigned char)row[3]);
		if ((char *)row[4] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[4], (unsigned char)row[5]);
		if ((char *)row[6] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[6], (unsigned char)row[7]);
		if ((char *)row[8] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[8], (unsigned char)row[9]);
		if ((char *)row[10] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[10], (unsigned char)row[11]);
		if ((char *)row[12] == 0) return;
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[12], (unsigned char)row[13]);
		if ((char *)row[14] == 0) return;
		unsigned char scope15 = (unsigned char)row[15];
		((CEditApiInstance *)EditApiInstance)->AssignScope((char *)row[14], scope15);
		row += 16;
		name = (char *)row[0];
	}
}

void CConfigManager::ConfigureSeqTimer() {}
void CConfigManager::CreateResourceFamilies() {}
void CConfigManager::CreateUserModules() {}
void CConfigManager::CreateFMDrivers() {}
void CConfigManager::SetupRouting() {}
void CConfigManager::LinkRTRouterTracks() {}

void CConfigManager::SetupSysex()
{
	/* First (and, this pass, only transcribed) loop: sm_ptSysExModuleInfo, real
	 * null-name terminated (matches AssignEditServerIDs()'s own convention). With
	 * this pass's own zero-initialized sm_ptSysExModuleInfo placeholder
	 * (config_info.cpp, 0x30 bytes = exactly 2 SysExModuleInfoEntry-sized slots),
	 * the very first entry's own name field is already null, so this loop body is
	 * real but never executes today -- transcribed faithfully anyway, same license
	 * as AssignEditServerIDs().
	 */
	if (sm_ptSysExModuleInfo == 0)
		return;

	CSysExApi *sysExApi = (CSysExApi *)SysExApi;
	const SysExModuleInfoEntry *entry = (const SysExModuleInfoEntry *)sm_ptSysExModuleInfo;

	while (entry->name != 0) {
		sysExApi->AssignSysExID(entry->name, entry->sysExId);
		if (entry->hasSender != 0)
			sysExApi->RegisterSender(entry->name);
		if (entry->hasReceiver != 0)
			sysExApi->RegisterReceiver(entry->name);
		if (entry->ecbOrSkip != 0xff)
			sysExApi->RegisterMessageClient(entry->name, entry->ecbOrSkip, entry->commMode,
			                                  entry->service);
		entry++;
	}

	/* Real function also walks sm_ptSysExConnectInfo (a tagged-union table, type
	 * 1/2/3 dispatching to 3 further Api vtable slots -- +0x70/+0x68/+0x6c, real
	 * meaning not decoded) and sm_ptSysExFilterInfo/sm_ptRTRouterInfo (a
	 * null-terminated linked list of 4-field records dispatching through slot
	 * +0x44, real meaning not decoded). Neither is transcribed this pass --
	 * genuinely out of scope (neither affects CClientCommServer/CSysExMsgTaskBase
	 * reachability, the actual point of this batch), and both are ALSO real no-ops
	 * today given their own zero-initialized placeholders (config_info.cpp) --
	 * left Tier-B (not transcribed) rather than fabricating slot names this pass
	 * didn't confirm.
	 */
}

void CConfigManager::MakeConnections() {}
void CConfigManager::RegisterChunkServer() {}
