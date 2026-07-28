// SPDX-License-Identifier: GPL-2.0
/*
 * ckg_ui_msg_sender.cpp  -  CKGUIMsgSender, the KARMA UI-notification
 * send-side helper. See oa_ckg_control_ui_msg.h for the full family
 * writeup and field derivations.
 *
 * Every method builds a fixed-shape CSKMessage on the stack and hands it
 * to KGOutGate_SendMessageToUI(). Two message shapes are used (see
 * header): a "ParameterChangeMessage" shape (build*ParamMessage() below)
 * shared by the 11 Common/ModuleParam-forwarding methods, and a smaller
 * "UI action" shape (buildUiActionMessage()) shared by everything else.
 * Both are written via explicit byte offsets rather than a named-field
 * struct -- see the header comment for why.
 */

#include "oa_ckg_control_ui_msg.h"

/* No libc in this freestanding kernel build (matches project convention
 * elsewhere in this tree -- no other file under src/ calls memset()/
 * memcpy() as real code either); tiny local helpers instead of
 * <string.h>. */
static void oa_zero(unsigned char *p, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		p[i] = 0;
}
static void oa_copy(unsigned char *dst, const unsigned char *src, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		dst[i] = src[i];
}

bool CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange;

CKGUIMsgSender::CKGUIMsgSender()
{
	/* .text+0x3c84e0, 1 byte -- confirmed real no-op (bare ret); this
	 * class is stateless. */
}

/*
 * Shared send-gate for the ParameterChangeMessage shape, factored out of
 * the byte-identical tail every one of the 11 (a)-family methods repeats
 * (see header comment (a)): operation==5 (Dim) always sends with
 * immediate=true, unconditionally; any other operation sends with
 * immediate=false, but ONLY if CSKSpecialMsgHandler's own sampling-
 * change flag is clear AND none of CKGControlMsgHandler's 3 "now
 * dumping" guards are set -- otherwise the message is silently dropped.
 */
static void sendParamChangeMessage(const CSKMessage *msg, int operation)
{
	if (operation == 5) {
		KGOutGate_SendMessageToUI(msg, true);
		return;
	}
	if (CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange)
		return;
	if (CKGControlMsgHandler::ms_bIsNowDumpingSong ||
	    CKGControlMsgHandler::ms_bIsNowDumpingCombi ||
	    CKGControlMsgHandler::ms_bIsNowDumpingProg)
		return;
	KGOutGate_SendMessageToUI(msg, false);
}

/* Common-param shape (0x24 bytes): +0x00 u16 class=0x24, +0x02 u16
 * subtype=5, +0x04 i32=2 (constant), +0x08 i32 msgId, +0x0c void*
 * engine-field, +0x10 i32=0, +0x14 i32=0xffff, +0x18 i32 value,
 * +0x1c i32 operation, +0x20 i32 trailing arg. */
static void buildCommonParamMessage(CSKMessage *msg, int msgId, long value,
				     int operation, long trailingArg)
{
	oa_zero(msg->raw, sizeof(msg->raw));
	unsigned char *p = msg->raw;
	*(unsigned short *)(p + 0x00) = 0x24;
	*(unsigned short *)(p + 0x02) = 0x5;
	*(int *)(p + 0x04) = 2;
	*(int *)(p + 0x08) = msgId;
	*(void **)(p + 0x0c) = *(void **)(CKGEngine::ms_poInstance + 0xa0);
	*(int *)(p + 0x10) = 0;
	*(int *)(p + 0x14) = 0xffff;
	*(int *)(p + 0x18) = (int)value;
	*(int *)(p + 0x1c) = operation;
	*(int *)(p + 0x20) = (int)trailingArg;
}

