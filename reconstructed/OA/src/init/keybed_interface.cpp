// SPDX-License-Identifier: GPL-2.0
/*
 * keybed_interface.cpp  -  CSTGKeybedInterface's ~20-method real
 * wire-protocol driver class (batch 64), following up on
 * keybed_init.cpp/keybed_receive.cpp/comport.cpp's own earlier
 * confirmed-real subset (Startup/Cleanup boot path, OnByteReceived,
 * ReceiveMessage's state==1 branch, CSTGComPort's own core methods).
 *
 * See oa_keybed_init.h for the full ground-truthing details (offsets,
 * gating semantics, per-method addresses/sizes, the free-function
 * modeling convention). Every method here is a faithful, instruction-
 * level reconstruction from a full objdump disassembly + relocation
 * trace of OA_real.ko (`.text+0x33d380`..`0x33e800`).
 *
 * NOT reconstructed in this pass (see oa_keybed_init.h's own comment on
 * `ApplyKeybedCalibration` and this file's own note below `Deferred`):
 * `CSTGKeybedInterface::ProcessNextKeybedEvent()` (`.text+0x33dcc0`,
 * 1712 bytes -- by far the largest method in this class). Genuinely
 * disproportionate new scope for this pass: a full disassembly trace
 * confirms real dependencies on FIVE entirely unmodeled
 * classes/functions elsewhere in OA.ko -- `PushUnsolicitedMessage`,
 * `CSTGDelayedMsg`, `CSTGControllerInfoUnsolMsg::Send()`,
 * `USTGKeyTouchTable::Convert9bitCountsTo8bitInterval()`, and
 * `CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult()` -- on top
 * of the already-deferred `ApplyKeybedCalibration`. It DOES reuse three
 * already-real methods from elsewhere in this project (`CSTGFrontPanel::
 * HandleAnalogController`, `CSTGKeybedKeyDebounceFilter::ProcessKeyOn`/
 * `ProcessKeyOff`, `CSTGKeybedKeyDebounceFilter::ProcessAPendingEvent`),
 * confirming this is the function that drives the debounce filter and
 * front-panel analog-controller path with real decoded keybed events --
 * but the 5-new-class gap is squarely the same "disproportionate to one
 * method" scope-decision precedent as `CSTGCX3RotaryV2::
 * GetFrontPanelRotaryStatus` (oa_front_panel_driver_cluster.md).
 */

#include "oa_comport.h"
#include "oa_keybed_init.h"
#include "oa_setup_global_resources.h" /* STGAPIFrontPanelStatus, CSTGKeybedInterface class */

static CSTGComPort *ComPort()
{
	return reinterpret_cast<CSTGComPort *>(CSTGKeybedInterface_sInstance());
}

/*
 * Confirmed real: every command-sending method below checks whether the
 * TX FIFO's occupancy now EXACTLY equals the number of bytes it just
 * queued (i.e. the FIFO was empty immediately before this call) before
 * triggering the UART TX interrupt -- if other bytes were already
 * queued, a previously-triggered interrupt's own service loop will pick
 * these up too, so triggering again would be redundant (not incorrect,
 * just confirmed-real behavior to preserve exactly).
 */
static inline unsigned char TxFifoOccupancy()
{
	return (unsigned char)(ComPort()->txFifo.head - ComPort()->txFifo.tail);
}

/* ------------------------------------------------------------------ */
/* SendCommand(unsigned char) / SendCommand(unsigned char const*, unsigned int) */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_SendCommandByte(unsigned char cmd)
{
	ComPort()->txFifo.WriteByte(cmd);
	if (TxFifoOccupancy() == 1)
		ComPort()->TriggerInterrupt();
}

