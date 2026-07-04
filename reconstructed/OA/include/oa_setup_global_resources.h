// SPDX-License-Identifier: GPL-2.0
/*
 * oa_setup_global_resources.h  -  setup_global_resources()/
 * cleanup_global_resources(): init_module step 8 (hard-fail), the "crux"
 * this project's own plan flagged in advance.
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko (`setup_global_resources`,
 * `.text+0x116c40`, 7267 bytes -- the largest single function this
 * project has reconstructed so far), then a full objdump disassembly +
 * relocation trace.
 *
 * SCOPE, stated plainly: this function allocates and constructs
 * essentially the whole engine (CSTGGlobal, CSTGEngine, CSTGFrontPanel,
 * CCostProfile, CSTGCPUInfo, CMeteredDebugOutput, CSTGSampleRateMonitor,
 * CSTGASK, plus several large heap-manager allocations and a big
 * hardware-status struct). Faithfully reconstructed here:
 *   - the real control flow: exact order of ~42 calls, and all THREE
 *     real hard-fail checks (confirmed via disassembly, not assumed):
 *     the initial `param != 0` early-out, and null-allocation checks on
 *     the CSTGEngine/CSTGFrontPanel/CSTGCPUInfo storage allocations
 *     (checked in that specific, unusual order -- CSTGCPUInfo's is
 *     checked LAST despite being allocated FIRST).
 *   - every literal size/constant used (0x6a578, 0x294fc, 0x123f2e3,
 *     0xaaf1140, 0x29cc4ec, 0x12a0, 0xbb94, 0x110, 0x8, 0x10 -- all
 *     confirmed immediates, not estimated).
 *   - the confirmed non-zero field writes into the large
 *     `STGAPIFrontPanelStatus` struct (0xff markers, a 16x120-byte
 *     0xff-fill grid, a 0x04 marker, a 0x473b8000 constant) and the
 *     NKS4-hardware-version-dependent 3-way front-panel-type branch.
 *   - the heap-manager "slot" resolution pattern (CSTGHeapManager::Alloc
 *     returns a small integer slot, later resolved to a real address via
 *     the SAME `sInstance+0x24+slot*0x14` + `sInstance+0x1e8498` formula
 *     this project already confirmed and uses in oa_heap.h's own
 *     `oa_heap_region()` -- re-derived locally here rather than reusing
 *     that helper directly, see the note below on why).
 *
 * NOT reconstructed field-by-field in this pass, honestly stubbed:
 *   - `STGAPIFrontPanelStatus`'s bulk all-zero region: the real code
 *     writes it via hundreds of individually-inlined zero-immediate
 *     stores (an unrolled loop the compiler never folded back) --
 *     behaviorally identical to a single `memset(0)` over that same
 *     byte range, which is what's done here instead of transcribing
 *     every single instruction.
 *   - the big ~19MB shared buffer's own zero-fill (256 entries x 1540
 *     bytes each, confirmed via disassembly to be ALL zero writes, no
 *     other values) -- same memset-equivalent treatment.
 *   - the virtual call through `CCostProfile`'s own vtable slot 2
 *     (`.text`'s disassembly shows a real indirect call via
 *     `[[CCostProfile::sInstance][0]][+8]` -- a genuine vtable dispatch
 *     with no static relocation naming the target). Declared as an
 *     opaque `void (*)()` call through a confirmed-real vtable slot,
 *     not guessed at further.
 *
 * A REAL, PRE-EXISTING STRUCTURAL ISSUE found while writing this header,
 * left as a known, documented tradeoff rather than fixed here: this
 * project already has TWO INCOMPATIBLE declarations of `CSTGGlobal`
 * across different headers -- `oa_types.h`'s minimal `struct CSTGGlobal
 * { static char *sInstance; };` (pulled in transitively by oa_heap.h)
 * and `oa_global.h`'s fuller `class CSTGGlobal` (with `static CSTGGlobal
 * *sInstance;` -- a DIFFERENT type for the same static member). They
 * currently coexist only because no single translation unit has ever
 * included both. This function needs the real `CSTGGlobal`/`CSTGEngine`
 * constructors (from oa_global.h/oa_engine.h) AND
 * `CSTGMultisampleBankManager`/`CSTGPCMPrecacheManager`'s `Initialize()`
 * (otherwise declared in oa_types.h) in the SAME translation unit --
 * hitting that exact conflict. Rather than refactor the whole project's
 * class-declaration structure (a separate, larger task), this header
 * declares its OWN local, method-only versions of
 * `CSTGMultisampleBankManager`/`CSTGPCMPrecacheManager` (matching their
 * real mangled names so future reconstruction of those classes' bodies
 * links correctly) instead of including oa_types.h. This is imperfect
 * (a real ODR risk if this header is ever co-included with oa_types.h
 * in the same TU) but avoids making the pre-existing conflict worse --
 * flagged here for whoever next consolidates these class declarations
 * project-wide.
 */

