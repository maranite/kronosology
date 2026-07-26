/*
 * test_dump_manager.cpp  -  host-side known-answer test for the DumpManager cluster
 * (Stage 6 breadth sweep, 2026-07-25): CCircByteBuffer, CDumpBuffer, CDumpManMod,
 * CDumpTask, CBufferingTask.
 *
 * Checks:
 *   [1] CCircByteBuffer: round-trip write/read, wraparound across the ring boundary,
 *       overflow rejection, Reset()
 *   [2] CDumpBuffer: ctor/Reset zero both own fields (mExpectedLength observable via
 *       the real CBufferingTask::GetDumpLength() path in [4])
 *   [3] CDumpManMod::Setup(): constructs a real CDumpTask + CBufferingTask, adds BOTH
 *       to the owning CModule's mTasks (count 0 -> 2, matching CModule::Add()'s own
 *       already-verified behavior in test_task.cpp)
 *   [4] The cross-link CDumpManMod::Setup() wires between the two sibling tasks:
 *       dumpTask->BufferingTask() == bufferingTask AND bufferingTask's own mDumpTask
 *       == dumpTask, in both directions
 *   [5] CDumpTask's ctor genuinely took the CSysExMsgTaskBase ECanTransmit==1 branch
 *       (mOutLink non-null, via SysExMsgTaskBaseTestHooks) -- the first time this
 *       reconstruction's own wired call graph exercises that branch end to end,
 *       confirming task.h's/sysex_msg_task_base.h's own "ground-truth reachable but
 *       dead in this reconstruction" verdict is now obsolete for this call path
 *   [6] CDumpBuffer::Read()/Write() length-tracking algorithm (promoted 2026-07-26,
 *       re-check of the DumpManager cluster batch): default (mLimitActive==0,
 *       mRemainingLength==0) Read() zero-fills and reports success on an empty
 *       "no dump announced" buffer; a tracked Write() (mLimitActive==1) clamps to
 *       mRemainingLength and clears mExpectedLength once exhausted; a tracked
 *       Read() (mLimitActive==0) drains mRemainingLength and zero-fills the tail
 *       when the caller asks for more than remains
 *   [7] CDumpMachine::ReadPacket()/WritePacket()/IsDumpEnded() -- real forwards
 *       into the owning CDumpTask's BufferingTask()'s embedded CDumpBuffer,
 *       exercised end to end through the real CDumpManMod::Setup()-built pair
 */

#include <cstdio>
#include <cstring>
#include "module.h"
#include "task.h"
#include "sysex_msg_task_base.h"
#include "dump_man_mod.h"
#include "dump_task.h"
#include "dump_man_state_machine.h"
#include "buffering_task.h"
#include "dump_buffer.h"
#include "circ_byte_buffer.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct ModuleTestHooks {
	static int TaskCount(const CModule &m)
	{
		return *(const int *)((const unsigned char *)&m + 0x14);
	}
	static void *TaskAt(const CModule &m, int i)
	{
		void **arr = *(void ***)((const unsigned char *)&m + 0x1c);
		return arr[i];
	}
};

struct DumpTaskTestHooks {
	static CDumpMachine *Machine(const CDumpTask &t) { return t.mMachine; }
	static CBufferingTask *Buffering(const CDumpTask &t) { return t.mBufferingTask; }
};

struct BufferingTaskTestHooks {
	static CDumpTask *DumpTask(const CBufferingTask &t) { return t.mDumpTask; }
};

struct SysExMsgTaskBaseTestHooks {
	static bool HasOutLink(const CSysExMsgTaskBase &t)
	{
		return *(void *const *)((const unsigned char *)&t + 0x88) != 0;
	}
};

struct DumpBufferTestHooks {
	static void SetRemainingLength(CDumpBuffer &b, unsigned int v)
	{
		*reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(&b) + 0x18) = v;
	}
	static void SetExpectedLength(CDumpBuffer &b, unsigned int v)
	{
		*reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(&b) + 0x1c) = v;
	}
};

/* CDumpBuffer is only ever constructed embedded inside a real CBufferingTask (never
 * standalone -- dump_buffer.h's own header comment); mLimitActive itself lives at
 * CBufferingTask's own +0xa0, one dword past the embedded CDumpBuffer's own end.
 * Poking it via the REAL owning CBufferingTask (rather than a standalone CDumpBuffer,
 * which would make this+0x20 read/write out of that smaller object's own bounds) is
 * the only safe way to exercise the length-tracking branch on a host KAT.
 */
struct BufferingTaskLimitTestHooks {
	static CDumpBuffer &Buffer(CBufferingTask &t)
	{
		return *reinterpret_cast<CDumpBuffer *>(reinterpret_cast<char *>(&t) + 0x80);
	}
	static void SetLimitActive(CBufferingTask &t, unsigned int v)
	{
		*reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(&t) + 0xa0) = v;
	}
};

