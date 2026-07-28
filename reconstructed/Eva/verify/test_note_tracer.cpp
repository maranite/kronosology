/*
 * test_note_tracer.cpp  -  host-side known-answer test for CNoteTracer
 * (src/ipc/note_tracer.cpp), 2026-07-28 "Tracer" family follow-up to
 * CParamTracer/CControllerTracer/CCtrlAndParamTracer.
 *
 * Exercises the O(1) mNoteIndex[]/mNotes[] insert/remove, the RendundantInsertion
 * retrigger-stacking hook, swap-removal re-indexing, GetLeftMost/GetRightMost, the
 * tail-cursor event-list emission (ListNotesOn/Off, ListSoundsOn/Off, the velocity-
 * delta overload's clamp), and the ClearEntries/RefreshEntries/ResetPendingNotes/
 * Swap() mNoteIndex[] invariant.
 */

#include <cstdio>

#include "note_tracer.h"
#include "event.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static CLinkedEvent *NewAnchor()
{
	CLinkedEvent *a = CLinkedEvent::sm_oEventsPool.GetNewEvent();
	a->SetNext(0);
	return a;
}

static int CountChain(CLinkedEvent *head)
{
	int n = 0;
	while (head != 0) { ++n; head = head->GetNext(); }
	return n;
}

static CNoteTracer::CBufferedNote MakeNote(unsigned char note, unsigned char channel, unsigned char velocity)
{
	CNoteTracer::CBufferedNote n;
	n.mCount = 0;
	n.mChannel = channel;
	n.mNote = note;
	n.mVelocity = velocity;
	return n;
}

