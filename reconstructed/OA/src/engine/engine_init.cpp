// SPDX-License-Identifier: GPL-2.0
/*
 * engine_init.cpp  -  CSTGEngine::Initialize() (.text+0x1b0, 1901 bytes,
 * the largest method this class has -- see oa_engine.h's own header
 * comment). Called from setup_global_resources (init_module step 8) once
 * CSTGGlobal's own construction is done.
 *
 * Ground-truthed via a full objdump -d -r -j .text disassembly (restricted
 * to the .text section specifically -- the naive form without `-j`
 * spuriously pulls in unrelated .init.text content at overlapping
 * relative addresses within the requested range, caught this pass).
 *
 * Structure, confirmed instruction-by-instruction: ~44 heap/bank-memory
 * allocations each immediately followed by a placement-new constructor
 * call (most singletons -- see each new class's own declaration in
 * oa_engine_init.h/oa_engine.h for per-class confirmed details), then a
 * batch of ~26 `Initialize()` calls on those same singletons in
 * construction order, three inlined ring-buffer-building loops
 * (TSTGArrayManager<CSTGPlaybackEvent/RecordEvent/RecordBuffer>), and
 * finally a conditional `InitCdromSupport()`-gated call to the
 * already-reconstructed `OA_ApplyCdromDegradation()` (sec 8).
 *
 * CORRECTS MASTER_REFERENCE.md sec 10.13's own earlier survey for the
 * ten "Model" class rows (32-41) -- see oa_engine_init.h's own header
 * comment for the full correction; this file's sizes are the
 * instruction-level ground truth.
 *
 * Four objects are real heap-`new`'d (`_Znwj` relocations confirmed,
 * matching sec 10.13's already-confirmed ctor/dtor cross-check):
 * CLoadBalancer, CSTGAudioDriverInterfaceKorgUsb, CSTGAudioManager,
 * CPowerOffTimer. Every other manager/model is CSTGBankMemory-placed.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"
#include "oa_bank_memory.h"
#include "cdrom_check.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

CSTGWaveSeqManager *CSTGWaveSeqManager::sInstance;
CSTGVectorManager *CSTGVectorManager::sInstance;
CSTGMidiDispatcher *CSTGMidiDispatcher::sInstance;
CSTGHDRMiniModel *CSTGHDRMiniModel::sInstance;
CSTGStreamingEventManager *CSTGStreamingEventManager::sInstance;
CSTGSmoother *CSTGSmoother::sInstance;

template <typename T> TSTGArrayManager<T> *TSTGArrayManager<T>::sInstance;

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/* Constructs `count` T objects (sizeof each `elemSize`, confirmed always
 * 16-aligned) into fresh CSTGBankMemory allocations, filling both of
 * TSTGArrayManager<T>'s confirmed arrays -- see oa_engine_init.h's own
 * header note on why these end up holding the same N pointers via two
 * different fill orders. `idOffset` is the confirmed per-element field
 * offset (relative to the allocated object, not fixed across T -- +0x4
 * for CSTGPlaybackEvent/CSTGRecordEvent, +0x0 for CSTGRecordBuffer) where
 * the element's own sequential position gets stamped as a 16-bit id. */
template <typename T>
static void BuildArrayManager(TSTGArrayManager<T> *mgr, unsigned int count,
			       unsigned int elemSize, unsigned int idOffset,
			       void (*construct)(unsigned char *))
{
	mgr->modulus = count + 1;
	mgr->bucketArray = ToU32(CSTGBankMemory::AllocAligned((count + 1) * 4, 0x10));
	mgr->indexArray = ToU32(CSTGBankMemory::AllocAligned(count * 4, 0x10));

	for (unsigned int i = 0; i < count; i++) {
		unsigned char *elem = CSTGBankMemory::AllocAligned(elemSize, 0x10);
		construct(elem);

		((unsigned int *)FromU32(mgr->indexArray))[i] = ToU32(elem);
		*(unsigned short *)(elem + idOffset) = (unsigned short)i;

		((unsigned int *)FromU32(mgr->bucketArray))[mgr->writeCursor] = ToU32(elem);
		mgr->writeCursor = (mgr->writeCursor + 1) % mgr->modulus;
	}
	mgr->count = count;
}

