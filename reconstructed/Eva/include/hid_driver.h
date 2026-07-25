/*
 * hid_driver.h  -  CHIDDriver, the real Linux-evdev USB keyboard driver behind
 * MMainHIDDriver (mains.cpp). Stage 6 breadth sweep, 2026-07-25.
 *
 * FOUND VIA: broad nm -C class-inventory sweep, cross-checked against mains.cpp's own
 * "Both real driver classes' own constructors are declared here as call-contract
 * externs -- not implemented, Stage 4+" note -- MMainPanelDriver/MMainHIDDriver
 * construct these two unconditionally, every boot, before any config-table gating
 * (unlike most of Stage 6's other candidates). CHIDDriver itself is small (13 real
 * vtable slots / 10 named methods) and self-contained -- no further subsystem
 * dependency beyond raw evdev syscalls -- making it one of the few remaining
 * boot-path-*direct* (not just boot-path-*adjacent*) classes still unclaimed.
 *
 * REAL VTABLE, byte-read directly from ground truth (not inferred from call sites --
 * `PTR__CHIDDriver_08fd9ce8`, 13 slots, ends at +0x34 where the typeinfo-name string
 * "10CHIDDriver" begins, confirmed by a raw dword read of the .rodata bytes):
 *
 *   slot  0 (+0x00)  ~CHIDDriver()                (complete-object dtor, 08e4fc80)
 *   slot  1 (+0x04)  ~CHIDDriver()                (deleting dtor,        08e4fce0)
 *   slot  2 (+0x08)  Open(void*)                                         08e4f7e0
 *   slot  3 (+0x0c)  Close(void*)                                        08e4f7f0
 *   slot  4 (+0x10)  IHIDDriver::GetDriverClass()                        08e4fdf0
 *   slot  5 (+0x14)  GetEvent(IHIDDriver::SUsbKeybEvent*)                08e4f920
 *   slot  6 (+0x18)  PutEvent(IHIDDriver::SUsbKeybEvent&)                08e4f800
 *   slot  7 (+0x1c)  GetKeyboardEvent(IAlphaKeybEvent::SKeyboardEvt&)    08e4f810
 *   slot  8 (+0x20)  ReadOvercurrentCondition()                          08e4f8e0
 *   slot  9 (+0x24)  EnableAfterOvercurrent()                            08e4f8f0
 *   slot 10 (+0x28)  KeyboardIsConnected()                               08e4fb20
 *   slot 11 (+0x2c)  SetLeds(unsigned char)                              08e4f910
 *   slot 12 (+0x30)  SetTypematicRateDelay(unsigned char)                08e4f900
 *
 * GetKeyboardEvent (slot 7) dispatches slot 5 (+0x14) ON ITSELF -- i.e. it calls its
 * own GetEvent() virtually rather than directly, confirmed by the raw vtable-slot
 * table above matching the decompile's `(**(code**)(*this+0x14))(this,&local_1c)` --
 * this reconstruction preserves that as a real (non-virtual-keyword, matching this
 * project's established "no real C++ virtual for ground-truth vtable slots" rule,
 * [[eva_clientcommserver_sysexmsgtaskbase_followup]]) call through mVtbl[5].
 *
 * REAL LAYOUT (from CHIDDriver::CHIDDriver@08e4fd50.c, confirms size 0x28 = 40 bytes,
 * matching MMainHIDDriver's own malloc(0x28)):
 *   +0x00  mVtbl        CNamedObjectBase's base vtable first (08e81378), then
 *                       CHIDDriver's own (08fd9ce8) once the name copy succeeds.
 *   +0x04  mName        malloc'd copy of the ctor's `name` arg ("HIDDriver" from
 *                       MMainHIDDriver -- the ctor's own eventsName/commandsName
 *                       args are dead, confirmed unused anywhere in the real body).
 *   +0x08  mFd          evdev fd, -1 until KeyboardIsConnected() opens one.
 *   +0x0c  mEventIndex  which /sys/class/input/eventN matched (0..3 scanned).
 *   +0x10  mReadState   GetEvent()'s own record-accumulator (see .cpp -- transcribed
 *                       literally, real semantics not fully resolved, see there).
 *   +0x14  mModifiers   1-byte running bitmask of held Ctrl/Alt/Shift/Meta state,
 *                       assembled bit-by-bit in GetEvent() as individual scancodes
 *                       for Ctrl(0x1d)/AltGr(0x61)/LShift(0x2a)/RShift(0x36)/
 *                       CapsLock(0x3a)/Meta-ish(0x38/0x64) toggle on/off.
 *   +0x18  mRawEvent    raw Linux `struct input_event` overlay -- read(2) writes
 *                       directly into the object here (`read(fd, this+0x18, 0x10)`,
 *                       exactly sizeof(struct input_event) on a 32-bit target: 2x
 *                       4-byte timeval + 2-byte type + 2-byte code + 4-byte value):
 *                         +0x18 tv_sec   (4, unused by GetEvent's own logic)
 *                         +0x1c tv_usec  (4, unused)
 *                         +0x20 type     (2, checked == 1 i.e. EV_KEY)
 *                         +0x22 code     (2, the evdev scancode, indexes
 *                                        s_kucMappingTable)
 *                         +0x24 value    (4, 0 = key-up, nonzero = key-down/repeat)
 *
 * s_kucMappingTable (.rodata+0x08fd9c00, 127 bytes) -- real evdev-scancode ->
 * Korg-internal-keycode lookup table, byte-read directly from ground truth (not
 * decompiled -- Ghidra only emitted a `DAT_`-less symbol reference, no per-entry
 * semantics recovered beyond "it's a lookup table indexed by scancode").
 *
 * IAlphaKeybEvent::SKeyboardEvt (GetKeyboardEvent's own out-param, from
 * IAlphaKeybEvent.h -- not in this project's tree, reconstructed shape only from the
 * decompile's own field writes) and IHIDDriver::SUsbKeybEvent (GetEvent's own
 * out-param) are both modeled as minimal local shapes sufficient to reproduce the
 * exact byte writes GetKeyboardEvent/GetEvent perform -- not full real headers.
 */

