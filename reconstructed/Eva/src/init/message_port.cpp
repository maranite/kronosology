/*
 * message_port.cpp  -  see include/message_port.h.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CMessagePort_08e88468 (Eva, VA 0x08e88468): 13 dwords, exactly
 * {~CMessagePort, ~CMessagePort(deleting), CMessagePort::Setup,
 * CMessagePort::Config, CMessagePort::Start, CModule::Destroy,
 * CModule::GetErrorMsg, CMessagePort::AddView(COutLink*),
 * CMessagePort::AddView(CTask*), CMessagePort::RemoveOutView,
 * CMessagePort::RemoveInView, CMessagePort::DisconnectPort,
 * CMessagePort::Dispatch} -- confirms the base CModule 7-slot shape (slots
 * 0-6, same convention as every other sibling in this batch) PLUS 6 further
 * real slots (7-12) this batch's own AddView/RemoveOutView/RemoveInView/
 * DisconnectPort/Dispatch survey found -- see file header for why those 6
 * stay Tier B.
 */

#include "message_port.h"
#include "omega_vtables.h"

CMessagePort::CMessagePort()
	: CModule("ViewBase" /* real: CViewBase::SysName, mains.cpp */),
	  mUnknown2c(0), mUnknown30(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CMessagePort_08e88468;
}

void CMessagePort::Setup()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CMessagePort::Config()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CMessagePort::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

extern "C" void CMessagePortSetupVSlot(void *obj)
{
	static_cast<CMessagePort *>(obj)->Setup();
}

extern "C" void CMessagePortConfigVSlot(void *obj)
{
	static_cast<CMessagePort *>(obj)->Config();
}

extern "C" void CMessagePortStartVSlot(void *obj)
{
	static_cast<CMessagePort *>(obj)->Start();
}

/* Real vtable definition -- moved here from omega_vtables.cpp, same "define
 * locally where the real forwarders live" precedent as this batch's other 3
 * siblings. Slots 7-12 (AddView x2/RemoveOutView/RemoveInView/DisconnectPort/
 * Dispatch) stay EvaVTableStub -- real methods confirmed to exist (see header
 * comment) but genuinely out of scope for this pass.
 */
void *PTR__CMessagePort_08e88468[13] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CMessagePortSetupVSlot, (void *)CMessagePortConfigVSlot, (void *)CMessagePortStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
