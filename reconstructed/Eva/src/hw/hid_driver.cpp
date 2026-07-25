/*
 * hid_driver.cpp  -  see include/hid_driver.h.
 */

#include "hid_driver.h"
#include "omega_vtables.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Real .rodata table at 0x08fd9c00, 127 (0x7f) bytes -- byte-read directly off the
 * ground-truth binary (not decompiled). Indexed by raw evdev scancode
 * (mRawCode/this+0x22), yields a Korg-internal keycode. Entries 0x7f mark "no
 * mapping" (same value as index 0's own 0x7f -- a real sentinel, not a gap).
 */
static const unsigned char s_kucMappingTable[0x7f] = {
	0x7f, 0x00, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x57, 0x57,
	0x1e, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x7f, 0x7f,
	0x47, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x32, 0x31, 0x7f,
	0x4c, 0x33, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x1d, 0x58, 0x24,
	0x5e, 0x5f, 0x5d, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x22,
	0x0e, 0x37, 0x38, 0x39, 0x3a, 0x48, 0x49, 0x4a, 0x4b, 0x5a, 0x5b, 0x5c, 0x65, 0x66,
	0x7f, 0x7f, 0x7f, 0x0b, 0x0c, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x34, 0x67, 0x61,
	0x23, 0x7f, 0x60, 0x7f, 0x20, 0x59, 0x21, 0x62, 0x64, 0x35, 0x63, 0x36, 0x1f, 0x34,
	0x7f, 0x7f, 0x7f, 0x7f, 0x70, 0x7f, 0x7f, 0x0f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
	0x7f,
};

namespace {
	/* Thunk matching the real vtable's own calling convention at each slot --
	 * every real CHIDDriver method above is a plain member function; these
	 * static wrappers give them the exact `void*(void*, ...)`-shaped address
	 * the vtable array below needs without relying on non-portable
	 * pointer-to-member-function-as-void* casts.
	 */
	void VDtor(CHIDDriver *self) { self->~CHIDDriver(); }
	int VOpen(CHIDDriver *self, void *arg) { return self->Open(arg); }
	int VClose(CHIDDriver *self, void *arg) { return self->Close(arg); }
	int VGetDriverClass(CHIDDriver *self) { return self->GetDriverClass(); }
	int VGetEvent(CHIDDriver *self, HIDUsbKeybEvent *out) { return self->GetEvent(out); }
	void VPutEvent(CHIDDriver *self, HIDUsbKeybEvent *evt) { self->PutEvent(*evt); }
	int VGetKeyboardEvent(CHIDDriver *self, AlphaKeybEvt *out) { return self->GetKeyboardEvent(out); }
	int VReadOvercurrentCondition(CHIDDriver *self) { return self->ReadOvercurrentCondition(); }
	int VEnableAfterOvercurrent(CHIDDriver *self) { return self->EnableAfterOvercurrent(); }
	int VKeyboardIsConnected(CHIDDriver *self) { return self->KeyboardIsConnected(); }
	void VSetLeds(CHIDDriver *self, unsigned char state) { self->SetLeds(state); }
	void VSetTypematicRateDelay(CHIDDriver *self, unsigned char delay) { self->SetTypematicRateDelay(delay); }
}

/* Real vtable, byte-exact (see hid_driver.h's own table, read directly off
 * .rodata+0x08fd9ce8). Slots 0/1 are the two real dtor entry points -- both wired
 * to the same shared ~CHIDDriver() thunk (the "deleting" variant differs only by
 * an extra operator delete(this), not modeled -- see the dtor's own comment).
 */
extern "C" void *PTR__CHIDDriver_08fd9ce8[13] = {
	(void *)&VDtor,                    /* slot  0 (+0x00) ~CHIDDriver() complete */
	(void *)&VDtor,                    /* slot  1 (+0x04) ~CHIDDriver() deleting */
	(void *)&VOpen,                    /* slot  2 (+0x08) Open(void*) */
	(void *)&VClose,                   /* slot  3 (+0x0c) Close(void*) */
	(void *)&VGetDriverClass,          /* slot  4 (+0x10) GetDriverClass() */
	(void *)&VGetEvent,                /* slot  5 (+0x14) GetEvent(SUsbKeybEvent*) */
	(void *)&VPutEvent,                /* slot  6 (+0x18) PutEvent(SUsbKeybEvent&) */
	(void *)&VGetKeyboardEvent,        /* slot  7 (+0x1c) GetKeyboardEvent(SKeyboardEvt&) */
	(void *)&VReadOvercurrentCondition,/* slot  8 (+0x20) ReadOvercurrentCondition() */
	(void *)&VEnableAfterOvercurrent,  /* slot  9 (+0x24) EnableAfterOvercurrent() */
	(void *)&VKeyboardIsConnected,     /* slot 10 (+0x28) KeyboardIsConnected() */
	(void *)&VSetLeds,                 /* slot 11 (+0x2c) SetLeds(uchar) */
	(void *)&VSetTypematicRateDelay,   /* slot 12 (+0x30) SetTypematicRateDelay(uchar) */
};

