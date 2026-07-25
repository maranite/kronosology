/*
 * sysapi_instance.h  -  CSysApiInstance, the real object behind the SysApiInstance
 * global (CKernel::GetSysApi()'s return value) that owns the driver list and the
 * registered-API-descriptor list (Stage 4), and -- confirmed 2026-07-23 -- the real
 * dynamic type of `Api` itself (mains.cpp): `Api = SysApiInstance;` runs in
 * SysApiInstance's own real static constructor, before main() (see .cpp).
 *
 * Real layout confirmed from Cleanup@0806ca50.c and cross-checked against
 * global.constructors.keyed.to.SysApiInstance@0806cc50.c (Eva_export) -- two embedded
 * COmegaPtrArray sub-objects, same embedding pattern as CModuleManager's own two
 * arrays. CORRECTION (2026-07-23): the field-offset-to-name mapping below was
 * originally guessed purely from Cleanup()'s own generic offset arithmetic; the real
 * static constructor installs each embedded array's OWN real vtable
 * (TNamedPtrArray<CDriverBase> vs TNamedPtrArray<CApiDescriptor>, distinguishable by
 * their own mangled vtable symbol names, symbols.csv), which shows the original guess
 * had the two swapped:
 *   +0x04..+0x1c  mApis      embedded COmegaPtrArray (own +0xc/+0x14 land at absolute
 *                             +0x10/+0x18 -- count / array), vtable-swapped to
 *                             PTR__TNamedPtrArray_08e811c0 (TNamedPtrArray<CApiDescriptor>).
 *                             Own +4 (mUnknown04) lands at absolute +8, reused directly
 *                             as the "call the free-element callback" flag Cleanup()
 *                             passes to RemoveAtIndex().
 *   +0x1c..+0x34  mDrivers   embedded COmegaPtrArray, same shape, landing at absolute
 *                             +0x28 (count) / +0x30 (array) / +0x20 (callDtor flag),
 *                             vtable-swapped to PTR__TNamedPtrArray_08e811a8
 *                             (TNamedPtrArray<CDriverBase>).
 * Cleanup()'s own code only ever does generic offset arithmetic on both (no named
 * fields), so this correction is documentation-only -- it does not change Cleanup()'s
 * functional correctness either way.
 *
 * Cleanup() itself is reconstructed faithfully (Tier A -- self-contained given
 * COmegaPtrArray is already reconstructed). EnableMultiTask()/WriteMessageToHost(int,int)
 * are real, correctly-mangled Tier-B link-stubs (empty bodies) -- genuinely deeper
 * substrate (multitasking-disable refcounting, host message queue) out of scope for
 * this pass. AddModule(CModule*) is Tier A -- a real 22-byte thiscall forwarder
 * straight to CModuleManager::AddModule() (module_manager.h), which is itself the
 * Tier-B link-stub end of this particular chain.
 *
 * RegisterApi(const char*, CApiBase*) -- .text+0x0806bab0, 1099 bytes -- PROMOTED TO
 * TIER A (Stage 6 breadth sweep, 2026-07-25). Real body: linear-scans the embedded
 * mApis descriptor array (own +0x10 count / +0x18 array, see field-mapping comment
 * above) for an existing entry with the same name (a real 4-way Duff's-device unroll
 * in the disassembly, collapsed to a plain loop here, same license as
 * omega_ptr_array.cpp's own collapses):
 *   - not found -> build a fresh 3-word {vtbl, name, api} descriptor object (identical
 *     shape to mains.cpp's own RegisterModuleDescriptor() helper, just installing
 *     CApiDescriptor's own vtable -- PTR__CApiDescriptor_08e81368, omega_vtables.h --
 *     at the end instead of a per-module one) and COmegaPtrArray::Add() it.
 *   - found, same api pointer -> logs "API <%s> already registered!" (Api+0x90) and
 *     returns without touching the array.
 *   - found, different api pointer -> logs "Replacing API <%s>!" (Api+0x90), removes
 *     the old descriptor via COmegaPtrArray::RemoveAtIndex() (own +0x08 "call dtor"
 *     flag), then builds+adds a fresh descriptor exactly as the not-found case (the
 *     real disassembly duplicates this tail twice via two different entry points;
 *     collapsed to one shared path here -- semantically identical, not a shortcut).
 * The real disassembly's own extra defensive re-check (`if the matched array slot's
 * raw pointer is NULL, treat as not-found`) is omitted as genuinely unreachable given
 * how entries are only ever added non-null (same "soft assert never actually taken"
 * convention already established elsewhere in this project, e.g. omega_ptr_array.cpp).
 * Return type corrected from the placeholder `void` to `int` (real ABI: `undefined4`,
 * always literal 1 on every path) -- safe, since every one of this pass's 7 real call
 * sites (mains.cpp) already discards the return value as a bare statement.
 *
 * This is exactly the function that sits at Api's own vtable slot +0xa4 (confirmed via
 * a direct raw-byte read of .rodata+08e81008+0xa4) -- the slot mains.cpp's 8-member
 * MMainXxx(void) family (ckernel.cpp's InitSystemLayer()) dispatches through to
 * register EditApiInstance/SeqApiInstance/ChkApiInstance/DumpApiInstance/RMApiInstance/
 * RTRouterApiInstance. All 7 wired-up call sites now call RegisterApi() directly by
 * name (mains.cpp) instead of the raw `(**(code**)(*Api+0xa4))(...)` vtable dispatch
 * they used before this was confirmed -- matching MMainSysEx's own pre-existing,
 * already-direct RegisterApi() call (the "one real outlier" Stage 3's own README
 * section flagged; turns out not to be a different mechanism at all, just a different
 * calling style for the exact same function). On this pass's own traced boot data
 * every one of the 7 calls registers a distinct, never-repeated name, so only the
 * "not found" path is genuinely exercised -- the "replacing" branch is real but
 * currently dead code, same status as several other functions in this project.
 *
 * AddConstructor(CModuleConstructor*) -- .text+0x0806b530, 22 bytes -- ADDED (Stage 6
 * breadth sweep, 2026-07-25). Real forwarder to CModuleManager::AddConstructor()
 * (module_manager.h), exact same 22-byte shape as AddModule()'s own forwarder just
 * above. Direct byte read of the ground-truth binary's own installed
 * PTR__CSysApiInstance_08e81008 vtable (file offset 0xE39048 = VA 08e81008+0x40)
 * confirms the real pointer sitting at slot +0x40 is exactly 0x0806b530 -- i.e. THIS
 * function is the real target of mains.cpp's RegisterModuleDescriptor(), which
 * dispatches all 15 of its module descriptors through `dispatchTarget`'s vtable slot
 * +0x40 (system_api.h). That slot was still wired to the generic EvaVTableStub no-op
 * (omega_vtables.cpp) before this batch, meaning none of those 15 real, boot-path
 * `Mains()`-registered descriptors ever reached CModuleManager's mConstructors array
 * -- same "Tier-B stub leaves a real array permanently empty" bug class as
 * CModuleManager::AddModule()/mModules (Stage 6 batch 3). Fixed by wiring
 * PTR__CSysApiInstance_08e81008[16] (byte offset 0x40) to a real forwarder
 * (omega_vtables.cpp) instead of EvaVTableStub.
 */