#ifndef HID_DRIVER_H
#define HID_DRIVER_H

/* Minimal local shape for IHIDDriver::SUsbKeybEvent, from GetEvent@08e4f920.c's own
 * field writes: `param_1` (uint scancode-mapped keycode), `param_1+4` (unused/
 * zeroed word -- decompile shows `*(undefined4*)(param_1+4)` never written by
 * GetEvent itself, only by GetKeyboardEvent's own local_1c relay -- kept as a
 * reserved dword here), `param_1+5` (a 1-byte modifier snapshot, `this[0x14]`).
 */
struct HIDUsbKeybEvent {
	unsigned int   keycode;    /* +0x00, written by GetEvent(). */
	unsigned int   reserved4;  /* +0x04 -- REAL GROUND-TRUTH BUG: GetEvent() never
	                             * writes this field; GetKeyboardEvent() reads it
	                             * anyway (as "local_18") to compute isKeyDown.
	                             * Deliberately left as an ordinary local (never
	                             * initialized by this reconstruction either) to
	                             * reproduce the same undefined read -- see
	                             * hid_driver.cpp's GetKeyboardEvent(). */
	unsigned char  modifiers;  /* written by GetEvent() (real offset +5, not
	                             * asserted here -- only field identity/order
	                             * matters, matching lcd_control.cpp's "local shape
	                             * sufficient to reproduce the call" convention). */
};

/* Minimal local shape for IAlphaKeybEvent::SKeyboardEvt, from
 * GetKeyboardEvent@08e4f810.c's own field writes. */
struct AlphaKeybEvt {
	unsigned int  isKeyDown;  /* +0x00: 1 if value==0 (key-up), matching the decompile's
	                            * `(local_18 == 0)` -- i.e. this field is really
	                            * "isKeyUp", preserved with that exact polarity. */
	unsigned int  keycode;    /* +0x0c: relayed from local_1c (GetEvent's own
	                            * `keycode` out-field). */
	unsigned int  unused8;    /* +0x04: real decompile writes 0 here unconditionally. */
	unsigned char modifiers;  /* +0x10: relayed from local_17 (GetEvent's own
	                            * `modifiers` byte), re-encoded bit-by-bit -- see .cpp. */
};

