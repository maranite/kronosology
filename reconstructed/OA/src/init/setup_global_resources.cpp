// SPDX-License-Identifier: GPL-2.0
/*
 * setup_global_resources.cpp  -  init_module step 8 (hard-fail), the
 * "crux" this project's own plan flagged in advance. See
 * oa_setup_global_resources.h for the full ground-truthing notes,
 * scope statement, and the known STGAPIFrontPanelStatus/CSTGGlobal
 * header-conflict tradeoffs.
 *
 * A faithful reconstruction of the real control flow (order of ~42
 * calls, all three real hard-fail checks) from a full objdump
 * disassembly + relocation trace of `setup_global_resources`
 * (`.text+0x116c40`, 7267 bytes) in OA_real.ko -- the largest single
 * function this project has reconstructed so far.
 */

#include "oa_setup_global_resources.h"
#include "oa_new_delete.h"	/* oa_size_t, for CSTGPCMPrecacheManager::Reset()'s new[]/delete[] use */

/* No <cstring> here -- it conflicts with oa_internal.h's own `strlen`
 * declaration (different exception specifier), same reasoning
 * stgheap_init.cpp already established for inlining its own zero-fill
 * rather than using libc memset. */
static void local_fill(void *p, unsigned char value, unsigned long n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned long i = 0; i < n; i++)
		b[i] = value;
}

/*
 * oa_heap_base()/oa_heap_region()-equivalent, re-derived locally (see
 * this header's own note on the pre-existing CSTGGlobal declaration
 * conflict that rules out including oa_heap.h directly here). Formula
 * confirmed identical to oa_heap.h's own via this exact function's
 * disassembly: -44 (0xFFFFFFD4) is the "heap not up yet" sentinel;
 * otherwise `*(heap+0x38) + *(heap+0x1e8498)` is the base, and
 * `*(heap+0x24+slot*0x14) + *(heap+0x1e8498)` resolves a slot -- with
 * an additional real null-check on `*(heap+0x18+slot*0x14)` this
 * function's own disassembly includes but oa_heap.h's existing
 * oa_heap_region() does not (a small, real, previously-unconfirmed
 * detail this pass adds).
 */
static unsigned char *local_heap_base(void)
{
	unsigned char *heap = (unsigned char *)CSTGHeapManager::sInstance;
	if (heap == (unsigned char *)(long)-44)
		return 0;
	return (unsigned char *)(*(unsigned int *)(heap + 0x38) +
				  *(unsigned int *)(heap + 0x1e8498));
}

static unsigned char *local_heap_region(unsigned int slot)
{
	unsigned char *heap = (unsigned char *)CSTGHeapManager::sInstance;
	if (heap == (unsigned char *)(long)-44 || slot > 0x1869f)
		return 0;
	unsigned int *rec = (unsigned int *)(heap + slot * 0x14);
	if (!*(unsigned int *)((unsigned char *)rec + 0x18))
		return 0;
	return (unsigned char *)(*(unsigned int *)((unsigned char *)rec + 0x24) +
				  *(unsigned int *)(heap + 0x1e8498));
}

