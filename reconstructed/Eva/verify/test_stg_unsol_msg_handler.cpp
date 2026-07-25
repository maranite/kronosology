/*
 * test_stg_unsol_msg_handler.cpp  -  host-side known-answer test for
 * CSTGUnsolMsgHandler (src/ipc/stg_unsol_msg_handler.cpp, Stage 6 breadth sweep,
 * 2026-07-25).
 *
 * Exercises the real, self-contained parts of this class: the ctor's 17-entry
 * dispatch-table construction (verified by re-deriving each handler's raw address the
 * same ABI-level way the ctor itself does, via `StgUnsolMsgHandlerTestHooks`, friended
 * in stg_unsol_msg_handler.h -- same convention as ustg_user_api.h's own
 * UstgUserApiTestHooks), HandleMessage()'s subtype-index bounds check (17 is
 * out-of-range and must not fault), and that the 8 confirmed-empty real no-op bodies
 * (5 static message handlers + Initialize/InitializeForSong/BeginHandling) are
 * callable without touching their arguments (a deliberately garbage/unreadable
 * pointer is passed -- if any of these ever gained a real body that dereferences it,
 * this test would crash instead of passing).
 *
 * CPanelIfcTask/USTGAPIControl are Tier-B link-stubs elsewhere in this project (empty
 * bodies) -- HandleMessage()'s own slider-forwarding tail and EndHandling()'s
 * ESSong-scope-check tail are real but provably dead given this pass's own data (see
 * stg_unsol_msg_handler.h), so they're not separately exercised here beyond
 * confirming the whole call doesn't crash.
 */

#include <cstdio>
#include <cstring>
#include "stg_unsol_msg_handler.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Friend accessor -- peeks the private dispatch table/owner pointer the real class
 * has no public accessor for. Re-derives each instance handler's raw address the
 * same union-based ABI trick the real ctor uses (stg_unsol_msg_handler.cpp), so this
 * test independently confirms the *wiring*, not just "it doesn't crash".
 */
struct StgUnsolMsgHandlerTestHooks {
	static CEditor::CPanelIfcTask *Owner(const CSTGUnsolMsgHandler &h) { return h.mOwner; }
	static const CSTGUnsolMsgHandler::Slot &SlotAt(const CSTGUnsolMsgHandler &h, int i) { return h.mTable[i]; }

	static void *AddrOfConstRef(void (CSTGUnsolMsgHandler::*mfp)(const STGMessage &))
	{
		union { void (CSTGUnsolMsgHandler::*m)(const STGMessage &); void *p[2]; } u;
		u.p[1] = 0;
		u.m = mfp;
		return u.p[0];
	}
	static void *AddrOfRef(void (CSTGUnsolMsgHandler::*mfp)(STGMessage &))
	{
		union { void (CSTGUnsolMsgHandler::*m)(STGMessage &); void *p[2]; } u;
		u.p[1] = 0;
		u.m = mfp;
		return u.p[0];
	}
};

int main()
{
	printf("CSTGUnsolMsgHandler known-answer test\n");
	printf("=============================================\n");

	CEditor::CPanelIfcTask fakeOwner;
	CSTGUnsolMsgHandler handler(&fakeOwner);

	printf("[1] Ctor stores the owner pointer verbatim\n");
	check("mOwner == &fakeOwner", StgUnsolMsgHandlerTestHooks::Owner(handler) == &fakeOwner);

	printf("[2] Dispatch table wiring matches the real subtype -> handler mapping\n");
	check("slot 0 (Control, const&) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 0).pfn ==
	          StgUnsolMsgHandlerTestHooks::AddrOfConstRef(&CSTGUnsolMsgHandler::ControlMsgHandler));
	check("slot 1 (Global, const&) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 1).pfn ==
	          StgUnsolMsgHandlerTestHooks::AddrOfConstRef(&CSTGUnsolMsgHandler::GlobalMsgHandler));
	check("slot 2 (Combi) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 2).pfn ==
	          StgUnsolMsgHandlerTestHooks::AddrOfRef(&CSTGUnsolMsgHandler::CombiMsgHandler));
	check("slot 6 (VoiceModel) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 6).pfn ==
	          StgUnsolMsgHandlerTestHooks::AddrOfRef(&CSTGUnsolMsgHandler::VoiceModelMsgHandler));
	check("slot 10 (TestControl, static) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 10).pfn ==
	          (void *)&CSTGUnsolMsgHandler::TestControlMsgHandler);
	check("slot 13 (FrontPanel, static) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 13).pfn ==
	          (void *)&CSTGUnsolMsgHandler::FrontPanelMsgHandler);
	check("slot 16 (SetList) address matches",
	      StgUnsolMsgHandlerTestHooks::SlotAt(handler, 16).pfn ==
	          StgUnsolMsgHandlerTestHooks::AddrOfRef(&CSTGUnsolMsgHandler::SetListMsgHandler));

	bool allAdjZero = true;
	for (int i = 0; i < 17; ++i)
		if (StgUnsolMsgHandlerTestHooks::SlotAt(handler, i).adj != 0)
			allAdjZero = false;
	check("all 17 slots have adj == 0 (plain direct-address encoding, never the odd/vtable-offset case)", allAdjZero);

	printf("[3] HandleMessage() subtype bounds check -- out-of-range index must not fault\n");
	{
		/* STGMessage stays opaque -- fabricate a 32-byte buffer, big enough for
		 * offset+4's int subtype field, matching how the real dispatcher only
		 * ever touches that one field (see header).
		 */
		unsigned char buf[32];
		memset(buf, 0, sizeof(buf));
		*(int *)(buf + 4) = 17; /* one past the real 0..16 range */
		STGMessage &msg = *(STGMessage *)buf;
		handler.HandleMessage(msg); /* must not fault */
		check("subtype==17 (out of range) handled without a crash", true);
	}

	printf("[4] Real confirmed-empty handlers tolerate a garbage message pointer (never dereferenced)\n");
	{
		STGMessage *bogus = (STGMessage *)(uintptr_t)0xdeadbeef; /* deliberately unreadable */
		handler.TestControlMsgHandler(*bogus);
		handler.ASKMsgHandler(*bogus);
		handler.CalibrationMsgHandler(*bogus);
		handler.FrontPanelMsgHandler(*bogus);
		handler.KLMMsgHandler(*bogus);
		CSTGUnsolMsgHandler::Initialize((CCombi *)bogus, (CCombi *)bogus);
		CSTGUnsolMsgHandler::InitializeForSong((CCombi *)bogus, (CCombi *)bogus);
		CSTGUnsolMsgHandler::BeginHandling();
		check("all 8 confirmed-empty handlers returned without touching their args", true);
	}

	printf("[5] EnterGlobalObjectEdit()/EndHandling()/SendValueSlider()/SendValueEncoder() don't crash\n");
	{
		handler.EnterGlobalObjectEdit(1);
		handler.EnterGlobalObjectEdit(0);
		handler.EndHandling();       /* dead branches given no producer sets sNowValueSlider/mForceSaveOnEnd */
		handler.SendValueSlider();   /* unconditional real call into the Tier-B OnAnalogEvent stub */
		handler.SendValueEncoder();  /* conditional on sEncoderValue!=0 -- stays 0, so a real no-op here */
		check("no crash across the remaining real (mostly currently-dead-branch) methods", true);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