/* Raw Linux `struct input_event` overlay for the 32-bit target -- 2x 4-byte
 * timeval halves + 2-byte type + 2-byte code + 4-byte value = 16 bytes, no padding
 * (offset 12 is already 4-byte aligned for `value`). Declared as its own POD struct
 * (rather than 5 loose CHIDDriver fields) so `read(fd, &mRawEvent, sizeof(mRawEvent))`
 * has a compiler-checkable destination size instead of aliasing into an undersized
 * adjacent member.
 */
struct HIDRawInputEvent {
	int            tv_sec;   /* +0x00 (CHIDDriver absolute +0x18), unused */
	int            tv_usec;  /* +0x04 (+0x1c), unused */
	short          type;     /* +0x08 (+0x20), checked == 1 (EV_KEY) */
	unsigned short code;     /* +0x0a (+0x22), the evdev scancode */
	int            value;    /* +0x0c (+0x24), 0 = key-up, nonzero = key-down/repeat */
};

class CHIDDriver {
public:
	/* .text+0x08e4fd50, 132 bytes. */
	CHIDDriver(const char *name, const char *eventsName, const char *commandsName);
	/* .text+0x08e4fc80 (complete-object) / 08e4fce0 (deleting) -- both real,
	 * transcribed as one shared body plus the deleting variant's extra
	 * operator delete(this), matching this project's established dtor-pair
	 * convention (task.h/out_link.h). */
	~CHIDDriver();

	/* .text+0x08e4f7e0/08e4f7f0 -- real, both unconditional no-ops (`return 0;`). */
	int Open(void *arg);
	int Close(void *arg);

	/* .text+0x08e4fdf0 -- IHIDDriver::GetDriverClass(), real, returns a constant. */
	int GetDriverClass();

	/* .text+0x08e4f920, 468 bytes -- real. Reads one raw evdev record, decodes
	 * scancode via s_kucMappingTable, tracks the modifier bitmask at mModifiers. */
	int GetEvent(HIDUsbKeybEvent *out);

	/* .text+0x08e4f800 -- real, unconditional no-op. */
	void PutEvent(HIDUsbKeybEvent &evt);

	/* .text+0x08e4f810, 194 bytes -- real. Dispatches GetEvent() through mVtbl[5]
	 * (not a direct call -- see file header), then relays/re-encodes into the
	 * higher-level AlphaKeybEvt shape. */
	bool GetKeyboardEvent(AlphaKeybEvt *out);

	/* .text+0x08e4f8e0/08e4f8f0 -- real, unconditional constants (0 / 1). */
	int ReadOvercurrentCondition();
	int EnableAfterOvercurrent();

	/* .text+0x08e4fb20, 333 bytes -- real. Scans /sys/class/input/eventN (N=0..3)
	 * for USB bustype ("0003", the literal dword 0x33303030 the decompile compares
	 * against -- ASCII "0003" read as a raw little-endian dword), opens
	 * /dev/input/eventN once found. Re-run-safe: returns cached state once mFd
	 * is set, closes and resets mFd if the /sys node disappears. */
	int KeyboardIsConnected();

	/* .text+0x08e4f910/08e4f900 -- real, unconditional no-ops (real hardware has
	 * no LED/typematic-rate control path here -- genuine ground-truth behavior,
	 * not a reconstruction gap). */
	void SetLeds(unsigned char state);
	void SetTypematicRateDelay(unsigned char delay);

private:
	void          *mVtbl;        /* +0x00 */
	char          *mName;        /* +0x04 */
	int            mFd;          /* +0x08, ctor sets -1 */
	int            mEventIndex;  /* +0x0c, ctor zeroes */
	int            mReadState;   /* +0x10, ctor zeroes */
	unsigned char  mModifiers;   /* +0x14, ctor zeroes */
	HIDRawInputEvent mRawEvent;  /* +0x18, ctor zeroes -- see HIDRawInputEvent above */

	friend struct HIDDriverTestHooks;
};

#endif /* HID_DRIVER_H */
