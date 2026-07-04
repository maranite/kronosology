// SPDX-License-Identifier: GPL-2.0
/*
 * engine_startup_bits.cpp  -  a batch of small, confirmed-real
 * methods on classes `setup_global_resources` (oa_setup_global_resources.h)
 * already declares, all default-constructed/initialized unconditionally
 * on OA.ko's own boot path: `CSTGFrontPanel::CSTGFrontPanel()`/
 * `Initialize()`, `CMeteredDebugOutput::CMeteredDebugOutput()`,
 * `CSTGCPUInfo::CSTGCPUInfo(unsigned int)`/`Update(float)`, and
 * `CSTGSampleRateMonitor::Initialize()`. Kept together in one file
 * since each is individually small (12-203 bytes) and they share a
 * common confirmed dependency chain (`CSTGCPUInfo` feeds
 * `CSTGSampleRateMonitor`).
 *
 * Ground-truthed via a full objdump -d -r disassembly of each real
 * function in OA_real.ko.
 *
 * `CSTGFrontPanel::CSTGFrontPanel()` (.text+0xbd70, 12 bytes): sets
 * the vtable pointer and `sInstance = this`, nothing else.
 *
 * `CSTGFrontPanel::Initialize()` (.text+0xbd80, 72 bytes): zeroes 3
 * confirmed flag bytes (+0x104/+0x105/+0x106), sets 2 confirmed float
 * constants (+0x108 = 0.9921875 = 127/128 exactly, +0x10c =
 * 0.03448275849223137 -- confirmed real .rodata immediates, real
 * meaning not determined beyond the literal values), then fills a
 * 128-entry identity-mapped byte table (byte[i] = i, at +0x4+i) and a
 * 128-entry zeroed byte table (at +0x84+i) in the same loop.
 *
 * `CMeteredDebugOutput::CMeteredDebugOutput()` (.text+0xb610, 107
 * bytes): the same real mutex-setup sequence (`rtwrap_pthread_
 * mutexattr_init`/`_settype`/`get_sizeof_rtwrap_pthread_mutex`/
 * `rtwrap_malloc`/`rtwrap_pthread_mutex_init`/`_mutexattr_destroy`)
 * already confirmed for other manager classes in managers.cpp, storing
 * the malloc'd mutex at +0x0. Confirms this class is a mutex-protected
 * ring buffer (matching this project's established file-daemon
 * ring-buffer precedent): the buffer itself is embedded starting at
 * +0x4, and the write cursor (+0xbb84) is initialized to point at it;
 * +0xbb88 (a flag byte) and +0xbb8c (a dword, presumably a read
 * cursor or count) are zeroed. `sInstance = this`.
 *
 * `CSTGCPUInfo::CSTGCPUInfo(unsigned int cpuCountOverride)`
 * (.text+0x23310, 172 bytes) and `CSTGCPUInfo::Update(float
 * tickRateHz)` (.text+0x233c0, 105 bytes) share the exact same
 * confirmed formula, differing only in the divisor's source (the
 * constructor uses a fixed literal 1500.0 -- matching this project's
 * own already-confirmed 1500Hz audio-tick rate from CSTGGlobal::
 * IncrementMicrosecondCount -- while Update() uses its own passed-in
 * argument instead): `cyclesPerTick = (khz * 1000.0) / divisor`. The
 * constructor also gathers `cpuCount = stg_num_online_cpus()`
 * (overridden by the constructor's own argument if nonzero) clamped to
 * a confirmed max of 4, and `khz = stg_get_cpu_khz()`. Both then
 * derive: `+0x8 = cyclesPerTick` (float), `+0xc = (int)cyclesPerTick`,
 * `+0x10`/`+0x18 = 1/cyclesPerTick` (an approximate SSE `rcpss`
 * reciprocal in the real disassembly -- modeled here with an exact
 * `1.0f/x` division instead, a confirmed-honest simplification since
 * no downstream code this project has reconstructed depends on
 * `rcpss`'s specific ~12-bit-precision rounding), `+0x14 =
 * cyclesPerTick` (confirmed the SAME value as +0x8, re-derived via a
 * genuinely intricate x87/SSE register interleaving in the real
 * disassembly -- traced carefully instruction-by-instruction, not
 * assumed), `+0x1c = 1000000.0 / khz`, `+0x20 = (int)(0.05 *
 * cyclesPerTick)`. `+0x14` is the one field this project's own
 * `CSTGSampleRateMonitor::Initialize()` (below) directly depends on.
 *
 * `CSTGSampleRateMonitor::Initialize()` (.text+0x672c0, 74 bytes)
 * reads `CSTGCPUInfo::sInstance->+0x14` (confirmed = `cyclesPerTick`
 * above) as a float, truncates to int, shifts left by 8 bits, stores
 * at `+0x8`, then fills a confirmed 256-entry `int` table at `+0xc`
 * with that same truncated (unshifted) integer value -- a uniform
 * initial "cycles per tick" table, presumably later refined by actual
 * measurements (matching this class's own confirmed `Run()`/
 * `GetMeasuredSampleRate()` siblings, not reconstructed in this pass).
 */

#include "oa_setup_global_resources.h"

