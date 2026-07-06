// SPDX-License-Identifier: GPL-2.0
/*
 * managers.cpp  -  constructors for a batch of the smallest manager classes
 * named in CSTGEngine::Initialize()'s confirmed construction table
 * (MASTER_REFERENCE.md sec 10.13/10.14). See include/oa_engine.h for the
 * class declarations and per-class ground-truthing notes.
 *
 * Ground-truthed offsets:
 *   CSTGDiskCostManager::CSTGDiskCostManager()       .text+0x62950  (6 bytes)
 *   CSTGSamplingDaemon::CSTGSamplingDaemon()         .text+0x11c600 (26 bytes)
 *   CSTGHDRFileReader::CSTGHDRFileReader()           .text+0x11ba60 (33 bytes)
 *   CSTGHDRFileWriter::CSTGHDRFileWriter()           .text+0x11c2d0 (33 bytes)
 *   CSTGStreamingFileReader::CSTGStreamingFileReader() .text+0x11a9d0 (47 bytes)
 *   CSTGFileCloser::CSTGFileCloser()                 .text+0x1199b0 (47 bytes)
 *   CSTGMetronome::CSTGMetronome()                   .text+0xd5e00  (56 bytes)
 *   CSTGTempoUtils::CSTGTempoUtils()                 .text+0x26ff0  (103 bytes)
 *   CSTGFileOpener::CSTGFileOpener()                 .text+0x119bd0 (953 bytes)
 *   CSTGCDWorker::CSTGCDWorker()                     .text+0x11b2a0 (77 bytes)
 *   CPowerOffTimer::CPowerOffTimer()                 .text+0x5d860  (58 bytes)
 *   CSTGAudioDriverInterfaceKorgUsb::CSTGAudioDriverInterfaceKorgUsb()
 *                                                     .text+0x340090 (57 bytes)
 *   CSTGVoiceModelManager::CSTGVoiceModelManager()   .text+0x1a9950 (143 bytes)
 *   CLoadBalancer::CLoadBalancer()                   .text+0x60b70  (281 bytes)
 *   CSTGMonitorMixer::CSTGMonitorMixer()             .text+0x69000  (6 bytes)
 *   CSTGAudioBusManager::CSTGAudioBusManager()       .text+0x23460  (60 bytes)
 *   CSTGEffectManager::CSTGEffectManager()           .text+0x207ef0 (103 bytes)
 *   CSTGHDRManager::CSTGHDRManager()                 .text+0xd3d60  (1061 bytes,
 *                                                     partially reconstructed --
 *                                                     see oa_engine.h)
 *   CSTGVoiceAllocator::CSTGVoiceAllocator()         .text+0x4b750  (4491 bytes,
 *                                                     partially reconstructed --
 *                                                     see oa_engine.h)
 *   CSTGAudioManager::CSTGAudioManager()             .text+0x649d0  (5785 bytes,
 *                                                     partially reconstructed --
 *                                                     see oa_engine.h)
 *   CSTGMessageProcessor::CSTGMessageProcessor()     .text+0xebb60  (5930 bytes,
 *                                                     partially reconstructed --
 *                                                     see oa_engine.h)
 */

#include "oa_global.h"		/* for ResolveActivePerformanceVarsManagerRaw(), sec 10.144 */
#include "oa_engine.h"
#include "oa_engine_init.h"	/* for CSTGPerformance::IsCurrentlyActive(), sec 10.144 */
#include "oa_bank_memory.h"
#include "oa_internal.h"
#include "oa_new_delete.h"	/* for __kmalloc/OA_GFP_KERNEL, sec 10.148 (CSTGCDWorker_InitializeBuffer) */

/* Host/target pointer-width helpers (ToU32/FromU32 per this project's
 * established convention) -- moved here (used to live further down, right
 * before the sec 10.144 batch) so CSTGHDRCircularBuffer's own methods
 * (sec 10.158), inserted earlier in this file right after CSTGHDRManager's
 * ctor, can use them too. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

CSTGDiskCostManager     *CSTGDiskCostManager::sInstance;
CSTGSamplingDaemon      *CSTGSamplingDaemon::sInstance;
CSTGHDRFileReader       *CSTGHDRFileReader::sInstance;
CSTGHDRFileWriter       *CSTGHDRFileWriter::sInstance;
CSTGStreamingFileReader *CSTGStreamingFileReader::sInstance;
CSTGFileCloser          *CSTGFileCloser::sInstance;
CSTGMetronome           *CSTGMetronome::sInstance;
CSTGTempoUtils          *CSTGTempoUtils::sInstance;
CSTGFileOpener          *CSTGFileOpener::sInstance;
CSTGCDWorker            *CSTGCDWorker::sInstance;
CPowerOffTimer          *CPowerOffTimer::sInstance;
CEmergencyStealer       *CEmergencyStealer::sInstance;
CLoadBalancer           *CLoadBalancer::sInstance;
CSTGVoiceModelManager   *CSTGVoiceModelManager::sInstance;
CSTGMonitorMixer        *CSTGMonitorMixer::sInstance;
CSTGAudioBusManager     *CSTGAudioBusManager::sInstance;
CSTGEffectManager       *CSTGEffectManager::sInstance;
CSTGHDRManager          *CSTGHDRManager::sInstance;
CSTGVoiceAllocator      *CSTGVoiceAllocator::sInstance;
CSTGAudioManager        *CSTGAudioManager::sInstance;
CSTGMessageProcessor    *CSTGMessageProcessor::sInstance;

/* Opaque sub-object constructors -- real bodies not reconstructed in this
 * pass (see oa_engine.h's CSTGPlaybackBuffer/CSTGMonitorMixerChannel/
 * CSTGSlotState comments). Defined empty purely so the owning classes'
 * arrays of each can be placement-constructed. */
CSTGPlaybackBuffer::CSTGPlaybackBuffer() { }
CSTGMonitorMixerChannel::CSTGMonitorMixerChannel() { }
CSTGSlotState::CSTGSlotState() { }

/* Real kernel-side RTAI recursive-mutex wrappers, confirmed via relocation
 * (same allocate/init shape as CPowerOffTimer's already-confirmed mutex,
 * but with an explicit recursive attribute this time). Not host-testable,
 * same treatment as the other rtwrap_* externs in this file. */
extern "C" void rtwrap_pthread_mutexattr_init(void *attr);
extern "C" int  get_pthread_recursive_attr_constant(void);
extern "C" void rtwrap_pthread_mutexattr_settype(void *attr, int type);
extern "C" void rtwrap_pthread_mutexattr_destroy(void *attr);

/* Real kernel-side RTAI condition-variable wrappers, confirmed via
 * relocation (CSTGAudioManager's constructor pairs these with the mutex
 * wrappers above, twice). */
extern "C" unsigned int get_sizeof_rtwrap_pthread_cond(void);
extern "C" void rtwrap_pthread_cond_init(void *cond, void *attr);

/*
 * Module-global state CSTGAudioBusManager's constructor touches, confirmed
 * via relocation (see oa_engine.h's CSTGAudioBusManager comment):
 *   STGAPILR2IndivToPhysBusId -- a confirmed real 20-byte/5-int `.rodata`
 *                                table (sec 10.132, extracted directly
 *                                from the binary at .rodata+0xa82c),
 *                                indexed by `SetLRBusIndivAssign()`'s own
 *                                `busIndex` parameter.
 *   gAllPlusHeadroom/gAllMinusHeadroom -- two 4-float arrays, reset to
 *                                {1,1,1,1}/{-1,-1,-1,-1} by this
 *                                constructor (confirmed via .rodata.cst16's
 *                                raw bytes at the relocation targets).
 */
unsigned char STGAPILR2IndivToPhysBusId[20] = {
	44, 0, 0, 0,  32, 0, 0, 0,  38, 0, 0, 0,  40, 0, 0, 0,  42, 0, 0, 0,
};
extern float gAllPlusHeadroom[4];
extern float gAllMinusHeadroom[4];

/* Pure virtual destructors still need a definition -- every derived
 * destructor's implicit chaining calls this. Empty: CSTGAudioDriverInterface
 * itself has no confirmed fields of its own to tear down. */
CSTGAudioDriverInterface::~CSTGAudioDriverInterface() { }

/* Real kernel-side RTAI wrapper functions -- confirmed real symbol names
 * via relocation, not host-testable (same treatment as __kmalloc/kfree).
 * `attr` is a real `void *` (a pthread_mutexattr_t pointer, or null for
 * "default attributes") -- CPowerOffTimer's call site below passes a
 * literal `0` (a valid null-pointer constant), CSTGVoiceAllocator's later
 * in this file passes a real address. */
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void);
extern "C" void *rtwrap_malloc(unsigned int size);
extern "C" void rtwrap_pthread_mutex_init(void *mutex, void *attr);

CSTGDiskCostManager::CSTGDiskCostManager()
{
	CSTGDiskCostManager::sInstance = this;
}

CSTGSamplingDaemon::CSTGSamplingDaemon()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x0) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0x4) = 0;
	CSTGSamplingDaemon::sInstance = this;
}

CSTGHDRFileReader::CSTGHDRFileReader()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x0) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0x4) = 0;
	CSTGHDRFileReader::sInstance = this;
	*(unsigned int *)(p + 0x10) = 0;
}

CSTGHDRFileWriter::CSTGHDRFileWriter()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x0) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0x4) = 0;
	CSTGHDRFileWriter::sInstance = this;
	*(unsigned int *)(p + 0x10) = 0;
}

CSTGStreamingFileReader::CSTGStreamingFileReader()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x0) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0x4) = 0;
	CSTGStreamingFileReader::sInstance = this;
	*(unsigned int *)(p + 0x10) = 0x8000;	/* confirmed: 32768, likely a buffer size */
	*(unsigned int *)(p + 0x18) = 0;
	*(unsigned int *)(p + 0x1c) = 0;
}