void CSTGKeybedInterface_SendCommandBuf(const unsigned char *buf, unsigned int len)
{
	ComPort()->txFifo.WriteBytes(buf, len);
	if (TxFifoOccupancy() == (unsigned char)len)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* SendByte(unsigned char) -- gated (state==2 exactly) */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_SendByte(unsigned char value)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] != 2)
		return;
	ComPort()->txFifo.WriteByte(value);
	if (TxFifoOccupancy() == 1)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* CSTGKeybedInterface::SetLED -- real body, supersedes the deliberate
 * no-op stub bar2_stubs.cpp carried until this pass (front-panel driver
 * cluster, sec/batch 63). Kept as a real mangled-ABI class method (see
 * oa_setup_global_resources.h's own class declaration) since
 * CSTGFrontPanel::SetLED/SetLEDBlinking/ResetLED already call it by that
 * exact name. `this` is always the `CSTGKeybedInterface::sInstance` blob
 * in every confirmed real call site (front_panel_handlers.cpp's own
 * `reinterpret_cast<CSTGKeybedInterface*>(CSTGKeybedInterface_sInstance())`),
 * so reinterpreting `this` as the raw byte blob here is safe. */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface::SetLED(unsigned int code, unsigned int action)
{
	unsigned char *sInstance = reinterpret_cast<unsigned char *>(this);
	if (sInstance[KEYBED_OFF_STATE] <= 1)
		return; /* confirmed real: unlike EnterKeyCheckMode/SendByte,
			 * SetLED accepts state 2 AND the calibration states
			 * (3/4) -- only rejects 0/1. */

	unsigned char ledIndex;
	if (code == 0x49)
		ledIndex = 0;
	else if (code == 0x4a)
		ledIndex = 1;
	else
		return; /* confirmed real: any other eSTGLEDCode is silently ignored */

	unsigned char cmdByte = (unsigned char)((action & 0x2f) | 0xd0);
	unsigned char buf[2] = { cmdByte, ledIndex };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* Member Startup()/Cleanup() -- hardcoded-port-0 retry variant, DISTINCT
 * from the free CSTGKeybedInterface_Startup/_Cleanup boot-path pair
 * (keybed_init.cpp). See oa_keybed_init.h's own comment for the
 * confirmed real difference. Written with gotos mirroring the real
 * disassembly 1:1, matching keybed_init.cpp's own established style for
 * this exact algorithm shape. */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_MemberCleanup(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	ComPort()->Cleanup();
	sInstance[KEYBED_OFF_STATE] = 0;
}

int CSTGKeybedInterface_MemberStartup(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	int outerRetries = 10;
	int retryCount;
	int found;

	CSTGKeybedKeyDebounceFilter_Initialize(sInstance + KEYBED_OFF_DEBOUNCE_FILTER);

outer_retry:
	retryCount = 1;

inner_loop:
	sInstance[KEYBED_OFF_ACK_FLAG] = 0;
	if (!ComPort()->Initialize((CSTGComPort::eComPortId)0, (CSTGComPort::eBaudRateCode)0x18,
				    (CSTGComPort::eReceiveFifoThresholdCode)0))
		goto no_ack;

	ComPort()->txFifo.WriteByte(0xa5);
	sInstance[KEYBED_OFF_STATE] = 1;

	if (TxFifoOccupancy() == 1)
		ComPort()->TriggerInterrupt();

	__const_udelay(0x20c4ac);
	if (sInstance[KEYBED_OFF_ACK_FLAG])
		goto check_ack;
	{
		int delayRetries = 0x32;
		do {
			__const_udelay(0x68dbc);
			if (sInstance[KEYBED_OFF_ACK_FLAG])
				goto check_ack;
			delayRetries--;
		} while (delayRetries != 0);
	}

check_ack:
	if (sInstance[KEYBED_OFF_ACK_FLAG])
		goto ack_received;

no_ack:
	ComPort()->Cleanup();
	retryCount++;
	if (retryCount != 7)
		goto inner_loop;

	found = 0;
	outerRetries--;
	if (outerRetries == 0)
		goto shared_check;
	goto outer_retry;

ack_received:
	found = 1;
	outerRetries--;
	if (outerRetries == 0)
		goto shared_check;
	goto final_success;

shared_check:
	if (!found)
		goto final_failure;

final_success:
	sInstance[KEYBED_OFF_STATE] = 2;
	return 1;

final_failure:
	ComPort()->Cleanup();
	sInstance[KEYBED_OFF_STATE] = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* TryComPort(eComPortId) -- single-attempt probe on a specific port */
/* ------------------------------------------------------------------ */

int CSTGKeybedInterface_TryComPort(int comPortId)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	sInstance[KEYBED_OFF_ACK_FLAG] = 0;

	if (!ComPort()->Initialize((CSTGComPort::eComPortId)comPortId,
				    (CSTGComPort::eBaudRateCode)0x18,
				    (CSTGComPort::eReceiveFifoThresholdCode)0)) {
		ComPort()->Cleanup();
		return 0;
	}

	ComPort()->txFifo.WriteByte(0xa5);
	sInstance[KEYBED_OFF_STATE] = 1;
	if (TxFifoOccupancy() == 1) {
		ComPort()->TriggerInterrupt();
	} else {
		__const_udelay(0x20c4ac);
		if (!sInstance[KEYBED_OFF_ACK_FLAG]) {
			int retries = 0x32;
			do {
				__const_udelay(0x68dbc);
				if (sInstance[KEYBED_OFF_ACK_FLAG])
					break;
				retries--;
			} while (retries != 0);
		}
	}

	if (!sInstance[KEYBED_OFF_ACK_FLAG]) {
		ComPort()->Cleanup();
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------------------ */
/* Calibration control triad */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_StartCalibration(unsigned int controller)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] <= 1)
		return;
	sInstance[KEYBED_OFF_STATE] = 3;
	unsigned char buf[2] = { 0xc0, (unsigned char)controller };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

void CSTGKeybedInterface_EndCalibration(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] != 3)
		return;
	sInstance[KEYBED_OFF_STATE] = 4;
	unsigned char buf[2] = { 0xc1, 0x00 };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

void CSTGKeybedInterface_CancelCalibration(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] != 3)
		return;
	sInstance[KEYBED_OFF_STATE] = 4;
	unsigned char buf[2] = { 0xc2, 0x00 };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* Key-check mode */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_EnterKeyCheckMode(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] != 2)
		return;
	unsigned char buf[2] = { 0x90, 0x7f };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

