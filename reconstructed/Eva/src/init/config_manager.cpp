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
 * SetupRouting()/MakeConnections()/RegisterChunkServer()/LinkRTRouterTracks()/
 * ConfigureSeqTimer() UPGRADED to Tier A (Stage 6 breadth sweep, batch 2026-07-25b):
 *   CConfigManager::SetupRouting()         .text+0x08056d80, 1 byte  (`return;`,
 *                                            confirmed genuinely empty in the real
 *                                            binary -- same "read it, don't assume"
 *                                            treatment as CSTGUnsolMsgHandler's own
 *                                            8 confirmed-empty slots)
 *   CConfigManager::MakeConnections()      .text+0x08056d90, 312 bytes
 *   CConfigManager::RegisterChunkServer()  .text+0x08056a50, 316 bytes
 *   CConfigManager::LinkRTRouterTracks()   .text+0x08056840, 511 bytes
 *   CConfigManager::ConfigureSeqTimer()    .text+0x08056ed0, 295 bytes
 * All 4 non-trivial ones are the same shape as SetupSysex() above: walk one of
 * CConfigManager's own static config tables (all real, non-null pointers to
 * zero-initialized placeholder data -- config_info.cpp), dispatching through a
 * per-subsystem "Api" facade's raw vtable slots. With today's zeroed table
 * contents every one of these is a real, live, boot-path-executed no-op (not
 * dead code) -- ConfigureSeqTimer() in particular unconditionally reaches its own
 * tail end regardless of table contents (see its own comment below and
 * config_info.cpp's SeqTimerInfo placeholder comment for a real divide-by-zero
 * hazard this surfaced and how it was avoided). Real vtable-slot offsets
 * confirmed by re-deriving each call site's own disassembly, same method as
 * SetupSysex(); ChkApi/SeqApi/RTRouterApi's own vtable arrays (mains.cpp) needed
 * bumping past their old 6-slot bound to safely cover the new, higher offsets --
 * same "give headroom" fix already applied to EditApiInstance's own array.
 *
 * CreateUserModules()/CreateFMDrivers() UPGRADED to Tier A (Stage 6 breadth sweep,
 * follow-up batch 2026-07-25, on top of CModuleManager::AddConstructor()/
 * RemoveConstructor(), module_manager.h):
 *   CConfigManager::CreateUserModules()  .text+0x08056440, 778 bytes
 *   CConfigManager::CreateFMDrivers()    .text+0x08056760, 204 bytes
 * Both walk a table of {name, param1, param2} triples (sm_ptCreateInfo/
 * sm_ptFMDriverInfo) terminated by a null name, looking up a matching "factory"
 * object by name and invoking its own vtable-slot+8 "Create" method with
 * (param1, param2, an auto-incrementing per-factory counter). CreateUserModules()
 * looks its factory up in CModuleManager's own mConstructors array (the "distinct
 * module factory array sub-structure" this project's README previously flagged as
 * unreconstructed -- now real, see module_manager.h/.cpp) and, on success, adds the
 * newly-created module straight into mModules via COmegaPtrArray::Add() (NOT via
 * CModuleManager::AddModule() -- a real, faithfully-preserved difference: this path
 * skips AddModule()'s own duplicate-by-name check/removal and host-notify logic).
 * CreateFMDrivers() looks its factory up through FMApi's own vtable slot +0x2c
 * instead, and on a successful create additionally registers the new driver through
 * FMApi's vtable slot +0x30, destroying it (vtable slot+4, the deleting dtor) if
 * registration reports failure. Two real, faithfully-preserved quirks in both
 * functions' own error paths, not something to "fix": the "unable to create
 * instance" error message in each case prints the entry's own param1 field
 * (`puVar1[1]`/`piVar4[1]` in the original decompile), not the name that was
 * actually searched for -- see each function body's own comment below.
 *
 * Both are real, safe no-ops on this pass's own traced boot path given today's
 * zero-initialized sm_ptCreateInfo/sm_ptFMDriverInfo placeholders (config_info.cpp):
 * each table's own first `name` field is null, so the top-level walk loop's own
 * condition is false on the very first check -- neither function's body (including
 * the mConstructors/FMApi lookups) ever actually executes, exactly the same "table's
 * own first field alone gates the whole function" safety property already
 * established for SetupRouting/MakeConnections/RegisterChunkServer/
 * LinkRTRouterTracks. CreateResourceFamilies remains a Tier-B link-stub -- depends on
 * the un-reconstructed CZ string-set container (247 methods), genuinely out of scope
 * for this pass.
 */