/* Module-param shape (0x28 bytes), the {SetModuleParamMax,
 * SetModuleParamMin,UpdateModuleParam,DimOnModuleParam} variant (all 4
 * confirmed byte-identical in field placement, independently
 * disassembled): class=0x28, constant=3, +0x18 deviceIndex, +0x1c
 * value, +0x20 operation, +0x24 trailing 4th real param (arg4/dim --
 * real, always written, but never read back by anything in this batch).
 * SendModuleParamMessage does NOT share this exact layout (see its own
 * bespoke body below -- value lands at +0x24 there instead, with +0x1c
 * an unwritten gap; a genuine, confirmed ground-truth difference, not a
 * transcription slip). */
static void buildModuleParamMessage(CSKMessage *msg, int msgId, long deviceIndex,
				     long value, int operation, long trailingArg)
{
	oa_zero(msg->raw, sizeof(msg->raw));
	unsigned char *p = msg->raw;
	*(unsigned short *)(p + 0x00) = 0x28;
	*(unsigned short *)(p + 0x02) = 0x5;
	*(int *)(p + 0x04) = 3;
	*(int *)(p + 0x08) = msgId;
	*(void **)(p + 0x0c) = *(void **)(CKGEngine::ms_poInstance + 0xa0);
	*(int *)(p + 0x10) = 0;
	*(int *)(p + 0x14) = 0xffff;
	*(int *)(p + 0x18) = (int)deviceIndex;
	*(int *)(p + 0x1c) = (int)value;
	*(int *)(p + 0x20) = operation;
	*(int *)(p + 0x24) = (int)trailingArg;
}

/* .text+0x3c84f0, 169 bytes */
void CKGUIMsgSender::SendCommonParamMessage(int msgId, long value, int operation, long arg4)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, operation, arg4);
	sendParamChangeMessage(&msg, operation);
}

/* .text+0x3c85b0, 176 bytes -- bespoke layout (does NOT reuse
 * buildModuleParamMessage, see its own comment): value lands at +0x24
 * here (not +0x1c), and +0x1c is an unwritten gap. arg5 (the function's
 * 5th real parameter, `[ebp+0x10]`) is never written into the message
 * at all in ground truth. */
void CKGUIMsgSender::SendModuleParamMessage(int msgId, long deviceIndex, long value,
					     int operation, long /*arg5*/)
{
	CSKMessage msg;
	oa_zero(msg.raw, sizeof(msg.raw));
	unsigned char *p = msg.raw;
	*(unsigned short *)(p + 0x00) = 0x28;
	*(unsigned short *)(p + 0x02) = 0x5;
	*(int *)(p + 0x04) = 3;
	*(int *)(p + 0x08) = msgId;
	*(void **)(p + 0x0c) = *(void **)(CKGEngine::ms_poInstance + 0xa0);
	*(int *)(p + 0x10) = 0;
	*(int *)(p + 0x14) = 0xffff;
	*(int *)(p + 0x18) = (int)deviceIndex;
	*(int *)(p + 0x20) = operation;
	*(int *)(p + 0x24) = (int)value;
	sendParamChangeMessage(&msg, operation);
}

/* .text+0x3c8670, 138 bytes -- operation literal 0. */
void CKGUIMsgSender::UpdateCommonParam(int msgId, long value, long arg3, bool /*dummy*/)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, 0, arg3);
	sendParamChangeMessage(&msg, 0);
}

/* .text+0x3c8700, 138 bytes -- operation literal 3. */
void CKGUIMsgSender::SetCommonParamMax(int msgId, long value, long arg3)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, 3, arg3);
	sendParamChangeMessage(&msg, 3);
}

/* .text+0x3c8790, 138 bytes -- operation literal 2. */
void CKGUIMsgSender::SetCommonParamMin(int msgId, long value, long arg3)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, 2, arg3);
	sendParamChangeMessage(&msg, 2);
}

/* .text+0x3c8820, 138 bytes -- operation literal 4, no 3rd real param
 * (trailing field written 0). */
void CKGUIMsgSender::RefreshCommonParam(int msgId, long value)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, 4, 0);
	sendParamChangeMessage(&msg, 4);
}