int setup_global_resources(int param)
{
	/*
	 * The real first gate: any nonzero param is an immediate hard
	 * failure, before any allocation happens at all. `param`'s own
	 * origin is documented in oa_init.h as an unresolved absolute
	 * global read by init_module itself (likely a module parameter).
	 */
	if (param != 0) {
		return -1;
	}

	/* Step 1: CSTGCPUInfo -- explicit 36-byte alloc (confirmed literal
	 * immediate), NOT sizeof(CSTGCPUInfo) (this reconstruction's own
	 * class is a placeholder with no real data members yet). */
	void *cpuInfoStorage = ::operator new(0x24);
	CSTGCPUInfo *cpuInfo = cpuInfoStorage ? new (cpuInfoStorage) CSTGCPUInfo(0) : 0;

	/*
	 * Step 2: a first HeapManager::Alloc(0x6a578) whose result the real
	 * code discards entirely (confirmed: `eax` is immediately
	 * overwritten by a fresh `sInstance` reload before the return value
	 * is ever stored anywhere) -- a real "fire and forget" allocation,
	 * reproduced faithfully rather than "corrected" into something that
	 * looks more useful.
	 */
	(void)CSTGHeapManager::Alloc(0x6a578);

	unsigned char *heapBase = local_heap_base();

	/* Step 3: a second Alloc(0x294fc), whose slot IS resolved to a real
	 * address and becomes STGAPIFrontPanelStatus::sInstance. */
	unsigned int panelSlot = CSTGHeapManager::Alloc(0x294fc);
	if (heapBase)
		*(unsigned int *)heapBase = panelSlot;
	unsigned char *panel = (panelSlot <= 0x1869f) ? local_heap_region(panelSlot) : 0;
	STGAPIFrontPanelStatus::sInstance = panel;

	if (panel) {
		/*
		 * The real code writes this whole region field-by-field with
		 * mostly-zero immediates (hundreds of individually-inlined
		 * stores an unrolled loop never folded back) -- memset first,
		 * then apply only the confirmed non-zero overrides (see this
		 * header's own note on why).
		 */
		local_fill(panel, 0, STGAPI_FRONTPANEL_SIZE);

		panel[STGAPI_OFF_MIDI_ECHO0] = 0xff;
		panel[STGAPI_OFF_MIDI_ECHO1] = 0xff;
		panel[STGAPI_OFF_FOOTSWITCH0] = 0xff;
		panel[STGAPI_OFF_FOOTSWITCH1] = 0xff;
		panel[STGAPI_OFF_PANEL_TYPE] = 0x15;
		panel[STGAPI_OFF_PANEL_SUBTYPE] = 0x58;
		panel[STGAPI_OFF_PANEL_TYPE2] = 0x15;
		panel[STGAPI_OFF_PANEL_SUBTYPE2] = 0x58;

		/* 16 x 120-byte 0xff-fill grid, 128-byte stride (confirmed:
		 * only the first 120 of each 128-byte row is filled; the
		 * remaining 8 bytes/row are left at the memset zero above). */
		for (int row = 0; row < 16; row++)
			local_fill(panel + STGAPI_OFF_CAL_GRID + row * 0x80, 0xff, 0x78);

		panel[STGAPI_OFF_CAL_MARKER] = 0x04;
		*(unsigned int *)(panel + STGAPI_OFF_FIXED_CONST) = 0x473b8000;
	}

	/* Step 4: GetInstalledRAM() -> panel+0xd30. */
	unsigned int installedRAM = GetInstalledRAM();
	if (panel)
		*(unsigned int *)(panel + STGAPI_OFF_INSTALLED_RAM) = installedRAM;

	heapBase = local_heap_base(); /* real code reloads it here too */

	/* Step 5: shared MIDI header. */
	CSTGSharedMemory_CreateMidiShareHeader();

	/*
	 * Step 6: a third, big Alloc(0x123f2e3, ~19MB), whose slot is
	 * stored at heapBase+0xc; the first 256 x 0x604-byte entries
	 * (0x60400 bytes total) of the resolved region are zeroed
	 * (confirmed via disassembly to be pure zero writes throughout,
	 * nothing else -- same memset-equivalent treatment).
	 */
	unsigned int bigSlot = CSTGHeapManager::Alloc(0x123f2e3);
	if (heapBase)
		*(unsigned int *)(heapBase + 0xc) = bigSlot;
	unsigned char *bigRegion = local_heap_region(bigSlot);
	if (bigRegion)
		local_fill(bigRegion, 0, 256 * 0x604);

	/* Step 7: multisample bank manager / PCM precache manager --
	 * return values discarded in the real code (confirmed: neither is
	 * tested at its call site). */
	CSTGMultisampleBankManager multisampleMgr;
	multisampleMgr.Initialize();
	CSTGPCMPrecacheManager pcmPrecacheMgr;
	pcmPrecacheMgr.Initialize();

	if (panel)
		panel[0x10f4] = 0x90; /* confirmed literal write, meaning not further resolved */

	/*
	 * Step 8: NKS4 hardware version/type queries, feeding into a real
	 * 3-way front-panel-type branch (bits (byte@panel+4 & 0xc)>>2
	 * selects the case, bits (& 0x30)>>4 feed panel+0x29125 in every
	 * case) that writes different combinations of the confirmed panel
	 * fields depending on detected hardware.
	 *
	 * CORRECTED (2026-07-04): GetOmapVersion/GetPSocVersion take TWO
	 * OUTPUT POINTER params and return void, not a packed `unsigned int`
	 * -- see their own declarations in oa_setup_global_resources.h for
	 * the full confirmed shape and cross-confirmation against
	 * OmapNKS4Module's own independently-reconstructed implementation.
	 * The confirmed real field order is version-then-revision for both.
	 */
	unsigned char omapVersion = 0, omapRevision = 0;
	unsigned char psocVersion = 0, psocRevision = 0;
	COmapNKS4Driver_GetOmapVersion(&omapVersion, &omapRevision);
	COmapNKS4Driver_GetPSocVersion(&psocVersion, &psocRevision);
	unsigned char hwVer = COmapNKS4Driver_GetHardwareVersion();
	unsigned char is88 = (unsigned char)COmapNKS4Driver_Is88Key();
	if (panel) {
		panel[STGAPI_OFF_NKS4_HW_VERSION - 4] = omapVersion;  /* +0x29120 */
		panel[STGAPI_OFF_NKS4_HW_VERSION - 3] = omapRevision; /* +0x29121 */
		panel[STGAPI_OFF_NKS4_HW_VERSION - 2] = psocVersion;  /* +0x29122 */
		panel[STGAPI_OFF_NKS4_HW_VERSION - 1] = psocRevision; /* +0x29123 */
		panel[STGAPI_OFF_NKS4_HW_VERSION] = hwVer; /* +0x29124 */
		if (!is88) {
			panel[STGAPI_OFF_PANEL_TYPE] = 0x1c;
			panel[STGAPI_OFF_PANEL_SUBTYPE] = 0x49;
		}
	}

	/* Real ground truth calls this unconditionally with this=panel (no
	 * separate null check right at this call site) -- but `panel` CAN
	 * be null here (heap-alloc failure, see `panel` above), and every
	 * OTHER dereference of `panel` in this same function is already
	 * guarded the same way, so this guard is this file's own
	 * established defensive convention, not a new departure. */
	char calLoaded = panel ? SCalibrationData_LoadCalibrationFile(panel) : 0;
	if (calLoaded) {
		/* Real 3-way branch on `(panel[4] & 0xc) >> 2`: case 1 writes
		 * panel_detected=1,type=0x1c,subtype=0x49 (matching the same
		 * values as the !is88 override above); case 2 writes
		 * panel_detected=1,type=0x15,subtype=0x58 (matching the
		 * default fill above); the fallthrough/other case writes
		 * footswitch0=0x24,footswitch1=0x3d instead. Every case also
		 * writes panel+0x29125 = (panel[4]&0x30)>>4. */
		if (panel) {
			unsigned char raw = panel[4];
			unsigned char kind = (unsigned char)((raw & 0xc) >> 2);
			panel[STGAPI_OFF_NKS4_PANEL_KIND] = (unsigned char)((raw & 0x30) >> 4);
			if (kind == 1) {
				panel[STGAPI_OFF_PANEL_DETECTED] = 1;
				panel[STGAPI_OFF_PANEL_TYPE] = 0x1c;
				panel[STGAPI_OFF_PANEL_SUBTYPE] = 0x49;
			} else if (kind == 2) {
				panel[STGAPI_OFF_PANEL_DETECTED] = 1;
				panel[STGAPI_OFF_PANEL_TYPE] = 0x15;
				panel[STGAPI_OFF_PANEL_SUBTYPE] = 0x58;
			} else {
				panel[STGAPI_OFF_FOOTSWITCH0] = 0x24;
				panel[STGAPI_OFF_FOOTSWITCH1] = 0x3d;
			}
		}
		if (panel && panel[STGAPI_OFF_NKS4_HW_VERSION] != 3)
			SetupNKS4Calibration(panel, 0);
	}
	if (!panel || panel[STGAPI_OFF_NKS4_HW_VERSION] != 3)
		IncProgressBar();

	/*
	 * Step 9: bank memory pool (~179MB, `0xaaf1140`), then three
	 * AllocAligned calls carving CSTGGlobal/CMeteredDebugOutput/
	 * CSTGEngine/CSTGFrontPanel's own storage out of it.
	 */
	CSTGBankMemory::Initialize(panel, 0xaaf1140);
	unsigned char *globalStorage = CSTGBankMemory::AllocAligned(0x29cc4ec, 0x10);
	CSTGGlobal *global = globalStorage ? new (globalStorage) CSTGGlobal() : 0;

	void *debugOutputStorage = ::operator new(0xbb94);
	new (debugOutputStorage) CMeteredDebugOutput();

	unsigned char *engineStorage = CSTGBankMemory::AllocAligned(0x8, 0x10);
	CSTGEngine *engine = engineStorage ? new (engineStorage) CSTGEngine() : 0;

	unsigned char *frontPanelStorage = CSTGBankMemory::AllocAligned(0x110, 0x10);
	CSTGFrontPanel *frontPanel = frontPanelStorage ? new (frontPanelStorage) CSTGFrontPanel() : 0;

	/*
	 * The three real hard-fail checks, in this specific (confirmed,
	 * unusual) order: CSTGEngine's storage first, then
	 * CSTGFrontPanel's, then -- last, despite being allocated FIRST --
	 * the original CSTGCPUInfo allocation.
	 */
	if (!engineStorage)
		return -1;
	if (!frontPanelStorage)
		return -1;
	if (!cpuInfo)
		return -1;

	/* Step 10: CCostProfile, then a real vtable-dispatched call through
	 * its own slot 2 (no static relocation names the target -- see
	 * header note), then CSTGCPUInfo::Update fed from that object's
	 * own +4 float field. */
	void *costProfileStorage = ::operator new(0x12a0);
	CCostProfile *costProfile = new (costProfileStorage) CCostProfile();
	typedef void (*VtableSlot2Fn)(CCostProfile *);
	VtableSlot2Fn slot2 = ((VtableSlot2Fn *)CCostProfile::sInstance->_vtablePtr)[2];
	slot2(CCostProfile::sInstance);
	cpuInfo->Update(CCostProfile::sInstance->_field4);

	/* CORRECTS a real bug in an earlier pass of this reconstruction:
	 * the disassembly's own `mov eax,0x0` immediately preceding this
	 * call carries an R_386_32 relocation on
	 * `_ZN21CSTGSampleRateMonitor9sInstanceE` -- the "0x00000000" shown
	 * in a raw objdump listing is just the unlinked placeholder the
	 * linker fills in later, NOT a literal null "this". The real call
	 * passes `&CSTGSampleRateMonitor::sInstance` (the singleton
	 * POINTER's own address) as `this` -- the exact same confirmed
	 * "address of the singleton pointer, not its value" idiom already
	 * independently confirmed for `CSTGPerformanceVarsManager::
	 * Initialize()` in `CSTGGlobal::Initialize()` (sec 10.55), now
	 * confirmed a second, recurring time. Caught while reconstructing
	 * `CSTGSampleRateMonitor::Initialize()` itself (sec 10.57) and
	 * re-verifying this call site's own relocation, not by re-reading
	 * the disassembly from scratch -- an earlier pass had read the
	 * raw "0x0" immediate at face value without checking for a
	 * relocation on it. */
	((CSTGSampleRateMonitor *)&CSTGSampleRateMonitor::sInstance)->Initialize();

	if (engine)
		engine->Initialize();
	IncProgressBar();
	if (global)
		global->Initialize();
	IncProgressBar();
	if (frontPanel)
		frontPanel->Initialize();

	/* Step 11: CSTGASK, initialized with the SAME heapBase+0xc slot the
	 * big MIDI/cost buffer's slot was stored at earlier -- confirmed
	 * via disassembly, a genuine reuse, not a copy-paste artifact. */
	unsigned char *askArg = 0;
	if (heapBase) {
		unsigned int askSlot = *(unsigned int *)(heapBase + 0xc);
		unsigned char *resolved = local_heap_region(askSlot);
		askArg = (unsigned char *)(((unsigned long)(resolved) + 0xfff) & ~0xfffUL);
	}
	CSTGASK ask;
	ask.Initialize(askArg);

	(void)costProfile;
	return 0;
}

