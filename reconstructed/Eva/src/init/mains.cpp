/*
 * mains.cpp  -  see include/mains.h.
 *
 * Mains() transcribed from Decomp/EVA_Decomp/eva_export/functions/Mains@0804d9e0.c
 * (365 bytes): fetches COmegaInterface::GetSysApi() fresh before *every* one of the
 * 17 calls below (not cached once) -- preserved literally, not "optimized" into a
 * single fetch, since it's exactly what the real binary does.
 *
 * The 17 MMainXxx(CSystemApi*, ...) functions were each individually read from their
 * own decompile files (addresses in the per-function comments below). All 17 return
 * `undefined4 0` unconditionally in the real binary; Mains() never checks the return
 * value, so it's collapsed to void here (mains.h) rather than threading a
 * never-observed constant through every wrapper.
 *
 * Two real patterns, confirmed by reading every one of the 17:
 *
 * 1. Registration shim over a small heap "module descriptor" (15 of 17): build a
 *    3-word {vtbl, namePtr, reserved} object, base-construct it with the generic
 *    CNamedObjectBase vtable, copy in a name string (real code encodes the name as a
 *    handful of packed dword/word/byte literal stores -- a GCC inlined-strcpy-of-a-
 *    literal artifact, replaced here with an actual strcpy() of the same bytes; see
 *    RegisterModuleDescriptor()), then overwrite the vtable with the module's own
 *    real PTR__CXxxConstructor vtable and register the descriptor through a
 *    CSystemApi-shaped object's vtable slot +0x40. The 17 real per-module
 *    "ModuleConstructor" vtables this ultimately installs are NOT reconstructed --
 *    each is a distinct real class with its own construction logic, genuinely Stage
 *    4/5 depth (see README.md). Declared here only as opaque extern data symbols
 *    (the real vtables already exist in the binary), installed byte-for-byte
 *    correct, but never dereferenced/dispatched through by this reconstruction.
 *
 *    12 of the 15 guard with `if (Api == 0) Api = api;` before registering
 *    (Editor/BatchDiskMan/ESCommon/ESProg/ESEffect/ESCombi/ESGlobal/ESMOSS/
 *    ESSampling/ESSetList/ESSong/ESDisk); 2 do not (AlphaKeybCtrl, Panel) -- real
 *    difference, preserved as found, not "fixed" into a consistent guard. Since
 *    Mains() always calls MMainPanelDriver first and *that* one (non-descriptor,
 *    see below) always runs unconditionally, Api is in practice already set by the
 *    time any of these run -- the guard is real but not load-bearing on this call
 *    path.
 *
 *    MMainLinuxDriver is the one outlier within this group: it *does* register a
 *    descriptor, but through a different, lazily-fetched CSystemApi-shaped object
 *    (`FMApi`, obtained via a virtual call through Api's own vtable slot +0xa0) at
 *    vtable slot +0x24, not through Api at +0x40. Preserved exactly.
 *
 * 2. Direct construction of a real, already-reconstructed-elsewhere-eventually
 *    driver class, registered through a CSystemApi-shaped object's vtable slot
 *    +0xb4 instead of +0x40 (2 of 17): MMainPanelDriver (CLinuxPanelDriver, no
 *    idempotency guard) and MMainHIDDriver (CHIDDriver, has the guard). Both real
 *    driver classes' own constructors are declared here as call-contract externs
 *    (__thiscall, confirmed from functions.csv) -- not implemented, Stage 4+.
 */

#include "mains.h"
#include "omega_interface.h"
#include "omega_vtables.h"
#include "module.h"
#include "module_manager.h"
#include "sysapi_instance.h"
#include "ustg_user_api.h"
#include "global_object_base.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* Module-scope globals the real disassembly references directly by these names
 * (symbols.csv confirms both as plain Global-namespace data labels, not class
 * members) -- Api is Eva's primary CSystemApi singleton; FMApi is a secondary sub-API,
 * lazily fetched only by MMainLinuxDriver.
 *
 * CORRECTED 2026-07-23: Api is NOT actually set by any guarded MMainXxx in this file
 * (the `if (Api == 0) Api = api;` guards you'll see below are real, but on this
 * project's own traced boot path they're always no-ops, since InitSystemLayer() and
 * every one of its own MMainXxx(void) calls -- including MMainEditMan(), which
 * dereferences Api unconditionally with NO guard -- run before Mains() ever does).
 * The real producer is SysApiInstance's own static constructor
 * (sysapi_instance.cpp): `Api = SysApiInstance;` runs before main(), full mechanism
 * documented in global_object_base.h. This is what fixes the MMainEditMan() NULL-Api
 * crash found via a live kronos_vm boot test. Kept as a plain (zero-initialized)
 * definition here, not `extern`, since this .cpp is still its canonical definition
 * site -- sysapi_instance.cpp only `extern`s it (matching module.cpp's existing
 * convention) and writes it once, before anything in this file's own code runs.
 */
CSystemApi *Api = 0;
CSystemApi *FMApi = 0;