void CSTGKeybedInterface_ExitKeyCheckMode(void)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] != 2)
		return;
	unsigned char buf[2] = { 0x80, 0x7f };
	ComPort()->txFifo.WriteBytes(buf, 2);
	if (TxFifoOccupancy() == 2)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* SetKeyChatterGateTime -- confirmed real bit-split encode; see
 * HARDWARE_REVIEW_LOG.md for the caveat on this encoding's semantic
 * meaning. */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_SetKeyChatterGateTime(unsigned int ms)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] <= 1)
		return;

	unsigned char byte1, byte2;
	if (ms <= 0x3d) {
		/* Confirmed real bit split: byte1 = bit1 of ms, byte2 = ms>>2.
		 * See HARDWARE_REVIEW_LOG.md -- exact bit operations preserved
		 * faithfully; the hardware-register-level MEANING of this
		 * split is not independently confirmed. */
		byte1 = (unsigned char)((ms >> 1) & 1);
		byte2 = (unsigned char)(ms >> 2);
	} else {
		/* Clamp: confirmed real fixed values for any ms > 61. */
		byte1 = 1;
		byte2 = 0xf;
	}

	unsigned char buf[3] = { 0xb4, byte1, byte2 };
	ComPort()->txFifo.WriteBytes(buf, 3);
	if (TxFifoOccupancy() == 3)
		ComPort()->TriggerInterrupt();
}

/* ------------------------------------------------------------------ */
/* WriteMessageToQueue / ReadMessageFromQueue -- raw ring-buffer pair */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_WriteMessageToQueue(const unsigned char *buf, unsigned int len)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	unsigned short writePos =
		*(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_WRITE);
	unsigned short readPos = *(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_READ);
	unsigned short queuedLen = (unsigned short)(writePos - readPos);

	if ((unsigned int)queuedLen + len > 0xff)
		return; /* confirmed real: entire message dropped, no partial write */

	for (unsigned int i = 0; i < len; i++) {
		unsigned char idx = (unsigned char)(writePos + i);
		sInstance[KEYBED_OFF_MSG_QUEUE_BUF + idx] = buf[i];
	}
	*(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_WRITE) =
		(unsigned short)(writePos + len);
}

