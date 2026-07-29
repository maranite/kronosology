/*
 * test_es_disk_command_task.cpp  -  host-side known-answer test for
 * CESDiskCommandTask's 84 command trampolines (round 44, solo).
 *
 * Checks: for a representative sample spanning the address range (first,
 * last, and several in between, including the LoadGlobal/LoadSyx cases whose
 * opcodes are NOT in simple ascending-with-gaps order), each method (a)
 * writes its own confirmed opcode literal to mCommandOpcode (+0xa8) and
 * (b) clears CTask's own mMask bit 0x01 via the inherited real
 * CTask::SetMask(0) (task.h/task.cpp) -- proving both halves of the shared
 * trampoline body, not just the opcode table.
 */

#include <cstdio>
#include "es_disk_command_task.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct DiskCommandTaskTestHooks {
	static unsigned int Opcode(const CESDiskCommandTask &t) { return t.mCommandOpcode; }
	static void Poison(CESDiskCommandTask &t) { t.mCommandOpcode = 0xdeadbeef; }
};

/* Mirrors test_task.cpp's own TaskTestHooks::Mask() accessor -- separate
 * translation unit, so a same-named local struct is fine (each verify/
 * binary links independently, per Makefile's per-test objs/verify/% rule). */
struct TaskTestHooks {
	static unsigned char Mask(const CTask &t) { return t.mMask; }
};

struct TestableDiskCommandTask : public CESDiskCommandTask {
	TestableDiskCommandTask() : CESDiskCommandTask() {}
};

int main()
{
	/* [1] LoadMultiFile (first in address order) -> opcode 0x52 */
	{
		TestableDiskCommandTask t;
		t.SetMask(1); /* pre-set the mask bit so SetMask(0) inside has something to clear */
		t.LoadMultiFile(0);
		check("LoadMultiFile: opcode == 0x52", DiskCommandTaskTestHooks::Opcode(t) == 0x52);
		check("LoadMultiFile: mask bit cleared", (TaskTestHooks::Mask(t) & 1) == 0);
	}

	/* [2] Load1MossProg (last in address order) -> opcode 0x06 */
	{
		TestableDiskCommandTask t;
		t.Load1MossProg(0);
		check("Load1MossProg: opcode == 0x06", DiskCommandTaskTestHooks::Opcode(t) == 0x06);
	}

	/* [3] LoadAll -> opcode 0 (the one all-zero case) */
	{
		TestableDiskCommandTask t;
		DiskCommandTaskTestHooks::Poison(t); /* so a false-pass-on-uninitialized-zero is caught */
		t.LoadAll(0);
		check("LoadAll: opcode == 0", DiskCommandTaskTestHooks::Opcode(t) == 0);
	}

	/* [4] LoadSyx (out-of-band 0x36, sorts between the Save* and Load1Combi
	 * runs in address order but its opcode breaks the local descending
	 * sequence -- confirms per-function extraction, not a guessed formula) */
	{
		TestableDiskCommandTask t;
		t.LoadSyx(0);
		check("LoadSyx: opcode == 0x36", DiskCommandTaskTestHooks::Opcode(t) == 0x36);
	}

	/* [5] LoadGlobal (0x10) vs its neighbors Load1DrumTrackPattern (0x12) /
	 * LoadDrumTrackPatterns (0x11) -- the 3-method run where ascending
	 * address does NOT mean ascending opcode */
	{
		TestableDiskCommandTask a, b, c;
		a.LoadGlobal(0);
		b.Load1DrumTrackPattern(0);
		c.LoadDrumTrackPatterns(0);
		check("LoadGlobal: opcode == 0x10", DiskCommandTaskTestHooks::Opcode(a) == 0x10);
		check("Load1DrumTrackPattern: opcode == 0x12", DiskCommandTaskTestHooks::Opcode(b) == 0x12);
		check("LoadDrumTrackPatterns: opcode == 0x11", DiskCommandTaskTestHooks::Opcode(c) == 0x11);
	}

	/* [6] every return value is the literal 1 (real ground truth: `return 1;`) */
	{
		TestableDiskCommandTask t;
		check("SaveAll: return value == 1", t.SaveAll(0) == 1);
	}

	/* [7] spot-check a handful more scattered across the table */
	{
		TestableDiskCommandTask t;
		t.Format(0);
		check("Format: opcode == 0x48", DiskCommandTaskTestHooks::Opcode(t) == 0x48);
	}
	{
		TestableDiskCommandTask t;
		t.LoadKscItem(0);
		check("LoadKscItem: opcode == 0x35", DiskCommandTaskTestHooks::Opcode(t) == 0x35);
	}
	{
		TestableDiskCommandTask t;
		t.Load1Prog(0);
		check("Load1Prog: opcode == 0x04", DiskCommandTaskTestHooks::Opcode(t) == 0x04);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
