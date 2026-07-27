/*
 * test_kg_msg_processor.cpp  -  host-side known-answer test for CKGMsgProcessor
 * (kg_msg_processor.h, Eva deferred-registry re-check batch, 2026-07-27).
 *
 * CKGMsgProcessor's own real methods (SetGEMax/Process/CheckAndSet-family/etc) stay out
 * of scope -- these are pure structural/host KAT checks of the ctor/dtor/
 * GetInstance() singleton accessor only.
 *
 * Checks:
 *   [1] GetInstance(): lazy singleton, same pointer on repeated calls
 *   [2] ctor: all 7 handler sub-object pointers non-null, each vtable-poked with
 *       its own distinct, correct real identity
 *   [3] ctor: mUnknown1c == 0xe, mUnknown2c == 1, mFlag28 == 0
 *   [4] ctor: mBuffer50/mBuffer10 non-null and fully zeroed
 *   [5] dtor: all 7 handler sub-objects get their own deleting-destructor
 *       (vtable slot 1) dispatched exactly once, in offset order
 */

#include <cstdio>
#include <cstring>

#include "kg_msg_processor.h"
#include "omega_vtables.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(const char *name, bool cond)
{
	++g_checks;
	if (!cond) {
		++g_failed;
		std::printf("FAIL: %s\n", name);
	}
}

} // namespace

struct KGMsgProcessorTestHooks {
	static void *Member(const CKGMsgProcessor &p, int off)
	{
		return *reinterpret_cast<void *const *>(
		    reinterpret_cast<const char *>(&p) + off);
	}
	static int Int(const CKGMsgProcessor &p, int off)
	{
		return *reinterpret_cast<const int *>(
		    reinterpret_cast<const char *>(&p) + off);
	}
	static unsigned char Byte(const CKGMsgProcessor &p, int off)
	{
		return *reinterpret_cast<const unsigned char *>(
		    reinterpret_cast<const char *>(&p) + off);
	}
};

/* Records how many times each of the 7 real per-class vtables' own slot-1
 * (deleting-dtor) entry gets dispatched, by temporarily swapping in a counting
 * stub instead of EvaVTableStub for slot 1 of each array. Restored afterward so
 * this test doesn't leave global state behind for any other verify binary run in
 * the same process (not applicable here since each verify binary is its own
 * process, but matches this project's own "leave globals as found" discipline).
 */
namespace {

int g_deleteCounts[7];
int g_deleteOrder[7];
int g_deleteOrderCount;

template <int N>
void CountingDelete(void *self)
{
	if (g_deleteOrderCount < 7)
		g_deleteOrder[g_deleteOrderCount++] = N;
	++g_deleteCounts[N];
	(void)self;
}

} // namespace