unsigned char CSTGKeybedInterface_ReadMessageFromQueue(unsigned char *outBuf)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	unsigned short writePos =
		*(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_WRITE);
	unsigned short readPos = *(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_READ);

	if (writePos == readPos)
		return 0; /* empty */

	unsigned char headerByte = sInstance[KEYBED_OFF_MSG_QUEUE_BUF + (unsigned char)readPos];
	unsigned char numBytes = CSTGKeybedComPort::GetNumBytesForMessageType(headerByte);

	for (unsigned int i = 0; i < numBytes; i++) {
		unsigned char idx = (unsigned char)(readPos + i);
		outBuf[i] = sInstance[KEYBED_OFF_MSG_QUEUE_BUF + idx];
	}
	*(unsigned short *)(sInstance + KEYBED_OFF_MSG_QUEUE_READ) =
		(unsigned short)(readPos + numBytes);
	return numBytes;
}

/* ------------------------------------------------------------------ */
/* ReceiveStartupMessage -- ungated handshake-ACK subset of ReceiveMessage */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_ReceiveStartupMessage(const unsigned char *buf)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if ((buf[0] & 0xf0) != 0xa0)
		return;
	unsigned short status16 = ((unsigned short)buf[1] << 8) | buf[2];
	*(unsigned short *)(STGAPIFrontPanelStatus::sInstance + STGAPI_OFF_KEYBED_STATUS16) =
		status16;
	sInstance[KEYBED_OFF_ACK_FLAG] = 1;
}

/* ------------------------------------------------------------------ */
/* HandleActiveSense -- the standalone idle-heartbeat ("type 6",
 * 0xE0-0xEF) dispatch; ReceiveMessage's own state==2 branch calls this
 * directly for the same header class. */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_HandleActiveSense(const unsigned char *buf)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	unsigned char *status = STGAPIFrontPanelStatus::sInstance;

	status[STGAPI_OFF_KEYBED_RAW0] = buf[0];
	status[STGAPI_OFF_KEYBED_RAW1] = buf[1];
	status[STGAPI_OFF_KEYBED_RAW2] = buf[2];
	status[STGAPI_OFF_KEYBED_RAW3] = buf[3];

	unsigned int nibble = buf[0] & 0xf;
	switch (nibble) {
	case 9:
		status[STGAPI_OFF_FOOTSWITCH0] = 0x24;
		status[STGAPI_OFF_FOOTSWITCH1] = 0x3d;
		status[STGAPI_OFF_NKS4_PANEL_KIND] = 1; /* +0x29125, confirmed real */
		break;
	case 8:
		status[STGAPI_OFF_FOOTSWITCH0] = 0x24;
		status[STGAPI_OFF_FOOTSWITCH1] = 0x3d;
		break;
	case 0xa:
		status[STGAPI_OFF_PANEL_DETECTED] = 1;
		sInstance[KEYBED_OFF_DEBOUNCE_FILTER] = 1; /* debounce filter's own +0x0 byte */
		break;
	case 0xd:
		status[STGAPI_OFF_KEYBED_NIBBLE_D_FLAG] = 1;
		break;
	default:
		break; /* confirmed real no-op for every other nibble */
	}
}

/* ------------------------------------------------------------------ */
/* ApplyAftertouchTable / ApplyCalibrationAndAfterTouchTable -- 3
 * confirmed real 256-byte per-keybed-model curve tables, extracted
 * verbatim from OA_real.ko's own `.rodata+0xa8600`/`0xa8700`/`0xa8800`. */
/* ------------------------------------------------------------------ */

static const unsigned char kAftertouchTable0[256] = {
#include "keybed_aftertouch_table0.inc"
};
static const unsigned char kAftertouchTable1[256] = {
#include "keybed_aftertouch_table1.inc"
};
static const unsigned char kAftertouchTable2[256] = {
#include "keybed_aftertouch_table2.inc"
};

unsigned char CSTGKeybedInterface_ApplyAftertouchTable(unsigned char raw)
{
	unsigned char mode = STGAPIFrontPanelStatus::sInstance[STGAPI_OFF_NKS4_PANEL_KIND];
	if (mode == 0)
		return kAftertouchTable0[raw];
	if (mode == 1)
		return kAftertouchTable1[raw];
	return kAftertouchTable2[raw]; /* mode >= 2 */
}

