// SPDX-License-Identifier: GPL-2.0
/*
 * engine_startup_bits2.cpp  -  a second small batch of confirmed-real
 * CSTGEngine::Initialize() dependencies (sec 10.58's own deferred list),
 * picked as the highest-leverage next targets: CSTGHeapManager (its own
 * file, see heap_manager.cpp/oa_heapmanager.h), stg_rtfifo_cleanup (see
 * rtfifo_init.cpp), and here: CLoadBalancer::Initialize()/~CLoadBalancer(),
 * CPowerOffTimer::Initialize(), CSTGDiskCostManager::Initialize(),
 * CSTGCommonLFO::Initialize(), CSTGCommonStepSeq::Initialize().
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CLoadBalancer::Initialize()        .text+0x60cb0, 223B
 *   CLoadBalancer::~CLoadBalancer()    .text+0x60c90,  25B (both D1/D2 alias the same code)
 *   CPowerOffTimer::Initialize()       .text+0x5d8a0, 387B
 *   CSTGDiskCostManager::Initialize()  .text+0x62970, 256B
 *   CSTGCommonLFO::Initialize()        .text+0x89990,  67B
 *   CSTGCommonStepSeq::Initialize()    .text+0x8b370,  67B
 *
 * All float math here is reproduced with plain C float arithmetic
 * (matching this project's own established substitute for the real
 * x87/SSE instruction sequences, e.g. engine_startup_bits.cpp/sec
 * 10.57) rather than replicating `rcpss`'s approximate-reciprocal
 * semantics bit-for-bit -- immaterial to Bar 2 (symbol resolution),
 * and not independently exercised by any confirmed caller in this pass.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_bank_memory.h"

extern "C" void PushUnsolicitedMessage(void *msg);

/*
 * CLoadBalancer::Initialize() -- confirmed real algorithm: reads
 * CSTGCPUInfo::sInstance's cpuCount (+0x0) and cyclesPerTick-as-int
 * (+0xc, `fieldC`) into STGAPIFrontPanelStatus's own status blob
 * (+0x1091 byte, +0x10b0/+0x10b4 dwords -- the 0x59682f00 literal at
 * +0x10b0 is a confirmed raw bit pattern, not independently decoded as
 * a meaningful float or int in this pass), computes
 * `0.15f * cyclesPerTick` (truncated to int) as a "grace" threshold
 * stored at +0x7c, two fixed literal constants (1552000/1000000,
 * microsecond-scale) at +0x88/+0x8c, zeroes +0x90/+0x94, then computes
 * `1.0f / (float)(cyclesPerTick_int - grace)` stored at +0x98 (the real
 * code uses `rcpss`, an approximate reciprocal -- see file header),
 * and finally zeroes four more STGAPIFrontPanelStatus fields
 * (+0x10a0/+0x10a4/+0x10a8/+0x10ac). Returns true (1) unconditionally.
 */
void CLoadBalancer::Initialize()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	unsigned char *status = STGAPIFrontPanelStatus::sInstance;
	CSTGCPUInfo *cpuInfo = CSTGCPUInfo::sInstance;

	status[0x1091] = (unsigned char)cpuInfo->cpuCount;
	*(unsigned int *)(status + 0x10b0) = 0x59682f00u;
	*(unsigned int *)(status + 0x10b4) = (unsigned int)cpuInfo->fieldC;

	int grace = (int)(0.15f * cpuInfo->field8);
	*(int *)(this_ + 0x7c) = grace;
	*(unsigned int *)(this_ + 0x88) = 1552000u;
	*(unsigned int *)(this_ + 0x8c) = 1000000u;
	*(unsigned int *)(this_ + 0x90) = 0;
	*(unsigned int *)(this_ + 0x94) = 0;

	int diff = cpuInfo->fieldC - grace;
	float recip = 1.0f / (float)diff;
	*(float *)(this_ + 0x98) = recip;

	*(unsigned int *)(status + 0x10ac) = 0;
	*(unsigned int *)(status + 0x10a8) = 0;
	*(unsigned int *)(status + 0x10a4) = 0;
	*(unsigned int *)(status + 0x10a0) = 0;
}

CLoadBalancer::~CLoadBalancer()
{
	CLoadBalancer::sInstance = 0;
	emergencyStealer.~CEmergencyStealer();
}

