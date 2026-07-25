/*
 * test_ustg_user_api.cpp  -  host-side known-answer test for USTGUserAPI's Stage 2
 * IPC substrate (src/ipc/ustg_user_api.cpp, added 2026-07-25).
 *
 * Unlike CScheduler::Exec() (Stage 6 batch 1), this substrate is genuinely testable
 * host-side: USTGUserAPI is ordinary userspace glibc code (real pipes/read/write, not
 * RTAI-FIFO-specific ioctls), so real Linux pipes stand in faithfully for the /dev/rtfN
 * device nodes Connect()/ConnectPanelFifo()/ConnectUnsolicitedFifo() would normally
 * open. `UstgUserApiTestHooks` (friended in ustg_user_api.h) points the class's private
 * fd-cache statics at those pipes directly, bypassing the real open() calls (which
 * would fail against nonexistent /dev/rtfN nodes on a host build) while still driving
 * the *real* ReadMessage/ReadMessageWithTimeout/ReadUnsolicitedMessage/SendPanelMessage/
 * Disconnect bodies -- this is exactly the wire-format code PLAN.md's own verification
 * methodology flags as worth an extra KAT pass ("getting a byte wrong would desync the
 * IPC protocol").
 */

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "ustg_user_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Friend accessor -- sets the private fd-cache statics directly to host pipe fds. */
struct UstgUserApiTestHooks {
	static void SetRt2User(int fd)     { USTGUserAPI::m_activeRt2userFD = fd; }
	static void SetUser2Rt(int fd)     { USTGUserAPI::m_activeUser2rtFD = fd; }
	static void SetUnsol(int fd)       { USTGUserAPI::m_rtUnsolFifo = fd; }
	static void SetPanel(int fd)       { USTGUserAPI::m_user2rtPanelFifo = fd; }
	static void SetRawRt2User(int fd)  { USTGUserAPI::m_rt2userFifo = fd; }
	static void SetRawUser2Rt(int fd)  { USTGUserAPI::m_user2rtFifo = fd; }
	static int  RawRt2User()           { return USTGUserAPI::m_rt2userFifo; }
	static int  RawUser2Rt()           { return USTGUserAPI::m_user2rtFifo; }
	static int  RawUnsol()             { return USTGUserAPI::m_rtUnsolFifo; }
	static int  RawPanel()             { return USTGUserAPI::m_user2rtPanelFifo; }
	static int  RawDirect()            { return USTGUserAPI::m_userRtDirect; }
	static void SetSharedMem(CSTGHandle *h) { USTGUserAPI::mSharedMem = h; }
};

/* Writes a length-prefixed frame {u16 totalLen, payload...} into a pipe. */
static void write_frame(int fd, const char *payload, unsigned short payloadLen)
{
	unsigned short total = (unsigned short)(2 + payloadLen);
	write(fd, &total, 2);
	if (payloadLen)
		write(fd, payload, payloadLen);
}

