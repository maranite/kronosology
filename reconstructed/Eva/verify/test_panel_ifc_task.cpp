/*
 * test_panel_ifc_task.cpp  -  host-side known-answer test for
 * CEditor::CPanelIfcTask/CPanelCfg (src/ui/panel_ifc_task.cpp), Stage 6
 * dedicated CPanelIfcTask batch, 2026-07-25.
 *
 * Checks:
 *   [1] Real ctor: mCfgLink real/non-null (CPanelCfg registered via CTask::Add),
 *       sInstance set, default field values (mTouchX/Y == -1, mUnknown90/94 ==
 *       0x100, mBlinkEnabled == 0).
 *   [2] SetLEDStatus(ELedCode, ELedState) / (int, ushort, ushort) / (ELedState) --
 *       real OutMono() ecb/buf content per overload, mBlinkEnabled/mLedAllState
 *       side effects for the 3-way single-int overload.
 *   [3] ShortBeep()/EnterDiagnostics() -- real OutMono(ecb, value) calls,
 *       mDiagMode side effect.
 *   [4] SetupPanelInterface() -- 3 real OutMono() calls (ecb 5/0xc/0xe), query
 *       buffer content (owner module name / task name pointers).
 *   [5] SetAllLED() -- 32-iteration loop, mask depends on state, mLedAllState
 *       side effect.
 *   [6] Exec() (0-arg) -- real unconditional tick through mScreen's own +0x10c
 *       vtable slot, mBlinkCounter increments, 50-tick blink fires the real
 *       32-iteration OutMono() loop and toggles mLedAllState.
 *   [7] OnButtonEvent()/OnTouchPanelEvent() -- real field side effects
 *       (mTouchActive/mTouchX/mTouchY/mTouchGen), PegMessageQueue::Push() stays
 *       an inert stand-in (no crash).
 *   [8] Exec(CMessage&) -- subtype dispatch (1->OnButtonEvent, 4->OnTouchPanelEvent),
 *       and the bit-0x200-clear early-out.
 */

#include <cstdio>
#include <cstring>

#include "panel_ifc_task.h"
#include "editor.h"
#include "task.h"
#include "module.h"
#include "omega_ptr_array.h"
#include "system_api.h"
#include "stg_unsol_msg_handler.h" /* CPanelOut::SAnalogEvt/SEncoderEvt real definitions */

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Same convention as out_link.h's own OutLinkTestHooks (test_out_link.cpp) --
 * redefined here since each verify/test_*.cpp file is its own standalone binary.
 */
struct OutLinkTestHooks {
	static void SetLink(COutLinkMono &o, void *fakeLink) { o.mLink = (CLink *)fakeLink; }
	static void *Links(COutLinkMono &o) { return o.mLinks; }
};

/* Friend accessor declared in panel_ifc_task.h. */
struct PanelIfcTaskTestHooks {
	static void SetScreen(CEditor::CPanelIfcTask &t, void *screen) { t.mScreen = screen; }
	static CPanelCfg *GetCfgLink(CEditor::CPanelIfcTask &t) { return t.mCfgLink; }
	static short GetTouchX(const CEditor::CPanelIfcTask &t) { return t.mTouchX; }
	static short GetTouchY(const CEditor::CPanelIfcTask &t) { return t.mTouchY; }
	static int GetTouchActive(const CEditor::CPanelIfcTask &t) { return t.mTouchActive; }
	static int GetTouchGen(const CEditor::CPanelIfcTask &t) { return t.mTouchGen; }
	static void SetTouchGen(CEditor::CPanelIfcTask &t, int v) { t.mTouchGen = v; }
	static int GetDiagMode(const CEditor::CPanelIfcTask &t) { return t.mDiagMode; }
	static int GetLedAllState(const CEditor::CPanelIfcTask &t) { return t.mLedAllState; }
	static int GetBlinkEnabled(const CEditor::CPanelIfcTask &t) { return t.mBlinkEnabled; }
	static int GetBlinkCounter(const CEditor::CPanelIfcTask &t) { return t.mBlinkCounter; }
	static void SetBlinkCounter(CEditor::CPanelIfcTask &t, int v) { t.mBlinkCounter = v; }
	static unsigned int GetLastQueryFlags(const CEditor::CPanelIfcTask &t) { return t.mLastQueryFlags; }
};

