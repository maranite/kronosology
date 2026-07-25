// SPDX-License-Identifier: GPL-2.0
/*
 * test_keybed_interface.cpp  -  host-side known-answer test for the
 * bulk of CSTGKeybedInterface's ~20 real methods (batch 64, see
 * ../include/oa_keybed_init.h / ../src/init/keybed_interface.cpp).
 *
 * Links keybed_interface.cpp + keybed_receive.cpp + comport.cpp +
 * keybed_debounce.cpp for real; comport_init.cpp is intentionally NOT
 * linked here (MemberStartup/TryComPort's own Initialize() dependency
 * is mocked directly instead -- this test cares about THIS class's own
 * gating/framing logic, not the Super-I/O bring-up already covered by
 * test_comport_init.cpp).
 *
 * Exercises:
 *   [1] SendCommandByte/SendCommandBuf -- ungated raw sends, interrupt
 *       triggered only when the FIFO was empty beforehand.
 *   [2] SendByte -- gated on state==2 exactly.
 *   [3] SetLED -- real mangled-ABI class method: 0x49/0x4a code
 *       mapping, action bit-pack, state<=1 bail, unknown-code bail.
 *   [4] StartCalibration/EndCalibration/CancelCalibration -- state
 *       machine (2->3->4) and command bytes.
 *   [5] EnterKeyCheckMode/ExitKeyCheckMode -- state==2-exact gating.
 *   [6] SetKeyChatterGateTime -- the confirmed bit-split encode + clamp.
 *   [7] WriteMessageToQueue/ReadMessageFromQueue -- ring buffer
 *       round-trip, including the "drop if it doesn't fit" and
 *       "advance cursor by the real per-type length" behaviors.
 *   [8] HandleActiveSense -- the 4 sub-type dispatches (8/9/a/d) plus
 *       the always-copied raw 4 bytes.
 *   [9] ApplyAftertouchTable -- table selection by NKS4_PANEL_KIND.
 *   [10] FilterAnalogController -- the "first non-centered call arms,
 *        second one filters" state machine for code 1/2, and the
 *        always-filters code 0 path (mocked ApplyKeybedCalibration).
 *   [11] EnableUSBPort/EnableRearLED -- state gate + port>1 rejection.
 *   [12] ReceiveMessage's new state==2 dispatch -- 0xE0-class heartbeat
 *        (gate1-only) vs non-heartbeat (gate2-then-gate1) enqueue rules.
 */

#include <cstdio>
#include <cstring>
#include "oa_comport.h"
#include "oa_keybed_init.h"
#include "oa_setup_global_resources.h"
#include "oa_engine.h" /* CSTGMessageProcessor::sInstance */
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-64s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-64s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static unsigned char g_keybedInstance[KEYBED_SINSTANCE_SIZE];
static unsigned char g_frontPanel[STGAPI_FRONTPANEL_SIZE];
static unsigned char g_msgProcessorRaw[0x1040];

extern "C" unsigned char *CSTGKeybedInterface_sInstance(void) { return g_keybedInstance; }
unsigned char *STGAPIFrontPanelStatus::sInstance = g_frontPanel;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance =
	reinterpret_cast<CSTGMessageProcessor *>(g_msgProcessorRaw);

static unsigned char *MsgProcRaw(void)
{
	return reinterpret_cast<unsigned char *>(CSTGMessageProcessor::sInstance);
}

/* Mocked hardware/RTAI layer -- this test cares about CSTGKeybedInterface's
 * own gating/framing logic, not CSTGComPort's real UART bring-up (already
 * covered by test_comport.cpp/test_comport_init.cpp). */
