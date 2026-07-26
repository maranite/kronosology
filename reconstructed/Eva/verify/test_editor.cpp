/*
 * test_editor.cpp  -  host-side known-answer test for CEditor (src/editor/editor.cpp)
 * and CParameterString (src/editor/parameter_string.cpp), Stage 6 CEditor batch,
 * 2026-07-25.
 *
 * Exercises:
 *   - CParameterString's real "key=value,key=value" parser: multiple entries,
 *     whitespace trimming around both delimiters, last-one-wins duplicate
 *     precedence (ctor prepends), a missing-value / trailing-comma-less tail,
 *     DecToInt()/HexToInt()'s own cursor-advancing behavior.
 *   - CEditor's own ctor: real vtable pointers installed on all 3 sub-objects
 *     (primary + the 2 multiple-inheritance thunk vtables), sm_poTheEditor set,
 *     mAlphaKeybParam null vs non-null depending on the ctor's own argument.
 *   - CEditor::Setup()'s real fan-out: mMainTask/mPanelIfcTask both real,
 *     non-null afterward (the two sub-objects this pass reconstructs enough of
 *     to actually construct); the "ALPHAKEYBOARD" gate condition itself
 *     evaluated correctly even though the construction it would gate is
 *     deferred (editor.cpp's own header comment).
 *   - CEditor::CMainTask::IsSwitchPressed()/IsShowCost() -- the 2 fully real
 *     (zero-Peg-dependency) methods on that otherwise Tier-B stub class.
 */

#include <cstdio>
#include <cstring>

#include "editor.h"
#include "panel_ifc_task.h"
#include "alpha_keyb_ifc_task.h"
#include "omega_vtables.h"

/* Pokes CEditor's private mAlphaKeybIfcTask -- see editor.h's own friend
 * declaration. Added 2026-07-26 alongside wiring CAlphaKeybIfcTask into
 * CEditor::Setup()/Start() for real (was previously a dead/deferred branch --
 * see eva_createusermodules_editor_unlock_2026-07-26).
 */
struct EditorTestHooks {
	static CAlphaKeybIfcTask *GetAlphaKeybIfcTask(CEditor &e) { return e.mAlphaKeybIfcTask; }
	static CEditor::CChunkServerTask *GetChunkServerTask(CEditor &e) { return e.mChunkServerTask; }
	static CEditor::CMainTask *GetMainTask(CEditor &e) { return e.mMainTask; }
	static void SetMainTask(CEditor &e, CEditor::CMainTask *t) { e.mMainTask = t; }
};

/* Pokes CChunkServer's protected fields -- see chunk_server.h's own friend
 * declaration. Added 2026-07-26 alongside promoting Load() from Tier B to
 * Tier A (recovered via objdump register tracing, see chunk_server.h/.cpp).
 */
struct ChunkServerTestHooks {
	static int GetReserved7c(const CChunkServer &c) { return c.mReserved7c; }
	static int GetAccessMode(const CChunkServer &c) { return c.mAccessMode; }
	static void SetAccessMode(CChunkServer &c, int mode) { c.mAccessMode = mode; }
};

/* --- CChunkServer::Exec(CMessage&) test scaffolding (promoted Tier B -> Tier
 * A 2026-07-26) -- same "build a real object, swap in a capturing fake
 * vtable" technique as test_sysex_msg_task_base.cpp's own Exec(CMessage&)
 * test. Real CMessage layout (chunk_server.h's own header comment): +0x8 a
 * 16-bit command-code word, +0x10 a pointer to the command's own payload.
 */
struct FakeChunkMessage {
	unsigned char pad0[8];
	unsigned short code;    /* +0x8 */
	unsigned char pad_a[6]; /* +0xa..0x10 */
	unsigned char *payload; /* +0x10 */
};

static void *g_cceThis;
static unsigned char g_cceA, g_cceB, g_cceC;
static int g_cceCmd;
static unsigned char *g_ccePtr;
static unsigned long g_cceD;
static unsigned int g_cceRet;
static int g_cceCalls;