CSTGFileCloser::CSTGFileCloser()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x0) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0x4) = 0;
	*(unsigned int *)(p + 0x10) = 0;
	*(unsigned int *)(p + 0x18) = 0;
	*(unsigned int *)(p + 0x14) = 0;
	CSTGFileCloser::sInstance = this;
}

CSTGMetronome::CSTGMetronome()
{
	unsigned char *p = (unsigned char *)this;
	const float kQuarter = 0.25f;	/* confirmed .rodata value, 0x3e800000 */

	p[0x0] &= 0xfa;			/* clear bits 0,2 -- see class comment in oa_engine.h */
	p[0xd] = 0x00;
	p[0xc] = 0x30;
	*(unsigned int *)(p + 0x14) = 0;
	*(float *)(p + 0x1c) = kQuarter;
	*(float *)(p + 0x18) = kQuarter;
	*(float *)(p + 0x10) = kQuarter;
	*(unsigned int *)(p + 0x20) = 0xac4;	/* confirmed: 2756 */
	p[0x24] = 0x01;
	*(unsigned int *)(p + 0x28) = 0xffffffff;
	CSTGMetronome::sInstance = this;
}

CSTGTempoUtils::CSTGTempoUtils()
{
	unsigned char *p = (unsigned char *)this;
	CSTGTempoUtils::sInstance = this;
	*(unsigned int *)(p + 0x04) = 0;
	p[0x00] = 0;
	p[0x01] = 0;
	p[0x08] = 0;
	p[0x09] = 0;
	*(unsigned int *)(p + 0x0c) = 0;
	*(unsigned int *)(p + 0x10) = 0;
	p[0x18] = 0;
	*(unsigned int *)(p + 0x14) = 0;
	*(unsigned int *)(p + 0x1c) = 0;
	*(unsigned int *)(p + 0x20) = 0;
	p[0x28] = 0;
	*(unsigned int *)(p + 0x24) = 0;
	*(unsigned int *)(p + 0x2c) = 0;
	*(unsigned int *)(p + 0x30) = 0;
	p[0x34] = 0;
	*(unsigned int *)(p + 0x38) = 0;
}

CSTGCDWorker::CSTGCDWorker()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x000) = 0;
	*(unsigned int *)(p + 0x228) = 0;
	*(unsigned int *)(p + 0x230) = 0;
	*(unsigned int *)(p + 0x22c) = 0;
	CSTGCDWorker::sInstance = this;
	*(unsigned int *)(p + 0x224) = 0;
	p[0x004] = 0;
	*(unsigned int *)(p + 0x00c) = 0;
	*(unsigned int *)(p + 0x010) = 0;
	*(unsigned int *)(p + 0x020) = 0;
	/* +0x234 (ring buffer capacity/modulus) deliberately NOT zeroed here --
	 * confirmed absent from the real constructor; set elsewhere. */
}

CSTGFileOpener::CSTGFileOpener()
{
	unsigned char *p = (unsigned char *)this;

	*(unsigned int *)(p + 0x00) = 0;
	*(unsigned int *)(p + 0x08) = 0;
	*(unsigned int *)(p + 0x04) = 0;
	/* +0x0c: confirmed untouched by the real constructor. */

	/* 32 identical 16-byte "slots" from +0x10 to +0x20c, each zeroing its
	 * own +0/+4/+8 and leaving its own +0xc untouched -- see the class
	 * comment in oa_engine.h for what's confirmed vs. not about these. */
	for (int i = 0; i < 32; i++) {
		unsigned char *slot = p + 0x10 + i * 0x10;
		*(unsigned int *)(slot + 0x0) = 0;
		*(unsigned int *)(slot + 0x8) = 0;
		*(unsigned int *)(slot + 0x4) = 0;
	}

	/* The ring buffer's own base/write-index/read-index, at +0x210/+0x214/
	 * +0x218 -- same shape as a 33rd slot, but confirmed (via
	 * ProcessCommands()) to actually be this daemon's command queue
	 * control block, not another data slot. */
	*(unsigned int *)(p + 0x210) = 0;
	*(unsigned int *)(p + 0x218) = 0;
	*(unsigned int *)(p + 0x214) = 0;
	/* +0x21c (capacity) deliberately NOT zeroed here -- same confirmed
	 * "set later" pattern as CSTGCDWorker's +0x234. */

	CSTGFileOpener::sInstance = this;
}

CPowerOffTimer::CPowerOffTimer()
{
	unsigned char *p = (unsigned char *)this;

	p[0x00] = 0;
	unsigned int mutexSize = get_sizeof_rtwrap_pthread_mutex();
	void *mutex = rtwrap_malloc(mutexSize);
	/* Stored as a 32-bit value, not a native `void*`: on the real 32-bit
	 * target a pointer is exactly 4 bytes, which is all the confirmed
	 * object layout has room for at +0x18 (the last field, object ends at
	 * +0x1c/28 bytes total) -- a native 8-byte host pointer would overrun
	 * that confirmed size. */
	*(unsigned int *)(p + 0x18) = (unsigned int)(unsigned long)mutex;
	rtwrap_pthread_mutex_init(mutex, 0);
	CPowerOffTimer::sInstance = this;
	*(unsigned int *)(p + 0x14) = 0;
}

/* Plain C-style trampoline for the +0x3c callback field -- the real field
 * is a flat function pointer (invoked from elsewhere in the binary via a
 * C-style call, not a C++ member-function-pointer call), so a real member
 * function pointer's compiler-specific, often multi-word representation
 * would not be faithful here. */
static void CSTGAudioDriverInterfaceKorgUsb_CallbackTrampoline(CSTGAudioDriverInterfaceKorgUsb *self, void *arg)
{
	self->Callback(arg);
}

CSTGAudioDriverInterfaceKorgUsb::CSTGAudioDriverInterfaceKorgUsb()
{
	/* Vtable pointer at +0x00 is set automatically by C++ (base class
	 * constructor sets the base vtable; this constructor's own implicit
	 * store sets the derived one) -- matches the confirmed disassembly,
	 * which turned out to be a relocated vtable-pointer store, not the
	 * literal `8` it first looked like. Named-member writes (not raw
	 * offset casts) -- see the class comment in oa_engine.h for why this
	 * one class needs that treatment. */
	channelsIn = 6;
	channelsOut = 6;
	selfPtr = this;
	/* confirmed to be a relocated pointer to Callback(void*), not literal
	 * 0 -- same "checked the relocation before trusting the disassembly's
	 * literal-looking immediate" catch as the vtable-pointer store above. */
	callbackFnPtr = (void *)&CSTGAudioDriverInterfaceKorgUsb_CallbackTrampoline;
}

void CSTGAudioDriverInterfaceKorgUsb::Callback(void *)
{
	/* Not reconstructed in this pass. */
}

CSTGAudioDriverInterfaceKorgUsb::~CSTGAudioDriverInterfaceKorgUsb()
{
	/* Real destructor (.text+0x33f7e0, 69 bytes) not reconstructed in this
	 * pass -- see the class comment in oa_engine.h. */
}

CSTGVoiceModelManager::CSTGVoiceModelManager()
{
	unsigned char *p = (unsigned char *)this;

	CSTGVoiceModelManager::sInstance = this;
	*(void **)(p + 0x00) = CSTGBankMemory::AllocAligned(0x52d00, 16);
	*(void **)(p + 0x04) = CSTGBankMemory::AllocAligned(0x9f600, 16);
	for (unsigned int off = 0x08; off <= 0x2c; off += 4)
		*(unsigned int *)(p + off) = 0;
	*(unsigned short *)(p + 0x58) = 0;
	CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors();
}

/*
 * ProcessAudioRate(unsigned int)/ProcessSubRate(unsigned int) (sec
 * 10.137): see oa_engine.h for the full confirmed shape.
 */
void CSTGVoiceModelManager::ProcessAudioRate(unsigned int tick)
{
	unsigned char *p = (unsigned char *)this;
	typedef void (*VtableSlot4cFn)(void *, unsigned int);
	for (int i = 0; i < *(short *)(p + 0x58); i++) {
		void *item = *(void **)(p + 0x30 + i * 4);
		void **vtable = *(void ***)item;
		((VtableSlot4cFn)vtable[0x4c / 4])(item, tick);
	}
}
void CSTGVoiceModelManager::ProcessSubRate(unsigned int tick)
{
	unsigned char *p = (unsigned char *)this;
	typedef void (*VtableSlot48Fn)(void *, unsigned int);
	for (int i = 0; i < *(short *)(p + 0x58); i++) {
		void *item = *(void **)(p + 0x30 + i * 4);
		void **vtable = *(void ***)item;
		((VtableSlot48Fn)vtable[0x48 / 4])(item, tick);
	}
}

/*
 * ~CSTGVoiceModelManager() (sec 10.147): see oa_engine.h for the full
 * confirmed shape, including the register-vs-memory `count` quirk this
 * reproduces via an explicit local variable refreshed only after a
 * non-null entry's virtual call.
 */
CSTGVoiceModelManager::~CSTGVoiceModelManager()
{
	unsigned char *p = (unsigned char *)this;
	typedef void (*VtableSlot4Fn)(void *);
	unsigned short count = *(unsigned short *)(p + 0x58);

	for (unsigned short i = 0; i < count; i++) {
		void *item = *(void **)(p + 0x30 + i * 4);
		if (item) {
			void **vtable = *(void ***)item;
			((VtableSlot4Fn)vtable[4 / 4])(item);
			count = *(unsigned short *)(p + 0x58);
		}
	}
}

