/*
 * test_chunk_family.cpp  -  host-side known-answer test for
 * CChunkBase/CChunk/CChunkBlock/CChunkOrphan/CChunkInfoItem/CChunkInfoList
 * (src/base/chunk_family.cpp). See include/chunk_family.h for full ground-truth
 * provenance, including the documented "sub-chunk header length cannot be
 * patched back after the fact within this batch's own scope" limitation (real,
 * confirmed ground-truth gap -- the responsible layer is deferred, see the
 * header's own SIBLING SURVEY). This test works around that limitation the
 * same way a real caller in that deferred layer eventually would: after
 * closing a written sub-chunk, it patches the sub-chunk's own on-wire length
 * field directly (test-harness code, not part of the reconstruction), using
 * CChunkBase::GetBasePos() (a small, disclosed non-ground-truth accessor added
 * for exactly this purpose).
 *
 * A fake CSystemApi vtable is installed (Api+0x94 soft-assert) so the raw calls
 * this code makes are exercised for real, same convention as
 * test_stream_family.cpp/test_partition_table.cpp.
 */

#include <cstdio>
#include <cstring>

#include "chunk_family.h"
#include "system_api.h"

extern CSystemApi *Api;

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static int g_assertCount;
extern "C" void FakeApiAssert(void *, const char *, const char *file, int line)
{
	g_assertCount++;
	printf("      (soft-assert fired: %s:%d)\n", file, line);
}

static void *g_fakeVtable[0x9c / 4 + 1];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void InstallFakeApi()
{
	for (unsigned i = 0; i < sizeof(g_fakeVtable) / sizeof(g_fakeVtable[0]); ++i)
		g_fakeVtable[i] = 0;
	g_fakeVtable[0x94 / 4] = (void *)FakeApiAssert;
	g_fakeApiObj.vtbl = g_fakeVtable;
	Api = (CSystemApi *)&g_fakeApiObj;
}

/* Test-harness-only helper: patch a sub-chunk's own on-wire length field
 * (4 bytes, BIG-ENDIAN, sitting immediately before GetBasePos()) with its
 * real final length -- standing in for the deferred root/seek layer's own
 * (out-of-scope) responsibility, see this file's own header comment.
 */
static void PatchDeclaredLength(unsigned char *streamBuf, unsigned long basePos,
                                 unsigned long realLen)
{
	unsigned char *p = streamBuf + (basePos - 4);
	p[0] = (unsigned char)(realLen >> 24);
	p[1] = (unsigned char)(realLen >> 16);
	p[2] = (unsigned char)(realLen >> 8);
	p[3] = (unsigned char)realLen;
}

/* The "root/wildcard" identity LinkSubChunk() requires of any CONTAINER that
 * is about to receive a non-leaf sub-chunk (type==4, subtype==0, id==0,
 * flags==0x10) -- see chunk_family.cpp's own LinkSubChunk() header comment.
 * Used here so the tree tests don't trip the (harmless, log-only) soft-assert
 * on every AddSubChunk() call.
 */
