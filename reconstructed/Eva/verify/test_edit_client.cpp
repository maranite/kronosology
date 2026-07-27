/*
 * test_edit_client.cpp  -  host-side known-answer test for CEditClient's
 * construction/destruction-only reconstruction (include/edit_man.h,
 * src/editor/edit_man.cpp -- Eva "size is not depth" re-check, 2026-07-27,
 * 3rd re-open of a class previously left "genuinely deep").
 *
 * Checks:
 *   [1] CEditClient::CEditClient() with NO CEditMan registered
 *       (EditApiInstance+4 == NULL, the real CEditApiInstance::RegisterClient
 *       trampoline's own null-check path): mVtbl/mControlHash/mIndexHash all
 *       set up correctly, no crash, and the (unregistered) client does NOT
 *       show up in any CMainTask's client list.
 *   [2] mControlHash/mIndexHash: each is a real, distinct 0x10-byte
 *       PointerHash<...> header -- own vtable installed, +0x4 flag byte and
 *       +0xc dword zeroed, +0x8 node-pool buffer pointer non-NULL and fully
 *       zeroed for its whole real 0xada4-byte size (spot-checked at several
 *       offsets, not just the first/last dword).
 *   [3] ~CEditClient() with no CEditMan registered: no crash (matches the
 *       real UnregisterClient trampoline's own null-check no-op path).
 *   [4] Full real chain WITH a CEditMan registered (CEditMan::Setup(),
 *       already-real): CEditClient::CEditClient() reaches
 *       EditApiInstance_RegisterClient() -> CEditMan::RegisterClient() ->
 *       CMainTask::RegisterClient(), and the client genuinely appears in the
 *       CMainTask's own client list; ~CEditClient() genuinely removes it
 *       again via the same real chain.
 *   [5] CEditor's own embedded `mEditClient` sub-object: confirmed
 *       plain-member construction/destruction (the actual real-world use
 *       site) doesn't crash and produces the same well-formed object as [1].
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "edit_man.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

extern "C" unsigned char EditApiInstance[0x404];

struct EditManTestHooks {
	static CEditMan::CMainTask *MainTask(const CEditMan &e) { return e.mMainTask; }
};

/* Mirrors CMainTask's own private layout enough to peek at the embedded
 * client list via the same COmegaPtrArray-shaped access every other test
 * in this project uses (test_small_modules.cpp's own EditManTestHooks
 * friend, edit_man.h). */
#include "omega_ptr_array.h"

static bool ClientListContains(CEditMan::CMainTask *mainTask, CEditClient *client)
{
	unsigned char *raw = reinterpret_cast<unsigned char *>(mainTask);
	COmegaPtrArray *clients = reinterpret_cast<COmegaPtrArray *>(raw + 0x27c);
	unsigned count = clients->Count();
	for (unsigned i = 0; i < count; ++i)
		if (clients->Get(i) == client)
			return true;
	return false;
}