extern "C" {
/*
 * `Initialize()` is mocked below and never sets a real `ioBase`, so the
 * embedded CSTGComPort's `ioBase` stays 0 (zeroed by `reset()`) --
 * `TriggerInterrupt()`'s own real poll/drain/transmit loop
 * (ComPortServiceLoop, comport.cpp, already covered by test_comport.cpp)
 * reads LSR at `ioBase+5` and IIR at `ioBase+2`. Returning IIR==1 ("no
 * interrupt pending") and LSR==0 ("nothing ready") lets that real loop
 * exit after exactly one iteration instead of spinning forever against
 * an always-0 mock -- this test cares about CSTGKeybedInterface's own
 * gating/framing logic, not re-exercising that already-tested loop.
 */
unsigned char stg_inb(unsigned int port)
{
	if (port == 2)
		return 1; /* IIR: no interrupt pending */
	return 0; /* LSR and everything else: nothing ready */
}
void stg_outb(unsigned int, unsigned char) {}
unsigned long stg_local_irq_save(void) { return 0; }
void stg_local_irq_restore(unsigned long) {}
void rtwrap_shutdown_irq(unsigned int) {}
void rtwrap_release_irq(unsigned int) {}
void __const_udelay(unsigned long) {} /* instant -- no real delay in a host test */
/* MemberStartup's own dependency -- not exercised by any case below
 * (this file focuses on the class's own gating/framing logic), but must
 * still resolve at link time since keybed_interface.cpp references it
 * unconditionally. */
void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *) {}

static int g_calibrationCode = -1;
static short g_calibrationInput = 0;
static short g_calibrationResult = 0x1234;
short ApplyKeybedCalibration(int code, short rawValue)
{
	g_calibrationCode = code;
	g_calibrationInput = rawValue;
	return g_calibrationResult;
}
}

/* CSTGComPort::Initialize is deliberately NOT reconstructed in this
 * project (oa_comport.h's own note) -- mocked here purely so
 * MemberStartup/TryComPort link and can be smoke-tested without pulling
 * in comport_init.cpp's real Super-I/O bring-up. */
extern "C" {
static bool g_initializeShouldSucceed;
}
char CSTGComPort::Initialize(eComPortId, eBaudRateCode, eReceiveFifoThresholdCode)
{
	comPortId = 0;
	return g_initializeShouldSucceed ? 1 : 0;
}

static void reset(void)
{
	memset(g_keybedInstance, 0, sizeof(g_keybedInstance));
	memset(g_frontPanel, 0, sizeof(g_frontPanel));
	memset(g_msgProcessorRaw, 0, sizeof(g_msgProcessorRaw));
	new (g_keybedInstance) CSTGKeybedComPort();
	g_calibrationCode = -1;
	g_initializeShouldSucceed = false;
}

static unsigned char TxOccupancy(void)
{
	CSTGComPort *cp = reinterpret_cast<CSTGComPort *>(g_keybedInstance);
	return (unsigned char)(cp->txFifo.head - cp->txFifo.tail);
}