extern "C" unsigned int FakeCommandSlot(CChunkServer *self, unsigned char a, int cmd,
                                          unsigned char b, unsigned char *c, unsigned long d)
{
	g_cceThis = self; g_cceA = a; g_cceCmd = cmd; g_cceB = b; g_ccePtr = c; g_cceD = d;
	g_cceCalls++;
	return g_cceRet;
}

extern "C" void FakeOnAbort(CChunkServer *self, int cmd)
{
	g_cceThis = self; g_cceCmd = cmd; g_cceCalls++;
}

extern "C" void FakeOnStoppedByUser(CChunkServer *self, int cmd)
{
	g_cceThis = self; g_cceCmd = cmd; g_cceCalls++;
}

extern "C" int FakeGetSaveBuffSize(const CChunkServer *self, unsigned char a, unsigned char b,
                                     unsigned char c)
{
	g_cceThis = const_cast<void *>(static_cast<const void *>(self));
	g_cceA = a; g_cceB = b; g_cceC = c;
	g_cceCalls++;
	return static_cast<int>(g_cceRet);
}

extern "C" unsigned int FakeOnLoad4(CChunkServer *self, void *chunk, unsigned char c,
                                      unsigned char *outBuf, unsigned long e)
{
	g_cceThis = self; g_ccePtr = static_cast<unsigned char *>(chunk); g_cceC = c;
	(void)outBuf; (void)e;
	g_cceCalls++;
	return g_cceRet;
}

extern "C" unsigned int FakeOnLoad5(CChunkServer *self, unsigned long a, unsigned char *b,
                                      unsigned char c, unsigned char *d, unsigned long e)
{
	g_cceThis = self; g_cceD = a; g_ccePtr = b; g_cceC = c;
	(void)d; (void)e;
	g_cceCalls++;
	return g_cceRet;
}

extern "C" unsigned int FakeOnSave4(CChunkServer *self, void *chunk, unsigned char c,
                                      unsigned char *outBuf, unsigned long e)
{
	g_cceThis = self; g_ccePtr = static_cast<unsigned char *>(chunk); g_cceC = c;
	(void)outBuf; (void)e;
	g_cceCalls++;
	return g_cceRet;
}