/* Real global CKernel::InitSystemLayer's own MMainXxx(void) family doesn't need
 * (that one is unrelated, see ckernel.cpp) -- this is the literal constant argument
 * MMainLinuxDriver passes when fetching FMApi through Api's vtable slot +0xa0, and
 * (per the InitSystemLayer-adjacent MMainXxx(void) family below) also the argument
 * MMainFileMan passes when registering FMApiInstance. Meaning/value not decoded (some
 * kind of named-sub-API id/index); real extern data symbol, zero-initialized here
 * since its real rodata/data-segment initial value isn't recovered.
 */
extern "C" int DAT_0930b174 = 0;

/* Base-subobject vtable every one of the 15 descriptor-pattern module objects
 * installs before overwriting it with its own real per-module vtable -- the same
 * symbol OA-side code elsewhere in the binary uses for any minimal named object.
 * Real definition + real slot count now in omega_vtables.h/.cpp (included above).
 */

/* The 15 real per-module "ModuleConstructor" vtables -- see file header. Real,
 * existing rodata symbols; declared extern, not fabricated, never dereferenced here.
 */
extern "C" {
void *PTR__CAlphaKeybCtrlConstructor_08eabb48;
void *PTR__CLinuxDriverConstructor_08fdaab0;
void *PTR__CEditorConstructor_08f29c10;
void *PTR__CPanelConstructor_08f7c2f0;
void *PTR__CBatchDiskManConstructor_08eabe08;
void *PTR__CESCommonModuleConstructor_08fbb048;
void *PTR__CESProgModuleConstructor_08fbd218;
void *PTR__CESEffectModuleConstructor_08fbb2c8;
void *PTR__CESCombiModuleConstructor_08fbe028;
void *PTR__CESGlobalModuleConstructor_08fbea28;
void *PTR__CESMOSSModuleConstructor_08fbbe48;
void *PTR__CESSamplingModuleConstructor_08fc6a48;
void *PTR__CESSetListModuleConstructor_08fd37a8;
void *PTR__CESSongModuleConstructor_08fc2818;
void *PTR__CESDiskModuleConstructor_08fcc0a8;
}

/* The 2 real driver classes MMainPanelDriver/MMainHIDDriver construct directly.
 * __thiscall ctors confirmed from functions.csv; bodies not reconstructed (Stage
 * 4+) -- both derive from the same CNamedObjectBase base (their own ctors install
 * PTR__CNamedObjectBase_08e81378 first, same pattern as the descriptor objects).
 */
/* Real vtables these 2 ctors install -- opaque, never dispatched through by any
 * reconstructed code (same "install but never dispatch" treatment as the 15
 * ModuleConstructor vtables above).
 */
extern "C" void *PTR__CHIDDriver_08fd9ce8 = 0;
extern "C" void *PTR__CLinuxPanelDriver_08fd9dc8 = 0;

class CLinuxPanelDriver {
public:
	/* .text+0x08e50050, 91 bytes -- reconstructed. */
	CLinuxPanelDriver(const char *name);

private:
	void *mVtbl;
	char *mName;
};

class CHIDDriver {
public:
	/* .text+0x08e4fd50, 132 bytes -- reconstructed. */
	CHIDDriver(const char *name, const char *eventsName, const char *commandsName);

private:
	void *mVtbl;
	char *mName;
	int   mFd;         /* +0x08, ctor sets -1 */
	int   mUnknown0c;  /* +0x0c, ctor zeroes */
	int   mUnknown10;  /* +0x10, ctor zeroes */
	char  mUnknown14;  /* +0x14, ctor zeroes (byte write in the real disassembly) */
	int   mUnknown18;  /* +0x18, ctor zeroes */
	int   mUnknown1c;  /* +0x1c, ctor zeroes */
	short mUnknown20;  /* +0x20, ctor zeroes */
	short mUnknown22;  /* +0x22, ctor zeroes */
	int   mUnknown24;  /* +0x24, ctor zeroes */
};

CLinuxPanelDriver::CLinuxPanelDriver(const char *name)
{
	mVtbl = PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = new char[len + 1];
	mName = dup;
	strcpy(dup, name);

	mVtbl = &PTR__CLinuxPanelDriver_08fd9dc8;

	USTGUserAPI::ConnectPanelFifo();
}

CHIDDriver::CHIDDriver(const char *name, const char * /*eventsName*/, const char * /*commandsName*/)
{
	mVtbl = PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = new char[len + 1];
	mName = dup;
	strcpy(dup, name);

	mVtbl = &PTR__CHIDDriver_08fd9ce8;
	mFd = -1;
	mUnknown0c = 0;
	mUnknown14 = 0;
	mUnknown10 = 0;
	mUnknown18 = 0;
	mUnknown1c = 0;
	mUnknown20 = 0;
	mUnknown22 = 0;
	mUnknown24 = 0;
}

