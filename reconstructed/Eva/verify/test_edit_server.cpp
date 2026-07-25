/*
 * test_edit_server.cpp  -  host-side known-answer test for CDataHandler/
 * CEditServer (src/editor/edit_server.cpp, Stage 6 breadth sweep,
 * 2026-07-25 -- MMainESCommon/MMainESGlobal survey batch).
 *
 * Neither class is reachable from this reconstruction's own currently-wired
 * boot path yet (see edit_server.h's file header), so this KAT exercises
 * both directly: construct a real CEditServer (its own ctor is safe, uses
 * only Tier-B stubs), register synthetic SDescriptor tables via
 * EditServerTestHooks (same friend-accessor pattern as
 * test_module_adjust_task_mask.cpp), then check Get()/Set()'s real
 * flag-driven behavior against hand-derived expected values.
 *
 * CEditApiInstance's real GetAssignedScope() stub always returns 0xff (see
 * edit_server.cpp) -- every CEditServer this test constructs therefore has
 * mAssignedScope==0xff, so every Get()/Set() call below uses group=0xff for
 * the outer "assigned scope" argument (real ground-truth argument order:
 * group=scope, index=descriptor's own group field, subIndex=descriptor's
 * own baseIndex/array-position -- see edit_server.h).
 */

#include <cstdio>
#include <cstring>

#include "edit_server.h"