#include "config_manager.h"
#include "system_api.h"
#include "tempo.h"
#include "module_manager.h"
#include "omega_ptr_array.h"

#include <cstring>

/* Real module-scope global (mains.cpp): `void *SysExApi = 0;`, later pointed at
 * `g_oSysExApiInstance` by CSysExApiInstance's own static constructor
 * (ConstructSysExApiInstance(), mains.cpp) -- the SAME global SetupSysex()'s real
 * disassembly dereferences directly (`ds:0x931b310`, confirmed via `nm`).
 */
extern void *SysExApi;

/* Same shape, for the 4 new functions below -- all real module-scope globals
 * (mains.cpp), all set by their own ConstructXxxApiInstance() static constructor. */
extern CSystemApi *Api;
extern void *SeqApi;
extern void *ChkApi;
extern void *RTRouterApi;

/* Real module-scope global (mains.cpp): `CSystemApi *FMApi = 0;`, lazily fetched by
 * MMainLinuxDriver through Api's own vtable slot +0xa0 (mains.cpp) -- the SAME
 * object CreateFMDrivers() dispatches its own factory-lookup (+0x2c) and
 * register (+0x30) calls through.
 */
extern CSystemApi *FMApi;

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

/* CResMan's own 3 static data members RegisterChunkServer() reads directly by name
 * (real mangled symbols confirmed via `nm -C -S`; CResMan itself, 63 methods, is not
 * reconstructed -- Tier-B placeholder data only, same treatment as CEditApiInstance
 * above). Real values confirmed by direct `.rodata`/`.data` byte read (readelf -S +
 * file-offset math), not guessed: SysName -> "ResourceManager", both version/release
 * bytes -> 0 in the real shipped binary.
 */
class CResMan {
public:
	static const char  *SysName;
	static unsigned char sm_kbyCurrentVersion;
	static unsigned char sm_kbyCurrentRelease;
};
const char  *CResMan::SysName = "ResourceManager";
unsigned char CResMan::sm_kbyCurrentVersion = 0;
unsigned char CResMan::sm_kbyCurrentRelease = 0;

/* CChkApi -- the abstract chunk-server registration facade `ChkApi` implements at
 * runtime (real base of CChkApiInstance, ground truth). Real vtable slots confirmed
 * this pass by direct `objdump -dr` re-derivation of
 * CConfigManager::RegisterChunkServer()'s own call sites:
 *   +0x20  Register(name, byteA, byteB, byteC, i1, i2, bool) -> int (0 = failure).
 *          Same slot used both for the one unconditional bootstrap call (registers
 *          CResMan itself as a chunk server) and per-table-entry.
 *   +0x34  IsOK() -> int (real code asserts result == 1)
 *   +0x38  Finalize() -- called unconditionally after the table walk, no return
 *          value used
 * Modeled as raw vtable-slot calls (module_manager.cpp's own CallVSlot convention),
 * not real C++ virtuals -- same reasoning as CSysExApi above.
 */
class CChkApi {
public:
	int Register(const char *name, char b0, unsigned char b1, unsigned char b2, int i1, int i2, bool flag)
	{
		typedef int (*Fn)(CChkApi *, const char *, char, unsigned char, unsigned char, int, int, bool);
		Fn fn = (Fn)(*(void ***)this)[0x20 / 4];
		return fn(this, name, b0, b1, b2, i1, i2, flag);
	}

	int IsOK()
	{
		typedef int (*Fn)(CChkApi *);
		Fn fn = (Fn)(*(void ***)this)[0x34 / 4];
		return fn(this);
	}

	void Finalize()
	{
		typedef void (*Fn)(CChkApi *);
		Fn fn = (Fn)(*(void ***)this)[0x38 / 4];
		fn(this);
	}

private:
	void *mVtbl;
};

