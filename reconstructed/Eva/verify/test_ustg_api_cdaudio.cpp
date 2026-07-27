/*
 * test_ustg_api_cdaudio.cpp  -  host-side known-answer test for
 * src/ipc/{cvalue,ustg_api_sampling,ustg_api_cdaudio}.cpp (added 2026-07-27).
 *
 * Same rationale as test_ustg_api_wrappers.cpp: reads back the exact wire bytes
 * written to the (test-hooked) user2rt pipe and feeds canned response frames
 * through the rt2user pipe, checking field-by-field against the real shapes
 * decoded from `objdump -dr -M intel`.
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "eva_types.h"
#include "ustg_api_cdaudio.h"
#include "ustg_api_sampling.h"
#include "ustg_user_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
	fflush(stdout);
}

struct UstgUserApiTestHooks {
	static void SetUser2Rt(int fd) { USTGUserAPI::m_activeUser2rtFD = fd; }
	static void SetRt2User(int fd) { USTGUserAPI::m_activeRt2userFD = fd; }
};

static int g_user2rt[2];
static int g_rt2user[2];

/* Reads back exactly `expectLen` bytes (NOT "up to sizeof(buf)" -- PlayStandby()
 * below issues 2 separate wire writes per call, so an oversized read would grab
 * both at once; read() never returns more than the byte count requested, so
 * requesting exactly expectLen isolates each write even back-to-back on the pipe).
 */
static void expect_wire(const char *label, const void *expected, unsigned expectLen)
{
	char buf[128];
	int n = read(g_user2rt[0], buf, expectLen);
	char what[160];
	snprintf(what, sizeof(what), "%s (wrote %d bytes, expected %u)", label, n, expectLen);
	check(what, n == (int)expectLen && memcmp(buf, expected, expectLen) == 0);
}

/* Same length-prefixed frame shape as test_ustg_api_wrappers.cpp's feed_response(). */
static void feed_response(const unsigned char *payload, unsigned short payloadLen)
{
	unsigned short total = (unsigned short)(2 + payloadLen);
	write(g_rt2user[1], &total, 2);
	write(g_rt2user[1], payload, payloadLen);
}

/* Feeds a full 24-byte SamplingSimpleMsg-shaped response (len=0x18 implied). */
static void feed_sampling_response(unsigned int subcode, unsigned int p0, unsigned int p1, unsigned int p2)
{
	struct __attribute__((packed)) {
		unsigned short subtype;
		unsigned int type, subcode, p0, p1, p2;
	} payload = { 1, 1, subcode, p0, p1, p2 };
	feed_response(reinterpret_cast<const unsigned char *>(&payload), sizeof(payload));
}