#ifndef OA_SETUP_GLOBAL_RESOURCES_H
#define OA_SETUP_GLOBAL_RESOURCES_H

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_bank_memory.h"
#include "oa_internal.h"  /* placement `operator new(size_t, void*)` */

/* Local, method-only declarations -- see the file header's own note on
 * why these don't reuse oa_types.h's versions. */
struct CSTGMultisampleBankManager { void Initialize(); };
struct CSTGPCMPrecacheManager {
	/*
	 * Initialize() (sec 10.144, `.text+0x46b10`, 56 bytes) confirmed:
	 * zeroes `fieldAt(0x0)`/`fieldAt(0x1)` (bytes), `fieldAt(0x4)`/
	 * `fieldAt(0xc)`/`fieldAt(0x10)`/`fieldAt(0x14)`/`fieldAt(0x18)`
	 * (dwords), `fieldAt(0x28)`/`fieldAt(0x29)` (bytes), then returns
	 * `true` unconditionally (`mov eax,0x1` immediately before `ret`) --
	 * a genuine return value, not `void` as this project's own earlier
	 * pass had guessed (this function's only confirmed real call site,
	 * `setup_global_resources.cpp` step 8, discards it, which is
	 * presumably why the `void` guess was never caught before).
	 */
	bool Initialize();
	/* AfterProcess()/Reset(bool,bool,unsigned long) (Bar 2, confirmed
	 * real via the real binary's own symbol table, own bodies not
	 * reconstructed) -- deliberately deferred externs. */
	void AfterProcess();
	void Reset(bool, bool, unsigned long);
};

/* CSTGHeapManager -- real singleton, matches oa_types.h's own
 * `static char *sInstance` convention (kept separate here for the same
 * reason as above), plus the one new method this function needs. */
struct CSTGHeapManager {
	static char *sInstance;
	/* Returns a small integer "slot" (confirmed via disassembly: the
	 * result is later checked against 100000 and used as an index --
	 * the exact same bound oa_heap.h's own oa_heap_region() checks --
	 * not a raw pointer), NOT independently named in the real binary
	 * beyond its mangled member-function symbol. */
	static unsigned int Alloc(unsigned int size);
};

/*
 * The confirmed hardware/calibration status struct this function builds
 * (STGAPIFrontPanelStatus::sInstance, a real singleton). Modeled as a
 * fixed-size opaque byte blob with named offset constants (matching
 * this project's own established convention for raw offset arithmetic
 * into not-fully-recovered structs, e.g. oa_cmd_proc.cpp's
 * `*(int*)(entry+0x10)`) rather than a padded C struct -- manually
 * computing every gap between ~15 non-contiguous confirmed fields
 * spanning a 0x29126-byte range is exactly the kind of arithmetic this
 * approach avoids getting subtly wrong. Sized to the highest confirmed
 * offset actually written (+0x29125), rounded up.
 *
 * Only the fields this function itself writes non-zero values into are
 * named via offset constants below; everything else is zeroed via
 * memset (see file header note) -- this project's own`.text` shows the
 * real code doing the equivalent via hundreds of individually-inlined
 * zero-immediate stores, not reconstructed instruction-by-instruction.
 */