static void ConstructPlaybackEvent(unsigned char *p) { new (p) CSTGPlaybackEvent(); }

/* CSTGRecordEvent has no constructor symbol of its own -- see
 * oa_engine_init.h's own header note. Reproduces the confirmed inline
 * sequence directly: base ctor, then manual vtable-pointer patch. */
static void ConstructRecordEvent(unsigned char *p)
{
	new (p) CSTGAudioEvent();
	/* Confirmed real 32-bit vtable-pointer field -- unsigned int store,
	 * not a native host pointer, same established reasoning as
	 * TSTGArrayManager<T>'s own fields above. */
	*(unsigned int *)p = ToU32(_ZTV15CSTGRecordEvent + 8);
}

static void ConstructRecordBuffer(unsigned char *p) { new (p) CSTGRecordBuffer(); }

/* CSTGRecordBuffer::CSTGRecordBuffer() (`.text+0xd6dc0`, 21 bytes)
 * reconstructed real (sec 10.148): zeroes the two confirmed dwords at
 * +0x3004/+0x3008 -- see oa_engine_init.h's own header comment for the
 * full real-vs-previously-assumed-size correction this discovery forced
 * (the `BuildArrayManager` call below now passes the corrected real
 * `CSTGRECORDBUFFER_SIZE` (0x301c) stride, not the old, wrong 0x38). */
CSTGRecordBuffer::CSTGRecordBuffer()
{
	field3004 = 0;
	field3008 = 0;
}

/* Raw indirect vtable dispatch, matching CCostProfile's own established
 * treatment (oa_setup_global_resources.h) -- used for the two confirmed
 * calls whose target's exact vtable layout isn't independently pinned
 * down (CSTGAudioDriverInterface slot 2, CSTGAudioManager slot 0). */
static void CallVtableSlot(void *obj, int slotIndex)
{
	typedef void (*Fn)(void *);
	void **vtable = *(void ***)obj;
	Fn fn = (Fn)vtable[slotIndex];
	fn(obj);
}