extern CSystemApi *Api;

extern "C" int FakeScopeIdFn(void *) { return 0xbeef; }
extern "C" void FakeApiNoOp() {}

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

/* Fake CLink descriptor + fake receiver, same shape as test_out_link.cpp. */
struct FakeReceiver { void *vtbl; };

static int g_recvCalls;
static unsigned short g_lastEcb;
static void *g_lastBuf;
static unsigned short g_lastLen;
static int g_lastBufSnapshot[2]; /* copied out DURING the call -- the caller's own
                                   * stack buffer is dangling by the time control
                                   * returns to the check() below it. */
static unsigned long g_lastValue; /* CLink+0x20 raw value, for the (ecb,value)
                                    * overload -- NOT a pointer in that mode. */
extern "C" int FakeReceiverSlot8(void *self, void *arg)
{
	(void)self;
	g_recvCalls++;
	unsigned char *link = (unsigned char *)arg - 0x10; /* arg == link+0x10 */
	unsigned short flags = *(unsigned short *)(link + 0x18);
	g_lastEcb = flags & 0xff;
	g_lastValue = *(unsigned long *)(link + 0x20);
	if (flags & 0x200) {
		/* pointer-buffer mode (the len-taking overload) -- link+0x1a/+0x20 are
		 * meaningful as (len, buf pointer); the (ecb,value) overload never
		 * touches either, so only trust these under the 0x200 mode bit.
		 */
		g_lastLen = *(unsigned short *)(link + 0x1a);
		g_lastBuf = *(void **)(link + 0x20);
		if (g_lastLen >= 8 && g_lastBuf != 0)
			memcpy(g_lastBufSnapshot, g_lastBuf, 8);
	}
	return 0;
}

static unsigned char g_fakeLink[0x30];
static FakeReceiver g_recv;
static void *g_recvVtbl[3];

static void wire_real_link(CPanelCfg &cfg)
{
	int dummyElem = 0;
	((COmegaPtrArray *)OutLinkTestHooks::Links(cfg))->Add(&dummyElem);

	memset(g_fakeLink, 0, sizeof(g_fakeLink));
	g_recvVtbl[0] = (void *)FakeApiNoOp;
	g_recvVtbl[1] = (void *)FakeApiNoOp;
	g_recvVtbl[2] = (void *)FakeReceiverSlot8;
	g_recv.vtbl = g_recvVtbl;
	*(void **)(g_fakeLink + 0x24) = &g_recv;

	OutLinkTestHooks::SetLink(cfg, g_fakeLink);
}

/* Fake PegScreen-shaped buffer: vtbl[0x10c/4] callable, +0x14/+0x16 dimensions. */
static int g_screenTickCalls;
extern "C" void FakeScreenTick(void *) { g_screenTickCalls++; }

static unsigned char g_fakeScreen[0x18];
static void *g_screenVtbl[(0x110 / 4) + 1];

static void *make_fake_screen()
{
	for (unsigned i = 0; i < sizeof(g_screenVtbl) / sizeof(g_screenVtbl[0]); i++)
		g_screenVtbl[i] = (void *)FakeApiNoOp;
	g_screenVtbl[0x10c / 4] = (void *)FakeScreenTick;
	memset(g_fakeScreen, 0, sizeof(g_fakeScreen));
	*(void **)g_fakeScreen = g_screenVtbl;
	*(short *)(g_fakeScreen + 0x14) = 800;
	*(short *)(g_fakeScreen + 0x16) = 480;
	return g_fakeScreen;
}