CEmergencyStealer::CEmergencyStealer()
{
	/* Real constructor's OWN full body (`.text+0x5d5b0`, 134 bytes) is
	 * NOT reconstructed in this pass -- it goes on to compute several
	 * further float-derived fields from CCostProfile::sInstance/
	 * CSTGCPUInfo::sInstance (a genuinely larger task). This one
	 * statement is, however, independently confirmed real (sec 10.148,
	 * cross-checked directly against the real ctor while reconstructing
	 * the sibling destructor below): the ctor's very first instruction
	 * is exactly `CEmergencyStealer::sInstance = this`. */
	CEmergencyStealer::sInstance = this;
}

/*
 * CEmergencyStealer::~CEmergencyStealer() (`.text+0x5d640`, 11 bytes,
 * sec 10.148): confirmed real and complete -- `CEmergencyStealer::
 * sInstance = 0;` unconditionally (same "no self-check before nuking
 * the singleton" quirk already confirmed for CSTGVoiceAllocator's/
 * CSTGMessageProcessor's own destructors, sec 10.147), nothing else.
 * Called explicitly, non-virtually, from CLoadBalancer::~CLoadBalancer()
 * (engine_startup_bits2.cpp) on its embedded `emergencyStealer` member.
 */
CEmergencyStealer::~CEmergencyStealer()
{
	CEmergencyStealer::sInstance = 0;
}

CLoadBalancer::CLoadBalancer()
{
	/* emergencyStealer's own constructor runs automatically (C++ member
	 * initialization order), matching the confirmed real call at the top
	 * of CLoadBalancer's constructor. */
	unsigned char *p = (unsigned char *)this;

	static const unsigned int zeroedOffsets[] = {
		0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c,
		0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78,
		0x7c, 0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0,
	};
	for (unsigned int off : zeroedOffsets)
		*(unsigned int *)(p + off) = 0;
	p[0xa4] = 0;

	CLoadBalancer::sInstance = this;
}

CSTGMonitorMixer::CSTGMonitorMixer()
{
	CSTGMonitorMixer::sInstance = this;
}

CSTGAudioBusManager::CSTGAudioBusManager()
{
	busGainReciprocal   = 0.0006666666595265269f;
	busGainScale        = 1500.0f;
	physBusIdTableHead  = *(int *)STGAPILR2IndivToPhysBusId;

	CSTGAudioBusManager::sInstance = this;

	gAllPlusHeadroom[0]  = gAllPlusHeadroom[1]  = gAllPlusHeadroom[2]  = gAllPlusHeadroom[3]  =  1.0f;
	gAllMinusHeadroom[0] = gAllMinusHeadroom[1] = gAllMinusHeadroom[2] = gAllMinusHeadroom[3] = -1.0f;
}

void CSTGAudioBusManager::SetLRBusIndivAssign(int busIndex)
{
	physBusIdTableHead = ((int *)STGAPILR2IndivToPhysBusId)[busIndex];
}

CSTGEffectManager::CSTGEffectManager()
{
	CSTGEffectManager::sInstance = this;

	zeroedCounter = 0;
	for (unsigned int i = 0; i < 198; i++)
		zeroedTable[i] = 0;

	/* +0xb1c..+0xb63 (_unrecovered_gap) deliberately left untouched here --
	 * confirmed real gap, not an oversight (see oa_engine.h). */

	defaultTempoA = 120.0f;
	defaultTempoB = 120.0f;
	_tailZeroed[0] = _tailZeroed[1] = _tailZeroed[2] = _tailZeroed[3] = 0;
}

CSTGHDRManager::CSTGHDRManager()
{
	/* playbackBuffers[16] is default-constructed automatically (a real
	 * C++ array member) -- matches the confirmed clean 88-byte-stride
	 * array with nothing else interleaved between elements. */

	/* +0x584..+0x5a4 gap: only 3 of these 32 bytes are confirmed zeroed
	 * by the real constructor (absolute +0x590/+0x594/+0x598, i.e.
	 * _unrecovered_gap[0xc]/[0x10]/[0x14]); the rest are left untouched
	 * here too, matching the real, partial gap (see oa_engine.h). */
	*(unsigned int *)(_unrecovered_gap + 0x0c) = 0;
	*(unsigned int *)(_unrecovered_gap + 0x10) = 0;
	*(unsigned int *)(_unrecovered_gap + 0x14) = 0;

	/* CSTGMonitorMixerChannel[16] at a confirmed 192-byte stride, each
	 * holding a real 172-byte CSTGMonitorMixerChannel in its first 172
	 * bytes. Channels 0-14 (not the last) get 3 more confirmed-zeroed
	 * dwords right after their true body (+0xac/+0xb0/+0xb4); channel 15
	 * does not -- see oa_engine.h's CSTGHDRManager comment for exactly
	 * how that asymmetry was confirmed (CSTGSampler's construction
	 * begins immediately at channel 15's true 172-byte boundary, with no
	 * trailing pad). */
	for (int i = 0; i < 16; i++) {
		new (monitorMixerChannelSlots[i]) CSTGMonitorMixerChannel();
		if (i < 15) {
			*(unsigned int *)(monitorMixerChannelSlots[i] + 0xac) = 0;
			*(unsigned int *)(monitorMixerChannelSlots[i] + 0xb0) = 0;
			*(unsigned int *)(monitorMixerChannelSlots[i] + 0xb4) = 0;
		}
	}

	/* Everything from here on in the real constructor -- a CSTGSampler,
	 * a 17th standalone CSTGPlaybackBuffer, a CSTGHDRCircularBuffer, a
	 * CSTGPlaybackEvent, a vtable-patched CCDAudioInputMixer, and the
	 * CSTGCDAudioPlay::sInstance aliasing -- is confirmed (see
	 * oa_engine.h's class comment) but not reconstructed in this pass. */

	CSTGHDRManager::sInstance = this;
}

/*
 * CSTGHDRCircularBuffer -- all nine confirmed real methods (sec 10.158).
 * See oa_engine.h's own class comment for the full confirmed field list;
 * comments here focus on each method's own control flow.
 */
CSTGHDRCircularBuffer::CSTGHDRCircularBuffer()
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x18) = 0;
	*(unsigned int *)(p + 0x1c) = 0;
	*(unsigned int *)(p + 0x08) = 0;
	*(unsigned int *)(p + 0x0c) = 0;
	*(unsigned int *)(p + 0x10) = 0;
	*(unsigned int *)(p + 0x14) = 0;
	*(unsigned int *)(p + 0x28) = 0;
	*(unsigned int *)(p + 0x2c) = 0;
	*(unsigned int *)(p + 0x00) = 0;
	p[0x04] = 0;
}

void CSTGHDRCircularBuffer::Initialize(unsigned long totalSizeArg, bool flag, unsigned char extra)
{
	unsigned char *p = (unsigned char *)this;

	*(unsigned int *)(p + 0x18) = totalSizeArg;
	*(unsigned int *)(p + 0x1c) = totalSizeArg;
	p[0x04] = flag;

	unsigned int allocSize = (unsigned int)extra + totalSizeArg;
	unsigned char *buf = CSTGBankMemory::AllocAligned(allocSize, 0x10);

	unsigned int es = *(unsigned int *)(p + 0x1c);
	*(unsigned int *)(p + 0x08) = ToU32(buf);
	*(unsigned int *)(p + 0x10) = ToU32(buf);
	*(unsigned int *)(p + 0x0c) = ToU32(buf);
	*(unsigned int *)(p + 0x20) = 0;
	*(unsigned int *)(p + 0x24) = es;
	*(unsigned int *)(p + 0x14) = ToU32(buf) + es;
	*(unsigned int *)(p + 0x28) = 0;
	*(unsigned int *)(p + 0x2c) = 0;
}

void CSTGHDRCircularBuffer::SetEffectiveSize(unsigned long newSize)
{
	unsigned char *p = (unsigned char *)this;
	unsigned int base = *(unsigned int *)(p + 0x08);

	*(unsigned int *)(p + 0x1c) = newSize;
	*(unsigned int *)(p + 0x24) = newSize;
	*(unsigned int *)(p + 0x10) = base;
	*(unsigned int *)(p + 0x0c) = base;
	*(unsigned int *)(p + 0x20) = 0;
	*(unsigned int *)(p + 0x14) = base + newSize;
	*(unsigned int *)(p + 0x28) = 0;
	*(unsigned int *)(p + 0x2c) = 0;
}

void CSTGHDRCircularBuffer::Reset()
{
	unsigned char *p = (unsigned char *)this;
	unsigned int base = *(unsigned int *)(p + 0x08);
	unsigned int es = *(unsigned int *)(p + 0x1c);

	*(unsigned int *)(p + 0x10) = base;
	*(unsigned int *)(p + 0x0c) = base;
	*(unsigned int *)(p + 0x20) = 0;
	*(unsigned int *)(p + 0x24) = es;
	*(unsigned int *)(p + 0x14) = base + es;
	*(unsigned int *)(p + 0x28) = 0;
	*(unsigned int *)(p + 0x2c) = 0;
}

/* Real, faithfully-preserved asymmetry vs. AdvanceFillPosition() below:
 * this method clamps its own decrement of availableReadBytes (via the
 * confirmed `cmova`-based min()) so it can never go negative;
 * AdvanceFillPosition() does NOT clamp its own availableFillBytes
 * decrement. */
void CSTGHDRCircularBuffer::AdvanceReadPosition(unsigned long n)
{
	unsigned char *p = (unsigned char *)this;
	unsigned int bufferEnd = *(unsigned int *)(p + 0x14);
	unsigned int readPos = *(unsigned int *)(p + 0x0c);
	unsigned int remaining = bufferEnd - readPos;

	if (remaining > n) {
		*(unsigned int *)(p + 0x0c) = readPos + n;
	} else {
		unsigned int base = *(unsigned int *)(p + 0x08);
		*(unsigned int *)(p + 0x0c) = base + (n - remaining);
	}

	unsigned int availableReadBytes = *(unsigned int *)(p + 0x20);
	unsigned int cappedN = (n > availableReadBytes) ? availableReadBytes : n;
	*(unsigned int *)(p + 0x20) = availableReadBytes - cappedN;
	*(unsigned int *)(p + 0x28) = *(unsigned int *)(p + 0x28) + cappedN;
}