/* Real per-entry layout of CConfigManager::sm_ptChunkInfo's own table, stride 5
 * dwords (0x14 bytes) -- confirmed field-by-field from RegisterChunkServer()'s own
 * disassembly, same method as SysExModuleInfoEntry above. With this pass's own
 * zero-initialized sm_ptChunkInfo placeholder (config_info.cpp), the first entry's
 * own id field is already 0, so the real per-entry loop body never executes --
 * transcribed faithfully anyway, same license as SetupSysex()'s own loop.
 */
struct ChunkInfoEntry {
	const char   *name;    /* +0x00, also the real while-condition field (decompiler
	                         * carries it as a plain int, but it's read both as a
	                         * truthy/name-pointer check and passed as Register()'s
	                         * own name arg -- same idiom as SysExModuleInfoEntry) */
	int           word1;   /* +0x04, low byte + bytes 1/2 read as Register()'s
	                         * 2nd/3rd/4th args */
	int           i1;      /* +0x08 */
	int           i2;      /* +0x0c */
	int           flagSrc; /* +0x10, compared == 0 for Register()'s trailing bool */
};

/* CSeqApi -- the abstract sequencer-timer facade `SeqApi` implements at runtime.
 * Real vtable slots confirmed from ConfigureSeqTimer()'s own disassembly:
 *   +0x24  CreateWheel(id) -> int (-1 = failure, real code retries/reports and
 *          moves to the next table entry on failure -- transcribed faithfully)
 *   +0x30  1-arg (wheelHandle) call, guarded by the entry's own 2nd field == 1;
 *          real meaning not decoded
 *   +0x48  SetTimerType(type, 0) -- called unconditionally once at the end
 */
class CSeqApi {
public:
	int CreateWheel(int id)
	{
		typedef int (*Fn)(CSeqApi *, int);
		Fn fn = (Fn)(*(void ***)this)[0x24 / 4];
		return fn(this, id);
	}

	void NotifyWheelFlag(int wheelHandle)
	{
		typedef void (*Fn)(CSeqApi *, int);
		Fn fn = (Fn)(*(void ***)this)[0x30 / 4];
		fn(this, wheelHandle);
	}

	void SetTimerType(int type, int arg2)
	{
		typedef void (*Fn)(CSeqApi *, int, int);
		Fn fn = (Fn)(*(void ***)this)[0x48 / 4];
		fn(this, type, arg2);
	}

private:
	void *mVtbl;
};

/* CRTRouterApi -- the abstract real-time-router facade `RTRouterApi` implements at
 * runtime. Real vtable slots confirmed from LinkRTRouterTracks()'s own disassembly:
 *   +0x2c  Reset() -- called unconditionally at function entry, no args besides
 *          `this`, return value discarded
 *   +0x20  FindHandle(name) -> int (0 = not found)
 *   +0x34  ConnectTracks(senderHandle, receiverHandle, bool)
 *   +0x3c  LinkTrackPair(senderHandle, receiverHandle, byteA, byteB)
 */
class CRTRouterApi {
public:
	void Reset()
	{
		typedef void (*Fn)(CRTRouterApi *);
		Fn fn = (Fn)(*(void ***)this)[0x2c / 4];
		fn(this);
	}

	int FindHandle(const char *name)
	{
		typedef int (*Fn)(CRTRouterApi *, const char *);
		Fn fn = (Fn)(*(void ***)this)[0x20 / 4];
		return fn(this, name);
	}

	void ConnectTracks(int senderHandle, int receiverHandle, bool flag)
	{
		typedef void (*Fn)(CRTRouterApi *, int, int, bool);
		Fn fn = (Fn)(*(void ***)this)[0x34 / 4];
		fn(this, senderHandle, receiverHandle, flag);
	}

	void LinkTrackPair(int senderHandle, int receiverHandle, unsigned char a, unsigned char b)
	{
		typedef void (*Fn)(CRTRouterApi *, int, int, unsigned char, unsigned char);
		Fn fn = (Fn)(*(void ***)this)[0x3c / 4];
		fn(this, senderHandle, receiverHandle, a, b);
	}

private:
	void *mVtbl;
};