/*
 * CPowerOffTimer::Initialize() -- confirmed real, panel-type-dependent
 * auto-power-off timer setup. Default lead time (stored at +0xc) is
 * 180 seconds' worth of "ticks" (`60.0f * 3.0f * CSTGAudioBusManager::
 * sInstance->busGainScale`, the confirmed +0x4 field of that class).
 * Panel type (STGAPIFrontPanelStatus+0x5 byte) selects a per-type
 * timeout from a confirmed 3-entry table (1200/3600/14400 "seconds"),
 * indices 0/1/2; types 0x16(22) and 3 are unconditional special cases
 * (fixed 120s lead, timer permanently disabled -- state flag +0x14=0,
 * threshold +0x8/+0x4=-1); any other type, or a table entry of 0, also
 * disables the timer the same way. Timer threshold values <= 1800s
 * (20/30 min) or an out-of-range/disabled type fall through a shared
 * "use 120s lead" path; 1800 < threshold <= 3600 uses a 180s lead;
 * threshold > 3600 uses a 300s lead. A trailing `cmp state,2` branch
 * (pushing an unsolicited message, real literal struct
 * {0x10,1,0,0x29,0}) is preserved verbatim even though it's
 * unreachable from any state this function itself produces (state is
 * only ever 0 or 1 at that point) -- a real, confirmed-but-effectively-
 * dead branch, matching this project's "preserve real quirks" policy
 * (e.g. the keybed 6-port off-by-one, sec 10.49).
 */
void CPowerOffTimer::Initialize()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	unsigned char *status = STGAPIFrontPanelStatus::sInstance;

	this_[0x0] = 0;
	*(unsigned int *)(this_ + 0x10) = 0;

	int defaultLead = (int)(60.0f * bus->busGainScale * 3.0f);
	*(int *)(this_ + 0xc) = defaultLead;

	unsigned char panelType = status[0x5];

	if (panelType == 0x16 || panelType == 0x3) {
		*(unsigned int *)(this_ + 0x8) = 0xffffffffu;
		*(unsigned int *)(this_ + 0x14) = 0;
		*(int *)(this_ + 0xc) = (int)(120.0f * bus->busGainScale);
		*(unsigned int *)(this_ + 0x4) = 0xffffffffu;
		return;
	}

	static const unsigned int kTable[3] = { 1200, 3600, 14400 };
	unsigned int tableVal = (panelType <= 2) ? kTable[panelType] : 0;

	unsigned int state;
	unsigned int threshold;
	if (tableVal != 0) {
		threshold = (unsigned int)((float)tableVal * bus->busGainScale);
		state = 1;
		*(unsigned int *)(this_ + 0x8) = threshold;

		if (tableVal > 1800) {
			float leadSeconds = (tableVal <= 3600) ? 180.0f : 300.0f;
			*(int *)(this_ + 0xc) = (int)(leadSeconds * bus->busGainScale);
			*(unsigned int *)(this_ + 0x4) = threshold;
			*(unsigned int *)(this_ + 0x14) = 1;
			return;
		}
		/* tableVal <= 1800: fall through to the shared 120s-lead path. */
	} else {
		threshold = 0xffffffffu;
		state = 0;
		*(unsigned int *)(this_ + 0x8) = threshold;
		*(unsigned int *)(this_ + 0x14) = 0;
	}

	*(int *)(this_ + 0xc) = (int)(120.0f * bus->busGainScale);

	if (state == 2) {
		/* Confirmed-real but unreachable given this function's own
		 * possible `state` values -- see file header note. */
		*(unsigned int *)(this_ + 0x14) = 1;
		unsigned char msg[16] = { 0 };
		*(unsigned short *)(msg + 0x0) = 0x10;
		*(unsigned short *)(msg + 0x2) = 0x1;
		*(unsigned int *)(msg + 0x4) = 0;
		*(unsigned int *)(msg + 0x8) = 0x29;
		*(unsigned int *)(msg + 0xc) = 0;
		PushUnsolicitedMessage(msg);
	}

	*(unsigned int *)(this_ + 0x4) = threshold;
	*(unsigned int *)(this_ + 0x14) = 1;
}

/*
 * CSTGDiskCostManager::Initialize() -- confirmed real, a mix of plain
 * field resets (all fields default to 400/0x190, except +0x44 = a
 * confirmed raw float bit pattern 0.03125f) and a shared "recompute
 * watermarks if +0x8/+0xc/+0x10 are all still 0" idiom applied twice
 * (once for +0x10, once again immediately after explicitly zeroing
 * +0x10) -- both checks are unconditionally true on a fresh object,
 * matching this project's "preserve real quirks" policy rather than
 * simplifying away the apparent redundancy (this is very likely a
 * shared reset routine also reused by CSTGDiskCostManager::
 * ResetWaterMarks(), not reconstructed in this pass). Also writes three
 * dwords into STGAPIFrontPanelStatus (+0x10f8/+0x10fc/+0x1100) and two
 * `fisttp`-truncated float->int conversions into +0x1104/+0x1110
 * (0.0f and a confirmed 0.5f constant, both trivial given +0x30/+0x3c
 * are freshly zeroed above), plus a fixed +0x10f0 = 0x20000 write.
 */