namespace {

/* Real vtable-slot dispatch helper -- same `(**(code**)(*obj + slot))(obj, arg)`
 * idiom used throughout ckernel.cpp/omega_interface.cpp for classes whose real
 * vtable layout isn't reconstructed.
 */
inline void CallVSlot(void *obj, int byteOffset, void *arg)
{
	typedef void (*Fn)(void *, void *);
	void *vtbl = *(void **)obj;
	Fn fn = *(Fn *)((char *)vtbl + byteOffset);
	fn(obj, arg);
}

/* Shared helper behind 15 of the 17 MMainXxx wrappers -- builds the generic 3-word
 * module descriptor object and registers it through `dispatchTarget`'s vtable slot
 * +0x40. `dispatchTarget` is normally Api, except MMainLinuxDriver's FMApi.
 */
void RegisterModuleDescriptor(void *dispatchTarget, void *moduleVtable, const char *name)
{
	void **descriptor = (void **)malloc(0xc);
	descriptor[0] = PTR__CNamedObjectBase_08e81378;
	descriptor[1] = 0;

	char *nameBuf = (char *)malloc(strlen(name) + 1);
	strcpy(nameBuf, name);
	descriptor[1] = nameBuf;

	descriptor[2] = 0;
	descriptor[0] = moduleVtable;

	CallVSlot(dispatchTarget, 0x40, descriptor);
}

} // namespace