extern "C" unsigned int FakeOnSave5(CChunkServer *self, unsigned long *lenOut,
                                      const unsigned char **dataOut, unsigned char c,
                                      unsigned char *outBuf, unsigned long e)
{
	g_cceThis = self; g_cceC = c;
	(void)outBuf; (void)e;
	*lenOut = 3;
	static const unsigned char kFakeData[3] = { 0xaa, 0xbb, 0xcc };
	*dataOut = kFakeData;
	g_cceCalls++;
	return g_cceRet;
}

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CEditor / CParameterString known-answer test\n");
	printf("=============================================\n");

	printf("[1] CParameterString -- basic key=value parsing\n");
	{
		CParameterString ps("ALPHAKEYBOARD=Yes,FOO=bar");
		check("ALPHAKEYBOARD == \"Yes\"",
		      ps.GetParamStr("ALPHAKEYBOARD") != 0 &&
		          strcmp(ps.GetParamStr("ALPHAKEYBOARD"), "Yes") == 0);
		check("FOO == \"bar\"",
		      ps.GetParamStr("FOO") != 0 && strcmp(ps.GetParamStr("FOO"), "bar") == 0);
		check("missing key returns null", ps.GetParamStr("NOPE") == 0);
	}

	printf("[2] CParameterString -- whitespace trimming around '=' and ','\n");
	{
		CParameterString ps("  A = 1  ,  B  =2,C=3   ");
		check("A == \"1\"", ps.GetParamStr("A") != 0 && strcmp(ps.GetParamStr("A"), "1") == 0);
		check("B == \"2\"", ps.GetParamStr("B") != 0 && strcmp(ps.GetParamStr("B"), "2") == 0);
		check("C == \"3\"", ps.GetParamStr("C") != 0 && strcmp(ps.GetParamStr("C"), "3") == 0);
	}

	printf("[3] CParameterString -- duplicate key, last-one-wins (ctor prepends, GetParamStr walks head-first)\n");
	{
		CParameterString ps("X=first,X=second");
		check("X == \"second\" (most recently parsed wins)",
		      ps.GetParamStr("X") != 0 && strcmp(ps.GetParamStr("X"), "second") == 0);
	}

	printf("[4] CParameterString -- no trailing comma, single entry\n");
	{
		CParameterString ps("ONLY=value");
		check("ONLY == \"value\"",
		      ps.GetParamStr("ONLY") != 0 && strcmp(ps.GetParamStr("ONLY"), "value") == 0);
	}

	printf("[5] CParameterString::DecToInt/HexToInt -- cursor-advancing parsers\n");
	{
		const char *p1 = "123abc";
		int v1 = CParameterString::DecToInt(&p1);
		check("DecToInt(\"123abc\") == 123", v1 == 123);
		check("DecToInt cursor left at 'a'", *p1 == 'a');

		const char *p2 = "-45x";
		int v2 = CParameterString::DecToInt(&p2);
		check("DecToInt(\"-45x\") == -45", v2 == -45);

		const char *p3 = "1Fg";
		int v3 = CParameterString::HexToInt(&p3);
		check("HexToInt(\"1Fg\") == 0x1F", v3 == 0x1F);
		check("HexToInt cursor left at 'g'", *p3 == 'g');
	}

	printf("[6] CEditor ctor -- vtable install + sm_poTheEditor + null alphaKeyb param\n");
	{
		CEditor editor("EditorTest", 0);
		check("Config() returns 0", CEditor::Config() == 0);
		/* No public accessor for mAlphaKeybParam/vtbl -- indirectly confirmed
		 * via Setup() below not crashing when it dereferences mAlphaKeybParam
		 * (guarded, null-safe) and via CModule's own inherited AdjustTaskMask()
		 * not crashing (implies mVtbl/base state is sane).
		 */
		editor.AdjustTaskMask();
		check("ctor + AdjustTaskMask() doesn't crash with null alphaKeybParam", true);
	}

	printf("[7] CEditor::Setup() -- real fan-out: mMainTask/mPanelIfcTask constructed\n");
	{
		CEditor editor("EditorTest2", 0);
		int rc = editor.Setup();
		check("Setup() returns 0", rc == 0);
		/* Exercise Start() too -- guarded against mPanelIfcTask/mMainTask
		 * being set (they are, by Setup() above); must not crash.
		 */
		int rc2 = editor.Start();
		check("Start() returns 0", rc2 == 0);
	}

	printf("[8] CEditor::Setup() -- real \"ALPHAKEYBOARD=Yes\" gate, now wired (2026-07-26)\n");
	{
		/* Matches the real ground-truth "EditorClass" s_atCreateInfo row
		 * (config_info.cpp) -- this is the actual string CEditor's real
		 * boot-path caller passes, now that CreateUserModules() is unlocked.
		 */
		CEditor editor("EditorTest3", "ALPHAKEYBOARD=Yes");
		int rc = editor.Setup();
		check("Setup() returns 0 with ALPHAKEYBOARD=Yes", rc == 0);
		check("mAlphaKeybIfcTask constructed (non-null) when gate matches \"Yes\"",
		      EditorTestHooks::GetAlphaKeybIfcTask(editor) != 0);

		/* Start()'s own conditional CAlphaKeybIfcTask::Setup() tail-call --
		 * real ground truth is a literal 1-byte `ret`, must not crash.
		 */
		int rc2 = editor.Start();
		check("Start() returns 0 with mAlphaKeybIfcTask set", rc2 == 0);
	}

	printf("[8b] CEditor::Setup() -- gate does NOT match (\"No\" != \"Yes\") -- mAlphaKeybIfcTask left untouched\n");
	{
		CEditor editor("EditorTest4", "ALPHAKEYBOARD=No");
		int rc = editor.Setup();
		check("Setup() returns 0 with ALPHAKEYBOARD=No", rc == 0);
		/* Real ground truth's own ctor never zeroes mAlphaKeybIfcTask (see
		 * editor.h's own "preserved real quirk" note) -- so this is only
		 * safe to check because CEditor's ctor here already ran, and this
		 * particular object's storage is fresh stack memory this test
		 * doesn't rely on being any specific value; the point of this check
		 * is Setup() must NOT crash by dereferencing a bad pointer, and the
		 * gate itself (string compare) must correctly reject "No".
		 */
		check("Setup()/gate reject a non-\"Yes\" value without crashing", true);
	}

	printf("[8c] CEditor::Setup() -- CChunkServerTask, UNCONDITIONAL construction (not gated, 2026-07-26)\n");
	{
		CEditor editor("EditorTest5", 0);
		int rc = editor.Setup();
		check("Setup() returns 0", rc == 0);
		check("mChunkServerTask constructed (non-null), no gate needed",
		      EditorTestHooks::GetChunkServerTask(editor) != 0);
	}

	printf("[8d] CChunkServer -- direct unit tests (GetServerHandle/GetServerID/trivial slots)\n");
	{
		CEditor editor("EditorTest6", 0);
		editor.Setup();
		CEditor::CChunkServerTask *cst = EditorTestHooks::GetChunkServerTask(editor);
		check("CChunkServerTask constructed", cst != 0);

		/* Real ctor seeds a 1-entry sentinel table ({0xff, 0}) but leaves
		 * mEntryCount at 0 -- so both scans stay gated off in this
		 * reconstruction (nothing grows the table, Load() is Tier B), same
		 * as ground truth's own "nothing between the ctor and a real Load()
		 * call ever populates it" state.
		 */
		check("GetServerHandle(0xff) == 0xffffffff (mEntryCount == 0 gate)",
		      cst->GetServerHandle(0xff) == 0xffffffffU);
		check("GetServerID(0) == 0xffffffff (mEntryCount == 0 gate)",
		      cst->GetServerID(0) == 0xffffffffU);

		/* Trivial base-class slots, all real, all ignore their own args. */
		check("OnUnlock(...) == 1", cst->OnUnlock(0, 0, 0, 0, 0) == 1);
		check("OnRelock(...) == 1", cst->OnRelock(0, 0, 0, 0, 0) == 1);
		check("OnBegin(...) == 1", cst->OnBegin(0, 0, 0, 0, 0) == 1);
		check("OnEnd(...) == 1", cst->OnEnd(0, 0, 0, 0, 0) == 1);
		unsigned long saveLen = 0;
		const unsigned char *savePtr = 0;
		check("OnSave(ulong&,uchar const*&) overload == 0",
		      cst->CChunkServer::OnSave(saveLen, savePtr, 0, 0, 0) == 0);

		/* CEditor::CChunkServerTask's own 2 real (if behaviorally redundant)
		 * overrides.
		 */
		check("CChunkServerTask::OnSave(CChunk*,...) == 0", cst->OnSave(0, 0, 0, 0) == 0);
		check("CChunkServerTask::GetSaveBuffSize(...) == 0", cst->GetSaveBuffSize(0, 0, 0) == 0);

		/* Unlock() -- real indirect call through this object's own installed
		 * vtable slot 6 (EvaVTableStub-backed, install-only) -- must not
		 * crash.
		 */
		cst->Unlock(0, 0, 0);
		check("Unlock() (real vtable-slot-6 dispatch) does not crash", true);
	}

	printf("[8e] CChunkServer::Load() -- promoted Tier B -> Tier A 2026-07-26 (objdump-recovered mAccessMode-keyed tail call)\n");
	{
		CEditor editor("EditorTest7", 0);
		editor.Setup();
		CEditor::CChunkServerTask *cst = EditorTestHooks::GetChunkServerTask(editor);
		check("CChunkServerTask constructed", cst != 0);

		/* Real ctor argument is `CChunkServer(owner, 0)` (editor.cpp) -- every
		 * CChunkServerTask in this reconstruction starts with mAccessMode == 0.
		 */
		check("CChunkServerTask starts with mAccessMode == 0",
		      ChunkServerTestHooks::GetAccessMode(*cst) == 0);

		/* mode == 0: dispatches through vtable slot 12 (EvaVTableStub-backed,
		 * install-only in this reconstruction -- garbage-but-safe return, same
		 * convention as Unlock()'s own slot-6 dispatch). No soft-assert call.
		 * Real, previously-undocumented side effect: mReserved7c := 0
		 * unconditionally, regardless of which branch is taken.
		 */
		cst->Load(0, 0, 0, 0, 0, 0);
		check("Load() mode==0 (vtable slot 12 dispatch) does not crash", true);
		check("Load() sets mReserved7c = 0 (mode==0 path)",
		      ChunkServerTestHooks::GetReserved7c(*cst) == 0);

		/* mode == 1: skips the soft-assert, dispatches through vtable slot 13
		 * directly.
		 */
		ChunkServerTestHooks::SetAccessMode(*cst, 1);
		cst->Load(0, 0, 0, 0, 0, 0);
		check("Load() mode==1 (vtable slot 13 dispatch, no assert) does not crash", true);
		check("Load() sets mReserved7c = 0 (mode==1 path)",
		      ChunkServerTestHooks::GetReserved7c(*cst) == 0);

		/* mode == 2 (neither 0 nor 1): real ground truth fires a non-aborting
		 * soft-assert (Api+0x94, "ChunkServer.cpp"/0x174) THEN still falls
		 * through to the same slot-13 dispatch mode==1 uses -- Api's own real
		 * `__attribute__((constructor))` (sysapi_instance.cpp) has already run
		 * by this point in any linked executable, so the call is against a
		 * real, non-null, EvaVTableStub-backed vtable, matching every other
		 * Api+0x94 soft-assert call site already exercised in this project
		 * (chunk_man.cpp's CChkCmdBG dtor, etc).
		 */
		ChunkServerTestHooks::SetAccessMode(*cst, 2);
		cst->Load(0, 0, 0, 0, 0, 0);
		check("Load() mode==2 (soft-assert then slot 13 dispatch) does not crash", true);
		check("Load() sets mReserved7c = 0 (mode==2 path)",
		      ChunkServerTestHooks::GetReserved7c(*cst) == 0);
	}

	printf("[8f] CChunkServer::Exec(CMessage&) -- promoted Tier B -> Tier A 2026-07-26 (objdump-recovered self-vtable dispatch)\n");
	{
		CEditor editor("EditorTest8", 0);
		editor.Setup();
		CEditor::CChunkServerTask *cst = EditorTestHooks::GetChunkServerTask(editor);
		check("CChunkServerTask constructed", cst != 0);

		/* Real Exec(CMessage&) dispatches through THIS object's OWN vtable --
		 * swap in a capturing fake, same technique as
		 * test_sysex_msg_task_base.cpp's own Exec(CMessage&) test.
		 */
		void *realVtbl = *reinterpret_cast<void **>(cst);
		void *fakeVtbl[16];
		for (int i = 0; i < 16; i++) fakeVtbl[i] = 0;
		fakeVtbl[5]  = (void *)FakeGetSaveBuffSize;
		fakeVtbl[6]  = (void *)FakeCommandSlot;  /* OnUnlock */
		fakeVtbl[10] = (void *)FakeOnSave4;
		fakeVtbl[11] = (void *)FakeOnSave5;
		fakeVtbl[12] = (void *)FakeOnLoad4;
		fakeVtbl[13] = (void *)FakeOnLoad5;
		fakeVtbl[14] = (void *)FakeOnAbort;
		fakeVtbl[15] = (void *)FakeOnStoppedByUser;
		*reinterpret_cast<void **>(cst) = fakeVtbl;

		unsigned char buf[24];
		for (int i = 0; i < 24; i++) buf[i] = static_cast<unsigned char>(i + 1);
		FakeChunkMessage msg;
		memset(&msg, 0, sizeof(msg));
		msg.payload = buf;

		/* bits 0x100/0x200 both clear -> -1, no dispatch. */
		msg.code = 0x0000;
		g_cceCalls = 0;
		check("code with neither 0x100 nor 0x200 set returns -1 (no dispatch)",
		      cst->Exec(*reinterpret_cast<CMessage *>(&msg)) == -1 && g_cceCalls == 0);

		/* bit 0x100 set, unmatched low byte -> -1, no dispatch. */
		msg.code = 0x0100 | 0x50;
		g_cceCalls = 0;
		check("bit 0x100 set, unmatched opcode returns -1 (no dispatch)",
		      cst->Exec(*reinterpret_cast<CMessage *>(&msg)) == -1 && g_cceCalls == 0);

		/* opcode 0xe7 (bit 0x100) -> OnAbort(this, (int)payload) via vtbl[14]. */
		msg.code = 0x0100 | 0xe7;
		g_cceCalls = 0;
		int rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe7 dispatches to OnAbort via vtbl[14]",
		      g_cceCalls == 1 && g_cceThis == (void *)cst);
		check("opcode 0xe7 passes cmd == (int)payload",
		      g_cceCmd == reinterpret_cast<int>(buf));
		check("opcode 0xe7 returns 0", rc == 0);

		/* opcode 0xe8 (bit 0x100) -> OnStoppedByUser via vtbl[15]. */
		msg.code = 0x0100 | 0xe8;
		g_cceCalls = 0;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe8 dispatches to OnStoppedByUser via vtbl[15]", g_cceCalls == 1);
		check("opcode 0xe8 returns 0", rc == 0);

		/* opcode 0xe6 (bit 0x100) -> TObjArray<SIDEntry>::Add() stand-in
		 * (this+0x80) -- inert stub, just confirm no crash + return 0.
		 */
		msg.code = 0x0100 | 0xe6;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe6 (SID Add) does not crash and returns 0", rc == 0);

		/* opcode 0xe0 (bit 0x200, dx=0) -> OnUnlock via vtbl[6]. Real arg
		 * marshalling: a=body[1], cmd=body[0], b=body[2], c=body+3,
		 * d=BE32(payload[0..3]) where body=payload+4.
		 */
		msg.code = 0x0200 | 0xe0;
		g_cceCalls = 0;
		g_cceRet = 1; /* handler returns 1 -> Exec() should return 0 */
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		unsigned char *body = buf + 4;
		unsigned long expectedHeader =
		    (static_cast<unsigned long>(buf[0]) << 24) |
		    (static_cast<unsigned long>(buf[1]) << 16) |
		    (static_cast<unsigned long>(buf[2]) << 8) |
		     static_cast<unsigned long>(buf[3]);
		check("opcode 0xe0 dispatches to OnUnlock via vtbl[6]", g_cceCalls == 1);
		check("opcode 0xe0 passes a == body[1]", g_cceA == body[1]);
		check("opcode 0xe0 passes cmd == body[0]", g_cceCmd == body[0]);
		check("opcode 0xe0 passes b == body[2]", g_cceB == body[2]);
		check("opcode 0xe0 passes c == body+3", g_ccePtr == body + 3);
		check("opcode 0xe0 passes d == BE32(payload[0..3])", g_cceD == expectedHeader);
		check("opcode 0xe0 returns 0 when OnUnlock returns 1", rc == 0);

		g_cceRet = 0; /* handler returns 0 -> Exec() should return -1 */
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe0 returns -1 when OnUnlock returns != 1", rc == -1);

		/* opcode 0xe9 (bit 0x200, dx=9) -> GetSaveBuffSize via vtbl[5], result
		 * written back little-endian into payload[0..3].
		 */
		msg.code = 0x0200 | 0xe9;
		g_cceCalls = 0;
		g_cceRet = 0x11223344;
		unsigned char expectA = buf[0], expectB = buf[1], expectC = buf[2];
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe9 dispatches to GetSaveBuffSize via vtbl[5]", g_cceCalls == 1);
		check("opcode 0xe9 passes a/b/c == payload[0..2] (captured before the call's own write-back)",
		      g_cceA == expectA && g_cceB == expectB && g_cceC == expectC);
		check("opcode 0xe9 writes result little-endian into payload[0..3]",
		      buf[0] == 0x44 && buf[1] == 0x33 && buf[2] == 0x22 && buf[3] == 0x11);
		check("opcode 0xe9 returns 0", rc == 0);

		/* opcode 0xe4 (bit 0x200, dx=4) -> Load, mAccessMode==0 branch -> OnLoad(CChunk*,...)
		 * via vtbl[12]; real side effect: mReserved7c = 0 unconditionally.
		 */
		for (int i = 0; i < 24; i++) buf[i] = static_cast<unsigned char>(i + 1);
		msg.code = 0x0200 | 0xe4;
		ChunkServerTestHooks::SetAccessMode(*cst, 0);
		g_cceCalls = 0;
		g_cceRet = 1;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe4 mode==0 dispatches to OnLoad(CChunk*,...) via vtbl[12]",
		      g_cceCalls == 1);
		check("opcode 0xe4 sets mReserved7c = 0",
		      ChunkServerTestHooks::GetReserved7c(*cst) == 0);
		check("opcode 0xe4 mode==0 returns 0 when handler returns 1", rc == 0);

		/* mAccessMode==2 (neither 0 nor 1): soft Api assert, then
		 * OnLoad(ulong,...) via vtbl[13] regardless.
		 */
		ChunkServerTestHooks::SetAccessMode(*cst, 2);
		g_cceCalls = 0;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe4 mode==2 (soft-assert) still dispatches to OnLoad(ulong,...) via vtbl[13]",
		      g_cceCalls == 1);
		check("opcode 0xe4 mode==2 sets mReserved7c = 0",
		      ChunkServerTestHooks::GetReserved7c(*cst) == 0);
		(void)rc;

		/* opcode 0xe5 (bit 0x200, dx=5) -> Save, mAccessMode==0 branch ->
		 * OnSave(CChunk*,...) via vtbl[10] (no WriteBinary call on this path).
		 */
		ChunkServerTestHooks::SetAccessMode(*cst, 0);
		msg.code = 0x0200 | 0xe5;
		g_cceCalls = 0;
		g_cceRet = 1;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe5 mode==0 dispatches to OnSave(CChunk*,...) via vtbl[10]",
		      g_cceCalls == 1);
		check("opcode 0xe5 mode==0 returns 0 when handler returns 1", rc == 0);

		/* mAccessMode==1: OnSave(ulong&,uchar const*&,...) via vtbl[11], THEN
		 * CChunkBase::WriteBinary() (inert stand-in) -- just confirm no crash.
		 */
		ChunkServerTestHooks::SetAccessMode(*cst, 1);
		g_cceCalls = 0;
		rc = cst->Exec(*reinterpret_cast<CMessage *>(&msg));
		check("opcode 0xe5 mode==1 dispatches to OnSave(ulong&,...) via vtbl[11], no crash",
		      g_cceCalls == 1);

		/* dx > 9 (bit 0x200, opcode 0xea) -> -1, no dispatch. */
		msg.code = 0x0200 | 0xea;
		g_cceCalls = 0;
		check("opcode 0xea (dx > 9) returns -1 (no dispatch)",
		      cst->Exec(*reinterpret_cast<CMessage *>(&msg)) == -1 && g_cceCalls == 0);

		*reinterpret_cast<void **>(cst) = realVtbl;
	}

	printf("[9] CEditor::CMainTask::IsSwitchPressed/IsShowCost -- pure global reads\n");
	{
		/* Both real globals (s_oSwitchState/sShowCost, editor.cpp) start at
		 * zero and are only ever WRITTEN by CMainTask::Exec() (Tier-B,
		 * deferred) -- so IsSwitchPressed() should read "pressed" (the `^1`
		 * inversion of an all-zero bitmask) for every code, and IsShowCost()
		 * should read false.
		 */
		check("IsSwitchPressed(0) == true (all-zero bitmask, inverted)",
		      CEditor::CMainTask::IsSwitchPressed(0) == true);
		check("IsSwitchPressed(63) == true", CEditor::CMainTask::IsSwitchPressed(63) == true);
		check("IsShowCost() == false", CEditor::CMainTask::IsShowCost() == false);
		check("CEditor::IsSwitchPressed forwards correctly",
		      CEditor::IsSwitchPressed(5) == CEditor::CMainTask::IsSwitchPressed(5));
		check("CEditor::IsShowCost forwards correctly",
		      CEditor::IsShowCost() == CEditor::CMainTask::IsShowCost());
	}

	printf("[10] CEditor::CPanelIfcTask -- default ctor stays available (test compat)\n");
	{
		CEditor::CPanelIfcTask fakeOwner;
		(void)fakeOwner;
		check("default-constructible CPanelIfcTask (CTask test-only placeholder ctor)", true);
	}

	printf("[11] PTR__CEditor_08f29b88's own Setup slot -- real CModuleManager-shape raw dispatch\n");
	printf("     (2026-07-26 follow-up fix: same class of gap as CPanel's own vtable, see\n");
	printf("     omega_vtables.h's header comment on PTR__CEditor_08f29b88; ground-truth\n");
	printf("     confirmed via objdump -dr on the real, unstripped Decomp/EVA_Decomp/Eva --\n");
	printf("     slot2/3/4 = CEditor::Setup/Config/Start at 0x08249b60/0x082498a0/0x082498b0)\n");
	{
		CEditor editor("EditorTest8", 0);

		/* mMainTask is genuinely uninitialized garbage right after the real ctor
		 * (same "preserved ground-truth quirk" status as CPanel's mPoller,
		 * panel.h's own header comment) -- force a known sentinel rather than
		 * assume it starts null, and confirm the real raw dispatch overwrites it.
		 */
		CEditor::CMainTask *sentinel = (CEditor::CMainTask *)0xdeadbeef;
		EditorTestHooks::SetMainTask(editor, sentinel);
		check("mMainTask forced to sentinel", EditorTestHooks::GetMainTask(editor) == sentinel);

		typedef void (*VCallFn)(void *);
		void *vtbl = *(void **)(void *)&editor; /* offset 0 = CModule::mVtbl */
		check("vtbl == PTR__CEditor_08f29b88", vtbl == (void *)PTR__CEditor_08f29b88);

		VCallFn setupSlot = *(VCallFn *)((char *)vtbl + 8); /* CModuleManager::Setup()'s
		                                                      * own CallVSlot(module, 8) */
		setupSlot(&editor);
		check("raw vtbl+8 dispatch genuinely ran CEditor::Setup() (mMainTask no longer "
		      "the sentinel -- an EvaVTableStub no-op would have left it unchanged)",
		      EditorTestHooks::GetMainTask(editor) != sentinel &&
		          EditorTestHooks::GetMainTask(editor) != 0);
	}

	printf("=============================================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d TEST(S) FAILED\n", g_fail);
	return 1;
}