void CSTGDiskCostManager::Initialize()
{
	unsigned char *this_ = reinterpret_cast<unsigned char *>(this);
	unsigned char *status = STGAPIFrontPanelStatus::sInstance;

	*(unsigned int *)(this_ + 0x0) = 0x190;
	*(unsigned int *)(this_ + 0x8) = 0;
	*(unsigned int *)(this_ + 0xc) = 0;
	*(unsigned int *)(this_ + 0x4) = 0x190;
	*(unsigned int *)(this_ + 0x2c) = 0;
	*(unsigned int *)(this_ + 0x30) = 0;
	*(unsigned int *)(this_ + 0x34) = 0;
	*(unsigned int *)(this_ + 0x44) = 0x3d000000u;	/* 0.03125f */
	*(unsigned int *)(this_ + 0x14) = 0x190000;
	*(unsigned int *)(this_ + 0x18) = 0x20000;
	*(unsigned int *)(this_ + 0x1c) = 0;
	*(unsigned int *)(this_ + 0x28) = 0;
	*(unsigned int *)(this_ + 0x38) = 0;
	*(unsigned int *)(this_ + 0x3c) = 0;
	*(unsigned int *)(this_ + 0x40) = 0;
	*(unsigned int *)(status + 0x10f8) = 0;

	unsigned int sum1 = *(unsigned int *)(this_ + 0xc) +
			    *(unsigned int *)(this_ + 0x8) +
			    *(unsigned int *)(this_ + 0x10);
	if (sum1 == 0) {
		*(unsigned int *)(this_ + 0x14) = 0x190000;
		*(unsigned int *)(this_ + 0x18) = 0x20000;
		*(unsigned int *)(this_ + 0x1c) = 0;
	}

	*(unsigned int *)(this_ + 0x10) = 0;
	*(unsigned int *)(status + 0x10fc) = 0;

	unsigned int field8 = *(unsigned int *)(this_ + 0x8);
	unsigned int sum2 = *(unsigned int *)(this_ + 0xc) + field8 +
			    *(unsigned int *)(this_ + 0x10);
	if (sum2 == 0) {
		*(unsigned int *)(this_ + 0x14) = 0x190000;
		*(unsigned int *)(this_ + 0x18) = 0x20000;
		*(unsigned int *)(this_ + 0x1c) = 0;
	}

	*(unsigned int *)(status + 0x1100) = field8;
	*(int *)(status + 0x1104) = (int)(*(float *)(this_ + 0x30));
	*(int *)(status + 0x1110) = (int)(0.5f + *(float *)(this_ + 0x3c));
	*(unsigned int *)(status + 0x10f0) = 0x20000;
}

/*
 * CSTGCommonLFO::Initialize() / CSTGCommonStepSeq::Initialize() --
 * confirmed real: allocate one big CSTGBankMemory pool (0x4a00/0x2000
 * bytes) and carve it into 32 fixed-stride blocks (0x250/0x100 bytes
 * each), calling CSTGLFOBase::InitializeQuad()/CSTGStepSeqBase::
 * InitializeQuad() on each block's start address.
 */
void CSTGCommonLFO::Initialize()
{
	sSubRateParams = (STGLFOSubRateParams *)
		CSTGBankMemory::AllocAligned(0x4a00, 0x10);

	unsigned char *p = (unsigned char *)sSubRateParams;
	unsigned char *end = p + 0x4a00;
	while (p != end) {
		CSTGLFOBase::InitializeQuad((STGLFOSubRateParams *)p);
		p += sizeof(STGLFOSubRateParams);
	}
}

void CSTGCommonStepSeq::Initialize()
{
	sSubRateParams = (STGStepSeqSubRateParams *)
		CSTGBankMemory::AllocAligned(0x2000, 0x10);

	unsigned char *p = (unsigned char *)sSubRateParams;
	unsigned char *end = p + 0x2000;
	while (p != end) {
		CSTGStepSeqBase::InitializeQuad((STGStepSeqSubRateParams *)p);
		p += sizeof(STGStepSeqSubRateParams);
	}
}

STGLFOSubRateParams *CSTGCommonLFO::sSubRateParams;
STGStepSeqSubRateParams *CSTGCommonStepSeq::sSubRateParams;