/* --- fake Api global ------------------------------------------------------ */

extern CSystemApi *Api; /* real global, mains.cpp, linked into every verify binary */

extern "C" void FakeApiNoOp() {}

static int g_scopeIdCalls;
extern "C" int FakeScopeIdFn(void *)
{
	g_scopeIdCalls++;
	return 7;
}

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main(void)
{
	printf("DumpManager cluster known-answer test\n");
	printf("======================================\n");

	setup_fake_api();

	printf("[1] CCircByteBuffer round-trip / wraparound / overflow\n");
	{
		CCircByteBuffer ring(8); /* already a power of two */

		unsigned char out[8];
		check("empty ring rejects a 1-byte read", ring.Read(out, 1) == false);

		unsigned char in1[5] = { 1, 2, 3, 4, 5 };
		check("write 5 into an empty 8-capacity ring succeeds", ring.Write(in1, 5) == true);

		unsigned char rd1[5] = {};
		check("read the same 5 bytes back", ring.Read(rd1, 5) == true);
		check("bytes match", memcmp(in1, rd1, 5) == 0);

		/* Write 6 more so the write cursor wraps the 8-byte boundary (ring is
		 * empty again at this point: count 0, capacity 8, so 6 fits with 2
		 * slots to spare).
		 */
		unsigned char in2[6] = { 10, 20, 30, 40, 50, 60 };
		check("write 6 more (wraps write cursor)", ring.Write(in2, 6) == true);
		check("3 more bytes overflow (6 held + 3 requested > 8 capacity)",
		      ring.Write(in1, 3) == false);

		unsigned char rd2[6] = {};
		check("read the wrapped 6 bytes back", ring.Read(rd2, 6) == true);
		check("wrapped bytes match", memcmp(in2, rd2, 6) == 0);

		check("ring empty again", ring.Read(out, 1) == false);

		ring.Write(in1, 3);
		ring.Reset();
		check("Reset() empties a non-empty ring", ring.Read(out, 1) == false);
	}

	printf("[2] CDumpManMod::Setup() constructs + registers the real sibling pair\n");
	CModule dumpManMod("DumpManager");
	CDumpManMod *asDumpManMod = reinterpret_cast<CDumpManMod *>(&dumpManMod);
	{
		check("mTasks starts empty", ModuleTestHooks::TaskCount(dumpManMod) == 0);

		asDumpManMod->Setup();

		check("mTasks count 0 -> 2", ModuleTestHooks::TaskCount(dumpManMod) == 2);

		void *t0 = ModuleTestHooks::TaskAt(dumpManMod, 0);
		void *t1 = ModuleTestHooks::TaskAt(dumpManMod, 1);
		CDumpTask *dumpTask = static_cast<CDumpTask *>(t0);
		CBufferingTask *bufferingTask = static_cast<CBufferingTask *>(t1);

		check("slot 0 is the CDumpTask (constructed first, per ground truth order)",
		      dumpTask != 0);
		check("slot 1 is the CBufferingTask (constructed second)", bufferingTask != 0);

		printf("[3] cross-links wired both directions\n");
		check("dumpTask->BufferingTask() == bufferingTask",
		      dumpTask->BufferingTask() == bufferingTask);
		check("bufferingTask's own mDumpTask == dumpTask",
		      BufferingTaskTestHooks::DumpTask(*bufferingTask) == dumpTask);

		printf("[4] CDumpTask's own state\n");
		check("mMachine non-null", DumpTaskTestHooks::Machine(*dumpTask) != 0);
		check("CSysExMsgTaskBase ECanTransmit==1 branch genuinely taken (mOutLink "
		      "non-null) -- first live exercise of this branch in this "
		      "reconstruction's own call graph",
		      SysExMsgTaskBaseTestHooks::HasOutLink(*dumpTask));

		printf("[5] CBufferingTask's own state\n");
		unsigned long dumpLen = 0xdeadbeef;
		check("GetDumpLength() returns false (no length announced yet) and writes 0",
		      bufferingTask->GetDumpLength(dumpLen) == false && dumpLen == 0);

		printf("[6] CDumpBuffer::Read()/Write() length-tracking algorithm "
		       "(promoted 2026-07-26)\n");
		{
			CDumpBuffer &buf = BufferingTaskLimitTestHooks::Buffer(*bufferingTask);

			/* Default state (mLimitActive==0, mRemainingLength==0, matching a
			 * freshly-constructed CBufferingTask -- no dump announced yet):
			 * Read() clamps to 0 available bytes and zero-fills the whole
			 * caller buffer, still reporting success.
			 */
			unsigned char rdefault[4] = { 0xaa, 0xaa, 0xaa, 0xaa };
			check("default-state Read() succeeds (0 bytes available, fully "
			      "zero-filled)",
			      buf.Read(rdefault, 4) == true);
			unsigned char zeros4[4] = { 0, 0, 0, 0 };
			check("default-state Read() zero-fills the entire request",
			      memcmp(rdefault, zeros4, 4) == 0);

			/* mLimitActive==1 (Write()'s own tracking-active polarity): a
			 * tracked Write() clamps to mRemainingLength and clears
			 * mExpectedLength once fully consumed.
			 */
			BufferingTaskLimitTestHooks::SetLimitActive(*bufferingTask, 1);
			DumpBufferTestHooks::SetRemainingLength(buf, 3);
			DumpBufferTestHooks::SetExpectedLength(buf, 3);

			unsigned char w5[5] = { 1, 2, 3, 4, 5 };
			check("tracked Write() of 5 into a 3-byte-remaining dump "
			      "succeeds (clamped)",
			      buf.Write(w5, 5) == true);
			check("tracked Write() clamp drains mRemainingLength to 0",
			      buf.RemainingLength() == 0);
			check("tracked Write() clears mExpectedLength once exhausted",
			      buf.ExpectedLength() == 0);

			unsigned char rback[3] = {};
			/* mLimitActive is still 1 here, so this Read() is the
			 * "no tracking" (pass-through) case for Read() -- only
			 * mLimitActive==0 engages Read()'s own tracking branch, the
			 * confirmed-opposite polarity documented in dump_buffer.h.
			 */
			check("plain (untracked-for-Read) readback gets the 3 bytes the "
			      "clamped Write() actually stored",
			      buf.Read(rback, 3) == true && memcmp(rback, w5, 3) == 0);

			/* mLimitActive==0: a tracked Read() that asks for more than
			 * mRemainingLength clamps to what's left and zero-fills the tail.
			 */
			BufferingTaskLimitTestHooks::SetLimitActive(*bufferingTask, 0);
			DumpBufferTestHooks::SetRemainingLength(buf, 2);
			DumpBufferTestHooks::SetExpectedLength(buf, 2);
			buf.Write(w5, 2); /* mLimitActive==0 here too -> untracked write,
			                    * just fills the ring for the Read() below. */

			unsigned char rpartial[4] = { 9, 9, 9, 9 };
			check("tracked Read() asking for 4 with only 2 remaining succeeds",
			      buf.Read(rpartial, 4) == true);
			unsigned char expectPartial[4] = { 1, 2, 0, 0 };
			check("tracked Read() delivers the 2 real bytes then zero-fills "
			      "the remaining 2",
			      memcmp(rpartial, expectPartial, 4) == 0);
			check("tracked Read() drains mRemainingLength to 0",
			      buf.RemainingLength() == 0);
			check("tracked Read() clears mExpectedLength once exhausted",
			      buf.ExpectedLength() == 0);

			/* Restore a clean default state before section [7] reuses this
			 * same real CBufferingTask.
			 */
			BufferingTaskLimitTestHooks::SetLimitActive(*bufferingTask, 0);
			DumpBufferTestHooks::SetRemainingLength(buf, 0);
			DumpBufferTestHooks::SetExpectedLength(buf, 0);
			buf.Reset();
		}

		printf("[7] CDumpMachine::ReadPacket()/WritePacket()/IsDumpEnded() -- "
		       "real forwards (promoted 2026-07-26)\n");
		{
			CDumpMachine *machine = DumpTaskTestHooks::Machine(*dumpTask);

			check("IsDumpEnded() true on a fresh (mRemainingLength==0) buffer",
			      machine->IsDumpEnded());

			/* mLimitActive==2 (any value other than 0 or 1) puts BOTH Write()
			 * (tracks only on ==1) and Read() (tracks only on ==0) on their
			 * plain pass-through paths at once -- an artificial value chosen
			 * only to isolate ReadPacket()/WritePacket()'s own +0x80 forward
			 * arithmetic from the length-tracking algorithm section [6]
			 * already covers; ground truth's real writer of this field
			 * (CBufferingTask::Put(), Tier B) is not known to ever use it.
			 */
			BufferingTaskLimitTestHooks::SetLimitActive(*bufferingTask, 2);

			unsigned char packet[4] = { 0x11, 0x22, 0x33, 0x44 };
			machine->WritePacket(packet, 4);

			unsigned char readback[4] = {};
			machine->ReadPacket(readback, 4);
			check("ReadPacket()/WritePacket() forward through "
			      "BufferingTask()+0x80's embedded CDumpBuffer correctly",
			      memcmp(readback, packet, 4) == 0);

			BufferingTaskLimitTestHooks::SetLimitActive(*bufferingTask, 0);
		}
	}

	printf("\n%s\n", g_fail ? "FAILED" : "ALL OK");
	return g_fail ? 1 : 0;
}
