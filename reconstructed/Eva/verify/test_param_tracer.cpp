/*
 * test_param_tracer.cpp  -  host-side known-answer test for CParamTracer +
 * CEventsPool (src/ipc/param_tracer.cpp, src/ipc/events_pool.cpp), CParamTracer
 * family pass, 2026-07-28.
 *
 * Exercises the sorted-array find/erase/upsert machinery, the DataInc/DataDec
 * 14-bit saturating counter, and end-to-end MIDI CC message emission through the
 * real CEventsPool freelist (confirming the packed tag-word encoding: byte0=0x3,
 * byte1=channel, byte2=CC number, byte3=7-bit value).
 */

#include <cstdio>

#include "param_tracer.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static SBytePair BP(unsigned char b0, unsigned char b1)
{
	SBytePair p; p.b0 = b0; p.b1 = b1; return p;
}

static int CountChain(CLinkedEvent *head)
{
	int n = 0;
	while (head != 0) { ++n; head = head->GetNext(); }
	return n;
}

int main(void)
{
	printf("CParamTracer known-answer test\n");
	printf("===============================\n");

	printf("[1] construction / defaults\n");
	{
		CParamTracer t;
		check("First() == 0 on empty tracer", t.First() == 0);
	}

	printf("[2] SetData insert-or-update, sorted order, Find\n");
	CParamTracer t(3, eNRPN);
	t.SetData(BP(10, 20), BP(1, 2));
	t.SetData(BP(5, 0), BP(9, 9));
	t.SetData(BP(10, 20), BP(7, 7)); /* update, not a second insert */

	const CParamTracer::SParam *p = t.First();
	check("First() is the lower addr after sorted insert", p != 0 && p->mAddr.b0 == 5 && p->mAddr.b1 == 0);
	p = t.Next(p);
	check("Next() reaches the second (updated) entry", p != 0 && p->mAddr.b0 == 10 && p->mAddr.b1 == 20);
	check("update-in-place took effect (data == {7,7}, not a duplicate insert)",
	      p->mData.b0 == 7 && p->mData.b1 == 7);
	check("Next() at the end returns 0", t.Next(p) == 0);

	const CParamTracer::SParam *found = t.Find(BP(5, 0));
	check("Find() locates an existing entry", found != 0 && found->mData.b0 == 9 && found->mData.b1 == 9);
	check("Find() returns 0 for a missing entry", t.Find(BP(1, 1)) == 0);

	found = t.FindEqualOrNext(BP(6, 0));
	check("FindEqualOrNext() returns the next-higher entry", found != 0 && found->mAddr.b0 == 10);
	check("FindEqualOrNext() past the end returns 0", t.FindEqualOrNext(BP(200, 0)) == 0);

	printf("[3] SetDataLSB/SetDataMSB against the cursor address\n");
	{
		CParamTracer t2(0, eRPN);
		t2.SetData(BP(1, 1), BP(0xff, 0xff)); /* selects the cursor via ModifyData-style flow below */
		/* SetDataLSB/MSB key off mCurAddr, which only Reset()/ctor touch in this
		 * reconstruction (real ground truth: some other, not-yet-reconstructed
		 * caller moves the cursor elsewhere) -- exercise the "no matching entry,
		 * insert fresh with the other byte defaulted to 0xff" path directly
		 * against the default cursor (0,0) instead. */
		CParamTracer t3(0, eRPN);
		t3.SetDataLSB(0x33);
		const CParamTracer::SParam *e = t3.Find(BP(0, 0));
		check("SetDataLSB inserts a fresh entry at the cursor addr", e != 0);
		check("SetDataLSB sets LSB, defaults MSB to 0xff", e != 0 && e->mData.b1 == 0x33 && e->mData.b0 == 0xff);
		t3.SetDataMSB(0x44);
		e = t3.Find(BP(0, 0));
		check("SetDataMSB updates the SAME existing entry (MSB set, LSB kept)",
		      e != 0 && e->mData.b0 == 0x44 && e->mData.b1 == 0x33);
	}

	printf("[4] Erase (single + kInvalidBytePair-terminated list)\n");
	t.Erase(BP(5, 0));
	check("Erase(single) removes exactly that entry", t.Find(BP(5, 0)) == 0);
	check("Erase(single) leaves the other entry intact", t.Find(BP(10, 20)) != 0);

	CParamTracer t4(0, eRPN);
	t4.SetData(BP(1, 0), BP(0, 0));
	t4.SetData(BP(2, 0), BP(0, 0));
	t4.SetData(BP(3, 0), BP(0, 0));
	SBytePair eraseList[3] = { BP(1, 0), BP(3, 0), kInvalidBytePair };
	t4.Erase(eraseList);
	check("Erase(list) removes every named entry", t4.Find(BP(1, 0)) == 0 && t4.Find(BP(3, 0)) == 0);
	check("Erase(list) leaves the untouched entry", t4.Find(BP(2, 0)) != 0);

	printf("[5] DataInc/DataDec 14-bit saturating counter\n");
	{
		CParamTracer t5;
		t5.Reset(); /* cursor -> kInvalidBytePair == {0,0} */
		t5.SetData(BP(0, 0), BP(0, 0x7e));
		t5.DataInc();
		const CParamTracer::SParam *e = t5.Find(BP(0, 0));
		check("DataInc LSB 0x7e -> 0x7f (no carry yet)", e->mData.b0 == 0 && e->mData.b1 == 0x7f);
		t5.DataInc();
		e = t5.Find(BP(0, 0));
		check("DataInc LSB 0x7f -> carries into MSB, LSB resets to 0",
		      e->mData.b0 == 1 && e->mData.b1 == 0);
		t5.DataDec();
		e = t5.Find(BP(0, 0));
		check("DataDec MSB 1/LSB 0 -> borrows, MSB 0 / LSB 0x7f",
		      e->mData.b0 == 0 && e->mData.b1 == 0x7f);
	}

	printf("[6] AppendSingleParam MIDI CC emission (via real CEventsPool)\n");
	{
		CParamTracer t6(4, eNRPN); /* channel 4, NRPN base CC98/99 */
		CParamTracer::SParam param;
		param.mAddr.b0 = 10;
		param.mAddr.b1 = 20;
		param.mData.b0 = 30;
		param.mData.b1 = 40;

		CLinkedEvent *list = 0;
		SBytePair lastAddr = kInvalidBytePair;
		int count = t6.AppendSingleParam(list, lastAddr, param);

		check("AppendSingleParam appends all 4 messages (fresh addr, both data bytes set)", count == 4);
		check("AppendSingleParam's list has exactly 4 nodes", CountChain(list) == 4);
		check("lastAddr updated to the param's own address", lastAddr.b0 == 10 && lastAddr.b1 == 20);

		/* Pushed front-first: Data LSB, Data MSB, Addr LSB, Addr MSB (reverse of
		 * build order) -- so list[0] is the LAST one built (Data LSB, CC 0x26). */
		int tag0 = *(int *)list;
		check("event[0] tag byte0 == 0x3 (class code)", (tag0 & 0xff) == 0x3);
		check("event[0] tag byte1 == channel 4", ((tag0 >> 8) & 0xff) == 4);
		check("event[0] tag byte2 == CC 0x26 (Data Entry LSB)", ((tag0 >> 16) & 0xff) == 0x26);
		check("event[0] tag byte3 == value 40", ((tag0 >> 24) & 0xff) == 40);

		CLinkedEvent *n1 = list->GetNext();
		int tag1 = *(int *)n1;
		check("event[1] tag byte2 == CC 6 (Data Entry MSB)", ((tag1 >> 16) & 0xff) == 6);
		check("event[1] tag byte3 == value 30", ((tag1 >> 24) & 0xff) == 30);

		CLinkedEvent *n2 = n1->GetNext();
		int tag2 = *(int *)n2;
		check("event[2] tag byte2 == CC 98 (NRPN addr LSB, ccBase itself)", ((tag2 >> 16) & 0xff) == 98);
		check("event[2] tag byte3 == addr LSB 20", ((tag2 >> 24) & 0xff) == 20);

		CLinkedEvent *n3 = n2->GetNext();
		int tag3 = *(int *)n3;
		check("event[3] tag byte2 == CC 99 (NRPN addr MSB, ccBase+1)", ((tag3 >> 16) & 0xff) == 99);
		check("event[3] tag byte3 == addr MSB 10", ((tag3 >> 24) & 0xff) == 10);
		check("event[3] is the last node", n3->GetNext() == 0);

		/* Second call with the SAME address: address CCs should be elided. */
		CLinkedEvent *list2 = 0;
		int count2 = t6.AppendSingleParam(list2, lastAddr, param);
		check("AppendSingleParam elides unchanged-address CCs (only 2 data messages)", count2 == 2);

		/* data.b0 == 0xff sentinel: that CC is skipped. */
		CLinkedEvent *list3 = 0;
		SBytePair fresh = kInvalidBytePair;
		CParamTracer::SParam param2;
		param2.mAddr.b0 = 1; param2.mAddr.b1 = 1;
		param2.mData.b0 = 0xff; param2.mData.b1 = 50;
		int count3 = t6.AppendSingleParam(list3, fresh, param2);
		check("AppendSingleParam skips the 0xff 'not set' data byte (3 of 4 messages)", count3 == 3);
	}

	if (g_fail == 0) {
		printf("PASSED (0 checks failed)\n");
		return 0;
	}
	printf("FAILED (%d checks failed)\n", g_fail);
	return 1;
}