void CSTGHDRCircularBuffer::AdvanceFillPosition(unsigned long n)
{
	unsigned char *p = (unsigned char *)this;
	unsigned int bufferEnd = *(unsigned int *)(p + 0x14);
	unsigned int fillPos = *(unsigned int *)(p + 0x10);
	unsigned int remaining = bufferEnd - fillPos;

	*(unsigned int *)(p + 0x24) = *(unsigned int *)(p + 0x24) - n;

	if (remaining > n) {
		*(unsigned int *)(p + 0x10) = fillPos + n;
	} else {
		unsigned int base = *(unsigned int *)(p + 0x08);
		*(unsigned int *)(p + 0x10) = base + (n - remaining);
	}
}

/* Reconciles `fillCarry` (+0x28, credited by AdvanceReadPosition()/
 * ReturnUnusedFillBytes()) into `availableFillBytes`, using `readCarry`
 * (+0x2c) as a high-water mark of how much of `fillCarry` has already been
 * folded in -- after this call, readCarry == fillCarry. */
void CSTGHDRCircularBuffer::ReaderDaemonAdjustAvailableFillBytes()
{
	unsigned char *p = (unsigned char *)this;
	unsigned int readCarry = *(unsigned int *)(p + 0x2c);
	unsigned int fillCarry = *(unsigned int *)(p + 0x28);
	unsigned int diff = fillCarry - readCarry;

	*(unsigned int *)(p + 0x24) = *(unsigned int *)(p + 0x24) + diff;
	*(unsigned int *)(p + 0x2c) = readCarry + diff;
}

void CSTGHDRCircularBuffer::IncrementAvailableReadBytes(unsigned long n)
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x20) = *(unsigned int *)(p + 0x20) + n;
}

void CSTGHDRCircularBuffer::ReturnUnusedFillBytes(unsigned long n)
{
	unsigned char *p = (unsigned char *)this;
	*(unsigned int *)(p + 0x20) = *(unsigned int *)(p + 0x20) - n;
	*(unsigned int *)(p + 0x28) = *(unsigned int *)(p + 0x28) + n;
}

CSTGVoiceAllocator::CSTGVoiceAllocator()
{
	/* +0x0000..+0x0898: 50 self-referencing "empty list node" records,
	 * 44 bytes apart -- confirmed exact field set, no sub-object ctor
	 * involved. */
	for (int i = 0; i < 50; i++) {
		unsigned char *p = selfRefNodes[i];
		*(unsigned int *)(p + 0x00) = 0;
		*(unsigned int *)(p + 0x04) = 0;
		*(unsigned int *)(p + 0x08) = 0;
		*(unsigned int *)(p + 0x24) = (unsigned int)(unsigned long)p;
		*(unsigned int *)(p + 0x1c) = 0;
		*(unsigned int *)(p + 0x20) = 0;
		*(unsigned int *)(p + 0x28) = 0;
		*(unsigned int *)(p + 0x10) = 0;
		*(unsigned int *)(p + 0x0c) = 0;
		*(unsigned short *)(p + 0x14) = 0;
		*(unsigned short *)(p + 0x16) = 0;
	}

	/* +0x0898..+0x0bb8: 800-byte gap, confirmed untouched by this ctor
	 * (_unrecovered_gap1, deliberately left alone here too). */

	/* +0x0bb8..+0xb478: 400 records, 108 bytes apart, each zeroing three
	 * small field groups and pointing 4 more fields (+0x34/+0x44/+0x54/
	 * +0x64 relative) back at the record's OWN base address -- confirmed
	 * exact field set via relocation-resolved instruction operands, a
	 * different shape from selfRefNodes above (4 back-pointers to one
	 * shared base, not each field pointing at itself). */
	for (int i = 0; i < 400; i++) {
		unsigned char *p = ownerBackRefRecords[i];
		unsigned int self = (unsigned int)(unsigned long)p;
		*(unsigned int *)(p + 0x24) = 0;
		*(unsigned int *)(p + 0x20) = 0;
		*(unsigned int *)(p + 0x28) = 0;
		*(unsigned int *)(p + 0x34) = self;
		*(unsigned int *)(p + 0x2c) = 0;
		*(unsigned int *)(p + 0x30) = 0;
		*(unsigned int *)(p + 0x38) = 0;
		*(unsigned int *)(p + 0x44) = self;
		*(unsigned int *)(p + 0x3c) = 0;
		*(unsigned int *)(p + 0x40) = 0;
		*(unsigned int *)(p + 0x48) = 0;
		*(unsigned int *)(p + 0x54) = self;
		*(unsigned int *)(p + 0x4c) = 0;
		*(unsigned int *)(p + 0x50) = 0;
		*(unsigned int *)(p + 0x58) = 0;
		*(unsigned int *)(p + 0x64) = self;
		*(unsigned int *)(p + 0x5c) = 0;
		*(unsigned int *)(p + 0x60) = 0;
		*(unsigned int *)(p + 0x68) = 0;
		*(unsigned int *)(p + 0x08) = 0;
		*(unsigned int *)(p + 0x0c) = 0;
	}

	/* +0xb478..+0x23d38: CSTGSlotState[16], a clean array at the class's
	 * own confirmed 0x188c-byte size. */
	for (int i = 0; i < 16; i++)
		new (&slotStates[i]) CSTGSlotState();

	/* +0x23d38..+0x3a7b8 (_unrecovered_bigArray) and +0x3a7b8..+0x44ea8
	 * (_unrecovered_tail) are both confirmed to exist -- the former a
	 * 400x232-byte array whose per-element contents weren't fully traced
	 * (including 5 copies of CSTGMultisampleBankUUIDBase::
	 * sLegacyBankPrefix per element), the latter a nested nested loop of
	 * CModelVoiceRequirementsData::Clear() calls -- neither reconstructed
	 * in this pass (see oa_engine.h's class comment). */

	/* +0x44ea8: a real recursive pthread mutex, same allocate/init shape
	 * as CPowerOffTimer's already-confirmed mutex but with an explicit
	 * recursive attribute this time (mutexattr_init -> settype(recursive)
	 * -> pass the attr into mutex_init -> mutexattr_destroy). */
	unsigned char attrStorage[64];	/* real pthread_mutexattr_t size not
					 * independently confirmed; generously
					 * sized since its contents are opaque
					 * to us and it never persists past
					 * this constructor. */
	rtwrap_pthread_mutexattr_init(attrStorage);
	int recursiveType = get_pthread_recursive_attr_constant();
	rtwrap_pthread_mutexattr_settype(attrStorage, recursiveType);
	unsigned int mutexSize = get_sizeof_rtwrap_pthread_mutex();
	void *mutex = rtwrap_malloc(mutexSize);
	requirementsMutex = (unsigned int)(unsigned long)mutex;
	rtwrap_pthread_mutex_init(mutex, attrStorage);
	rtwrap_pthread_mutexattr_destroy(attrStorage);

	CSTGVoiceAllocator::sInstance = this;
}

/* Real kernel-side RTAI mutex teardown wrappers, confirmed via
 * relocation -- same treatment as the other rtwrap_* externs in this
 * file (and the identical pair already declared in engine.cpp for
 * CPowerOffTimer's own destructor). */
extern "C" void rtwrap_pthread_mutex_destroy(void *mutex);
extern "C" void rtwrap_free(void *ptr);

/*
 * ~CSTGVoiceAllocator() (sec 10.147): see oa_engine.h for the full
 * confirmed shape (unconditional `sInstance = 0`, then
 * destroy+free of the ctor's own `requirementsMutex`).
 */
CSTGVoiceAllocator::~CSTGVoiceAllocator()
{
	CSTGVoiceAllocator::sInstance = 0;
	void *mutex = (void *)(unsigned long)requirementsMutex;
	rtwrap_pthread_mutex_destroy(mutex);
	rtwrap_free(mutex);
}

/* Real kernel-side RTAI mutex lock/unlock, confirmed via relocation --
 * same rtwrap_* family as the destroy/free pair above. */
extern "C" void rtwrap_pthread_mutex_lock(void *mutex);
extern "C" void rtwrap_pthread_mutex_unlock(void *mutex);

/*
 * The confirmed real node shape EmergencyFreeVoiceList (and its own
 * not-yet-reconstructed sibling StealVoiceList) walks: `next` at +0x0,
 * `payload` (a `CSTGVoice*`) at +0x8. Declared with a native `next`
 * pointer (not a packed 32-bit int) deliberately -- on BOTH the real
 * 32-bit target and this project's 64-bit host verify build, a plain
 * pointer-typed `next` member occupies enough room that `payload`
 * still lands at +0x8, so there is no host/target width hazard here
 * (unlike other packed-pointer fields elsewhere in this project, e.g.
 * CSTGProgramSlot's family, sec 10.143) -- the two field offsets simply
 * coincide regardless of pointer width. Nothing else in this project
 * reads this exact node layout yet (only this function and its still-
 * deferred StealVoiceList sibling ever will), so there's no cross-
 * function byte-layout dependency to preserve either.
 */
struct STGVoiceListNode {
	STGVoiceListNode *next;	/* +0x0 */
	CSTGVoice *payload;	/* +0x8 */
};

