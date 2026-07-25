/*
 * panel_driver.cpp  -  see include/panel_driver.h.
 */

#include "panel_driver.h"
#include "omega_vtables.h"
#include "ustg_user_api.h"
#include "comm_driver.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

/* STGMessage's real layout is still Stage 2 (see ustg_user_api.h). All 5
 * USTGAPIFrontPanel wrappers below build the same 4-field header
 * lcd_control.cpp's STGMessageLocalShape already established, plus one trailing
 * `u32 param` field whenever the opcode needs one -- confirmed by reading all 5
 * real decompiles (ResetLED/SetLED/SetLEDBlinking always pass a plain 5-field
 * struct; SetLED16Bit packs both halves into the one trailing param; Beep omits
 * the trailing field entirely, sending just the 4-field header).
 */
namespace {

struct FrontPanelMsg4 {
	unsigned short type;    /* real local_1c, always 0x10 */
	unsigned short subtype; /* real local_1a, always 1 */
	unsigned int   field8;  /* real local_18 -- always 0xd ("front panel" class) here */
	unsigned int   field12; /* real local_14 -- the per-call opcode (0/1/2/3/4) */
};

struct FrontPanelMsg5 {
	FrontPanelMsg4 header;
	unsigned int   param;   /* real local_10 -- LED mask / combined 16-bit pair */
};

} // namespace

namespace USTGAPIFrontPanel {

/* .text+0x08e1d440, 59 bytes. */
void SetLED(unsigned int ledMask)
{
	FrontPanelMsg5 msg;
	msg.header.type = 0x10;
	msg.header.subtype = 1;
	msg.header.field8 = 0xd;
	msg.header.field12 = 0;
	msg.param = ledMask;
	USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
}

/* .text+0x08e1d480, 59 bytes. */
void SetLEDBlinking(unsigned int ledMask)
{
	FrontPanelMsg5 msg;
	msg.header.type = 0x10;
	msg.header.subtype = 1;
	msg.header.field8 = 0xd;
	msg.header.field12 = 1;
	msg.param = ledMask;
	USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
}

/* .text+0x08e1d4c0, 59 bytes. */
void ResetLED(unsigned int ledMask)
{
	FrontPanelMsg5 msg;
	msg.header.type = 0x10;
	msg.header.subtype = 1;
	msg.header.field8 = 0xd;
	msg.header.field12 = 2;
	msg.param = ledMask;
	USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
}

/* .text+0x08e1d500, 67 bytes. Real: packs both halves into one dword
 * (`param_2 << 0x10 | param_1`) before the header fields are even set -- order
 * preserved faithfully, has no observable effect since it's all one local struct.
 */
void SetLED16Bit(unsigned int ledMaskLo, unsigned short ledMaskHi)
{
	FrontPanelMsg5 msg;
	msg.param = ((unsigned int)ledMaskHi << 16) | ledMaskLo;
	msg.header.type = 0x10;
	msg.header.subtype = 1;
	msg.header.field8 = 0xd;
	msg.header.field12 = 3;
	USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
}

/* .text+0x08e1d550, 51 bytes. Real: only the 4-field header, no trailing param. */
void Beep()
{
	FrontPanelMsg4 msg;
	msg.type = 0x10;
	msg.subtype = 1;
	msg.field8 = 0xd;
	msg.field12 = 4;
	USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
}

} // namespace USTGAPIFrontPanel

namespace {
	void VDtor(CLinuxPanelDriver *self) { self->~CLinuxPanelDriver(); }
	int VOpen(CLinuxPanelDriver *self, void *arg) { return self->Open(arg); }
	int VClose(CLinuxPanelDriver *self, void *arg) { return self->Close(arg); }
	int VGetDriverClass(CLinuxPanelDriver *self) { return self->GetDriverClass(); }
	int VGetEvent(CLinuxPanelDriver *self, PanelDriverEvent *out) { return self->GetEvent(out); }
	void VPutEvent(CLinuxPanelDriver *self, PanelDriverEvent *evt) { self->PutEvent(*evt); }
	int VPutCommand(CLinuxPanelDriver *self, PanelDriverCommand *cmd) { return self->PutCommand(cmd); }
}