void Mains()
{
	CSystemApi *sysApi;

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainPanelDriver(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainHIDDriver(sysApi, "KeyboardEvents", "KeyboardCommands");

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainAlphaKeybCtrl(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainLinuxDriver(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainEditor(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainPanel(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainBatchDiskMan(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESCommon(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESProg(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESEffect(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESCombi(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESGlobal(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESMOSS(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESSampling(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESSetList(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESSong(sysApi);

	sysApi = (CSystemApi *)COmegaInterface::GetSysApi();
	MMainESDisk(sysApi);
}

/* .text+0x08e4fe70, 88 bytes. No idempotency guard -- uses the module-global Api
 * directly (already set by whatever ran before Mains(), not param `api`).
 */
void MMainPanelDriver(CSystemApi * /*api*/)
{
	/* Real malloc(8) + placement CLinuxPanelDriver::CLinuxPanelDriver(this,"PanelDriver"),
	 * same collapse convention as omega_interface.cpp's `new CKernel(0)` -- 8 bytes is
	 * CLinuxPanelDriver's real observed size, preserved via placement new rather than a
	 * bare `new CLinuxPanelDriver(...)` (which would allocate this TU's own, wrong,
	 * not-reconstructed sizeof instead).
	 */
	void *raw = malloc(8);
	CLinuxPanelDriver *driver = new (raw) CLinuxPanelDriver("PanelDriver");
	CallVSlot(Api, 0xb4, driver);
}

/* .text+0x08e4f750, 120 bytes. Has the idempotency guard. */
void MMainHIDDriver(CSystemApi *api, const char *eventsName, const char *commandsName)
{
	if (Api == 0)
		Api = api;

	/* Real malloc(0x28) + placement construct -- see MMainPanelDriver's comment above. */
	void *raw = malloc(0x28);
	CHIDDriver *driver = new (raw) CHIDDriver("HIDDriver", eventsName, commandsName);
	CallVSlot(Api, 0xb4, driver);
}

/* .text+0x0823e840, 177 bytes. No idempotency guard. Real name bytes decode to
 * "AlphaKeybCtrlClass" (18 chars); confirmed by hand from the packed dword/word
 * literal stores in the decompile.
 */
void MMainAlphaKeybCtrl(CSystemApi * /*api*/)
{
	RegisterModuleDescriptor(Api, &PTR__CAlphaKeybCtrlConstructor_08eabb48, "AlphaKeybCtrlClass");
}

/* .text+0x08e57680, 229 bytes. Has the idempotency guard on Api, plus a second,
 * independent lazy fetch of FMApi through Api's own vtable slot +0xa0 (arg
 * DAT_0930b174) -- only done once (guarded on FMApi == 0). Registers through
 * FMApi at slot +0x24, NOT through Api at +0x40 -- the one real outlier in this
 * family. Name decodes to "LinuxDriver".
 */
void MMainLinuxDriver(CSystemApi *api)
{
	if (Api == 0)
		Api = api;

	if (FMApi == 0) {
		typedef void *(*GetSubApiFn)(void *, int);
		void *vtbl = *(void **)Api;
		GetSubApiFn fn = *(GetSubApiFn *)((char *)vtbl + 0xa0);
		FMApi = (CSystemApi *)fn(Api, DAT_0930b174);
	}

	void **descriptor = (void **)malloc(0xc);
	descriptor[0] = PTR__CNamedObjectBase_08e81378;
	descriptor[1] = 0;

	char *nameBuf = (char *)malloc(strlen("LinuxDriver") + 1);
	strcpy(nameBuf, "LinuxDriver");
	descriptor[1] = nameBuf;

	descriptor[2] = 0;
	descriptor[0] = &PTR__CLinuxDriverConstructor_08fdaab0;

	CallVSlot(FMApi, 0x24, descriptor);
}

/* .text+0x08249fb0, 183 bytes. Has the idempotency guard. Name decodes to
 * "EditorClass".
 */
void MMainEditor(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CEditorConstructor_08f29c10, "EditorClass");
}

/* .text+0x089ee440, 163 bytes. No idempotency guard. Name decodes to "PanelClass". */
void MMainPanel(CSystemApi * /*api*/)
{
	RegisterModuleDescriptor(Api, &PTR__CPanelConstructor_08f7c2f0, "PanelClass");
}

/* .text+0x08240ef0, 196 bytes. Has the idempotency guard. Name decodes to
 * "BatchDiskManClass".
 */
void MMainBatchDiskMan(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CBatchDiskManConstructor_08eabe08, "BatchDiskManClass");
}

/* .text+0x08bd1e60, 194 bytes. Has the idempotency guard. Name decodes to
 * "CommonEditServer".
 */
void MMainESCommon(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESCommonModuleConstructor_08fbb048, "CommonEditServer");
}

/* .text+0x08bfd8e0, 193 bytes. Has the idempotency guard. Name decodes to
 * "ProgEditServer".
 */
void MMainESProg(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESProgModuleConstructor_08fbd218, "ProgEditServer");
}

/* .text+0x08bea9c0, 194 bytes. Has the idempotency guard. Name decodes to
 * "EffectEditServer".
 */
void MMainESEffect(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESEffectModuleConstructor_08fbb2c8, "EffectEditServer");
}

/* .text+0x08c4b130, 190 bytes. Has the idempotency guard. Name decodes to
 * "CombiEditServer".
 */
void MMainESCombi(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESCombiModuleConstructor_08fbe028, "CombiEditServer");
}

/* .text+0x08c5eca0, 194 bytes. Has the idempotency guard. Name decodes to
 * "GlobalEditServer".
 */
void MMainESGlobal(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESGlobalModuleConstructor_08fbea28, "GlobalEditServer");
}

/* .text+0x08bedd80, 193 bytes. Has the idempotency guard. Name decodes to
 * "MOSSEditServer".
 */
void MMainESMOSS(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESMOSSModuleConstructor_08fbbe48, "MOSSEditServer");
}

/* .text+0x08d61b00, 200 bytes. Has the idempotency guard. Name decodes to
 * "SamplingEditServer".
 */
void MMainESSampling(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESSamplingModuleConstructor_08fc6a48, "SamplingEditServer");
}

/* .text+0x08e0a280, 196 bytes. Has the idempotency guard. Name decodes to
 * "SetListEditServer".
 */
void MMainESSetList(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESSetListModuleConstructor_08fd37a8, "SetListEditServer");
}

/* .text+0x08c95fe0, 193 bytes. Has the idempotency guard. Name decodes to
 * "SongEditServer".
 */
void MMainESSong(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESSongModuleConstructor_08fc2818, "SongEditServer");
}

/* .text+0x08ddc580, 193 bytes. Has the idempotency guard. Name decodes to
 * "DiskEditServer".
 */
void MMainESDisk(CSystemApi *api)
{
	if (Api == 0)
		Api = api;
	RegisterModuleDescriptor(Api, &PTR__CESDiskModuleConstructor_08fcc0a8, "DiskEditServer");
}

/* ===========================================================================
 * A THIRD, genuinely different MMainXxx family: the 9 void(void) functions
 * CKernel::InitSystemLayer() calls (ckernel.cpp) -- MMainEditMan/MMainViewer/
 * MMainSeqTimer/MMainFileMan/MMainSysEx/MMainChunkMan/MMainRTRouter/MMainDumpMan/
 * MMainResMan. Investigated per this batch's own instruction (don't assume shape):
 * these are NOT cheap registration shims like the 17-member family above, and NOT
 * the same shape as InitSystemLayer's OWN 12 other callees either. Each:
 *   1. Registers a named sub-API through Api's vtable slot +0xa4 (a pattern not seen
 *      anywhere else in this reconstruction) -- except MMainSysEx, which calls
 *      CSysApiInstance::RegisterApi() by name instead, and MMainRTRouter, which does
 *      ONLY this step (no module construction at all -- the smallest of the 9, 45
 *      bytes).
 *   2. For the other 7: malloc's a real per-subsystem module object and either (a)
 *      base-constructs it via the shared CModule::CModule(name) ctor then manually
 *      vtable-swaps in the module's own real vtable -- CEditMan/CViewBase+
 *      CMessagePort/CSeqTimer/CSysEx-module/CChunkMan/CDumpManMod, no real derived
 *      ctor ever called, same manual-vtable-swap idiom as everywhere else in this
 *      project -- or (b) calls a real, distinct derived-class constructor
 *      (CFileMan::CFileMan/CResMan::CResMan) that itself is hundreds-to-thousands of
 *      bytes deep (0xa5c / 0x21a0 malloc sizes) -- genuinely Stage 4/5 depth, out of
 *      scope for this pass, same boundary as CFileMan/CResMan's own full class bodies
 *      being out of scope everywhere else in this project.
 *   3. Registers the new module via CSysApiInstance::AddModule() (real 22-byte
 *      forwarder, reconstructed -- see sysapi_instance.cpp).
 *   4. 3 of the 9 (MMainSysEx/MMainChunkMan/MMainResMan) do one extra step: hand the
 *      new module to its own API instance object via a named setter
 *      (CChkApiInstance::SetOwnerModule/CRMApiInstance::SetResMan).
 *
 * All 9 wrapper functions themselves are transcribed faithfully (Tier A, all under
 * 130 bytes, fully mechanical). Every module CONSTRUCTOR is real (CModule::CModule,
 * module.cpp) except CFileMan::CFileMan/CResMan::CResMan, which are Tier-B link-stubs
 * (empty bodies) -- the two genuinely too-deep derived ctors in this family. The 6
 * real per-module vtables these install, the 8 real API-instance globals, and the
 * handful of real but undecoded DAT_ constants passed to the +0xa4 registration slot
 * are all opaque Tier-B placeholders, same "install/pass but never dispatch or read
 * back" treatment as the 15+2 driver/module vtables above.
 * ===========================================================================
 */

/* Real per-module vtables (opaque -- never dispatched through by any reconstructed
 * code; every derived module class itself is out of scope, see file header above).
 */
extern "C" {
void *PTR__CEditMan_08e85ea8;
void *PTR__CMessagePort_08e88468;
void *PTR__CSeqTimer_08e892a8;
void *PTR__CSysEx_08e899e8;
void *PTR__CChunkMan_08e85968;
void *PTR__CDumpManMod_08e85ca8;
}

/* Real name-string constants registered against Api's vtable slot +0xa4 --
 * CORRECTED 2026-07-23: these are NOT undecoded integers. Ground-truth confirmed two
 * ways: (1) reading every one of the corresponding 9 XxxApiInstance globals' own real
 * `global.constructors.keyed.to.<Name>@<addr>.c` decompile (Eva_export) shows each one
 * assigned a plain C string literal (`DAT_0930aae8 = "EditApi";` etc.) right after its
 * paired `XxxApi = XxxApiInstance;`; (2) a direct raw-byte read of the real binary at
 * Api's own installed vtable slot +0xa4 (.rodata+08e81008+0xa4) resolves to exactly
 * CSysApiInstance::RegisterApi's real .text address (0806bab0, sysapi_instance.h) --
 * i.e. every one of the 8 raw-dispatch call sites below was really calling
 * `((CSysApiInstance*)Api)->RegisterApi(name, instance)` with `name` being one of
 * these 9 strings, exactly matching MMainSysEx's own pre-existing direct call (the
 * "one real outlier" earlier Stage notes flagged -- it isn't an outlier in mechanism,
 * only in calling style). All 8 raw-dispatch call sites below are now direct
 * RegisterApi() calls; the `int` DAT_x declarations and the raw `(**(code**)(*Api+
 * 0xa4))(...)` dispatch pattern they used are gone.
 */
extern "C" const char *DAT_0930aae8 = "EditApi";      /* MMainEditMan */
extern "C" const char *DAT_0931b20c = "SeqApi";       /* MMainSeqTimer */
extern "C" const char *DAT_0930a6ac = "ChkApi";       /* MMainChunkMan */
extern "C" const char *_DAT_0930a324 = "RTRouterApi"; /* MMainRTRouter */
extern "C" const char *DAT_0930a6bc = "DumpApi";      /* MMainDumpMan */
extern "C" const char *DAT_0931b1f0 = "RMApi";        /* MMainResMan */
extern "C" const char *_DAT_0931b314 = "SysExApi";    /* MMainSysEx */

/* Real API-instance globals these 9 functions register modules against / pass to
 * RegisterApi() as the "instance" argument. Every one is a real object in the real
 * binary; declared here as byte buffers, sized to the real max offset each object's
 * own static constructor below writes (see each ctor's own comment) -- contents
 * otherwise unfaithful (zero-initialized), matching config_info.cpp's own
 * placeholder-table convention. EditApiInstance is also used (read-only, on an
 * unreachable path given zeroed config data) by config_manager.cpp's
 * AssignEditServerIDs() -- shared here as the one real global, not redefined per-file.
 *
 * CORRECTED 2026-07-23: EditApiInstance was previously declared `void *EditApiInstance
 * = 0;` -- a null POINTER, when the real global is (like the other 6 non-scalar-typed
 * siblings here) the OBJECT ITSELF; passing its bare name as an "instance" argument
 * was therefore passing NULL, not the object's own address. Fixed to a properly-sized
 * byte buffer, same shape as its siblings. SeqApiInstance/ChkApiInstance/
 * DumpApiInstance/RMApiInstance/RTRouterApiInstance were also undersized relative to
 * what their own real static constructors (below) actually write -- bumped to real
 * sizes; g_oSysExApiInstance/FMApiInstance were already large enough.
 */
extern "C" unsigned char EditApiInstance[0x404] = {};
static unsigned char SeqApiInstance[8] = {};
static unsigned char FMApiInstance[0x4e0] = {};
static unsigned char g_oSysExApiInstance[0x40] = {};
static unsigned char ChkApiInstance[4] = {};
static unsigned char DumpApiInstance[8] = {};
static unsigned char RMApiInstance[0x2c] = {};
static unsigned char RTRouterApiInstance[0x1c] = {};

/* Real paired "XxxApi" singleton pointers these 7 static constructors set (SysExApi
 * is also real but its own paired constant -- _DAT_0931b314 -- is already the string
 * MMainSysEx passes to RegisterApi() directly, so it's declared right alongside).
 * None are read by any other reconstructed code yet (nothing here parallels
 * MMainLinuxDriver's own lazy FMApi fetch for any of these 7) -- declared for
 * shape/mechanism fidelity, matching the real binary's own global layout.
 */
void *EditApi = 0;
void *SeqApi = 0;
void *ChkApi = 0;
void *DumpApi = 0;
void *RMApi = 0;
void *RTRouterApi = 0;
void *SysExApi = 0;

/* Opaque real per-class vtables these 7 static constructors install -- same
 * "install but never dispatch" treatment as PTR__CHIDDriver_08fd9ce8/
 * PTR__CLinuxPanelDriver_08fd9dc8 above (nothing in this reconstruction ever
 * dispatches through an XxxApiInstance object's own vtable -- RegisterApi()/
 * AssignScope() etc. are all called directly by name, not through these).
 */
extern "C" void *PTR__CEditApiInstance_08e85da8 = 0;
extern "C" void *PTR__CSeqApiInstance_08e88fa8 = 0;
extern "C" void *PTR__CChkApiInstance_08e855c8 = 0;
extern "C" void *PTR__CDumpApiInstance_08e85ba8 = 0;
extern "C" void *PTR__CRTRouterApiInstance_08e822e8 = 0;
extern "C" void *PTR__CSysExApiInstance_08e89a28 = 0;
/* RMApiInstance's own ctor transiently installs these 2 before overwriting with its
 * real, final CRMApiInstance vtable (see the ctor below) -- included for the same
 * shape-fidelity reason, never left installed nor dispatched through.
 */
extern "C" void *PTR__CRMApi_08e88de8 = 0;
extern "C" void *PTR__CRMApiCallBack_08e886e8 = 0;
extern "C" void *PTR__CRMApiInstance_08e88c48 = 0;
extern "C" void *DAT_08e88d80 = 0; /* real 2nd vtable-like slot RMApiInstance+4 installs */

/* 7 real static constructors -- global.constructors.keyed.to.<Name>@<addr>.c
 * (Eva_export), transcribed 2026-07-23. Same CGlobalObjectBase-first idiom as
 * SysApiInstance's own (sysapi_instance.cpp) -- see global_object_base.h for the
 * shared mechanism (CKernel::AddGlobalObject registration) this gives every one of
 * these objects, not just SysApiInstance.
 */

/* global.constructors.keyed.to.EditApiInstance@080d2560.c, 212 bytes. */
__attribute__((constructor))
static void ConstructEditApiInstance()
{
	new (EditApiInstance) CGlobalObjectBase();
	*(void **)EditApiInstance = &PTR__CEditApiInstance_08e85da8;
	*(int *)(EditApiInstance + 4) = 0;
	*(int *)(EditApiInstance + 8) = 0;
	*(int *)(EditApiInstance + 12) = 0;
	/* Real: 7 iterations of a 9x16-byte SSE-store loop, zeroing [0x10, 0x400) --
	 * collapsed to one memset (same license as omega_ptr_array.cpp's Duff's-device
	 * collapses). Redundant given EditApiInstance's own `= {}` static-init, but
	 * transcribed anyway for faithfulness to the real instruction sequence.
	 */
	memset(EditApiInstance + 0x10, 0, 0x3f0);
	*(int *)(EditApiInstance + 1024) = 0;

	EditApi = EditApiInstance;
	DAT_0930aae8 = "EditApi";
}

/* global.constructors.keyed.to.SeqApiInstance@08167d30.c, 89 bytes. */
__attribute__((constructor))
static void ConstructSeqApiInstance()
{
	new (SeqApiInstance) CGlobalObjectBase();
	*(void **)SeqApiInstance = &PTR__CSeqApiInstance_08e88fa8;
	*(int *)(SeqApiInstance + 4) = 0;

	SeqApi = SeqApiInstance;
	DAT_0931b20c = "SeqApi";
}

/* global.constructors.keyed.to.ChkApiInstance@080bfd60.c, 79 bytes. */
__attribute__((constructor))
static void ConstructChkApiInstance()
{
	new (ChkApiInstance) CGlobalObjectBase();
	*(void **)ChkApiInstance = &PTR__CChkApiInstance_08e855c8;

	ChkApi = ChkApiInstance;
	DAT_0930a6ac = "ChkApi";
}

/* global.constructors.keyed.to.DumpApiInstance@080cef10.c, 89 bytes. */
__attribute__((constructor))
static void ConstructDumpApiInstance()
{
	new (DumpApiInstance) CGlobalObjectBase();
	*(void **)DumpApiInstance = &PTR__CDumpApiInstance_08e85ba8;
	*(int *)(DumpApiInstance + 4) = 2;

	DumpApi = DumpApiInstance;
	DAT_0930a6bc = "DumpApi";
}

/* global.constructors.keyed.to.RTRouterApiInstance@080878a0.c, 167 bytes. Real ctor
 * also initializes 2 unrelated file-scope globals here (kInvalidBytePair/
 * kPitchBendDefault, 2-byte pairs) -- coincidental compiler grouping (same original
 * translation unit as RTRouterApiInstance), not modeled since nothing in this
 * reconstruction reads either.
 */
__attribute__((constructor))
static void ConstructRTRouterApiInstance()
{
	new (RTRouterApiInstance) CGlobalObjectBase();
	*(void **)RTRouterApiInstance = &PTR__CRTRouterApiInstance_08e822e8;
	*(int *)(RTRouterApiInstance + 4) = 0;
	*(int *)(RTRouterApiInstance + 8) = 0;
	*(int *)(RTRouterApiInstance + 12) = 0;
	*(int *)(RTRouterApiInstance + 16) = 0;
	*(int *)(RTRouterApiInstance + 20) = 0;
	*(int *)(RTRouterApiInstance + 24) = 0;

	RTRouterApi = RTRouterApiInstance;
	_DAT_0930a324 = "RTRouterApi";
}

/* global.constructors.keyed.to.RMApiInstance@08165f70.c, 219 bytes. The one real
 * outlier in this group of 7: constructs a real sub-object (CRMJob, malloc(0x54)) in
 * the middle, and its own vtable is written twice (a transient CRMApi/CRMApiCallBack
 * pair, then overwritten with the final, real CRMApiInstance/DAT_08e88d80 pair) --
 * transcribed exactly as found. CRMJob::CRMJob() itself is Tier-B (not
 * reconstructed, raw opaque blob only, same treatment as CTracer/CErrorHandler in
 * ckernel.cpp).
 */
__attribute__((constructor))
static void ConstructRMApiInstance()
{
	new (RMApiInstance) CGlobalObjectBase();
	*(void **)RMApiInstance = &PTR__CRMApi_08e88de8;
	*(void **)(RMApiInstance + 4) = &PTR__CRMApiCallBack_08e886e8;

	void *job = malloc(0x54); /* CRMJob -- Tier-B, uninitialized raw blob */

	*(void **)RMApiInstance = &PTR__CRMApiInstance_08e88c48;
	*(void **)(RMApiInstance + 4) = &DAT_08e88d80;
	*(int *)(RMApiInstance + 32) = -1;
	*(int *)(RMApiInstance + 36) = 0;
	*(int *)(RMApiInstance + 16) = 0;
	*(int *)(RMApiInstance + 20) = 0;
	*(int *)(RMApiInstance + 24) = 0;
	*(int *)(RMApiInstance + 28) = 0;
	*(int *)(RMApiInstance + 40) = 0;
	*(void **)(RMApiInstance + 8) = job;

	RMApi = RMApiInstance;
	DAT_0931b1f0 = "RMApi";
}

/* global.constructors.keyed.to.g_oSysExApiInstance@0817a5c0.c, 97 bytes. */
__attribute__((constructor))
static void ConstructSysExApiInstance()
{
	new (g_oSysExApiInstance) CGlobalObjectBase();
	/* Real: 8-dword zero loop at +0x10..+0x2c, collapsed to one memset. */
	memset(g_oSysExApiInstance + 0x10, 0, 8 * sizeof(int));
	*(void **)g_oSysExApiInstance = &PTR__CSysExApiInstance_08e89a28;

	SysExApi = g_oSysExApiInstance;
	_DAT_0931b314 = "SysExApi";
}

/* Real per-module name strings (CModule::CModule's own arg) -- decoded by hand from
 * each module's own class-static SysName field where the real call site passes one
 * (CEditMan::SysName/CViewBase::SysName/CChunkMan::SysName); MMainSeqTimer/MMainSysEx/
 * MMainDumpMan pass a plain string literal directly in the real disassembly instead.
 * Declared as opaque extern data here (real content not decoded) for the 3 SysName
 * cases -- passing any non-null string is enough for CModule::CModule's own real
 * logic (strlen/malloc/strcpy), so an unfaithful placeholder name does not change
 * this pass's own control flow.
 */
extern "C" const char *CEditMan_SysName = "EditMan";
extern "C" const char *CViewBase_SysName = "ViewBase";
extern "C" const char *CChunkMan_SysName = "ChunkMan";

/* CFileMan::CFileMan()/CResMan::CResMan() -- .text+0x081068d0-ish/0x08160a20-ish
 * (not individually looked up), 0xa5c / 0x21a0 malloc sizes in the real MMainFileMan/
 * MMainResMan callers below. Tier-B link-stubs: real derived-class constructors,
 * genuinely too deep for this pass (each is its own large subsystem, matching
 * PLAN.md's "UI/CForm/Peg-scale breadth is out of scope" boundary).
 */
class CFileMan {
public:
	CFileMan() {}
};

class CResMan {
public:
	CResMan() {}
};

/* CChkApiInstance::SetOwnerModule()/CRMApiInstance::SetResMan() -- Tier-B link-stubs. */
class CChkApiInstance {
public:
	static void SetOwnerModule(void *self, void *module);
};
void CChkApiInstance::SetOwnerModule(void * /*self*/, void * /*module*/) {}

class CRMApiInstance {
public:
	static void SetResMan(void *self, void *resMan);
};
void CRMApiInstance::SetResMan(void * /*self*/, void * /*resMan*/) {}

/* .text+0x080d2a00, 111 bytes. Registers EditApiInstance via RegisterApi() (Api's
 * vtable slot +0xa4, ground-truth-confirmed to be CSysApiInstance::RegisterApi --
 * see sysapi_instance.h), then builds a base CModule("EditMan") and vtable-swaps in
 * CEditMan's own real vtable. THIS is the crash site a live kronos_vm boot test found
 * 2026-07-23 (Api was null; see mains.cpp's own Api declaration comment and
 * sysapi_instance.cpp's ConstructSysApiInstance()).
 */
void MMainEditMan()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0930aae8, (CApiBase *)EditApiInstance);

	void *raw = malloc(0x30);
	CModule *module = new (raw) CModule(CEditMan_SysName);
	*(void **)module = PTR__CEditMan_08e85ea8;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x0814d000, 89 bytes. No +0xa4 registration -- builds a base
 * CModule("ViewBase") vtable-swapped to CMessagePort's own vtable, plus 2 extra
 * fields (a real CViewBase/CMessagePort-specific short+int pair) beyond CModule's
 * own base layout.
 */
void MMainViewer()
{
	void *raw = malloc(0x34);
	CModule *module = new (raw) CModule(CViewBase_SysName);
	*(void **)module = PTR__CMessagePort_08e88468;
	*(short *)((char *)module + 0x2c) = 0;
	*(int *)((char *)module + 0x30) = 0;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x081693d0, 116 bytes. */
void MMainSeqTimer()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0931b20c, (CApiBase *)SeqApiInstance);

	void *raw = malloc(0x30);
	CModule *module = new (raw) CModule("SequenceTimer");
	*(void **)module = PTR__CSeqTimer_08e892a8;
	*(int *)((char *)module + 0x2c) = 0;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x08105a70, 101 bytes. The one MMainXxx(void) member of this family that
 * calls a real, distinct derived-class ctor (CFileMan::CFileMan, Tier-B) instead of
 * the shared CModule::CModule() base + vtable-swap idiom. Also the one member NOT
 * converted to a direct RegisterApi() call despite dispatching through the same
 * +0xa4 slot: unlike the other 6 raw-dispatch siblings, DAT_0930b174 has no matching
 * `global.constructors.keyed.to.*` producer setting it to a string (FMApiInstance
 * itself isn't a CGlobalObjectBase-style global either -- it's constructed inline by
 * this very function, via CFileMan::CFileMan()) -- Ghidra types it `undefined4` at
 * every site, including MMainLinuxDriver's own reuse of the same constant as a
 * `GetSubApiFn` integer id argument (mains.cpp, above). Left as a raw vtable
 * dispatch with its `int` type intact, not asserted to be a name string like its 6
 * siblings.
 */
void MMainFileMan()
{
	typedef void (*RegisterSubApiFn)(void *, int, void *);
	RegisterSubApiFn reg = *(RegisterSubApiFn *)((char *)*(void **)Api + 0xa4);
	reg(Api, DAT_0930b174, FMApiInstance);

	void *raw = malloc(0xa5c);
	CFileMan *fileMan = new (raw) CFileMan();
	((CSysApiInstance *)SysApiInstance)->AddModule((CModule *)fileMan);
	*(void **)(FMApiInstance + 0x4d8) = fileMan;
}

/* .text+0x08179ca0, 116 bytes. Uses CSysApiInstance::RegisterApi() by name instead
 * of the +0xa4 vtable slot -- previously documented as "the one real outlier in this
 * family"; ground-truth confirmed 2026-07-23 that it isn't an outlier in mechanism at
 * all (Api's own vtable+0xa4 slot IS RegisterApi, see sysapi_instance.h), only in
 * calling style -- the other 6 raw-dispatch siblings now call it the same way this
 * one always did.
 */
void MMainSysEx()
{
	((CSysApiInstance *)SysApiInstance)->RegisterApi(_DAT_0931b314, (CApiBase *)g_oSysExApiInstance);

	void *raw = malloc(0x2c);
	CModule *module = new (raw) CModule("SysExModule");
	*(void **)module = PTR__CSysEx_08e899e8;
	*(void **)(g_oSysExApiInstance + 4) = module;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
	*(void **)(g_oSysExApiInstance + 4) = module;
}

/* .text+0x080cb9e0, 127 bytes. */
void MMainChunkMan()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0930a6ac, (CApiBase *)ChkApiInstance);

	void *raw = malloc(0x30);
	CModule *module = new (raw) CModule(CChunkMan_SysName);
	*(void **)module = PTR__CChunkMan_08e85968;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
	CChkApiInstance::SetOwnerModule(ChkApiInstance, module);
}

/* .text+0x0807fbe0, 45 bytes. Smallest of the 9 -- +0xa4 registration only, no
 * module construction at all.
 */
void MMainRTRouter()
{
	((CSysApiInstance *)Api)->RegisterApi(_DAT_0930a324, (CApiBase *)RTRouterApiInstance);
}

/* .text+0x080cf850, 109 bytes. */
void MMainDumpMan()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0930a6bc, (CApiBase *)DumpApiInstance);

	void *raw = malloc(0x2c);
	CModule *module = new (raw) CModule("DumpManager");
	*(void **)module = PTR__CDumpManMod_08e85ca8;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x08160db0, 111 bytes. The other MMainXxx(void) member calling a real,
 * distinct derived-class ctor (CResMan::CResMan, Tier-B). */
void MMainResMan()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0931b1f0, (CApiBase *)RMApiInstance);

	void *raw = malloc(0x21a0);
	CResMan *resMan = new (raw) CResMan();
	((CSysApiInstance *)SysApiInstance)->AddModule((CModule *)resMan);
	CRMApiInstance::SetResMan(RMApiInstance, resMan);
}