void CSTGEngine::Initialize()
{
	CLoadBalancer *loadBalancer = new CLoadBalancer();

	CSTGDiskCostManager *diskCost =
		new (CSTGBankMemory::AllocAligned(0x48, 0x10)) CSTGDiskCostManager();

	CSTGAudioDriverInterfaceKorgUsb *audioDriver = new CSTGAudioDriverInterfaceKorgUsb();

	CSTGAudioManager *audioManager = new CSTGAudioManager();

	new (CSTGBankMemory::AllocAligned(0x44eac, 0x10)) CSTGVoiceAllocator();
	new (CSTGBankMemory::AllocAligned(0xb7c, 0x10)) CSTGEffectManager();
	new (CSTGBankMemory::AllocAligned(0xe134, 0x10)) CSTGWaveSeqManager();
	new (CSTGBankMemory::AllocAligned(0x1c9e8, 0x10)) CSTGVectorManager();
	new (CSTGBankMemory::AllocAligned(0xa8, 0x10)) CSTGMidiDispatcher();
	new (CSTGBankMemory::AllocAligned(0x1040, 0x10)) CSTGMessageProcessor();
	new (CSTGBankMemory::AllocAligned(0xcb0, 0x10)) CSTGFrontPanelSmoothers();
	new (CSTGBankMemory::AllocAligned(0x5c, 0x10)) CSTGVoiceModelManager();

	/* Three TSTGArrayManager<T> headers: 24 bytes each, 3 fields
	 * zeroed (+0x0/+0x4/+0x8 -- confirmed via the real disassembly's
	 * own 3 explicit zero-stores), sInstance set. modulus/indexArray/
	 * count get filled later by BuildArrayManager() below, matching
	 * the real code's own two-pass structure (headers allocated here,
	 * element arrays/loop run much further down). */
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance =
		(TSTGArrayManager<CSTGPlaybackEvent> *)CSTGBankMemory::AllocAligned(0x18, 0x10);
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->bucketArray = 0;
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->writeCursor = 0;
	TSTGArrayManager<CSTGPlaybackEvent>::sInstance->_unused8 = 0;

	TSTGArrayManager<CSTGRecordEvent>::sInstance =
		(TSTGArrayManager<CSTGRecordEvent> *)CSTGBankMemory::AllocAligned(0x18, 0x10);
	TSTGArrayManager<CSTGRecordEvent>::sInstance->bucketArray = 0;
	TSTGArrayManager<CSTGRecordEvent>::sInstance->writeCursor = 0;
	TSTGArrayManager<CSTGRecordEvent>::sInstance->_unused8 = 0;

	TSTGArrayManager<CSTGRecordBuffer>::sInstance =
		(TSTGArrayManager<CSTGRecordBuffer> *)CSTGBankMemory::AllocAligned(0x18, 0x10);
	TSTGArrayManager<CSTGRecordBuffer>::sInstance->bucketArray = 0;
	TSTGArrayManager<CSTGRecordBuffer>::sInstance->writeCursor = 0;
	TSTGArrayManager<CSTGRecordBuffer>::sInstance->_unused8 = 0;

	new (CSTGBankMemory::AllocAligned(0x18b10, 0x10)) CSTGHDRManager();
	new (CSTGBankMemory::AllocAligned(0xf60, 0x10)) CSTGHDRMiniModel();
	new (CSTGBankMemory::AllocAligned(0x402, 0x10)) CSTGMonitorMixer();
	new (CSTGBankMemory::AllocAligned(0x2c, 0x10)) CSTGMetronome();
	new (CSTGBankMemory::AllocAligned(0x3c, 0x10)) CSTGTempoUtils();
	new (CSTGBankMemory::AllocAligned(0x14c44, 0x10)) CSTGStreamingEventManager();
	new (CSTGBankMemory::AllocAligned(0x220, 0x10)) CSTGFileOpener();
	new (CSTGBankMemory::AllocAligned(0x20, 0x10)) CSTGFileCloser();
	new (CSTGBankMemory::AllocAligned(0x44, 0x10)) CSTGHDRFileReader();
	new (CSTGBankMemory::AllocAligned(0x38, 0x10)) CSTGStreamingFileReader();
	new (CSTGBankMemory::AllocAligned(0x14, 0x10)) CSTGHDRFileWriter();
	new (CSTGBankMemory::AllocAligned(0x238, 0x10)) CSTGCDWorker();
	new (CSTGBankMemory::AllocAligned(0x10, 0x10)) CSTGSamplingDaemon();
	new (CSTGBankMemory::AllocAligned(0x24, 0x10)) CSTGKLMManager();
	new (CSTGBankMemory::AllocAligned(0xf028, 0x10)) CSTGSmoother();

	CPowerOffTimer *powerOffTimer = new CPowerOffTimer();

	new (CSTGBankMemory::AllocAligned(0x1830, 0x10)) CSTGLFOTables();

	CSTGStreamingEventManager::sInstance->Initialize(0x191, 0x10000);
	loadBalancer->Initialize();
	diskCost->Initialize();
	CallVtableSlot(audioDriver, 2);
	CallVtableSlot(audioManager, 0);

	new (CSTGBankMemory::AllocAligned(0xcc, 0x10)) CSTGMIDIClockSync();

	/* CSTGMidiPortManager's own struct-init block: a real, confirmed
	 * 528-byte allocation (NOT part of any "Model" class -- see
	 * oa_engine_init.h's header note correcting sec 10.13's table) with
	 * explicit field writes at +0x0/+0x1/+0x2/+0x3 (bytes, zeroed),
	 * +0xc/+0x70/+0xd4 (dwords, 0xffffffff -- "unset" sentinels),
	 * +0x138/+0x13c (dwords, zeroed), +0x140/+0x1a4 (dwords,
	 * 0xffffffff), +0x208/+0x20c (dwords, zeroed). No constructor is
	 * called for it (confirmed: CSTGMidiPortManager has no C1/C2 symbol
	 * at all, see oa_engine.h's own header note on that class). */
	unsigned char *midiPortMgr = CSTGBankMemory::AllocAligned(0x210, 0x10);
	midiPortMgr[0x0] = 0;
	midiPortMgr[0x1] = 0;
	midiPortMgr[0x2] = 0;
	midiPortMgr[0x3] = 0;
	*(unsigned int *)(midiPortMgr + 0xc) = 0xffffffff;
	*(unsigned int *)(midiPortMgr + 0x70) = 0xffffffff;
	*(unsigned int *)(midiPortMgr + 0xd4) = 0xffffffff;
	*(unsigned int *)(midiPortMgr + 0x138) = 0;
	*(unsigned int *)(midiPortMgr + 0x13c) = 0;
	*(unsigned int *)(midiPortMgr + 0x140) = 0xffffffff;
	*(unsigned int *)(midiPortMgr + 0x1a4) = 0xffffffff;
	*(unsigned int *)(midiPortMgr + 0x208) = 0;
	*(unsigned int *)(midiPortMgr + 0x20c) = 0;
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgr;
	CSTGMidiPortManager::sInstance->Initialize();

	/* Ten "Model" classes, each: allocate, construct, then dispatch a
	 * real virtual call through the constructed object's own confirmed
	 * vtable slot 2 (raw indirect, matching CCostProfile's established
	 * treatment -- see oa_engine_init.h's own header note). */
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGOffModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGPCMModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGAnalogSyncModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGOrganModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGPluckedModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGMS20Model(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGPolysixModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x108, 0x10)) CSTGVPMModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x508, 0x10)) CSTGPianoModel(), 2);
	CallVtableSlot(new (CSTGBankMemory::AllocAligned(0x124, 0x10)) CSTGEPModel(), 2);

	CSTGCommonLFO::Initialize();
	CSTGCommonStepSeq::Initialize();

	CSTGEffectManager::sInstance->Initialize();
	CSTGWaveSeqManager::sInstance->Initialize();
	CSTGVectorManager::sInstance->Initialize();
	CSTGMidiDispatcher::sInstance->Initialize();
	CSTGVoiceAllocator::sInstance->Initialize();

	/* Three inlined ring-buffer-building loops (see BuildArrayManager()'s
	 * own header comment for the shared algorithm). Confirmed counts:
	 * 4000 CSTGPlaybackEvent (104 bytes each, id field at +0x4), 200
	 * CSTGRecordEvent (56 bytes each, id field at +0x4), 96
	 * CSTGRecordBuffer (CORRECTED sec 10.148: 0x301c/12316 bytes each,
	 * NOT 56 -- see oa_engine_init.h's own header comment; id field at
	 * +0x0). */
	BuildArrayManager(TSTGArrayManager<CSTGPlaybackEvent>::sInstance, 4000, 0x68, 0x4,
			   ConstructPlaybackEvent);
	BuildArrayManager(TSTGArrayManager<CSTGRecordEvent>::sInstance, 200, 0x38, 0x4,
			   ConstructRecordEvent);
	BuildArrayManager(TSTGArrayManager<CSTGRecordBuffer>::sInstance, 96, CSTGRECORDBUFFER_SIZE, 0x0,
			   ConstructRecordBuffer);

	CSTGHDRManager::sInstance->Initialize();
	CSTGFileOpener::sInstance->Initialize();
	CSTGFileCloser::sInstance->Initialize();
	CSTGHDRFileReader::sInstance->Initialize();
	CSTGStreamingFileReader::sInstance->Initialize(0x8000);
	CSTGHDRFileWriter::sInstance->Initialize();
	CSTGCDWorker::sInstance->Initialize();
	CSTGSamplingDaemon::sInstance->Initialize();
	CSTGHDRMiniModel::sInstance->Initialize();
	CSTGMonitorMixer::sInstance->Initialize();
	CSTGKLMManager::sInstance->Initialize();
	CSTGSmoother::sInstance->Initialize();
	powerOffTimer->Initialize();

	/* Confirmed conditional tail: on InitCdromSupport() failure
	 * (nonzero return -- see cdrom_check.h's own confirmed return-value
	 * convention), runs the already-reconstructed degradation block
	 * (sec 8) -- confirming that file's own note that
	 * OA_ApplyCdromDegradation() was "reproduced as a standalone helper
	 * for Stage 3 to call" was exactly right. */
	if (InitCdromSupport())
		OA_ApplyCdromDegradation();
}
