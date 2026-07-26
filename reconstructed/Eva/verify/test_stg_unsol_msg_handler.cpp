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
	static void SetForceSaveOnEnd(CSTGUnsolMsgHandler &h, unsigned char v) { h.mForceSaveOnEnd = v; }

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

		/* --- Stage 6 batch 3 (2026-07-25): EffectSlotMsgHandler, promoted from
		 * Tier B -- reuses this same fake-EditApi harness (its "generic" tail
		 * dispatches through the exact same +0x28/+0x30 vtable slots, plus +0x3c/
		 * +0x38 which stay unreached here since s_eNowRestoreSeqParameters is 0,
		 * same as the five siblings above).
		 */
		printf("[7] EffectSlotMsgHandler (Stage 6 batch 3, real EditApi dispatch)\n");
		{
			/* idx==0 -> table sentinel (0xff,0xff): must return without dispatching. */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 1;               /* kind == prog */
				*(unsigned int *)(buf + 0x10) = 0xffff; /* wildcard target */
				*(int *)(buf + 0x18) = 0;                /* idx 0 -> (0xff,0xff) sentinel */

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);
				check("EffectSlotMsgHandler: idx==0 sentinel returns without dispatch", !cap.called);
			}

			/* Guard mismatch: kind==1 (prog), target neither matches CStorage's
			 * current selection nor 0xfffe/0xffff -> must return without dispatch.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 1;
				*(unsigned int *)(buf + 0xc) = 5;
				*(unsigned int *)(buf + 0x10) = 6;   /* mismatched, not a wildcard */
				*(int *)(buf + 0x18) = 4;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);
				check("EffectSlotMsgHandler: guard mismatch returns without dispatch", !cap.called);
			}

			/* Generic tail (idx==4, kind==1/ESProg, midiSource==0 -> CSWTCH_231[0]==4):
			 * bVar1=1,cVar5=4 (table idx4); code = (bVar1+2) + iVar3 = 3+10 = 13.
			 * Payload is the message's own +0x1c field directly (4 bytes, no local
			 * copy), lastEditMessage stays 0x500c since flag(4) != 3.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 1;                 /* kind == prog */
				*(unsigned int *)(buf + 0x10) = 0xffff;   /* wildcard target */
				*(unsigned short *)(buf + 2) = 0;          /* midiSource 0 -> CSWTCH_231[0]==4 */
				*(int *)(buf + 0x14) = 10;                 /* iVar3 */
				*(int *)(buf + 0x18) = 4;                  /* idx 4 -> table (1,4) */
				*(unsigned int *)(buf + 0x1c) = 0xdeadcafe;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (generic idx4): dispatched", cap.called);
				check("EffectSlotMsgHandler (generic idx4): scope name == \"ESProg\"", cap.scopeName && strcmp(cap.scopeName, "ESProg") == 0);
				check("EffectSlotMsgHandler (generic idx4): code == 13 (1+2+10)", cap.code == 13);
				check("EffectSlotMsgHandler (generic idx4): value == 4 (table[9])", cap.value == 4);
				check("EffectSlotMsgHandler (generic idx4): payload == 0xdeadcafe (raw +0x1c, no local copy)", cap.payload == 0xdeadcafe);
				check("EffectSlotMsgHandler (generic idx4): len == 4, flag == 4 (CSWTCH_231[0])", cap.len == 4 && cap.flag == 4);
			}

			/* idx==0xb (case 10/11/12 group, cVar4==0), kind==0/ESCombi,
			 * field(+0x1c)==0 branch -> single call, payload byte 0.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 0;                 /* kind == combi */
				*(unsigned int *)(buf + 0x10) = 0xffff;   /* wildcard target */
				*(int *)(buf + 0x18) = 0xb;                /* idx 11 -> table (0xd,0x0) */
				*(unsigned int *)(buf + 0x1c) = 0;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (idx==0xb, field==0): dispatched", cap.called);
				check("EffectSlotMsgHandler (idx==0xb, field==0): scope name == \"ESCombi\"", cap.scopeName && strcmp(cap.scopeName, "ESCombi") == 0);
				check("EffectSlotMsgHandler (idx==0xb, field==0): code == 13 (0xd+0 cVar4)", cap.code == 13);
				check("EffectSlotMsgHandler (idx==0xb, field==0): value == 0", cap.value == 0);
				check("EffectSlotMsgHandler (idx==0xb, field==0): payload == 0, len == 1", cap.payload == 0 && cap.len == 1);
			}

			/* idx==0xb, field(+0x1c)!=0 branch -> two calls; only the second
			 * (final) is directly observable via the shared Capture -- matches
			 * this file's existing single-observation convention.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 0;
				*(unsigned int *)(buf + 0x10) = 0xffff;
				*(int *)(buf + 0x18) = 0xb;
				*(unsigned int *)(buf + 0x1c) = 0x77;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (idx==0xb, field!=0): dispatched", cap.called);
				check("EffectSlotMsgHandler (idx==0xb, field!=0): 2nd-call value == 1 (0+1)", cap.value == 1);
				check("EffectSlotMsgHandler (idx==0xb, field!=0): 2nd-call payload == 0x77 (raw field byte)", cap.payload == 0x77);
				check("EffectSlotMsgHandler (idx==0xb, field!=0): len == 1, flag == 1", cap.len == 1 && cap.flag == 1);
			}

			/* idx==3 (default-group, non-idx2), kind==2/ESSong: code = bVar1(1) +
			 * iVar3(5) = 6; field(+0x1c)==20 (>0xc) clamps to 20-12=8, len==4.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 2;                 /* kind == song */
				*(unsigned int *)(buf + 0x10) = 0xffff;   /* wildcard target */
				*(int *)(buf + 0x14) = 5;                  /* iVar3 */
				*(int *)(buf + 0x18) = 3;                  /* idx 3 -> table (1,3) */
				*(int *)(buf + 0x1c) = 20;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (idx==3): dispatched", cap.called);
				check("EffectSlotMsgHandler (idx==3): scope name == \"ESSong\"", cap.scopeName && strcmp(cap.scopeName, "ESSong") == 0);
				check("EffectSlotMsgHandler (idx==3): code == 6 (1+5)", cap.code == 6);
				check("EffectSlotMsgHandler (idx==3): value == 3 (table[7], unchanged)", cap.value == 3);
				check("EffectSlotMsgHandler (idx==3): payload == 8 (20-12 clamp), len == 4", cap.payload == 8 && cap.len == 4);
			}

			/* idx==2 (default-group special case), kind==2/ESSong, field(+0x1c)==
			 * 0x19 branch -> single call, payload byte 0.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 2;
				*(unsigned int *)(buf + 0x10) = 0xffff;
				*(int *)(buf + 0x14) = 7;                  /* iVar3 */
				*(int *)(buf + 0x18) = 2;                  /* idx 2 -> table (1,8) */
				*(int *)(buf + 0x1c) = 0x19;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (idx==2, field==0x19): dispatched", cap.called);
				check("EffectSlotMsgHandler (idx==2, field==0x19): code == 8 (1+7)", cap.code == 8);
				check("EffectSlotMsgHandler (idx==2, field==0x19): value == 8 (table[5], unchanged)", cap.value == 8);
				check("EffectSlotMsgHandler (idx==2, field==0x19): payload == 0, len == 1", cap.payload == 0 && cap.len == 1);
			}

			/* idx==2, field(+0x1c)!=0x19 branch -> two calls; final (2nd) call
			 * observable: value == 9 (8+1), payload == (0x50-1) == 0x4f.
			 */
			{
				memset(buf, 0, sizeof(buf));
				*(int *)(buf + 0x20) = 2;
				*(unsigned int *)(buf + 0x10) = 0xffff;
				*(int *)(buf + 0x14) = 7;
				*(int *)(buf + 0x18) = 2;
				*(int *)(buf + 0x1c) = 0x50;

				cap = Capture();
				STGMessage &msg = *(STGMessage *)buf;
				handler.EffectSlotMsgHandler(msg);

				check("EffectSlotMsgHandler (idx==2, field!=0x19): dispatched", cap.called);
				check("EffectSlotMsgHandler (idx==2, field!=0x19): 2nd-call value == 9 (8+1)", cap.value == 9);
				check("EffectSlotMsgHandler (idx==2, field!=0x19): 2nd-call payload == 0x4f (0x50-1)", cap.payload == 0x4f);
				check("EffectSlotMsgHandler (idx==2, field!=0x19): len == 1, flag == 1", cap.len == 1 && cap.flag == 1);
			}
		}

		EditApi = realEditApi;
	}

	/* --- Stage 6 batch 4 (2026-07-26): GlobalMsgHandler, the one genuine new lead
	 * out of the six remaining Tier-B handlers surveyed in this same session's
	 * memory writeup -- see header/.cpp for the two real, genuinely asymmetric
	 * restore-guard shapes (case 0's snapshotted iVar8, case 2's goto
	 * LAB_089192c8 double-beginRestore re-entry). SetWithoutUpdatingSTG is a
	 * file-local static in stg_unsol_msg_handler.cpp (not header-declared, same
	 * convention as USTGAPIControl/USTGAPIFsck) -- its own two call sites (case 1,
	 * case 2's wave-seq-table sub-branch) can't be observed directly from here, so
	 * coverage below confirms every OTHER observable effect (scope/code/value/
	 * payload reaching the real EditApi vtable dispatch) plus "does not crash"
	 * for the SetWithoutUpdatingSTG-reaching branches, same as this file's
	 * existing Tier-B-adjacent stub coverage elsewhere.
	 */
	printf("[9] GlobalMsgHandler (Stage 6 batch 4, 2026-07-26, real EditApi dispatch)\n");
	{
		struct Capture {
			bool called;
			const char *scopeName;
			unsigned char scope, code, value;
			unsigned int payload;
			int len, flag;
		};
		static Capture cap;
		static unsigned int g_flagVal;
		static unsigned char g_paramObj[0x28];

		struct Fake9 {
			static unsigned char GetScopeId(void *, const char *name)
			{
				cap.scopeName = name;
				return 0x40;
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
			static void QueryFlag(void *, unsigned char, int, int, unsigned char *out, int len)
			{
				memcpy(out, &g_flagVal, (size_t)len);
			}
			static void *GetParamPtr(void *, unsigned char, unsigned char, unsigned char)
			{
				return g_paramObj;
			}
		};

		struct Trap9 { static void Nop() {} };
		void *fakeVtbl[16];
		for (int i = 0; i < 16; ++i)
			fakeVtbl[i] = (void *)Trap9::Nop;
		fakeVtbl[0x20 / 4] = (void *)Fake9::GetParamPtr;
		fakeVtbl[0x28 / 4] = (void *)Fake9::GetScopeId;
		fakeVtbl[0x2c / 4] = (void *)Fake9::QueryFlag;
		fakeVtbl[0x30 / 4] = (void *)Fake9::SetParam;

		void *fakeObj = fakeVtbl;
		void *realEditApi = EditApi;
		EditApi = &fakeObj;

		unsigned char buf[64];

		/* case 0, code 0x6e (>0x6d): out-of-range global-param code, must
		 * return without dispatch.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0x10) = 0x6e;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case0: code>0x6d returns without dispatch", !cap.called);
		}

		/* case 0, code 0 (table[0] = (1,0), not in the 0x26/0x2c-0x2d/4-6
		 * special ranges): value == (char)iVar5 + 0, code == 1.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0x10) = 0;   /* paramCode 0 */
			*(unsigned int *)(buf + 0xc) = 5;     /* iVar5 */
			*(unsigned int *)(buf + 0x14) = 0x11223344; /* local_2c payload */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case0 (code 0): dispatched", cap.called);
			check("GlobalMsgHandler case0 (code 0): scope name == \"ESGlobal\"", cap.scopeName && strcmp(cap.scopeName, "ESGlobal") == 0);
			check("GlobalMsgHandler case0 (code 0): code == 1 (table[0] byte0)", cap.code == 1);
			check("GlobalMsgHandler case0 (code 0): value == 5 (iVar5 + table[0] byte1==0)", cap.value == 5);
			check("GlobalMsgHandler case0 (code 0): payload == field0x14 verbatim", cap.payload == 0x11223344);
		}

		/* case 0, code 4 (table[4] = (1,4), inside the 4..6 "normalize to
		 * boolean" range): local_2c becomes (field0x14 != 0) -> payload == 1.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0x10) = 4;
			*(unsigned int *)(buf + 0xc) = 9;
			*(unsigned int *)(buf + 0x14) = 0x99; /* nonzero -> normalized to 1 */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case0 (code 4): code == 1 (table[4] byte0)", cap.code == 1);
			check("GlobalMsgHandler case0 (code 4): value == 13 (9+4)", cap.value == 13);
			check("GlobalMsgHandler case0 (code 4): payload normalized to boolean 1", cap.payload == 1);
		}

		/* case 0, code 0x26: real div-by-12 special case -- reaches this
		 * function's own inner setParam(code=8,value=0) first (not directly
		 * observable beyond "no crash", since the fake vtbl's SetParam
		 * overwrites `cap` for the LATER, final call too), then the outer
		 * call with code == table[0x26] byte0 (0x01), value == (iVar5%12) +
		 * table[0x26] byte1. iVar5=0xc+3 -> iVar5%12==3. table idx 0x26=38 =
		 * (0x08,0x01) i.e. byte0=8, byte1=1 -- code==8, value==3+1==4.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			*(unsigned int *)(buf + 0x10) = 0x26;
			*(unsigned int *)(buf + 0xc) = 0xc + 3;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case0 (code 0x26): dispatched (final call observed)", cap.called);
			check("GlobalMsgHandler case0 (code 0x26): final code == 8 (table[0x26] byte0)", cap.code == 8);
			check("GlobalMsgHandler case0 (code 0x26): final value == 4 ((15%12)+1)", cap.value == 4);
		}

		/* case 1: target != -1 -> no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0xc) = 0;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case1: target != -1 returns without dispatch", !cap.called);
		}

		/* case 1: idx 0 -> table[0] = (0x0a,0x1f), idx not in the bitmask ->
		 * no addend. Inner call is the LITERAL (code=0xa,value=2); the
		 * SetWithoutUpdatingSTG(scope,0x0a,0x1f,...) stub call isn't directly
		 * observable, but must not crash.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x18) = 0;   /* idx 0 */
			*(unsigned int *)(buf + 0x10) = 0xaabbccdd;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case1 (idx 0): dispatched", cap.called);
			check("GlobalMsgHandler case1 (idx 0): code == 0xa (literal)", cap.code == 0xa);
			check("GlobalMsgHandler case1 (idx 0): value == 2 (literal)", cap.value == 2);
			check("GlobalMsgHandler case1 (idx 0): payload == field0x10 verbatim", cap.payload == 0xaabbccdd);
		}

		/* case 1: idx 0x1f (>0x1e) -> no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x18) = 0x1f;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case1: idx>0x1e returns without dispatch", !cap.called);
		}

		/* case 2, sub==0x20, negative field0x1c: label entry directly, no
		 * inner call, final code==0xb, value==3, payload boolean 0 (negative
		 * -> ~x>>31 == 0).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 2;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x14) = 0x20;
			*(int *)(buf + 0x1c) = -5;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case2 (sub 0x20, negative): dispatched", cap.called);
			check("GlobalMsgHandler case2 (sub 0x20, negative): code == 0xb", cap.code == 0xb);
			check("GlobalMsgHandler case2 (sub 0x20, negative): value == 3", cap.value == 3);
			check("GlobalMsgHandler case2 (sub 0x20, negative): payload boolean == 0", cap.payload == 0);
		}

		/* case 2, sub==0x20, non-negative field0x1c: real inner call fires
		 * first (code=0xb,value=2, not separately observable beyond no-crash
		 * since the fake SetParam gets overwritten by the final call too),
		 * then final code==0xb, value==3, payload boolean 1 (non-negative ->
		 * ~x>>31 == 1).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 2;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x14) = 0x20;
			*(int *)(buf + 0x1c) = 7;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case2 (sub 0x20, non-negative): final code == 0xb", cap.code == 0xb);
			check("GlobalMsgHandler case2 (sub 0x20, non-negative): final value == 3", cap.value == 3);
			check("GlobalMsgHandler case2 (sub 0x20, non-negative): payload boolean == 1", cap.payload == 1);
		}

		/* case 2, sub in the wave-seq-table special range (0x10..0x21) with
		 * kGlobalMsgWaveSeqFlag set (every index except 0x20, already
		 * covered above) -- e.g. sub 0x11 (kGlobalMsgWaveSeqFlag[1]==1). The
		 * observable inner EditApiSendParamMsg call always uses the LITERAL
		 * (code=0xb, value=4) -- kWaveSeqParamAP's own {subCode,subValue}
		 * pair for this sub only ever reaches the non-observable
		 * SetWithoutUpdatingSTG() stub call, not this call. Real
		 * getParamPtr()->+0x24 clamp: with clamp >= target, no adjustment,
		 * so payload == the raw, unclamped target.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 2;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x14) = 0x11;
			*(unsigned int *)(buf + 0x10) = 3; /* target */

			*(int *)(g_paramObj + 0x24) = 10; /* clamp >= target -> no adjustment */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case2 (wave-seq-table, sub 0x11): dispatched", cap.called);
			check("GlobalMsgHandler case2 (wave-seq-table, sub 0x11): code == 0xb (literal)", cap.code == 0xb);
			check("GlobalMsgHandler case2 (wave-seq-table, sub 0x11): value == 4 (literal)", cap.value == 4);
			check("GlobalMsgHandler case2 (wave-seq-table, sub 0x11): payload == target (3, unclamped)", cap.payload == 3);
		}

		/* case 2, default sub-branch (sub not 0x20, not in the special
		 * flagged range) -- e.g. sub 1: table[1] = (0x0b,0x07), not < 0xe
		 * with the 0x3030 bitmask set (bit 1 -> 0x3030 & 2 == 0) so no +1
		 * adjustment. code == 0xb, value == 7, payload == field0x1c
		 * verbatim.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 2;
			*(int *)(buf + 0xc) = -1;
			*(unsigned int *)(buf + 0x14) = 1;
			*(unsigned int *)(buf + 0x1c) = 0x55667788;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case2 (default, sub 1): dispatched", cap.called);
			check("GlobalMsgHandler case2 (default, sub 1): code == 0xb (table[1] byte0)", cap.code == 0xb);
			check("GlobalMsgHandler case2 (default, sub 1): value == 7 (table[1] byte1)", cap.value == 7);
			check("GlobalMsgHandler case2 (default, sub 1): payload == field0x1c verbatim", cap.payload == 0x55667788);
		}

		/* case 2: target check fails -> no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 2;
			*(int *)(buf + 0xc) = 5; /* != -1 */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case2: target != -1 returns without dispatch", !cap.called);
		}

		/* case 3: field0xc mismatches the queried flag -> dispatch with the
		 * real literal code==0xa, value==1, payload == field0xc verbatim.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 3;
			*(unsigned int *)(buf + 0xc) = 0x42;
			g_flagVal = 0x99; /* mismatched */

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case3 (mismatch): dispatched", cap.called);
			check("GlobalMsgHandler case3 (mismatch): code == 0xa (literal)", cap.code == 0xa);
			check("GlobalMsgHandler case3 (mismatch): value == 1 (literal)", cap.value == 1);
			check("GlobalMsgHandler case3 (mismatch): payload == field0xc verbatim", cap.payload == 0x42);
		}

		/* case 3: field0xc matches the queried flag -> no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 3;
			*(unsigned int *)(buf + 0xc) = 0x77;
			g_flagVal = 0x77;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case3 (match): returns without dispatch", !cap.called);
		}

		/* case 4: field0xc mismatches -> dispatch with code==0xb, value==1. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 4;
			*(unsigned int *)(buf + 0xc) = 0x12;
			g_flagVal = 0x34;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);

			check("GlobalMsgHandler case4 (mismatch): dispatched", cap.called);
			check("GlobalMsgHandler case4 (mismatch): code == 0xb (literal)", cap.code == 0xb);
			check("GlobalMsgHandler case4 (mismatch): value == 1 (literal)", cap.value == 1);
			check("GlobalMsgHandler case4 (mismatch): payload == field0xc verbatim", cap.payload == 0x12);
		}

		/* case 4: field0xc matches -> no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 4;
			*(unsigned int *)(buf + 0xc) = 0x21;
			g_flagVal = 0x21;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler case4 (match): returns without dispatch", !cap.called);
		}

		/* default (subtype 5, out of range 0..4): no dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 5;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.GlobalMsgHandler(msg);
			check("GlobalMsgHandler default subtype: returns without dispatch", !cap.called);
		}

		EditApi = realEditApi;
	}

	/* Stage 6 breadth sweep, 2026-07-25: USTGAPIControl::SaveRandomSeed()/
	 * ForceErPShutdown() promoted from Tier-B link-stubs to real bodies
	 * (stg_unsol_msg_handler.cpp) -- SaveRandomSeed() now genuinely
	 * fork()+execve()s "/korg/Eva/mumount" (which does not exist on this host,
	 * so the child harmlessly exit(1)s and the parent's own real error path
	 * fires -- exactly the same shape a real device missing the on-image
	 * /korg/Eva/mumount binary would hit), and ForceErPShutdown() builds a
	 * real STGMessage and forwards it through the already-real
	 * SendSTGMessageWithSource() (which fails gracefully with m_activeUser2rtFD
	 * == -1, unconnected, in this host harness -- same behavior already proven
	 * safe by test_ustg_user_api.cpp). Exercised here via EndHandling()'s own
	 * real mForceSaveOnEnd-gated tail, with a fake EditApi forcing the
	 * "ESSong flag == 0" branch that reaches both calls -- not just a direct
	 * unit call, since USTGAPIControl/USTGAPIFsck are file-local (no header
	 * declaration) by this project's own established convention for
	 * not-reconstructed-elsewhere classes.
	 */
	printf("[8] EndHandling()'s real mForceSaveOnEnd tail -- SaveRandomSeed()/ForceErPShutdown() (Stage 6 breadth sweep, 2026-07-25)\n");
	{
		struct Fake8 {
			static unsigned char GetScopeId(void *, const char *) { return 0x40; }
			static void QueryFlag(void *, unsigned char, int, int, unsigned char *out, int)
			{
				*out = 0; /* force the "flag == 0" branch */
			}
		};
		struct Trap8 { static void Nop() {} };
		void *fakeVtbl[16];
		for (int i = 0; i < 16; ++i)
			fakeVtbl[i] = (void *)Trap8::Nop;
		fakeVtbl[0x28 / 4] = (void *)Fake8::GetScopeId;
		fakeVtbl[0x2c / 4] = (void *)Fake8::QueryFlag;

		void *fakeObj = fakeVtbl;
		void *realEditApi = EditApi;
		EditApi = &fakeObj;

		StgUnsolMsgHandlerTestHooks::SetForceSaveOnEnd(handler, 1);
		handler.EndHandling(); /* real fork/execve/waitpid + sleep(3) + STGMessage send -- no crash expected */
		check("EndHandling() with mForceSaveOnEnd set + flag==0: does not crash "
		      "(real SaveRandomSeed()/sync()/sleep(3)/ForceErPShutdown() path)",
		      true);

		EditApi = realEditApi;
	}

	/* --- Tier A, batch 5 (2026-07-26): ProgramSlotMsgHandler --------------------
	 *
	 * Promoted from Tier B this session (follow-up to the 5-handler recheck).
	 * CModeManager::IsOnTimbreProgramEditInContext() is a file-local opaque stub
	 * hardcoded to always return false (stg_unsol_msg_handler.cpp) -- same
	 * "genuinely unmodeled external subsystem, confirmed real but currently dead"
	 * status as s_eNowRestoreSeqParameters elsewhere in this file, not something
	 * this test can force true -- so only the "not in timbre edit" halves of the
	 * idx==8/idx==9 special cases are exercised here; their ChangeToTopPage/
	 * CKGMsgProcessor tails are real but unreachable from this test harness,
	 * same license already used for CPanelIfcTask/USTGAPIControl-reaching tails
	 * elsewhere in this file.
	 */
	printf("[10] ProgramSlotMsgHandler (Tier A batch 5, 2026-07-26, real EditApi dispatch)\n");
	{
		struct Capture {
			bool called;
			const char *scopeName;
			unsigned char scope, code, value;
			unsigned int payload;
			int len, flag;
		};
		static Capture cap;

		struct Fake10 {
			static unsigned char GetScopeId(void *, const char *name)
			{
				cap.scopeName = name;
				return 0x40;
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
			static void QueryFlag(void *, unsigned char, int, int, unsigned char *out, int len)
			{
				memset(out, 0, (size_t)len); /* forces the "queried != payload" ChangeToTopPage
				                               * branch to never look equal by accident in any
				                               * test below that reaches idx==8/9's own query --
				                               * moot in practice since IsOnTimbreProgramEditInContext
				                               * always returns false, gating those branches off. */
			}
		};

		struct Trap10 { static void Nop() {} };
		void *fakeVtbl[16];
		for (int i = 0; i < 16; ++i)
			fakeVtbl[i] = (void *)Trap10::Nop;
		fakeVtbl[0x28 / 4] = (void *)Fake10::GetScopeId;
		fakeVtbl[0x2c / 4] = (void *)Fake10::QueryFlag;
		fakeVtbl[0x30 / 4] = (void *)Fake10::SetParam;

		void *fakeObj = fakeVtbl;
		void *realEditApi = EditApi;
		EditApi = &fakeObj;

		unsigned char buf[64];

		/* p+8 != 1: must return without dispatch. */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 0;
			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);
			check("ProgramSlotMsgHandler: p+8 != 1 returns without dispatch", !cap.called);
		}

		/* idx == 0x4a (special case A): kind==0 (Combi) -> "ESCombi", code =
		 * p[0x14](5) + table[0x4a*2](0x48) = 0x4d, value = p[0x28](0x20) +
		 * table[0x4a*2+1](0x10) = 0x30, payload = &msg[0x20] verbatim, len=4.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 0;                 /* kind == Combi */
			*(unsigned int *)(buf + 0x10) = 0xffff;   /* wildcard target */
			buf[0x14] = 5;
			buf[0x28] = 0x20;
			*(int *)(buf + 0x1c) = 0x4a;               /* idx */
			*(unsigned int *)(buf + 0x20) = 0xdeadbeef;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=0x4a: dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=0x4a: scope name == \"ESCombi\"", cap.scopeName && strcmp(cap.scopeName, "ESCombi") == 0);
			check("ProgramSlotMsgHandler idx=0x4a: code == 0x4d", cap.code == 0x4d);
			check("ProgramSlotMsgHandler idx=0x4a: value == 0x30", cap.value == 0x30);
			check("ProgramSlotMsgHandler idx=0x4a: payload == msg[0x20] verbatim", cap.payload == 0xdeadbeef);
			check("ProgramSlotMsgHandler idx=0x4a: len == 4, flag == 1", cap.len == 4 && cap.flag == 1);
		}

		/* idx == 9 (special case B), not in timbre-edit context (always false
		 * here): code = p[0x14](2) + table[9*2](0x48) = 0x4a, value =
		 * table[9*2+1] = 0 (unmodified), payload16 = (p[0x28](1)<<7)|p[0x20]
		 * word(5) = 0x85, len == 2.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 0;                 /* kind == Combi -> "ESCombi" */
			*(unsigned int *)(buf + 0x10) = 0xffff;
			buf[0x14] = 2;
			*(int *)(buf + 0x28) = 1;
			*(uint16_t *)(buf + 0x20) = 5;
			*(int *)(buf + 0x1c) = 9;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=9: dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=9: code == 0x4a (2+0x48)", cap.code == 0x4a);
			check("ProgramSlotMsgHandler idx=9: value == 0 (table[9] unmodified)", cap.value == 0);
			check("ProgramSlotMsgHandler idx=9: payload == 0x85 ((1<<7)|5)", cap.payload == 0x85);
			check("ProgramSlotMsgHandler idx=9: len == 2, flag == 1", cap.len == 2 && cap.flag == 1);
		}

		/* idx == 0xb, kind==0 (Combi): decrements msg[0x20]'s own byte
		 * (5 -> 4), falls into the generic tail with SVar6=0: code =
		 * p[0x14](7) + table[0xb*2](0x48) = 0x4f, value = 0 + table[0xb*2+1]
		 * (2) = 2, flag = kCSWTCH_231[midiSource=3] = 1, payload = &msg[0x20]
		 * (now holding the decremented value, 4).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 0;                 /* kind == Combi */
			*(unsigned int *)(buf + 0x10) = 0xffff;
			*(uint16_t *)(buf + 2) = 3;                /* midiSource */
			buf[0x14] = 7;
			*(int *)(buf + 0x1c) = 0xb;
			*(uint32_t *)(buf + 0x20) = 5;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=0xb: dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=0xb: code == 0x4f (7+0x48)", cap.code == 0x4f);
			check("ProgramSlotMsgHandler idx=0xb: value == 2 (table[0xb] byte1)", cap.value == 2);
			check("ProgramSlotMsgHandler idx=0xb: payload == 4 (msg[0x20] decremented 5->4)", cap.payload == 4);
			check("ProgramSlotMsgHandler idx=0xb: flag == 1 (kCSWTCH_231[3])", cap.flag == 1);
		}

		/* idx == 5 (plain-default group), kind==2 (Song) -> "ESSong": SVar6=0,
		 * code = p[0x14](3) + table[5*2](0x48) = 0x4b, value = 0 +
		 * table[5*2+1](0x1e) = 0x1e.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 2;                 /* kind == Song -> "ESSong" */
			*(unsigned int *)(buf + 0x10) = 0xffff;
			buf[0x14] = 3;
			*(int *)(buf + 0x1c) = 5;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=5 (default): dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=5 (default): scope name == \"ESSong\"", cap.scopeName && strcmp(cap.scopeName, "ESSong") == 0);
			check("ProgramSlotMsgHandler idx=5 (default): code == 0x4b (3+0x48)", cap.code == 0x4b);
			check("ProgramSlotMsgHandler idx=5 (default): value == 0x1e (SVar6=0 + table[5] byte1)", cap.value == 0x1e);
		}

		/* idx == 0x4b (SVar6 = msg[0x18]): code = p[0x14](1) + table[0x4b*2]
		 * (0x48) = 0x49, value = msg[0x18](9) + table[0x4b*2+1](0x56) = 0x5f.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 0;
			*(unsigned int *)(buf + 0x10) = 0xffff;
			buf[0x14] = 1;
			buf[0x18] = 9;
			*(int *)(buf + 0x1c) = 0x4b;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=0x4b: dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=0x4b: code == 0x49 (1+0x48)", cap.code == 0x49);
			check("ProgramSlotMsgHandler idx=0x4b: value == 0x5f (9+0x56)", cap.value == 0x5f);
		}

		/* idx > 0x4c (out of range): SVar6 = 0, generic tail still reached
		 * (real ground truth reaches the same tail; this reconstruction
		 * clamps the table index rather than reading OOB, see .cpp header).
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 0;
			*(unsigned int *)(buf + 0x10) = 0xffff;
			buf[0x14] = 1;
			*(int *)(buf + 0x1c) = 0x100;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx>0x4c: dispatched (still reaches the generic tail)", cap.called);
		}

		/* idx == 0xe (kind != 0 -> "ESSong" direct-store-PMR-status special
		 * case): songIdx = 0xe-0xe = 0, table = (0x6a, 0x00), code = table[0]
		 * = 0x6a, value = p[0x14](4) + table[1](0) = 4, payload = p[0x20]
		 * low byte, len == 1.
		 */
		{
			memset(buf, 0, sizeof(buf));
			*(int *)(buf + 8) = 1;
			*(int *)(buf + 0x24) = 1;                 /* kind == Prog -> "ESSong" branch entry */
			*(unsigned int *)(buf + 0x10) = 0xffff;
			buf[0x14] = 4;
			*(int *)(buf + 0x20) = 0x77;
			*(int *)(buf + 0x1c) = 0xe;

			cap = Capture();
			STGMessage &msg = *(STGMessage *)buf;
			handler.ProgramSlotMsgHandler(msg);

			check("ProgramSlotMsgHandler idx=0xe (song direct-store): dispatched", cap.called);
			check("ProgramSlotMsgHandler idx=0xe: scope name == \"ESSong\"", cap.scopeName && strcmp(cap.scopeName, "ESSong") == 0);
			check("ProgramSlotMsgHandler idx=0xe: code == 0x6a (table[0])", cap.code == 0x6a);
			check("ProgramSlotMsgHandler idx=0xe: value == 4 (4+0)", cap.value == 4);
			check("ProgramSlotMsgHandler idx=0xe: payload == 0x77 (msg[0x20] low byte), len == 1", cap.payload == 0x77 && cap.len == 1);
		}

		EditApi = realEditApi;
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
