/*
 * ckernel.cpp  -  see include/ckernel.h.
 *
 * Transcribed from the Ghidra decompile export:
 *   CKernel::CKernel(int)   .text+0x0805d4c0, 656 bytes  (CKernel@0805d4c0.c)
 *   CKernel::~CKernel()     .text+0x0805d820, 505 bytes  (_CKernel@0805d820.c)
 *   CKernel::InitSystemLayer() .text+0x0805dba0, 326 bytes (InitSystemLayer@0805dba0.c)
 *   CKernel::GetSysApi()    .text+0x0805db90, 6 bytes    (GetSysApi@0805db90.c)
 *
 * NOT the same GetSysApi as COmegaInterface::GetSysApi() (.text+0x0804e0a0, already
 * reconstructed in omega_interface.cpp) -- that one just forwards to this one.
 *
 * Scope / what's real vs. stubbed:
 *  - CKernel's own 0x10-byte instance layout, and every singleton-construction /
 *    -teardown step the ctor/dtor perform, are transcribed faithfully, including the
 *    exact malloc sizes and raw field offsets Ghidra recovered.
 *  - The real disassembly brackets essentially every malloc()/free() in this function
 *    with HAL_DisableInterrupts()/HAL_EnableInterrupts() (same pair already noted as
 *    dropped in omega_interface.cpp's Init() -- a kernel-side critical-section shim,
 *    not a real userspace primitive). Dropped here too, for the same reason, rather
 *    than repeating the same comment at each of the ~12 call sites.
 *  - Every class CKernel constructs/tears down but never calls a *named* method on
 *    (CHostInterfaceBase/CHostInterface, CTracer, CDummyMsgInput) is left as a raw
 *    vtable-dispatched blob -- there is nothing to reconstruct beyond the vtable
 *    install/call, since Ghidra never resolved those jump tables either. Classes
 *    CKernel *does* call named methods on (CScheduler, COmegaPtrArray, CErrorHandler,
 *    CModuleManager, CConfigManager, CSysApiInstance) are declared as call-contract
 *    extern classes/methods -- real mangled symbols, not implemented (Stage 4+).
 *  - sm_poGlobalObjectList: a global registry of "auto-registering" objects -- CONFIRMED
 *    2026-07-23 to genuinely be a COmegaPtrArray (not just COmegaPtrArray-*shaped*; the
 *    real class itself, per CKernel::AddGlobalObject()'s own decompile, see below).
 *    Both ctor and dtor walk it calling different vtable slots (+8 / +0xc / +0x10 /
 *    +0x14) on each entry -- confirmed by direct raw-byte read of the real binary to be
 *    CGlobalObjectBase's own 4 "phase hook" no-ops (PreKernelConstructor/
 *    PostKernelConstructor/PreKernelDestructor/PostKernelDestructor, all `return 0`,
 *    none overridden by any XxxApiInstance-derived class this project touches -- see
 *    global_object_base.h). The producer IS now traced: every XxxApiInstance-style
 *    global's own static constructor (CGlobalObjectBase's base-ctor step) calls
 *    CKernel::AddGlobalObject(this) automatically, before main() -- see
 *    global_object_base.h and sysapi_instance.cpp. This means sm_poGlobalObjectList is
 *    no longer permanently empty in this reconstruction: by the time CKernel::CKernel()
 *    itself runs (well after static init), it already holds every constructed
 *    XxxApiInstance object, and both walks below are now real, live code paths, not
 *    dead ones -- still safe, since none of those objects override the 4 phase hooks.
 *  - InitSystemLayer() calls a *second*, unrelated "MMainXxx" family
 *    (MMainEditMan/MMainViewer/MMainSeqTimer/MMainFileMan/MMainSysEx/MMainChunkMan/
 *    MMainRTRouter/MMainDumpMan/MMainResMan) -- all void(void), no CSystemApi
 *    argument. Do not confuse with Mains()'s 17-member MMainXxx(CSystemApi*) family
 *    (src/init/mains.cpp) -- same naming convention, different call shape, genuinely
 *    different functions.
 */

