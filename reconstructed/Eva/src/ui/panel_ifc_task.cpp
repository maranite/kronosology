/*
 * panel_ifc_task.cpp  -  see include/panel_ifc_task.h.
 *
 * Ctor transcribed from Decomp/EVA_Decomp/eva_export/functions/
 * CPanelIfcTask@0824b7e0.c, cross-checked against direct `objdump -dr` (the
 * ctor's own real field-write order, and the `sInstance` global's real name,
 * came from the raw disassembly -- Ghidra's own decompile rendered it as an
 * anonymous `DAT_0939c314` write). Dtor from _CPanelIfcTask@0824b3b0.c (the D1
 * complete-object destructor -- @0824b3e0.c is the D0 deleting-dtor variant,
 * not separately modeled, see header). Every instance method transcribed from
 * its own eponymous eva_export/functions/ .c file (addresses in each method's
 * own header-comment above its declaration, panel_ifc_task.h) with
 * OnButtonEvent/OnTouchPanelEvent's own message-buffer layout additionally
 * cross-checked against direct `objdump -dr` (Ghidra's own local-variable
 * splitting for these two obscured the real, single 20-byte contiguous
 * PegMessage buffer).
 */

#include "panel_ifc_task.h"
#include "omega_vtables.h"
#include "stg_unsol_msg_handler.h"

#include <cstdlib>
#include <new>

unsigned char CEditor::CPanelIfcTask::sm_aucTouchPanelMargin[4];
CEditor::CPanelIfcTask *CEditor::CPanelIfcTask::sInstance = 0;

namespace {

/* Real ground-truth external dependency: `PegMessageQueue::Push(PegThing::
 * mpMessageQueue, PegMessage const*)` (.text+0x081a8750) -- PegMessageQueue/
 * PegThing are the one remaining Peg-toolkit gap this whole project has
 * consistently left unmodeled (editor.h's own header comment). Modeled here as
 * an inert, install-only stand-in -- never actually delivers the message
 * anywhere -- purely so OnTouchPanelEvent/OnButtonEvent/Exec(CMessage&)'s own
 * real field-touching side effects can be transcribed faithfully without
 * fabricating Peg's own toolkit. Same EvaVTableStub-style convention as every
 * other genuinely undecoded external call in this project.
 */
void *PegThing_mpMessageQueue = 0;

void PegMessageQueuePush(void *queue, const void *msg)
{
	(void)queue;
	(void)msg;
}

} // namespace

/* CTask's own test-only placeholder default ctor (task.h) -- not ground truth. */
CEditor::CPanelIfcTask::CPanelIfcTask()
	: mCfgLink(0), mLastQueryFlags(0), mReserved84(0), mUnknown88(0), mUnknown8c(0),
	  mUnknown90(0), mUnknown94(0), mScreen(0), mDiagMode(0), mLedAllState(0),
	  mBlinkEnabled(0), mTouchActive(0), mTouchX(0), mTouchY(0), mTouchGen(0)
{
}

/* REAL, full ctor (CPanelIfcTask@0824b7e0.c). Real ground truth's own
 * HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets around each malloc
 * are dropped -- same established "kernel-side critical-section shim,
 * no-op-and-dropped userspace concern" precedent as task.cpp/module.cpp/
 * out_link.cpp.
 */
CEditor::CPanelIfcTask::CPanelIfcTask(CEditor *owner, void *screen)
	: CTask(*owner, "PanelIfcTask", 3, 1, 0x804b),
	  mCfgLink(0), mLastQueryFlags(0), mReserved84(0), mUnknown88(0), mUnknown8c(0),
	  mUnknown90(0x100), mUnknown94(0x100), mScreen(screen), mDiagMode(0),
	  mLedAllState(0), mBlinkEnabled(0), mTouchActive(0), mTouchX(-1), mTouchY(-1),
	  mTouchGen(0)
	  /* mBlinkCounter (+0xa8) deliberately NOT initialized here -- matches
	   * ground truth, see header comment. */
{
	unsigned char *self = reinterpret_cast<unsigned char *>(this);
	*reinterpret_cast<void **>(self)     = (void *)PTR__CPanelIfcTask_08f29ce8;
	*reinterpret_cast<void **>(self + 8) = (void *)&EvaDataPlaceholder_08f29d04;

	void *cfgRaw = malloc(0x3c);
	CPanelCfg *cfg = new (cfgRaw) CPanelCfg(*this, "PanelCfg", 0, 0x804b);
	mCfgLink = cfg;

	Add(cfg);

	sInstance = this;

	void *unsolRaw = malloc(0x98);
	new (unsolRaw) CSTGUnsolMsgHandler(this);
}

