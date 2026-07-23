/*
 * ckernel.h  -  CKernel, Eva's "app kernel" object (Stage 3).
 *
 * COmegaInterface::Init() (src/init/omega_interface.cpp) heap-allocates and
 * placement-constructs one of these at Omega+8 before doing anything else. Real
 * layout confirmed only as far as CKernel::CKernel(int)/~CKernel() themselves touch
 * `this` directly (0x10 bytes: vtable + 3 zeroed ints) -- CKernel is-a some
 * unreconstructed `TVector<?>` template base (its own vtable install,
 * PTR__TVector_08e80c58, is shared with a handful of other TVector-shaped globals
 * elsewhere in the binary). The bulk of what CKernel::CKernel/~CKernel actually do is
 * construct/tear down a fixed set of Kernel-owned singletons (scheduler, module
 * manager, config manager, tracer, error handler, host interface, dummy msg input) --
 * see ckernel.cpp's own header comment for the full inventory of what's faithfully
 * transcribed vs. declared as an out-of-scope extern.
 */

#ifndef CKERNEL_H
#define CKERNEL_H

class CGlobalObjectBase;

class CKernel {
public:
	/* param 0 selects PTR__CHostInterfaceBase_08e80b68 (the branch actually taken
	 * on Omega's own boot path -- COmegaInterface::Init() calls `new CKernel(0)`);
	 * nonzero selects PTR__CHostInterface_08e80b08 instead. Neither class is
	 * reconstructed -- both host-interface objects are only ever vtable-dispatched
	 * (never named-member-called) anywhere this constructor/destructor touches them.
	 */
	explicit CKernel(int hostInterfaceKind);
	~CKernel();

	/* Real body: `return SysApiInstance;` -- a global this class only ever reads,
	 * never writes; whatever sets it is not yet traced. Returns void* (matching
	 * omega_interface.h's existing _func_int_char_ptr-adjacent convention of not
	 * yet giving CSystemApi/CSysApiInstance a reconstructed relationship) --
	 * callers (Mains(), the MMainXxx family) cast it to CSystemApi* themselves.
	 */
	static void *GetSysApi();

	/* Real body: 7x CScheduler::InsertLevel(0..6), CConfigManager::AssignEditServerIDs(),
	 * MMainEditMan(), CModuleManager::Setup()+Config(), 7 more MMainXxx system-layer
	 * inits (Viewer/SeqTimer/FileMan/SysEx/ChunkMan/RTRouter/DumpMan/ResMan -- a
	 * *different* MMainXxx family than Mains()'s CSystemApi-registration one, see
	 * ckernel.cpp), CModuleManager::Setup()+Config() again, CScheduler::Enable(1),
	 * CModuleManager::AdjustTaskMask()+Start(). All of those callees are Stage 4+,
	 * declared as call-contract externs in ckernel.cpp.
	 */
	static void InitSystemLayer();

	/* .text+0x0805e630, 169 bytes -- not yet reconstructed (Stage 4+). Called once
	 * per scheduling-signal wakeup from OmegaSchedulingThread (omega_threads.cpp).
	 */
	void Exec();

	/* .text+0x0805dcf0, 273 bytes -- not yet reconstructed (Stage 4+). Called once
	 * from OmegaInitThread (omega_threads.cpp).
	 */
	static void InitUserLayer();

	/* .text+0x0805da90, 123 bytes / .text+0x0805db40, 70 bytes -- CGlobalObjectBase's
	 * own ctor/dtor call these (global_object_base.h); this is the real mechanism
	 * behind sm_poGlobalObjectList and, transitively, Api/SysApiInstance (see
	 * sysapi_instance.h/.cpp) -- traced 2026-07-23. AddGlobalObject lazily creates
	 * sm_poGlobalObjectList (a COmegaPtrArray, growBy=1/initialCapacity=0/ownFlag=0,
	 * vtable-swapped to PTR__TPtrArray_08e80bc8) the first time it's ever called, then
	 * COmegaPtrArray::Add()s. Both real, Tier A.
	 */
	static void AddGlobalObject(CGlobalObjectBase *obj);
	static void RemoveGlobalObject(CGlobalObjectBase *obj);

private:
	/* Resolved by CKernel::Exec() (ckernel.cpp, Stage 4): this+0x04/+0x08 are the
	 * begin/end pointers of CKernel's own embedded TVector<CTimerObject*,1> base
	 * (matching PTR__TVector_08e80c58's real name, symbols.csv) -- Exec() walks
	 * `[this+4, this+8)` as a flat CTimerObject* array. Both zeroed by the ctor and
	 * never populated by any reconstructed code, so this vector is always empty
	 * (begin==end==0) on every path this pass exercises -- Exec()'s own walk over it
	 * is real and transcribed faithfully, but is dead code given this pass's own
	 * construction. Retained the original mUnknown04/mUnknown0c field names (rather
	 * than renaming to mTimerBegin/mTimerEnd) since the dtor's own `free(mUnknown04)`
	 * call still treats it as a single heap block, not a base-class object -- the
	 * TVector-vs-heap-block relationship isn't fully resolved either way.
	 */
	void *mVtbl;      /* this+0x00 -- TVector<?> base vtable slot, PTR__TVector_08e80c58 */
	void *mUnknown04; /* this+0x04 -- CTimerObject** begin (see above); dtor frees it */
	int   mUnknown08; /* this+0x08 -- CTimerObject** end (see above) */
	int   mUnknown0c; /* this+0x0c -- zeroed by ctor, never read back by ctor/dtor */
};

#endif /* CKERNEL_H */