#define STGAPI_FRONTPANEL_SIZE          0x29200
#define STGAPI_OFF_MIDI_ECHO0           0x00fc  /* confirmed 0xff */
#define STGAPI_OFF_MIDI_ECHO1           0x00fd  /* confirmed 0xff */
#define STGAPI_OFF_CAL_GRID             0x010b  /* 16 x [120 real + 8 skipped] bytes, see below */
#define STGAPI_OFF_CAL_MARKER           0x1091  /* confirmed 0x04 */
#define STGAPI_OFF_FOOTSWITCH0          0x1082
#define STGAPI_OFF_FOOTSWITCH1          0x1083
#define STGAPI_OFF_PANEL_DETECTED       0x108a
#define STGAPI_OFF_PANEL_TYPE           0x108b
#define STGAPI_OFF_PANEL_SUBTYPE        0x108c
#define STGAPI_OFF_PANEL_TYPE2          0x108e
#define STGAPI_OFF_PANEL_SUBTYPE2       0x108f
#define STGAPI_OFF_INSTALLED_RAM        0x0d30  /* GetInstalledRAM()'s result */
#define STGAPI_OFF_FIXED_CONST          0x29118 /* confirmed 0x473b8000 */
#define STGAPI_OFF_NKS4_HW_VERSION      0x29124
#define STGAPI_OFF_NKS4_PANEL_KIND      0x29125

struct STGAPIFrontPanelStatus {
	static unsigned char *sInstance;
};

/*
 * New classes this function constructs, none declared anywhere else in
 * this project yet. Bodies (beyond the confirmed ctor/Initialize calls
 * themselves) not reconstructed in this pass -- confirmed real via
 * relocation, same "declare the shape, defer the body" treatment as
 * every other not-yet-implemented class in this project.
 */
struct CSTGCPUInfo {
	static CSTGCPUInfo *sInstance;
	CSTGCPUInfo(unsigned int cpuCountOverride);
	void Update(float tickRateHz);

	/* Confirmed real fields (sec 10.57), starting cleanly at +0x0 (the
	 * constructor's own very first write): `cpuCount` (clamped to a
	 * confirmed max of 4), `khz` (from `stg_get_cpu_khz()`), then a
	 * derived "cycles per audio tick" value and several related
	 * quantities -- `field14` is the one field this project's own
	 * `CSTGSampleRateMonitor::Initialize()` directly depends on. */
	unsigned int cpuCount;	/* +0x0 */
	unsigned int khz;	/* +0x4 */
	float field8;		/* +0x8, cyclesPerTick */
	int fieldC;		/* +0xc, (int)cyclesPerTick */
	float field10;		/* +0x10, ~1/cyclesPerTick */
	float field14;		/* +0x14, cyclesPerTick (same as +0x8) */
	float field18;		/* +0x18, ~1/cyclesPerTick (same as +0x10) */
	float field1c;		/* +0x1c, 1000000.0/khz */
	int field20;		/* +0x20, (int)(0.05*cyclesPerTick) */
};

/*
 * CStartupFile -- confirmed real base class of CCostProfile (sec 10.60),
 * NOT independently reconstructed in this pass beyond its confirmed
 * shape: `CStartupFile::CStartupFile(char const*)` (.text+0x457c0, 10
 * bytes) and a virtual `~CStartupFile()` both exist as real mangled
 * symbols with their own vtable (`_ZTV12CStartupFile`), and
 * `CCostProfile::CCostProfile()`'s own disassembly proves the
 * inheritance directly: it calls this constructor FIRST (with the
 * literal string "CostProfile", extracted from `.rodata.str1.1+0x603`),
 * then immediately overwrites the vtable pointer with CCostProfile's
 * own (`&_ZTV12CCostProfile + 8`, the standard Itanium "+8 to skip
 * offset-to-top/RTTI" convention) -- but leaves `+0x4` completely
 * untouched (CCostProfile's own zeroing only starts at `+0x8`),
 * confirming `+0x4` is a real CStartupFile-OWNED field (matching this
 * project's earlier note that `CCostProfile::sInstance->_field4` is a
 * "confirmed real" value read by `CSTGCPUInfo::Update()` -- it's real
 * because CStartupFile's own not-yet-reconstructed constructor sets
 * it, not CCostProfile's). Declared here as an opaque 8-byte base
 * (vtable ptr + this one confirmed-to-exist field) -- other methods
 * (`Path()`, `Load()`, `Save(bool)`) are real and confirmed but out of
 * scope for this pass.
 */
