/*
 * test_ustg_api_wrappers.cpp  -  host-side known-answer test for
 * src/ipc/ustg_api_wrappers.cpp (added 2026-07-27).
 *
 * Every check below reads back the exact wire bytes USTGUserAPI::
 * SendSTGMessageWithSource() writes to the (test-hooked) user2rt pipe and compares
 * them field-by-field against the real message shape decoded from
 * `objdump -dr -M intel` (see ustg_api_wrappers.cpp's own header comment) -- this is
 * the class of bug most likely from a hand-transcription of this many near-identical
 * field layouts (a byte in the wrong slot desyncs the real IPC protocol), so every
 * function gets its own byte-exact check rather than just "did it not crash".
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "ustg_api_wrappers.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Friend accessor -- same shape as test_ustg_user_api.cpp's own (a fresh
 * definition here since each verify/ binary is compiled independently).
 */
struct UstgUserApiTestHooks {
	static void SetUser2Rt(int fd) { USTGUserAPI::m_activeUser2rtFD = fd; }
	static void SetRt2User(int fd) { USTGUserAPI::m_activeRt2userFD = fd; }
};

static int g_user2rt[2];
static int g_rt2user[2];

/* Reads back exactly `expectLen` bytes written to the user2rt pipe and compares. */
static void expect_wire(const char *label, const void *expected, unsigned expectLen)
{
	char buf[128];
	int n = read(g_user2rt[0], buf, sizeof(buf));
	char what[160];
	snprintf(what, sizeof(what), "%s (wrote %d bytes, expected %u)", label, n, expectLen);
	check(what, n == (int)expectLen && memcmp(buf, expected, expectLen) == 0);
}

/* Feeds a canned response frame (2-byte total-length prefix + payload) into the
 * rt2user pipe for a subsequent SharedMemXxxDump() poll to read back.
 */
static void feed_response(const unsigned char *payload, unsigned short payloadLen)
{
	unsigned short total = (unsigned short)(2 + payloadLen);
	write(g_rt2user[1], &total, 2);
	write(g_rt2user[1], payload, payloadLen);
}

