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
 *    CSystemApi-shaped object's vtable slot +0x40.
 *
 *    UPDATE (2026-07-25, CreateUserModules() unlock pass): these 15 vtables ARE
 *    now genuinely dereferenced -- see the dedicated comment right above their
 *    definitions, below. 14 of the 15 module classes behind them (everything
 *    except LinuxDriver, whose own descriptor never reaches CreateUserModules(),
 *    and except CEditor, now real) are still not reconstructed, so those 13
 *    vtables' own Create slots route through a shared safe "return NULL" stub
 *    rather than being left as bare, no-longer-safe scalar placeholders (see
 *    below for why a scalar was never actually safe once config_info.cpp's own
 *    sm_ptCreateInfo bug was fixed).
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
 * 2. Direct construction of a real driver class, registered through a
 *    CSystemApi-shaped object's vtable slot +0xb4 instead of +0x40 (2 of 17):
 *    MMainPanelDriver (CLinuxPanelDriver, no idempotency guard) and MMainHIDDriver
 *    (CHIDDriver, has the guard). Both real driver classes are now fully
 *    reconstructed -- see include/hid_driver.h/include/panel_driver.h (Stage 6
 *    breadth sweep, 2026-07-25; found via the project's broad `nm -C` class-
 *    inventory sweep -- both are small, self-contained, and boot-path-*direct*,
 *    constructed unconditionally every boot before any of Mains()'s own config-table
 *    gating applies).
 */

#include "mains.h"
#include "omega_interface.h"
#include "omega_vtables.h"
#include "module.h"
#include "module_manager.h"
#include "sysapi_instance.h"
#include "ustg_user_api.h"
#include "global_object_base.h"
#include "hid_driver.h"
#include "panel_driver.h"
#include "file_man.h"
#include "res_man.h"
#include "editor.h"

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

/* LinuxDriver's own real vtable -- unlike the 14 below, this one is registered
 * through FMApi's own vtable slot +0x24 (MMainLinuxDriver, below), not through
 * Api's slot +0x40 / CModuleManager::mConstructors. That slot is still a generic
 * EvaVTableStub (omega_vtables.cpp) which never dereferences its argument at all
 * -- so this one genuinely is safe to leave as the old bare, never-dereferenced
 * scalar placeholder (out of scope for this pass; CreateFMDrivers() stays
 * disabled, config_info.cpp).
 */
extern "C" void *PTR__CLinuxDriverConstructor_08fdaab0 = 0;

/* FIX (2026-07-25, CreateUserModules() unlock pass): the 14 real per-module
 * "ModuleConstructor" vtables below ARE genuinely dereferenced now.
 * CConfigManager::CreateUserModules() (config_manager.cpp) was a real, safe
 * no-op as long as its own sm_ptCreateInfo table (config_info.cpp) stayed
 * zeroed -- now that config_info.cpp's own bug is fixed and sm_ptCreateInfo
 * points at the REAL 14-entry {name,param1,param2} table (byte-verified
 * against Eva's own .data at 0x091ada20, config_info.cpp's own header
 * comment), CreateUserModules() genuinely looks each of these 14 names up in
 * mConstructors (already real, populated by RegisterModuleDescriptor() below
 * on every boot regardless of this fix -- Mains() itself was never gated on
 * CreateUserModules()) and dereferences the found entry's own vtable slot+8
 * ("Create"). The 14 symbols below were previously bare scalar `void*`
 * placeholders (always zero, and -- worse -- their ADDRESS was installed as
 * a fake "vtable", so slot+8 read 8 bytes past a lone 4-byte global, pure
 * memory-layout luck) -- upgraded to properly-sized 3-slot arrays
 * ([D1 dtor][D0 dtor][Create], same shape confirmed byte-exact for
 * CEditorConstructor itself via direct .rodata read, see below) so every
 * dereference is well-defined.
 *
 * Only CEditorConstructor's own Create slot does real work (placement-
 * constructs a real CEditor -- the actual point of this fix). The other 13
 * (BatchDiskMan/Panel/AlphaKeybCtrl/10x CESxxxModule) route Create through
 * ModuleFactoryCreateStub, a shared "return NULL" stub: their own real
 * per-module classes (CBatchDiskMan/CPanel/CAlphaKeybCtrl, the 10 CESxxx
 * model classes) are not reconstructed in this project, and CreateUserModules()
 * already has a real, ground-truth-faithful "unable to create instance"
 * warning path for exactly this case (config_manager.cpp) -- returning NULL is
 * the faithful, safe behavior, not a workaround. Both dtor slots point at the
 * generic EvaVTableStub since nothing in this reconstruction's call graph ever
 * destroys a CModuleConstructor object (mains.cpp only ever registers them).
 */