/* REAL D1 (complete-object) destructor -- CTask::~CTask() runs automatically as
 * the base-class dtor once this body returns, matching ground truth's own
 * tail-jump. Ground truth does NOT destroy mCfgLink or the CSTGUnsolMsgHandler
 * the ctor allocated -- transcribed as found, not "completed".
 */
CEditor::CPanelIfcTask::~CPanelIfcTask()
{
	unsigned char *self = reinterpret_cast<unsigned char *>(this);
	*reinterpret_cast<void **>(self)     = (void *)PTR__CPanelIfcTask_08f29ce8;
	*reinterpret_cast<void **>(self + 8) = (void *)&EvaDataPlaceholder_08f29d04;
}

void CEditor::CPanelIfcTask::SetMargin(EMargin which, unsigned char value)
{
	if (value < 0x32)
		sm_aucTouchPanelMargin[which] = value;
}

/* Real body has no bounds check on the read side (unlike SetMargin's write-side
 * check) -- preserved as found, not "fixed" to match.
 */
unsigned char CEditor::CPanelIfcTask::GetMargin(EMargin which)
{
	return sm_aucTouchPanelMargin[which];
}

void CEditor::CPanelIfcTask::SetLEDStatus(int ledCode, int ledState)
{
	int buf[2];
	buf[0] = ledCode;
	buf[1] = ledState;
	mCfgLink->mLastResult = mCfgLink->OutMono(0, buf, 8);
}

void CEditor::CPanelIfcTask::SetLEDStatus(int deviceIndex, unsigned short onMask,
                                          unsigned short offMask)
{
	int buf[2];
	buf[0] = deviceIndex;
	buf[1] = (static_cast<int>(onMask) << 16) | offMask; /* real: CONCAT22(onMask, offMask) */
	mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
}

void CEditor::CPanelIfcTask::SetLEDStatus(int ledState)
{
	if (ledState == 1) {
		mLedAllState = 1;
		for (int i = 0; i < 0x20; ++i) {
			int buf[2];
			buf[0] = i;
			buf[1] = -1;
			mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
		}
	} else if (ledState == 2) {
		mLedAllState = 1;
		for (int i = 0; i < 0x20; ++i) {
			int buf[2];
			buf[0] = i;
			buf[1] = -1;
			mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
		}
		mBlinkEnabled = 1;
		mBlinkCounter = 0;
		return; /* real: this branch returns before the mBlinkEnabled=0 tail below */
	} else if (ledState != 0) {
		return;
	} else {
		mLedAllState = 0;
		for (int i = 0; i < 0x20; ++i) {
			int buf[2];
			buf[0] = i;
			buf[1] = 0xffff;
			mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
		}
	}
	mBlinkEnabled = 0;
}

void CEditor::CPanelIfcTask::ShortBeep()
{
	mCfgLink->mLastResult = mCfgLink->OutMono(2, 0);
}

void CEditor::CPanelIfcTask::EnterDiagnostics(int flag)
{
	mDiagMode = flag;
	mCfgLink->mLastResult = mCfgLink->OutMono(3, static_cast<unsigned long>(flag));
}

void CEditor::CPanelIfcTask::SetupPanelInterface()
{
	/* Real: reads CTask's own private mName (+0x4)/mOwnerModule (+0x3c), and
	 * CModule's own private mName (+0x4) through that -- raw offset access
	 * across the class boundary, same "treat objects as raw blobs" idiom as
	 * sysex_msg_task_base.cpp's own mMask read (task.h/module.h note no
	 * friend declaration was added for this).
	 */
	const unsigned char *self = reinterpret_cast<const unsigned char *>(this);
	const unsigned char *ownerModule = *reinterpret_cast<unsigned char * const *>(self + 0x3c);
	const char *ownerModuleName = *reinterpret_cast<char * const *>(ownerModule + 4);
	const char *taskName = *reinterpret_cast<char * const *>(self + 4);

	unsigned int query[3];
	query[0] = 0xffffffff;
	query[1] = reinterpret_cast<unsigned long>(ownerModuleName);
	query[2] = reinterpret_cast<unsigned long>(taskName);

	mCfgLink->mLastResult = mCfgLink->OutMono(5, query, 0xc);

	mLastQueryFlags = query[0];
	mCfgLink->mLastResult = mCfgLink->OutMono(0xc, mLastQueryFlags);
	mCfgLink->mLastResult = mCfgLink->OutMono(0xe, mLastQueryFlags);
}

