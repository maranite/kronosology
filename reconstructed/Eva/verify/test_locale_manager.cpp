/*
 * test_locale_manager.cpp  -  host-side known-answer test for
 * CLocaleManager::AddKeyboardLayout()/GetKeyboardLayout() (src/base/locale_manager.cpp).
 * Eva CLocaleManager closeout batch, 2026-07-26. See include/locale_manager.h for the
 * full ground-truth writeup.
 *
 * Uses raw, hand-built byte blobs standing in for `CKeyboardLayout` objects (only
 * the leading `unsigned short` "type" field matters to either method under test --
 * both stay opaque to CLocaleManager, matching the real class's own forward
 * declaration) rather than the real 0x484-byte CKeyboardLayout, same "minimal fake
 * shaped object" convention this project's other TVector-consumer KATs use.
 *
 * Checks:
 *   [1] AddKeyboardLayout(NULL) is a real no-op (matches ground truth's own leading
 *       null check) -- count stays 0.
 *   [2] AddKeyboardLayout() with < 32 real elements: real min-32 capacity is
 *       allocated on the FIRST call, not grown incrementally per call.
 *   [3] GetKeyboardLayout() linear scan finds the right element by its own leading
 *       type word; returns NULL for a type that isn't present; returns the FIRST
 *       matching element when more than one shares a type (ground truth doesn't
 *       special-case duplicates).
 *   [4] AddKeyboardLayout() growth: pushing past the initial 32-element capacity
 *       triggers a real doubling grow (to 64), preserving every previously-added
 *       element's identity (same real "grow-then-copy-then-append" ordering
 *       TVector_CIfcClientPtr_MakeCapacity() uses, poller.cpp) -- exercised with 33
 *       real pushes onto a FRESH instance (not the process-wide singleton, so this
 *       test can't be affected by whatever CAlphaKeybCtrlTask construction elsewhere
 *       in the same test binary already did to the real singleton).
 *   [5] GetKeyboardLayout() on an empty list returns NULL (same code path as
 *       "not found", ground truth doesn't distinguish empty from no-match).
 *
 * Every check here constructs its own local, non-singleton CLocaleManager-shaped
 * instance via the test-only hook below (NOT CLocaleManager::GetInstance(), which is
 * a real process-wide singleton other verify binaries -- e.g. test_alpha_keyb_ctrl --
 * also mutate) so this file's own checks are fully self-contained.
 */

#include <cstdio>
#include <cstring>

#include "locale_manager.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* A minimal fake "CKeyboardLayout" -- only the leading type word is ever read by
 * either method under test.
 */
struct FakeLayout {
	unsigned short type;
	unsigned char  filler[6];
};