class CStartupFile {
public:
	CStartupFile(const char *name);
	virtual ~CStartupFile();
	void *_vtablePtr;	/* +0x0 */
	float _field4;		/* +0x4, real CStartupFile-owned field */
};

/*
 * CCostProfile : public CStartupFile -- ground-truthed via readelf+
 * objdump (`-j .text`) against OA_real.ko, `.text+0x60100`, 2009 bytes.
 * Confirmed structure, cross-checked exactly against this project's
 * OWN already-established allocation size in setup_global_resources.cpp
 * (`::operator new(0x12a0)`, sec 10.48):
 *   1. CStartupFile::CStartupFile(this, "CostProfile") -- base ctor
 *   2. vtable pointer overwritten with `&_ZTV12CCostProfile + 8`
 *   3. +0x8..+0x327 (800 bytes) unconditionally zeroed, one dword at a
 *      time (a real compiler-unrolled sequence, not a loop -- 200
 *      individual `mov` instructions, confirmed via direct count)
 *   4. A real loop: 198 (0xf78/0x14) CCostProfileEntry objects starting
 *      at +0x328, EACH entry's own +0x4/+0x8/+0xc/+0x10 fields zeroed
 *      but its +0x0 field left COMPLETELY UNTOUCHED (never written
 *      anywhere in this constructor, before or during the loop) -- a
 *      real, confirmed quirk preserved verbatim rather than "fixed",
 *      matching this project's established policy (e.g. the keybed
 *      6-port off-by-one, sec 10.49; CPowerOffTimer's dead branch, sec
 *      10.59). Total size: 0x328 + 198*0x14 = 0x12a0, exactly matching
 *      the confirmed real allocation.
 *   5. sInstance = this
 */
struct CCostProfileEntry {
	unsigned int _unaccounted0;	/* +0x0, confirmed NEVER written by CCostProfile's own ctor */
	unsigned int field4;		/* +0x4, confirmed zeroed */
	unsigned int field8;		/* +0x8, confirmed zeroed */
	unsigned int fieldC;		/* +0xc, confirmed zeroed */
	unsigned int field10;		/* +0x10, confirmed zeroed */
};

#define CCOSTPROFILE_ENTRY_COUNT 198

class CCostProfile : public CStartupFile {
public:
	static CCostProfile *sInstance;
	CCostProfile();
	unsigned char _unrecovered_zeroed[0x328 - 0x8]; /* +0x8..+0x327, confirmed all zeroed */
	CCostProfileEntry entries[CCOSTPROFILE_ENTRY_COUNT]; /* +0x328.., confirmed count/stride */
};

struct CMeteredDebugOutput {
	static CMeteredDebugOutput *sInstance;
	CMeteredDebugOutput();
};

struct CSTGFrontPanel {
	static CSTGFrontPanel *sInstance;
	CSTGFrontPanel();
	void Initialize();

	/*
	 * RequestAnalogInputStatus(eSTGAnalogDeviceCode) (sec 10.131,
	 * .text+0xbee0, 26 bytes, confirmed via CSTGControllerRTData::
	 * RequestAnalogInputPositions's own 19 call sites) confirmed:
	 * ignores its own `this` entirely, instead building a hardware
	 * command word (`0x1a00000 | (deviceCode << 8)`) and forwarding it
	 * directly to the confirmed real, genuinely external
	 * `OmapNKS4OutputFifo_WriteCommand()` (already one of this
	 * project's own audited 32 real external dependencies, sec
	 * 10.119/10.120). Real enum type not modeled -- represented as a
	 * plain `unsigned int` (regparm passes it in a register regardless
	 * of the real enum's declared width).
	 */
	void RequestAnalogInputStatus(unsigned int deviceCode);
};