#ifndef SYSAPI_INSTANCE_H
#define SYSAPI_INSTANCE_H

class CModule;
class CModuleConstructor;
class CApiBase;

class CSysApiInstance {
public:
	/* .text+0x0806ca50, 497 bytes. */
	void Cleanup();

	/* .text+0x0806b3a0, 22 bytes -- Tier-B link-stub. Real signature takes/returns
	 * the previous enable-state int (a save/restore refcount pattern -- see every
	 * CModuleManager caller's own `iVarN = EnableMultiTask(0); ...; EnableMultiTask(iVarN);`
	 * bracket in module_manager.cpp); returning 0 here is a safe "was already
	 * enabled" default under the stub.
	 */
	int EnableMultiTask(int enable);

	/* .text+0x0806aa00, 64 bytes -- Tier-B link-stub. */
	void WriteMessageToHost(int a, int b);

	/* .text+0x0806b550, 22 bytes. Real forwarder to CModuleManager::AddModule() --
	 * see module_manager.h.
	 */
	void AddModule(CModule *module);

	/* .text+0x0806b530, 22 bytes. Real forwarder to CModuleManager::AddConstructor()
	 * -- see module_manager.h and this header's own file comment above for why this
	 * is genuinely boot-path reachable (the real target of Api's vtable slot +0x40,
	 * mains.cpp's RegisterModuleDescriptor()).
	 */
	void AddConstructor(CModuleConstructor *ctor);

	/* .text+0x0806bab0, 1099 bytes. Tier A -- see file comment above for the full
	 * accounting. Real return type is `undefined4`, always literal 1 -- every one of
	 * this project's own 7 call sites already discards it as a bare statement.
	 */
	int RegisterApi(const char *name, CApiBase *api);
};

/* Real global (CKernel::GetSysApi()'s own body: `return SysApiInstance;`). CORRECTED
 * 2026-07-23: this is the real ~0x34-byte CSysApiInstance OBJECT ITSELF (not a pointer
 * to one elsewhere) -- same "bare global name decays to its own address" pattern Ghidra
 * shows for EditApiInstance/RMApiInstance/RTRouterApiInstance/g_oSysExApiInstance
 * (mains.cpp), vs. the "&global" pattern it shows for SeqApiInstance/ChkApiInstance/
 * DumpApiInstance -- purely a per-global Ghidra type-inference artifact (array- vs.
 * scalar-inferred), not two different real mechanisms; every XxxApiInstance-family
 * global is really "itself the object" either way. Previously left as a null-
 * initialized `void*` (a real bug: SysApiInstance's own real static constructor was
 * not yet modeled, so Api/SysApiInstance were unconditionally null -- this was the root
 * cause of the MMainEditMan() crash found via a live kronos_vm boot test 2026-07-23).
 * Now a real, correctly-sized byte buffer with a real `__attribute__((constructor))`
 * populating it before main() -- see .cpp. CKernel::~CKernel()'s own Cleanup() call and
 * CScheduler's WriteMessageToHost calls are still not reached by this pass's own traced
 * boot path, but SysApiInstance itself is no longer null when they would be.
 */
extern unsigned char SysApiInstance[0x34];

#endif /* SYSAPI_INSTANCE_H */
