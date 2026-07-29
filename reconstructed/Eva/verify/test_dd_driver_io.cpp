/*
 * test_dd_driver_io.cpp  -  host-side known-answer test for CDDriverIO's
 * round-52 13-method batch (solo, 2026-07-29). See include/dd_driver_io.h
 * for the full derivation and the deferred-item list.
 */
#include <cstdio>
#include <cstring>
#include "dd_driver_io.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/*
 * Mocks the unreconstructed CAtaApi/CScsiApi target: a fake object whose
 * +0x0 is a vtable pointer, whose vtable+0x8 slot is this function. Records
 * the last (obj, opcode) pair and writes a per-opcode-configurable response
 * byte into buf[0], with an optional "first N calls of this opcode return 0"
 * override to exercise scsi_req_sense's own retry branches without falling
 * into unbounded recursion (see header comment on why a uniform response
 * would recurse without terminating).
 */
static void *g_lastObj;
static int g_lastOpcode = -1;
static int g_callCount;
static unsigned char g_resp6 = 1, g_resp7 = 1, g_respOther = 1;
static int g_resp6ZeroCount; // first this-many opcode-6 calls return 0
static int g_resp7ZeroCount; // first this-many opcode-7 calls return 0

static void MockExecFn(void *self, int opcode, void *buf)
{
	g_lastObj = self;
	g_lastOpcode = opcode;
	g_callCount++;
	unsigned char v;
	if (opcode == 6 && g_resp6ZeroCount > 0) {
		g_resp6ZeroCount--;
		v = 0;
	} else if (opcode == 7 && g_resp7ZeroCount > 0) {
		g_resp7ZeroCount--;
		v = 0;
	} else if (opcode == 6) {
		v = g_resp6;
	} else if (opcode == 7) {
		v = g_resp7;
	} else {
		v = g_respOther;
	}
	((unsigned char *)buf)[0] = v;
}

static void ResetMock()
{
	g_lastObj = 0;
	g_lastOpcode = -1;
	g_callCount = 0;
	g_resp6 = g_resp7 = g_respOther = 1;
	g_resp6ZeroCount = 0;
	g_resp7ZeroCount = 0;
}