struct CSTGSampleRateMonitor {
	static CSTGSampleRateMonitor *sInstance;
	/* CORRECTED (sec 10.57): NOT called with `this=0` -- the real
	 * disassembly's `mov eax,0x0` immediately before this call carries
	 * an R_386_32 relocation on `sInstance` itself, meaning the real
	 * `this` is `&CSTGSampleRateMonitor::sInstance` (the singleton
	 * POINTER's own address), the same confirmed "address of the
	 * singleton pointer" idiom already independently confirmed for
	 * `CSTGPerformanceVarsManager::Initialize()`. An earlier pass read
	 * the raw "0x0" at face value without checking for a relocation on
	 * it -- see setup_global_resources.cpp's own updated call site and
	 * MASTER_REFERENCE.md sec 10.57 for the full correction. */
	void Initialize();

	/* Confirmed real fields, accessed via raw offset arithmetic in
	 * engine_startup_bits.cpp (matching this project's established
	 * convention -- not named struct members here, since this class's
	 * layout before +0x8 isn't otherwise recovered): +0x8 = a shifted
	 * (<<8) cached copy of CSTGCPUInfo's own +0x14, +0xc = a 256-entry
	 * `int` table (0xc..0x40b) uniformly seeded from the same
	 * unshifted value. */
};

struct CSTGASK {
	void Initialize(void *arg);
};

extern "C" {

/*
 * The following are all confirmed-real (via relocation) not-yet-
 * reconstructed functions this project's own established convention
 * declares as externs -- same treatment as init_module's other steps.
 */
unsigned int GetInstalledRAM(void);
void CSTGSharedMemory_CreateMidiShareHeader(void);
/*
 * CORRECTED (2026-07-04): a fresh, careful re-disassembly of this
 * call site (setup_global_resources.cpp's own step 8) found these two
 * take TWO OUTPUT POINTER parameters and return void -- NOT a packed
 * `unsigned int` return value as this project's own earlier pass
 * (sec 10.48) had modeled them. This independently CONFIRMS (rather
 * than contradicts) `OmapNKS4Module/driver.cpp`'s own already-
 * reconstructed real implementation (`void COmapNKS4Driver_GetOmapVersion(
 * unsigned char *v, unsigned char *r) { *v=...; *r=...; }`), arrived at
 * completely independently from that OTHER binary's own disassembly in
 * an earlier session -- a strong double-confirmation, not a coincidence.
 * The real caller passes (v, r) as (eax, edx) and stores `*v` at the
 * LOWER of the two consecutive panel offsets it writes, `*r` at the
 * higher one, for both Omap and PSoc (see setup_global_resources.cpp's
 * own updated comment for the exact confirmed offsets).
 */
void COmapNKS4Driver_GetOmapVersion(unsigned char *version, unsigned char *revision);
void COmapNKS4Driver_GetPSocVersion(unsigned char *version, unsigned char *revision);
unsigned char COmapNKS4Driver_GetHardwareVersion(void);
int COmapNKS4Driver_Is88Key(void);
/* Confirmed real (relocation from CSTGGlobal::SetNKS4TestModeFlag,
 * sec 10.90); already a real unresolved (`U`) symbol in OA_real.ko
 * itself, not a gap on this project's part. Parameter CORRECTED
 * (2026-07-04) from `bool` to `int`, matching OmapNKS4Module's own
 * already-reconstructed real implementation (`void
 * COmapNKS4Driver_SetTestMode(int on) { ... (on != 0) ... }`). */
void COmapNKS4Driver_SetTestMode(int testMode);
char SCalibrationData_LoadCalibrationFile(void);
/*
 * CORRECTED (2026-07-04): takes TWO params, not one -- confirmed via
 * the real call site (setup_global_resources.cpp's own step 8 tail,
 * .text+0x11860c), which sets up `edx=0` (a literal) immediately
 * before the call. The real function body itself (OmapNKS4Module.ko's
 * own disassembly) stores BOTH eax and edx into two separate globals,
 * confirming it genuinely consumes both, not just eax with edx as
 * dead/ignored register noise. The second param's own real meaning
 * isn't resolved by this one call site alone (always passes the
 * literal 0 here); modeled as an `int flag` pending further evidence.
 */
void SetupNKS4Calibration(void *panel, int flag);
void SetupKeybedCalibration(void *panel);
void SCalibrationData_InitAll(void);
void IncProgressBar(void);

int setup_global_resources(int param);
void cleanup_global_resources(void);

} /* extern "C" */

#endif /* OA_SETUP_GLOBAL_RESOURCES_H */
