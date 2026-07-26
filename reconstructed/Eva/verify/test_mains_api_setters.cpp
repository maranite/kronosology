/*
 * test_mains_api_setters.cpp  -  host-side known-answer test for 3 small XxxApiInstance
 * setters promoted from Tier-B empty-body link-stubs to real bodies in a 2026-07-26
 * breadth-sweep batch: CChkApiInstance::SetOwnerModule() and CRMApiInstance::SetResMan()
 * (both mains.cpp, MMainChunkMan()/MMainResMan()'s own "item-4" real call sites), and
 * CEditApiInstance::AssignScope() (config_manager.cpp, CConfigManager::
 * AssignEditServerIDs()'s own real-but-currently-unreachable loop body).
 *
 * All 3 classes are declared/defined LOCALLY inside their owning .cpp file (never
 * exposed via any header -- CChkApiInstance/CRMApiInstance/CEditApiInstance are all
 * 20-30+-method god-objects genuinely out of scope in full; only these 3 specific
 * methods were promoted). This file re-declares each class with the exact matching
 * signature so the linker resolves against the real definitions in objs/init/mains.o
 * and objs/init/config_manager.o (same "same-shape forward declaration in an
 * unrelated TU" mechanism ordinary header `extern` declarations use elsewhere in this
 * project) -- it does NOT reach the real `ChkApiInstance`/`RMApiInstance`/
 * `EditApiInstance` file-local static globals (those stay `static`, un-testable from
 * here); this test drives each method against its OWN freshly allocated buffer
 * instead, checking only the documented field offsets/behavior.
 *
 * Checks:
 *   [1] SetOwnerModule(): first call (owner slot NULL) -- real field written at
 *       self+0x4, no "already assigned" branch taken
 *   [2] SetOwnerModule(): second call on the SAME self (owner slot now non-NULL) --
 *       real "already assigned" soft-assert branch taken (via Api+0x94, does not
 *       abort), field still overwritten unconditionally afterward
 *   [3] SetOwnerModule(NULL module): real "module is NULL" soft-assert branch taken,
 *       field still written (to NULL) -- neither assert is fatal, ground truth never
 *       early-returns after either fires
 *   [4] SetResMan(): real field written at self+0xc, single assert-only-on-NULL shape
 *       (no "already assigned" re-check, unlike SetOwnerModule -- a real, confirmed
 *       difference between the two, see mains.cpp's own comment)
 *   [5] SetResMan(NULL): soft-assert taken, field still written to NULL
 *   [6] AssignScope(): real indexed store at this+8+scope*4, returns 1 always;
 *       3 independent scope slots (0/1/2) don't clobber each other
 *
 * Every check exercises Api's real assert-report path (Api+0x94, EvaVTableStub-backed
 * -- see test_tempo.cpp's own header comment for why this is always safe to call: the
 * real mains.cpp global ctor chain has already wired up a dereferenceable Api/vtable
 * by the time any verify `main()` runs, since `make verify` links all of $(OBJ) into
 * every test binary).
 */

#include <cstdio>
#include <cstring>

#include "system_api.h"

extern CSystemApi *Api; /* mains.cpp -- see test_tempo.cpp's own comment */

/* Re-declarations matching mains.cpp's CChkApiInstance/CRMApiInstance and
 * config_manager.cpp's CEditApiInstance exactly -- see this file's own header
 * comment for why this links against the real definitions without a shared header.
 */
class CChkApiInstance {
public:
	static void SetOwnerModule(void *self, void *module);
};

class CRMApiInstance {
public:
	static void SetResMan(void *self, void *resMan);
};

class CEditApiInstance {
public:
	int AssignScope(const char *name, unsigned char scope);
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("mains.cpp/config_manager.cpp XxxApiInstance setter known-answer test\n");
	printf("======================================================================\n");

	/* [1]-[3] CChkApiInstance::SetOwnerModule() -- real field at self+0x4. */
	{
		unsigned char self[8];
		memset(self, 0, sizeof(self));
		int fakeModuleA, fakeModuleB;

		CChkApiInstance::SetOwnerModule(self, &fakeModuleA);
		check("[1] SetOwnerModule(): self+0x4 == module (first call, no prior owner)",
		      *(void **)(self + 4) == (void *)&fakeModuleA);

		/* [2] Same self, owner slot now non-NULL -- real "already assigned" soft-assert
		 * branch (does not abort; field still overwritten unconditionally after).
		 */
		CChkApiInstance::SetOwnerModule(self, &fakeModuleB);
		check("[2] SetOwnerModule(): re-call with owner already set still overwrites",
		      *(void **)(self + 4) == (void *)&fakeModuleB);

		/* [3] module == NULL -- real "module is NULL" soft-assert branch, field still
		 * written to NULL afterward (neither assert early-returns in ground truth).
		 */
		CChkApiInstance::SetOwnerModule(self, 0);
		check("[3] SetOwnerModule(self, NULL): field still written to NULL",
		      *(void **)(self + 4) == 0);
	}

	/* [4]-[5] CRMApiInstance::SetResMan() -- real field at self+0xc, single
	 * assert-only-on-NULL shape (no re-check unlike SetOwnerModule above).
	 */
	{
		unsigned char self[0x10];
		memset(self, 0, sizeof(self));
		int fakeResMan;

		CRMApiInstance::SetResMan(self, &fakeResMan);
		check("[4] SetResMan(): self+0xc == resMan",
		      *(void **)(self + 0xc) == (void *)&fakeResMan);

		CRMApiInstance::SetResMan(self, 0);
		check("[5] SetResMan(self, NULL): field written to NULL, no crash",
		      *(void **)(self + 0xc) == 0);
	}

	/* [6] CEditApiInstance::AssignScope() -- real indexed store at this+8+scope*4,
	 * always returns 1. 3 independent slots checked for no cross-clobbering.
	 */
	{
		unsigned char buf[8 + 4 * 3];
		memset(buf, 0, sizeof(buf));
		CEditApiInstance *self = (CEditApiInstance *)buf;

		int rc0 = self->AssignScope("Prog", 0);
		int rc1 = self->AssignScope("Combi", 1);
		int rc2 = self->AssignScope("Global", 2);

		check("[6] AssignScope() always returns 1", rc0 == 1 && rc1 == 1 && rc2 == 1);
		char **table = (char **)(buf + 8);
		check("[6] AssignScope(): slot 0 == \"Prog\"", strcmp(table[0], "Prog") == 0);
		check("[6] AssignScope(): slot 1 == \"Combi\"", strcmp(table[1], "Combi") == 0);
		check("[6] AssignScope(): slot 2 == \"Global\"", strcmp(table[2], "Global") == 0);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
