/*
 * test_client_comm_server.cpp  -  host-side known-answer test for
 * CClientCommServer::ComputeCRCByte()/CheckIncomingSexCRCByte() (src/ipc/
 * client_comm_server.cpp, Stage 6 breadth sweep, 2026-07-25).
 *
 * Both are a running XOR checksum over a byte range -- easy to hand-compute
 * expected values for small buffers and cross-check the collapsed-loop
 * implementation against a trivial reference loop.
 */

#include <cstdio>
#include <cstring>

#include "client_comm_server.h"

/* Friend hook -- pokes mEvTag/mEvBuf directly so ComputeCRCByte() can be
 * exercised without running the real (Tier-B, does not populate them)
 * constructor. Same convention as module.h's ModuleTestHooks.
 */
struct ClientCommServerTestHooks {
	static void SetEvent(CClientCommServer &obj, const unsigned char *buf, unsigned char len)
	{
		obj.mEvTag = (int)((unsigned)len << 16);
		obj.mEvBuf = (void *)buf;
	}
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Reference implementation -- a naive, unrolled-loop-free XOR fold, matching
 * ComputeCRCByte()'s own documented semantics: crc = buf[start], then XOR in
 * buf[start+1 .. len-1] (byte-wrapping index).
 */
static unsigned char ReferenceComputeCRC(const unsigned char *buf, unsigned char len,
                                          unsigned char start)
{
	unsigned char crc = buf[start];
	for (unsigned char i = (unsigned char)(start + 1); i < len; i = (unsigned char)(i + 1))
		crc ^= buf[i];
	return crc;
}

int main()
{
	/* CClientCommServer has a Tier-B ctor requiring real CSexServiceTask&/
	 * CSysExMsgTaskBase& objects this pass doesn't construct -- allocate raw
	 * storage and only exercise the two Tier-A pure methods, matching
	 * test_module_adjust_task_mask.cpp's own "poke the real offsets, skip the
	 * real ctor" pattern.
	 */
	union {
		double alignForcer;
		unsigned char bytes[sizeof(CClientCommServer)];
	} raw;
	memset(raw.bytes, 0, sizeof(raw.bytes));
	CClientCommServer *obj = reinterpret_cast<CClientCommServer *>(raw.bytes);

	/* --- ComputeCRCByte --- */
	{
		unsigned char buf[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x00};
		ClientCommServerTestHooks::SetEvent(*obj, buf, 8);

		for (unsigned char start = 0; start < 7; start++) {
			unsigned char expect = ReferenceComputeCRC(buf, 8, start);
			unsigned char got = obj->ComputeCRCByte(start);
			char label[64];
			snprintf(label, sizeof(label), "ComputeCRCByte(start=%u) == 0x%02x", start, expect);
			check(label, got == expect);
		}

		/* Single-byte range (start == len-1): crc is just buf[start], no folding. */
		ClientCommServerTestHooks::SetEvent(*obj, buf, 1);
		check("ComputeCRCByte single-byte range", obj->ComputeCRCByte(0) == buf[0]);

		/* Longer buffer, exercises the >8-byte unrolled-loop-equivalent path. */
		unsigned char big[20];
		for (int i = 0; i < 20; i++) big[i] = (unsigned char)(i * 7 + 3);
		ClientCommServerTestHooks::SetEvent(*obj, big, 20);
		check("ComputeCRCByte 20-byte buffer, start=0",
		      obj->ComputeCRCByte(0) == ReferenceComputeCRC(big, 20, 0));
		check("ComputeCRCByte 20-byte buffer, start=5",
		      obj->ComputeCRCByte(5) == ReferenceComputeCRC(big, 20, 5));
	}

	/* --- CheckIncomingSexCRCByte --- */
	{
		/* len < 2 always rejects, regardless of content. */
		unsigned char one[1] = {0x00};
		check("CheckIncomingSexCRCByte len=0 rejects", obj->CheckIncomingSexCRCByte(one, 0) == false);
		check("CheckIncomingSexCRCByte len=1 rejects", obj->CheckIncomingSexCRCByte(one, 1) == false);

		/* A buffer whose own trailing byte is the running XOR of the rest
		 * self-verifies (crc folds to 0).
		 */
		unsigned char valid[5] = {0x11, 0x22, 0x33, 0x44, 0x00};
		unsigned char x = 0;
		for (int i = 0; i < 4; i++) x ^= valid[i];
		valid[4] = x;
		check("CheckIncomingSexCRCByte accepts self-consistent buffer",
		      obj->CheckIncomingSexCRCByte(valid, 5) == true);

		/* Corrupting any single byte must reject. */
		unsigned char corrupt[5] = {0x11, 0x22, 0x33, 0x44, (unsigned char)(x ^ 0x01)};
		check("CheckIncomingSexCRCByte rejects corrupted buffer",
		      obj->CheckIncomingSexCRCByte(corrupt, 5) == false);

		/* Longer (>16-byte) self-consistent buffer, exercises the SIMD-fold-
		 * equivalent path the real binary uses for len >= 0x11.
		 */
		unsigned char longbuf[24];
		for (int i = 0; i < 23; i++) longbuf[i] = (unsigned char)(i * 13 + 1);
		unsigned char lx = 0;
		for (int i = 0; i < 23; i++) lx ^= longbuf[i];
		longbuf[23] = lx;
		check("CheckIncomingSexCRCByte accepts 24-byte self-consistent buffer",
		      obj->CheckIncomingSexCRCByte(longbuf, 24) == true);
		longbuf[23] ^= 0x80;
		check("CheckIncomingSexCRCByte rejects corrupted 24-byte buffer",
		      obj->CheckIncomingSexCRCByte(longbuf, 24) == false);
	}

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