/*
 * CSTGVoiceAllocator::EmergencyFreeVoiceList(TLinkedList<TListLink<
 * CSTGVoice>,CSTGVoice>*) (sec 10.149, `.text+0x53de0`, 84 bytes)
 * confirmed: lock `requirementsMutex`, walk the list rooted at `*list`
 * (a plain node-pointer head, NOT itself a `TLinkedList` object --
 * `list` is the confirmed real caller-supplied ADDRESS of just the head
 * field, e.g. `CSTGSlotVoiceData::EmergencyFreeAllVoices`'s own
 * `this+0x44`/`this+0x50`), calling `FreeVoice()` on each node's
 * payload (advancing to `next` BEFORE the call, confirmed via the real
 * disassembly's own instruction order -- safe even if `FreeVoice`
 * itself frees the node), then UNCONDITIONALLY calling
 * `DoPendingMoveVoices()` once (even if the list was empty), then
 * unlock.
 */
void CSTGVoiceAllocator::EmergencyFreeVoiceList(void *list)
{
	void *mutex = (void *)(unsigned long)requirementsMutex;
	rtwrap_pthread_mutex_lock(mutex);

	STGVoiceListNode *node = *(STGVoiceListNode **)list;
	while (node) {
		STGVoiceListNode *next = node->next;
		FreeVoice(node->payload);
		node = next;
	}

	DoPendingMoveVoices();
	rtwrap_pthread_mutex_unlock(mutex);
}

CSTGAudioManager::CSTGAudioManager()
{
	/* +0xa48..+0xa5c (target-relative, i.e. right after the vtable
	 * pointer): two complete mutex+condvar pairs. Confirmed real
	 * allocate/init shape, same as CPowerOffTimer's/CSTGVoiceAllocator's
	 * mutexes, plus a matching condvar pair each time. */
	mutexCondFlag1 = 0;
	unsigned int mutex1Size = get_sizeof_rtwrap_pthread_mutex();
	void *mutex1 = rtwrap_malloc(mutex1Size);
	mutex1Handle = (unsigned int)(unsigned long)mutex1;
	unsigned int cond1Size = get_sizeof_rtwrap_pthread_cond();
	void *cond1 = rtwrap_malloc(cond1Size);
	cond1Handle = (unsigned int)(unsigned long)cond1;
	rtwrap_pthread_mutex_init(mutex1, nullptr);
	rtwrap_pthread_cond_init(cond1, nullptr);
	mutexCondFlag2 = 0;

	unsigned int mutex2Size = get_sizeof_rtwrap_pthread_mutex();
	void *mutex2 = rtwrap_malloc(mutex2Size);
	mutex2Handle = (unsigned int)(unsigned long)mutex2;
	unsigned int cond2Size = get_sizeof_rtwrap_pthread_cond();
	void *cond2 = rtwrap_malloc(cond2Size);
	cond2Handle = (unsigned int)(unsigned long)cond2;
	rtwrap_pthread_mutex_init(mutex2, nullptr);
	rtwrap_pthread_cond_init(cond2, nullptr);

	/* +0xa60..+0x454c: confirmed to exist (a CPU-core-count-dependent
	 * branch/array, and 13 CProfiler+CDurationStats sub-object slots,
	 * 3 of them also with a CSTGFrontPanelStatusReporter, linked into
	 * CProfiler::sListOfProfilers) but NOT reconstructed in this pass --
	 * see the class comment in oa_engine.h for exactly what's confirmed
	 * there and why it wasn't modeled here. */

	trailingCount       = 0x100;
	trailingMask        = 0xff;
	trailingReciprocal  = 0.00390625f;
	trailingUnity       = 1.0f;

	CSTGAudioManager::sInstance = this;
}

CSTGMessageProcessor::CSTGMessageProcessor()
{
	/* +0x00..+0x1040 (confirmed minimum, likely a bit more -- see class
	 * comment): three unsolicited-message sender/message pairs
	 * (ProgramSlot/ControllerInfo/IFX), each embedding a 32-element
	 * CSTGDelayedMsg queue. Not reconstructed in this pass -- would need
	 * 7 new opaque vtabled sub-classes for comparatively little
	 * additional confirmed value, since none of their own internal
	 * fields beyond "has a vtable" and "embeds an array" were traced. */

	/* CSTGMessageProcessor::sInstance is confirmed to be set HERE, right
	 * after the three unsol-msg pairs above and BEFORE everything below
	 * -- a genuine, confirmed exception to every other manager in this
	 * file (where sInstance is set last). Reproduced at exactly this
	 * point, not just "somewhere in the constructor", since later code
	 * (the effector/algorithm registrations) may reasonably depend on
	 * the singleton already being valid. */
	CSTGMessageProcessor::sInstance = this;

	/* Confirmed to exist below this point, NOT reconstructed in this pass:
	 *   - +0x64: a real CEffectorDatabase*, heap-`new`'d here (confirmed
	 *     via _Znwj + CEffectorDatabase::CEffectorDatabase(int,
	 *     CEffector*), seeded with the global placeholder g_oNoEffect).
	 *   - ~15 CSTGBankMemory::AllocAligned()-backed buffers.
	 *   - 14 distinct CSTGXxxMsgHandler sub-object constructions (one per
	 *     message category this processor dispatches to), plus
	 *     CSTGNullMsgHandler (which sets its own separate sInstance).
	 *   - 198 CEffectorDatabase::Register(int, CEffector*, char const*)
	 *     calls and 8 CMOSSAlgorithmDatabase::Register(CMOSSAlgorithm
	 *     const*) calls -- each CEffector individually heap-`new`'d and
	 *     registered into the EXTERNAL database at +0x64, not embedded in
	 *     `this` (confirmed: no [ebx+CONST]-style access past the
	 *     unsol-msg-pairs region appears anywhere in the rest of the
	 *     function). This is why the constructor is 5930 bytes of code
	 *     despite this object's own confirmed footprint being
	 *     comparatively modest.
	 * See oa_engine.h's class comment for the full ground-truthing.
	 */
}

/*
 * ~CSTGMessageProcessor() (sec 10.147): see oa_engine.h's class comment
 * (just above CSTGMessageProcessor's own declaration) for the full
 * confirmed shape and the CEffectorDatabase forward-declaration note.
 */
CSTGMessageProcessor::~CSTGMessageProcessor()
{
	CSTGMessageProcessor::sInstance = 0;

	/* Both fields are packed 32-bit pointers on the real 32-bit target
	 * (this class's own storage is a plain byte array, not native
	 * pointer members) -- read as `unsigned int` and round-tripped
	 * through `(unsigned long)`, the same host/target-width-safe idiom
	 * established sec 10.142/10.143, not a native `void**`/`T**` load
	 * (which would read 8 bytes on this project's 64-bit host verify
	 * build instead of the real target's 4). */
	unsigned char *p = (unsigned char *)this;
	CEffectorDatabase *effectorDb =
		(CEffectorDatabase *)(unsigned long)*(unsigned int *)(p + 0x64);
	if (effectorDb) {
		effectorDb->~CEffectorDatabase();
		operator delete(effectorDb);
	}

	void *tail = (void *)(unsigned long)*(unsigned int *)(p + 0x68);
	operator delete(tail);
}

/*
 * CEffectorDatabase::~CEffectorDatabase() (`.text+0x3d5ff0`, 21 bytes,
 * sec 10.148): the destructor `~CSTGMessageProcessor()` above already
 * calls directly (non-virtually -- confirmed via the real disassembly's
 * own plain `call`, no vtable load). Confirmed real: `delete[]` a single
 * confirmed pointer field at `+0x0` if non-null (`operator delete[]
 * (nullptr)` is a standard-mandated no-op regardless, but the real
 * disassembly does perform the null check first, not unconditionally --
 * preserved as found). This class's own constructor/`Register()`/etc.
 * are NOT reconstructed in this pass -- `+0x0`'s own element type isn't
 * independently confirmed beyond "some `new[]`-allocated array".
 */
CEffectorDatabase::~CEffectorDatabase()
{
	void *arr = (void *)(unsigned long)*(unsigned int *)this;
	if (arr)
		operator delete[](arr);
}

/*
 * A batch of small `Initialize()`/`ProcessCommands()` bodies for classes
 * whose constructors already live in this file (sec 10.144, 2026-07-04),
 * picked directly from the unresolved-symbol/stub sweep. Every allocation
 * call target confirmed via its own `.rel.text` relocation against
 * `CSTGBankMemory::AllocAligned(unsigned int, unsigned int)` (`.text+0x232e0`,
 * already implemented, `src/mem/bank_memory.cpp`).
 */

/*
 * CSTGVoice::CSTGVoice(unsigned short) (sec 10.157, `.text+0x5bff0`, 375
 * bytes) -- see oa_engine.h's own class comment for the full confirmed
 * field list. Vtable-install only (slots never dispatched within this
 * ctor or, per the sec 10.153/10.154 checks already run on this whole
 * subsystem, anywhere else currently linked), so the zero-filled
 * `_ZTV9CSTGVoice` placeholder below is safe.
 */
unsigned char _ZTV9CSTGVoice[20];