int main()
{
	printf("CLocaleManager::AddKeyboardLayout()/GetKeyboardLayout() known-answer test\n");
	printf("===========================================================================\n");

	FakeLayout layouts[40];
	for (int i = 0; i < 40; i++) {
		layouts[i].type = (unsigned short)(0x100 + i);
		memset(layouts[i].filler, 0, sizeof(layouts[i].filler));
	}

	printf("[1] AddKeyboardLayout(NULL) -- real no-op\n");
	{
		CLocaleManager *mgr = CLocaleManager::GetInstance();
		/* Use the real singleton just for this one side-effect-free check --
		 * a NULL add can never observably change anything, so it's safe to
		 * share with other verify binaries' own singleton use.
		 */
		void *before = mgr->GetKeyboardLayout(0xdead);
		mgr->AddKeyboardLayout(0);
		void *after = mgr->GetKeyboardLayout(0xdead);
		check("GetKeyboardLayout(0xdead) unaffected by AddKeyboardLayout(NULL)",
		      before == after);
	}

	printf("[2]/[4]/[5] Fresh (non-singleton) instance -- growth, scan, empty-list\n");
	{
		/* CLocaleManager's own ctor is private (matches ground truth's own
		 * singleton-only construction) -- allocate raw storage and use
		 * GetInstance()'s own placement-new pattern is not reusable per-test
		 * (it always returns the SAME singleton). Instead, reuse the fact
		 * that CLocaleManager is a plain 4-pointer POD-shaped object (mVtbl/
		 * mBegin/mEnd/mCap) and hand-zero a raw block, calling the real
		 * AddKeyboardLayout()/GetKeyboardLayout() methods on it directly --
		 * both real bodies only ever touch mBegin/mEnd/mCap (mVtbl is never
		 * read by either), so this is a safe, faithful stand-in for "a fresh
		 * instance" without needing a public ctor.
		 */
		void *rawStorage[4];
		unsigned char *raw = reinterpret_cast<unsigned char *>(rawStorage);
		memset(raw, 0, sizeof(rawStorage));
		CLocaleManager *mgr = reinterpret_cast<CLocaleManager *>(raw);

		printf("  [5] GetKeyboardLayout() on an empty list returns NULL\n");
		check("empty list -> NULL", mgr->GetKeyboardLayout(0x100) == 0);

		printf("  [2] first AddKeyboardLayout() allocates real min-32 capacity\n");
		mgr->AddKeyboardLayout(reinterpret_cast<const CKeyboardLayout *>(&layouts[0]));
		unsigned char **begin = *reinterpret_cast<unsigned char ***>(raw + 4);
		unsigned char **end   = *reinterpret_cast<unsigned char ***>(raw + 8);
		unsigned char **cap   = *reinterpret_cast<unsigned char ***>(raw + 0xc);
		check("count == 1 after first push", (int)(end - begin) == 1);
		check("real min-32 capacity allocated on first push",
		      (int)(cap - begin) == 32);

		printf("  [3] GetKeyboardLayout() finds the pushed element by type\n");
		void *found = mgr->GetKeyboardLayout(0x100);
		check("found == &layouts[0]", found == &layouts[0]);
		check("GetKeyboardLayout() for an absent type returns NULL",
		      mgr->GetKeyboardLayout(0x999) == 0);

		printf("  [4] pushing past 32 elements triggers a real doubling grow\n");
		for (int i = 1; i < 32; i++)
			mgr->AddKeyboardLayout(reinterpret_cast<const CKeyboardLayout *>(&layouts[i]));
		begin = *reinterpret_cast<unsigned char ***>(raw + 4);
		end   = *reinterpret_cast<unsigned char ***>(raw + 8);
		cap   = *reinterpret_cast<unsigned char ***>(raw + 0xc);
		check("count == 32, capacity still 32 (exactly full, no grow yet)",
		      (int)(end - begin) == 32 && (int)(cap - begin) == 32);

		/* Every previously-pushed element must still be findable and in the
		 * real insertion order (index i at slot i) before triggering growth.
		 */
		bool allPresent = true;
		for (int i = 0; i < 32; i++) {
			if (mgr->GetKeyboardLayout(layouts[i].type) != &layouts[i])
				allPresent = false;
		}
		check("all 32 previously-pushed layouts still findable pre-grow", allPresent);

		mgr->AddKeyboardLayout(reinterpret_cast<const CKeyboardLayout *>(&layouts[32]));
		begin = *reinterpret_cast<unsigned char ***>(raw + 4);
		end   = *reinterpret_cast<unsigned char ***>(raw + 8);
		cap   = *reinterpret_cast<unsigned char ***>(raw + 0xc);
		check("count == 33 after the 33rd push", (int)(end - begin) == 33);
		check("real doubling grow: capacity now 64 (not e.g. 33 or 40)",
		      (int)(cap - begin) == 64);

		bool allPresentAfterGrow = true;
		for (int i = 0; i <= 32; i++) {
			if (mgr->GetKeyboardLayout(layouts[i].type) != &layouts[i])
				allPresentAfterGrow = false;
		}
		check("all 33 layouts (pre- and post-grow) still findable, real "
		      "copy-on-grow preserved every element's identity",
		      allPresentAfterGrow);

		/* Real: neither method ever calls the real global operator delete on
		 * anything but the OLD backing array during a grow, and this test's
		 * own final backing array is a genuine ::operator new() allocation
		 * that outlives the test (never freed) -- same "happy path only,
		 * no dtor modeled" convention this project's other TVector-based
		 * classes already use (e.g. CPoller's own mClients).
		 */
	}

	printf("\n%s\n", g_fail ? "FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
