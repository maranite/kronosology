/*
 * dump_man_mod.cpp  -  see include/dump_man_mod.h.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CDumpManMod_08e85ca8 (Eva, VA 0x08e85ca8): 7 dwords, exactly
 * {~CDumpManMod, ~CDumpManMod(deleting), CDumpManMod::Setup, CDumpManMod::Config,
 * CDumpManMod::Start, CModule::Destroy, CModule::GetErrorMsg} -- a clean match against
 * module.h's own documented 7-slot CModule layout, cross-confirming both.
 */

#include "dump_man_mod.h"
#include "dump_task.h"
#include "buffering_task.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

CDumpManMod::CDumpManMod()
	: CModule("DumpManager")
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CDumpManMod_08e85ca8;
}

void CDumpManMod::Setup()
{
	void *raw1 = malloc(0x98);
	CDumpTask *dumpTask = new (raw1) CDumpTask(*this);
	/* Real soft NULL assert on dumpTask omitted. */

	void *raw2 = malloc(0xac);
	CBufferingTask *bufferingTask = new (raw2) CBufferingTask(*this);
	/* Real soft asserts (bufferingTask != 0, bufferingTask's own mDumpTask
	 * starts null, dumpTask != 0, dumpTask's own mBufferingTask starts null)
	 * all omitted, same convention as every other soft assert in this project.
	 */

	bufferingTask->LinkDumpTask(dumpTask);
	dumpTask->LinkBufferingTask(bufferingTask);

	Add(dumpTask);
	Add(bufferingTask);
}

void CDumpManMod::Config()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CDumpManMod::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

extern "C" void CDumpManModSetupVSlot(void *obj)
{
	static_cast<CDumpManMod *>(obj)->Setup();
}

extern "C" void CDumpManModConfigVSlot(void *obj)
{
	static_cast<CDumpManMod *>(obj)->Config();
}

extern "C" void CDumpManModStartVSlot(void *obj)
{
	static_cast<CDumpManMod *>(obj)->Start();
}

/* Real vtable definition (module.h/omega_vtables.h: 7 slots, dtor pair + Setup/
 * Config/Start + Destroy/GetErrorMsg) -- moved here from omega_vtables.cpp now that
 * slots 2/3/4 have real forwarders to wire in (same "define locally where the real
 * forwarders live" precedent as es_common.cpp's own PTR__CESCommon_08fbafc8). Slots
 * 0/1 (dtor pair) and 5/6 (Destroy/GetErrorMsg, confirmed by direct .rodata byte read
 * to be CModule's own unmodified implementations, not a CDumpManMod-specific
 * override) stay EvaVTableStub -- nothing on this pass's own traced boot path
 * destroys a CDumpManMod or calls Destroy()/GetErrorMsg() on one.
 */
void *PTR__CDumpManMod_08e85ca8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CDumpManModSetupVSlot, (void *)CDumpManModConfigVSlot, (void *)CDumpManModStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
