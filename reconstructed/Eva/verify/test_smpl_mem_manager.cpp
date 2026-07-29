/*
 * test_smpl_mem_manager.cpp  -  host-side known-answer test for
 * CSmplMemManager (src/base/smpl_mem_manager.cpp). See
 * include/smpl_mem_manager.h for full ground-truth provenance and the
 * documented deferred-method list.
 *
 * Links against src/base/smpl_mem_manager_stub.cpp, a REAL host-functional
 * backing for the CUsrMultisample/CUsrSample/CUsrDrumsample/CUsrRel/
 * USTGAPIPCMBanks/CSmplModeMgr/CDeviceDesc externs this class depends on --
 * real backing arrays + a shared RAM arena, so allocation, search, and
 * cut/insert/clear/copy round-trips are genuinely exercised, not just
 * pure-logic checks.
 */

#include <cstdio>
#include <cstring>

#include "smpl_mem_manager.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* ---- helpers to reach into the stub's backing records for setup ---- */

static void SetMultisampleName(unsigned int idx, const char *name, unsigned char flag)
{
	CUsrMultisample h;
	h.Init();
	h.Bless(idx);
	char *raw = (char *)h.mResolved;
	memset(raw, 0, 24);
	strncpy(raw, name, 23);
	*((unsigned short *)(raw + 0x18)) = 0;
	*((unsigned char *)(raw + 0x1a)) = flag;
}

static void SetSampleUsed(unsigned int idx, bool used, unsigned short drumIndex, unsigned long sampleRate)
{
	CUsrSample h;
	h.Init();
	h.Bless(idx);
	char *raw = (char *)h.mResolved;
	*(unsigned short *)raw = drumIndex;
	*(unsigned int *)(raw + 0x1c) = used ? 1u : 0u;
	*(unsigned long *)(raw + 0x1c + 4) = sampleRate;
}

static void SetDrumName(unsigned int idx, const char *name)
{
	CUsrDrumsample h;
	h.Bless(idx);
	char *raw = (char *)h.mResolved;
	memset(raw, 0, 24);
	strncpy(raw, name, 23);
}

static void MsAllocTests()
{
	printf("-- multisample slot allocation --\n");
	CSmplMemManager m;

	/* fresh manager: mMsTop==0 -> fast path */
	short a = -99, b = -99;
	m.getnewmsno(0, &a, &b);
	check("getnewmsno fresh: first==0", a == 0);
	check("getnewmsno fresh: second(no wantSecond)==first", b == 0);

	m.getnewmsno(1, &a, &b);
	check("getnewmsno fresh wantSecond: first==0", a == 0);
	check("getnewmsno fresh wantSecond: second==1", b == 1);

	m.incms(0);
	m.incms(5);
	check("incms bumps mMsTop past highest index", m.getfreemsnum(0) == 4000 - 2);

	unsigned char pct = 255;
	int freeCount = m.getfreemsnum(&pct);
	check("getfreemsnum count", freeCount == 4000 - 2);
	check("getfreemsnum pct nonzero for near-empty heap", pct >= 1 && pct <= 100);

	m.decms(5); /* top was 6 (5+1), decms(5) matches mMsTop-1 -- should shrink back to slot 5's neighbourhood */
	m.clearms();
	check("clearms resets count", m.getfreemsnum(0) == 4000);

	m.addms(10);
	/* addms() re-derives mMsCount by scanning [mMsTop, 10) and counting
	 * slots whose RECORD is already flagged used -- it doesn't mark
	 * anything used itself. None of slots 0..9 were ever flagged here, so
	 * the free count stays at the full 4000 (see MsSearchTests below for
	 * the pre-populated-used-slots case). */
	check("addms with no pre-flagged slots leaves count unchanged", m.getfreemsnum(0) == 4000);
}

static void MsSearchTests()
{
	printf("-- multisample slot occupancy / search --\n");
	CSmplMemManager m;
	m.clearms();

	/* mark a few slots "used" (flag!=0) via the stub's raw record access */
	SetMultisampleName(3, "Piano-L", 1);
	SetMultisampleName(4, "Piano-R", 1);
	m.addms(5); /* count scans 0..4, sees slots 3/4 used -> count==2 */
	check("addms counts pre-populated used slots", m.getfreemsnum(0) == 4000 - 2);

	unsigned int slider = m.getslidermsno(0);
	check("getslidermsno(0) returns the first used slot", slider == 3);

	/* multisamplecompare fast path: both target+candidate unused, same name -> true */
	CUsrMultisample cand, target;
	cand.Init();
	target.Init();
	cand.Bless(500);
	target.Bless(501);
	SetMultisampleName(500, "Empty", 0);
	SetMultisampleName(501, "Empty", 0);
	check("multisamplecompare: both-unused trivial match", m.multisamplecompare(&cand, "Empty", &target));

	SetMultisampleName(500, "Piano-L", 1);
	SetMultisampleName(501, "Piano-L", 1);
	check("multisamplecompare: deep path stubbed -> false even with matching name/flag",
	      !m.multisamplecompare(&cand, "Piano-L", &target));

	check("multisamplecompare: name mismatch -> false", !m.multisamplecompare(&cand, "Nope", &target));
}