int main()
{
	printf("CEditor::CPanelIfcTask / CPanelCfg known-answer test\n");
	printf("======================================================\n");

	setup_fake_api();

	CEditor owner("TestEditor", 0);

	printf("[1] Real ctor: mCfgLink real, sInstance set, default field values\n");
	CEditor::CPanelIfcTask task(&owner, make_fake_screen());
	{
		check("mCfgLink is non-null (real CPanelCfg sub-object)",
		      PanelIfcTaskTestHooks::GetCfgLink(task) != 0);
		check("mTouchX defaults to -1 (0xffff)",
		      PanelIfcTaskTestHooks::GetTouchX(task) == -1);
		check("mTouchY defaults to -1 (0xffff)",
		      PanelIfcTaskTestHooks::GetTouchY(task) == -1);
		check("mBlinkEnabled defaults to 0",
		      PanelIfcTaskTestHooks::GetBlinkEnabled(task) == 0);
		check("mLedAllState defaults to 0",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 0);
	}

	wire_real_link(*PanelIfcTaskTestHooks::GetCfgLink(task));

	printf("[2] SetLEDStatus() -- all 3 overloads\n");
	{
		g_recvCalls = 0;
		task.SetLEDStatus(7, 1);
		check("SetLEDStatus(code,state): ecb == 0", g_lastEcb == 0);
		check("SetLEDStatus(code,state): len == 8", g_lastLen == 8);
		check("SetLEDStatus(code,state): buf == {7,1}",
		      g_lastBufSnapshot[0] == 7 && g_lastBufSnapshot[1] == 1);

		g_recvCalls = 0;
		task.SetLEDStatus(3, (unsigned short)0x1122, (unsigned short)0x3344);
		check("SetLEDStatus(idx,on,off): ecb == 1", g_lastEcb == 1);
		check("SetLEDStatus(idx,on,off): buf packs CONCAT22(on,off)",
		      g_lastBufSnapshot[0] == 3 &&
		      (unsigned int)g_lastBufSnapshot[1] == 0x11223344u);

		g_recvCalls = 0;
		task.SetLEDStatus(2); /* blink-enable branch */
		check("SetLEDStatus(2): calls OutMono 32 times", g_recvCalls == 32);
		check("SetLEDStatus(2): mBlinkEnabled becomes 1",
		      PanelIfcTaskTestHooks::GetBlinkEnabled(task) == 1);
		check("SetLEDStatus(2): mLedAllState becomes 1",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 1);

		g_recvCalls = 0;
		task.SetLEDStatus(0); /* all-off branch, also clears mBlinkEnabled */
		check("SetLEDStatus(0): calls OutMono 32 times", g_recvCalls == 32);
		check("SetLEDStatus(0): mLedAllState becomes 0",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 0);
		check("SetLEDStatus(0): mBlinkEnabled cleared back to 0",
		      PanelIfcTaskTestHooks::GetBlinkEnabled(task) == 0);
	}

	printf("[3] ShortBeep()/EnterDiagnostics()\n");
	{
		g_recvCalls = 0;
		task.ShortBeep();
		check("ShortBeep(): ecb == 2", g_lastEcb == 2);
		check("ShortBeep(): value == 0", g_lastValue == 0);
		check("ShortBeep(): dispatched once", g_recvCalls == 1);

		g_recvCalls = 0;
		task.EnterDiagnostics(1);
		check("EnterDiagnostics(1): ecb == 3", g_lastEcb == 3);
		check("EnterDiagnostics(1): value == 1", g_lastValue == 1);
		check("EnterDiagnostics(1): mDiagMode == 1",
		      PanelIfcTaskTestHooks::GetDiagMode(task) == 1);
	}

	printf("[4] SetupPanelInterface() -- 3 real OutMono() calls\n");
	{
		g_recvCalls = 0;
		task.SetupPanelInterface();
		check("SetupPanelInterface(): dispatches 3 times", g_recvCalls == 3);
		check("SetupPanelInterface(): last ecb == 0xe (final forward)",
		      g_lastEcb == 0xe);
		check("SetupPanelInterface(): mLastQueryFlags == 0xffffffff "
		      "(seed value, real receiver stand-in never writes back)",
		      PanelIfcTaskTestHooks::GetLastQueryFlags(task) == 0xffffffffu);
	}
	task.EnterDiagnostics(0); /* reset mDiagMode for later checks */

	printf("[5] SetAllLED()\n");
	{
		g_recvCalls = 0;
		task.SetAllLED(1);
		check("SetAllLED(1): 32 real OutMono() calls", g_recvCalls == 32);
		check("SetAllLED(1): mLedAllState == 1",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 1);

		g_recvCalls = 0;
		task.SetAllLED(0);
		check("SetAllLED(0): 32 real OutMono() calls", g_recvCalls == 32);
		check("SetAllLED(0): mLedAllState == 0",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 0);
	}

	printf("[6] Exec() (0-arg) -- real screen tick + 50-tick blink\n");
	{
		g_screenTickCalls = 0;
		int rc = task.Exec();
		check("Exec(): return value is 0", rc == 0);
		check("Exec(): real unconditional tick through mScreen+0x10c",
		      g_screenTickCalls == 1);
		check("Exec(): mBlinkEnabled==0 -> no counter movement (still 0)",
		      PanelIfcTaskTestHooks::GetBlinkEnabled(task) == 0);

		task.SetLEDStatus(2); /* mBlinkEnabled=1, mBlinkCounter=0 */
		g_recvCalls = 0;
		int ticks = 0;
		while (PanelIfcTaskTestHooks::GetBlinkCounter(task) != 0 ||
		       ticks == 0) {
			task.Exec();
			ticks++;
			if (ticks > 60)
				break;
		}
		check("Exec(): blink loop fires after crossing the 0x31 (49) threshold "
		      "(exactly 50 ticks)",
		      ticks == 50);
		check("Exec(): blink loop dispatched 32 OutMono() calls on the firing tick",
		      g_recvCalls == 32);
		check("Exec(): mLedAllState toggled by the blink (was 1, now 0)",
		      PanelIfcTaskTestHooks::GetLedAllState(task) == 0);
		check("Exec(): mBlinkCounter reset to 0 after firing",
		      PanelIfcTaskTestHooks::GetBlinkCounter(task) == 0);
	}

	printf("[7] OnButtonEvent()/OnTouchPanelEvent() -- real field side effects\n");
	{
		struct { unsigned int code; unsigned int index; unsigned int pad2, flag; } evt;
		evt.code = 1; evt.index = 0x0b; evt.pad2 = 0; evt.flag = 0;
		task.OnButtonEvent(reinterpret_cast<const CPanelOut::SButtonEvt *>(&evt));
		check("OnButtonEvent(): callable without crashing (Push() stand-in)", true);

		struct { int kind; unsigned char pad[4]; } tevt;
		tevt.kind = 1;
		tevt.pad[0] = 100; /* x */
		tevt.pad[1] = 100; /* y */
		PanelIfcTaskTestHooks::SetTouchGen(task, 0);
		task.OnTouchPanelEvent(reinterpret_cast<const CPanelOut::STouchPanelEvt *>(&tevt));
		check("OnTouchPanelEvent(kind=1): mTouchActive == 1",
		      PanelIfcTaskTestHooks::GetTouchActive(task) == 1);
		check("OnTouchPanelEvent(kind=1): mTouchGen reset to 0",
		      PanelIfcTaskTestHooks::GetTouchGen(task) == 0);
		check("OnTouchPanelEvent(kind=1): mTouchX/mTouchY committed (non -1)",
		      PanelIfcTaskTestHooks::GetTouchX(task) != -1 &&
		      PanelIfcTaskTestHooks::GetTouchY(task) != -1);

		tevt.kind = 0;
		task.OnTouchPanelEvent(reinterpret_cast<const CPanelOut::STouchPanelEvt *>(&tevt));
		check("OnTouchPanelEvent(kind=0): mTouchActive back to 0",
		      PanelIfcTaskTestHooks::GetTouchActive(task) == 0);

		PanelIfcTaskTestHooks::SetTouchGen(task, 1);
		short beforeX = PanelIfcTaskTestHooks::GetTouchX(task);
		tevt.kind = 2;
		task.OnTouchPanelEvent(reinterpret_cast<const CPanelOut::STouchPanelEvt *>(&tevt));
		check("OnTouchPanelEvent(kind=2) with mTouchGen!=0: early-return, "
		      "mTouchX unchanged",
		      PanelIfcTaskTestHooks::GetTouchX(task) == beforeX);
	}

	printf("[8] Exec(CMessage&) -- subtype dispatch\n");
	{
		unsigned char raw[24];
		memset(raw, 0, sizeof(raw));

		/* bit 0x200 clear -> real early-out, return 0, no dispatch */
		*(unsigned short *)(raw + 8) = 0x0001;
		int rc0 = task.Exec(*reinterpret_cast<CMessage *>(raw));
		check("Exec(CMessage&): bit 0x200 clear -> returns 0, no dispatch",
		      rc0 == 0);

		/* subtype 1 -> OnButtonEvent, via evt payload pointer at +0x10 */
		struct { unsigned int code; unsigned int index; unsigned int pad2, flag; } bevt;
		bevt.code = 0; bevt.index = 0x0c; bevt.pad2 = 0; bevt.flag = 1;
		*(unsigned short *)(raw + 8) = 0x0201; /* bit 0x200 | subtype 1 */
		*reinterpret_cast<void **>(raw + 0x10) = &bevt;
		int rc1 = task.Exec(*reinterpret_cast<CMessage *>(raw));
		check("Exec(CMessage&) subtype 1: returns 0 (dispatched to OnButtonEvent)",
		      rc1 == 0);

		/* subtype 4 -> OnTouchPanelEvent */
		struct { int kind; unsigned char pad[4]; } tevt2;
		tevt2.kind = 0;
		*(unsigned short *)(raw + 8) = 0x0204; /* bit 0x200 | subtype 4 */
		*reinterpret_cast<void **>(raw + 0x10) = &tevt2;
		int rc4 = task.Exec(*reinterpret_cast<CMessage *>(raw));
		check("Exec(CMessage&) subtype 4: returns 0 (dispatched to OnTouchPanelEvent)",
		      rc4 == 0);

		/* unknown subtype -> -1 */
		*(unsigned short *)(raw + 8) = 0x0207;
		int rc7 = task.Exec(*reinterpret_cast<CMessage *>(raw));
		check("Exec(CMessage&) unknown subtype: returns -1", rc7 == -1);
	}

	printf("[9] OnAnalogEvent()/OnEncoderEvent() -- real dispatch, no crash\n");
	{
		/* mDiagMode gate: EnterDiagnostics(1) sets it via the real, already-
		 * verified code path (see [3] above) rather than a raw poke.
		 */
		task.EnterDiagnostics(1);
		check("EnterDiagnostics(1): mDiagMode == 1",
		      PanelIfcTaskTestHooks::GetDiagMode(task) == 1);

		CPanelOut::SAnalogEvt aevt;
		aevt.type = 8;
		aevt.value = 500;
		task.OnAnalogEvent(&aevt); /* diag-mode reroute, Peg push, no crash */
		check("OnAnalogEvent() under mDiagMode!=0: does not crash", true);

		task.EnterDiagnostics(0);
		check("EnterDiagnostics(0): mDiagMode == 0",
		      PanelIfcTaskTestHooks::GetDiagMode(task) == 0);

		/* type==0x19: its own special case, Peg push + early return, same as
		 * every currently-reconstructed real caller (stg_unsol_msg_handler.cpp).
		 */
		aevt.type = 0x19;
		aevt.value = 700;
		task.OnAnalogEvent(&aevt);
		check("OnAnalogEvent(type=0x19): Peg-push special case, does not crash", true);

		/* type in [8..0x18]: real knob/fader index mapping, forwards to the
		 * inert CControlSurface stand-in (.cpp) -- exercise the full range,
		 * including both switch-table ends.
		 */
		for (int t = 8; t <= 0x18; ++t) {
			aevt.type = t;
			aevt.value = static_cast<short>(t * 10);
			task.OnAnalogEvent(&aevt);
		}
		check("OnAnalogEvent(type=8..0x18): full knob/fader range, no crash", true);

		/* unmatched type: default -> return, no crash */
		aevt.type = 0;
		aevt.value = 0;
		task.OnAnalogEvent(&aevt);
		aevt.type = 0xff;
		task.OnAnalogEvent(&aevt);
		check("OnAnalogEvent(type=0 / 0xff): default no-op case, no crash", true);

		CPanelOut::SEncoderEvt eevt;
		eevt.value = 200; /* > 0x7f -- exercises the real signed-char extension */
		eevt.reserved[0] = eevt.reserved[1] = eevt.reserved[2] = 0;
		eevt.zero = 0;
		task.OnEncoderEvent(&eevt);
		check("OnEncoderEvent(value=200): sign-extended byte payload, no crash", true);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