void CEditor::CPanelIfcTask::SetAllLED(int state)
{
	mLedAllState = state;
	int mask = (state == 0) ? 0xffff : -1;
	for (int i = 0; i < 0x20; ++i) {
		int buf[2];
		buf[0] = i;
		buf[1] = mask;
		mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
	}
}

void CEditor::CPanelIfcTask::OnTouchPanelEvent(const CPanelOut::STouchPanelEvt *evt)
{
	struct PanelTouchMsg {
		unsigned short cmd;
		unsigned short pad;
		unsigned int   z1, z2, z3;
		short          x, y;
	} msg;
	msg.pad = 0;
	msg.z1 = 0;
	msg.z2 = 0;
	msg.z3 = 0;

	const unsigned char *raw = reinterpret_cast<const unsigned char *>(evt);
	int kind = *reinterpret_cast<const int *>(raw); /* evt+0x0 */

	if (kind == 1) {
		mTouchActive = 1;
		mTouchGen = 0;
		msg.cmd = 10;
	} else if (kind == 2) {
		if (mTouchGen != 0)
			return;
		msg.cmd = 9;
	} else {
		if (kind != 0)
			return;
		mTouchActive = 0;
		msg.cmd = 0xb;
	}

	/* Real: unconditional dereference of mScreen's own +0x14/+0x16 fields --
	 * a genuine, transcribed null-deref hazard while mScreen stays null (see
	 * header comment), same as Exec()'s own +0x10c virtual tick below.
	 */
	const unsigned char *screen = reinterpret_cast<const unsigned char *>(mScreen);
	int bx = raw[4];
	int by = raw[5];
	int m0 = sm_aucTouchPanelMargin[0];
	int m1 = sm_aucTouchPanelMargin[1];
	int m2 = sm_aucTouchPanelMargin[2];
	int m3 = sm_aucTouchPanelMargin[3];
	short screenW = *reinterpret_cast<const short *>(screen + 0x14);
	short screenH = *reinterpret_cast<const short *>(screen + 0x16);

	short x = static_cast<short>(((bx - m0) * screenW) / (0x100 - m0 - m2));
	short y = static_cast<short>(((by - m1) * screenH) / (0x100 - m1 - m3));
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	msg.x = x;
	msg.y = y;

	PegMessageQueuePush(PegThing_mpMessageQueue, &msg);

	if (msg.cmd == 10) {
		mTouchX = msg.x;
		mTouchY = msg.y;
		msg.cmd = 9;
		PegMessageQueuePush(PegThing_mpMessageQueue, &msg);
	}
}

void CEditor::CPanelIfcTask::OnButtonEvent(const CPanelOut::SButtonEvt *evt)
{
	struct PanelButtonMsg {
		unsigned short cmd;
		unsigned short charcode;
		unsigned int   z1, z2, z3;
		unsigned int   packed;
	} msg;
	msg.z1 = 0;
	msg.z2 = 0;
	msg.z3 = 0;
	msg.charcode = 0;

	const unsigned int *raw = reinterpret_cast<const unsigned int *>(evt);
	unsigned int code = raw[0]; /* evt+0x0 */
	if (code > 1)
		return;

	unsigned short cmd;
	if (code == 1) {
		msg.cmd = 0x1e;
		cmd = 0x1e;
	} else {
		msg.cmd = 0x1f;
		cmd = 0x1f;
	}

	unsigned int buttonIndex = raw[1]; /* evt+0x4 */
	unsigned int packed = buttonIndex << 0x10;

	if (mDiagMode == 0) {
		switch (buttonIndex) {
		case 0x0b: msg.charcode = 0x30; break;
		case 0x0c: msg.charcode = 0x31; break;
		case 0x0d: msg.charcode = 0x32; break;
		case 0x0e: msg.charcode = 0x33; break;
		case 0x0f: msg.charcode = 0x34; break;
		case 0x10: msg.charcode = 0x35; break;
		case 0x11: msg.charcode = 0x36; break;
		case 0x12: msg.charcode = 0x37; break;
		case 0x13: msg.charcode = 0x38; break;
		case 0x14: msg.charcode = 0x39; break;
		case 0x15: msg.charcode = 0x2d; break;
		case 0x16: msg.charcode = 0x2e; break;
		case 0x17: msg.charcode = 0x0d; break;
		case 0x33: packed |= 4; msg.charcode = 0x2b; break;
		case 0x34: packed |= 4; msg.charcode = 0x2d; break;
		default:
			msg.cmd = static_cast<unsigned short>((cmd != 0x1e) + 0x5000);
			goto sendMsg;
		}
		/* Real ground truth's own dead check (confirmed via objdump -dr:
		 * msg.cmd is provably never 0 along this path -- preserved verbatim
		 * rather than dropped as "obviously unreachable").
		 */
		if (msg.cmd == 0)
			return;
	} else {
		msg.cmd = static_cast<unsigned short>((cmd != 0x1e) + 0x5000);
	}

sendMsg:
	if (raw[3] != 0) /* evt+0xc */
		packed |= 3;
	msg.packed = packed;

	PegMessageQueuePush(PegThing_mpMessageQueue, &msg);
}