int main(void)
{
	printf("CNoteTracer known-answer test\n");
	printf("==============================\n");

	printf("[1] construction / basic insert-remove\n");
	{
		CNoteTracer t(3);
		check("GetLeftMost empty == -1", t.GetLeftMost() == -1);
		check("GetRightMost empty == -1", t.GetRightMost() == -1);

		t.Insert(MakeNote(60, 3, 100));
		t.Insert(MakeNote(64, 3, 80));
		t.Insert(MakeNote(67, 3, 90));
		check("GetLeftMost after 3 inserts == 60", t.GetLeftMost() == 60);
		check("GetRightMost after 3 inserts == 67", t.GetRightMost() == 67);

		t.Remove(64);
		check("GetLeftMost after removing middle note == 60", t.GetLeftMost() == 60);
		check("GetRightMost after removing middle note == 67", t.GetRightMost() == 67);

		/* swap-removal re-indexing: remove the (now) leftmost, the former
		 * rightmost slot must still be independently removable afterward. */
		t.Remove(60);
		t.Remove(67);
		check("GetLeftMost empty again == -1", t.GetLeftMost() == -1);
	}

	printf("[2] Remove() no-op on untracked note\n");
	{
		CNoteTracer t;
		t.Insert(MakeNote(60, 0, 100));
		t.Remove(61); /* not tracked -- must not disturb note 60 */
		check("note 60 still tracked after removing untracked note 61", t.GetLeftMost() == 60);
	}

	printf("[3] RendundantInsertion: repeated note-on stacks, doesn't duplicate the slot\n");
	{
		CNoteTracer t;
		t.Insert(MakeNote(60, 0, 100));
		t.Insert(MakeNote(60, 0, 110)); /* re-trigger while still sounding */
		check("still only one distinct note tracked", t.GetLeftMost() == 60 && t.GetRightMost() == 60);

		/* first Remove() only releases the stacked retrigger, note stays tracked */
		t.Remove(60);
		check("note 60 still tracked after first Remove (pending retrigger)", t.GetLeftMost() == 60);
		/* second Remove() actually releases it */
		t.Remove(60);
		check("note 60 released after second Remove", t.GetLeftMost() == -1);
	}

	printf("[4] growth beyond initial 32-entry capacity\n");
	{
		CNoteTracer t;
		for (int i = 0; i < 40; ++i)
			t.Insert(MakeNote((unsigned char)i, 0, 64));
		check("GetLeftMost after 40 inserts == 0", t.GetLeftMost() == 0);
		check("GetRightMost after 40 inserts == 39", t.GetRightMost() == 39);
		for (int i = 0; i < 40; ++i)
			t.Remove((unsigned char)i);
		check("all 40 notes released", t.GetLeftMost() == -1);
	}

	printf("[5] ListNotesOn / ListNotesOff / ListSoundsOn / ListSoundsOff tag words\n");
	{
		CNoteTracer t(5);
		t.Insert(MakeNote(60, 5, 100));

		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		int n = t.ListNotesOn(cursor);
		check("ListNotesOn returns note count 1", n == 1);
		check("ListNotesOn appended exactly one event", CountChain(anchor->GetNext()) == 1);
		unsigned expectOn = 0x1u | (5u << 8) | (60u << 16) | (100u << 24);
		check("ListNotesOn tag word", (unsigned)anchor->GetNext()->GetTag() == expectOn);

		anchor = NewAnchor();
		cursor = anchor;
		t.ListNotesOff(cursor);
		unsigned expectOff = 0x40000000u | (5u << 8) | (60u << 16);
		check("ListNotesOff tag word (no velocity byte)", (unsigned)anchor->GetNext()->GetTag() == expectOff);

		anchor = NewAnchor();
		cursor = anchor;
		t.ListSoundsOn(cursor);
		unsigned expectSoundOn = 0xdu | (5u << 8) | (60u << 16) | (100u << 24);
		check("ListSoundsOn tag word", (unsigned)anchor->GetNext()->GetTag() == expectSoundOn);

		anchor = NewAnchor();
		cursor = anchor;
		t.ListSoundsOff(cursor);
		unsigned expectSoundOff = 0x4000000cu | (5u << 8) | (60u << 16);
		check("ListSoundsOff tag word", (unsigned)anchor->GetNext()->GetTag() == expectSoundOff);
	}

	printf("[6] ListNotesOn(cursor, velocityDelta) clamp behavior\n");
	{
		CNoteTracer t(0);
		t.Insert(MakeNote(60, 0, 100));

		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		t.ListNotesOn(cursor, (signed char)20); /* 100+20=120, within [0,127] */
		unsigned tag = (unsigned)anchor->GetNext()->GetTag();
		check("velocity+20 == 120, no clamp", ((tag >> 24) & 0xff) == 120);

		anchor = NewAnchor();
		cursor = anchor;
		t.ListNotesOn(cursor, (signed char)50); /* 100+50=150, clamps to 127 */
		tag = (unsigned)anchor->GetNext()->GetTag();
		check("velocity+50 clamps to 127 (positive delta)", ((tag >> 24) & 0xff) == 127);

		CNoteTracer t2(0);
		t2.Insert(MakeNote(60, 0, 5));
		anchor = NewAnchor();
		cursor = anchor;
		t2.ListNotesOn(cursor, (signed char)-20); /* 5-20=-15, clamps to 1 */
		tag = (unsigned)anchor->GetNext()->GetTag();
		check("velocity-20 clamps to 1 (negative delta)", ((tag >> 24) & 0xff) == 1);
	}

	printf("[7] ClearEntries / RefreshEntries invariant\n");
	{
		CNoteTracer t;
		t.Insert(MakeNote(60, 0, 100));
		t.Insert(MakeNote(70, 0, 90));
		t.ClearEntries();
		/* mNotes[] itself is untouched by ClearEntries -- RefreshEntries rebuilds
		 * the index from it, so lookups work again afterward. */
		t.RefreshEntries();
		check("GetLeftMost survives Clear+RefreshEntries round-trip", t.GetLeftMost() == 60);
		t.Remove(60);
		check("Remove works correctly after RefreshEntries rebuild", t.GetLeftMost() == 70);
	}

	printf("[8] ResetPendingNotes discards mSize, ClearEntries does not\n");
	{
		CNoteTracer t;
		t.Insert(MakeNote(60, 0, 100));
		t.ResetPendingNotes();
		check("GetLeftMost empty after ResetPendingNotes", t.GetLeftMost() == -1);
		/* re-inserting the same note number must work cleanly (index was
		 * invalidated, slot count reset) */
		t.Insert(MakeNote(60, 0, 50));
		check("re-insert after ResetPendingNotes works", t.GetLeftMost() == 60);
	}

	printf("[9] friend Swap(CNoteTracer&, CNoteTracer&)\n");
	{
		CNoteTracer a(1), b(2);
		a.Insert(MakeNote(10, 1, 1));
		a.Insert(MakeNote(20, 1, 1));
		b.Insert(MakeNote(90, 2, 1));

		Swap(a, b);
		check("a now holds b's note (90)", a.GetLeftMost() == 90 && a.GetRightMost() == 90);
		check("b now holds a's notes (10,20)", b.GetLeftMost() == 10 && b.GetRightMost() == 20);

		/* index cache must be consistent post-swap: removable independently */
		b.Remove(10);
		check("b's mNoteIndex correctly rebuilt after Swap", b.GetLeftMost() == 20);
		a.Remove(90);
		check("a's mNoteIndex correctly rebuilt after Swap", a.GetLeftMost() == -1);
	}

	printf("[10] copy ctor / operator=\n");
	{
		CNoteTracer src(7);
		src.Insert(MakeNote(30, 7, 5));
		src.Insert(MakeNote(40, 7, 6));

		CNoteTracer copy(src);
		check("copy ctor: left", copy.GetLeftMost() == 30);
		check("copy ctor: right", copy.GetRightMost() == 40);
		copy.Remove(30);
		check("copy is independent of src (src still has 30)", src.GetLeftMost() == 30);

		CNoteTracer dst;
		dst.Insert(MakeNote(99, 0, 1)); /* pre-existing content must be fully replaced */
		dst = src;
		check("operator=: left", dst.GetLeftMost() == 30);
		check("operator=: right", dst.GetRightMost() == 40);
		dst.Remove(30);
		check("dst is independent of src after operator=", src.GetLeftMost() == 30);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