#include "ckernel.h"
#include "config_manager.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "scheduler.h"
#include "module_manager.h"
#include "error_handler.h"
#include "sysapi_instance.h"

#include <cstdlib>
#include <new>

/* CScheduler, COmegaPtrArray, CModuleManager, CErrorHandler, CSysApiInstance are now
 * real, shared, Stage-4-reconstructed classes (see their own headers) -- no longer
 * declared locally here as bare call-contract stubs.
 */

/* Real, plain-C++-linkage functions (mains.cpp) -- NOT extern "C" (these are genuine
 * C++ functions, no reason for C linkage; an earlier pass of this file wrapped them
 * in extern "C" by mistake, which silently mismatched mains.cpp's own plain
 * declarations and produced unresolved-symbol link errors despite both sides
 * "matching" at the source level -- fixed here).
 */
void MMainEditMan();
void MMainViewer();
void MMainSeqTimer();
void MMainFileMan();
void MMainSysEx();
void MMainChunkMan();
void MMainRTRouter();
void MMainDumpMan();
void MMainResMan();

/* CTracer -- still vtable-dispatched-only everywhere else in this file (ctor/dtor
 * only build/tear down a raw blob, never call a named method on it), except
 * InitUserLayer()'s own real EnableUpdate(1) call, which is the one place a named
 * method is needed. Kept file-local (not a shared header) since nowhere else in this
 * reconstruction touches CTracer by name.
 */
class CTracer {
public:
	/* .text+0x0806ce80, not measured -- Tier-B link-stub. */
	void EnableUpdate(int /*enable*/) {}
};

/* Global registry CKernel's ctor/dtor both walk -- see header comment above. Real
 * definition (not just extern): never written by any reconstructed code besides this
 * file's own zero-init, so it stays permanently empty (both walks below are real,
 * faithfully transcribed, but no-ops given that) -- the real producer that populates
 * it isn't traced.
 */
COmegaPtrArray *sm_poGlobalObjectList = 0;

/* CKernel-owned singletons the constructor creates and the destructor tears down.
 * Real global symbol names taken directly from the disassembly's own references
 * (module-scope globals, not CKernel instance members).
 */
CKernel      *g_poKernel = 0;
static void       *g_poHostInterface = 0; /* CHostInterfaceBase* or CHostInterface* -- vtable-dispatched only, class not reconstructed */
static CScheduler *g_poScheduler = 0;
/* g_poModuleManager itself is now defined in module_manager.cpp (CModuleManager's own
 * methods need to reach it too, via CSysApiInstance::AddModule's real forwarding) --
 * declared there as `void *g_poModuleManager`, pulled in via module_manager.h above.
 * Still the same raw CModuleManager-shaped blob this ctor builds by hand (2 embedded
 * COmegaPtrArray + 4 trailing fields), passed to the real CModuleManager:: methods by
 * cast.
 */
static void       *g_poConfigManager = 0; /* CConfigManager instance, malloc(1) -- ctor never touches any field of it */
static void       *g_poTracer = 0;        /* CTracer* -- vtable-dispatched only, class not reconstructed */
static void       *g_poErrorHandler = 0;  /* CErrorHandler*-shaped raw blob; real ~CErrorHandler() invoked at teardown */
static void       *g_poDummyMsgInput = 0; /* CDummyMsgInput* -- vtable-dispatched only, class not reconstructed */

/* Small helpers replicating the `(**(code**)(*obj + slot))(obj, ...)` indirect-call
 * idiom Ghidra emits for classes whose vtable layout isn't reconstructed -- same
 * pattern already used as-is (uncommented-helper form) in omega_interface.cpp's
 * ExitRequested(). Byte offsets, not word indices, matching the original pointer
 * arithmetic exactly.
 */
static inline void CallVSlot1(void *obj, int byteOffset)
{
	typedef void (*Fn)(void *);
	void *vtbl = *(void **)obj;
	Fn fn = *(Fn *)((char *)vtbl + byteOffset);
	fn(obj);
}