/* .text+0x3c88b0, 149 bytes -- operation literal 5 (always immediate). */
void CKGUIMsgSender::DimOnCommonParam(int msgId, long value, bool dim)
{
	CSKMessage msg;
	buildCommonParamMessage(&msg, msgId, value, 5, dim);
	sendParamChangeMessage(&msg, 5);
}

/* .text+0x3c89f0, 146 bytes -- operation literal 3; value at +0x1c,
 * arg4 (4th real param) written to the +0x24 trailing slot (real, but
 * nothing downstream reads it back). */
void CKGUIMsgSender::SetModuleParamMax(int msgId, long deviceIndex, long value, long arg4)
{
	CSKMessage msg;
	buildModuleParamMessage(&msg, msgId, deviceIndex, value, 3, arg4);
	sendParamChangeMessage(&msg, 3);
}

/* .text+0x3c8a90, 146 bytes -- operation literal 2. */
void CKGUIMsgSender::SetModuleParamMin(int msgId, long deviceIndex, long value, long arg4)
{
	CSKMessage msg;
	buildModuleParamMessage(&msg, msgId, deviceIndex, value, 2, arg4);
	sendParamChangeMessage(&msg, 2);
}

/* .text+0x3c8b30, 156 bytes -- operation literal 0; arg4 written to the
 * +0x24 trailing slot; the 5th real param (`dummy`) never read at all. */
void CKGUIMsgSender::UpdateModuleParam(int msgId, long deviceIndex, long value,
					long arg4, bool /*dummy*/)
{
	CSKMessage msg;
	buildModuleParamMessage(&msg, msgId, deviceIndex, value, 0, arg4);
	sendParamChangeMessage(&msg, 0);
}

/* .text+0x3c8950, 156 bytes -- operation literal 5 (always immediate);
 * `value` (the real 3rd param) goes to its usual +0x1c slot, `dim` goes
 * to the +0x24 trailing slot (zero-extended from its own byte read) --
 * confirmed via direct disassembly, NOT the same shape as
 * DimOnCommonParam (which reuses its trailing slot for `dim` and keeps
 * `value` in the shape's normal value slot too, but Common's normal
 * value slot and Module's differ in absolute offset). */
void CKGUIMsgSender::DimOnModuleParam(int msgId, long deviceIndex, long value, bool dim)
{
	CSKMessage msg;
	buildModuleParamMessage(&msg, msgId, deviceIndex, value, 5, dim);
	sendParamChangeMessage(&msg, 5);
}

/*
 * --- "UI action" shape (family (b)): fixed class 0x14, always sent
 * unconditionally with immediate=false. Common layout: +0x00 u16
 * class=0x14, +0x02 u16 subtype=5, +0x04 i32=0 (constant), +0x08 i32
 * sub-opcode (a per-method literal), then +0xc/+0x10 hold EITHER the
 * CKGEngine::ms_poInstance[+0xa0] pointer followed by the method's own
 * single real parameter (when the method touches CKGEngine), OR the
 * method's own 1-2 real parameters directly with no engine pointer at
 * all (when it doesn't) -- confirmed individually per method below, not
 * assumed from a naming pattern. buildUiAction() takes the already-
 * decided values for +0xc/+0x10 so each method body just supplies them.
 */
static void buildUiAction(unsigned char *buf, int subOpcode, int field0xc, int field0x10)
{
	oa_zero(buf, 0x14);
	*(unsigned short *)(buf + 0x00) = 0x14;
	*(unsigned short *)(buf + 0x02) = 0x5;
	*(int *)(buf + 0x04) = 0;
	*(int *)(buf + 0x08) = subOpcode;
	*(int *)(buf + 0x0c) = field0xc;
	*(int *)(buf + 0x10) = field0x10;
}

static void sendUiAction(unsigned char *p)
{
	CSKMessage msg;
	oa_copy(msg.raw, p, 0x14);
	KGOutGate_SendMessageToUI(&msg, false);
}

