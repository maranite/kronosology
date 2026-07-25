// SPDX-License-Identifier: GPL-2.0
/*
 * test_program_bank_init.cpp  -  host-side known-answer test for
 * CSTGProgramBank::Initialize()/GetPatchSize() -- batch 61, see
 * src/engine/program_bank_init.cpp.
 *
 * CSTGProgram::Initialize()/Copy() are given trivial call-tracking mocks
 * here (both remain deliberately deferred no-ops in the real project;
 * this file's own mocks exist only to verify CSTGProgramBank::
 * Initialize()'s own confirmed call sequence/argument values, not to
 * re-verify either callee's own internals).
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got != want) {
		printf("  FAILED: %s (got 0x%lx, want 0x%lx)\n", label, got, want);
		g_fail++;
	} else {
		printf("  ok: %s\n", label);
	}
}
static void check_true(const char *label, bool cond)
{
	if (!cond) {
		printf("  FAILED: %s\n", label);
		g_fail++;
	} else {
		printf("  ok: %s\n", label);
	}
}

static int g_progInitCalls;
static void *g_progInitThis;
static unsigned int g_progInitBankId, g_progInitFlags, g_progInitVoiceModel;

static int g_progCopyCalls;
static void *g_progCopyThis[0x80];
static CSTGProgram *g_progCopySrc[0x80];
static unsigned int g_progCopyBankId[0x80], g_progCopyFlags[0x80], g_progCopyVoiceModel[0x80];

void CSTGProgram::Initialize(unsigned int bankId, unsigned int flags, unsigned int voiceModel)
{
	g_progInitCalls++;
	g_progInitThis = this;
	g_progInitBankId = bankId;
	g_progInitFlags = flags;
	g_progInitVoiceModel = voiceModel;
}

void CSTGProgram::Copy(CSTGProgram *src, unsigned int bankId, unsigned int flags, unsigned int voiceModel)
{
	int i = g_progCopyCalls++;
	if (i < 0x80) {
		g_progCopyThis[i] = this;
		g_progCopySrc[i] = src;
		g_progCopyBankId[i] = bankId;
		g_progCopyFlags[i] = flags;
		g_progCopyVoiceModel[i] = voiceModel;
	}
}

/* A CSTGProgramBank followed by enough raw storage to hold 0x80 embedded
 * CSTGProgram-stride (0xcec byte) slots at +3, matching the real object's
 * own confirmed layout -- CSTGProgram itself is never separately sized
 * here (its ctor/full field layout live elsewhere), so this is a plain
 * byte buffer cast through CSTGProgram*, exactly like the real
 * ground-truth pointer arithmetic this class's own methods use. */
struct BankStorage {
	CSTGProgramBank bank;
	unsigned char pad[3 + 0x80 * 0xcec];
};

int main(void)
{
	printf("CSTGProgramBank::Initialize()/GetPatchSize() known-answer test (batch 61)\n");
	printf("===========================================================================\n");

	printf("\n[1] flag=true (bankType=0 -> voiceModelType=1, flag=true -> flags=0x610)\n");
	{
		static BankStorage storage;
		memset(&storage, 0xCD, sizeof(storage));
		g_progInitCalls = g_progCopyCalls = 0;

		storage.bank.Initialize(5, 0, true);

		check_eq("_bankId", storage.bank._bankId, 5);
		check_eq("_flag", storage.bank._flag, 1);
		check_eq("_bankType", storage.bank._bankType, 0);
		check_eq("GetPatchSize()", storage.bank.GetPatchSize(), 0x610);

		check_eq("CSTGProgram::Initialize() called once", g_progInitCalls, 1);
		check_true("Initialize() this == (char*)&bank + 3",
			   g_progInitThis == (void *)((char *)&storage.bank + 3));
		check_eq("Initialize() bankId arg", g_progInitBankId, 5);
		check_eq("Initialize() flags arg", g_progInitFlags, 0x610);
		check_eq("Initialize() voiceModelType arg", g_progInitVoiceModel, 1);

		check_eq("CSTGProgram::Copy() called 127 times", g_progCopyCalls, 0x7F);
		CSTGProgram *programs = (CSTGProgram *)((char *)&storage.bank + 3);
		check_true("Copy() call 0 this == programs[1]",
			   g_progCopyThis[0] == (void *)&programs[1]);
		check_true("Copy() call 0 src == programs[0]",
			   g_progCopySrc[0] == &programs[0]);
		check_eq("Copy() call 0 bankId arg", g_progCopyBankId[0], 5);
		check_eq("Copy() call 0 flags arg", g_progCopyFlags[0], 0x610);
		check_eq("Copy() call 0 voiceModelType arg", g_progCopyVoiceModel[0], 1);
		check_true("Copy() call 126 (last) this == programs[127]",
			   g_progCopyThis[0x7E] == (void *)&programs[0x7F]);
		check_eq("Copy() call 126 bankId arg (unchanged across loop)", g_progCopyBankId[0x7E], 5);
	}

	printf("\n[2] flag=false, bankType=3 (nonzero -> voiceModelType=2, flag=false -> flags=0xE14)\n");
	{
		static BankStorage storage;
		memset(&storage, 0xCD, sizeof(storage));
		g_progInitCalls = g_progCopyCalls = 0;

		storage.bank.Initialize(9, 3, false);

		check_eq("_bankId", storage.bank._bankId, 9);
		check_eq("_flag", storage.bank._flag, 0);
		check_eq("_bankType", storage.bank._bankType, 3);
		check_eq("GetPatchSize()", storage.bank.GetPatchSize(), 0xE14);
		check_eq("Initialize() flags arg", g_progInitFlags, 0xE14);
		check_eq("Initialize() voiceModelType arg", g_progInitVoiceModel, 2);
		check_eq("Copy() call 0 flags arg", g_progCopyFlags[0], 0xE14);
		check_eq("Copy() call 0 voiceModelType arg", g_progCopyVoiceModel[0], 2);
	}

	printf("\n[3] bankType=1 (boundary: nonzero -> voiceModelType=2)\n");
	{
		static BankStorage storage;
		memset(&storage, 0xCD, sizeof(storage));
		g_progInitCalls = g_progCopyCalls = 0;

		storage.bank.Initialize(1, 1, true);
		check_eq("Initialize() voiceModelType arg (bankType==1)", g_progInitVoiceModel, 2);
	}

	printf("\n===========================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
