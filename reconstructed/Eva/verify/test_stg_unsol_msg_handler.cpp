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

/* Real globals defined in mains.cpp / stg_unsol_msg_handler.cpp -- declared here
 * (file scope; `extern "C"` linkage-specifications for a single declaration are
 * not legal at block/function scope, so these can't be declared right where
 * they're used below) so test [6] can swap EditApi to a fake object and drive
 * PatchMsgHandler's own DAT_0af0df1e mode-gate byte directly.
 */
extern void *EditApi;
extern unsigned char DAT_0af0df1e;

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

	/* --- Stage 6 batch 2 (2026-07-25): PatchMsgHandler/EffectMgrMsgHandler/
	 * EffectMsgHandler/HDRTrackMsgHandler/SetListMsgHandler -----------------
	 *
	 * All five unconditionally dispatch through EditApi's own vtable (+0x28
	 * GetScopeId, +0x30 SetParam) -- exercising them for real requires a
	 * controllable fake EditApi object with a real vtable at those offsets,
	 * capturing the args each handler computed. Real EditApi is swapped out
	 * for the duration of this block and restored after.
	 */
	printf("[6] PatchMsgHandler/EffectMgrMsgHandler/EffectMsgHandler/HDRTrackMsgHandler/SetListMsgHandler (Stage 6 batch 2, real EditApi dispatch)\n");
	{
		struct Capture {
			bool called;
			const char *scopeName;
			unsigned char scope, code, value;
			unsigned int payload;
			int len, flag;
		};
		static Capture cap;

		struct Fake {
			static unsigned char GetScopeId(void *, const char *name)
			{
				cap.scopeName = name;
				return 0x40; /* fixed, distinguishable scope id */
			}
			static void SetParam(void *, unsigned char scope, unsigned char code, unsigned char value,
			                     void *payload, int len, int flag)
			{
				cap.called = true;
				cap.scope = scope;
				cap.code = code;
				cap.value = value;
				cap.len = len;
				cap.flag = flag;
				cap.payload = 0;
				if (len > 0 && (size_t)len <= sizeof(cap.payload))
					memcpy(&cap.payload, payload, (size_t)len);
			}
		};

		/* 16-slot fake vtable -- covers every offset any of these five
		 * handlers dispatch through (+0x28/+0x30; +0x2c/+0x38/+0x3c are never
		 * reached since s_eNowRestoreSeqParameters stays 0, but filled with a
		 * harmless no-op for parity with mains.cpp's own EvaVTableStub
		 * precedent regardless).
		 */
		struct Trap { static void Nop() {} };
		void *fakeVtbl[16];
		for (int i = 0; i < 16; ++i)
			fakeVtbl[i] = (void *)Trap::Nop;
		fakeVtbl[0x28 / 4] = (void *)Fake::GetScopeId;
		fakeVtbl[0x30 / 4] = (void *)Fake::SetParam;

		void *fakeObj = fakeVtbl; /* first field of the fake object IS its vtable ptr */
		void *realEditApi = EditApi;
		EditApi = &fakeObj;

		unsigned char buf[64];

		/* -- PatchMsgHandler: needs the opaque DAT_0af0df1e mode byte == 3
		 * (mod 8) to take its one real branch at all -- see header/.cpp.
		 */
		{
			unsigned char saved = DAT_0af0df1e;
			DAT_0af0df1e = 3;

			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0xc) = 0xdeadbeef; /* irrelevant -- bypassed via target==0xffff */
			*(unsigned int *)(buf + 0x10) = 0xffff;    /* wildcard target -- always accepted */
			buf[0x14] = 7;                              /* SVar1 */
			*(int *)(buf + 0x18) = 5;                    /* decremented to 4, then sent as payload */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.PatchMsgHandler(msg);

			check("PatchMsgHandler: dispatched", cap.called);
			check("PatchMsgHandler: scope name == \"ESProg\"", cap.scopeName && strcmp(cap.scopeName, "ESProg") == 0);
			check("PatchMsgHandler: scope id == fake 0x40", cap.scope == 0x40);
			check("PatchMsgHandler: code == 0x53 (real constant)", cap.code == 0x53);
			check("PatchMsgHandler: value == SVar1 (7)", cap.value == 7);
			check("PatchMsgHandler: payload decremented 5 -> 4", cap.payload == 4);
			check("PatchMsgHandler: len == 4, flag == 1", cap.len == 4 && cap.flag == 1);

			DAT_0af0df1e = saved;
		}

		/* -- EffectMgrMsgHandler: kind==1 (prog), table idx 0 (19,0), target
		 * wildcard 0xffff so the ESProg (not ESSampling) branch is taken,
		 * cVar6 += 2 -> base 21, + SVar1(5) -> 26.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(int *)(buf + 0x20) = 1;                  /* kind == prog */
			*(unsigned int *)(buf + 0x10) = 0xffff;    /* wildcard target */
			buf[0x14] = 5;                              /* SVar1 */
			*(int *)(buf + 0x18) = 0;                    /* LFO table index 0 -> (19,0) */
			*(unsigned int *)(buf + 0x1c) = 0x11223344; /* payload */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.EffectMgrMsgHandler(msg);

			check("EffectMgrMsgHandler: dispatched", cap.called);
			check("EffectMgrMsgHandler: scope name == \"ESProg\"", cap.scopeName && strcmp(cap.scopeName, "ESProg") == 0);
			check("EffectMgrMsgHandler: code == 26 (19+2+5)", cap.code == 26);
			check("EffectMgrMsgHandler: value == 0 (table[1])", cap.value == 0);
			check("EffectMgrMsgHandler: payload == 0x11223344", cap.payload == 0x11223344);
		}

		/* -- EffectMsgHandler: kind==1, target wildcard 0xffff, +0x18 != 0
		 * branch (value = low byte of +0x18, code = raw byte at +0x14).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(int *)(buf + 0x20) = 1;
			*(unsigned int *)(buf + 0x10) = 0xffff;
			*(unsigned int *)(buf + 0x18) = 7;   /* nonzero -> "else" branch, value = 7 */
			buf[0x14] = 42;                       /* code */
			*(unsigned int *)(buf + 0x1c) = 0xcafebabe;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.EffectMsgHandler(msg);

			check("EffectMsgHandler: dispatched", cap.called);
			check("EffectMsgHandler: scope name == \"ESEffect\"", cap.scopeName && strcmp(cap.scopeName, "ESEffect") == 0);
			check("EffectMsgHandler: code == 42 (raw byte at +0x14)", cap.code == 42);
			check("EffectMsgHandler: value == 7 (low byte of +0x18)", cap.value == 7);
			check("EffectMsgHandler: payload == 0xcafebabe", cap.payload == 0xcafebabe);
		}

		/* -- HDRTrackMsgHandler: target==0xffff wildcard, subtype 0 (not the
		 * 0xb/0xc special case) -> else branch: code = field10_byte + table[0],
		 * value = table[1]. Table[0] pair (idx0) is (0x58,0x0b) = (88,11).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0xc) = 0xffff;  /* wildcard target (this handler checks +0xc) */
			*(int *)(buf + 0x14) = 0;                /* subtype 0 */
			*(unsigned int *)(buf + 0x10) = 5;       /* field10, low byte combined into code */
			*(unsigned int *)(buf + 0x18) = 0xaabbccdd;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.HDRTrackMsgHandler(msg);

			check("HDRTrackMsgHandler: dispatched", cap.called);
			check("HDRTrackMsgHandler: scope name == \"ESSong\"", cap.scopeName && strcmp(cap.scopeName, "ESSong") == 0);
			check("HDRTrackMsgHandler: code == 93 (5+88)", cap.code == 93);
			check("HDRTrackMsgHandler: value == 11 (table[1])", cap.value == 11);
			check("HDRTrackMsgHandler: payload == 0xaabbccdd", cap.payload == 0xaabbccdd);
		}

		/* -- SetListMsgHandler, "else" branch (subtype 6 -> idx 0, table
		 * (1,0)): code == 1, value == 0.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 0x14) = 6;
			*(unsigned int *)(buf + 0x18) = 0x12345678;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.SetListMsgHandler(msg);

			check("SetListMsgHandler (case 6): dispatched", cap.called);
			check("SetListMsgHandler (case 6): scope name == \"ESSetList\"", cap.scopeName && strcmp(cap.scopeName, "ESSetList") == 0);
			check("SetListMsgHandler (case 6): code == 1", cap.code == 1);
			check("SetListMsgHandler (case 6): value == 0", cap.value == 0);
			check("SetListMsgHandler (case 6): payload == 0x12345678", cap.payload == 0x12345678);
		}

		/* -- SetListMsgHandler, "CSWTCH_290-gated" branch (subtype 3 -> idx 1,
		 * s_akbyAPSlot pair (19,5)): code == (19 + field0x10) & 0xff, value == 5.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 0x14) = 3;
			*(int *)(buf + 0x10) = 2; /* added into the code byte */
			*(unsigned int *)(buf + 0x18) = 0x99aabbcc;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.SetListMsgHandler(msg);

			check("SetListMsgHandler (case 3): dispatched", cap.called);
			check("SetListMsgHandler (case 3): code == 21 (19+2)", cap.code == 21);
			check("SetListMsgHandler (case 3): value == 5", cap.value == 5);
			check("SetListMsgHandler (case 3): payload == 0x99aabbcc", cap.payload == 0x99aabbcc);
		}

		EditApi = realEditApi;
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
