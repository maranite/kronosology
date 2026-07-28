/*
 * test_seq_pattern_data.cpp  -  host-side known-answer test for
 * CSeqEvent/CSeqPat/CPatternDataHolder/CDrumTrackPatternDataHolder
 * (src/init/seq_pattern_data.cpp). See include/seq_pattern_data.h for full
 * ground-truth provenance.
 *
 * A synthetic CPatternDataHolder-shaped buffer is built by hand (embedded
 * CSeqPat[] array + CSeqEvent[] area, offsets set explicitly via
 * TestHooks) so every accessor/scan/link-chase can be exercised against a
 * known layout without needing any real caller.
 */

#include <cstdio>
#include <cstring>

#include "seq_pattern_data.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct CPatternDataHolderTestHooks {
	static void SetFields(CPatternDataHolder &h, int numPatterns, int numEventSlots,
	                       int patternAreaOffset, int eventAreaOffset)
	{
		h.mNumPatterns = numPatterns;
		h.mNumEventSlots = numEventSlots;
		h.mPatternAreaOffset = patternAreaOffset;
		h.mEventAreaOffset = eventAreaOffset;
	}
	static int UsedPatternCount(const CPatternDataHolder &h) { return h.mUsedPatternCount; }
	static int TotalEventCount(const CPatternDataHolder &h) { return h.mTotalEventCount; }
	static int NumPatterns(const CPatternDataHolder &h) { return h.mNumPatterns; }
	static int NumEventSlots(const CPatternDataHolder &h) { return h.mNumEventSlots; }
};

/* A synthetic holder: 4 CSeqPat slots followed by 32 CSeqEvent slots, all
 * embedded inline (matching ground truth's real "offsets from `this`"
 * layout, not pointers).
 */
struct TestHolder {
	CPatternDataHolder base;
	CSeqPat pats[4];
	CSeqEvent events[32];
};

static void SetEv(CSeqEvent &e, unsigned char type, unsigned char linkA_hi = 0,
                   unsigned char linkA_lo = 0, unsigned char linkB_hi = 0, unsigned char linkB_lo = 0)
{
	memset(&e, 0, sizeof e);
	e.mType = type;
	e.mLinkA[0] = linkA_hi;
	e.mLinkA[1] = linkA_lo;
	e.mLinkB[0] = linkB_hi;
	e.mLinkB[1] = linkB_lo;
}