CHIDDriver::CHIDDriver(const char *name, const char * /*eventsName*/, const char * /*commandsName*/)
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = new char[len + 1];
	mName = dup;
	strcpy(dup, name);

	mVtbl = PTR__CHIDDriver_08fd9ce8;
	mFd = -1;
	mEventIndex = 0;
	mReadState = 0;
	mModifiers = 0;
	mRawEvent.tv_sec = 0;
	mRawEvent.tv_usec = 0;
	mRawEvent.type = 0;
	mRawEvent.code = 0;
	mRawEvent.value = 0;
}

CHIDDriver::~CHIDDriver()
{
	mVtbl = PTR__CHIDDriver_08fd9ce8;
	if (mFd >= 0) {
		close(mFd);
		mFd = -1;
	}
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	if (mName != 0)
		delete[] mName;
	mVtbl = (void *)PTR__CObjectBase_08e79d68;
	/* Deleting-dtor variant (08e4fce0) additionally calls operator delete(this) --
	 * not modeled here since nothing in this reconstruction's own call graph
	 * constructs a CHIDDriver via `new` through a base-class pointer yet
	 * (MMainHIDDriver uses placement new over a raw malloc, matching every other
	 * driver/module in mains.cpp).
	 */
}

int CHIDDriver::Open(void * /*arg*/)
{
	return 0;
}

int CHIDDriver::Close(void * /*arg*/)
{
	return 0;
}

int CHIDDriver::GetDriverClass()
{
	return 10;
}

int CHIDDriver::GetEvent(HIDUsbKeybEvent *out)
{
	int result = 0;
	if (mFd < 0)
		return result;

	ssize_t n = read(mFd, &mRawEvent, sizeof(mRawEvent));
	if (n <= 0xf)
		return result;

	/* Real: `iVar3 = (sVar2>>4) + this[0x10]; this[0x10] = iVar3; if (iVar3 != 0) ...`
	 * -- transcribed literally. Since a successful read always yields exactly one
	 * complete 16-byte record (sVar2>>4 == 1) and mReadState is reset to 0 at the
	 * end of a processed record, this always evaluates true after any full read;
	 * preserved as found rather than collapsed to a bare `if (n == 0x10)`.
	 */
	int recordCount = (int)(n >> 4) + mReadState;
	mReadState = recordCount;
	if (recordCount == 0)
		return result;
	mReadState = 0;

	if (mRawEvent.type != 1)
		return result;

	if (mRawEvent.code >= 0x7f)
		return result;

	out->keycode = (unsigned int)(unsigned char)s_kucMappingTable[mRawEvent.code];

	switch (mRawEvent.code) {
	case 0x1d: /* Ctrl */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xfe);
		else
			mModifiers = (unsigned char)(mModifiers | 0x01);
		break;
	case 0x61: /* AltGr */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xfd);
		else
			mModifiers = (unsigned char)(mModifiers | 0x02);
		break;
	case 0x2a: /* LShift */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xfb);
		else
			mModifiers = (unsigned char)(mModifiers | 0x04);
		break;
	case 0x36: /* RShift */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xf7);
		else
			mModifiers = (unsigned char)(mModifiers | 0x08);
		break;
	case 0x38: /* Alt */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xef);
		else
			mModifiers = (unsigned char)(mModifiers | 0x10);
		break;
	case 0x64: /* right Alt/Meta */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xdf);
		else
			mModifiers = (unsigned char)(mModifiers | 0x20);
		break;
	case 0x3a: /* CapsLock */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0xbf);
		else
			mModifiers = (unsigned char)(mModifiers | 0x40);
		break;
	case 0x45: /* NumLock */
		if (mRawEvent.value == 0)
			mModifiers = (unsigned char)(mModifiers & 0x7f);
		else
			mModifiers = (unsigned char)(mModifiers | 0x80);
		break;
	default:
		break;
	}

	out->modifiers = mModifiers;
	result = 1;
	return result;
}