int CEditor::CPanelIfcTask::Exec()
{
	/* Real: unconditional virtual tick through mScreen's own vtable slot
	 * +0x10c -- a genuine, transcribed null-deref hazard while mScreen stays
	 * null (see header comment).
	 */
	typedef void (*ScreenTickFn)(void *);
	void *screenVtbl = *reinterpret_cast<void * const *>(mScreen);
	ScreenTickFn fn = reinterpret_cast<ScreenTickFn>(
		*reinterpret_cast<void * const *>(reinterpret_cast<const char *>(screenVtbl) + 0x10c));
	fn(mScreen);

	if (mBlinkEnabled != 0) {
		int counter = mBlinkCounter + 1;
		mBlinkCounter = counter;
		if (counter > 0x31) {
			int oldState = mLedAllState;
			mBlinkCounter = 0;
			mLedAllState = (oldState == 0) ? 1 : 0;
			int mask = (oldState == 0) ? -1 : 0xffff;
			for (int i = 0; i < 0x20; ++i) {
				int buf[2];
				buf[0] = i;
				buf[1] = mask;
				mCfgLink->mLastResult = mCfgLink->OutMono(1, buf, 8);
			}
		}
	}
	return 0;
}

int CEditor::CPanelIfcTask::Exec(CMessage &message)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&message);
	unsigned short flags = *reinterpret_cast<const unsigned short *>(raw + 8);
	if ((flags & 0x200) == 0)
		return 0;

	unsigned short subtype = flags & 0xff;

	if (subtype == 2) {
		struct PanelDiagMsg {
			unsigned short cmd;
			unsigned short pad;
			unsigned int   z1, z2, z3;
			int            payload;
		} out;
		out.cmd = 0x500e;
		out.pad = 0;
		out.z1 = 0;
		out.z2 = 0;
		out.z3 = 0;
		char c = **reinterpret_cast<char * const *>(raw + 0x10);
		out.payload = static_cast<int>(c);
		PegMessageQueuePush(PegThing_mpMessageQueue, &out);
		return 0;
	}
	if (subtype == 1) {
		OnButtonEvent(*reinterpret_cast<const CPanelOut::SButtonEvt * const *>(raw + 0x10));
		return 0;
	}
	if (subtype == 3) {
		unsigned short count = *reinterpret_cast<const unsigned short *>(raw + 0xa) >> 3;
		const unsigned char *cursor =
			*reinterpret_cast<const unsigned char * const *>(raw + 0x10);
		for (unsigned short i = 0; i < count; ++i) {
			OnAnalogEvent(reinterpret_cast<const CPanelOut::SAnalogEvt *>(cursor));
			cursor += 8;
		}
		return 0;
	}
	if (subtype == 4) {
		OnTouchPanelEvent(*reinterpret_cast<const CPanelOut::STouchPanelEvt * const *>(raw + 0x10));
		return 0;
	}
	return -1;
}

void CEditor::CPanelIfcTask::OnAnalogEvent(const CPanelOut::SAnalogEvt *) { /* Tier-B link-stub. .text+0x0824be00, 403 bytes. */ }
void CEditor::CPanelIfcTask::OnEncoderEvent(const CPanelOut::SEncoderEvt *) { /* Tier-B link-stub. .text+0x0824bdb0, 79 bytes. */ }

CPanelCfg::CPanelCfg(const CTask &owner, const char *name, int direction, unsigned short mode)
	: COutLinkMono(owner, name, direction, mode), mLastResult(0)
{
	mVtbl = (void *)PTR__CPanelCfg_08f29d48;
}