/* Confirmed real fixed-point "divide by 255, round toward zero" idiom
 * (GCC's canonical 0x80808081 reciprocal for unsigned /255), used by
 * both ApplyCalibrationAndAfterTouchTable and FilterAnalogController
 * below to rescale an 8-bit table/raw value up to a ~0-1023 range. */
static short ScaleByteToWideRange(unsigned char value)
{
	int scaled = ((int)value * 1023) + 0x7f;
	int divided = (int)(((long long)scaled * 0x80808081LL) >> 32);
	divided += scaled;
	int roundBit = scaled >> 31;
	divided >>= 7;
	divided -= roundBit;
	return (short)divided;
}

short CSTGKeybedInterface_ApplyCalibrationAndAfterTouchTable(short rawAnalog)
{
	short calibrated = ApplyKeybedCalibration(7, rawAnalog);
	unsigned char scaled = (unsigned char)((unsigned short)calibrated >> 2);

	unsigned char mode = STGAPIFrontPanelStatus::sInstance[STGAPI_OFF_NKS4_PANEL_KIND];
	unsigned char tableVal;
	if (mode == 0)
		tableVal = kAftertouchTable0[scaled];
	else if (mode == 1)
		tableVal = kAftertouchTable1[scaled];
	else
		tableVal = kAftertouchTable2[scaled];

	if (tableVal == 0x80)
		return 0x200; /* confirmed real: centered sentinel maps to 512, no rescale */
	return ScaleByteToWideRange(tableVal);
}

/* ------------------------------------------------------------------ */
/* FilterAnalogController */
/* ------------------------------------------------------------------ */

bool CSTGKeybedInterface_FilterAnalogController(unsigned int code, unsigned char *value)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	bool changed = true;

	if (code == 1) {
		unsigned char *flag = &sInstance[KEYBED_OFF_ANALOG1_CHANGED_FLAG];
		if (*value == 0x80)
			changed = (*flag != 0);
		*flag = 1;
	} else if (code == 2) {
		unsigned char *flag = &sInstance[KEYBED_OFF_ANALOG2_CHANGED_FLAG];
		if (*value == 0x80)
			changed = (*flag != 0);
		*flag = 1;
	}
	/* code == 0 (aftertouch pressure): `changed` stays true unconditionally. */

	if (!changed)
		return false;

	if (STGAPIFrontPanelStatus::sInstance[STGAPI_OFF_NKS4_HW_VERSION] != 3)
		return true; /* confirmed real: "not ready" bail, no filtering done,
			      * *value left untouched, still reports true */

	short scaled;
	unsigned char raw = *value;
	if (raw == 0x80)
		scaled = 0x200;
	else
		scaled = ScaleByteToWideRange(raw);

	short result = ApplyKeybedCalibration((int)code, scaled);
	if (result == (short)0xffff)
		return false; /* confirmed real: no calibration data, *value untouched */

	*value = (unsigned char)((unsigned short)result >> 2);
	return true;
}

/* ------------------------------------------------------------------ */
/* EnableUSBPort / EnableRearLED */
/* ------------------------------------------------------------------ */

void CSTGKeybedInterface_EnableUSBPort(int port, bool enable)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] <= 1)
		return;
	if (port > 1)
		return; /* confirmed real: 2-port hardware, silently rejects the rest */
	unsigned char buf[3] = { 0xb8, (unsigned char)port, (unsigned char)enable };
	ComPort()->txFifo.WriteBytes(buf, 3);
	if (TxFifoOccupancy() == 3)
		ComPort()->TriggerInterrupt();
}

void CSTGKeybedInterface_EnableRearLED(bool enable)
{
	unsigned char *sInstance = CSTGKeybedInterface_sInstance();
	if (sInstance[KEYBED_OFF_STATE] <= 1)
		return;
	unsigned char buf[3] = { 0xb9, 0x00, (unsigned char)enable };
	ComPort()->txFifo.WriteBytes(buf, 3);
	if (TxFifoOccupancy() == 3)
		ComPort()->TriggerInterrupt();
}