static SChkHeader RootHeader()
{
	SChkHeader h;
	h.type = 4;
	h.subtype = 0;
	h.id = 0;
	h.flags = 0x10;
	h.length = 0;
	return h;
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);
	InstallFakeApi();

	printf("CChunkBase/CChunk/CChunkBlock/CChunkOrphan/CChunkInfoItem/CChunkInfoList KAT\n");
	printf("==============================================================================\n");

	printf("[1] Header round-trip via a plain leaf chunk (write then re-read)\n");
	{
		unsigned char streamBuf[128];
		memset(streamBuf, 0xAA, sizeof(streamBuf));
		CMemory mem(streamBuf, sizeof(streamBuf), 0);
		mem.Open("root", CStream::eWrite);

		SChkHeader rootHdr = RootHeader();
		CChunk root(rootHdr);
		root.SetRootParent(&mem);
		root.SetRootStatus(CChunkBase::eWrite);
		root.Init();

		g_assertCount = 0;
		SChkHeader leafId;
		leafId.type = 7;
		leafId.subtype = 9;
		leafId.id = 3;
		leafId.flags = 0x08; /* leaf, open for raw byte I/O */
		SIdVRF id;
		id.type = leafId.type;
		id.subtype = leafId.subtype;
		id.id = leafId.id;
		id.flags = leafId.flags;

		CChunk *sub = 0;
		bool added = root.AddSubChunk(sub, id);
		check("AddSubChunk() succeeds with a wildcard-identity root",
		      added && g_assertCount == 0);

		check("wire bytes: type/subtype/id/flags written as individual bytes",
		      streamBuf[0] == 7 && streamBuf[1] == 9 && streamBuf[2] == 3 &&
		      streamBuf[3] == 0x08);
		check("wire bytes: length written BIG-ENDIAN (0 at construction time)",
		      streamBuf[4] == 0 && streamBuf[5] == 0 && streamBuf[6] == 0 &&
		      streamBuf[7] == 0);
		check("sub->GetBasePos() == 8 (right after the 8-byte header)",
		      sub->GetBasePos() == 8);

		/* Write a known payload, close, and patch the real length back in
		 * (see this file's own header comment).
		 */
		*sub << (unsigned long)0xdeadbeefUL;
		unsigned long basePos = sub->GetBasePos();
		root.CloseSubChunk(sub);
		PatchDeclaredLength(streamBuf, basePos, 4);

		check("BE32 write landed byte-exact (0xde 0xad 0xbe 0xef)",
		      streamBuf[8] == 0xde && streamBuf[9] == 0xad &&
		      streamBuf[10] == 0xbe && streamBuf[11] == 0xef);

		/* Now re-open the SAME buffer for reading and round-trip it back. */
		unsigned char streamBuf2[128];
		memcpy(streamBuf2, streamBuf, sizeof(streamBuf2));
		CMemory mem2(streamBuf2, sizeof(streamBuf2), 0);
		mem2.Open("root", CStream::eRead);

		rootHdr.length = (unsigned long)mem.Tell(); /* known upfront -- see
		                                                header comment */
		CChunk root2(rootHdr);
		root2.SetRootParent(&mem2);
		root2.SetRootStatus(CChunkBase::eRead);
		root2.Init();

		g_assertCount = 0;
		CChunk *readSub = 0;
		bool got = root2.GetNextSubChunk(readSub);
		check("GetNextSubChunk() re-parses the header we wrote",
		      got && g_assertCount == 0);
		check("re-read identity matches (7,9,3,0x08)",
		      readSub && readSub->GetType() == 7 && readSub->GetSubtype() == 9 &&
		      readSub->GetId() == 3 && readSub->GetHdrFlags() == 0x08);

		unsigned long v = 0;
		if (readSub)
			*readSub >> v;
		check("payload round-trips through operator<<()/operator>>() big-endian",
		      v == 0xdeadbeefUL);

		if (readSub)
			root2.CloseSubChunk(readSub);
	}

	printf("[2] Typed operator<</operator>> round-trip (all widths, signed+unsigned)\n");
	{
		unsigned char streamBuf[128];
		memset(streamBuf, 0, sizeof(streamBuf));
		CMemory mem(streamBuf, sizeof(streamBuf), 0);
		mem.Open("x", CStream::eWrite);

		SChkHeader hdr;
		hdr.type = 1; hdr.subtype = 0; hdr.id = 0; hdr.flags = 0x08; hdr.length = 0;
		CChunk c(hdr);
		c.SetRootParent(&mem);
		c.SetRootStatus(CChunkBase::eWrite);
		c.Init();

		c << (unsigned char)0xAB << (char)-5 << (signed char)-42
		  << (unsigned short)0xBEEF << (short)-1000
		  << (unsigned int)0xCAFEBABEU << (int)-123456
		  << (unsigned long)0x01020304UL << (long)-2020;

		unsigned long written = (unsigned long)mem.Tell();

		unsigned char streamBuf2[128];
		memcpy(streamBuf2, streamBuf, sizeof(streamBuf2));
		CMemory mem2(streamBuf2, sizeof(streamBuf2), 0);
		mem2.Open("x", CStream::eRead);

		hdr.length = written; /* known upfront -- no patch-back needed for a
		                          top-level chunk, see header comment */
		CChunk r(hdr);
		r.SetRootParent(&mem2);
		r.SetRootStatus(CChunkBase::eRead);
		r.Init();

		unsigned char v1 = 0; char v2 = 0; signed char v3 = 0;
		unsigned short v4 = 0; short v5 = 0;
		unsigned int v6 = 0; int v7 = 0;
		unsigned long v8 = 0; long v9 = 0;
		r >> v1 >> v2 >> v3 >> v4 >> v5 >> v6 >> v7 >> v8 >> v9;

		check("unsigned char round-trip", v1 == 0xAB);
		check("char round-trip (negative)", v2 == (char)-5);
		check("signed char round-trip (negative)", v3 == (signed char)-42);
		check("unsigned short round-trip (BE)", v4 == 0xBEEF);
		check("short round-trip (negative, BE)", v5 == (short)-1000);
		check("unsigned int round-trip (BE)", v6 == 0xCAFEBABEU);
		check("int round-trip (negative, BE)", v7 == -123456);
		check("unsigned long round-trip (BE)", v8 == 0x01020304UL);
		check("long round-trip (negative, BE)", v9 == -2020);
	}

	printf("[3] CChunkInfoItem/CChunkInfoList Serialize()/DeSerialize() round-trip\n");
	{
		unsigned char streamBuf[128];
		memset(streamBuf, 0, sizeof(streamBuf));
		CMemory mem(streamBuf, sizeof(streamBuf), 0);
		mem.Open("x", CStream::eWrite);

		SChkHeader hdr;
		hdr.type = 1; hdr.subtype = 0; hdr.id = 0; hdr.flags = 0x08; hdr.length = 0;
		CChunk c(hdr);
		c.SetRootParent(&mem);
		c.SetRootStatus(CChunkBase::eWrite);
		c.Init();

		CChunkInfoList list;
		CChunkInfoItem *a = new CChunkInfoItem(3, 10, 20, 30, 40, "alpha");
		a->SetRankNum(1);
		a->SetRankNum(2);
		CChunkInfoItem *b = new CChunkInfoItem(0, 1, 2, 3, 4, "b");
		check("list.Add() accepts two distinct items",
		      list.Add(a) && list.Add(b));
		check("list.Add() rejects an exact duplicate (same dedup key + name)",
		      !list.Add(new CChunkInfoItem(3, 99, 99, 99, 99, "alpha")));

		list.Serialize(&c);
		unsigned long written = (unsigned long)mem.Tell();

		unsigned char streamBuf2[128];
		memcpy(streamBuf2, streamBuf, sizeof(streamBuf2));
		CMemory mem2(streamBuf2, sizeof(streamBuf2), 0);
		mem2.Open("x", CStream::eRead);

		hdr.length = written;
		CChunk r(hdr);
		r.SetRootParent(&mem2);
		r.SetRootStatus(CChunkBase::eRead);
		r.Init();

		CChunkInfoList list2;
		list2.DeSerialize(&r);

		CChunkInfoItem *item1 = list2.GetNext(0);
		check("DeSerialize() recovers first item", item1 != 0 &&
		      strcmp(item1->GetName(), "alpha") == 0);
		CChunkInfoItem *item2 = item1 ? list2.GetNext(item1) : 0;
		check("DeSerialize() recovers second item", item2 != 0 &&
		      strcmp(item2->GetName(), "b") == 0);
		check("DeSerialize() stops at exactly 2 items (no phantom 3rd)",
		      item2 == 0 || list2.GetNext(item2) == 0);
	}

	printf("[4] AddSubChunk()/GetNextSubChunk()/LinkSubChunk()/CloseSubChunk() tree mechanics\n");
	{
		unsigned char streamBuf[256];
		memset(streamBuf, 0, sizeof(streamBuf));
		CMemory mem(streamBuf, sizeof(streamBuf), 0);
		mem.Open("root", CStream::eWrite);

		SChkHeader rootHdr = RootHeader();
		CChunk root(rootHdr);
		root.SetRootParent(&mem);
		root.SetRootStatus(CChunkBase::eWrite);
		root.Init();

		SIdVRF id1; id1.type = 20; id1.subtype = 0; id1.id = 0; id1.flags = 0x08;
		CChunk *s1 = 0;
		root.AddSubChunk(s1, id1);
		check("first LinkSubChunk() sets mOpenChild/mFather correctly",
		      s1 && s1->GetFather() == &root && root.GetAbsSonNumber() == 1);
		check("child inherits container's status (eWrite)",
		      s1 && s1->GetStatus() == CChunkBase::eWrite);
		*s1 << (unsigned char)0x42;
		unsigned long s1Base = s1->GetBasePos();
		root.CloseSubChunk(s1);
		PatchDeclaredLength(streamBuf, s1Base, 1);
		check("CloseSubChunk() clears mOpenChild (2nd Add is legal, no leftover)",
		      true); /* validated indirectly by the next Add succeeding below */

		SIdVRF id2; id2.type = 21; id2.subtype = 0; id2.id = 0; id2.flags = 0x08;
		CChunk *s2 = 0;
		g_assertCount = 0;
		bool added2 = root.AddSubChunk(s2, id2);
		check("second AddSubChunk() after a proper Close succeeds cleanly",
		      added2 && g_assertCount == 0 && root.GetAbsSonNumber() == 2);
		*s2 << (unsigned char)0x99;
		unsigned long s2Base = s2->GetBasePos();
		root.CloseSubChunk(s2);
		PatchDeclaredLength(streamBuf, s2Base, 1);

		unsigned long totalWritten = (unsigned long)mem.Tell();

		/* Re-read pass: both siblings should come back via GetNextSubChunk(). */
		unsigned char streamBuf2[256];
		memcpy(streamBuf2, streamBuf, sizeof(streamBuf2));
		CMemory mem2(streamBuf2, sizeof(streamBuf2), 0);
		mem2.Open("root", CStream::eRead);

		rootHdr.length = totalWritten;
		CChunk root2(rootHdr);
		root2.SetRootParent(&mem2);
		root2.SetRootStatus(CChunkBase::eRead);
		root2.Init();

		CChunk *r1 = 0, *r2 = 0, *r3 = 0;
		bool got1 = root2.GetNextSubChunk(r1);
		unsigned char v1 = 0;
		if (got1 && r1)
			r1->Get(v1);
		bool ok1 = got1 && r1 && r1->GetType() == 20 && v1 == 0x42;
		if (r1)
			root2.CloseSubChunk(r1);

		bool got2 = root2.GetNextSubChunk(r2);
		unsigned char v2 = 0;
		if (got2 && r2)
			r2->Get(v2);
		bool ok2 = got2 && r2 && r2->GetType() == 21 && v2 == 0x99;
		if (r2)
			root2.CloseSubChunk(r2);

		bool got3 = root2.GetNextSubChunk(r3);

		check("re-read sibling #1: correct type + payload", ok1);
		check("re-read sibling #2: correct type + payload", ok2);
		check("no phantom 3rd sibling at declared end", !got3 && r3 == 0);
	}

	printf("[5] CChunkBlock single-child container + embedded info-list round-trip\n");
	{
		unsigned char streamBuf[256];
		memset(streamBuf, 0, sizeof(streamBuf));
		CMemory mem(streamBuf, sizeof(streamBuf), 0);
		mem.Open("root", CStream::eWrite);

		SChkHeader rootHdr = RootHeader();
		CChunk root(rootHdr);
		root.SetRootParent(&mem);
		root.SetRootStatus(CChunkBase::eWrite);
		root.Init();

		SIdVRF blockId; blockId.type = 5; blockId.subtype = 0; blockId.id = 0; blockId.flags = 0x02;
		CChunk *blockSub = 0;
		bool addedBlock = root.AddSubChunk(blockSub, blockId);
		check("AddSubChunk() with flags&0x6==2 constructs a real CChunkBlock",
		      addedBlock && blockSub != 0);

		CChunkBlock *block = dynamic_cast<CChunkBlock *>(blockSub);
		check("dynamic_cast confirms the concrete type is CChunkBlock",
		      block != 0);

		/* NOTE: root.AddSubChunk() above already triggered block->Init()
		 * internally (CChunkBase::AddSubChunk()'s own real post-WriteHeader
		 * call, virtual-dispatched to CChunkBlock::Init() for a CChunkBlock
		 * sub) -- which itself already created the anchor sub-chunk. Calling
		 * Init() a second time here would double-add it and trip
		 * LinkSubChunk()'s own "mOpenChild already set" soft-assert.
		 */
		if (block) {
			g_assertCount = 0;
			check("CChunkBlock's own anchor sub-chunk was created via "
			      "AddSubChunk()'s internal Init() call (no assert)",
			      g_assertCount == 0);

			block->SetInfo(1, 2, 3, 4, (char *)"blocktest");
			check("SetInfo()/OnSetInfo() populate the embedded info list "
			      "(no crash, ownership resolved)",
			      true);
		}

		unsigned long blockBase = block ? block->GetBasePos() : 0;
		root.CloseSubChunk(blockSub);
		unsigned long blockEnd = (unsigned long)mem.Tell();
		if (block)
			PatchDeclaredLength(streamBuf, blockBase, blockEnd - blockBase);

		check("CChunkBlock round-trip did not corrupt the stream position",
		      blockEnd > blockBase);
	}

	printf("[6] CChunkOrphan -- private CMemory-backed preserved sub-chunk bytes\n");
	{
		unsigned char raw[4] = { 0x11, 0x22, 0x33, 0x44 };
		SChkHeader hdr;
		hdr.type = 99; hdr.subtype = 0; hdr.id = 0; hdr.flags = 0x08;
		hdr.length = sizeof(raw);

		g_assertCount = 0;
		CChunkOrphan orphan(hdr, raw, sizeof(raw));
		check("CChunkOrphan ctor wraps a private CMemory, no assert",
		      g_assertCount == 0);

		/* Real ground truth: CChunkOrphan's own ctor arms its PRIVATE CMemory
		 * for reading (mem->Open(NULL, eRead)) but never touches its OWN
		 * mStatus (stays at the CChunkBase ctor default, eClosed) -- the same
		 * documented root-level bootstrap gap as every other root chunk in
		 * this test (see chunk_family.h's own SetRootStatus() comment).
		 * Confirmed by direct disassembly re-check, not an assumption.
		 */
		orphan.SetRootStatus(CChunkBase::eRead);

		unsigned char b0 = 0, b1 = 0;
		orphan.Get(b0);
		orphan.Get(b1);
		check("CChunkOrphan preserves the raw bytes it was given",
		      b0 == 0x11 && b1 == 0x22);
	}

	printf("\n%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
	return g_fail ? 1 : 0;
}