static void SmplAllocTests()
{
	printf("-- sample slot allocation --\n");
	CSmplMemManager m;
	m.clearsmpl();

	short a = -99, b = -99;
	m.getnewsmplno(0, &a, &b, -1); /* -1 -> use mSmplTop (0) */
	check("getnewsmplno fresh: first==0", a == 0);

	m.getnewsmplno(1, &a, &b, -1);
	check("getnewsmplno fresh wantSecond: first==0", a == 0);
	check("getnewsmplno fresh wantSecond: second==1", b == 1);

	m.incsmpl(0);
	m.incsmpl(1);
	check("incsmpl bumps count", m.getfreesmplnum(0) == 16000 - 2);

	m.clearsmpl();
	check("clearsmpl resets", m.getfreesmplnum(0) == 16000);

	/* explicit startFrom override */
	m.getnewsmplno(0, &a, &b, 100);
	check("getnewsmplno startFrom==100 (still free) returns 100", a == 100);
}

static void SmplCompareTests()
{
	printf("-- samplecompare / drum name matching --\n");
	CSmplMemManager m;

	SetDrumName(7, "Kick-Left");
	SetSampleUsed(200, true, 7, 44100);
	SetSampleUsed(201, true, 7, 44100);
	SetSampleUsed(202, true, 7, 48000);

	CUsrSample s200, s201, s202;
	s200.Init(); s200.Bless(200);
	s201.Init(); s201.Bless(201);
	s202.Init(); s202.Bless(202);

	check("samplecompare: same drum name + same rate -> match",
	      m.samplecompare(&s200, "Kick-Left", &s201));
	check("samplecompare: same drum name + different rate -> no match",
	      !m.samplecompare(&s200, "Kick-Left", &s202));
	check("samplecompare: name mismatch -> no match",
	      !m.samplecompare(&s200, "Kick-Right", &s201));
}

static void RltvTests()
{
	printf("-- relative (loop/attack point) counters --\n");
	CSmplMemManager m;
	m.clearrltv();
	check("clearrltv resets", m.getuserltvnum() == 0);
	m.addrltv(5);
	check("addrltv", m.getuserltvnum() == 5);
	m.decrltv(2);
	check("decrltv", m.getuserltvnum() == 3);
	unsigned char pct = 0;
	int freeCount = m.getfreerltvnum(&pct);
	check("getfreerltvnum count", freeCount == 16000 - 3);
}

static void BankAndRamDataTests()
{
	printf("-- per-bank bookkeeping + RAM byte-range round-trip --\n");
	CSmplMemManager m;

	m.setramsize();
	check("isexistbank(0) true after setramsize (nonzero heap)", m.isexistbank(0));
	check("isexistbank(1) false (unpopulated bank)", !m.isexistbank(1));

	m.setfreetop(0, 1000);
	check("getfreetop reflects setfreetop", m.getfreetop(0) == 1000);

	unsigned long remain = 0;
	m.getremainsize(&remain);
	check("getremainsize(ulong*) nonzero", remain != 0);

	int remainBank = m.getremainsize((unsigned char)0);
	check("getremainsize(bank) matches getremainsize(ulong*)", (unsigned long)remainBank == remain);

	/* real byte round-trip through the RAM arena */
	char src[64];
	for (int i = 0; i < 64; ++i)
		src[i] = (char)(i * 3 + 1);
	m.writedata(0, (char *)0x2000 /* dst offset, cast used as offset per ground truth */, src, 64);

	char dst[64];
	memset(dst, 0, sizeof(dst));
	m.readdata(0, dst, (char *)0x2000, 64);
	check("writedata/readdata round-trip byte-exact", memcmp(src, dst, 64) == 0);

	/* copydata: copy the same 64 bytes from offset 0x2000 to 0x5000 */
	m.copydata(0, 0x5000, 0, 0x2000, 64);
	memset(dst, 0, sizeof(dst));
	m.readdata(0, dst, (char *)0x5000, 64);
	check("copydata round-trip byte-exact", memcmp(src, dst, 64) == 0);

	/* dataclear: zero out the 64 bytes at 0x5000 */
	m.dataclear(0, 0x5000, 64);
	memset(dst, 0xff, sizeof(dst));
	m.readdata(0, dst, (char *)0x5000, 64);
	char zeros[64];
	memset(zeros, 0, sizeof(zeros));
	check("dataclear zeroes the range", memcmp(dst, zeros, 64) == 0);
}

static void HdFreeSizeTests()
{
	printf("-- HD free-size bookkeeping --\n");
	CSmplMemManager m;
	m.refreshhdfreesize((EDevice_Id)0);
	unsigned int before = m.gethdfreesize();
	check("gethdfreesize returns something after refresh", before > 0);

	m.dechdfreesize(0x1000);
	unsigned int after = m.gethdfreesize();
	check("dechdfreesize decreases free size", after == before - 0x1000);
}

int main()
{
	MsAllocTests();
	MsSearchTests();
	SmplAllocTests();
	SmplCompareTests();
	RltvTests();
	BankAndRamDataTests();
	HdFreeSizeTests();

	printf("%s (%d check%s failed)\n", g_fail ? "FAILED" : "PASSED", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