static inline void CallVSlot2(void *obj, int byteOffset, int arg)
{
	typedef void (*Fn)(void *, int);
	void *vtbl = *(void **)obj;
	Fn fn = *(Fn *)((char *)vtbl + byteOffset);
	fn(obj, arg);
}

/* CKernel::AddGlobalObject(CGlobalObjectBase*) .text+0x0805da90, 123 bytes.
 * CKernel::RemoveGlobalObject(CGlobalObjectBase*) .text+0x0805db40, 70 bytes.
 * See ckernel.h's own comment and global_object_base.h for the full mechanism this
 * unblocks (Api/SysApiInstance). `obj` is only ever passed through to
 * COmegaPtrArray::Add()/FindIndex()/RemoveAtIndex() as an opaque void* here -- no
 * CGlobalObjectBase member is dereferenced, so the forward declaration in ckernel.h is
 * sufficient and this file doesn't need global_object_base.h itself.
 */
void CKernel::AddGlobalObject(CGlobalObjectBase *obj)
{
	if (sm_poGlobalObjectList == 0) {
		void *raw = malloc(0x18);
		sm_poGlobalObjectList = new (raw) COmegaPtrArray(1, 0, 0);
		*(void **)sm_poGlobalObjectList = &PTR__TPtrArray_08e80bc8[0];
	}
	sm_poGlobalObjectList->Add(obj);
}

void CKernel::RemoveGlobalObject(CGlobalObjectBase *obj)
{
	if (sm_poGlobalObjectList != 0) {
		/* Real code re-reads this+4 (mUnknown04) as the callDtorCallback flag to
		 * pass through to RemoveAtIndex() -- same field CKernel::~CKernel()'s own
		 * final teardown loop reads the same way (see below).
		 */
		int callDtorCallback = *(int *)((char *)sm_poGlobalObjectList + 4);
		unsigned idx = sm_poGlobalObjectList->FindIndex(obj);
		sm_poGlobalObjectList->RemoveAtIndex(idx, callDtorCallback);
	}
}