void cleanup_global_resources(void)
{
	/* Not yet disassembled -- 286 bytes, confirmed to exist (called
	 * symmetrically from init_module's unwind cascade). Left as a
	 * confirmed-real, not-yet-implemented no-op rather than guessed
	 * at, same treatment as CleanupSharedHeap before it was reconstructed. */
}

/*
 * CSTGMultisampleBankManager::Initialize() (sec 10.149, `.text+0x3cd00`,
 * 78 bytes) confirmed: resets 8 confirmed dwords -- a `CSTGMultisampleBankUUID`-
 * shaped 4-dword group at +0xa000..+0xa00c (matching the exact same 4
 * fields the real binary's own sibling `SetSamplingSessionID(UUID
 * const&)` copies FROM a caller-supplied UUID, confirmed via a
 * neighboring disassembly of that sibling), then +0xa010 (zeroed),
 * +0xa014 (the confirmed real 0xffffffff "unset" sentinel value already
 * seen elsewhere in this project, e.g. CSTGMidiPortManager's own +0xc/
 * +0x70/+0xd4), +0xa018/+0xa01c (zeroed). No calls, no branches.
 *
 * Same known, ALREADY-DOCUMENTED tradeoff as CSTGPCMPrecacheManager::
 * Initialize() just below (see this file's own real call site above,
 * `init_global_resources()` step 7): `CSTGMultisampleBankManager
 * multisampleMgr; multisampleMgr.Initialize();` is a throwaway stack
 * local whose real, fixed heap-arena address is `oa_multisample_bank_
 * manager()` (oa_heap.h, `oa_heap_base()+0x60524`) -- this reconstruction
 * keeps using the dummy stack `this` at that one call site (matching
 * this project's pre-existing, explicitly-flagged decision not to
 * refactor the CSTGGlobal/CSTGMultisampleBankManager declaration-
 * ecosystem conflict in this pass), so this call still writes past a
 * technically-1-byte empty-class stack object there -- exactly as
 * already tolerated for CSTGPCMPrecacheManager's own promotion (sec
 * 10.144). The real body itself is verified directly, on a properly-
 * sized buffer, in test_setup_global_resources.cpp instead.
 */