CSTGVoice::CSTGVoice(unsigned short note)
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned short *)(base + 0x04) = note;
	*(unsigned int *)(base + 0x00) = ToU32(_ZTV9CSTGVoice + 8);

	*(unsigned char *)(base + 0x3d) = 0;
	*(unsigned char *)(base + 0x40) = 0;
	*(unsigned int *)(base + 0x5c) = 0x1105;

	*(unsigned int *)(base + 0x98) = ToU32(base);
	*(unsigned int *)(base + 0x90) = 0;
	*(unsigned int *)(base + 0x94) = 0;
	*(unsigned int *)(base + 0x9c) = 0;

	*(unsigned int *)(base + 0xa8) = ToU32(base);
	*(unsigned int *)(base + 0xa0) = 0;
	*(unsigned int *)(base + 0xa4) = 0;
	*(unsigned int *)(base + 0xac) = 0;

	*(unsigned int *)(base + 0xb8) = ToU32(base);
	*(unsigned int *)(base + 0xb0) = 0;
	*(unsigned int *)(base + 0xb4) = 0;
	*(unsigned int *)(base + 0xbc) = 0;

	*(unsigned int *)(base + 0xc8) = ToU32(base);
	*(unsigned int *)(base + 0xc0) = 0;
	*(unsigned int *)(base + 0xc4) = 0;
	*(unsigned int *)(base + 0xcc) = 0;

	*(unsigned int *)(base + 0xd8) = ToU32(base);
	*(unsigned int *)(base + 0xd0) = 0;
	*(unsigned int *)(base + 0xd4) = 0;
	*(unsigned int *)(base + 0xdc) = 0;

	*(unsigned int *)(base + 0xe8) = ToU32(base);
	*(unsigned int *)(base + 0xe0) = 0;
	*(unsigned int *)(base + 0xe4) = 0;
	*(unsigned int *)(base + 0xec) = 0;

	*(unsigned int *)(base + 0x08) = 0;
	*(unsigned int *)(base + 0x0c) = 0;
	*(unsigned int *)(base + 0x10) = 0;
	*(unsigned int *)(base + 0x38) = 0;
	*(unsigned char *)(base + 0x59) = 0;

	/* Six packed 32-bit pointers into this same object -- real target
	 * pointers, ToU32 per this project's established convention. The
	 * targets themselves (+0x18/+0x1c/+0x20/+0x24/+0x28/+0x2c) are a
	 * confirmed real gap: never written by this ctor. */
	*(unsigned int *)(base + 0x68) = ToU32(base + 0x2c);
	*(unsigned int *)(base + 0x6c) = ToU32(base + 0x20);
	*(unsigned int *)(base + 0x70) = ToU32(base + 0x24);
	*(unsigned int *)(base + 0x74) = ToU32(base + 0x28);
	*(unsigned int *)(base + 0x80) = ToU32(base + 0x18);
	*(unsigned int *)(base + 0x84) = ToU32(base + 0x1c);

	*(unsigned int *)(base + 0x54) = 0;
	*(unsigned int *)(base + 0x30) = 0;
	*(unsigned int *)(base + 0x34) = 0;
	*(unsigned int *)(base + 0x60) = 0;

	*(unsigned char *)(base + 0x3c) = 0x20;
	*(unsigned char *)(base + 0x3e) = 0x20;
	*(unsigned char *)(base + 0x3f) = 0x20;
	*(unsigned char *)(base + 0x41) = 0x20;

	*(unsigned int *)(base + 0x44) = 0;
	*(unsigned int *)(base + 0x48) = 0;
	*(unsigned int *)(base + 0x4c) = 0;
	*(unsigned char *)(base + 0x50) = 0;
}

/*
 * CSTGVoiceAllocator::Initialize() (sec 10.157, `.text+0x4c920`, 719
 * bytes). Builds THREE confirmed doubly-linked "insert at tail" free
 * lists, threading `next`/`prev` pointer pairs into records the ctor
 * (sec 10.147) already zeroed at those exact offsets (see oa_engine.h's
 * own class comment) -- confirmed via independent cross-check: the
 * ctor's own already-documented field list for `selfRefNodes`/
 * `ownerBackRefRecords` zeroes precisely the fields this function goes
 * on to thread (+0x0/+0x4/+0x8 and +0x4c/+0x50/+0x58 respectively), a
 * strong independent signal this decode is right, not guessed.
 *
 * Each list shares the same shape: a head/tail/count triplet living in
 * `_unrecovered_tail`, and a per-node `owner` back-pointer set to the
 * CONSTANT address of the list's own head field -- the same "owner
 * back-pointer to shared parent" idiom already confirmed for
 * CSTGSmoother's free list (sec 10.154) and CSTGFrontPanelSmoothers
 * (sec 10.153).
 */
static void STGVoiceAllocAppendList(unsigned char *base, unsigned int headOff, unsigned int tailOff,
	unsigned int nextOff, unsigned int prevOff,
	unsigned int ownerOff, unsigned char *node, unsigned int *tail,
	unsigned int *count)
{
	unsigned int nodeAddr = ToU32(node);
	unsigned int headAddr = ToU32(base + headOff);

	if (*tail != 0) {
		unsigned char *tailNode = FromU32(*tail);
		unsigned int tailNext = *(unsigned int *)(tailNode + nextOff);
		*(unsigned int *)(node + prevOff) = *tail;
		*(unsigned int *)(node + nextOff) = tailNext;
		if (tailNext != 0) {
			unsigned char *tn = FromU32(tailNext);
			*(unsigned int *)(tn + prevOff) = nodeAddr;
		}
		*(unsigned int *)(tailNode + nextOff) = nodeAddr;
	} else {
		*(unsigned int *)(base + headOff) = nodeAddr;
	}
	*(unsigned int *)(base + tailOff) = nodeAddr;
	*(unsigned int *)(node + ownerOff) = headAddr;
	(*count)++;
	*tail = nodeAddr;
}

void CSTGVoiceAllocator::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int i;

	*(unsigned int *)(base + 0x44b3c) = 0;
	*(unsigned char *)(base + 0x44b40) = 0;
	*(unsigned short *)(base + 0x44ea4) = 0;
	*(unsigned int *)(base + 0x44b38) = 0;
	*(unsigned int *)(base + 0x44b0c) = 0;
	*(unsigned char *)(base + 0x44b0a) = 0;
	*(unsigned char *)(base + 0x44b08) = 0;
	*(unsigned char *)(base + 0x44b09) = 0;

	/* ---- list1: ownerBackRefRecords[400], stride 0x6c ---- */
	{
		unsigned int tail = *(unsigned int *)(base + 0x3a7d4);
		unsigned int count = *(unsigned int *)(base + 0x3a7d8);
		for (i = 0; i < 0x190; i++) {
			unsigned char *node = base + 0xbb8 + i * 0x6c;
			*(unsigned short *)(node + 0x0) = (unsigned short)i;
			STGVoiceAllocAppendList(base, 0x3a7d0, 0x3a7d4,
						 0x4c, 0x50, 0x58, node, &tail, &count);
		}
		*(unsigned int *)(base + 0x3a7d8) = count;
	}

	/* ---- list2: _unrecovered_bigArray[400], stride 0xe8 ---- */
	{
		unsigned int tail = *(unsigned int *)(base + 0x3a7c8);
		unsigned int count = *(unsigned int *)(base + 0x3a7cc);
		for (i = 0; i < 0x190; i++) {
			unsigned char *node = base + 0x23d38 + i * 0xe8;
			*(unsigned short *)(node + 0x0) = (unsigned short)i;
			STGVoiceAllocAppendList(base, 0x3a7c4, 0x3a7c8,
						 0xd8, 0xdc, 0xe0, node, &tail, &count);
		}
		*(unsigned int *)(base + 0x3a7cc) = count;
	}

	/*
	 * ---- list3: selfRefNodes[50], stride 0x2c ----
	 * Each node's own +0xc/+0x10 pointer fields are set from TWO base
	 * addresses read fresh out of `CSTGVoiceModelManager::sInstance`
	 * every iteration (its own +0x0/+0x4 fields -- both real 32-bit
	 * target pointers, matching this project's already-opaque
	 * `CSTGVoiceModelManager::_unrecovered[92]` declaration), each
	 * offset by a running per-node byte accumulator (0x1a80 and 0x3300
	 * respectively) -- two distinct per-voice-model tables this project
	 * hasn't otherwise named.
	 */
	{
		unsigned int tail = *(unsigned int *)(base + 0x3a7bc);
		unsigned int count = *(unsigned int *)(base + 0x3a7c0);
		unsigned int accum1 = 0, accum2 = 0;
		unsigned char *mgr = (unsigned char *)CSTGVoiceModelManager::sInstance;
		for (i = 0; i < 0x32; i++) {
			unsigned char *node = base + i * 0x2c;
			unsigned int arrBBase = *(unsigned int *)(mgr + 0x0);
			unsigned int arrABase = *(unsigned int *)(mgr + 0x4);
			*(unsigned int *)(node + 0xc) = arrBBase + accum1;
			*(unsigned int *)(node + 0x10) = arrABase + accum2;
			*(unsigned short *)(node + 0x14) = (unsigned short)i;
			STGVoiceAllocAppendList(base, 0x3a7b8, 0x3a7bc,
						 0x0, 0x4, 0x8, node, &tail, &count);
			accum2 += 0x3300;
			accum1 += 0x1a80;
		}
		*(unsigned int *)(base + 0x3a7c0) = count;
	}

	/* 200 CSTGVoice objects, packed 32-bit pointers stored into
	 * voicePtrs[]. */
	for (i = 0; i < 200; i++) {
		void *mem = CSTGBankMemory::AllocAligned(0xf0, 0x10);
		new (mem) CSTGVoice((unsigned short)i);
		voicePtrs[i] = ToU32(mem);
	}
	/* Second pass: a separate 0x4000-byte buffer per voice, pointer
	 * stored at voice+0x60. */
	for (i = 0; i < 200; i++) {
		void *buf = CSTGBankMemory::AllocAligned(0x4000, 0x10);
		unsigned char *voice = FromU32(voicePtrs[i]);
		*(unsigned int *)(voice + 0x60) = ToU32(buf);
	}

	/* 16x128-word table at +0x40b54, companion 16-word row-flag array
	 * at +0x41b54. */
	for (unsigned int row = 0; row < 16; row++) {
		*(unsigned short *)(base + 0x41b54 + row * 2) = 0;
		unsigned char *rowBase = base + 0x40b54 + row * 0x100;
		for (unsigned int col = 0; col < 0x80; col++)
			*(unsigned short *)(rowBase + col * 2) = 0;
	}

	*(unsigned short *)(base + 0x44b10) = 0;
	*(unsigned short *)(base + 0x44b12) = 0;
	*(unsigned short *)(base + 0x44b14) = 0;
	*(unsigned short *)(base + 0x44b16) = 0;
	*(unsigned short *)(base + 0x44b18) = 0;
	*(unsigned short *)(base + 0x44b1a) = 0;
	*(unsigned short *)(base + 0x44b1c) = 0;
	*(unsigned short *)(base + 0x44b1e) = 0;
	*(unsigned short *)(base + 0x44b20) = 0;
	*(unsigned short *)(base + 0x44b22) = 0;
}