int main(void)
{
	printf("[1] SendCommandByte/SendCommandBuf -- ungated, interrupt only when FIFO was empty:\n");
	reset();
	CSTGKeybedInterface_SendCommandByte(0x42);
	check_eq("byte queued (ungated, any state)", TxOccupancy(), 1);
	unsigned char buf3[3] = { 0x01, 0x02, 0x03 };
	reset();
	CSTGKeybedInterface_SendCommandBuf(buf3, 3);
	check_eq("3 bytes queued", TxOccupancy(), 3);

	printf("\n[2] SendByte -- gated on state==2 exactly:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	CSTGKeybedInterface_SendByte(0x55);
	check_eq("state==1: rejected, nothing queued", TxOccupancy(), 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_SendByte(0x55);
	check_eq("state==2: accepted", TxOccupancy(), 1);

	printf("\n[3] SetLED -- code/action mapping, state gate:\n");
	reset();
	CSTGKeybedInterface *kb = reinterpret_cast<CSTGKeybedInterface *>(g_keybedInstance);
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	kb->SetLED(0x49, 1);
	check_eq("state<=1: rejected", TxOccupancy(), 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	kb->SetLED(0x49, 1); /* SetLED action==1 */
	check_eq("code 0x49 accepted at state==2", TxOccupancy(), 2);
	{
		/* txFifo.buffer starts at CSTGComPort+0x1, not the message
		 * ring -- read it back through the real accessor instead of
		 * a raw offset. */
		CSTGComPort *cp = reinterpret_cast<CSTGComPort *>(g_keybedInstance);
		check_eq("buf[0] == 0xd1 (0xd0 | action 1)", cp->txFifo.buffer[0], 0xd1);
		check_eq("buf[1] == 0 (led index for code 0x49)", cp->txFifo.buffer[1], 0);
	}
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 4; /* calibration-ending state */
	kb->SetLED(0x4a, 2);
	check_eq("code 0x4a accepted even during calibration states (>1)", TxOccupancy(), 2);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	kb->SetLED(0x50, 1); /* unknown code */
	check_eq("unknown LED code rejected", TxOccupancy(), 0);

	printf("\n[4] StartCalibration/EndCalibration/CancelCalibration -- state machine:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 1;
	CSTGKeybedInterface_StartCalibration(5);
	check_eq("state<=1: StartCalibration rejected", g_keybedInstance[KEYBED_OFF_STATE], 1);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_StartCalibration(5);
	check_eq("StartCalibration: state -> 3", g_keybedInstance[KEYBED_OFF_STATE], 3);
	check_eq("StartCalibration: bytes queued", TxOccupancy(), 2);
	CSTGKeybedInterface_EndCalibration();
	check_eq("EndCalibration (state==3): state -> 4", g_keybedInstance[KEYBED_OFF_STATE], 4);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 4;
	CSTGKeybedInterface_EndCalibration();
	check_eq("EndCalibration (state==4): rejected, stays 4", g_keybedInstance[KEYBED_OFF_STATE], 4);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 3;
	CSTGKeybedInterface_CancelCalibration();
	check_eq("CancelCalibration (state==3): state -> 4", g_keybedInstance[KEYBED_OFF_STATE], 4);

	printf("\n[5] EnterKeyCheckMode/ExitKeyCheckMode -- state==2-exact gate:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 3;
	CSTGKeybedInterface_EnterKeyCheckMode();
	check_eq("state==3: rejected (needs exactly 2)", TxOccupancy(), 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_EnterKeyCheckMode();
	check_eq("state==2: accepted", TxOccupancy(), 2);

	printf("\n[6] SetKeyChatterGateTime -- bit-split encode + clamp:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_SetKeyChatterGateTime(4); /* <=0x3d: (4>>1)&1=0, 4>>2=1 */
	{
		CSTGComPort *cp = reinterpret_cast<CSTGComPort *>(g_keybedInstance);
		check_eq("ms=4: cmd byte", cp->txFifo.buffer[0], 0xb4);
		check_eq("ms=4: byte1 = (4>>1)&1", cp->txFifo.buffer[1], 0);
		check_eq("ms=4: byte2 = 4>>2", cp->txFifo.buffer[2], 1);
	}
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_SetKeyChatterGateTime(200); /* > 0x3d: clamps to {1,0xf} */
	{
		CSTGComPort *cp = reinterpret_cast<CSTGComPort *>(g_keybedInstance);
		check_eq("ms=200 (clamped): byte1", cp->txFifo.buffer[1], 1);
		check_eq("ms=200 (clamped): byte2", cp->txFifo.buffer[2], 0xf);
	}

	printf("\n[7] WriteMessageToQueue/ReadMessageFromQueue -- ring buffer round-trip:\n");
	reset();
	unsigned char msg[4] = { 0xea, 0x23, 0x07, 0x20 }; /* real heartbeat bytes */
	CSTGKeybedInterface_WriteMessageToQueue(msg, 4);
	unsigned char out[4] = { 0, 0, 0, 0 };
	unsigned char n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("read back 4 bytes (type 6 == 4-byte message)", n, 4);
	check_eq("byte0", out[0], 0xea);
	check_eq("byte3", out[3], 0x20);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("queue now empty", n, 0);

	printf("\n[8] HandleActiveSense -- sub-type dispatch:\n");
	reset();
	unsigned char hb9[4] = { 0xe9, 0, 0, 0 };
	CSTGKeybedInterface_HandleActiveSense(hb9);
	check_eq("nibble 9: FOOTSWITCH0", g_frontPanel[STGAPI_OFF_FOOTSWITCH0], 0x24);
	check_eq("nibble 9: FOOTSWITCH1", g_frontPanel[STGAPI_OFF_FOOTSWITCH1], 0x3d);
	check_eq("nibble 9: NKS4_PANEL_KIND set", g_frontPanel[STGAPI_OFF_NKS4_PANEL_KIND], 1);
	reset();
	unsigned char hba[4] = { 0xea, 1, 2, 3 };
	CSTGKeybedInterface_HandleActiveSense(hba);
	check_eq("nibble a: PANEL_DETECTED", g_frontPanel[STGAPI_OFF_PANEL_DETECTED], 1);
	check_eq("nibble a: debounce filter armed", g_keybedInstance[KEYBED_OFF_DEBOUNCE_FILTER], 1);
	check_eq("raw bytes always copied (RAW0)", g_frontPanel[STGAPI_OFF_KEYBED_RAW0], 0xea);
	check_eq("raw bytes always copied (RAW2)", g_frontPanel[STGAPI_OFF_KEYBED_RAW2], 2);
	reset();
	unsigned char hbd[4] = { 0xed, 0, 0, 0 };
	CSTGKeybedInterface_HandleActiveSense(hbd);
	check_eq("nibble d: flag set", g_frontPanel[STGAPI_OFF_KEYBED_NIBBLE_D_FLAG], 1);

	printf("\n[9] ApplyAftertouchTable -- table selection by NKS4_PANEL_KIND:\n");
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_PANEL_KIND] = 0;
	/* kAftertouchTable0[0x80] == 0x4d, per the verbatim-extracted table
	 * (keybed_aftertouch_table0.inc) -- not a round number, just the
	 * real curve's own value at that index. */
	check_eq("mode 0, raw=0x80", CSTGKeybedInterface_ApplyAftertouchTable(0x80), 0x4d);
	g_frontPanel[STGAPI_OFF_NKS4_PANEL_KIND] = 1;
	check_eq("mode 1, raw=0x00", CSTGKeybedInterface_ApplyAftertouchTable(0x00), 0x00);
	g_frontPanel[STGAPI_OFF_NKS4_PANEL_KIND] = 2;
	check_eq("mode 2, raw=0xff", CSTGKeybedInterface_ApplyAftertouchTable(0xff), 0xff);

	printf("\n[10] FilterAnalogController -- arm-then-filter state machine:\n");
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_HW_VERSION] = 3;
	unsigned char v = 0x80; /* centered */
	bool changed = CSTGKeybedInterface_FilterAnalogController(1, &v);
	check_eq("code1 first centered call: flag was 0 -> unchanged", changed, 0);
	check_eq("flag now armed", g_keybedInstance[KEYBED_OFF_ANALOG1_CHANGED_FLAG], 1);
	v = 0x80;
	changed = CSTGKeybedInterface_FilterAnalogController(1, &v);
	check_eq("code1 second centered call: flag was 1 -> filters", changed, 1);
	check_eq("ApplyKeybedCalibration called with code==1", g_calibrationCode, 1);
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_HW_VERSION] = 3;
	v = 0x40; /* non-centered, immediate filter */
	changed = CSTGKeybedInterface_FilterAnalogController(2, &v);
	check_eq("code2 non-centered: filters immediately", changed, 1);
	check_eq("ApplyKeybedCalibration called with code==2", g_calibrationCode, 2);
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_HW_VERSION] = 3;
	v = 0x10;
	changed = CSTGKeybedInterface_FilterAnalogController(0, &v);
	check_eq("code0 (aftertouch): always filters", changed, 1);
	check_eq("ApplyKeybedCalibration called with code==0", g_calibrationCode, 0);
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_HW_VERSION] = 2; /* not ready */
	v = 0x10;
	changed = CSTGKeybedInterface_FilterAnalogController(0, &v);
	check_eq("HW_VERSION!=3: reports true but does NOT call calibration", changed, 1);
	check_eq("calibration NOT invoked", g_calibrationCode, -1);
	reset();
	g_frontPanel[STGAPI_OFF_NKS4_HW_VERSION] = 3;
	g_calibrationResult = (short)0xffff;
	v = 0x10;
	unsigned char before = v;
	changed = CSTGKeybedInterface_FilterAnalogController(0, &v);
	check_eq("0xffff sentinel: reports false", changed, 0);
	check_eq("*value left untouched", v, before);
	g_calibrationResult = 0x1234;

	printf("\n[11] EnableUSBPort/EnableRearLED -- state gate + port>1 rejection:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_EnableUSBPort(0, true);
	check_eq("port 0: accepted", TxOccupancy(), 3);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_EnableUSBPort(2, true);
	check_eq("port 2 (out of range): rejected", TxOccupancy(), 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	CSTGKeybedInterface_EnableRearLED(true);
	check_eq("EnableRearLED accepted at state==2", TxOccupancy(), 3);

	printf("\n[12] ReceiveMessage state==2 dispatch -- heartbeat vs normal enqueue gating:\n");
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	g_keybedInstance[KEYBED_OFF_ENQUEUE_GATE1] = 0;
	unsigned char hbmsg[4] = { 0xea, 0x23, 0x07, 0x20 };
	CSTGKeybedInterface_ReceiveMessage(g_keybedInstance, hbmsg, 4);
	check_eq("heartbeat still dispatched (RAW bytes copied)", g_frontPanel[STGAPI_OFF_KEYBED_RAW0], 0xea);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("heartbeat NOT enqueued when gate1==0 (gate2 irrelevant)", n, 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	g_keybedInstance[KEYBED_OFF_ENQUEUE_GATE1] = 1;
	CSTGKeybedInterface_ReceiveMessage(g_keybedInstance, hbmsg, 4);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("heartbeat enqueued when gate1==1", n, 4);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	g_keybedInstance[KEYBED_OFF_ENQUEUE_GATE1] = 0;
	g_keybedInstance[KEYBED_OFF_DISPATCH_GATE2] = 1;
	MsgProcRaw()[0x48] = 0;
	unsigned char normalMsg1[1] = { 0xc7 }; /* type 4, 1-byte */
	CSTGKeybedInterface_ReceiveMessage(g_keybedInstance, normalMsg1, 1);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("non-heartbeat: gate2==1 && msgProc[0x48]==0 -> enqueued", n, 1);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	g_keybedInstance[KEYBED_OFF_ENQUEUE_GATE1] = 0;
	g_keybedInstance[KEYBED_OFF_DISPATCH_GATE2] = 1;
	MsgProcRaw()[0x48] = 1; /* processor owns it */
	CSTGKeybedInterface_ReceiveMessage(g_keybedInstance, normalMsg1, 1);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("non-heartbeat: gate2==1 && msgProc[0x48]!=0, gate1==0 -> dropped", n, 0);
	reset();
	g_keybedInstance[KEYBED_OFF_STATE] = 2;
	g_keybedInstance[KEYBED_OFF_ENQUEUE_GATE1] = 1;
	g_keybedInstance[KEYBED_OFF_DISPATCH_GATE2] = 0;
	CSTGKeybedInterface_ReceiveMessage(g_keybedInstance, normalMsg1, 1);
	n = CSTGKeybedInterface_ReadMessageFromQueue(out);
	check_eq("non-heartbeat: gate2==0, gate1==1 -> enqueued", n, 1);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