int main()
{
	pipe(g_user2rt);
	pipe(g_rt2user);
	UstgUserApiTestHooks::SetUser2Rt(g_user2rt[1]);
	UstgUserApiTestHooks::SetRt2User(g_rt2user[0]);

	/* --- CValue serialization rules (pure, no I/O) --- */
	{
		CValue v;
		memset(&v, 0xAA, sizeof(v));
		WriteCValueDword(v, 0x12345678u);
		unsigned char expected[8] = { 1, 4, 0, 0, 0x78, 0x56, 0x34, 0x12 };
		check("WriteCValueDword byte-exact", memcmp(v.raw, expected, 8) == 0);
	}
	{
		/* Self-describing blob: byte[1] of the source is the length prefix,
		 * real copied size = byte[1] + 4 (see eva_types.h).
		 */
		unsigned char src[9] = { 0x03, 0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00, 0x00 };
		CValue dest;
		memset(&dest, 0, sizeof(dest));
		CopyCValueBlob(dest, src);
		/* len = src[1](5) + 4 = 9 bytes copied */
		check("CopyCValueBlob copies exactly len+4 bytes", memcmp(dest.raw, src, 9) == 0);
		check("CopyCValueBlob does not overrun past len+4", dest.raw[9] == 0);
	}

	/* --- USTGAPISampling primitives --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp = {
			0x18, 1, 1, 0xc, /*p0=arg3*/ 30, /*p1=opcode*/ 0x50, /*p2=arg2*/ 20
		};
		USTGAPISampling::SendSimpleMessage(0x50, 20, 30);
		expect_wire("SendSimpleMessage wire bytes (opcode=1st, arg2=2nd->payload2, arg3=3rd->payload0)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp = {
			0x18, 1, 1, 0xc, 0, /*opcode*/ 0x51, 0
		};
		feed_sampling_response(9, 0, 0, 0x1234);
		unsigned long out = 0xdeadbeef;
		bool ok = USTGAPISampling::ReceiveSimpleMessage(0x51, out);
		expect_wire("ReceiveSimpleMessage wire bytes", &exp, sizeof(exp));
		check("ReceiveSimpleMessage returns true on subcode==9", ok);
		check("ReceiveSimpleMessage writes back response payload2", out == 0x1234);
	}
	{
		/* subcode mismatch -> false, out still updated from whatever arrived */
		feed_sampling_response(0x99, 0, 0, 0x5555);
		unsigned long out = 0;
		bool ok = USTGAPISampling::ReceiveSimpleMessage(0x52, out);
		char drain[64];
		read(g_user2rt[0], drain, sizeof(drain));
		check("ReceiveSimpleMessage returns false on subcode!=9", ok == false);
		check("ReceiveSimpleMessage still writes back payload2 on mismatch", out == 0x5555);
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp = {
			0x18, 1, 1, 0xc, /*p0=arg3*/ 7, /*p1=opcode*/ 0x60, /*p2=arg4*/ 8
		};
		feed_sampling_response(9, 0x111, 0x222, 0x333);
		char buf[24];
		bool ok = USTGAPISampling::ReceiveMessage(buf, 0x60, 7, 8);
		expect_wire("ReceiveMessage wire bytes (opcode=2nd param)", &exp, sizeof(exp));
		check("ReceiveMessage returns true on subcode==9", ok);
		unsigned int p0back;
		memcpy(&p0back, buf + 0xc, sizeof(p0back));
		check("ReceiveMessage buffer holds response payload0 after the call", p0back == 0x111);
	}

	/* --- USTGAPICDAudio --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp = {
			0x18, 1, 1, 0xc, 0, 0x37, /*level*/ 0x64
		};
		USTGAPICDAudio::SetLevel(0x64);
		expect_wire("SetLevel wire bytes (opcode 0x37)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp = {
			0x18, 1, 1, 0xc, /*chan*/ 3, 0x38, /*level*/ 0x50
		};
		USTGAPICDAudio::SetChanLevel(3, 0x50);
		expect_wire("SetChanLevel wire bytes (opcode 0x38, level before chan in payload)", &exp, sizeof(exp));
	}
	{
		feed_sampling_response(9, 0, 0, 0);
		bool ok = USTGAPICDAudio::PlayStart();
		char drain[64]; read(g_user2rt[0], drain, sizeof(drain));
		check("PlayStart returns true when response payload2==0", ok);
	}
	{
		feed_sampling_response(9, 0, 0, 7);
		bool ok = USTGAPICDAudio::PlayStop();
		char drain[64]; read(g_user2rt[0], drain, sizeof(drain));
		check("PlayStop returns false when response payload2!=0", ok == false);
	}
	{
		/* GetCurrentPosition: normal case, payload0(status) >= 0 */
		feed_sampling_response(9, 2 /*status*/, 0, 999 /*position*/);
		unsigned long pos = 0;
		EAudioStatus status = 0;
		bool ok = USTGAPICDAudio::GetCurrentPosition(pos, status);
		char drain[64]; read(g_user2rt[0], drain, sizeof(drain));
		check("GetCurrentPosition ok path returns true", ok);
		check("GetCurrentPosition ok path sets position from payload2", pos == 999);
		check("GetCurrentPosition ok path sets status from payload0", status == 2);
	}
	{
		/* GetCurrentPosition: negative status field -> position=0, status=3, false */
		feed_sampling_response(9, (unsigned int)-1, 0, 12345);
		unsigned long pos = 111;
		EAudioStatus status = 111;
		bool ok = USTGAPICDAudio::GetCurrentPosition(pos, status);
		char drain[64]; read(g_user2rt[0], drain, sizeof(drain));
		check("GetCurrentPosition negative-status path returns false", ok == false);
		check("GetCurrentPosition negative-status path zeroes position", pos == 0);
		check("GetCurrentPosition negative-status path sets status=3", status == 3);
	}
	{
		/* PlayStandby: needs SharedScratch() non-NULL, i.e.
		 * USTGUserAPI::mFrontPanelStatusAddress set -- reuses the same public
		 * static ustg_user_api.h already exposes (no friend needed).
		 */
		static unsigned char scratchRegion[0xd34 + 0x100];
		USTGUserAPI::mFrontPanelStatusAddress = scratchRegion;

		feed_sampling_response(9, 0, 0, 0 /*payload2==0 -> success*/);
		bool ok = USTGAPICDAudio::PlayStandby("TESTFILE.WAV", 1, 2, 3, 4);

		/* First wire write is the SendSimpleMessage(0x33,a3,a5) call. */
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp1 = {
			0x18, 1, 1, 0xc, /*a5*/ 4, 0x33, /*a3*/ 2
		};
		expect_wire("PlayStandby 1st send (SendSimpleMessage 0x33)", &exp1, sizeof(exp1));

		/* 2nd wire write is ReceiveMessage(&local,0x32,a4,a2). */
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, p0, p1, p2; } exp2 = {
			0x18, 1, 1, 0xc, /*a4*/ 3, 0x32, /*a2*/ 1
		};
		expect_wire("PlayStandby 2nd send (ReceiveMessage 0x32)", &exp2, sizeof(exp2));

		check("PlayStandby returns true on payload2==0", ok);
		check("PlayStandby copies name into SharedScratch()",
		      memcmp(scratchRegion + 0xd34, "TESTFILE.WAV", 13) == 0);
	}
	{
		/* PlayStandby with SharedScratch() NULL -- real early-return, no I/O. */
		USTGUserAPI::mFrontPanelStatusAddress = 0;
		bool ok = USTGAPICDAudio::PlayStandby("X", 0, 0, 0, 0);
		check("PlayStandby returns false immediately when SharedScratch() is NULL", ok == false);
	}

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