CKernel::CKernel(int hostInterfaceKind)
{
	mVtbl = PTR__TVector_08e80c58;
	mUnknown04 = 0;
	mUnknown08 = 0;
	mUnknown0c = 0;

	g_poKernel = this;

	/* Phase-1 walk over sm_poGlobalObjectList, vtable slot +8, arg 0. Re-reads the
	 * list/count each iteration and bails if the list goes null mid-loop -- real
	 * defensive coding against reentrancy from the very call being made.
	 */
	if (sm_poGlobalObjectList != 0) {
		int count = *(int *)((char *)sm_poGlobalObjectList + 0xc);
		for (int i = 0; i < count; i++) {
			void **entries = *(void ***)((char *)sm_poGlobalObjectList + 0x14);
			void *entry = entries[i];
			CallVSlot2(entry, 8, 0);
			if (sm_poGlobalObjectList == 0)
				break;
			count = *(int *)((char *)sm_poGlobalObjectList + 0xc);
		}
	}

	if (hostInterfaceKind == 0) {
		void **p = (void **)malloc(4);
		*p = PTR__CHostInterfaceBase_08e80b68;
		g_poHostInterface = p;
	} else {
		void **p = (void **)malloc(4);
		*p = PTR__CHostInterface_08e80b08;
		g_poHostInterface = p;
	}

	/* CScheduler::CScheduler() placement-constructed into a fresh 0x2c-byte block. */
	void *schedRaw = malloc(0x2c);
	g_poScheduler = new (schedRaw) CScheduler();

	/* ModuleManager: hand-built 0x44-byte (17 dword) block, not a CModuleManager
	 * placement-construct -- two embedded COmegaPtrArray sub-objects at dword
	 * index 1 (byte 4) and index 7 (byte 0x1c), default-constructed then their
	 * vtable slot immediately overwritten with a TNamedPtrArray-flavored vtable
	 * (the same base-ctor-then-derived-vtable-install pattern seen throughout
	 * this file and the MMainXxx family) -- so these are really
	 * TNamedPtrArray-derived-from-COmegaPtrArray sub-objects, not plain
	 * COmegaPtrArray. Trailing 4 dwords (index 0xd..0x10) zeroed, meaning
	 * unconfirmed.
	 */
	{
		unsigned *mm = (unsigned *)malloc(0x44);
		mm[0] = 0;
		new (mm + 1) COmegaPtrArray();
		mm[1] = (unsigned)PTR__TNamedPtrArray_08e80c10;
		new (mm + 7) COmegaPtrArray();
		mm[7] = (unsigned)PTR__TNamedPtrArray_08e80bf8;
		mm[0xd] = 0;
		mm[0xe] = 0;
		mm[0xf] = 0;
		mm[0x10] = 0;
		g_poModuleManager = mm;
	}

	/* CConfigManager -- malloc(1), no fields written; smallest possible heap
	 * object for an (apparently) empty class.
	 */
	g_poConfigManager = malloc(1);

	/* CTracer -- 0x20-byte (8 dword) raw blob, vtable-dispatched only. */
	{
		unsigned *tr = (unsigned *)malloc(0x20);
		tr[0] = (unsigned)PTR__CTracer_08e81468;
		tr[3] = 0;
		*(unsigned short *)(tr + 4) = 100;
		*(unsigned short *)((char *)tr + 0x12) = 0;
		*(unsigned short *)(tr + 5) = 0;
		tr[6] = 0;
		tr[7] = 0;
		tr[2] = 0;
		tr[1] = 0;
		g_poTracer = tr;
	}

	/* CErrorHandler -- 0x10-byte (4 dword) raw blob built by hand (not via
	 * CErrorHandler's own constructor); real ~CErrorHandler() is still invoked
	 * on it at teardown (see dtor below) -- an inconsistency faithfully
	 * preserved, not "fixed" into a placement-new.
	 */
	{
		unsigned *eh = (unsigned *)malloc(0x10);
		eh[0] = 0;
		*(unsigned short *)(eh + 1) = 0x1e;
		*(unsigned short *)((char *)eh + 6) = 0;
		*(unsigned short *)(eh + 2) = 0;
		eh[3] = 0;
		g_poErrorHandler = eh;
	}

	/* CDummyMsgInput -- single-pointer (vtable-only) blob, then a second walk
	 * over sm_poGlobalObjectList (captured *before* this malloc, same list as
	 * phase 1 above), this time vtable slot +0xc. Real code re-derives the
	 * per-iteration entry with an extra (redundant, given the loop bound) NULL
	 * fallback when the index runs past the live count -- preserved literally.
	 */
	{
		COmegaPtrArray *savedList = sm_poGlobalObjectList;
		void **p = (void **)malloc(4);
		*p = PTR__CDummyMsgInput_08e80c80;
		g_poDummyMsgInput = p;

		if (savedList != 0) {
			int count = *(int *)((char *)savedList + 0xc);
			for (int i = 0; i < count; i++) {
				void *entry;
				if (i < count)
					entry = (*(void ***)((char *)savedList + 0x14))[i];
				else
					entry = 0;
				CallVSlot2(entry, 0xc, 0);
				if (sm_poGlobalObjectList == 0)
					return;
				count = *(int *)((char *)sm_poGlobalObjectList + 0xc);
			}
		}
	}
}

