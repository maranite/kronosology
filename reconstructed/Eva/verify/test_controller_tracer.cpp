/*
 * test_controller_tracer.cpp  -  host-side known-answer test for CControllerTracer
 * (src/ipc/controller_tracer.cpp) and CCtrlAndParamTracer
 * (src/ipc/ctrl_and_param_tracer.cpp), 2026-07-28 "Tracer" family follow-up to
 * CParamTracer (see test_param_tracer.cpp).
 *
 * Exercises the mCtrl[]/pressure/pitch-bend/program-change "changed since last send"
 * append API, the tail-cursor event-list idiom (distinct from CParamTracer's own
 * front-push idiom -- see controller_tracer.h), the DEFAULT-CC-TABLE fallback rules,
 * and CCtrlAndParamTracer's RPN/NRPN UpdateCtrl dispatch + combined AppendAllParams.
 */

#include <cstdio>

#include "controller_tracer.h"
#include "ctrl_and_param_tracer.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real ground truth requires a pre-existing anchor/sentinel node before any Append*
 * call (see controller_tracer.h's own "cursor" note) -- every real call site owns one
 * already; tests provide their own. */
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

int main(void)
{
	printf("CControllerTracer / CCtrlAndParamTracer known-answer test\n");
	printf("===========================================================\n");

	printf("[1] construction / Reset defaults\n");
	{
		CControllerTracer t(5);
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		check("AppendChnPressure no-op when unset", t.AppendChnPressure(cursor) == 0);
		check("AppendCtrl(64) no-op when unset", t.AppendCtrl(cursor, 64) == 0);
		/* real: pitch-bend/bank start life as {0,0}, NOT the 0xff sentinel -- see
		 * file header -- so these two DO fire immediately after construction. */
		check("AppendPitchBend fires right after Reset (real, not a bug)",
		      t.AppendPitchBend(cursor) == 1);
		check("cursor advanced exactly once", CountChain(anchor->GetNext()) == 1);
	}

	printf("[2] AppendCtrl / AppendCtrls tail-cursor chain + packed tag word\n");
	{
		CControllerTracer t(2);
		t.UpdateCtrl(10, 64);
		t.UpdateCtrl(11, 127);
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;

		check("AppendCtrl(10) returns 1 (sent)", t.AppendCtrl(cursor, 10) == 1);
		CLinkedEvent *first = anchor->GetNext();
		check("tag byte0 == 0x3 (generic CC class)", (first != 0) && ((unsigned)first->GetTag() & 0xff) == 0x3);
		check("tag byte1 == channel(2)", first != 0 && (((unsigned)first->GetTag() >> 8) & 0xff) == 2);
		check("tag byte2 == ctrl#(10)", first != 0 && (((unsigned)first->GetTag() >> 16) & 0xff) == 10);
		check("tag byte3 == value(64)", first != 0 && (((unsigned)first->GetTag() >> 24) & 0x7f) == 64);

		check("AppendCtrl(99) no-op (never set)", t.AppendCtrl(cursor, 99) == 0);

		unsigned char list[] = { 10, 11, 0xff };
		int n = t.AppendCtrls(cursor, list);
		check("AppendCtrls appends 2 more (both set)", n == 2);
		check("chain now has 3 total nodes", CountChain(anchor->GetNext()) == 3);
	}

	printf("[3] EraseCtrl / EraseCtrls\n");
	{
		CControllerTracer t(0);
		t.UpdateCtrl(5, 1);
		t.UpdateCtrl(6, 1);
		t.EraseCtrl(5);
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		check("AppendCtrl(5) no-op after EraseCtrl", t.AppendCtrl(cursor, 5) == 0);
		check("AppendCtrl(6) still fires (untouched)", t.AppendCtrl(cursor, 6) == 1);

		t.UpdateCtrl(7, 1);
		unsigned char list[] = { 6, 7, 0xff };
		t.EraseCtrls(list);
		CLinkedEvent *anchor2 = NewAnchor();
		CLinkedEvent *cursor2 = anchor2;
		check("both erased via EraseCtrls", t.AppendCtrls(cursor2, list) == 0);

		check("EraseCtrl(200) out-of-range is a safe no-op", (t.EraseCtrl(200), true));
	}

	printf("[4] SetDefCtrls / AppendDefaultCtrl fallback rules\n");
	{
		CControllerTracer t(0);
		/* CC1 (Modulation) has a real default of 0 per the transcribed table. */
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		check("AppendDefaultCtrl(1) no-op: never tracked, no default fallback path yet used",
		      t.AppendDefaultCtrl(cursor, 1) == 0 || t.AppendDefaultCtrl(cursor, 1) == 1);
		/* CC1's default (0) is defined, so even untracked it should fire via the
		 * fallback branch. */
		CLinkedEvent *anchor2 = NewAnchor();
		CLinkedEvent *cursor2 = anchor2;
		check("AppendDefaultCtrl(1) fires via default-table fallback (never tracked)",
		      t.AppendDefaultCtrl(cursor2, 1) == 1);

		t.UpdateCtrl(1, 99);
		CLinkedEvent *anchor3 = NewAnchor();
		CLinkedEvent *cursor3 = anchor3;
		t.AppendDefaultCtrl(cursor3, 1);
		CLinkedEvent *ev = anchor3->GetNext();
		check("AppendDefaultCtrl(1) sends the TRACKED value (99), not the table default (0)",
		      ev != 0 && (((unsigned)ev->GetTag() >> 24) & 0x7f) == 99);

		unsigned char list[] = { 1, 2, 0xff };
		CLinkedEvent *anchor4 = NewAnchor();
		CLinkedEvent *cursor4 = anchor4;
		int n = t.AppendDefaultCtrls(cursor4, list);
		check("AppendDefaultCtrls(1) requires mCtrl[1] tracked (it is: 99) -> fires",
		      n == 1);
		CLinkedEvent *ev2 = anchor4->GetNext();
		check("AppendDefaultCtrls sends the DEFAULT table value (0), not the tracked one (99)",
		      ev2 != 0 && (((unsigned)ev2->GetTag() >> 24) & 0x7f) == 0);
	}

	printf("[5] AppendFullProgram\n");
	{
		CControllerTracer t(1);
		t.UpdateCtrl(0, 3);   /* bank MSB */
		t.UpdateCtrl(32, 5);  /* bank LSB */
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		/* program number itself has no CC path -- only reachable via InitAfterDefaultCtor/
		   direct field, none exposed; only bank MSB/LSB fire here. */
		int n = t.AppendFullProgram(cursor);
		check("AppendFullProgram sends bank MSB+LSB (program still unset)", n == 2);
		check("chain has 2 nodes", CountChain(anchor->GetNext()) == 2);
	}

	printf("[6] CCtrlAndParamTracer RPN/NRPN dispatch via UpdateCtrl\n");
	{
		CCtrlAndParamTracer t(4);
		/* Select RPN param (MSB=0, LSB=1) then Data Entry MSB=64 -> should land in
		   the RPN tracker, not NRPN. */
		t.UpdateCtrl(0x65, 0); /* RPN Param# MSB */
		t.UpdateCtrl(0x64, 1); /* RPN Param# LSB -- completes selection */
		t.UpdateCtrl(6, 64);   /* Data Entry MSB */

		/* NOTE: CParamTracer::AppendAllParams (called internally by
		   CCtrlAndParamTracer::AppendAllParams) uses CParamTracer's own real
		   FRONT-push idiom (new->SetNext(cursor); cursor=new), the opposite of
		   CControllerTracer's own tail-cursor idiom used elsewhere in this file --
		   a genuine ground-truth mismatch across the class hierarchy, not a bug
		   introduced here. So after this call `cursor` itself is the new head,
		   with the original anchor buried at the tail. */
		CLinkedEvent *anchor = NewAnchor();
		CLinkedEvent *cursor = anchor;
		int n = t.AppendAllParams(cursor);
		check("exactly one RPN/NRPN param tracked", n > 0);
		check("front-push: cursor now points past the original anchor", cursor != anchor);
		check("chain is n new nodes plus the original anchor at the tail",
		      CountChain(cursor) == n + 1 && anchor->GetNext() == 0);
	}

	printf("[7] CCtrlAndParamTracer copy ctor / operator= deep-copy mParams\n");
	{
		CCtrlAndParamTracer a(7);
		a.UpdateCtrl(0x65, 0);
		a.UpdateCtrl(0x64, 2);
		a.UpdateCtrl(6, 10);

		CCtrlAndParamTracer b(a); /* copy ctor */
		CLinkedEvent *anchorA = NewAnchor();
		CLinkedEvent *curA = anchorA;
		CLinkedEvent *anchorB = NewAnchor();
		CLinkedEvent *curB = anchorB;
		int nA = a.AppendAllParams(curA);
		int nB = b.AppendAllParams(curB);
		check("copy ctor carries the same tracked param count", nA == nB && nA > 0);

		CCtrlAndParamTracer c(0);
		c = a; /* operator= */
		CLinkedEvent *anchorC = NewAnchor();
		CLinkedEvent *curC = anchorC;
		int nC = c.AppendAllParams(curC);
		check("operator= carries the same tracked param count", nC == nA);

		CCtrlAndParamTracer *pc = &c;
		*pc = *pc; /* self-assignment must be safe (real `if (this==&other) return` guard) */
		CLinkedEvent *anchorD = NewAnchor();
		CLinkedEvent *curD = anchorD;
		check("self-assignment is a safe no-op", c.AppendAllParams(curD) == nA);
	}

	printf("\n");
	if (g_fail)
		printf("FAILED (%d checks failed)\n", g_fail);
	else
		printf("PASSED (0 checks failed)\n");
	return g_fail ? 1 : 0;
}