void CSTGMultisampleBankManager::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xa010) = 0;
	*(unsigned int *)(base + 0xa000) = 0;
	*(unsigned int *)(base + 0xa014) = 0xffffffff;
	*(unsigned int *)(base + 0xa018) = 0;
	*(unsigned int *)(base + 0xa01c) = 0;
	*(unsigned int *)(base + 0xa004) = 0;
	*(unsigned int *)(base + 0xa008) = 0;
	*(unsigned int *)(base + 0xa00c) = 0;
}

/*
 * CSTGPCMPrecacheManager::Initialize() (sec 10.144): see
 * oa_setup_global_resources.h for the full confirmed shape.
 */
bool CSTGPCMPrecacheManager::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	base[0x0] = 0;
	base[0x1] = 0;
	*(unsigned int *)(base + 0x4) = 0;
	*(unsigned int *)(base + 0xc) = 0;
	*(unsigned int *)(base + 0x10) = 0;
	*(unsigned int *)(base + 0x14) = 0;
	*(unsigned int *)(base + 0x18) = 0;
	base[0x28] = 0;
	base[0x29] = 0;
	return true;
}

/*
 * CSTGASK::Initialize(void*) (sec 10.145): see
 * oa_setup_global_resources.h for the full confirmed shape.
 */
void CSTGASK::Initialize(void *arg)
{
	SKMain_Initialize(arg);
}