CKernel::~CKernel()
{
	/* Phase-3 walk over sm_poGlobalObjectList, vtable slot +0x10. */
	if (sm_poGlobalObjectList != 0) {
		int count = *(int *)((char *)sm_poGlobalObjectList + 0xc);
		for (int i = 0; i < count; i++) {
			void *entry = (*(void ***)((char *)sm_poGlobalObjectList + 0x14))[i];
			CallVSlot2(entry, 0x10, 0);
			if (sm_poGlobalObjectList == 0)
				break;
			count = *(int *)((char *)sm_poGlobalObjectList + 0xc);
		}
	}

	if (g_poDummyMsgInput != 0)
		CallVSlot1(g_poDummyMsgInput, 4);

	if (g_poErrorHandler != 0) {
		((CErrorHandler *)g_poErrorHandler)->~CErrorHandler();
		free(g_poErrorHandler);
	}

	if (g_poTracer != 0)
		CallVSlot1(g_poTracer, 4);

	free(g_poConfigManager);

	if (g_poModuleManager != 0) {
		unsigned *mm = (unsigned *)g_poModuleManager;
		mm[7] = (unsigned)PTR__TNamedPtrArray_08e80bf8;
		((COmegaPtrArray *)(mm + 7))->Destroy();
		mm[7] = (unsigned)PTR__COmegaPtrArray_08e80be0;
		mm[1] = (unsigned)PTR__TNamedPtrArray_08e80c10;
		((COmegaPtrArray *)(mm + 1))->Destroy();
		mm[1] = (unsigned)PTR__COmegaPtrArray_08e80be0;
		free(mm);
	}

	if (g_poScheduler != 0) {
		unsigned *sc = (unsigned *)g_poScheduler;
		sc[1] = (unsigned)PTR__TPtrArray_08e80c40;
		((COmegaPtrArray *)(sc + 1))->Destroy();
		sc[1] = (unsigned)PTR__COmegaPtrArray_08e80be0;
		free(g_poScheduler);
	}

	if (g_poHostInterface != 0)
		CallVSlot1(g_poHostInterface, 4);

	((CSysApiInstance *)SysApiInstance)->Cleanup();

	/* Real teardown loop for sm_poGlobalObjectList itself: repeatedly pops the
	 * front entry (vtable slot +0x14, arg 0), then removes it from the list via
	 * COmegaPtrArray::FindIndex()/RemoveAtIndex() -- until the list is empty, at
	 * which point it frees the list object itself (vtable slot +4) and clears
	 * the global, then finally resets `this`'s own vtable and frees this+0x04.
	 * Preserved close to literal -- the double NULL-list checks mid-loop are
	 * real (the vtable call at +0x14 can apparently free the list out from
	 * under this loop).
	 */
	for (;;) {
		if (sm_poGlobalObjectList == 0)
			break;
		if (*(int *)((char *)sm_poGlobalObjectList + 0xc) == 0) {
			CallVSlot1(sm_poGlobalObjectList, 4);
			sm_poGlobalObjectList = 0;
			break;
		}

		void *frontEntry;
		if (*(int *)((char *)sm_poGlobalObjectList + 0xc) < 1)
			frontEntry = 0;
		else
			frontEntry = (*(void ***)((char *)sm_poGlobalObjectList + 0x14))[0];
		CallVSlot2(frontEntry, 0x14, 0);

		COmegaPtrArray *list = sm_poGlobalObjectList;
		void *entry0 = 0;
		if (*(int *)((char *)sm_poGlobalObjectList + 0xc) > 0)
			entry0 = (*(void ***)((char *)sm_poGlobalObjectList + 0x14))[0];
		int elemSize = *(int *)((char *)sm_poGlobalObjectList + 4);
		unsigned idx = list->FindIndex(entry0);
		list->RemoveAtIndex(idx, elemSize);
	}

	mVtbl = PTR__TVector_08e80c58;
	free(mUnknown04);
}

void *CKernel::GetSysApi()
{
	return SysApiInstance;
}

void CKernel::InitSystemLayer()
{
	g_poScheduler->InsertLevel(0);
	g_poScheduler->InsertLevel(1);
	g_poScheduler->InsertLevel(2);
	g_poScheduler->InsertLevel(3);
	g_poScheduler->InsertLevel(4);
	g_poScheduler->InsertLevel(5);
	g_poScheduler->InsertLevel(6);

	CConfigManager::AssignEditServerIDs();
	MMainEditMan();

	((CModuleManager *)g_poModuleManager)->Setup();
	((CModuleManager *)g_poModuleManager)->Config();

	MMainViewer();
	MMainSeqTimer();
	MMainFileMan();
	MMainSysEx();
	MMainChunkMan();
	MMainRTRouter();
	MMainDumpMan();
	MMainResMan();

	((CModuleManager *)g_poModuleManager)->Setup();
	((CModuleManager *)g_poModuleManager)->Config();

	g_poScheduler->Enable(1);

	((CModuleManager *)g_poModuleManager)->AdjustTaskMask();
	((CModuleManager *)g_poModuleManager)->Start();
}

