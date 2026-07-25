/*
 * panel_driver.h  -  CLinuxPanelDriver + USTGAPIFrontPanel, the real front-panel
 * LED/button/beep driver behind MMainPanelDriver (mains.cpp). Stage 6 breadth sweep,
 * 2026-07-25 -- reconstructed alongside hid_driver.h (see that file's header for the
 * shared "found via nm -C sweep, boot-path-direct not just -adjacent" rationale).
 *
 * REAL VTABLE, byte-read directly from ground truth (`PTR__CLinuxPanelDriver_08fd9dc8`,
 * 8 slots, ends at +0x20 where the typeinfo-name string "17CLinuxPanelDriver" begins):
 *
 *   slot 0 (+0x00)  ~CLinuxPanelDriver()               (complete-object, 08e4ffe0)
 *   slot 1 (+0x04)  ~CLinuxPanelDriver()               (deleting,        08e50010)
 *   slot 2 (+0x08)  Open(void*)                                          08e4fee0
 *   slot 3 (+0x0c)  Close(void*)                                         08e4fef0
 *   slot 4 (+0x10)  CPanelDriver::GetDriverClass()                       08e500d0
 *   slot 5 (+0x14)  GetEvent(CPanelDriver::SEvent*)                      08e4ffa0
 *   slot 6 (+0x18)  PutEvent(CPanelDriver::SEvent&)                      08e4ff00
 *   slot 7 (+0x1c)  PutCommand(CPanelDriver::SCommand*)                  08e4ff10
 *
 * REAL LAYOUT (from CLinuxPanelDriver::CLinuxPanelDriver@08e50050.c, confirms size
 * 0x08 = 8 bytes, matching MMainPanelDriver's own malloc(8)):
 *   +0x00  mVtbl  CNamedObjectBase's base vtable first, then this class's own.
 *   +0x04  mName  malloc'd copy of the ctor's `name` arg ("PanelDriver").
 *
 * GetEvent() reads 8-byte panel-button/encoder/touch events straight off
 * CCommDriver::getInstance()'s own mEventFd (CCommDriver's real +0x10 field,
 * comm_driver.h -- cross-checked and consistent between the two reconstructions).
 * PutCommand() is a small opcode switch dispatching to USTGAPIFrontPanel's 5 static
 * wrappers below, each of which builds one 16-byte STGMessage and sends it via
 * USTGUserAPI::SendPanelMessage() (ustg_user_api.h) -- confirms STGMessage's
 * `{u16 type; u16 subtype; u32 field8; u32 field12}` shape (lcd_control.cpp's own
 * STGMessageLocalShape) generalizes to a 5th trailing `u32 param` field whenever the
 * opcode needs one, all still under the same real `type=0x10`/`subtype=1` header
 * pair LoadStoredSettings() already established -- `type` here doubles as a message
 * *byte length* (0x10 == sizeof of the 4-field shape), not a type tag; kept the same
 * field name as lcd_control.cpp for consistency even though "byte length" is the more
 * accurate reading.
 *
 * CPanelDriver::SEvent/SCommand (both from a CPanelDriver.h not in this project's
 * tree) modeled as minimal local shapes sufficient to reproduce the real byte reads/
 * writes -- SEvent is 8 opaque bytes (GetEvent only ever read()s straight into it,
 * never interprets fields itself); SCommand is `{u32 opcode; u16 param; ...}` from
 * PutCommand's own field reads (opcode at +0, a u16 param at +4, and for opcode 6
 * -- SetLED16Bit -- a second u16 at +6 read together with +4 as one u32).
 */

#ifndef PANEL_DRIVER_H
#define PANEL_DRIVER_H

/* Minimal local shape for CPanelDriver::SEvent -- GetEvent() only ever read()s 8 raw
 * bytes into it, never interprets any field itself (matches CCommDriver's own
 * "3 raw fifo fds" convention -- decoding is presumably done by whatever consumes it,
 * out of scope here).
 */
struct PanelDriverEvent {
	unsigned char raw[8];
};

/* Minimal local shape for CPanelDriver::SCommand, from PutCommand@08e4ff10.c's own
 * field reads: opcode selects the operation, param{Lo,Hi} together form the u32
 * `SetLED16Bit` needs (led-mask low/high halves) or just paramLo alone for the
 * single-LED opcodes.
 */
struct PanelDriverCommand {
	unsigned int   opcode;   /* +0x00 */
	unsigned short paramLo;  /* +0x04 */
	unsigned short paramHi;  /* +0x06 (only read for opcode 6, SetLED16Bit) */
};

/* USTGAPIFrontPanel -- 5 real static wrappers (.text+0x08e1d440..0x08e1d550, all
 * __cdecl, all 51-67 bytes), each building one real STGMessage and sending it via
 * USTGUserAPI::SendPanelMessage(). Modeled as plain free functions in a namespace
 * (matching this project's USTGAPILCDControl precedent -- a bare class-name prefix
 * used only for symbol scoping, no instance state) rather than a class, since none
 * of the 5 real functions take or use a `this`.
 */
namespace USTGAPIFrontPanel {
	void ResetLED(unsigned int ledMask);
	void SetLED(unsigned int ledMask);
	void SetLEDBlinking(unsigned int ledMask);
	void SetLED16Bit(unsigned int ledMaskLo, unsigned short ledMaskHi);
	void Beep();
}

class CLinuxPanelDriver {
public:
	/* .text+0x08e50050, 91 bytes. */
	explicit CLinuxPanelDriver(const char *name);
	/* .text+0x08e4ffe0 (complete-object) / 08e50010 (deleting) -- shared body,
	 * same convention as CHIDDriver's dtor pair. */
	~CLinuxPanelDriver();

	/* .text+0x08e4fee0/08e4fef0 -- real, both unconditional no-ops. */
	int Open(void *arg);
	int Close(void *arg);

	/* .text+0x08e500d0 -- CPanelDriver::GetDriverClass(), real constant. */
	int GetDriverClass();

	/* .text+0x08e4ffa0, 53 bytes -- real. Reads one 8-byte event from
	 * CCommDriver's shared mEventFd. */
	bool GetEvent(PanelDriverEvent *out);

	/* .text+0x08e4ff00 -- real, unconditional no-op. */
	void PutEvent(PanelDriverEvent &evt);

	/* .text+0x08e4ff10, 123 bytes -- real. 5-way opcode switch over
	 * USTGAPIFrontPanel's wrappers. */
	int PutCommand(PanelDriverCommand *cmd);

private:
	void *mVtbl;  /* +0x00 */
	char *mName;  /* +0x04 */

	friend struct PanelDriverTestHooks;
};

#endif /* PANEL_DRIVER_H */