static unsigned char *PCMPrecacheFromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int PCMPrecacheToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * CSTGPCMPrecacheManager::Reset(bool, bool, unsigned long) (sec 10.154):
 * see oa_setup_global_resources.h for the full confirmed shape, including
 * the confirmed real "scalar new/delete for a single element, array
 * new[]/delete[] otherwise" allocator-form quirk.
 */
bool CSTGPCMPrecacheManager::Reset(bool flagFromN3, bool flagFromN2, unsigned long count)
{
	unsigned char *base = (unsigned char *)this;

	base[0x0] = flagFromN3;
	base[0x1] = flagFromN2;
	*(unsigned int *)(base + 0x8) = 0;

	unsigned int oldCount = *(unsigned int *)(base + 0x4);
	unsigned int oldPtr = *(unsigned int *)(base + 0x14);
	if (oldPtr != 0) {
		if (oldCount == 1)
			operator delete(PCMPrecacheFromU32(oldPtr));
		else
			operator delete[](PCMPrecacheFromU32(oldPtr));
	}

	*(unsigned int *)(base + 0x4) = (unsigned int)count;
	*(unsigned int *)(base + 0x14) = 0;

	if ((long)count > 0) {
		unsigned char *elems;
		if (count == 1)
			elems = (unsigned char *)operator new((oa_size_t)0xc);
		else
			elems = (unsigned char *)operator new[]((oa_size_t)(count * 0xc));
		for (unsigned long i = 0; i < count; i++) {
			unsigned char *e = elems + i * 0xc;
			*(unsigned int *)(e + 0x0) = 0;
			*(unsigned int *)(e + 0x4) = 0;
			*(unsigned int *)(e + 0x8) = 0;
		}
		*(unsigned int *)(base + 0x14) = PCMPrecacheToU32(elems);
	}

	*(unsigned int *)(base + 0x18) = 0;
	*(unsigned int *)(base + 0xc) = 0;
	*(unsigned int *)(base + 0x10) = 0;
	base[0x28] = 0;
	base[0x29] = 0;
	return true;
}