/* Real "currently mid-Exec" guard around the host-interface poll below -- checked
 * again inside the if() since CScheduler::Exec() (Tier-B, below) could in principle
 * re-enter Exec() on the real system; not exercised by this pass's own data.
 */
static int g_bViewerTaskRunning = 0;
static int g_bHostInterfaceBusy = 0;

/* .text+0x0804d200, 109 bytes -- Tier-B link-stub, real body not reconstructed (not
 * needed: only reachable from Exec()'s own timer walk below, which is itself dead
 * code given this pass's construction -- see Exec()'s comment).
 */
static int HAL_GetSystemTime()
{
	return 0;
}

/* CKernel::Exec() .text+0x0805e630, 169 bytes. Called once per scheduling-signal
 * wakeup from OmegaSchedulingThread (omega_threads.cpp).
 *
 * The timer-array walk below is real and transcribed faithfully (this+4/this+8 are
 * CKernel's own embedded TVector<CTimerObject*,1> begin/end pointers -- see ckernel.h)
 * but is dead code given this pass's own construction: nothing populates that vector,
 * so it is always empty (begin==end==0) and the loop body never runs. CScheduler::Exec()
 * is a Tier-B link-stub (scheduler.h) -- real per-tick task dispatch, out of scope.
 */
void CKernel::Exec()
{
	struct CTimerObjectShape {
		int   startTime;
		int   interval;
		void (*callback)(void *);
		void *arg;
		char  active;
	};

	CTimerObjectShape **begin = (CTimerObjectShape **)mUnknown04;
	CTimerObjectShape **end = (CTimerObjectShape **)mUnknown08;
	if (end - begin != 0) {
		for (CTimerObjectShape **p = begin; p < end; p++) {
			CTimerObjectShape *timer = *p;
			if (timer != 0 && timer->active != 0) {
				int now = HAL_GetSystemTime();
				if ((unsigned)(now - timer->startTime) >= (unsigned)timer->interval)
					timer->callback(timer->arg);
			}
		}
	}

	g_poScheduler->Exec();

	if (g_bViewerTaskRunning == 0 && g_bHostInterfaceBusy == 0) {
		g_bHostInterfaceBusy = 1;
		CallVSlot1(g_poHostInterface, 8);
		g_bHostInterfaceBusy = 0;
	}
}

/* CKernel::InitUserLayer() .text+0x0805dcf0, 273 bytes. Called once from
 * OmegaInitThread (omega_threads.cpp). A flat sequence, no control flow -- transcribed
 * faithfully. Every callee below is a real, correctly-mangled Tier-B link-stub (empty
 * body) -- genuinely out of scope for this pass (9 distinct CConfigManager bring-up
 * steps plus 4 EnableUpdate() variants across CModuleManager/CScheduler/CTracer/
 * CErrorHandler). See config_manager.h/.cpp for the 9 CConfigManager methods.
 */
void CKernel::InitUserLayer()
{
	CConfigManager::ConfigureSeqTimer();
	CConfigManager::CreateResourceFamilies();
	CConfigManager::CreateUserModules();
	CConfigManager::CreateFMDrivers();

	((CModuleManager *)g_poModuleManager)->Setup();
	((CModuleManager *)g_poModuleManager)->Config();

	CConfigManager::SetupRouting();
	CConfigManager::LinkRTRouterTracks();
	CConfigManager::SetupSysex();
	CConfigManager::MakeConnections();
	CConfigManager::RegisterChunkServer();

	((CModuleManager *)g_poModuleManager)->AdjustTaskMask();
	((CModuleManager *)g_poModuleManager)->Start();
	((CModuleManager *)g_poModuleManager)->EnableUpdate(1);

	g_poScheduler->EnableUpdate(1);
	((CTracer *)g_poTracer)->EnableUpdate(1);
	((CErrorHandler *)g_poErrorHandler)->EnableUpdate(1);
}