int main()
{
	pipe(g_user2rt);
	pipe(g_rt2user);
	UstgUserApiTestHooks::SetUser2Rt(g_user2rt[1]);
	UstgUserApiTestHooks::SetRt2User(g_rt2user[0]);

	/* --- USTGAPICombi --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 0, /*a1*/10, /*a2*/11, /*a4*/13, /*a3*/12, /*a5*/14, /*perf*/3
		};
		USTGAPICombi::UpdateCombiParameter(10, 11, 12, 13, 14, 3);
		expect_wire("UpdateCombiParameter wire bytes", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 1, 20, 21, 23, 22, 24, 5
		};
		USTGAPICombi::UpdateVectorMotionParameter(20, 21, 22, 23, 24, 5);
		expect_wire("UpdateVectorMotionParameter wire bytes (subcode=1)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 2, 30, 31, 33, 32, 34, 6
		};
		USTGAPICombi::UpdateControllerInfoParameter(30, 31, 32, 33, 34, 6);
		expect_wire("UpdateControllerInfoParameter wire bytes (subcode=2)", &exp, sizeof(exp));
	}
	{
		/* round 59: 4-param convenience overload -- forwards to the 6-param
		 * version above with a1=0/a2=0xffff hardcoded, own params passed
		 * through as a3/a4/a5/perfType.
		 */
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 2, /*a1*/0, /*a2*/0xffff, /*a4*/61, /*a3*/60, /*a5*/62, /*perf*/9
		};
		USTGAPICombi::UpdateControllerInfoParameter(60, 61, 62, 9);
		expect_wire("UpdateControllerInfoParameter(4-param) forwards with a1=0/a2=0xffff", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 4, 40, 41, 43, 42, 44, 7
		};
		USTGAPICombi::UpdateAudioInputParameter(40, 41, 42, 43, 44, 7);
		expect_wire("UpdateAudioInputParameter wire bytes (subcode=4)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a4, a3, a5, perf; } exp = {
			0x24, 1, 2, 5, 50, 51, 53, 52, 54, 8
		};
		USTGAPICombi::UpdateEffectBalanceParameter(50, 51, 52, 53, 54, 8);
		expect_wire("UpdateEffectBalanceParameter wire bytes (subcode=5)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a3, a4, a6, a5, a7, a8; } exp = {
			0x2c, 1, 2, 3, 1, 2, 3, 4, 6, 5, 7, 8
		};
		USTGAPICombi::UpdateToneAdjustParameter(1, 2, 3, 4, 5, 6, 7, 8);
		expect_wire("UpdateToneAdjustParameter wire bytes", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, const0, a1, a2, a3, const2; } exp = {
			0x20, 1, 2, 6, 0, 100, 101, 102, 2
		};
		USTGAPICombi::UpdateSequenceMetronomeParameter(100, 101, 102);
		expect_wire("UpdateSequenceMetronomeParameter wire bytes", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, bank, prog, prog2, bank2, perf; } exp = {
			0x20, 1, 2, 7, 5, 9, 9, 5, 2
		};
		unsigned char resp[16] = {0};
		unsigned int echoSubcode = 7;
		memcpy(resp + 6, &echoSubcode, 4); /* echo subcode=7 at response buffer+8 (payload index 6) */
		/* SharedMemCombiDump() sends first, THEN reads -- feed the response
		 * before calling so the very first ReadMessage() succeeds immediately.
		 */
		feed_response(resp, sizeof(resp));
		bool ok = USTGAPICombi::SharedMemCombiDump(5, 9, 2);
		expect_wire("SharedMemCombiDump wire bytes", &exp, sizeof(exp));
		check("SharedMemCombiDump returns true on subcode==7 echo", ok);
	}

	/* --- USTGAPIEffect / USTGAPIEffectSlot --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, a5, a6, perf; } exp = {
			0x24, 1, 9, 0, 1, 2, 3, 4, (unsigned int)(int)(short)-5, 6
		};
		USTGAPIEffect::UpdateParam(6, 1, 2, 3, 4, -5);
		expect_wire("USTGAPIEffect::UpdateParam wire bytes (type=9)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, a5, a6, perf; } exp = {
			0x24, 1, 8, 0, 11, 12, 13, 14, (unsigned int)(int)(short)15, 16
		};
		USTGAPIEffectSlot::UpdateParam(16, 11, 12, 13, 14, 15);
		expect_wire("USTGAPIEffectSlot::UpdateParam wire bytes (type=8)", &exp, sizeof(exp));
	}

	/* --- USTGAPIEffectMgr --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, a5, a6, perf; } exp = {
			0x24, 1, 7, 5, (unsigned int)-1, 2, 3, 4, 5, 9
		};
		USTGAPIEffectMgr::UpdateEffectLFOParameter(9, -1, 2, 3, 4, 5);
		expect_wire("UpdateEffectLFOParameter wire bytes", &exp, sizeof(exp));
	}

	/* --- USTGAPIGlobal --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a1, a3; } exp = {
			0x18, 1, 1, 0, 22, 11, (unsigned int)-3
		};
		USTGAPIGlobal::UpdateGlobalParameter(11, 22, -3);
		expect_wire("UpdateGlobalParameter wire bytes (a2 field before a1)", &exp, sizeof(exp));
	}

	/* --- USTGAPIHDRTrack --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a3, a4; } exp = {
			0x1c, 1, 0xe, 0, 1, 2, 3, (unsigned int)-4
		};
		USTGAPIHDRTrack::UpdateHDRTrackParameter(1, 2, 3, -4);
		expect_wire("UpdateHDRTrackParameter wire bytes", &exp, sizeof(exp));
	}

	/* --- USTGAPIProgramSlot --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, a5, a6, a8, a1, a7; } exp = {
			0x2c, 1, 3, 0, 2, 3, 4, 5, 6, 8, 1, 7
		};
		USTGAPIProgramSlot::UpdateProgramSlotParameter(1, 2, 3, 4, 5, 6, 7, 8);
		expect_wire("UpdateProgramSlotParameter wire bytes (a2..a6,a8,a1,a7 order)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, c0a, c0b, a5, a1, c0c; } exp = {
			0x2c, 1, 3, 2, 2, 3, 4, 0, 0, 1, 1, 0
		};
		USTGAPIProgramSlot::UpdateProgramSlotEnabled(1, 2, 3, 4, true);
		expect_wire("UpdateProgramSlotEnabled wire bytes (true)", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a2, a3, a4, c0a, c0b, a5, a1, c0c; } exp = {
			0x2c, 1, 3, 2, 2, 3, 4, 0, 0, 0, 1, 0
		};
		USTGAPIProgramSlot::UpdateProgramSlotEnabled(1, 2, 3, 4, false);
		expect_wire("UpdateProgramSlotEnabled wire bytes (false)", &exp, sizeof(exp));
	}

	/* --- USTGAPISetList --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a3, a4; } exp = {
			0x1c, 1, 0x10, 0, (unsigned int)-1, 2, 3, 4
		};
		USTGAPISetList::UpdateSlotParam(-1, 2, 3, 4);
		expect_wire("UpdateSlotParam wire bytes", &exp, sizeof(exp));
	}

	/* --- USTGAPIPatch --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1, a2, a3, a4; } exp = {
			0x1c, 1, 5, 1, 2, 3, (unsigned int)-4, 5
		};
		USTGAPIPatch::UpdateOscSelectByType(2, 3, -4, 5);
		expect_wire("UpdateOscSelectByType wire bytes", &exp, sizeof(exp));
	}

	/* --- USTGAPIDrumkitData --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1; } exp = {
			0x10, 1, 1, 0xd, 42
		};
		USTGAPIDrumkitData::SetCurrentKitId(42);
		expect_wire("SetCurrentKitId wire bytes", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1; } exp = {
			0x10, 1, 1, 0xf, 7
		};
		/* SharedMemDrumKitDump polls with bufSize=0x10 (16) -- the response
		 * frame's own total length (2 + payload) must fit within that, so the
		 * payload here is 10 bytes (just enough to place the echoed subcode
		 * at payload offset 6 == buffer offset 8), NOT the 16-byte payload
		 * SharedMemCombiDump's own bufSize=0x20 case above can afford.
		 */
		unsigned char resp[10] = {0};
		unsigned int echoSubcode = 0xf;
		memcpy(resp + 6, &echoSubcode, 4);
		feed_response(resp, sizeof(resp));
		bool ok = USTGAPIDrumkitData::SharedMemDrumKitDump(7);
		expect_wire("SharedMemDrumKitDump wire bytes", &exp, sizeof(exp));
		check("SharedMemDrumKitDump returns true on subcode==0xf echo", ok);
	}
	{
		/* Feed a response with a WRONG echoed subcode so the real "read
		 * succeeded but subcode mismatched" false path is exercised (still
		 * sized to fit bufSize=0x10 so ReadMessage() actually consumes it
		 * rather than rejecting it as oversize).
		 */
		unsigned char resp[10] = {0};
		unsigned int wrongSubcode = 0x99;
		memcpy(resp + 6, &wrongSubcode, 4);
		feed_response(resp, sizeof(resp));
		bool ok = USTGAPIDrumkitData::SharedMemDrumKitDump(7);
		char drain[64];
		read(g_user2rt[0], drain, sizeof(drain)); /* drain the send this call made */
		check("SharedMemDrumKitDump returns false on subcode mismatch", ok == false);
	}

	/* --- USTGAPIWaveSequenceData --- */
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1; } exp = {
			0x10, 1, 1, 0xe, 55
		};
		USTGAPIWaveSequenceData::SetCurrentSequenceId(55);
		expect_wire("SetCurrentSequenceId wire bytes", &exp, sizeof(exp));
	}
	{
		struct __attribute__((packed)) { unsigned short len, sub; unsigned int type, subcode, a1; } exp = {
			0x10, 1, 1, 0x10, 3
		};
		/* Same bufSize=0x10 sizing constraint as the DrumKitDump case above. */
		unsigned char resp[10] = {0};
		unsigned int echoSubcode = 0x10;
		memcpy(resp + 6, &echoSubcode, 4);
		feed_response(resp, sizeof(resp));
		bool ok = USTGAPIWaveSequenceData::SharedMemWaveSequenceDump(3);
		expect_wire("SharedMemWaveSequenceDump wire bytes", &exp, sizeof(exp));
		check("SharedMemWaveSequenceDump returns true on subcode==0x10 echo", ok);
	}

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