/* Real per-entry layout of CConfigManager::sm_ptRTRouterInfo's own table, stride 5
 * dwords -- confirmed field-by-field from LinkRTRouterTracks()'s own disassembly.
 * With this pass's own zero-initialized sm_ptRTRouterInfo placeholder
 * (config_info.cpp), the first entry's own sender-name field is already 0, so the
 * real per-entry loop body never executes -- transcribed faithfully anyway.
 */
struct RTRouterInfoEntry {
	const char *senderName;   /* +0x00, also the real while-condition field */
	const char *receiverName; /* +0x04 */
	int pairCount;    /* +0x08 */
	const unsigned char *pairs; /* +0x0c, packed {byte,byte} pairs, pairCount of them */
	int flagSrc;      /* +0x10, compared == 0 for ConnectTracks()'s trailing bool */
};

/* Real Api+0x90/+0x94 message-report calls RegisterChunkServer()/LinkRTRouterTracks()
 * make on their own error paths -- same slots/shape already established in
 * tempo.cpp's ApiAssert() (+0x94) and sysex_msg_task_base.cpp's own comment. cdecl,
 * EvaVTableStub-backed (omega_vtables.cpp) -- safe to call with any argument shape.
 * Neither path is reachable given this pass's own zero-initialized table data,
 * except LinkRTRouterTracks()'s real unconditional Reset()/RegisterChunkServer()'s
 * real unconditional bootstrap Register() + Finalize()/IsOK() assert tail, all of
 * which ARE exercised -- see each function's own comment below.
 */
inline void ApiWarn1(const char *fmt, const void *arg)
{
	typedef void (*Fn)(void *, const char *, const void *);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x90);
	fn(Api, fmt, arg);
}

inline void ApiWarn0(const char *fmt)
{
	typedef void (*Fn)(void *, const char *);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x90);
	fn(Api, fmt);
}

inline void ApiAssertCfgMan(int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", "CfgMan.cpp", line);
}

/* Real Api+0x44 call MakeConnections() dispatches through, 4 times per fully-
 * populated table entry. Signature (5 names + trailing literal-0 flag) matches
 * `CSysApiInstance::Connect(char const*, char const*, char const*, char const*,
 * char const*, EForceBuffered)` (nm -C) closely enough to be a plausible identity
 * (same 5-name + trailing-enum shape) -- NOT independently confirmed via a direct
 * vtable-slot byte read the way SetupSysex()'s own slots were, so modeled here as a
 * raw, un-named vtable dispatch rather than claiming the identification is proven.
 */
inline void ApiConnect(int a, int b, int c, int d, int e)
{
	typedef void (*Fn)(void *, int, int, int, int, int, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x44);
	fn(Api, a, b, c, d, e, 0);
}

} // namespace

/* CEditApiInstance::AssignScope() -- promoted to a real body 2026-07-26. Direct
 * `objdump -dr` of .text+0x080d23e0 (23 bytes) shows a trivial indexed store, not a
 * broker-lookup: `((char **)((char*)this + 8))[scope] = name; return 1;` -- a fixed
 * per-scope name-pointer table starting at `this+8` (CEditApiInstance itself stays
 * an opaque byte buffer everywhere else in this project -- EditApiInstance[], below
 * -- so this is modeled as a raw offset write on that same buffer, same convention
 * as mains.cpp's `FMApiInstance+0x4d8` write and this file's own many `Api`-vtable
 * raw dispatches).
 *
 * Still genuinely unreachable on this pass's own boot path -- AssignEditServerIDs()'s
 * calling loop (below) never executes because config_info.cpp's own
 * sm_ptEditServerInfo placeholder is zero-initialized (first name is already NULL) --
 * but that is a property of this project's OWN placeholder config data, not of this
 * function itself, so it is transcribed for real rather than left as an empty stub,
 * matching the standing "faithful dead code over convenient no-op" convention this
 * project already uses for USTGAPILCDControl::LoadStoredSettings()'s dead `local_10`
 * read and AssignEditServerIDs()'s own loop body just below.
 */
class CEditApiInstance {
public:
	int AssignScope(const char *name, unsigned char scope);
};