void CHIDDriver::PutEvent(HIDUsbKeybEvent & /*evt*/)
{
	return;
}

bool CHIDDriver::GetKeyboardEvent(AlphaKeybEvt *out)
{
	HIDUsbKeybEvent raw;
	/* Real dispatch: `(**(code**)(*this+0x14))(this,&local_1c)` -- mVtbl slot 5 is
	 * GetEvent itself (see file header's vtable table); calling through the
	 * pointer rather than directly, matching the real disassembly and this
	 * project's convention of never asserting a real ground-truth vtable call as
	 * a C++ `virtual`.
	 */
	typedef int (*GetEventSlot)(CHIDDriver *, HIDUsbKeybEvent *);
	void **vtbl = (void **)mVtbl;
	GetEventSlot fn = (GetEventSlot)vtbl[5];
	int ok = fn(this, &raw);

	if (ok != 0) {
		/* REAL BUG, CONFIRMED AT THE DECOMPILE LEVEL, PRESERVED NOT FIXED:
		 * GetKeyboardEvent's stack layout (Ghidra's own `local_1c`/`local_18`/
		 * `local_17` naming) places `local_18` exactly at SUsbKeybEvent's own
		 * +4 byte -- but GetEvent() only ever writes +0 (keycode) and +5
		 * (modifiers); +4 is never touched. `raw.reserved4` below is therefore
		 * genuinely uninitialized stack memory in the real binary too, and
		 * `isKeyDown` is computed from it unconditionally. `raw` here is a
		 * plain uninitialized local (not zeroed), reproducing the same
		 * undefined read the real binary performs rather than papering over
		 * it with a deterministic zero-init.
		 */
		out->isKeyDown = (raw.reserved4 == 0) ? 1u : 0u;
		out->keycode = raw.keycode;
		out->unused8 = 0;
		out->modifiers = raw.modifiers;
	}
	return ok != 0;
}

int CHIDDriver::ReadOvercurrentCondition()
{
	return 0;
}

int CHIDDriver::EnableAfterOvercurrent()
{
	return 1;
}

int CHIDDriver::KeyboardIsConnected()
{
	char path[64];
	struct stat st;
	int eventIdx;

	if (mFd < 0) {
		mEventIndex = 0;
		int i = 0;
		do {
			sprintf(path, "/sys/class/input/event%d/device/id/bustype", i);
			int fd = open(path, O_RDONLY);
			if (fd >= 0) {
				unsigned int bustype = 0;
				read(fd, &bustype, 4);
				close(fd);
				/* Real: compares the 4 raw bytes read from the sysfs file
				 * against the literal dword 0x33303030 -- ASCII "0003" (the
				 * USB bustype string bare_conf/input.h defines) read as a
				 * little-endian dword.
				 */
				if (bustype == 0x33303030) {
					i = mEventIndex;
					break;
				}
			}
			i = mEventIndex + 1;
			mEventIndex = i;
		} while (i < 4);
		eventIdx = i;
	} else {
		/* fd already open -- real: re-fetches the cached index rather than
		 * re-scanning, still re-checks the sysfs node below every call (this is
		 * the real unplug-detection path).
		 */
		eventIdx = mEventIndex;
	}

	sprintf(path, "/sys/class/input/event%d", eventIdx);
	if (stat(path, &st) == 0) {
		int ok = 1;
		if (mFd < 0) {
			sprintf(path, "/dev/input/event%d", mEventIndex);
			mFd = open(path, O_NONBLOCK);
			ok = (mFd >= 0) ? 1 : 0;
		}
		return ok;
	}

	/* sysfs node gone -- real: only closes+resets+returns 0 if we had a fd open;
	 * if we never had one (scan above found nothing in range), still falls
	 * through and returns 0 via the same final `return uVar2` (uVar2 initialized
	 * to 0 on this branch), same effective result.
	 */
	if (mFd >= 0) {
		close(mFd);
		mFd = -1;
	}
	return 0;
}

void CHIDDriver::SetLeds(unsigned char /*state*/)
{
	return;
}

void CHIDDriver::SetTypematicRateDelay(unsigned char /*delay*/)
{
	return;
}