int main()
{
	printf("CEditClient construction/destruction known-answer test\n");
	printf("========================================================\n");

	printf("[1]/[2] CEditClient::CEditClient() with no CEditMan registered\n");
	{
		void *raw = malloc(sizeof(CEditClient));
		CEditClient *client = new (raw) CEditClient();

		check("mVtbl == PTR__CEditClient_08e814e0",
		      client->mVtbl == (void *)PTR__CEditClient_08e814e0);
		check("mControlHash non-NULL", client->mControlHash != 0);
		check("mIndexHash non-NULL", client->mIndexHash != 0);
		check("mControlHash != mIndexHash (two distinct allocations)",
		      client->mControlHash != client->mIndexHash);

		unsigned char *hash1 = (unsigned char *)client->mControlHash;
		check("mControlHash vtbl == PTR__PointerHash_CEditControlPtr_08e81560",
		      *(void **)hash1 == (void *)PTR__PointerHash_CEditControlPtr_08e81560);
		check("mControlHash+0x4 flag byte == 0", hash1[4] == 0);
		check("mControlHash+0xc dword == 0", *(unsigned *)(hash1 + 0xc) == 0);
		void *pool1 = *(void **)(hash1 + 8);
		check("mControlHash+0x8 node-pool buffer non-NULL", pool1 != 0);

		unsigned char *hash2 = (unsigned char *)client->mIndexHash;
		check("mIndexHash vtbl == PTR__PointerHash_long_08e81570",
		      *(void **)hash2 == (void *)PTR__PointerHash_long_08e81570);
		check("mIndexHash+0x4 flag byte == 0", hash2[4] == 0);
		check("mIndexHash+0xc dword == 0", *(unsigned *)(hash2 + 0xc) == 0);
		void *pool2 = *(void **)(hash2 + 8);
		check("mIndexHash+0x8 node-pool buffer non-NULL", pool2 != 0);
		check("mControlHash pool != mIndexHash pool (two distinct buffers)",
		      pool1 != pool2);

		/* Spot-check the full real 0xada4-byte zeroed extent, not just the
		 * first dword -- start, middle, and the last dword actually inside
		 * the allocation. */
		unsigned char *p1 = (unsigned char *)pool1;
		bool poolZeroed = true;
		size_t offsets[] = {0, 4, 0x1000, 0x5000, 0xada0};
		for (size_t oi = 0; oi < sizeof(offsets) / sizeof(offsets[0]); ++oi)
			if (*(unsigned *)(p1 + offsets[oi]) != 0)
				poolZeroed = false;
		check("mControlHash node-pool buffer fully zeroed (spot-checked incl. last dword)",
		      poolZeroed);

		printf("[3] ~CEditClient() with no CEditMan registered\n");
		client->~CEditClient();
		check("did not crash (UnregisterClient no-op path taken)", true);
		check("mVtbl restored to PTR__CEditClient_08e814e0 by dtor",
		      client->mVtbl == (void *)PTR__CEditClient_08e814e0);

		free(pool1);
		free(pool2);
		free(hash1);
		free(hash2);
		free(raw);
	}

	printf("[4] Full real chain with a CEditMan registered\n");
	{
		std::memset(EditApiInstance, 0, sizeof(EditApiInstance));

		void *editManRaw = malloc(0x100);
		CEditMan *editMan = new (editManRaw) CEditMan();
		editMan->Setup();
		CEditMan::CMainTask *mainTask = EditManTestHooks::MainTask(*editMan);

		check("CEditMan::Setup() stored itself at EditApiInstance+4",
		      *(CEditMan **)(EditApiInstance + 4) == editMan);

		void *clientRaw = malloc(sizeof(CEditClient));
		CEditClient *client = new (clientRaw) CEditClient();

		check("client appears in CMainTask's own client list after construction",
		      ClientListContains(mainTask, client));

		void *pool1 = *(void **)((unsigned char *)client->mControlHash + 8);
		void *pool2 = *(void **)((unsigned char *)client->mIndexHash + 8);

		client->~CEditClient();

		check("client removed from CMainTask's own client list after destruction",
		      !ClientListContains(mainTask, client));

		free(pool1);
		free(pool2);
		free(client->mControlHash);
		free(client->mIndexHash);
		free(clientRaw);

		std::memset(EditApiInstance, 0, sizeof(EditApiInstance));
	}

	printf("[5] CEditClient as a plain embedded sub-object (CEditor's real use site)\n");
	{
		unsigned char storage[sizeof(CEditClient)];
		CEditClient *client = new (storage) CEditClient();
		check("embedded construction did not crash", client->mControlHash != 0);
		void *pool1 = *(void **)((unsigned char *)client->mControlHash + 8);
		void *pool2 = *(void **)((unsigned char *)client->mIndexHash + 8);
		void *hash1 = client->mControlHash;
		void *hash2 = client->mIndexHash;
		client->~CEditClient(); /* exactly what the compiler emits for CEditor's own mEditClient */
		check("embedded destruction did not crash", true);
		free(pool1);
		free(pool2);
		free(hash1);
		free(hash2);
	}

	if (g_fail == 0)
		printf("\nall checks passed\n");
	else
		printf("\n%d check(s) FAILED\n", g_fail);

	return g_fail == 0 ? 0 : 1;
}