int main()
{
	unsigned char mockVtable[12] = {0};
	unsigned char mockObj[4] = {0};
	*(void **)(mockVtable + 8) = (void *)&MockExecFn;
	*(void **)mockObj = mockVtable;
	CDDriverIO::sm_poDriverApi[0] = mockObj;
	memset(CDDriverIO::devstat_tab, 0, sizeof(CDDriverIO::devstat_tab));

	check("HasInternalCDRW default false", CDDriverIO::HasInternalCDRW() == 0);
	CDDriverIO::EnableDevInfoCache(1);
	check("HasInternalCDRW unaffected by EnableDevInfoCache",
	      CDDriverIO::HasInternalCDRW() == 0);

	int probe = 0x1234;
	CDDriverIO::SetFMBrowseToReport(&probe);
	CDDriverIO::ClearFMBrowseToReport();
	check("SetFMBrowseToReport/ClearFMBrowseToReport: no crash", true);

	check("warmupdrive() == 0", CDDriverIO::warmupdrive() == 0);
	check("cooldowndrive() == 0", CDDriverIO::cooldowndrive() == 0);

	check("rate2speed(0xaf) == 0 (at boundary)", CDDriverIO::rate2speed(0xaf) == 0);
	check("rate2speed(0xb0) == 1 (just above boundary)", CDDriverIO::rate2speed(0xb0) == 1);
	check("rate2speed(0x160) == 2", CDDriverIO::rate2speed(0x160) == 2);
	check("rate2speed(0xffff) == -1 sentinel", CDDriverIO::rate2speed(0xffff) == 0xffffffff);

	check("speed2rate(1) == 0xb0", CDDriverIO::speed2rate(1) == 0xb0);
	check("speed2rate(8) == 0x586", CDDriverIO::speed2rate(8) == 0x586);
	check("speed2rate(0x18) == 0x1090", CDDriverIO::speed2rate(0x18) == 0x1090);
	check("speed2rate(0xff) == -1 sentinel", CDDriverIO::speed2rate(0xff) == 0xffffffff);
	check("speed2rate(unknown) == 0", CDDriverIO::speed2rate(0x7f) == 0);

	check("scsi_mode_sel() == 1 always", CDDriverIO::scsi_mode_sel() == 1);

	// --- prechkdiskchg ---
	ResetMock();
	CDDriverIO::devstat_tab[0] = 0x08;
	check("prechkdiskchg: devstat bit3 set -> true, no driver call",
	      CDDriverIO::prechkdiskchg(0) == true && g_callCount == 0);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0;
	CDDriverIO::s_senseKey = 6;
	CDDriverIO::s_senseAsc = 0;
	CDDriverIO::s_senseAscq = 0;
	check("prechkdiskchg: sense returns non-null, senseKey==6 -> true",
	      CDDriverIO::prechkdiskchg(0) == true);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0;
	CDDriverIO::s_senseKey = 0;
	check("prechkdiskchg: sense returns non-null, senseKey!=6 -> false",
	      CDDriverIO::prechkdiskchg(0) == false);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0x04;
	CDDriverIO::s_senseKey = 0; // != 4/6/2, so the retry itself takes the trivial tail path
	g_resp6ZeroCount = 1; // only the outer opcode-6 call fails; the retry's own succeeds
	check("prechkdiskchg: sense returns null, devstat bit2 set -> true",
	      CDDriverIO::prechkdiskchg(0) == true);

	// --- GetProgress ---
	ResetMock();
	g_respOther = 5; // GetProgress dispatches opcode 0x26 ("other")
	int dummyOut = 0;
	check("GetProgress: dispatches opcode 0x26, returns byte",
	      CDDriverIO::GetProgress(0, &dummyOut) == 5 && g_lastOpcode == 0x26);

	ResetMock();
	CDDriverIO::s_senseKey = 0; // != 4/6/2, keeps the internal retry's own body trivial
	g_respOther = 0; // GetProgress's own opcode 0x26 dispatch fails
	CDDriverIO::GetProgress(0, &dummyOut);
	check("GetProgress: zero response triggers a scsi_req_sense retry (2 calls)",
	      g_callCount == 2);

	// --- ExecuteCommand ---
	ResetMock();
	g_respOther = 7;
	char pbuf[8] = {0};
	pbuf[4] = 0; // device id byte
	check("ExecuteCommand: dispatches msgType, device id from pbuf[4]",
	      CDDriverIO::ExecuteCommand(0x30, pbuf) == 7 && g_lastOpcode == 0x30);

	ResetMock();
	g_respOther = 0;
	char pbuf2[8] = {0};
	pbuf2[4] = 0;
	CDDriverIO::ExecuteCommand(0x30, pbuf2);
	check("ExecuteCommand: zero response (msgType != 0x25) retries via scsi_req_sense",
	      g_callCount == 2);

	ResetMock();
	g_respOther = 0;
	char pbuf3[8] = {0};
	pbuf3[4] = 0;
	CDDriverIO::ExecuteCommand(0x25, pbuf3);
	check("ExecuteCommand: zero response with msgType == 0x25 does NOT retry",
	      g_callCount == 1);

	// --- scsi_req_sense ---
	ResetMock();
	CDDriverIO::s_senseKey = 0; // != 4/6/2, keeps the retry's own body trivial
	g_resp6ZeroCount = 1; // only the outer opcode-6 call fails; the retry's own succeeds
	unsigned char *sense = CDDriverIO::scsi_req_sense(0);
	check("scsi_req_sense: opcode-6 failure -> NULL, discarding the retry's own result",
	      sense == 0 && g_callCount == 2);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0;
	CDDriverIO::s_senseKey = 6;
	sense = CDDriverIO::scsi_req_sense(0);
	check("scsi_req_sense: senseKey==6 sets unit-attention bit3",
	      sense == &CDDriverIO::s_senseKey && (CDDriverIO::devstat_tab[0] & 8) != 0);
	check("scsi_req_sense: falls through to tail, clears busy bit4",
	      (CDDriverIO::devstat_tab[0] & 0x10) == 0);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0x10; // pre-set busy bit, should survive the tail clear... unless overwritten
	CDDriverIO::s_senseKey = 2;
	CDDriverIO::s_senseAsc = 4;
	CDDriverIO::s_senseAscq = 7;
	sense = CDDriverIO::scsi_req_sense(0);
	check("scsi_req_sense: senseKey==2,asc==4,ascq==7 -> early return, sets busy bit4",
	      sense == &CDDriverIO::s_senseKey && (CDDriverIO::devstat_tab[0] & 0x10) != 0);

	ResetMock();
	CDDriverIO::devstat_tab[0] = 0;
	CDDriverIO::s_senseKey = 4;
	CDDriverIO::s_senseAsc = 9;
	CDDriverIO::s_senseAscq = 2;
	sense = CDDriverIO::scsi_req_sense(0);
	check("scsi_req_sense: senseKey==4,asc==9,ascq==2 enters reset_spinup, sets bit2",
	      sense == &CDDriverIO::s_senseKey && (CDDriverIO::devstat_tab[0] & 2) != 0
	      && g_callCount == 3); // opcode6 + 2x opcode7, no recursion (both opcode7 calls succeed)

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