/*
 * CSTGMonitorMixer::Initialize() (`.text+0x69010`, 4 bytes) confirmed: a
 * single `AND [this],0xe0` -- clears the low 5 bits of `this->fieldAt(0)`,
 * leaving the top 3 bits untouched. Same "partial flag clear implies a
 * base/preceding sub-object already initialized" idiom already noted for
 * `CSTGMetronome`'s own constructor (sec 10.71's write-up).
 */
void CSTGMonitorMixer::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	base[0] = (unsigned char)(base[0] & 0xe0);
}

/*
 * CSTGHDRFileWriter::Initialize() (`.text+0x11c310`, 46 bytes) and
 * CSTGSamplingDaemon::Initialize() (`.text+0x11c620`, 46 bytes) confirmed
 * BYTE-IDENTICAL except for `this`: `this->fieldAt(0xc) = 0x129`, then
 * `this->fieldAt(0) = AllocAligned(0x948, 0x10)`. A real, faithfully-
 * preserved twin -- not simplified into a shared helper, matching this
 * project's own established precedent of reproducing genuine compiled
 * duplication rather than "cleaning it up" (e.g. sec 10.61's LFO/StepSeq
 * `InitializeQuad()` pair).
 */
void CSTGHDRFileWriter::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xc) = 0x129;
	*(unsigned int *)base = ToU32(CSTGBankMemory::AllocAligned(0x948, 0x10));
}

void CSTGSamplingDaemon::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xc) = 0x129;
	*(unsigned int *)base = ToU32(CSTGBankMemory::AllocAligned(0x948, 0x10));
}

/*
 * CSTGFileCloser::Initialize() (`.text+0x1199e0`, 71 bytes) confirmed: TWO
 * independent, identically-shaped alloc blocks -- `fieldAt(0xc)=0x11fa`,
 * `fieldAt(0)=AllocAligned(0x8fd0,0x10)`, then `fieldAt(0x1c)=0x11fa`,
 * `fieldAt(0x10)=AllocAligned(0x8fd0,0x10)` -- two SEPARATE 0x8fd0-byte
 * buffers with matching size-tag fields, not one buffer used twice.
 */
void CSTGFileCloser::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xc) = 0x11fa;
	*(unsigned int *)base = ToU32(CSTGBankMemory::AllocAligned(0x8fd0, 0x10));
	*(unsigned int *)(base + 0x1c) = 0x11fa;
	*(unsigned int *)(base + 0x10) = ToU32(CSTGBankMemory::AllocAligned(0x8fd0, 0x10));
}

/*
 * CSTGCDWorker::Initialize() (`.text+0x11b2f0`, 61 bytes) confirmed:
 * `this->fieldAt(0x20) = CSTGCDWorker_InitializeBuffer(this)` (a genuine
 * plain-C-linkage function, NOT a class method -- confirmed via its own
 * unmangled `T CSTGCDWorker_InitializeBuffer` symbol, taking `this`
 * implicitly as its sole regparm argument and returning a value this
 * function stores verbatim), then `fieldAt(0x234)=0x81`, then
 * `fieldAt(0x228)=AllocAligned(0x408,0x10)`. `CSTGCDWorker_InitializeBuffer`
 * itself is real now too (sec 10.148, `.text+0x11b7a0`, 30 bytes):
 * `worker` is confirmed NEVER read (the real disassembly loads its own
 * literal size/flags immediates and never touches the incoming EAX --
 * kept as an unused parameter here only so this call site's own
 * `this`-in-EAX regparm slot matches the real ABI byte-for-byte, not
 * because the real function does anything with it). Real body:
 * `sSCSIReadBuffer = __kmalloc(0xa00, 0xd1)`, returned verbatim (the
 * caller above stores it into `fieldAt(0x20)`). `sSCSIReadBuffer` is a
 * genuine, already-named real symbol (confirmed via its own `.bss` local
 * -- `b sSCSIReadBuffer` -- at the real store instruction's relocation
 * target), not independently referenced by any other reconstructed
 * function in this pass. `0xd1` = `OA_GFP_KERNEL(0xd0) | __GFP_DMA(0x1)`
 * -- this era's kernel `__GFP_DMA` flag -- consistent with a CD-ROM SCSI
 * controller's own DMA-capable buffer requirement. Three real siblings
 * (`_CleanupBuffer`/`_OpenCD`/`_ReadCD`) confirmed via the symbol table
 * but not yet referenced from any reconstructed call site.
 */
static void *sSCSIReadBuffer;

extern "C" unsigned int CSTGCDWorker_InitializeBuffer(void *worker)
{
	(void)worker;
	sSCSIReadBuffer = __kmalloc(0xa00, OA_GFP_KERNEL | 0x1);
	return ToU32(sSCSIReadBuffer);
}

void CSTGCDWorker::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x20) = CSTGCDWorker_InitializeBuffer(this);
	*(unsigned int *)(base + 0x234) = 0x81;
	*(unsigned int *)(base + 0x228) = ToU32(CSTGBankMemory::AllocAligned(0x408, 0x10));
}

/*
 * CSTGCDWorker::ProcessCommands() (`.text+0x11b720`, 124 bytes, sec
 * 10.158): a single-producer/single-consumer ring-buffer consumer loop
 * over this worker's own command queue -- `+0x228` base (a 0x408-byte
 * `AllocAligned` buffer per Initialize() above), `+0x22c` producer index
 * (never written by this method -- some other, not-yet-reconstructed
 * producer side owns it), `+0x230` consumer index (advanced here),
 * `+0x234` capacity (confirmed `0x81` == 129 == 0x408/8, an exact match
 * for an 8-byte-per-entry ring). Each entry is 8 bytes: a type-tag byte at
 * +0x0, a size/count dword at +0x4. Only tag==0 is handled: it calls
 * `CSTGHDRCircularBuffer::IncrementAvailableReadBytes(size)` on
 * `CSTGHDRManager::sInstance`'s own embedded `CSTGHDRCircularBuffer`
 * sub-object (`CSTGHDRManager::sInstance + 0x189c8`, confirmed offset --
 * see that class's own header comment in oa_engine.h). Any OTHER tag
 * value is a real, faithfully-preserved no-op: the entry is simply
 * consumed (the consumer index still advances) with no further action --
 * this function itself never handles a second command type.
 *
 * `CSTGHDRManager::sInstance` is dereferenced here as a plain native
 * pointer (NOT a packed/ToU32'd field -- it's declared a real
 * `CSTGHDRManager*` in oa_engine.h and already dereferenced directly
 * elsewhere, e.g. `engine.cpp`'s `CSTGHDRManager::sInstance->
 * ProcessCommands()`), so no MAP_32BIT hazard for THAT pointer. The ring
 * buffer's own base pointer (`+0x228`), by contrast, IS a packed 32-bit
 * field (stored via ToU32() in Initialize() above) and IS reconstituted
 * and dereferenced here (`entry[0]`/`entry+4`) -- a real KAT exercising a
 * populated entry needs that specific buffer to be MAP_32BIT-backed.
 */
void CSTGCDWorker::ProcessCommands()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int producerIdx = *(unsigned int *)(base + 0x22c);
	unsigned int consumerIdx = *(unsigned int *)(base + 0x230);

	while (producerIdx != consumerIdx) {
		unsigned int capacity = *(unsigned int *)(base + 0x234);
		unsigned int nextIdx = (consumerIdx + 1) % capacity;
		unsigned char *entry = FromU32(*(unsigned int *)(base + 0x228)) + consumerIdx * 8;

		*(unsigned int *)(base + 0x230) = nextIdx;

		unsigned char tag = entry[0];
		unsigned int sizeParam = *(unsigned int *)(entry + 4);
		if (tag == 0) {
			unsigned char *hdrBase = (unsigned char *)CSTGHDRManager::sInstance;
			CSTGHDRCircularBuffer *circBuf =
				(CSTGHDRCircularBuffer *)(hdrBase + 0x189c8);
			circBuf->IncrementAvailableReadBytes(sizeParam);
		}

		producerIdx = *(unsigned int *)(base + 0x22c);
		consumerIdx = *(unsigned int *)(base + 0x230);
	}
}