int main()
{
	CKGMsgProcessor *p1 = CKGMsgProcessor::GetInstance();
	CKGMsgProcessor *p2 = CKGMsgProcessor::GetInstance();
	check("GetInstance(): non-null", p1 != 0);
	check("GetInstance(): lazy singleton, same pointer on repeat calls", p1 == p2);

	const CKGMsgProcessor &p = *p1;

	void *h0 = KGMsgProcessorTestHooks::Member(p, 0x00);
	void *h1 = KGMsgProcessorTestHooks::Member(p, 0x04);
	void *h2 = KGMsgProcessorTestHooks::Member(p, 0x08);
	void *h3 = KGMsgProcessorTestHooks::Member(p, 0x0c);
	void *h4 = KGMsgProcessorTestHooks::Member(p, 0x10);
	void *h5 = KGMsgProcessorTestHooks::Member(p, 0x14);
	void *h6 = KGMsgProcessorTestHooks::Member(p, 0x18);

	check("ctor: mCommonHandler non-null", h0 != 0);
	check("ctor: mModuleHandler non-null", h1 != 0);
	check("ctor: mUIControlHandler non-null", h2 != 0);
	check("ctor: mSPRUIControlHandler non-null", h3 != 0);
	check("ctor: mSPRUICommonParamHandler non-null", h4 != 0);
	check("ctor: mSPRUIAudioTrackParamHandler non-null", h5 != 0);
	check("ctor: mSPRUIDrumTrackParamHandler non-null", h6 != 0);

	check("ctor: mCommonHandler vtable == PTR__CKGCommonMsgHandler_08f752e8",
	      *(void **)h0 == (void *)PTR__CKGCommonMsgHandler_08f752e8);
	check("ctor: mModuleHandler vtable == PTR__CKGModuleMsgHandler_08f75288",
	      *(void **)h1 == (void *)PTR__CKGModuleMsgHandler_08f75288);
	check("ctor: mUIControlHandler vtable == PTR__CKGUIControlMsgHandler_08f751a8",
	      *(void **)h2 == (void *)PTR__CKGUIControlMsgHandler_08f751a8);
	check("ctor: mSPRUIControlHandler vtable == PTR__CSPRUIControlMsgHandler_08f75508",
	      *(void **)h3 == (void *)PTR__CSPRUIControlMsgHandler_08f75508);
	check("ctor: mSPRUICommonParamHandler vtable == PTR__CSPRUICommonParamMsgHandler_08f754c8",
	      *(void **)h4 == (void *)PTR__CSPRUICommonParamMsgHandler_08f754c8);
	check("ctor: mSPRUIAudioTrackParamHandler vtable == PTR__CSPRUIAudioTrackParamMsgHandler_08f75488",
	      *(void **)h5 == (void *)PTR__CSPRUIAudioTrackParamMsgHandler_08f75488);
	check("ctor: mSPRUIDrumTrackParamHandler vtable == PTR__CSPRUIDrumTrackTrackParamMsgHandler_08f75448",
	      *(void **)h6 == (void *)PTR__CSPRUIDrumTrackTrackParamMsgHandler_08f75448);

	check("ctor: mUnknown1c == 0xe", KGMsgProcessorTestHooks::Int(p, 0x1c) == 0xe);
	check("ctor: mUnknown2c == 1", KGMsgProcessorTestHooks::Int(p, 0x2c) == 1);
	check("ctor: mFlag28 == 0", KGMsgProcessorTestHooks::Byte(p, 0x28) == 0);

	void *buf50 = KGMsgProcessorTestHooks::Member(p, 0x20);
	void *buf10 = KGMsgProcessorTestHooks::Member(p, 0x24);
	check("ctor: mBuffer50 non-null", buf50 != 0);
	check("ctor: mBuffer10 non-null", buf10 != 0);
	if (buf50 != 0) {
		unsigned char zero[0x50];
		std::memset(zero, 0, sizeof(zero));
		check("ctor: mBuffer50 fully zeroed",
		      std::memcmp(buf50, zero, 0x50) == 0);
	}
	if (buf10 != 0) {
		unsigned char zero[0x10];
		std::memset(zero, 0, sizeof(zero));
		check("ctor: mBuffer10 fully zeroed",
		      std::memcmp(buf10, zero, 0x10) == 0);
	}

	/* [5] dtor: each of the 7 handlers' own deleting-dtor slot (index 1) gets
	 * dispatched exactly once, in +0x00..+0x18 offset order. Build a standalone
	 * CKGMsgProcessor (not the shared singleton) with its own private set of
	 * per-class vtables so this doesn't disturb the real PTR__ arrays' shared
	 * EvaVTableStub entries for any other check in this binary.
	 */
	{
		for (int i = 0; i < 7; ++i)
			g_deleteCounts[i] = 0;
		g_deleteOrderCount = 0;

		void *localVtbls[7][2];
		void *ownerRaw = ::operator new(sizeof(CKGMsgProcessor));
		CKGMsgProcessor *owner =
		    reinterpret_cast<CKGMsgProcessor *>(ownerRaw);

		void *members[7];
		int sizes[7] = { 0x18, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x18 };
		for (int i = 0; i < 7; ++i) {
			members[i] = ::operator new(sizes[i]);
			localVtbls[i][0] = 0; /* slot 0 unused by this test */
			*reinterpret_cast<void ***>(members[i]) = localVtbls[i];
		}
		localVtbls[0][1] = (void *)&CountingDelete<0>;
		localVtbls[1][1] = (void *)&CountingDelete<1>;
		localVtbls[2][1] = (void *)&CountingDelete<2>;
		localVtbls[3][1] = (void *)&CountingDelete<3>;
		localVtbls[4][1] = (void *)&CountingDelete<4>;
		localVtbls[5][1] = (void *)&CountingDelete<5>;
		localVtbls[6][1] = (void *)&CountingDelete<6>;

		/* Placement-construct via the real ctor is not usable here (it would
		 * overwrite our counting-stub member pointers with the real
		 * PTR__Xxx-backed mallocs) -- instead poke the 7 member offsets
		 * directly, matching this test's own hooks-based access convention.
		 */
		void **rawFields = reinterpret_cast<void **>(ownerRaw);
		for (int i = 0; i < 7; ++i)
			rawFields[i] = members[i];

		owner->~CKGMsgProcessor();

		for (int i = 0; i < 7; ++i) {
			char name[64];
			std::snprintf(name, sizeof(name),
			              "dtor: handler[%d] deleting-dtor dispatched exactly once", i);
			check(name, g_deleteCounts[i] == 1);
		}
		check("dtor: handlers deleted in +0x00..+0x18 offset order",
		      g_deleteOrderCount == 7 &&
		          g_deleteOrder[0] == 0 && g_deleteOrder[1] == 1 &&
		          g_deleteOrder[2] == 2 && g_deleteOrder[3] == 3 &&
		          g_deleteOrder[4] == 4 && g_deleteOrder[5] == 5 &&
		          g_deleteOrder[6] == 6);

		for (int i = 0; i < 7; ++i)
			::operator delete(members[i]);
		::operator delete(ownerRaw);
	}

	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed != 0 ? 1 : 0;
}