int main()
{
	printf("test_seq_pattern_data:\n");

	/* [1] CSeqPat::Initialize()/Initialize(name)/SetName() name-field and
	 * metadata behavior.
	 */
	{
		CSeqPat pat;
		memset(&pat, 0xAA, sizeof pat);
		pat.Initialize();
		check("Initialize(): GetEventOffset() == 0xFFFFFFFF", pat.GetEventOffset() == 0xFFFFFFFFu);

		pat.Initialize("Bass1");
		char buf[25];
		memcpy(buf, &pat, 24);
		buf[24] = 0;
		check("Initialize(name): name field == \"Bass1\" + 19 spaces",
		      memcmp(buf, "Bass1                   ", 24) == 0);
		check("Initialize(name): GetEventOffset() == 0xFFFFFFFF", pat.GetEventOffset() == 0xFFFFFFFFu);

		pat.SetName("XY");
		memcpy(buf, &pat, 24);
		check("SetName(name): name field == \"XY\" + 22 spaces",
		      memcmp(buf, "XY                      ", 24) == 0);
		/* SetName(const char*) must not touch mEventOffset. */
		pat.SetEventOffset(0x12345678u);
		pat.SetName("Z");
		check("SetName(name) leaves mEventOffset untouched", pat.GetEventOffset() == 0x12345678u);

		pat.SetName();
		memcpy(buf, &pat, 24);
		check("SetName(): name field == 24 spaces", memcmp(buf, "                        ", 24) == 0);

		/* Name copy stops at first non-printable/high-bit byte. */
		char withCtrl[6] = { 'A', 'B', 0x1f, 'C', 'D', 0 };
		pat.SetName(withCtrl);
		memcpy(buf, &pat, 24);
		check("SetName(): stops at control byte, pads rest with spaces",
		      memcmp(buf, "AB                      ", 24) == 0);
	}

	/* [2] CSeqPat::SetEventOffset()/GetEventOffset() round trip + SetEvent(). */
	{
		CSeqPat pat;
		pat.SetEventOffset(0x03020100u);
		check("SetEventOffset/GetEventOffset round trip", pat.GetEventOffset() == 0x03020100u);

		/* SetEvent(index, ptr): ptr!=0 && ptr>=index -> mEventOffset = ptr-index */
		pat.SetEvent(10, (CSeqEvent *)(uintptr_t)50);
		check("SetEvent(10, ptr=50): mEventOffset == 40", pat.GetEventOffset() == 40u);
		check("GetEvent(index): index + mEventOffset", pat.GetEvent(5) == 45u);

		/* ptr < index -> invalid */
		pat.SetEvent(10, (CSeqEvent *)(uintptr_t)5);
		check("SetEvent(10, ptr=5): invalid -> 0xFFFFFFFF", pat.GetEventOffset() == 0xFFFFFFFFu);
		check("GetEvent(index) on invalid offset -> 0", pat.GetEvent(5) == 0);

		/* ptr == 0 -> invalid */
		pat.SetEventOffset(0x11111111u);
		pat.SetEvent(10, 0);
		check("SetEvent(10, ptr=0): invalid -> 0xFFFFFFFF", pat.GetEventOffset() == 0xFFFFFFFFu);
	}

	/* [3] CSeqPat::GetEvent(unsigned long eventAreaBase, int matchId): link-chain walk. */
	{
		CSeqEvent area[8];
		for (int i = 0; i < 8; i++)
			SetEv(area[i], 0);
		/* area[0]: type 1, link fields (big-endian on disk): b6 (mLinkB) =
		 * target id 0x0007, b4 (mLinkA) = step count 2 (i.e. advance by
		 * 2*8 bytes = 2 slots) if no match.
		 */
		SetEv(area[0], 1, /*A hi*/0x00, /*A lo*/0x02, /*B hi*/0x00, /*B lo*/0x07);
		SetEv(area[2], 0);      /* landed-on slot after following link (area[0]+2) */
		SetEv(area[5], 3);      /* sentinel elsewhere, unrelated */

		CSeqPat pat;
		pat.SetEventOffset(0);  /* pat's events start at area[0] */

		/* matchId == 7 matches area[0]'s own mLinkB directly -> returns area[0] itself. */
		CSeqEvent *r = pat.GetEvent((unsigned long)(uintptr_t)&area[0], 7);
		check("GetEvent(base,matchId): direct match on first link event", r == &area[0]);

		/* matchId != 7 -> follow link (advance by mLinkA=2 slots) to area[2], which
		 * has type!=1 so the walk stops there. */
		r = pat.GetEvent((unsigned long)(uintptr_t)&area[0], 99);
		check("GetEvent(base,matchId): follows link to area[2] on mismatch", r == &area[2]);

		/* Invalid pattern (mEventOffset==0xFFFFFFFF) -> 0. */
		pat.SetEventOffset(0xFFFFFFFFu);
		check("GetEvent(base,matchId): invalid pattern -> null",
		      pat.GetEvent((unsigned long)(uintptr_t)&area[0], 7) == 0);
	}

	/* [4] CPatternDataHolder: full synthetic layout. */
	{
		TestHolder h;
		memset(&h, 0, sizeof h);
		CPatternDataHolderTestHooks::SetFields(h.base, /*numPatterns*/4, /*numEventSlots*/32,
			(int)((char *)&h.pats[0] - (char *)&h.base),
			(int)((char *)&h.events[0] - (char *)&h.base));

		check("GetPatternTop() points at pats[0]", h.base.GetPatternTop() == &h.pats[0]);
		check("GetEventAreaTop()/GetPatternEventTop() point at events[0]",
		      h.base.GetEventAreaTop() == &h.events[0] && h.base.GetPatternEventTop() == &h.events[0]);
		check("GetEventAreaEnd() points at events[31] (last slot)",
		      h.base.GetEventAreaEnd() == &h.events[31]);

		check("GetPat(0)==&pats[0], GetPat(3)==&pats[3]",
		      h.base.GetPat(0) == &h.pats[0] && h.base.GetPat(3) == &h.pats[3]);
		check("GetPat(4) out of range -> null", h.base.GetPat(4) == 0);

		/* Build 2 real patterns:
		 *   pat[0] -> events[0..2] real, events[3] sentinel (4 slots total)
		 *   pat[1] -> events[4] sentinel immediately (1 slot total)
		 *   pat[2], pat[3] -> uninitialized/invalid (0xFFFFFFFF)
		 */
		for (int i = 0; i < 4; i++)
			h.pats[i].Initialize();
		for (int i = 0; i < 32; i++)
			SetEv(h.events[i], 0);
		h.events[3].mType = 3;
		h.events[4].mType = 3;
		/* Go through CPatternDataHolder::SetEvent() (not CSeqPat::SetEvent()
		 * directly) so the holder's own event-area-base pointer is supplied
		 * as the "index" arg -- matching real ground-truth call shape (see
		 * CSeqPat::SetEvent()'s doc comment: mEventOffset = ptr - index, and
		 * every real caller passes the event-area base as `index`).
		 */
		h.base.SetEvent(0, &h.events[0]);
		h.base.SetEvent(1, &h.events[4]);
		/* pats[2]/pats[3] left invalid by Initialize(). */

		unsigned long ev0 = h.base.GetEvent(0);
		check("CPatternDataHolder::GetEvent(0) resolves to &events[0]",
		      ev0 == (unsigned long)(uintptr_t)&h.events[0]);
		check("CPatternDataHolder::GetEvent(2) (invalid pattern) -> 0", h.base.GetEvent(2) == 0);

		check("GetEventDirect(4) == &events[4]", h.base.GetEventDirect(4) == &h.events[4]);

		check("GetNumOfEvent(patIndex=0) == 4 slots through sentinel",
		      h.base.GetNumOfEvent(0) == 4);
		check("GetNumOfEvent(patIndex=1) == 1 (immediate sentinel)",
		      h.base.GetNumOfEvent(1) == 1);
		check("GetNumOfEvent(patIndex=2) == 0 (invalid pattern)",
		      h.base.GetNumOfEvent(2) == 0);

		check("GetNumOfEvent(CSeqEvent*) direct: 4 slots from events[0]",
		      h.base.GetNumOfEvent(&h.events[0], false) == 4);
		check("GetNumOfEvent(null,false) == 0", h.base.GetNumOfEvent((CSeqEvent *)0, false) == 0);

		check("GetNumOfEventsToEnd(0) == 4+1 == 5",
		      h.base.GetNumOfEventsToEnd(0) == 5);
		check("GetTotalNumOfEvents() == GetNumOfEventsToEnd(0)",
		      h.base.GetTotalNumOfEvents() == h.base.GetNumOfEventsToEnd(0));
		check("GetNumOfEventsToEnd(2) == 0 (both remaining patterns invalid)",
		      h.base.GetNumOfEventsToEnd(2) == 0);

		check("GetNextTopEvent(0) finds pattern 1's start (events[4])",
		      h.base.GetNextTopEvent(0) == (unsigned long)(uintptr_t)&h.events[4]);
		check("GetNextTopEvent(1) finds nothing (2,3 invalid) -> 0",
		      h.base.GetNextTopEvent(1) == 0);

		h.base.SetInfo();
		check("SetInfo(): mUsedPatternCount == 2",
		      CPatternDataHolderTestHooks::UsedPatternCount(h.base) == 2);
		check("SetInfo(): mTotalEventCount == 4+1 == 5",
		      CPatternDataHolderTestHooks::TotalEventCount(h.base) == 5);

		/* GetFreeEventTop() depends on mTotalEventCount, set by SetInfo() above. */
		check("GetFreeEventTop() == events[5] after SetInfo()",
		      h.base.GetFreeEventTop() == &h.events[5]);

		/* ClearUnusedArea(): zero from events[5] through events[31]'s first
		 * byte (inclusive length quirk -- see header comment). Poison the
		 * whole tail first so we can tell exactly what got zeroed.
		 */
		memset(&h.events[5], 0xEE, sizeof(CSeqEvent) * (32 - 5));
		h.base.ClearUnusedArea();
		bool fullyZeroed = true;
		for (int i = 5; i < 31; i++) {
			CSeqEvent z;
			memset(&z, 0, sizeof z);
			if (memcmp(&h.events[i], &z, sizeof z) != 0)
				fullyZeroed = false;
		}
		check("ClearUnusedArea(): events[5..30] fully zeroed", fullyZeroed);
		check("ClearUnusedArea(): events[31] byte 0 (mType) zeroed", h.events[31].mType == 0);
		check("ClearUnusedArea(): events[31] byte 1 left untouched (0xEE, length quirk)",
		      h.events[31].mUnknown1 == 0xEE);
	}

	/* [5] CDrumTrackPatternDataHolder::Initialize() literal constants. */
	{
		CDrumTrackPatternDataHolder h;
		memset(&h, 0xAA, sizeof h);
		h.Initialize();
		check("CDrumTrackPatternDataHolder::Initialize(): mNumPatterns == 1000",
		      CPatternDataHolderTestHooks::NumPatterns(h) == 1000);
		check("CDrumTrackPatternDataHolder::Initialize(): mNumEventSlots == 0x13880",
		      CPatternDataHolderTestHooks::NumEventSlots(h) == 0x13880);
		check("CDrumTrackPatternDataHolder::Initialize(): GetPatternTop() offset == 0x18",
		      (char *)h.GetPatternTop() - (char *)&h == 0x18);
		check("CDrumTrackPatternDataHolder::Initialize(): GetEventAreaTop() offset == 0x7d18",
		      (char *)h.GetEventAreaTop() - (char *)&h == 0x7d18);
	}

	printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail ? 1 : 0;
}
