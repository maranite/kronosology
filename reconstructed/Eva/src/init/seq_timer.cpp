/*
 * seq_timer.cpp  -  see include/seq_timer.h.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CSeqTimer_08e892a8 (Eva, VA 0x08e892a8): 7 dwords, exactly {~CSeqTimer,
 * ~CSeqTimer(deleting), CSeqTimer::Setup, CSeqTimer::Config, CSeqTimer::Start,
 * CModule::Destroy, CModule::GetErrorMsg} -- same clean 7-slot CModule-shaped
 * match as every other sibling in this batch.
 */

#include "seq_timer.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

/* SeqApi -- real global (mains.cpp), set by ConstructSeqApiInstance(). */
extern void *SeqApi;

CSeqTimer::CSeqTimer(const char *name)
	: CModule(name), mEngine(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CSeqTimer_08e892a8;
}

void CSeqTimer::Setup()
{
	void *raw = malloc(0x128);
	mEngine = new (raw) CTimerEngine(*this);
	Add(mEngine);

	typedef void (*NotifyEngineFn)(void *, CTimerEngine *);
	NotifyEngineFn fn = (NotifyEngineFn)(((void **)*(void **)SeqApi)[8]);
	fn(SeqApi, mEngine);
}

void CSeqTimer::Config()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CSeqTimer::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

extern "C" void CSeqTimerSetupVSlot(void *obj)
{
	static_cast<CSeqTimer *>(obj)->Setup();
}

extern "C" void CSeqTimerConfigVSlot(void *obj)
{
	static_cast<CSeqTimer *>(obj)->Config();
}

extern "C" void CSeqTimerStartVSlot(void *obj)
{
	static_cast<CSeqTimer *>(obj)->Start();
}

/* Real vtable definition -- moved here from omega_vtables.cpp, same "define
 * locally where the real forwarders live" precedent as this batch's other 3
 * siblings.
 */
void *PTR__CSeqTimer_08e892a8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CSeqTimerSetupVSlot, (void *)CSeqTimerConfigVSlot, (void *)CSeqTimerStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