extern "C" {
unsigned int stg_num_online_cpus(void);
unsigned int stg_get_cpu_khz(void);

/* Same real mutex-setup primitives managers.cpp's own manager
 * constructors already use -- declared here too since no shared
 * header exists yet for them (matching this project's established
 * per-TU duplication for these). */
void rtwrap_pthread_mutexattr_init(void *attr);
int  get_pthread_recursive_attr_constant(void);
void rtwrap_pthread_mutexattr_settype(void *attr, int type);
void rtwrap_pthread_mutexattr_destroy(void *attr);
unsigned int get_sizeof_rtwrap_pthread_mutex(void);
void *rtwrap_malloc(unsigned int size);
void rtwrap_pthread_mutex_init(void *mutex, void *attr);
}

CSTGFrontPanel *CSTGFrontPanel::sInstance;
CMeteredDebugOutput *CMeteredDebugOutput::sInstance;
CSTGCPUInfo *CSTGCPUInfo::sInstance;
/* CSTGSampleRateMonitor's own body/constructor isn't reconstructed in
 * this pass (only Initialize(), below) -- its `sInstance` static
 * storage is defined here anyway since setup_global_resources.cpp's
 * own corrected call site (sec 10.57) now takes its address. */
CSTGSampleRateMonitor *CSTGSampleRateMonitor::sInstance;

CSTGFrontPanel::CSTGFrontPanel()
{
	CSTGFrontPanel::sInstance = this;
}

void CSTGFrontPanel::Initialize()
{
	unsigned char *self = (unsigned char *)this;
	self[0x104] = 0;
	self[0x105] = 0;
	self[0x106] = 0;
	*(float *)(self + 0x108) = 0.9921875f;
	*(float *)(self + 0x10c) = 0.03448275849223137f;
	for (unsigned int i = 0; i < 0x80; i++) {
		self[4 + i] = (unsigned char)i;
		self[0x84 + i] = 0;
	}
}

CMeteredDebugOutput::CMeteredDebugOutput()
{
	unsigned char attr[64]; /* real target uses a real, sized rtwrap
				  * pthread_mutexattr_t; the host size doesn't
				  * need to match exactly since this project's
				  * KATs only mock the rtwrap_* calls. */
	rtwrap_pthread_mutexattr_init(attr);
	rtwrap_pthread_mutexattr_settype(attr, get_pthread_recursive_attr_constant());
	unsigned char *self = (unsigned char *)this;
	*(void **)self = rtwrap_malloc(get_sizeof_rtwrap_pthread_mutex());
	rtwrap_pthread_mutex_init(*(void **)self, attr);
	rtwrap_pthread_mutexattr_destroy(attr);
	self[0xbb88] = 0;
	*(unsigned int *)(self + 0xbb8c) = 0;
	CMeteredDebugOutput::sInstance = this;
	/* Confirmed real 32-bit pointer field on the target -- modeled as
	 * `unsigned int` (truncate on write) rather than a native host
	 * pointer, since an 8-byte host write here would span into the
	 * confirmed-real +0xbb88 flag byte right after it (they're
	 * tightly packed with no gap on the real 32-bit target). Same
	 * established fix as CSTGGlobal::CSTGGlobal()'s own tightly-packed
	 * list fields (sec 10.55/10.56) -- caught here via this method's
	 * own host KAT the same way. */
	*(unsigned int *)(self + 0xbb84) = (unsigned int)(unsigned long)(self + 4);
}

CSTGCPUInfo::CSTGCPUInfo(unsigned int cpuCountOverride)
{
	CSTGCPUInfo::sInstance = this;
	cpuCount = stg_num_online_cpus();
	unsigned int khzVal = stg_get_cpu_khz();
	khz = khzVal;
	if (cpuCountOverride != 0)
		cpuCount = cpuCountOverride;
	if (cpuCount > 4)
		cpuCount = 4;

	float cyclesPerTick = ((float)khzVal * 1000.0f) / 1500.0f;
	field8 = cyclesPerTick;
	fieldC = (int)cyclesPerTick;
	field10 = 1.0f / cyclesPerTick;
	field14 = cyclesPerTick;
	field18 = 1.0f / cyclesPerTick;
	field1c = 1000000.0f / (float)khzVal;
	field20 = (int)(0.05 * cyclesPerTick);
}

void CSTGCPUInfo::Update(float tickRateHz)
{
	float cyclesPerTick = ((float)khz * 1000.0f) / tickRateHz;
	field8 = cyclesPerTick;
	fieldC = (int)cyclesPerTick;
	field10 = 1.0f / cyclesPerTick;
	field14 = cyclesPerTick;
	field18 = 1.0f / cyclesPerTick;
	field1c = 1000000.0f / (float)khz;
	field20 = (int)(0.05 * cyclesPerTick);
}

void CSTGSampleRateMonitor::Initialize()
{
	unsigned char *self = (unsigned char *)this;
	int cyclesPerTick = (int)CSTGCPUInfo::sInstance->field14;
	*(unsigned int *)(self + 0x8) = (unsigned int)cyclesPerTick << 8;
	for (unsigned int i = 0; i < 0x100; i++)
		*(int *)(self + 0xc + i * 4) = cyclesPerTick;
}