struct EditServerTestHooks {
	static CDataHandler &Data(CEditServer &s) { return s.mData; }
	static void SetVtbl(CEditServer &s, void *v) { s.mVtbl = v; }
	static void *GetVtbl(CEditServer &s) { return s.mVtbl; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* A fake "owner" object Get()/Set() read/write member fields on directly via
 * raw offsets, matching SDescriptor::offset's own real semantics.
 */
struct FakeOwner {
	int   intField;    /* +0 */
	short shortField;  /* +4 */
	unsigned char byteField; /* +6 */
	int   arr[4];      /* +8..+24, for kFlagIndexed tests */
	char  blob[8];     /* for block-copy/var-len tests */
};

static SDescriptor MakeDescriptor()
{
	SDescriptor d;
	memset(&d, 0, sizeof(d));
	d.notifyId = -1; /* no notify hook by default -- avoids the Api vtable
	                   * dispatch entirely for most cases below */
	return d;
}

/* --- Even/direct custom getter/setter callbacks -- the only encoding any
 * real descriptor table this project has seen elsewhere actually uses (see
 * edit_server.cpp's own comment on the odd/vtable-thunk branch). */
static int __attribute__((aligned(4))) FakeGetter(void * /*self*/, int /*subIndex*/, void *buf)
{
	*(int *)buf = 0x4242;
	return 1; /* "value obtained, use it" */
}
static int __attribute__((aligned(4))) FakeGetterDecline(void * /*self*/, int /*subIndex*/, void * /*buf*/)
{
	return 0; /* "decline -- fall back to raw memory get" */
}
static int __attribute__((aligned(4))) FakeSetter(void *self, int subIndex, const void *buf, int /*source*/)
{
	FakeOwner *owner = (FakeOwner *)self;
	owner->intField = *(const int *)buf + subIndex;
	return 1;
}
static int __attribute__((aligned(4))) FakeCommand(void *self, unsigned subIndex, int /*source*/)
{
	((FakeOwner *)self)->intField = 0x1000 + (int)subIndex;
	return 0; /* >=0 -> Set() returns 0 (success) for a command descriptor */
}

int main()
{
	printf("CDataHandler/CEditServer test\n");
	printf("==============================\n");

	CEditServer server("TestScope");
	CDataHandler &data = EditServerTestHooks::Data(server);
	FakeOwner owner;
	memset(&owner, 0, sizeof(owner));

	/* --- plain scalar field, offset-based, unsigned 4-byte -------------- */
	SDescriptor dPlain = MakeDescriptor();
	dPlain.offset = 0; /* FakeOwner::intField */
	dPlain.elemSize = 4;
	dPlain.group = 1;
	dPlain.baseIndex = 0;
	dPlain.minValue = 0;
	dPlain.maxValue = 1000;

	SDescriptor descrPlain[1] = { dPlain };
	data.AddDescriptors((CObjectBase *)&owner, descrPlain, 1, false);

	int got = 0;
	int rc = server.Get(0xff, 1, 0, &got, 4);
	check("Get(): plain scalar field reads owner's initial 0", rc == 1 && got == 0);

	int newVal = 777;
	unsigned setRc = server.Set(0xff, 1, 0, &newVal, 4, 0);
	check("Set(): plain scalar field accepts an in-range value (returns 0)", setRc == 0);
	check("Set(): plain scalar field actually wrote through to the owner", owner.intField == 777);

	rc = server.Get(0xff, 1, 0, &got, 4);
	check("Get(): plain scalar field now reads back the value just Set()", rc == 1 && got == 777);

	/* --- range check -------------------------------------------------------
	 * Real Set()'s own return-code convention (confirmed from Set@080700b0.c):
	 * 0 means "no change-notification was posted" -- true for BOTH a
	 * rejected/out-of-range value AND a successful set with
	 * sm_bNotifyEnabled==false (this reconstruction's default). It is NOT a
	 * generic success/failure boolean. The only reliable way to observe a
	 * rejection is that the value did NOT get written through.
	 */
	int tooHigh = 5000;
	setRc = server.Set(0xff, 1, 0, &tooHigh, 4, 0);
	check("Set(): out-of-range value (> maxValue) returns 0 (same as a "
	      "successful-no-notify set -- real return code is not a boolean)", setRc == 0);
	check("Set(): out-of-range value did NOT get written through", owner.intField == 777);

	/* --- not-found: real Get() ALSO returns 1 here, not 0 --------------------
	 * Confirmed from Get@0806fb40.c: when CDataHandler::FindDescriptor() misses,
	 * the entire "found" block is skipped and execution falls straight to the
	 * loop-bottom `if (mLoopContinue != 0) { ...; return 1; }` tail --
	 * mLoopContinue is unconditionally set to 1 at the top of every iteration
	 * and nothing clears it on a miss, so this always returns 1. Get()'s
	 * return value is therefore NOT a "found" boolean either -- it returns 0
	 * only for specific validation failures encountered AFTER a hit (disabled/
	 * command flags, a width mismatch), not for "not found" itself. This was
	 * this batch's own initial (wrong) assumption, corrected by this KAT.
	 */
	rc = server.Get(0xff, 99, 0, &got, 4);
	check("Get(): unknown descriptor returns 1, NOT 0 (real, verified quirk -- "
	      "see comment above)", rc == 1);
	setRc = server.Set(0xff, 99, 0, &newVal, 4, 0);
	check("Set(): unknown descriptor returns 1 (real asymmetric not-found convention)", setRc == 1);

	/* --- wrong scope: CDataHandler::FindDescriptor's own outer gate --------
	 * Same "not found" path as above (CDataHandler::FindDescriptor's own
	 * mScope check fails first) -- so this ALSO returns 1, not 0.
	 */
	rc = server.Get(0x01, 1, 0, &got, 4);
	check("Get(): wrong scope (group != mAssignedScope) also returns 1 (same "
	      "not-found path as above)", rc == 1);

	/* --- disabled descriptor: both Get and Set fail ---------------------- */
	SDescriptor dDisabled = MakeDescriptor();
	dDisabled.offset = 4; /* shortField, unused here */
	dDisabled.elemSize = 2;
	dDisabled.group = 2;
	dDisabled.flags = kFlagDisabled;
	SDescriptor descrDisabled[1] = { dDisabled };
	data.AddDescriptors((CObjectBase *)&owner, descrDisabled, 1, false);

	rc = server.Get(0xff, 2, 0, &got, 2);
	check("Get(): kFlagDisabled descriptor always fails", rc == 0);
	short sval = 5;
	setRc = server.Set(0xff, 2, 0, &sval, 2, 0);
	check("Set(): kFlagDisabled descriptor always fails", setRc == 0);

	/* --- command descriptor: Set triggers callback, Get always fails ----- */
	SDescriptor dCmd = MakeDescriptor();
	dCmd.setterFn = (void *)&FakeCommand;
	dCmd.setterExtra = 0;
	dCmd.group = 3;
	dCmd.baseIndex = 7; /* must match the subIndex used below -- a non-indexed
	                     * descriptor is only found by the direct hash-table
	                     * slot (subIndex+8+group*256), which is keyed by its
	                     * own baseIndex at registration time; this test's own
	                     * bug (baseIndex left at 0 while calling with
	                     * subIndex=7) was caught by the very "not found"
	                     * behavior this batch had to characterize above. */
	dCmd.flags = kFlagCommand;
	SDescriptor descrCmd[1] = { dCmd };
	data.AddDescriptors((CObjectBase *)&owner, descrCmd, 1, false);

	rc = server.Get(0xff, 3, 7, &got, 4);
	check("Get(): kFlagCommand descriptor always fails (0xc0 mask)", rc == 0);
	setRc = server.Set(0xff, 3, 7, &newVal, 4, 0);
	/* Real formula `~r >> 0x1f`: r>=0 (callback "succeeded") -> 1; r<0
	 * ("failed") -> 0 -- confirmed by hand-evaluating both cases, matches
	 * ResolveDescriptorCallback... err, matches CEditServer::Set()'s own
	 * command-path return statement exactly. FakeCommand returns 0 (>=0). */
	check("Set(): kFlagCommand callback result >=0 maps to a Set() return of 1",
	      setRc == 1);
	check("Set(): kFlagCommand callback actually ran (owner.intField == 0x1000+7)",
	      owner.intField == 0x1000 + 7);

	/* --- indexed array field --------------------------------------------- */
	SDescriptor dIdx = MakeDescriptor();
	dIdx.offset = 8; /* FakeOwner::arr */
	dIdx.elemSize = 4;
	dIdx.group = 4;
	dIdx.baseIndex = 10;
	dIdx.count = 4; /* claims sub-indices 10..13 */
	dIdx.minValue = 0;
	dIdx.maxValue = 100000;
	dIdx.flags = kFlagIndexed;
	SDescriptor descrIdx[1] = { dIdx };
	data.AddDescriptors((CObjectBase *)&owner, descrIdx, 1, false);

	owner.arr[2] = 999; /* index 12 = baseIndex(10)+2 */
	rc = server.Get(0xff, 4, 12, &got, 4);
	check("Get(): indexed array field reads the right element (arr[2])", rc == 1 && got == 999);

	int idxVal = 55;
	setRc = server.Set(0xff, 4, 11, &idxVal, 4, 0);
	check("Set(): indexed array field writes the right element (arr[1])",
	      setRc == 0 && owner.arr[1] == 55);

	/* out-of-range sub-index (14, past baseIndex+count-1==13) should miss the
	 * indexed range test in FindDescriptor -> not found -> Get fails, Set
	 * "succeeds" (matches the not-found convention above). */
	rc = server.Get(0xff, 4, 14, &got, 4);
	check("Get(): sub-index past an indexed descriptor's own range is not found "
	      "(returns 1, same not-found quirk as above)", rc == 1);

	/* --- custom getter: even/direct encoding, both accept and decline ---- */
	SDescriptor dGet = MakeDescriptor();
	dGet.getterFn = (void *)&FakeGetter;
	dGet.getterExtra = 0;
	dGet.elemSize = 4;
	dGet.group = 5;
	dGet.flags = kFlagCustomGetter;
	SDescriptor descrGet[1] = { dGet };
	data.AddDescriptors((CObjectBase *)&owner, descrGet, 1, false);

	got = 0;
	rc = server.Get(0xff, 5, 0, &got, 4);
	check("Get(): kFlagCustomGetter dispatches the callback (accept path)",
	      rc == 1 && got == 0x4242);

	SDescriptor dGetDecline = MakeDescriptor();
	dGetDecline.getterFn = (void *)&FakeGetterDecline;
	dGetDecline.offset = 0; /* falls back to owner.intField's own current value */
	dGetDecline.elemSize = 4;
	dGetDecline.group = 6;
	dGetDecline.flags = kFlagCustomGetter;
	SDescriptor descrGetDecline[1] = { dGetDecline };
	data.AddDescriptors((CObjectBase *)&owner, descrGetDecline, 1, false);

	got = 0;
	int expectedFallback = owner.intField; /* whatever it currently is (mutated
	                                         * by earlier Set()-family tests
	                                         * above) -- the point of this
	                                         * check is that the decline path
	                                         * reads owner.intField directly,
	                                         * not any fixed value. */
	rc = server.Get(0xff, 6, 0, &got, 4);
	check("Get(): kFlagCustomGetter callback declining (0) falls back to the raw memory get",
	      rc == 1 && got == expectedFallback);

	/* --- custom setter: even/direct encoding ------------------------------ */
	SDescriptor dSet = MakeDescriptor();
	dSet.setterFn = (void *)&FakeSetter;
	dSet.setterExtra = 0;
	dSet.elemSize = 4;
	dSet.group = 7;
	dSet.baseIndex = 3; /* must match the subIndex used below -- see dCmd's
	                     * own comment above for why (non-indexed descriptors
	                     * are keyed by baseIndex in the direct hash table). */
	dSet.minValue = 0;
	dSet.maxValue = 1000000;
	dSet.flags = kFlagCustomSetter;
	SDescriptor descrSet[1] = { dSet };
	data.AddDescriptors((CObjectBase *)&owner, descrSet, 1, false);

	int setInput = 100;
	setRc = server.Set(0xff, 7, 3, &setInput, 4, 0);
	check("Set(): kFlagCustomSetter dispatches the callback (owner.intField==100+3)",
	      setRc == 0 && owner.intField == 103);

	/* --- SetDefault -------------------------------------------------------
	 * Needs a real (non-EvaVTableStub) vtable slot +0xc since SetDefault()
	 * dispatches through the SERVER's own vtable, not the descriptor's --
	 * install a tiny fake vtable for this one check.
	 */
	{
		typedef int (*SetDefaultFn)(CEditServer *, unsigned char, unsigned char, unsigned char,
		                             const void *, unsigned, int);
		static SetDefaultFn fakeVtbl[4] = { 0, 0, 0, 0 };
		struct SetDefaultTrap {
			static int Call(CEditServer *self, unsigned char group, unsigned char index,
			                unsigned char subIndex, const void *buf, unsigned len, int src)
			{
				return self->Set(group, index, subIndex, buf, len, src);
			}
		};
		fakeVtbl[3] = &SetDefaultTrap::Call; /* slot +0xc / 4 == index 3 */

		SDescriptor dDef = MakeDescriptor();
		dDef.offset = 0;
		dDef.elemSize = 4;
		dDef.group = 8;
		dDef.minValue = 0;
		dDef.maxValue = 1000000;
		dDef.defaultValue = 424242;
		dDef.flags = kFlagHasDefault;
		SDescriptor descrDef[1] = { dDef };
		data.AddDescriptors((CObjectBase *)&owner, descrDef, 1, false);

		void *savedVtbl = EditServerTestHooks::GetVtbl(server);
		EditServerTestHooks::SetVtbl(server, (void *)fakeVtbl);
		int defRc = server.SetDefault(0xff, 8, 0);
		EditServerTestHooks::SetVtbl(server, savedVtbl);

		check("SetDefault(): kFlagHasDefault descriptor restores its default value",
		      defRc == 0 && owner.intField == 424242);
	}

	/* --- SetDefault without kFlagHasDefault: no-op ------------------------ */
	SDescriptor dNoDef = MakeDescriptor();
	dNoDef.offset = 0;
	dNoDef.elemSize = 4;
	dNoDef.group = 9;
	SDescriptor descrNoDef[1] = { dNoDef };
	data.AddDescriptors((CObjectBase *)&owner, descrNoDef, 1, false);
	int before = owner.intField;
	server.SetDefault(0xff, 9, 0);
	check("SetDefault(): descriptor without kFlagHasDefault leaves the value untouched",
	      owner.intField == before);

	/* --- block-copy / var-len ---------------------------------------------- */
	SDescriptor dBlock = MakeDescriptor();
	dBlock.offset = (int)((char *)&owner.blob - (char *)&owner);
	dBlock.group = 10;
	dBlock.count = 8;
	dBlock.flags = kFlagBlockCopy;
	SDescriptor descrBlock[1] = { dBlock };
	data.AddDescriptors((CObjectBase *)&owner, descrBlock, 1, false);

	char srcBlob[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	setRc = server.Set(0xff, 10, 0, srcBlob, 8, 0);
	check("Set(): kFlagBlockCopy writes the full blob", setRc == 0 && memcmp(owner.blob, srcBlob, 8) == 0);

	char dstBlob[8];
	memset(dstBlob, 0, 8);
	rc = server.Get(0xff, 10, 0, dstBlob, 8);
	check("Get(): kFlagBlockCopy reads the full blob back", rc == 1 && memcmp(dstBlob, srcBlob, 8) == 0);

	char shortBlob[4] = { 9, 9, 9, 9 };
	setRc = server.Set(0xff, 10, 0, shortBlob, 4, 0);
	check("Set(): kFlagBlockCopy accepts bufLen < count (partial write)", setRc == 0);

	char blobBeforeReject[8];
	memcpy(blobBeforeReject, owner.blob, 8);
	char tooLong[16];
	memset(tooLong, 0, 16);
	setRc = server.Set(0xff, 10, 0, tooLong, 16, 0);
	/* Real Set() returns 0 for this too (same "return code isn't a success/
	 * fail boolean" quirk documented above) -- the only observable rejection
	 * signal is that the blob is left untouched. */
	check("Set(): kFlagBlockCopy rejects bufLen > count (blob left untouched)",
	      memcmp(owner.blob, blobBeforeReject, 8) == 0);

	SDescriptor dVar = MakeDescriptor();
	dVar.offset = (int)((char *)&owner.blob - (char *)&owner);
	dVar.group = 11;
	dVar.count = 8;
	dVar.flags = kFlagVarLen;
	SDescriptor descrVar[1] = { dVar };
	data.AddDescriptors((CObjectBase *)&owner, descrVar, 1, false);

	setRc = server.Set(0xff, 11, 0, srcBlob, 8, 0);
	check("Set(): kFlagVarLen accepts an exact-length buffer", setRc == 0);

	memset(owner.blob, 0xaa, 8);
	char wrongLen[4] = { 9, 9, 9, 9 };
	setRc = server.Set(0xff, 11, 0, wrongLen, 4, 0);
	/* Same "return code isn't boolean" quirk -- check the blob instead. */
	check("Set(): kFlagVarLen rejects a wrong-length buffer (blob left untouched)",
	      owner.blob[0] == (char)0xaa);

	/* --- CDataHandler::FindDescriptor de-dup (AddDescriptors re-registration)
	 * Real finding: the direct hash-table hit path returns owner via a
	 * SEPARATE global "last registered owner" field (CDataHandler-relative
	 * +0x40020, see AddDescriptors' own comment) that is written ONLY by the
	 * "new registration" path -- the alreadyRegistered=true de-dup path only
	 * updates the linear-list SRegistration's own `owner` field, which the
	 * FALLBACK SCAN reads but a direct hash hit never does. So re-registering
	 * an already-hash-indexed descriptor under a new owner does NOT change
	 * what a direct hash-table Get()/Set() observes -- it still returns
	 * whichever owner the most recent *new* (alreadyRegistered=false)
	 * AddDescriptors() call happened to pass, project-wide (this global
	 * field is shared across every registration on this CDataHandler
	 * instance, not per-descriptor). Confirmed by this KAT: `owner` was the
	 * last *new*-registration owner (dVar's, just above), so it stays
	 * observable even after 2 subsequent dedup re-registrations targeting
	 * different owners.
	 */
	data.AddDescriptors((CObjectBase *)&owner, descrPlain, 1, true /* alreadyRegistered */);
	rc = server.Get(0xff, 1, 0, &got, 4);
	check("AddDescriptors(): re-registering the same table (alreadyRegistered=true) "
	      "still finds it afterward", rc == 1);

	FakeOwner owner2;
	memset(&owner2, 0, sizeof(owner2));
	owner2.intField = 0xdead;
	data.AddDescriptors((CObjectBase *)&owner2, descrPlain, 1, true);
	int got2 = -1;
	rc = server.Get(0xff, 1, 0, &got2, 4);
	check("AddDescriptors(): alreadyRegistered=true does NOT retarget a direct hash "
	      "hit's observed owner (real quirk -- still reads owner, not owner2; see "
	      "comment above)", rc == 1 && got2 == owner.intField && got2 != (int)0xdead);

	/* --- PutNotify: sm_bNotifyEnabled defaults false in this reconstruction */
	int putRc = server.PutNotify(0xff, 1);
	check("PutNotify(): returns 0 while sm_bNotifyEnabled is false (this "
	      "reconstruction's default -- CNotifyList is Tier-B, see edit_server.cpp)",
	      putRc == 0);

	printf("\n%s (%d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail);
	return g_fail == 0 ? 0 : 1;
}
