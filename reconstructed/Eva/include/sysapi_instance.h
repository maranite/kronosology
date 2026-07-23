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
 * RegisterApi(const char*, CApiBase*) is a Tier-B link-stub body (genuinely deep named-
 * API registry, out of scope) but its SIGNATURE and CALL SITES are now Tier A: direct
 * raw-byte read of the real binary (.rodata+08e81008+0xa4) confirms this is exactly
 * what sits at Api's own vtable slot +0xa4 -- the slot mains.cpp's 8-member
 * MMainXxx(void) family (ckernel.cpp's InitSystemLayer()) dispatches through to
 * register EditApiInstance/SeqApiInstance/ChkApiInstance/DumpApiInstance/RMApiInstance/
 * RTRouterApiInstance. Those 8 call sites now call RegisterApi() directly by name
 * (mains.cpp) instead of the raw `(**(code**)(*Api+0xa4))(...)` vtable dispatch they
 * used before this was confirmed -- matching MMainSysEx's own pre-existing, already-
 * direct RegisterApi() call (the "one real outlier" Stage 3's own README section
 * flagged; turns out not to be a different mechanism at all, just a different calling
 * style for the exact same function).
 */

#ifndef SYSAPI_INSTANCE_H
#define SYSAPI_INSTANCE_H

class CModule;
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

	/* .text+0x0806bab0, 1099 bytes -- Tier-B link-stub, not reconstructed (named-API
	 * registry substrate, genuinely out of scope for this pass).
	 */
	void RegisterApi(const char *name, CApiBase *api);
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