/* Real vtable, byte-exact (see panel_driver.h's own table, read directly off
 * .rodata+0x08fd9dc8).
 */
extern "C" void *PTR__CLinuxPanelDriver_08fd9dc8[8] = {
	(void *)&VDtor,           /* slot 0 (+0x00) ~CLinuxPanelDriver() complete */
	(void *)&VDtor,           /* slot 1 (+0x04) ~CLinuxPanelDriver() deleting */
	(void *)&VOpen,           /* slot 2 (+0x08) Open(void*) */
	(void *)&VClose,          /* slot 3 (+0x0c) Close(void*) */
	(void *)&VGetDriverClass, /* slot 4 (+0x10) GetDriverClass() */
	(void *)&VGetEvent,       /* slot 5 (+0x14) GetEvent(SEvent*) */
	(void *)&VPutEvent,       /* slot 6 (+0x18) PutEvent(SEvent&) */
	(void *)&VPutCommand,     /* slot 7 (+0x1c) PutCommand(SCommand*) */
};

CLinuxPanelDriver::CLinuxPanelDriver(const char *name)
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = new char[len + 1];
	mName = dup;
	strcpy(dup, name);

	mVtbl = PTR__CLinuxPanelDriver_08fd9dc8;

	USTGUserAPI::ConnectPanelFifo();
}

CLinuxPanelDriver::~CLinuxPanelDriver()
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	if (mName != 0)
		delete[] mName;
	mVtbl = (void *)PTR__CObjectBase_08e79d68;
	/* Deleting-dtor variant (08e50010) additionally calls operator delete(this) --
	 * not modeled, same reasoning as hid_driver.cpp's dtor.
	 */
}

int CLinuxPanelDriver::Open(void * /*arg*/)
{
	return 0;
}

int CLinuxPanelDriver::Close(void * /*arg*/)
{
	return 0;
}

int CLinuxPanelDriver::GetDriverClass()
{
	return 10;
}

bool CLinuxPanelDriver::GetEvent(PanelDriverEvent *out)
{
	/* Real: `iVar1 = CCommDriver::getInstance(); if (-1 < *(int*)(iVar1+0x10))
	 * read(*(int*)(iVar1+0x10), param_1, 8);` -- treats CCommDriver as an opaque
	 * blob and reads its mEventFd field directly by raw offset, same idiom
	 * module_manager.cpp uses for CModule's own fields. mEventFd is confirmed at
	 * +0x10 by comm_driver.h's own field layout (cross-checked, not just assumed
	 * to match this call site).
	 */
	CCommDriver *comm = CCommDriver::getInstance();
	int eventFd = *(int *)((char *)comm + 0x10);
	if (eventFd < 0)
		return false;

	ssize_t n = read(eventFd, out->raw, 8);
	return n == 8;
}

void CLinuxPanelDriver::PutEvent(PanelDriverEvent & /*evt*/)
{
	return;
}

int CLinuxPanelDriver::PutCommand(PanelDriverCommand *cmd)
{
	switch (cmd->opcode) {
	case 1:
		USTGAPIFrontPanel::ResetLED((unsigned int)cmd->paramLo);
		return 0;
	case 2:
		USTGAPIFrontPanel::SetLED((unsigned int)cmd->paramLo);
		return 0;
	case 3:
		USTGAPIFrontPanel::SetLEDBlinking((unsigned int)cmd->paramLo);
		return 0;
	case 6:
		USTGAPIFrontPanel::SetLED16Bit((unsigned int)cmd->paramLo, cmd->paramHi);
		return 0;
	case 7:
		USTGAPIFrontPanel::Beep();
		return 0;
	default:
		return 0;
	}
}