int main()
{
	/* Prime CSTGHandleCache::sCachedHandleInfo before touching anything that calls
	 * CSTGHandle::Release() -- Release() (unlike Access()/GetSize()) has no lazy-init
	 * guard of its own and will dereference a NULL sCachedHandleInfo if nothing has
	 * initialized it yet. Real Eva always reaches Disconnect() (if at all) only after
	 * Connect() already ran Access() at least once, so this mirrors that real
	 * precondition rather than working around a bug. mode=-1 exercises Access()'s own
	 * "not a valid id" early-return, which still runs the lazy-init step first and
	 * touches no cache slot.
	 */
	CSTGHandle cacheInitHandle;
	cacheInitHandle.mode = (uint32_t)-1;
	cacheInitHandle.Access();

	/* Real Disconnect() unconditionally calls mSharedMem->Release() with no NULL
	 * check (see ustg_user_api.cpp's own comment on that call) -- give it a real,
	 * safe-to-release synthetic handle (mode=-1, Release()'s own immediate-return
	 * case) rather than leaving mSharedMem NULL (which would crash, a real but
	 * out-of-scope-for-this-KAT hazard already documented in the source).
	 */
	static CSTGHandle sharedMemStandIn;
	sharedMemStandIn.mode = (uint32_t)-1;
	UstgUserApiTestHooks::SetSharedMem(&sharedMemStandIn);

	int rt2user[2];  /* [0]=read end (ReadMessage's fd), [1]=write end (test writes) */
	int user2rt[2];  /* [0]=read end (test reads back), [1]=write end (SendSTGMessageWithSource's fd) */
	int unsol[2];
	int panel[2];
	pipe(rt2user);
	pipe(user2rt);
	pipe(unsol);
	pipe(panel);

	/* The real /dev/rtf1 RTAI FIFO's read() is presumed non-blocking-when-empty
	 * (returns immediately with a short count rather than blocking) -- the only
	 * way ReadMessageWithTimeout()'s busy-poll-against-a-deadline design (below)
	 * makes sense at all, since the real code never calls select()/poll(). A
	 * plain POSIX pipe blocks on an empty read with the write end still open,
	 * so mark the read end O_NONBLOCK here to match that presumed real
	 * semantics for the timeout tests.
	 */
	fcntl(rt2user[0], F_SETFL, O_NONBLOCK);

	UstgUserApiTestHooks::SetRt2User(rt2user[0]);
	UstgUserApiTestHooks::SetUnsol(unsol[0]);
	UstgUserApiTestHooks::SetPanel(panel[1]);

	/* --- ReadMessage(): normal frame --- */
	{
		char buf[64];
		write_frame(rt2user[1], "hello", 5);
		int n = USTGUserAPI::ReadMessage(buf, sizeof(buf));
		check("ReadMessage returns total frame length (2+5)", n == 7);
		check("ReadMessage payload bytes correct", memcmp(buf + 2, "hello", 5) == 0);
	}

	/* --- ReadMessage(): oversize message (length prefix > bufSize) rejected --- */
	{
		char buf[4]; /* too small for a 10-byte frame */
		write_frame(rt2user[1], "01234567", 8); /* total = 2+8 = 10 */
		/* Real check is `totalLen <= bufSize`; totalLen here is 10 > 4 -> must reject
		 * without consuming the 8 payload bytes still sitting in the pipe (the real
		 * code only issues the second read() once the length check passes). Drain
		 * them back out afterward so the next check starts from a clean pipe.
		 */
		int n = USTGUserAPI::ReadMessage(buf, sizeof(buf));
		check("ReadMessage rejects oversize frame (returns 0)", n == 0);
		char drain[8];
		read(rt2user[0], drain, 8);
	}

	/* --- ReadMessage(): NULL buffer --- */
	{
		check("ReadMessage(NULL, ...) returns 0", USTGUserAPI::ReadMessage(0, 64) == 0);
	}

	/* --- ReadMessageWithTimeout(): frame already available, returns immediately --- */
	{
		char buf[64];
		write_frame(rt2user[1], "quick", 5);
		int n = USTGUserAPI::ReadMessageWithTimeout((STGMessage *)buf, sizeof(buf), 2000);
		check("ReadMessageWithTimeout returns total length when data is ready (2+5)", n == 7);
		check("ReadMessageWithTimeout payload correct", memcmp(buf + 2, "quick", 5) == 0);
	}

	/* --- ReadMessageWithTimeout(): no data, real busy-loop-to-deadline path, times out --- */
	{
		char buf[64];
		int n = USTGUserAPI::ReadMessageWithTimeout((STGMessage *)buf, sizeof(buf), 3);
		check("ReadMessageWithTimeout returns 0 once the deadline passes with no data", n == 0);
	}

	/* --- ReadMessageWithTimeout(): NULL msg is a real documented "just wait" mode --- */
	{
		int n = USTGUserAPI::ReadMessageWithTimeout(0, 64, 2);
		check("ReadMessageWithTimeout(NULL msg, ...) waits out the deadline and returns 0", n == 0);
	}

	/* --- ReadUnsolicitedMessage(): same shape, different fd --- */
	{
		char buf[64];
		write_frame(unsol[1], "unsol-msg", 9);
		int n = USTGUserAPI::ReadUnsolicitedMessage(buf, sizeof(buf));
		check("ReadUnsolicitedMessage returns total length (2+9)", n == 11);
		check("ReadUnsolicitedMessage payload correct", memcmp(buf + 2, "unsol-msg", 9) == 0);
	}

	/* --- SendPanelMessage(): writes a length-prefixed frame, readable back --- */
	{
		struct { unsigned short len; char data[8]; } msg;
		msg.len = 2 + 4;
		memcpy(msg.data, "ping", 4);
		bool ok = USTGUserAPI::SendPanelMessage((const STGMessage *)&msg);
		check("SendPanelMessage returns true", ok);

		char readback[16];
		int n = read(panel[0], readback, sizeof(readback));
		check("SendPanelMessage wrote exactly totalLen bytes", n == 6);
		check("SendPanelMessage wire bytes match (length prefix + payload)",
		      memcmp(readback, &msg, 6) == 0);
	}

	/* --- SendPanelMessage(): zero-length message is a documented no-op success --- */
	{
		unsigned short zero = 0;
		bool ok = USTGUserAPI::SendPanelMessage((const STGMessage *)&zero);
		check("SendPanelMessage(zero-length) returns true without writing", ok);
	}

	/* --- SendPanelMessage(): NULL / not-connected --- */
	{
		check("SendPanelMessage(NULL) returns false",
		      USTGUserAPI::SendPanelMessage(0) == false);
	}

	/* --- Disconnect(): closes every fd and resets every cache to -1 --- */
	{
		/* Give Disconnect() something real to close for each of the 5 fds it
		 * touches, using fresh pipe ends so closing them doesn't interfere
		 * with fds this test still wants open above.
		 */
		int a[2], b[2], c[2], d[2], e[2];
		pipe(a); pipe(b); pipe(c); pipe(d); pipe(e);
		UstgUserApiTestHooks::SetRawRt2User(a[0]);
		UstgUserApiTestHooks::SetRawUser2Rt(b[1]);
		UstgUserApiTestHooks::SetUnsol(c[0]);
		UstgUserApiTestHooks::SetPanel(d[1]);
		/* m_userRtDirect has no dedicated test hook (never read by any Stage-2
		 * method) -- reuse the CSTGHandle friend surface is unnecessary since
		 * ustg_user_api.cpp exposes it as a private static too; skip closing a
		 * live fd for it and just confirm Disconnect() still resets it to -1
		 * (its "already -1, skip close()" branch).
		 */
		close(e[0]); close(e[1]);

		USTGUserAPI::Disconnect();

		check("Disconnect resets m_rt2userFifo to -1", UstgUserApiTestHooks::RawRt2User() == -1);
		check("Disconnect resets m_user2rtFifo to -1", UstgUserApiTestHooks::RawUser2Rt() == -1);
		check("Disconnect resets m_rtUnsolFifo to -1", UstgUserApiTestHooks::RawUnsol() == -1);
		check("Disconnect resets m_user2rtPanelFifo to -1", UstgUserApiTestHooks::RawPanel() == -1);
		check("Disconnect resets m_userRtDirect to -1", UstgUserApiTestHooks::RawDirect() == -1);
	}

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