int CEditApiInstance::AssignScope(const char *name, unsigned char scope)
{
	((const char **)((char *)this + 0x8))[scope] = name;
	return 1;
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

/* .text+0x08056ed0, 295 bytes. Confirmed genuinely non-empty despite this table's
 * own zero-initialized placeholder data -- see config_info.cpp's SeqTimerInfo
 * comment (a real divide-by-zero hazard was found and avoided there, not here).
 */
void CConfigManager::ConfigureSeqTimer()
{
	if (sm_ptSeqTimerInfo == 0)
		return;

	unsigned *info = (unsigned *)sm_ptSeqTimerInfo;
	CSeqApi *seqApi = (CSeqApi *)SeqApi;

	int *entry = (int *)info[3];
	int id = *entry;
	while (id != 0) {
		int wheel = seqApi->CreateWheel(id);
		while (wheel == -1) {
			entry += 2;
			ApiWarn0("ERROR: failed creating sequence timer wheel");
			id = *entry;
			if (id == 0)
				goto doneWalk;
			wheel = seqApi->CreateWheel(id);
		}
		if (entry[1] == 1)
			seqApi->NotifyWheelFlag(wheel);
		entry += 2;
		id = *entry;
	}
doneWalk:

	if ((int)info[0] == 1)
		seqApi->SetTimerType(1, 0);
	else if ((int)info[0] == 2)
		seqApi->SetTimerType(2, 0);
	else
		seqApi->SetTimerType(0, 0);

	BPM::SetLowerLimit(info[1]);
	BPM::SetUpperLimit(info[2]);
}

/* Tier-B link-stub -- CZ string-set container dependency (247 methods), genuinely
 * out of scope for this pass. See this file's own header comment.
 */
void CConfigManager::CreateResourceFamilies() {}

/* Real per-entry table shape shared by sm_ptCreateInfo (CreateUserModules()) and
 * sm_ptFMDriverInfo (CreateFMDrivers()): {name, param1, param2} triples, terminated
 * by a null name.
 */
namespace {
struct CreateInfoEntry {
	const char *name;
	void *param1;
	void *param2;
};

/* FMApi's own vtable slot +0x2c ("get named driver factory") -- same raw-vtable-
 * dispatch idiom as this file's own ApiWarn0/ApiWarn1/ApiAssertCfgMan helpers above. */
inline void *FMApiGetDriverFactory(const char *name)
{
	typedef void *(*Fn)(void *, const char *);
	void *vtbl = *(void **)FMApi;
	Fn fn = *(Fn *)((char *)vtbl + 0x2c);
	return fn(FMApi, name);
}

/* FMApi's own vtable slot +0x30 ("register a constructed driver"). */
inline int FMApiRegisterDriver(void *driver, void *arg)
{
	typedef int (*Fn)(void *, void *, void *);
	void *vtbl = *(void **)FMApi;
	Fn fn = *(Fn *)((char *)vtbl + 0x30);
	return fn(FMApi, driver, arg);
}
} // namespace

/* .text+0x08056440, 778 bytes. Walks sm_ptCreateInfo, looking each entry's name up
 * in CModuleManager's own mConstructors array (g_poModuleManager+0x28 count /
 * +0x30 array -- module_manager.h) by linear by-name scan (real code is Duff's-
 * device-unrolled; collapsed to a plain loop here, same license as
 * omega_ptr_array.cpp's own collapses). On a match, calls the found
 * CModuleConstructor's own vtable-slot+8 "Create" method with (param1, param2, an
 * auto-incrementing per-constructor counter at the constructor object's own +8
 * field), then -- on success -- adds the newly-created module straight into
 * mModules via COmegaPtrArray::Add(g_poModuleManager+4, ...), deliberately NOT
 * through CModuleManager::AddModule() (a real difference in the original
 * disassembly, preserved as found: this path skips AddModule()'s own duplicate-by-
 * name check/removal and host-notify logic entirely).
 */
void CConfigManager::CreateUserModules()
{
	if (sm_ptCreateInfo == 0)
		return;

	CreateInfoEntry *entry = (CreateInfoEntry *)sm_ptCreateInfo;
	while (entry->name != 0) {
		char *mgr = (char *)g_poModuleManager;
		int count = *(int *)(mgr + 0x28);
		void **ctorArray = *(void ***)(mgr + 0x30);

		void *found = 0;
		for (int i = 0; i < count; i++) {
			const char *ctorName = *(char **)((char *)ctorArray[i] + 4);
			if (strcmp(entry->name, ctorName) == 0) {
				found = ctorArray[i];
				break;
			}
		}

		if (found == 0) {
			ApiWarn1("CConfigManager::CreateUserModules() ERROR: module factory <%s> "
			         "unavailable.", entry->name);
		} else {
			int *ctorObj = (int *)found;
			int counter = ctorObj[2];
			typedef void *(*CreateFn)(void *, void *, void *, int);
			void *ctorVtbl = *(void **)ctorObj;
			CreateFn create = *(CreateFn *)((char *)ctorVtbl + 8);
			ctorObj[2] = counter + 1;
			void *newModule = create(ctorObj, entry->param1, entry->param2, counter);

			if (newModule == 0) {
				/* Real quirk, preserved as found: the original decompile's own error
				 * message prints the entry's param1 field here (`puVar1[1]`), not the
				 * name that was actually searched for (__s1/entry->name) -- not
				 * "fixed" to use the name instead.
				 */
				ApiWarn1("CConfigManager::CreateUserModules() ERROR: unable to create "
				         "instance of user module <%s>.", entry->param1);
			} else {
				((COmegaPtrArray *)(mgr + 4))->Add(newModule);
			}
		}

		entry++;
	}
}

/* .text+0x08056760, 204 bytes. Same {name, param1, param2}-table-walk shape as
 * CreateUserModules(), but looks its factory up through FMApi's own vtable slot
 * +0x2c instead of CModuleManager's mConstructors, and -- on a successful create --
 * additionally registers the new driver through FMApi's vtable slot +0x30,
 * destroying it (vtable slot+4, the deleting dtor) if registration reports failure
 * (return value 0).
 */
void CConfigManager::CreateFMDrivers()
{
	if (sm_ptFMDriverInfo == 0)
		return;

	CreateInfoEntry *entry = (CreateInfoEntry *)sm_ptFMDriverInfo;
	for (; entry->name != 0; entry++) {
		void *factory = FMApiGetDriverFactory(entry->name);
		if (factory == 0) {
			ApiWarn1("ConfigManager::CreateFMDrivers() ERROR: driver factory <%s> "
			         "unavailable.", entry->name);
			continue;
		}

		int *factoryObj = (int *)factory;
		int counter = factoryObj[2];
		typedef void *(*CreateFn)(void *, void *, void *, int);
		void *factoryVtbl = *(void **)factoryObj;
		CreateFn create = *(CreateFn *)((char *)factoryVtbl + 8);
		factoryObj[2] = counter + 1;
		void *driver = create(factoryObj, entry->param1, entry->param2, counter);

		if (driver == 0) {
			/* Same real quirk as CreateUserModules(): prints param1 (`piVar4[1]`),
			 * not the name that was actually searched for.
			 */
			ApiWarn1("ConfigManager::CreateFMDrivers() ERROR: unable to create "
			         "instance of driver <%s>.", entry->param1);
			continue;
		}

		int registered = FMApiRegisterDriver(driver, entry->param2);
		if (registered == 0) {
			typedef void (*DtorFn)(void *);
			void *driverVtbl = *(void **)driver;
			DtorFn dtor = *(DtorFn *)((char *)driverVtbl + 4);
			dtor(driver);
		}
	}
}

/* .text+0x08056d80, 1 byte -- confirmed genuinely empty (`return;`) in the real
 * binary, same "read every one, don't assume" treatment as SetupRouting's own
 * siblings in this file.
 */
void CConfigManager::SetupRouting() {}

/* .text+0x08056840, 511 bytes. */
void CConfigManager::LinkRTRouterTracks()
{
	CRTRouterApi *rtRouterApi = (CRTRouterApi *)RTRouterApi;
	rtRouterApi->Reset();

	if (sm_ptRTRouterInfo == 0)
		return;

	const RTRouterInfoEntry *entry = (const RTRouterInfoEntry *)sm_ptRTRouterInfo;
	while (entry->senderName != 0) {
		int senderHandle = rtRouterApi->FindHandle(entry->senderName);
		int receiverHandle = rtRouterApi->FindHandle(entry->receiverName);

		if (senderHandle == 0 || receiverHandle == 0) {
			if (senderHandle == 0)
				ApiWarn1("ERROR: RTRouter unable to find handle for sender module <%s>.",
				         entry->senderName);
			if (receiverHandle == 0)
				ApiWarn1("ERROR: RTRouter unable to find handle for receiver module <%s>.",
				         entry->receiverName);
		} else {
			rtRouterApi->ConnectTracks(senderHandle, receiverHandle, entry->flagSrc == 0);
			for (int i = 0; i < entry->pairCount; i++) {
				rtRouterApi->LinkTrackPair(senderHandle, receiverHandle,
				                            entry->pairs[i * 2], entry->pairs[i * 2 + 1]);
			}
		}
		entry++;
	}
}

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

/* .text+0x08056d90, 312 bytes. Real per-entry stride: 20 dwords (0x50 bytes) --
 * confirmed by re-deriving the real disassembly's own [ebx+N] offsets across the
 * 4 unrolled dispatch calls plus the trailing "next entry" field. Each entry can
 * carry 1-4 real connections; a 0 in any sub-slot's own "next" field (dword index
 * 5/10/15) ends that entry's own dispatch chain early. See ApiConnect()'s own
 * comment above for the Api+0x44 slot identification.
 */
void CConfigManager::MakeConnections()
{
	if (sm_ptConnectInfo == 0)
		return;

	int *p = (int *)sm_ptConnectInfo;
	while (*p != 0) {
		ApiConnect(p[0], p[1], p[2], p[3], p[4]);
		if (p[5] == 0) return;
		ApiConnect(p[5], p[6], p[7], p[8], p[9]);
		if (p[10] == 0) return;
		ApiConnect(p[10], p[11], p[12], p[13], p[14]);
		if (p[15] == 0) return;
		ApiConnect(p[15], p[16], p[17], p[18], p[19]);
		p += 0x14;
	}
}

/* .text+0x08056a50, 316 bytes. The one unconditional bootstrap Register() call
 * (registering CResMan/"ResourceManager" itself as a chunk server) and the
 * Finalize()/IsOK() assert tail both execute regardless of table contents --
 * confirmed genuinely live given this pass's own non-null (if zeroed)
 * sm_ptChunkInfo placeholder, unlike the per-entry loop body itself (which is
 * dead, first entry's own id field already 0). IsOK()'s real return value is
 * whatever EvaVTableStub happens to leave in EAX (omega_vtables.cpp) -- not
 * guaranteed 1, so the assert branch may or may not fire; either way it's a
 * harmless EvaVTableStub-backed no-op call (Api+0x94), same as every other
 * assert-report path in this file.
 */
void CConfigManager::RegisterChunkServer()
{
	if (sm_ptChunkInfo == 0)
		return;

	CChkApi *chkApi = (CChkApi *)ChkApi;
	chkApi->Register(CResMan::SysName, 5, CResMan::sm_kbyCurrentVersion,
	                  CResMan::sm_kbyCurrentRelease, 1, 0, true);

	const ChunkInfoEntry *entry = (const ChunkInfoEntry *)sm_ptChunkInfo;
	while (entry->name != 0) {
		int result = chkApi->Register(entry->name,
		                                (char)entry->word1,
		                                *((const unsigned char *)&entry->word1 + 1),
		                                *((const unsigned char *)&entry->word1 + 2),
		                                entry->i1, entry->i2, entry->flagSrc == 0);
		while (result == 0) {
			const char *failedName = entry->name;
			entry++;
			ApiWarn1("ERROR: unable to register chunk server module <%s>.", failedName);
			if (entry->name == 0)
				goto finalize;
			result = chkApi->Register(entry->name,
			                           (char)entry->word1,
			                           *((const unsigned char *)&entry->word1 + 1),
			                           *((const unsigned char *)&entry->word1 + 2),
			                           entry->i1, entry->i2, entry->flagSrc == 0);
		}
		entry++;
	}

finalize:
	chkApi->Finalize();
	if (chkApi->IsOK() != 1)
		ApiAssertCfgMan(0xc5);
}