static int engineField()
{
	return *(int *)(CKGEngine::ms_poInstance + 0xa0);
}

/* Each of the following builds the 0x14-byte shape above and sends it.
 * Sub-opcode literals and the +0xc/+0x10 field contents are transcribed
 * individually per ground-truth disassembly -- see each function's own
 * comment for whether it carries the CKGEngine pointer or its own
 * parameter(s) directly. */

/* .text+0x3c8bd0, engineField+on */
void CKGUIMsgSender::UpdateChordAssignLED(bool on)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x10, engineField(), (int)on);
	sendUiAction(buf);
}

/* .text+0x3c8c20, engineField+geIndex */
void CKGUIMsgSender::ChangeGE(long geIndex)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x11, engineField(), (int)geIndex);
	sendUiAction(buf);
}

/* .text+0x3c8c70, engineField+dummy */
void CKGUIMsgSender::ChangePerformance(bool dummy)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x12, engineField(), (int)dummy);
	sendUiAction(buf);
}

/* .text+0x3c8cc0, engineField+action */
void CKGUIMsgSender::UpdateDynamicMIDIAction(long action)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x13, engineField(), (int)action);
	sendUiAction(buf);
}

/* .text+0x3c8d10, engineField+arg */
void CKGUIMsgSender::ResetValuesInControlBuffer(int arg)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x14, engineField(), arg);
	sendUiAction(buf);
}

/* .text+0x3c8d60, engineField, no param */
void CKGUIMsgSender::ResetCurrentScene()
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x15, engineField(), 0);
	sendUiAction(buf);
}

/* .text+0x3c8db0, engineField+arg */
void CKGUIMsgSender::UpdateNoteMapTable(long arg)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x17, engineField(), (int)arg);
	sendUiAction(buf);
}

/* .text+0x3c8e00, engineField, no param */
void CKGUIMsgSender::UpdateGERTParamNames()
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x18, engineField(), 0);
	sendUiAction(buf);
}

/* .text+0x3c8e50 -- variant sub-shape: no engine-field pointer at all,
 * both +0xc/+0x10 are literal zero. */
void CKGUIMsgSender::UpdateKarmaInitialInfo()
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x6, 0, 0);
	sendUiAction(buf);
}

/* .text+0x3c8ea0, no engineField: a,b directly */
void CKGUIMsgSender::NotifyPadStatus(int a, int b)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x1a, a, b);
	sendUiAction(buf);
}

/* .text+0x3c8ee0, no engineField: a,b directly */
void CKGUIMsgSender::UpdateNoteMapSmallTable(int a, long b)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x1b, a, (int)b);
	sendUiAction(buf);
}

/* .text+0x3c8f20 -- same no-engineField variant as UpdateKarmaInitialInfo. */
void CKGUIMsgSender::UpdateRTCModelName()
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0xf, 0, 0);
	sendUiAction(buf);
}

/* .text+0x3c8f70, no engineField: on at +0xc, +0x10 unused */
void CKGUIMsgSender::UpdateAutoClockSource(bool on)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x1c, (int)on, 0);
	sendUiAction(buf);
}

/* .text+0x3c8fc0, no engineField: caption at +0xc */
void CKGUIMsgSender::UpdateSceneChangeCaption(int caption)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x21, caption, 0);
	sendUiAction(buf);
}

/* .text+0x3c9010, engineField, no param */
void CKGUIMsgSender::UpdateValuesForScene()
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x22, engineField(), 0);
	sendUiAction(buf);
}

/* .text+0x3c9060, no engineField: a,b directly */
void CKGUIMsgSender::UpdateGEInfo(int a, int b)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x23, a, b);
	sendUiAction(buf);
}

/* .text+0x3c90a0, no engineField: on at +0xc */
void CKGUIMsgSender::UpdateSoftPedalStatus(bool on)
{
	unsigned char buf[0x14];
	buildUiAction(buf, 0x27, (int)on, 0);
	sendUiAction(buf);
}