/*
 * CSTGSamplingDaemon::ProcessCommands() (`.text+0x11c7a0`, 120 bytes, sec
 * 10.160): the SAME shape ring-buffer consumer loop as CSTGCDWorker::
 * ProcessCommands() above, but at this class's OWN field offsets --
 * `+0x0` base, `+0x4` producer index (never written here), `+0x8`
 * consumer index (advanced here), `+0xc` capacity (confirmed never
 * zeroed by this class's own ctor either, matching the same "Initialize()
 * sets it later" gap already documented for CSTGCDWorker/CSTGFileCloser).
 * Each entry is 8 bytes: a 16-bit type tag at +0x0, a dword payload
 * pointer at +0x4 (unlike CSTGCDWorker's sibling, the payload here is a
 * POINTER, not a size -- confirmed by the two handled tags below, both of
 * which use it as an address). CONFIRMED: no vtable dispatch and no
 * indirect calls anywhere in this function (fully verified via `call`
 * mnemonic search over its own disassembly) -- unlike most of this
 * class's sibling `ProcessCommands()` methods (CSTGFileCloser/
 * CSTGHDRFileReader/CSTGHDRFileWriter/CSTGStreamingFileReader), which all
 * dispatch through a not-yet-recovered vtable or pointer-to-member-
 * function table and remain deliberately stubbed (see bar2_stubs.cpp).
 *
 * tag==0: writes 3 into the payload object's own `+0xc` field (a
 * confirmed real byte-for-byte offset write into the opaque region of
 * whatever the payload points to -- most likely a `CSTGRecordBuffer*`,
 * see the tag==1 case below, but this project's own `CSTGRecordBuffer`
 * declaration only names its own `+0x3004`/`+0x3008` fields, sec 10.148,
 * so this write is left as a raw byte-offset store rather than adding an
 * unconfirmed named field), then pushes `{payload, 0}` (8-byte stride)
 * onto `CSTGFileCloser::sInstance`'s own FIRST embedded ring (the one at
 * `+0x0`/`+0x4`/`+0x8`/`+0xc`, confirmed via relocation to
 * `CSTGFileCloser::sInstance` -- a completely separate ring from the one
 * `CSTGFileCloser::ProcessCommands()` itself consumes at `+0x10`/`+0x14`/
 * `+0x18`/`+0x1c`; this is the producer side CSTGFileCloser's own
 * `ProcessCommands()` never advances the writer of, confirming these are
 * two independent, differently-shaped rings sharing one 32-byte object,
 * not one ring read two ways).
 *
 * tag==1: pushes the SAME payload pointer directly into
 * `TSTGArrayManager<CSTGRecordBuffer>::sInstance`'s own `bucketArray`
 * (stride 4, confirmed via `BuildArrayManager()`'s own established
 * fill pattern in engine_init.cpp -- returning a completed
 * `CSTGRecordBuffer` to the free-list array, the strongest evidence the
 * payload really is a `CSTGRecordBuffer*`).
 *
 * Any OTHER tag value is a real, faithfully-preserved no-op: the entry
 * is simply consumed (consumer index still advances) with no further
 * action, matching CSTGCDWorker::ProcessCommands()'s own established
 * "unhandled tag" quirk.
 */
void CSTGSamplingDaemon::ProcessCommands()
{
	unsigned char *base = (unsigned char *)this;
	unsigned int producerIdx = *(unsigned int *)(base + 0x4);
	unsigned int consumerIdx = *(unsigned int *)(base + 0x8);

	while (producerIdx != consumerIdx) {
		unsigned int capacity = *(unsigned int *)(base + 0xc);
		unsigned int nextIdx = (consumerIdx + 1) % capacity;
		unsigned char *entry = FromU32(*(unsigned int *)(base + 0x0)) + consumerIdx * 8;

		*(unsigned int *)(base + 0x8) = nextIdx;

		unsigned short tag = *(unsigned short *)(entry + 0);
		unsigned char *payload = FromU32(*(unsigned int *)(entry + 4));

		if (tag == 0) {
			*(unsigned int *)(payload + 0xc) = 3;

			unsigned char *fc = (unsigned char *)CSTGFileCloser::sInstance;
			unsigned int fcCursor = *(unsigned int *)(fc + 0x4);
			unsigned char *fcEntry = FromU32(*(unsigned int *)(fc + 0x0)) + fcCursor * 8;
			*(unsigned int *)(fcEntry + 0) = ToU32(payload);
			*(unsigned int *)(fcEntry + 4) = 0;
			*(unsigned int *)(fc + 0x4) = (fcCursor + 1) % *(unsigned int *)(fc + 0xc);
		} else if (tag == 1) {
			TSTGArrayManager<CSTGRecordBuffer> *mgr = TSTGArrayManager<CSTGRecordBuffer>::sInstance;
			((unsigned int *)FromU32(mgr->bucketArray))[mgr->writeCursor] = ToU32(payload);
			mgr->writeCursor = (mgr->writeCursor + 1) % mgr->modulus;
		}

		producerIdx = *(unsigned int *)(base + 0x4);
		consumerIdx = *(unsigned int *)(base + 0x8);
	}
}

/*
 * CSTGHDRFileReader::Initialize() (`.text+0x11ba90`, 148 bytes, sec
 * 10.151) confirmed: `fieldAt(0xc)=0xfa1`, `fieldAt(0)=AllocAligned
 * (0x7d08,0x10)`, `fieldAt(0x10)=AllocAligned(0x8000,0x10)`, then zeroes
 * 12 dwords from `+0x14` through `+0x40` inclusive -- matching this
 * class's own confirmed `_unrecovered[68]` (`0x44`) total size exactly.
 */
void CSTGHDRFileReader::Initialize()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xc) = 0xfa1;
	*(unsigned int *)(base + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0x7d08, 0x10));
	*(unsigned int *)(base + 0x10) = ToU32(CSTGBankMemory::AllocAligned(0x8000, 0x10));
	*(unsigned int *)(base + 0x14) = 0;
	*(unsigned int *)(base + 0x18) = 0;
	*(unsigned int *)(base + 0x1c) = 0;
	*(unsigned int *)(base + 0x20) = 0;
	*(unsigned int *)(base + 0x24) = 0;
	*(unsigned int *)(base + 0x28) = 0;
	*(unsigned int *)(base + 0x2c) = 0;
	*(unsigned int *)(base + 0x30) = 0;
	*(unsigned int *)(base + 0x34) = 0;
	*(unsigned int *)(base + 0x38) = 0;
	*(unsigned int *)(base + 0x3c) = 0;
	*(unsigned int *)(base + 0x40) = 0;
}

/*
 * CSTGStreamingFileReader::Initialize(unsigned long) (`.text+0x11aa10`,
 * 120 bytes, sec 10.151) confirmed: `fieldAt(0x10)=0x10000`,
 * `fieldAt(0x14)=0x8000`, `fieldAt(0x18)=AllocAligned(0x10000,0x10)`,
 * `fieldAt(0xc)=0x192`, `fieldAt(0)=AllocAligned(0x12d8,0x10)`, then
 * zeroes 6 dwords from `+0x20` through `+0x34` inclusive -- matching
 * this class's own confirmed `_unrecovered[56]` (`0x38`) total size
 * exactly. **A real, faithfully-preserved quirk**: the incoming
 * `unsigned long` parameter (this project's own header comment
 * previously assumed, before ever disassembling the real body, that the
 * confirmed real call-site value `0x8000` was passed straight through)
 * is confirmed NEVER READ -- the real disassembly's very first
 * instruction overwrites the incoming EDX with the literal `0x10`
 * before anything else runs. Same class of "confirmed real, but the
 * parameter is dead" quirk already established for
 * `CSTGCDWorker_InitializeBuffer` (sec 10.148).
 */
void CSTGStreamingFileReader::Initialize(unsigned long /* confirmed real: never read */)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x10) = 0x10000;
	*(unsigned int *)(base + 0x14) = 0x8000;
	*(unsigned int *)(base + 0x18) = ToU32(CSTGBankMemory::AllocAligned(0x10000, 0x10));
	*(unsigned int *)(base + 0xc) = 0x192;
	*(unsigned int *)(base + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0x12d8, 0x10));
	*(unsigned int *)(base + 0x20) = 0;
	*(unsigned int *)(base + 0x24) = 0;
	*(unsigned int *)(base + 0x28) = 0;
	*(unsigned int *)(base + 0x2c) = 0;
	*(unsigned int *)(base + 0x30) = 0;
	*(unsigned int *)(base + 0x34) = 0;
}

/*
 * CSTGHDRManager::ProcessCommands() (`.text+0xd5dd0`, 41 bytes) confirmed:
 * calls three confirmed-real siblings on `this`, in this exact order --
 * `ProcessPlaybackCommands()` (`.text+0xd5950`), `ProcessRecordCommands()`
 * (`.text+0xd5b20`), `ProcessSamplerCommands()` (`.text+0xd5c50`) -- none
 * of which are reconstructed in this pass (each dispatches through
 * further not-yet-recovered state); declared here as confirmed-real,
 * deliberately deferred siblings so this dispatcher itself can be faithful
 * without overclaiming the sub-bodies.
 */
void CSTGHDRManager::ProcessCommands()
{
	ProcessPlaybackCommands();
	ProcessRecordCommands();
	ProcessSamplerCommands();
}

/*
 * CSTGPerformance::IsCurrentlyActive() const (`.text+0xb9ad0`, 42 bytes)
 * confirmed: resolves the active `CSTGPerformanceVarsManager` via the SAME
 * shared `ResolveActivePerformanceVarsManagerRaw()` helper used throughout
 * this codebase (sec 10.71/10.80); null manager -> false; else checks its
 * own `fieldAt(0x23d1) == 2` (the SAME "active state" gate value already
 * confirmed everywhere else in this cluster); only if that holds, compares
 * its own `fieldAt(0x23d4)` (a packed 32-bit pointer, host/target width
 * handled the same way as `CSTGProgramSlot::IsActive()`, sec 10.142/10.143)
 * against `this` -- i.e. "is THIS the performance currently recorded as
 * active on the active manager." This independently confirms
 * `CSTGPerformanceVarsManager+0x23d4` holds a `CSTGPerformance*` (a
 * DIFFERENT object's own `+0x23d4`, reached via a different pointer chain,
 * is explicitly NOT the same field -- see `UpdateDModRoutings`'s own
 * disclaimer, sec 10.135).
 */
bool CSTGPerformance::IsCurrentlyActive() const
{
	unsigned char *mgr = ResolveActivePerformanceVarsManagerRaw();
	if (!mgr)
		return false;
	if (mgr[0x23d1] != 2)
		return false;
	unsigned int stored = *(unsigned int *)(mgr + 0x23d4);
	return stored == ToU32((void *)this);
}