namespace {

void *ModuleFactoryCreateStub(void *, void *, void *, int)
{
	return 0;
}

/* .text+0x08249fb0's own real Create() (CEditorConstructor::Create,
 * .text+0x0898fb40, 61 bytes): mallocs a fresh CEditor and placement-
 * constructs it with (param1, param2) -- the ctorObj ("this") and counter
 * args are real per the vtable-slot ABI but genuinely unused by the real
 * disassembly (confirmed via objdump), same as this project's other
 * MMainPanelDriver/MMainHIDDriver "use our own real sizeof, not the ground
 * truth's oversized fixed malloc constant" convention (mains.cpp, above).
 */
void *CEditorConstructorCreate(void * /*ctorObj*/, void *name, void *alphaKeybParam, int /*counter*/)
{
	void *raw = malloc(sizeof(CEditor));
	return new (raw) CEditor((const char *)name, (const char *)alphaKeybParam);
}

} // namespace

extern "C" {
void *PTR__CAlphaKeybCtrlConstructor_08eabb48[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CEditorConstructor_08f29c10[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)CEditorConstructorCreate };
void *PTR__CPanelConstructor_08f7c2f0[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CBatchDiskManConstructor_08eabe08[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESCommonModuleConstructor_08fbb048[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESProgModuleConstructor_08fbd218[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESEffectModuleConstructor_08fbb2c8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESCombiModuleConstructor_08fbe028[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESGlobalModuleConstructor_08fbea28[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESMOSSModuleConstructor_08fbbe48[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESSamplingModuleConstructor_08fc6a48[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESSetListModuleConstructor_08fd37a8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESSongModuleConstructor_08fc2818[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
void *PTR__CESDiskModuleConstructor_08fcc0a8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)ModuleFactoryCreateStub };
}

/* The 2 real driver classes MMainPanelDriver/MMainHIDDriver construct directly.
 * Both fully reconstructed (Stage 6 breadth sweep, 2026-07-25) -- see
 * include/hid_driver.h/include/panel_driver.h for the real ctor/dtor/method bodies
 * and byte-exact real vtables (PTR__CHIDDriver_08fd9ce8[13]/
 * PTR__CLinuxPanelDriver_08fd9dc8[8], now properly-sized arrays defined in
 * hid_driver.cpp/panel_driver.cpp -- previously bare `void* = 0` scalars here, the
 * same undersized-vtable bug class found repeatedly elsewhere in this project).
 */

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
	 * same collapse convention as omega_interface.cpp's `new CKernel(0)`. CLinuxPanelDriver
	 * is now fully reconstructed (panel_driver.h) and its real sizeof is genuinely 8 bytes,
	 * so a bare `new CLinuxPanelDriver(...)` would also be size-correct now -- kept as
	 * malloc+placement-new anyway for consistency with this file's own established
	 * convention (and in case any future field is discovered that isn't yet modeled).
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

	/* Real malloc(0x28) + placement construct -- CHIDDriver's real sizeof is genuinely
	 * 0x28 now too (hid_driver.h) -- see MMainPanelDriver's comment above. */
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

/* Real per-module vtables -- the derived module class behind each is still out of
 * scope (see file header above), but as of the Stage 6 AddModule()/EnableUpdate()
 * batch these are genuinely dispatched through (CModuleManager::Setup/Config/Start()
 * walk a now-really-populated mModules), so they're real EvaVTableStub-backed slot
 * arrays declared in omega_vtables.h/.cpp -- NOT the bare always-NULL scalar globals
 * this file used to declare locally (that shape was only safe while AddModule() was
 * still a Tier-B no-op; see omega_vtables.h for the full explanation).
 */

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

/* Opaque real per-class vtables these 7 static constructors install.
 *
 * WORKAROUND (2026-07-24): this block's own comment used to claim "nothing in
 * this reconstruction ever dispatches through an XxxApiInstance object's own
 * vtable" -- disproven by a live kronos_vm boot: CKernel::CKernel()'s own
 * Phase-1 walk over sm_poGlobalObjectList (ckernel.cpp, traced the same day
 * as this file's own EditApiInstance work, apparently after this comment was
 * written) dispatches vtable slots +8/+0xc/+0x10/+0x14 (PreKernelConstructor/
 * PostKernelConstructor/PreKernelDestructor/PostKernelDestructor) on EVERY
 * CGlobalObjectBase-derived object it's ever seen, including all 7 of these
 * (each is placement-`new`'d as one, unconditionally, in its own ctor below)
 * -- BUG: kernel NULL pointer dereference... well, userspace segfault "ip
 * (null)", a call through a null function pointer. These 6 were bare `void*
 * = 0` instead of a properly-sized array (unlike this file's own correctly-
 * shaped PTR__CHIDDriver_08fd9ce8/PTR__CLinuxPanelDriver_08fd9dc8, or
 * sysapi_instance.cpp's PTR__CSysApiInstance_08e81008[94]) -- reading slot
 * index 2 (byte offset +8) out of a single 4/8-byte variable is undefined
 * behavior, landing on whatever happens to be adjacent in `.data`. Fixed to
 * properly-sized `EvaVTableStub`-filled arrays (6 slots -- the confirmed
 * CGlobalObjectBase minimum actually dispatched through; none of these
 * classes' own real vtable sizes are independently ground-truthed, matching
 * PTR__CSysApiInstance_08e81008's own "safe regardless of real slot count"
 * precedent, omega_vtables.cpp). RegisterApi()/AssignScope() etc. are still
 * correct to call directly by name -- only the CKernel-side phase-hook
 * dispatch was the gap.
 *
 * WORKAROUND #2 (2026-07-25): PTR__CEditApiInstance_08e85da8 specifically (the
 * other 5 XxxApiInstance vtables below are untouched -- nothing reconstructed
 * dispatches through them past slot 5 yet) bumped 6 -> 20 slots. Stage 6
 * batch 6 (stg_unsol_msg_handler.cpp) added real dispatch through EditApi's
 * own vtable at byte offsets +0x28/+0x2c (GetScopeId/QueryFlag, `EndHandling()`
 * -- dead-branch only, never actually invoked given this pass's own data) and
 * this batch (2026-07-25) added five real, *unconditionally invoked* callers of
 * +0x28/+0x30 (PatchMsgHandler/EffectMgrMsgHandler/EffectMsgHandler/
 * HDRTrackMsgHandler/SetListMsgHandler) -- +0x30/4 = slot 12, +0x3c/4 = slot 15,
 * both past the old 6-slot bound. That combination is no longer a "dead branch
 * that happens to never execute" case: these five handlers hit it on every
 * call. Same fix shape as WORKAROUND #1 above, just a bigger array (20 slots
 * gives headroom past the highest confirmed offset, +0x3c).
 *
 * WORKAROUND #3 (Stage 6 breadth sweep, batch 2026-07-25b, config_manager.cpp):
 * ChkApi/SeqApi/RTRouterApi (3 of the remaining 5 untouched-past-slot-5 arrays)
 * bumped for the same reason -- CConfigManager::RegisterChunkServer()/
 * ConfigureSeqTimer()/LinkRTRouterTracks() dispatch through ChkApi+0x38 (slot
 * 14), SeqApi+0x48 (slot 18), and RTRouterApi+0x2c (slot 11, UNCONDITIONALLY --
 * the one real "not even data-gated" case of the three, see LinkRTRouterTracks()'s
 * own comment) respectively -- all past the old 6-slot bound. ChkApi -> 16,
 * SeqApi -> 20 (matches EditApiInstance's own headroom convention), RTRouterApi
 * -> 16. DumpApi/RMApi are untouched -- nothing reconstructed dispatches through
 * either past slot 5 yet.
 */
extern "C" void *PTR__CEditApiInstance_08e85da8[20] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *PTR__CSeqApiInstance_08e88fa8[20] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *PTR__CChkApiInstance_08e855c8[16] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *PTR__CDumpApiInstance_08e85ba8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *PTR__CRTRouterApiInstance_08e822e8[16] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *PTR__CSysExApiInstance_08e89a28[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* RMApiInstance's own ctor transiently installs these 2 before overwriting with its
 * real, final CRMApiInstance vtable (see the ctor below) -- included for the same
 * shape-fidelity reason, never left installed nor dispatched through, so left as
 * a bare placeholder (never survives to be walked).
 *
 * PTR__CRMApiCallBack_08e886e8 itself is now declared/defined in omega_vtables.h/.cpp
 * instead (Stage 6, CFileMan/CResMan ctor batch, 2026-07-25) -- CResMan::CResMan()
 * needed this exact vtable's own real slot count (7, was a bare scalar here, the same
 * undersized-vtable bug class fixed repeatedly elsewhere in this project), and it's
 * shared between that ctor and this one. `&PTR__CRMApiCallBack_08e886e8` below still
 * yields the correct (array-base) address unchanged.
 */
extern "C" void *PTR__CRMApi_08e88de8 = 0;
/* Final, real, dispatched-through vtable -- same fix as the 6 above. */
extern "C" void *PTR__CRMApiInstance_08e88c48[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
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
	*(void **)EditApiInstance = PTR__CEditApiInstance_08e85da8;
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
	*(void **)SeqApiInstance = PTR__CSeqApiInstance_08e88fa8;
	*(int *)(SeqApiInstance + 4) = 0;

	SeqApi = SeqApiInstance;
	DAT_0931b20c = "SeqApi";
}

/* global.constructors.keyed.to.ChkApiInstance@080bfd60.c, 79 bytes. */
__attribute__((constructor))
static void ConstructChkApiInstance()
{
	new (ChkApiInstance) CGlobalObjectBase();
	*(void **)ChkApiInstance = PTR__CChkApiInstance_08e855c8;

	ChkApi = ChkApiInstance;
	DAT_0930a6ac = "ChkApi";
}

/* global.constructors.keyed.to.DumpApiInstance@080cef10.c, 89 bytes. */
__attribute__((constructor))
static void ConstructDumpApiInstance()
{
	new (DumpApiInstance) CGlobalObjectBase();
	*(void **)DumpApiInstance = PTR__CDumpApiInstance_08e85ba8;
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
	*(void **)RTRouterApiInstance = PTR__CRTRouterApiInstance_08e822e8;
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

	*(void **)RMApiInstance = PTR__CRMApiInstance_08e88c48;
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
	*(void **)g_oSysExApiInstance = PTR__CSysExApiInstance_08e89a28;

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

/* CFileMan::CFileMan() (.text+0x081011e0, 1079 bytes) / CResMan::CResMan()
 * (.text+0x081523a0, 1333 bytes) are now REAL (Stage 6 breadth sweep, 2026-07-25 --
 * the "What's still open" CFileMan/CResMan ctor batch, file_man.h/res_man.h) --
 * upgraded from the Tier-B placeholder stub classes this file used to declare
 * locally. Both real ctors are called with NO name argument (functions.csv:
 * `CFileMan::CFileMan()`/`CResMan::CResMan()`, no params) because each does exactly
 * what every other MMainXxx module does -- chains into CModule's own base ctor with
 * a hardcoded class-static name literal (`CFileMan::SysName`/`CResMan::SysName`,
 * confirmed real 4-byte globals in symbols.csv, content not decoded -- same
 * "opaque, non-null is enough" treatment as `CEditMan_SysName` etc. above) -- then
 * installs its own real vtable itself (unlike every other MMainXxx(void) module
 * here, which leaves that to its caller). The rest of each class (~50-60 further
 * methods, some multi-KB -- CResMan::Save() alone is 16175 bytes) remains a
 * genuine god-object, out of scope for this pass, matching PLAN.md's
 * "UI/CForm/Peg-scale breadth is out of scope" boundary -- see file_man.h/res_man.h
 * for the exact per-field/per-method breakdown.
 */

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
	*(void **)module = (void *)PTR__CEditMan_08e85ea8;
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
	*(void **)module = (void *)PTR__CMessagePort_08e88468;
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
	*(void **)module = (void *)PTR__CSeqTimer_08e892a8;
	*(int *)((char *)module + 0x2c) = 0;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x08105a70, 101 bytes. The one MMainXxx(void) member of this family that
 * calls a real, distinct derived-class ctor (CFileMan::CFileMan, now Tier A --
 * file_man.h/.cpp) instead of the shared CModule::CModule() base + vtable-swap
 * idiom. Also the one member NOT converted to a direct RegisterApi() call despite
 * dispatching through the same +0xa4 slot: unlike the other 6 raw-dispatch
 * siblings, DAT_0930b174 has no matching `global.constructors.keyed.to.*` producer
 * setting it to a string (FMApiInstance itself isn't a CGlobalObjectBase-style
 * global either -- it's constructed inline by this very function, via
 * CFileMan::CFileMan()) -- Ghidra types it `undefined4` at every site, including
 * MMainLinuxDriver's own reuse of the same constant as a `GetSubApiFn` integer id
 * argument (mains.cpp, above). Left as a raw vtable dispatch with its `int` type
 * intact, not asserted to be a name string like its 6 siblings.
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
	*(void **)module = (void *)PTR__CSysEx_08e899e8;
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
	*(void **)module = (void *)PTR__CChunkMan_08e85968;
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
	*(void **)module = (void *)PTR__CDumpManMod_08e85ca8;
	((CSysApiInstance *)SysApiInstance)->AddModule(module);
}

/* .text+0x08160db0, 111 bytes. The other MMainXxx(void) member calling a real,
 * distinct derived-class ctor (CResMan::CResMan, now Tier A -- res_man.h/.cpp). */
void MMainResMan()
{
	((CSysApiInstance *)Api)->RegisterApi(DAT_0931b1f0, (CApiBase *)RMApiInstance);

	void *raw = malloc(0x21a0);
	CResMan *resMan = new (raw) CResMan();
	((CSysApiInstance *)SysApiInstance)->AddModule((CModule *)resMan);
	CRMApiInstance::SetResMan(RMApiInstance, resMan);
}
